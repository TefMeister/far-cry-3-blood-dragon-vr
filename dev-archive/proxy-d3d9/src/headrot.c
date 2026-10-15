/* headrot.c - see headrot.h (2026-10-15, /pd). */
#include <math.h>
#include <string.h>

#include "headrot.h"

#define HR_UNIT_MIN     0.5f    /* |V2| from a VP block's last row: a unit axis, so far from these bounds */
#define HR_UNIT_MAX     2.0f
#define HR_SCALE_MIN    1e-4f   /* xs, ys below this: not a perspective lens */

static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void hr_rot_from_quat(float qx, float qy, float qz, float qw, float R[9]) {
    float n = sqrtf(qx * qx + qy * qy + qz * qz + qw * qw);
    if (n < 1e-6f) { qx = qy = qz = 0; qw = 1; n = 1; }
    qx /= n; qy /= n; qz /= n; qw /= n;
    R[0] = 1 - 2 * (qy * qy + qz * qz); R[1] = 2 * (qx * qy - qz * qw);     R[2] = 2 * (qx * qz + qy * qw);
    R[3] = 2 * (qx * qy + qz * qw);     R[4] = 1 - 2 * (qx * qx + qz * qz); R[5] = 2 * (qy * qz - qx * qw);
    R[6] = 2 * (qx * qz - qy * qw);     R[7] = 2 * (qy * qz + qx * qw);     R[8] = 1 - 2 * (qx * qx + qy * qy);
}

/* A turn by a about +y takes +z to (sin a, 0, cos a): read a from column 2 (the head's back axis). */
float hr_yaw(const float R[9]) { return atan2f(R[2], R[8]); }

void hr_remove_yaw(float R[9], float yaw0) {
    float c = cosf(yaw0), s = sinf(yaw0), o[9];
    int j;
    for (j = 0; j < 3; j++) {                 /* Ry(-a) = [[c, 0, -s], [0, 1, 0], [s, 0, c]] */
        o[j]     = c * R[j] - s * R[6 + j];
        o[3 + j] = R[3 + j];
        o[6 + j] = s * R[j] + c * R[6 + j];
    }
    memcpy(R, o, sizeof o);
}

/* rows 0..2 of a 4-column block <- R^T * rows 0..2 */
static void mix_rows(float *m, const float R[9]) {
    float o[12];
    int i, k, c;
    for (i = 0; i < 3; i++)
        for (c = 0; c < 4; c++) {
            float s = 0;
            for (k = 0; k < 3; k++) s += R[k * 3 + i] * m[k * 4 + c];
            o[i * 4 + c] = s;
        }
    memcpy(m, o, sizeof o);
}

void hr_rotate_view(float m[16], const float R[9]) { mix_rows(m, R); }

void hr_rotate_inv_view(float m[16], const float R[9]) {
    float o[9];
    int i, j, k;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) {
            float s = 0;
            for (k = 0; k < 3; k++) s += m[i * 4 + k] * R[k * 3 + j];
            o[i * 3 + j] = s;
        }
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) m[i * 4 + j] = o[i * 3 + j];
}

/* V (rows 0..2, 4 columns) and the lens from a VP block; 0 when it does not decompose. */
static int split_vp(const float m[16], float V[12], HrLens *lens, float *A, float *B) {
    float n, u[3];
    int c, r;
    for (c = 0; c < 4; c++) V[8 + c] = -m[12 + c];
    n = sqrtf(dot3(V + 8, V + 8));
    if (!(n > HR_UNIT_MIN && n < HR_UNIT_MAX)) return 0;
    for (r = 0; r < 2; r++) {
        const float *row = m + r * 4;
        float p = dot3(row, V + 8) / (n * n), s;
        for (c = 0; c < 3; c++) u[c] = row[c] - p * V[8 + c];
        s = sqrtf(dot3(u, u));
        if (!(s > HR_SCALE_MIN)) return 0;
        for (c = 0; c < 4; c++) V[r * 4 + c] = (row[c] - p * V[8 + c]) / s;
        if (r == 0) { lens->xs = s; lens->p02 = p; } else { lens->ys = s; lens->p12 = p; }
    }
    *A = dot3(m + 8, V + 8) / (n * n);
    *B = m[11] - *A * V[11];
    return 1;
}

int hr_vp_lens(const float m[16], HrLens *lens) {
    float V[12], A, B;
    HrLens l;
    if (!split_vp(m, V, &l, &A, &B)) return 0;
    if (lens) *lens = l;
    return 1;
}

int hr_rotate_vp(float m[16], const float R[9], HrLens *lens) {
    float V[12], A, B;
    HrLens l;
    int c;
    if (!split_vp(m, V, &l, &A, &B)) return 0;
    mix_rows(V, R);
    for (c = 0; c < 4; c++) {
        float e3 = c == 3 ? 1.0f : 0.0f;
        m[c]      = l.xs * V[c] + l.p02 * V[8 + c];
        m[4 + c]  = l.ys * V[4 + c] + l.p12 * V[8 + c];
        m[8 + c]  = A * V[8 + c] + B * e3;
        m[12 + c] = -V[8 + c];
    }
    if (lens) *lens = l;
    return 1;
}

/* ndc.x = xs * t - p02 with t = x / -z, so the picture's edges ndc = -1, +1 are t = (p02 - 1) / xs, (p02 + 1) / xs. */
void hr_lens_tangents(const HrLens *lens, float tan4[4]) {
    tan4[0] = (lens->p02 - 1.0f) / lens->xs;
    tan4[1] = (lens->p02 + 1.0f) / lens->xs;
    tan4[2] = (lens->p12 + 1.0f) / lens->ys;
    tan4[3] = (lens->p12 - 1.0f) / lens->ys;
}
