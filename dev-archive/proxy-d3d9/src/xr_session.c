/* xr_session.c - headset-thread half of the headset output (2026-10-11). See xr.h.
 *
 * Ported from Alan Wake's xr_main (staging/alan-wake-vr/proxy-d3d9/src/aw_xr.c), cut down to the floating screen:
 * loader loaded from the exe folder at run time (a machine without it is unaffected), [vr] RuntimeJson sets
 * XR_RUNTIME_JSON for this process only, D3D11 device on the runtime's adapter, one swapchain per eye, TWO QUAD
 * LAYERS (eyeVisibility LEFT / RIGHT) world-locked XR_SCREEN_DIST_M ahead of where the head was at start. Changed from
 * Alan Wake: an eye's swapchain is only re-uploaded when a new picture for it arrived (the last released image is
 * reused otherwise), alpha is forced opaque, and [vr] TestPattern=1 shows left red / right blue.
 * OpenXR headers: Khronos, Apache-2.0 OR MIT (third_party/openxr). */
#define COBJMACROS
#define CINTERFACE
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "settings.h"
#include "xr_shared.h"
#include "xr_views.h"

#define FMT_BGRA        87          /* DXGI_FORMAT_B8G8R8A8_UNORM */
#define FMT_BGRA_SRGB   91          /* DXGI_FORMAT_B8G8R8A8_UNORM_SRGB */
#define FMT_RGBA        28          /* DXGI_FORMAT_R8G8B8A8_UNORM */
#define FMT_RGBA_SRGB   29          /* DXGI_FORMAT_R8G8B8A8_UNORM_SRGB */
#define OPAQUE_ALPHA    255

static PFN_xrGetInstanceProcAddr p_gipa;
#define XR_FN(name) static PFN_##name p_##name
XR_FN(xrCreateInstance); XR_FN(xrDestroyInstance); XR_FN(xrGetSystem); XR_FN(xrCreateSession);
XR_FN(xrDestroySession); XR_FN(xrCreateReferenceSpace); XR_FN(xrDestroySpace); XR_FN(xrPollEvent);
XR_FN(xrBeginSession); XR_FN(xrEndSession); XR_FN(xrEnumerateSwapchainFormats); XR_FN(xrCreateSwapchain);
XR_FN(xrDestroySwapchain); XR_FN(xrEnumerateSwapchainImages); XR_FN(xrWaitFrame); XR_FN(xrBeginFrame);
XR_FN(xrEndFrame); XR_FN(xrAcquireSwapchainImage); XR_FN(xrWaitSwapchainImage); XR_FN(xrReleaseSwapchainImage);
XR_FN(xrGetD3D11GraphicsRequirementsKHR); XR_FN(xrLocateSpace); XR_FN(xrLocateViews);

typedef HRESULT(WINAPI *PFN_CreateDXGIFactory1_)(REFIID, void **);
static PFN_D3D11_CREATE_DEVICE p_D3D11CreateDevice;
static PFN_CreateDXGIFactory1_ p_CreateDXGIFactory1;

typedef struct {
    XrSwapchain sc;
    XrSwapchainImageD3D11KHR *img;
    uint32_t count;
    int uploaded;                   /* an image has been released at least once: the layer may be shown */
    unsigned long serial;           /* the picture last uploaded */
} EyeChain;

static void logx(const char *msg, XrResult r) { g_xr_log("xr: %s (XrResult %d)", msg, (int)r); }

static int load_d3d11(void) {
    HMODULE d11 = LoadLibraryA("d3d11.dll"), dxgi = LoadLibraryA("dxgi.dll");
    if (!d11 || !dxgi) return 0;
    p_D3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)(void *)GetProcAddress(d11, "D3D11CreateDevice");
    p_CreateDXGIFactory1 = (PFN_CreateDXGIFactory1_)(void *)GetProcAddress(dxgi, "CreateDXGIFactory1");
    return p_D3D11CreateDevice && p_CreateDXGIFactory1;
}

static int load_fn(XrInstance inst, const char *name, PFN_xrVoidFunction *out) {
    return XR_SUCCEEDED(p_gipa(inst, name, out)) && *out;
}
#define LOAD(inst, name) load_fn(inst, #name, (PFN_xrVoidFunction *)&p_##name)

