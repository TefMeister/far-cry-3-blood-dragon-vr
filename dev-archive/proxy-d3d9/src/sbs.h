/* sbs.h - Blood Dragon's two eyes side by side in the window (2026-10-08, /pd). Ported from Hard Reset's d3d9 proxy
 * (staging/hard-reset-vr/proxy-d3d9: ensure_sbs / capture / compose / drop_sbs), changed for frame-alternating eyes:
 * Hard Reset draws both eyes inside one frame, Blood Dragon draws one eye per frame.
 *
 * At each Present: copy the finished back buffer into its eye's half of a double-width surface (left | right), then
 * stretch the whole surface over the back buffer, so the window shows both eyes squeezed side by side. Switch file
 * fc3bd_vr_sbs.txt beside the exe, checked once a second, so it can be added or removed while the game runs. */
#ifndef BD_SBS_H
#define BD_SBS_H

typedef void (*SbsLogFn)(const char *fmt, ...);

void sbs_init(const char *exe_dir, SbsLogFn log);

/* At Present, before the real one. `eye` = stereo_frame_eye() (+1 right, -1 left, 0 = not a camera frame). */
void sbs_on_present(void *device, int eye);

/* Before the real Reset: default-pool surfaces must be released first. */
void sbs_on_reset(void);

#endif
