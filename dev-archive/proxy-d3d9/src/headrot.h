/* headrot.h - the headset's head rotation applied to Blood Dragon's camera blocks (2026-10-15, /pd). Pure functions:
 * tools/headrot_test.c checks them against a camera actually turned by the head.
 *
 * Convention (camera trace 2026-10-08, dossier §6): every block is 4 rows the shader dots with (p, 1). The view c12
 * has rows right, up, BACK (view space x right, y up, z backward: right-handed, -z forward), and the projection c8
 * has last row (0, 0, -1, 0), so clip.w = -view.z. That is OpenXR's own view convention, so a headset orientation
 * quaternion is used as it comes: no axis mirroring (Alan Wake, left-handed, needed one; this game does not).
 *
 * R is a 3x3 rotation, row-major (R[r*3+c]); its COLUMNS are the head's axes written in the game camera's frame.
 * The game decides the camera; the head turns on top of it:
 *   view       V' = R4^T V       rows 0..2 of V mixed by R^T (all four columns, so the translation follows)
 *   inv view   M' = M R4         the 3x3 part times R, translation column unchanged
 *   VP blocks  VP' = P R4^T V    P is recovered from the block itself (below), V rebuilt, P put back.
 * A VP block is r0 = xs V0 + p02 V2, r1 = ys V1 + p12 V2, r2 = A V2 + B e3, r3 = -V2 (e3 = (0,0,0,1)); V2 is unit,
 * so V2 = -r3, p02 = r0.V2, xs = |r0 - p02 V2|, and likewise for row 1, A = r2.V2, B = r2.w - A V2.w.
 * The per-eye shift (stereo_math.h) is applied AFTER the turn, so the eye moves along the head's right axis. */
#ifndef BD_HEADROT_H
#define BD_HEADROT_H

/* The lens recovered from a VP block: horizontal / vertical scale and off-centre terms. */
typedef struct { float xs, ys, p02, p12; } HrLens;

/* R from an OpenXR orientation quaternion. Normalises; identity for a zero quaternion. */
void hr_rot_from_quat(float qx, float qy, float qz, float qw, float R[9]);

/* RECENTRING. The heading of the head's forward (-z) about +y, and R <- Ry(-yaw0) R, so a head facing yaw0 faces the
 * game's forward. Pitch and roll stay measured from gravity. */
float hr_yaw(const float R[9]);
void  hr_remove_yaw(float R[9], float yaw0);

void hr_rotate_view(float m[16], const float R[9]);
void hr_rotate_inv_view(float m[16], const float R[9]);

/* Returns 0 (block untouched) when the block does not decompose as above. `lens` may be NULL. */
int hr_rotate_vp(float m[16], const float R[9], HrLens *lens);
int hr_vp_lens(const float m[16], HrLens *lens);

/* The lens as OpenXR fov tangents {left, right, up, down} (left and down negative). */
void hr_lens_tangents(const HrLens *lens, float tan4[4]);

#endif
