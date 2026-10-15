/* test_camera.h - the camera fixtures stereo_test.c and headrot_test.c share (2026-10-15, /pd): random numbers,
 * 4x4 maths, and Blood Dragon's blocks built the way the game uploads them (measured 2026-10-08: z-up world, view
 * rows right / up / back, every block 4 rows the shader dots with (p, 1)). Header-only, test code only. */
#ifndef BD_TEST_CAMERA_H
#define BD_TEST_CAMERA_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }

static void mul4(const float *a, const float *b, float *o) {   /* o = a * b, row-major */
    int r, c, k;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++) {
            float s = 0;
            for (k = 0; k < 4; k++) s += a[r * 4 + k] * b[k * 4 + c];
            o[r * 4 + c] = s;
        }
}
static void apply(const float *m, const float *p, float *o) {  /* o = m * p (p is a column 4-vector) */
    int r;
    for (r = 0; r < 4; r++) o[r] = m[r * 4] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3] * p[3];
}
static float diff4(const float *a, const float *b) {
    float m = 0;
    int i;
    for (i = 0; i < 4; i++) m = fmaxf(m, fabsf(a[i] - b[i]) / fmaxf(1.0f, fabsf(b[i])));
    return m;
}

typedef struct { float rot[9], cam[3]; } Cam;

/* Rows: right, up, back (the game's view has -forward in row 2). */
static Cam make_cam(float yaw, float pitch, const float *pos) {
    Cam c;
    float f[3] = { cosf(pitch) * sinf(yaw), cosf(pitch) * cosf(yaw), sinf(pitch) };   /* z up, like Dunia */
    float up0[3] = { 0, 0, 1 }, r[3], u[3], n;
    r[0] = f[1] * up0[2] - f[2] * up0[1];
    r[1] = f[2] * up0[0] - f[0] * up0[2];
    r[2] = f[0] * up0[1] - f[1] * up0[0];
    n = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    r[0] /= n; r[1] /= n; r[2] /= n;
    u[0] = r[1] * f[2] - r[2] * f[1];
    u[1] = r[2] * f[0] - r[0] * f[2];
    u[2] = r[0] * f[1] - r[1] * f[0];
    memcpy(c.rot, r, sizeof r);
    memcpy(c.rot + 3, u, sizeof u);
    c.rot[6] = -f[0]; c.rot[7] = -f[1]; c.rot[8] = -f[2];
    memcpy(c.cam, pos, 3 * sizeof(float));
    return c;
}
static void view_of(const Cam *c, float *v, int with_translation) {
    int r;
    memset(v, 0, 16 * sizeof(float));
    for (r = 0; r < 3; r++) {
        v[r * 4] = c->rot[r * 3]; v[r * 4 + 1] = c->rot[r * 3 + 1]; v[r * 4 + 2] = c->rot[r * 3 + 2];
        v[r * 4 + 3] = with_translation ? -(c->rot[r * 3] * c->cam[0] + c->rot[r * 3 + 1] * c->cam[1] +
                                            c->rot[r * 3 + 2] * c->cam[2]) : 0;
    }
    v[15] = 1;
}
static void inv_view_of(const Cam *c, float *iv) {
    int r;
    memset(iv, 0, 16 * sizeof(float));
    for (r = 0; r < 3; r++) {
        iv[r * 4] = c->rot[r]; iv[r * 4 + 1] = c->rot[3 + r]; iv[r * 4 + 2] = c->rot[6 + r];
        iv[r * 4 + 3] = c->cam[r];
    }
    iv[15] = 1;
}

#endif
