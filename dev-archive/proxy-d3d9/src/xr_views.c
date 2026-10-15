/* xr_views.c - see xr_views.h (2026-10-15, /pd). */
#define COBJMACROS
#define CINTERFACE
#include <windows.h>
#include <d3d11.h>
#include <math.h>
#include <string.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "settings.h"
#include "headrot.h"
#include "xr_views.h"

static CRITICAL_SECTION g_vcs;
static int g_ready, g_head_on, g_proj_on;
static void (*g_log)(const char *fmt, ...);

static XrQuaternionf g_live_head;               /* headset thread */
static XrView g_live_view[2];
static int g_live_valid;

static XrQuaternionf g_rend_head;               /* game thread: the pose the frame now being drawn uses */
static XrView g_rend_view[2];
static int g_rend_valid;
static float g_lens[4];                         /* game thread: the game's lens, tangents */
static int g_lens_valid;

static XrView g_cap_view[2];                    /* the pose and lens each eye's last stored picture was drawn with */
static int g_cap_valid[2];

static int g_recentre = 1;                      /* game thread: the next pose's heading becomes the forward */
static float g_yaw0;
static SHORT g_prev_key;

void xr_views_init(const char *ini_path, void (*log)(const char *fmt, ...)) {
    char v[32];
    if (g_ready) return;
    g_log = log;
    InitializeCriticalSection(&g_vcs);
    g_head_on = GetPrivateProfileIntA("vr", XR_INI_HEADTRACK, 0, ini_path) != 0;
    GetPrivateProfileStringA("vr", XR_INI_LAYERS, "", v, sizeof v, ini_path);
    g_proj_on = _stricmp(v, XR_LAYERS_PROJECTION) == 0;
    g_ready = 1;
    log("xr: head tracking %s (numpad 2 re-centres), eyes shown as %s", g_head_on ? "ON" : "off",
        g_proj_on ? "projection views at their render pose" : "a floating screen");
}

int xr_views_head_on(void) { return g_ready && g_head_on; }
int xr_views_projection_on(void) { return g_ready && g_proj_on; }

void xr_views_publish(const XrQuaternionf *head, const XrView *views) {
    if (!g_ready) return;
    EnterCriticalSection(&g_vcs);
    g_live_head = *head;
    g_live_view[0] = views[0];
    g_live_view[1] = views[1];
    g_live_valid = 1;
    LeaveCriticalSection(&g_vcs);
}

void xr_views_set_lens(const float tan4[4]) {
    memcpy(g_lens, tan4, sizeof g_lens);
    g_lens_valid = 1;
}

/* The picture just stored for `eye` was drawn with the render pose: file it, with the game's lens, for submission. */
void xr_views_on_capture(int eye) {
    int e;
    if (!g_ready || !g_rend_valid || !g_lens_valid) return;
    EnterCriticalSection(&g_vcs);
    for (e = 0; e < 2; ++e) {
        if (eye >= 0 && e != eye) continue;
        g_cap_view[e] = g_rend_view[e];
        g_cap_view[e].fov.angleLeft = atanf(g_lens[0]);
        g_cap_view[e].fov.angleRight = atanf(g_lens[1]);
        g_cap_view[e].fov.angleUp = atanf(g_lens[2]);
        g_cap_view[e].fov.angleDown = atanf(g_lens[3]);
        g_cap_valid[e] = 1;
    }
    LeaveCriticalSection(&g_vcs);
}

void xr_views_snapshot(void) {
    SHORT key;
    if (!g_ready) return;
    key = GetAsyncKeyState(XR_RECENTRE_KEY);
    if ((key & 0x8000) && !(g_prev_key & 0x8000)) {
        g_recentre = 1;
        g_log("xr: re-centred: the way the headset faces now is the game's forward");
    }
    g_prev_key = key;
    EnterCriticalSection(&g_vcs);
    g_rend_head = g_live_head;
    g_rend_view[0] = g_live_view[0];
    g_rend_view[1] = g_live_view[1];
    g_rend_valid = g_live_valid;
    LeaveCriticalSection(&g_vcs);
}

void xr_views_clear(void) { g_rend_valid = 0; }

int xr_views_render_rot(float rot9[9]) {
    if (!g_ready || !g_head_on || !g_rend_valid) return 0;
    hr_rot_from_quat(g_rend_head.x, g_rend_head.y, g_rend_head.z, g_rend_head.w, rot9);
    if (g_recentre) { g_yaw0 = hr_yaw(rot9); g_recentre = 0; }
    hr_remove_yaw(rot9, g_yaw0);
    return 1;
}

int xr_views_fill_projection(XrCompositionLayerProjectionView *pv, const XrSwapchain *sc, int w, int h) {
    int e, ok;
    if (!g_ready) return 0;
    EnterCriticalSection(&g_vcs);
    ok = g_cap_valid[0] && g_cap_valid[1];
    for (e = 0; e < 2 && ok; ++e) {
        memset(&pv[e], 0, sizeof pv[e]);
        pv[e].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        pv[e].pose = g_cap_view[e].pose;
        pv[e].fov = g_cap_view[e].fov;
        pv[e].subImage.swapchain = sc[e];
        pv[e].subImage.imageRect.extent.width = w;
        pv[e].subImage.imageRect.extent.height = h;
    }
    LeaveCriticalSection(&g_vcs);
    return ok;
}
