/* stereo.h - Blood Dragon's per-eye camera shift, Far Cry 2's way: alternate eyes per frame by rewriting the camera
 * blocks the game uploads to the vertex shaders (2026-10-08, /pd). Maths: stereo_math.h.
 *
 * Starts OFF (a bad match can never break the first launch) unless fc3bd_vr_stereo_on.txt sits beside the exe.
 * Numpad keys, read at each Present: 5 on/off, 4 / 6 eye separation smaller / larger, 8 mode (alternate, left only,
 * right only). Only the player's cameras are shifted: a 16:9 perspective view-projection at c0 or c4 marks the camera
 * whose view (c12) and inverse view (c32) follow; the sun's square shadow cameras are left alone. */
#ifndef BD_STEREO_H
#define BD_STEREO_H

#include <windows.h>

typedef void (*StereoLogFn)(const char *fmt, ...);

void stereo_init(const char *exe_dir, StereoLogFn log);

/* At each Present, before the real one: keys, the eye flip, a stats line now and then. */
void stereo_on_present(void);

/* At SetVertexShaderConstantF: returns `data`, or `scratch` (room for `count` registers) holding the shifted copy. */
const float *stereo_filter(UINT start, const float *data, UINT count, float *scratch);

#endif