static int load_all(XrInstance i) {
    return LOAD(i, xrDestroyInstance) && LOAD(i, xrGetSystem) && LOAD(i, xrCreateSession) &&
           LOAD(i, xrDestroySession) && LOAD(i, xrCreateReferenceSpace) && LOAD(i, xrDestroySpace) &&
           LOAD(i, xrPollEvent) && LOAD(i, xrBeginSession) && LOAD(i, xrEndSession) &&
           LOAD(i, xrEnumerateSwapchainFormats) && LOAD(i, xrCreateSwapchain) && LOAD(i, xrDestroySwapchain) &&
           LOAD(i, xrEnumerateSwapchainImages) && LOAD(i, xrWaitFrame) && LOAD(i, xrBeginFrame) &&
           LOAD(i, xrEndFrame) && LOAD(i, xrAcquireSwapchainImage) && LOAD(i, xrWaitSwapchainImage) &&
           LOAD(i, xrReleaseSwapchainImage) && LOAD(i, xrGetD3D11GraphicsRequirementsKHR) && LOAD(i, xrLocateSpace) &&
           LOAD(i, xrLocateViews);
}

static void ini_path(char *out, size_t n) { snprintf(out, n, "%s%s", xr_exe_dir(), XR_INI_FILE); }

/* [vr] RuntimeJson=<path>: this runtime for this game only (the loader reads XR_RUNTIME_JSON). Set here, inside the
 * process, because a Steam relaunch drops any variable set outside. */
static void apply_runtime_choice(void) {
    char ini[MAX_PATH], rt[MAX_PATH];
    ini_path(ini, sizeof ini);
    GetPrivateProfileStringA("vr", "RuntimeJson", "", rt, sizeof rt, ini);
    if (rt[0]) {
        SetEnvironmentVariableA("XR_RUNTIME_JSON", rt);
        g_xr_log("xr: runtime for this game only: %s", rt);
    } else {
        g_xr_log("xr: no [vr] RuntimeJson in %s - using the system's active OpenXR runtime", XR_INI_FILE);
    }
}

static int test_pattern_on(void) {
    char ini[MAX_PATH];
    ini_path(ini, sizeof ini);
    return GetPrivateProfileIntA("vr", "TestPattern", 0, ini) != 0;
}

static int pick_format(XrSession s, int64_t *fmt, int *swap_rb) {
    int64_t fmts[XR_MAX_SWAPCHAIN_FORMATS];
    uint32_t n = 0, i;
    const int64_t want[4] = { FMT_BGRA_SRGB, FMT_BGRA, FMT_RGBA_SRGB, FMT_RGBA };
    int w;
    if (XR_FAILED(p_xrEnumerateSwapchainFormats(s, XR_MAX_SWAPCHAIN_FORMATS, &n, fmts))) return 0;
    for (w = 0; w < 4; ++w)
        for (i = 0; i < n; ++i)
            if (fmts[i] == want[w]) { *fmt = want[w]; *swap_rb = (w >= 2); return 1; }
    return 0;
}

static int make_chain(XrSession s, int64_t fmt, int w, int h, EyeChain *c) {
    XrSwapchainCreateInfo ci;
    uint32_t i;
    memset(&ci, 0, sizeof ci);
    ci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    ci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    ci.format = fmt; ci.width = (uint32_t)w; ci.height = (uint32_t)h;
    ci.sampleCount = 1; ci.faceCount = 1; ci.arraySize = 1; ci.mipCount = 1;
    if (XR_FAILED(p_xrCreateSwapchain(s, &ci, &c->sc))) return 0;
    p_xrEnumerateSwapchainImages(c->sc, 0, &c->count, NULL);
    c->img = (XrSwapchainImageD3D11KHR *)calloc(c->count, sizeof *c->img);
    if (!c->img) return 0;
    for (i = 0; i < c->count; ++i) c->img[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
    p_xrEnumerateSwapchainImages(c->sc, c->count, &c->count, (XrSwapchainImageBaseHeader *)c->img);
    return 1;
}

/* BGRX (the game's bytes) -> the swapchain's order, alpha opaque, in place. */
static void to_swapchain_order(unsigned char *px, int n, int swap_rb) {
    int i;
    for (i = 0; i < n; ++i, px += 4) {
        if (swap_rb) { unsigned char t = px[0]; px[0] = px[2]; px[2] = t; }
        px[3] = OPAQUE_ALPHA;
    }
}

/* Solid colour in BGRX: left eye red, right eye blue. */
static void fill_pattern(unsigned char *px, int n, int e) {
    int i;
    for (i = 0; i < n; ++i, px += 4) {
        px[0] = e ? 255 : 0; px[1] = 0; px[2] = e ? 0 : 255; px[3] = OPAQUE_ALPHA;
    }
}

static void upload(ID3D11DeviceContext *ctx, EyeChain *c, const unsigned char *px, int w) {
    XrSwapchainImageAcquireInfo ai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrSwapchainImageWaitInfo wi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    XrSwapchainImageReleaseInfo ri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    uint32_t idx = 0;
    if (XR_FAILED(p_xrAcquireSwapchainImage(c->sc, &ai, &idx))) return;
    wi.timeout = XR_INFINITE_DURATION;
    if (XR_SUCCEEDED(p_xrWaitSwapchainImage(c->sc, &wi))) {
        ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource *)c->img[idx].texture, 0, NULL, px,
                                              (UINT)w * 4, 0);
        if (XR_SUCCEEDED(p_xrReleaseSwapchainImage(c->sc, &ri))) c->uploaded = 1;
    }
}

