/* sbs.c - see sbs.h (2026-10-08, /pd). Render thread only. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

#include "sbs.h"
#include "settings.h"

static SbsLogFn g_log;
static char g_flag[MAX_PATH];
static int g_on;
static DWORD g_last_check, g_last_stats;
static IDirect3DSurface9 *g_surf;          /* 2W x H: left eye | right eye */
static UINT g_w, g_h;
static D3DFORMAT g_fmt;
static int g_have[2];                       /* each half holds a picture since the surface was made */
static int g_described;
static unsigned long g_cap[2], g_composed, g_fail, g_not_eye;
static HRESULT g_last_hr;

void sbs_init(const char *exe_dir, SbsLogFn log) {
    g_log = log;
    snprintf(g_flag, sizeof g_flag, "%s%s", exe_dir, SBS_ON_FILE);
}

static void drop(void) {
    if (g_surf) { IDirect3DSurface9_Release(g_surf); g_surf = NULL; }
    g_have[0] = g_have[1] = 0;
}

static int ensure(IDirect3DDevice9 *dev, IDirect3DSurface9 *bb) {
    D3DSURFACE_DESC d;
    HRESULT hr;
    if (FAILED(IDirect3DSurface9_GetDesc(bb, &d))) return 0;
    if (!g_described) {
        g_described = 1;
        g_log("sbs: back buffer %ux%u format %d multisample %d usage 0x%lx pool %d", d.Width, d.Height, (int)d.Format,
              (int)d.MultiSampleType, (unsigned long)d.Usage, (int)d.Pool);
    }
    if (g_surf && g_w == d.Width * 2 && g_h == d.Height && g_fmt == d.Format) return 1;
    drop();
    hr = IDirect3DDevice9_CreateRenderTarget(dev, d.Width * 2, d.Height, d.Format, D3DMULTISAMPLE_NONE, 0, FALSE,
                                             &g_surf, NULL);
    g_log("sbs: side-by-side surface %ux%u format %d -> 0x%08lx", d.Width * 2, d.Height, (int)d.Format,
          (unsigned long)hr);
    if (FAILED(hr)) { g_surf = NULL; return 0; }
    g_w = d.Width * 2;
    g_h = d.Height;
    g_fmt = d.Format;
    return 1;
}

static void check_switch(void) {
    DWORD now = GetTickCount();
    int on;
    if (g_last_check && now - g_last_check < SBS_SWITCH_CHECK_MS) return;
    g_last_check = now;
    on = GetFileAttributesA(g_flag) != INVALID_FILE_ATTRIBUTES;
    if (on != g_on) {
        g_on = on;
        g_log("sbs: %s (%s)", on ? "ON" : "off", SBS_ON_FILE);
        if (!on) drop();
    }
}

static void stats(void) {
    DWORD now = GetTickCount();
    if (!g_on || now - g_last_stats < SBS_STATS_MS) return;
    g_last_stats = now;
    g_log("sbs: per %u ms: captured left %lu, right %lu; composed %lu; frames with no camera eye %lu; failures %lu "
          "(last 0x%08lx)", SBS_STATS_MS, g_cap[0], g_cap[1], g_composed, g_not_eye, g_fail, (unsigned long)g_last_hr);
    g_cap[0] = g_cap[1] = g_composed = g_not_eye = g_fail = 0;
}

void sbs_on_present(void *device, int eye) {
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)device;
    IDirect3DSurface9 *bb = NULL;
    HRESULT hr;
    check_switch();
    if (!g_on || !dev) return;
    stats();
    if (eye == 0) { g_not_eye++; return; }   /* menus, loading: leave the frame as the game drew it */
    hr = IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
    if (SUCCEEDED(hr) && ensure(dev, bb)) {
        int half = eye < 0 ? 0 : 1;
        RECT r = { half ? (LONG)(g_w / 2) : 0, 0, half ? (LONG)g_w : (LONG)(g_w / 2), (LONG)g_h };
        hr = IDirect3DDevice9_StretchRect(dev, bb, NULL, g_surf, &r, D3DTEXF_NONE);
        if (SUCCEEDED(hr)) { g_cap[half]++; g_have[half] = 1; }
        if (SUCCEEDED(hr) && g_have[0] && g_have[1]) {
            hr = IDirect3DDevice9_StretchRect(dev, g_surf, NULL, bb, NULL, D3DTEXF_LINEAR);
            if (SUCCEEDED(hr)) g_composed++;
        }
    } else if (SUCCEEDED(hr)) {
        hr = E_FAIL;
    }
    if (bb) IDirect3DSurface9_Release(bb);
    if (FAILED(hr)) { g_fail++; g_last_hr = hr; }
}

void sbs_on_reset(void) { drop(); }
