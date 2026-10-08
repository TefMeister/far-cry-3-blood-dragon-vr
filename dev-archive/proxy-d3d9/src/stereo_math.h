/* stereo_math.h - the per-eye camera shift for Blood Dragon's vertex-shader constants (2026-10-08, /pd).
 * Pure functions: tools/stereo_test.c checks them against a camera actually moved sideways.
 *
 * Register map (measured 2026-10-08, dossier §6): c0-c3 view-projection without translation (camera-relative
 * input), c4-c7 view-projection with translation, c8-c11 projection, c12-c15 view (world -> view), c32-c35 inverse
 * view. Every block is 4 rows the shader dots with the position (row-major as uploaded).
 *
 * Moving the eye d world units along the camera's right axis (d > 0 = right eye) changes each block by one or three
 * numbers, because view-space x simply drops by d:
 *   view       row0.w -= d
 *   VP blocks  row0.w -= P00 * d   (row0.xyz = P00 * right for a symmetric projection, so P00 = |row0.xyz|)
 *   inv view   translation += d * right   (right = the first column of its rotation) */
#ifndef BD_STEREO_MATH_H
#define BD_STEREO_MATH_H

/* A perspective view-projection whose picture aspect (|row1| / |row0|) is within [aspect_min, aspect_max]. The
 * sun's shadow cameras have aspect 1 and orthographic blocks a zero last row, so both are refused. */
int st_is_persp_vp(const float m[16], float aspect_min, float aspect_max);

/* A rigid transform: orthonormal 3x3 rows, last row (0,0,0,1). Both the view and the inverse view are. */
int st_is_rigid(const float m[16]);

void st_shift_vp(float m[16], float d);
void st_shift_view(float m[16], float d);
void st_shift_inv_view(float m[16], float d);

#endif
