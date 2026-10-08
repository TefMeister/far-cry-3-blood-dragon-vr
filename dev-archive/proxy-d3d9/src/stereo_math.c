/* stereo_math.c - see stereo_math.h (2026-10-08, /pd). */
#include <math.h>

#include "stereo_math.h"

#define ST_W_ROW_MIN    0.5f    /* |last row xyz| of a perspective VP is the (unit) forward axis */
#define ST_W_ROW_MAX    2.0f
#define ST_ROW_MIN      1e-3f
#define ST_RIGID_EPS    1e-2f

static float len3(const float *v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

int st_is_persp_vp(const float m[16], float aspect_min, float aspect_max) {
    float l0 = len3(m), l1 = len3(m + 4), lw = len3(m + 12), aspect;
    int i;
    for (i = 0; i < 16; i++)
        if (!isfinite(m[i])) return 0;
    if (lw < ST_W_ROW_MIN || lw > ST_W_ROW_MAX || l0 < ST_ROW_MIN || l1 < ST_ROW_MIN || len3(m + 8) < ST_ROW_MIN)
        return 0;
    aspect = l1 / l0;
    return aspect >= aspect_min && aspect <= aspect_max;
}

int st_is_rigid(const float m[16]) {
    int r;
    if (fabsf(m[12]) > ST_RIGID_EPS || fabsf(m[13]) > ST_RIGID_EPS || fabsf(m[14]) > ST_RIGID_EPS ||
        fabsf(m[15] - 1.0f) > ST_RIGID_EPS)
        return 0;
    for (r = 0; r < 3; r++)
        if (fabsf(len3(m + r * 4) - 1.0f) > ST_RIGID_EPS) return 0;
    return fabsf(dot3(m, m + 4)) < ST_RIGID_EPS && fabsf(dot3(m, m + 8)) < ST_RIGID_EPS &&
           fabsf(dot3(m + 4, m + 8)) < ST_RIGID_EPS;
}

void st_shift_vp(float m[16], float d) { m[3] -= len3(m) * d; }

void st_shift_view(float m[16], float d) { m[3] -= d; }

void st_shift_inv_view(float m[16], float d) {
    m[3] += d * m[0];
    m[7] += d * m[4];
    m[11] += d * m[8];
}
