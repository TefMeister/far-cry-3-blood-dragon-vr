/* xr_capture.c - game-thread half of the headset output: switch file, back-buffer capture, each eye's latest picture
 * (2026-10-11). See xr.h. Capture shape from Alan Wake's aw_xr_capture (staging/alan-wake-vr/proxy-d3d9/src/aw_xr.c):
 * StretchRect into a plain render target, GetRenderTargetData into system memory, LockRect, copy rows. That readback
 * waits for the GPU (Hard Reset measured 14 ms a frame that way), so frame rate drops while it is on; Hard Reset's
 * event-query ring (staging/hard-reset-vr/proxy-d3d9/src/d3d9_readback.c) is the known cure if it matters. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "settings.h"
#include "xr_shared.h"
#include "xr_views.h"

XrLogFn g_xr_log;
volatile LONG g_xr_run;
volatile LONG g_xr_capture;

static char g_dir[MAX_PATH], g_flag[MAX_PATH];
static CRITICAL_SECTION g_frame_cs;
static int g_ready;
static HANDLE g_thread;
static DWORD g_last_check, g_last_stats;

static unsigned char *g_eye_buf[2];
static int g_w, g_h;                          /* size of the stored pictures */
static unsigned long g_serial[2];

static IDirect3DSurface9 *g_cap_rt, *g_cap_sys;
static UINT g_cap_w, g_cap_h;
static D3DFORMAT g_cap_fmt;
static int g_told_format, g_described;
static unsigned long g_cap[3], g_fail;        /* captures: left, right, no-camera frames (both eyes) */
static HRESULT g_last_hr;

const char *xr_exe_dir(void) { return g_dir; }

void xr_init(XrLogFn log) {
    char *slash;
    g_xr_log = log;
    GetModuleFileNameA(NULL, g_dir, MAX_PATH);
    slash = strrchr(g_dir, '\\');
    if (slash) slash[1] = 0;
    snprintf(g_flag, sizeof g_flag, "%s%s", g_dir, XR_ON_FILE);
    InitializeCriticalSection(&g_frame_cs);
    {
        char ini[MAX_PATH];
        snprintf(ini, sizeof ini, "%s%s", g_dir, XR_INI_FILE);
        xr_views_init(ini, log);
    }
    g_ready = 1;
    log("xr: headset output off until %s is beside the exe (runtime: [vr] RuntimeJson in %s)", XR_ON_FILE,
        XR_INI_FILE);
}

static void release_capture(void) {
    if (g_cap_rt)  { IDirect3DSurface9_Release(g_cap_rt);  g_cap_rt = NULL; }
    if (g_cap_sys) { IDirect3DSurface9_Release(g_cap_sys); g_cap_sys = NULL; }
}

static DWORD WINAPI xr_thread(LPVOID unused) {
    (void)unused;
    xr_session_main();
    InterlockedExchange(&g_xr_run, 0);
    return 0;
}

static void check_switch(void) {
    DWORD now = GetTickCount();
    LONG on;
    if (g_last_check && now - g_last_check < XR_SWITCH_CHECK_MS) return;
    g_last_check = now;
    on = GetFileAttributesA(g_flag) != INVALID_FILE_ATTRIBUTES;
    if (on == g_xr_capture) return;
    InterlockedExchange(&g_xr_capture, on);
    g_xr_log("xr: capture %s (%s)", on ? "ON" : "off", XR_ON_FILE);
    if (!on) { release_capture(); return; }
    if (!g_thread) {   /* first time on: start the headset thread once; it lives until the game closes */
        InterlockedExchange(&g_xr_run, 1);
        g_thread = CreateThread(NULL, 0, xr_thread, NULL, 0, NULL);
        g_xr_log("xr: headset thread %s", g_thread ? "started" : "FAILED to start");
    }
}

static int ensure(IDirect3DDevice9 *dev, const D3DSURFACE_DESC *d) {
    HRESULT hr;
    if (g_cap_rt && g_cap_w == d->Width && g_cap_h == d->Height && g_cap_fmt == d->Format) return 1;
    release_capture();
    hr = IDirect3DDevice9_CreateRenderTarget(dev, d->Width, d->Height, d->Format, D3DMULTISAMPLE_NONE, 0, FALSE,
                                             &g_cap_rt, NULL);
    if (SUCCEEDED(hr))
        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(dev, d->Width, d->Height, d->Format, D3DPOOL_SYSTEMMEM,
                                                          &g_cap_sys, NULL);
    g_xr_log("xr: capture surfaces %ux%u format %d -> 0x%08lx", d->Width, d->Height, (int)d->Format,
             (unsigned long)hr);
    if (FAILED(hr)) { release_capture(); return 0; }
    g_cap_w = d->Width;
    g_cap_h = d->Height;
    g_cap_fmt = d->Format;
    return 1;
}

