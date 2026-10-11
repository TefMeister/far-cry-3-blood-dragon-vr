/* xr.h - Blood Dragon's two eyes in an OpenXR headset, as a world-locked screen (2026-10-11, reader helper, dev PC).
 *
 * Ported from Alan Wake's d3d9 proxy (staging/alan-wake-vr/proxy-d3d9/src/aw_xr.c), the closer fit: Alan Wake, like
 * Blood Dragon, draws ONE eye per frame and labels it at Present. Hard Reset draws both eyes inside one frame and
 * reads back a double-width surface through a ring, which Blood Dragon does not need yet.
 *
 * Game thread, at each Present, BEFORE sbs_on_present (so the eye's own picture is taken, not the side-by-side one):
 * the back buffer is copied, read back to memory and stored as that eye's latest picture (eye 0 = a frame with no
 * camera shift, a menu: stored for both eyes). Headset thread: loads openxr_loader.dll from the exe folder at run
 * time, makes its own D3D11 device, one swapchain per eye, and shows each eye its own picture on a quad 2 m ahead,
 * placed where the head was when the session started. No head tracking.
 *
 * Switch: fc3bd_vr_openxr.txt beside the exe (settings.h XR_*). Runtime: [vr] RuntimeJson= in fc3bd_vr.ini. */
#ifndef BD_XR_H
#define BD_XR_H

typedef void (*XrLogFn)(const char *fmt, ...);

void xr_init(XrLogFn log);

/* At Present, before the real one and before sbs. `eye` = stereo_frame_eye() (+1 right, -1 left, 0 = no camera). */
void xr_on_present(void *device, int eye);

/* Before the real Reset: our default-pool surface must be gone first. */
void xr_on_reset(void);

/* DLL detach: tell the headset thread to stop (not joined: loader lock). */
void xr_shutdown(void);

#endif