static int make_device(XrInstance inst, XrSystemId sys, ID3D11Device **dev, ID3D11DeviceContext **ctx) {
    XrGraphicsRequirementsD3D11KHR req = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
    IDXGIFactory1 *fac = NULL;
    IDXGIAdapter1 *ad = NULL, *chosen = NULL;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr;
    UINT i;
    p_xrGetD3D11GraphicsRequirementsKHR(inst, sys, &req);   /* required before xrCreateSession */
    if (!load_d3d11()) { g_xr_log("xr: d3d11.dll / dxgi.dll unavailable"); return 0; }
    if (SUCCEEDED(p_CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&fac))) {
        for (i = 0; IDXGIFactory1_EnumAdapters1(fac, i, &ad) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 d;
            IDXGIAdapter1_GetDesc1(ad, &d);
            if (!chosen && memcmp(&d.AdapterLuid, &req.adapterLuid, sizeof(LUID)) == 0) chosen = ad;
            else IDXGIAdapter1_Release(ad);
        }
        IDXGIFactory1_Release(fac);
    }
    if (req.minFeatureLevel > fl) fl = req.minFeatureLevel;
    hr = p_D3D11CreateDevice((IDXGIAdapter *)chosen, chosen ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                             NULL, 0, &fl, 1, D3D11_SDK_VERSION, dev, NULL, ctx);
    if (chosen) IDXGIAdapter1_Release(chosen);
    if (FAILED(hr)) { g_xr_log("xr: D3D11 device failed (0x%08lx)", (unsigned long)hr); return 0; }
    return 1;
}

static void fill_quad(XrCompositionLayerQuad *q, XrSpace space, int e, const EyeChain *c, XrVector3f anchor,
                      int w, int h) {
    memset(q, 0, sizeof *q);
    q->type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    q->space = space;
    q->eyeVisibility = e == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
    q->subImage.swapchain = c->sc;
    q->subImage.imageRect.extent.width = w;
    q->subImage.imageRect.extent.height = h;
    q->pose.orientation.w = 1.0f;
    q->pose.position.x = anchor.x;
    q->pose.position.y = anchor.y;
    q->pose.position.z = anchor.z - XR_SCREEN_DIST_M;
    q->size.width = XR_SCREEN_WIDTH_M;
    q->size.height = XR_SCREEN_WIDTH_M * (float)h / (float)w;
}