/* Store the locked rows for eye slot `e` (0, 1) or both (-1). Called with the frame lock held. */
static void store(const D3DLOCKED_RECT *lr, int which) {
    int e, y;
    if (g_w != (int)g_cap_w || g_h != (int)g_cap_h) {
        for (e = 0; e < 2; ++e) { free(g_eye_buf[e]); g_eye_buf[e] = NULL; }
        g_w = (int)g_cap_w;
        g_h = (int)g_cap_h;
    }
    for (e = 0; e < 2; ++e) {
        if (which >= 0 && e != which) continue;
        if (!g_eye_buf[e]) g_eye_buf[e] = (unsigned char *)malloc((size_t)g_w * g_h * 4);
        if (!g_eye_buf[e]) continue;
        for (y = 0; y < g_h; ++y)
            memcpy(g_eye_buf[e] + (size_t)y * g_w * 4, (const unsigned char *)lr->pBits + (size_t)y * lr->Pitch,
                   (size_t)g_w * 4);
        g_serial[e]++;
    }
}

static void stats(void) {
    DWORD now = GetTickCount();
    if (now - g_last_stats < XR_STATS_MS) return;
    g_last_stats = now;
    g_xr_log("xr: per %u ms: captured left %lu, right %lu, no-camera (both eyes) %lu; failures %lu (last 0x%08lx)",
             XR_STATS_MS, g_cap[0], g_cap[1], g_cap[2], g_fail, (unsigned long)g_last_hr);
    g_cap[0] = g_cap[1] = g_cap[2] = g_fail = 0;
}

void xr_on_present(void *device, int eye) {
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)device;
    IDirect3DSurface9 *bb = NULL;
    D3DSURFACE_DESC d;
    D3DLOCKED_RECT lr;
    HRESULT hr;
    int which = eye < 0 ? 0 : eye > 0 ? 1 : -1;
    if (!g_ready || !dev) return;
    check_switch();
    if (!g_xr_capture || !g_xr_run) { xr_views_clear(); return; }
    stats();
    hr = IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
    if (SUCCEEDED(hr)) hr = IDirect3DSurface9_GetDesc(bb, &d);
    if (SUCCEEDED(hr) && !g_described) {
        g_described = 1;
        g_xr_log("xr: back buffer %ux%u format %d multisample %d", d.Width, d.Height, (int)d.Format,
                 (int)d.MultiSampleType);
    }
    if (SUCCEEDED(hr) && ((int)d.Width > XR_MAX_W || (int)d.Height > XR_MAX_H ||
                          (d.Format != D3DFMT_X8R8G8B8 && d.Format != D3DFMT_A8R8G8B8))) {
        if (!g_told_format) g_xr_log("xr: back buffer format %d / size not handled - nothing captured", (int)d.Format);
        g_told_format = 1;
        hr = E_FAIL;
    }
    if (SUCCEEDED(hr) && !ensure(dev, &d)) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = IDirect3DDevice9_StretchRect(dev, bb, NULL, g_cap_rt, NULL, D3DTEXF_NONE);
    if (SUCCEEDED(hr)) hr = IDirect3DDevice9_GetRenderTargetData(dev, g_cap_rt, g_cap_sys);
    if (SUCCEEDED(hr)) hr = IDirect3DSurface9_LockRect(g_cap_sys, &lr, NULL, D3DLOCK_READONLY);
    if (SUCCEEDED(hr)) {
        EnterCriticalSection(&g_frame_cs);
        store(&lr, which);
        LeaveCriticalSection(&g_frame_cs);
        IDirect3DSurface9_UnlockRect(g_cap_sys);
        g_cap[which < 0 ? 2 : which]++;
        xr_views_on_capture(which);       /* this picture was drawn with the render pose taken at the last Present */
    } else {
        g_fail++;
        g_last_hr = hr;
    }
    if (bb) IDirect3DSurface9_Release(bb);
    xr_views_snapshot();                  /* the pose for the frame that starts now */
}

void xr_on_reset(void) { release_capture(); }

void xr_shutdown(void) { InterlockedExchange(&g_xr_run, 0); }

void xr_frame_size(int *w, int *h) {
    EnterCriticalSection(&g_frame_cs);
    *w = g_w;
    *h = g_h;
    LeaveCriticalSection(&g_frame_cs);
}

int xr_frame_take(int e, unsigned char *dst, int w, int h, unsigned long *serial) {
    int ok = 0;
    EnterCriticalSection(&g_frame_cs);
    if (g_w == w && g_h == h && g_eye_buf[e] && g_serial[e] != *serial) {
        memcpy(dst, g_eye_buf[e], (size_t)w * h * 4);
        *serial = g_serial[e];
        ok = 1;
    }
    LeaveCriticalSection(&g_frame_cs);
    return ok;
}
