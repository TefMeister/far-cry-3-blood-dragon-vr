/* xr_views.h - the headset's head pose, shared between the headset thread and the game thread (2026-10-15, /pd).
 * Shape from Alan Wake's aw_xr_views.c (staging/alan-wake-vr/proxy-d3d9/src/aw_xr_views.c).
 *
 * One game frame, which draws ONE eye:
 *   headset thread, every headset frame: xr_views_publish() stores the latest head orientation and both eyes' poses.
 *   game thread, at Present, after the frame's picture was stored for its eye: xr_views_on_capture() files the pose
 *     that frame was rendered with (and the game's lens) under that eye; then xr_views_snapshot() freezes the latest
 *     pose as the RENDER pose for the frame that starts now, and xr_views_render_rot() gives its head rotation to the
 *     camera edit (stereo_set_head).
 *   headset thread: xr_views_fill_projection() submits each eye's picture as a projection view with exactly the pose
 *     and lens it was drawn with, so the runtime only has to correct for the head movement since.
 *
 * Settings in fc3bd_vr.ini, read when the headset output starts: [vr] HeadTrack=1 turns the game's camera with the
 * head; [vr] Layers=projection submits each eye as a projection view at its render pose instead of the floating
 * screen. Both off by default. Numpad 2 re-centres: the way the headset faces now becomes the game's forward. */
#ifndef BD_XR_VIEWS_H
#define BD_XR_VIEWS_H

#include <windows.h>

void xr_views_init(const char *ini_path, void (*log)(const char *fmt, ...));

int xr_views_head_on(void);
int xr_views_projection_on(void);

/* Game thread. */
void xr_views_set_lens(const float tan4[4]);   /* the game's lens this frame, {left, right, up, down} tangents */
void xr_views_on_capture(int eye);              /* 0 left, 1 right, -1 both (a frame with no camera) */
void xr_views_snapshot(void);
void xr_views_clear(void);                      /* headset output off: no render pose any more */
int  xr_views_render_rot(float rot9[9]);        /* 0 = no head rotation this frame */

#ifdef XR_CURRENT_API_VERSION
/* Headset thread. */
void xr_views_publish(const XrQuaternionf *head, const XrView *views);
int  xr_views_fill_projection(XrCompositionLayerProjectionView *pv, const XrSwapchain *sc, int w, int h);
#endif

#endif