/* Head orientation and both eyes' poses at the frame's display time, for the game's next frame (xr_views.h). */
static void publish_pose(XrSession s, XrSpace space, XrSpace view_space, XrTime t) {
    XrViewLocateInfo li = { XR_TYPE_VIEW_LOCATE_INFO };
    XrViewState vs = { XR_TYPE_VIEW_STATE };
    XrView v[2] = { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } };
    XrSpaceLocation head = { XR_TYPE_SPACE_LOCATION };
    uint32_t n = 0;
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = t;
    li.space = space;
    if (XR_FAILED(p_xrLocateViews(s, &li, &vs, 2, &n, v)) || n != 2 ||
        !(vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT))
        return;
    if (XR_FAILED(p_xrLocateSpace(view_space, space, t, &head)) ||
        !(head.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
        return;
    xr_views_publish(&head.pose.orientation, v);
}

/* Session events; returns 0 when the session is ending for good. */
static int poll_events(XrInstance inst, XrSession sess, int *running, int *logged) {
    XrEventDataBuffer ev;
    memset(&ev, 0, sizeof ev);
    ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    while (p_xrPollEvent(inst, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            const XrEventDataSessionStateChanged *sc = (const XrEventDataSessionStateChanged *)&ev;
            if (*logged < XR_EVENT_LOG_MAX) { g_xr_log("xr: session state %d", (int)sc->state); ++*logged; }
            if (sc->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi = { XR_TYPE_SESSION_BEGIN_INFO };
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (XR_SUCCEEDED(p_xrBeginSession(sess, &bi))) { *running = 1; g_xr_log("xr: session running"); }
            } else if (sc->state == XR_SESSION_STATE_STOPPING) {
                p_xrEndSession(sess);
                *running = 0;
            } else if (sc->state == XR_SESSION_STATE_EXITING || sc->state == XR_SESSION_STATE_LOSS_PENDING) {
                return 0;
            }
        }
        memset(&ev, 0, sizeof ev);
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
    return 1;
}

void xr_session_main(void) {
    char path[MAX_PATH];
    HMODULE lib;
    XrInstance inst = XR_NULL_HANDLE;
    XrSystemId sys = XR_NULL_SYSTEM_ID;
    XrSession sess = XR_NULL_HANDLE;
    XrSpace space = XR_NULL_HANDLE, view_space = XR_NULL_HANDLE;
    XrVector3f anchor = { 0.0f, 0.0f, 0.0f };
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    EyeChain eye[2];
    unsigned char *pix = NULL;
    int running = 0, anchored = 0, w = 0, h = 0, swap_rb = 0, e, logged = 0, pattern;
    int64_t fmt = 0;
    unsigned long submitted = 0, empty = 0, uploads = 0;
    DWORD last_stats = GetTickCount();
    XrResult r;
    memset(eye, 0, sizeof eye);

    apply_runtime_choice();
    pattern = test_pattern_on();
    snprintf(path, sizeof path, "%s%s", xr_exe_dir(), XR_LOADER_FILE);
    lib = LoadLibraryA(path);
    if (!lib) { g_xr_log("xr: %s not found beside the exe - headset output OFF", XR_LOADER_FILE); return; }
    p_gipa = (PFN_xrGetInstanceProcAddr)(void *)GetProcAddress(lib, "xrGetInstanceProcAddr");
    if (!p_gipa || !LOAD(XR_NULL_HANDLE, xrCreateInstance)) { g_xr_log("xr: loader has no entry point"); return; }
    {
        const char *exts[1] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
        XrInstanceCreateInfo ici;
        memset(&ici, 0, sizeof ici);
        ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
        strcpy(ici.applicationInfo.applicationName, XR_APP_NAME);
        ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        ici.enabledExtensionCount = 1;
        ici.enabledExtensionNames = exts;
        r = p_xrCreateInstance(&ici, &inst);
        if (XR_FAILED(r)) { logx("xrCreateInstance failed: no usable 32-bit runtime with D3D11", r); return; }
    }
    if (!load_all(inst)) { g_xr_log("xr: a required OpenXR function is missing"); goto done; }
    {
        XrSystemGetInfo gi = { XR_TYPE_SYSTEM_GET_INFO };
        gi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        r = p_xrGetSystem(inst, &gi, &sys);
        if (XR_FAILED(r)) { logx("no headset system", r); goto done; }
    }
    if (!make_device(inst, sys, &dev, &ctx)) goto done;
    {
        XrGraphicsBindingD3D11KHR bind = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
        XrSessionCreateInfo sci = { XR_TYPE_SESSION_CREATE_INFO };
        XrReferenceSpaceCreateInfo rs = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        bind.device = dev;
        sci.next = &bind;
        sci.systemId = sys;
        r = p_xrCreateSession(inst, &sci, &sess);
        if (XR_FAILED(r)) { logx("session failed", r); goto done; }
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        rs.poseInReferenceSpace.orientation.w = 1.0f;
        p_xrCreateReferenceSpace(sess, &rs, &space);
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;   /* the head, to place the screen at eye height */
        p_xrCreateReferenceSpace(sess, &rs, &view_space);
    }
    g_xr_log("xr: session created%s; waiting for the runtime to start it", pattern ? " (TEST PATTERN)" : "");

    while (InterlockedCompareExchange(&g_xr_run, 1, 1) == 1) {
        XrFrameWaitInfo fwi = { XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState fs = { XR_TYPE_FRAME_STATE };
        XrFrameBeginInfo fbi = { XR_TYPE_FRAME_BEGIN_INFO };
        XrFrameEndInfo fei = { XR_TYPE_FRAME_END_INFO };
        XrCompositionLayerQuad quad[2];
        XrCompositionLayerProjection proj = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        XrCompositionLayerProjectionView pv[2];
        XrSwapchain sc[2];
        const XrCompositionLayerBaseHeader *layers[2];
        int have = 0;

        if (!poll_events(inst, sess, &running, &logged)) break;
        if (!running) { Sleep(XR_IDLE_SLEEP_MS); continue; }
        if (!eye[0].sc) {                         /* swapchains once the first picture's size is known */
            xr_frame_size(&w, &h);
            if (w <= 0 || h <= 0) { Sleep(XR_IDLE_SLEEP_MS); continue; }
            if (!pick_format(sess, &fmt, &swap_rb) || !make_chain(sess, fmt, w, h, &eye[0]) ||
                !make_chain(sess, fmt, w, h, &eye[1]) || !(pix = (unsigned char *)malloc((size_t)w * h * 4))) {
                g_xr_log("xr: swapchain setup failed");
                break;
            }
            g_xr_log("xr: two %dx%d eye swapchains, format %d%s", w, h, (int)fmt, swap_rb ? " (swizzled)" : "");
        }
        if (XR_FAILED(p_xrWaitFrame(sess, &fwi, &fs))) { Sleep(XR_IDLE_SLEEP_MS); continue; }
        p_xrBeginFrame(sess, &fbi);
        if (!anchored && view_space) {             /* the screen goes where the head is at the start, then stays */
            XrSpaceLocation loc = { XR_TYPE_SPACE_LOCATION };
            if (XR_SUCCEEDED(p_xrLocateSpace(view_space, space, fs.predictedDisplayTime, &loc)) &&
                (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
                anchor = loc.pose.position;
                anchored = 1;
                g_xr_log("xr: screen placed %.1f m ahead at head height %.2f m", (double)XR_SCREEN_DIST_M,
                         (double)anchor.y);
            }
        }
        if (view_space && (xr_views_head_on() || xr_views_projection_on()))
            publish_pose(sess, space, view_space, fs.predictedDisplayTime);
        if (fs.shouldRender) {
            for (e = 0; e < 2; ++e) {
                if (pattern ? !eye[e].uploaded : xr_frame_take(e, pix, w, h, &eye[e].serial)) {
                    if (pattern) fill_pattern(pix, w * h, e);
                    to_swapchain_order(pix, w * h, swap_rb);
                    upload(ctx, &eye[e], pix, w);
                    uploads++;
                }
            }
            sc[0] = eye[0].sc;
            sc[1] = eye[1].sc;
            if (eye[0].uploaded && eye[1].uploaded && xr_views_projection_on() && !pattern &&
                xr_views_fill_projection(pv, sc, w, h)) {
                proj.space = space;
                proj.viewCount = 2;
                proj.views = pv;
                layers[0] = (const XrCompositionLayerBaseHeader *)&proj;
                have = 1;
            } else if (eye[0].uploaded && eye[1].uploaded) {
                for (e = 0; e < 2; ++e) {
                    fill_quad(&quad[e], space, e, &eye[e], anchor, w, h);
                    layers[e] = (const XrCompositionLayerBaseHeader *)&quad[e];
                }
                have = 2;
            }
        }
        fei.displayTime = fs.predictedDisplayTime;
        fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        fei.layerCount = (uint32_t)have;
        fei.layers = have ? layers : NULL;
        p_xrEndFrame(sess, &fei);
        if (have) submitted++; else empty++;
        if (GetTickCount() - last_stats >= XR_STATS_MS) {
            last_stats = GetTickCount();
            g_xr_log("xr: headset per %u ms: frames submitted %lu, empty %lu, pictures uploaded %lu", XR_STATS_MS,
                     submitted, empty, uploads);
            submitted = empty = uploads = 0;
        }
    }

done:
    for (e = 0; e < 2; ++e) {
        if (eye[e].sc) p_xrDestroySwapchain(eye[e].sc);
        free(eye[e].img);
    }
    free(pix);
    if (view_space) p_xrDestroySpace(view_space);
    if (space) p_xrDestroySpace(space);
    if (sess) p_xrDestroySession(sess);
    if (ctx) ID3D11DeviceContext_Release(ctx);
    if (dev) ID3D11Device_Release(dev);
    if (inst && p_xrDestroyInstance) p_xrDestroyInstance(inst);
    g_xr_log("xr: headset output stopped");
}
