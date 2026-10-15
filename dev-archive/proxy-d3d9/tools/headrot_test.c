/* headrot_test.c - checks src/headrot.c against a camera ACTUALLY turned by the head (2026-10-15, /pd).
 * Builds Blood Dragon's blocks as measured (stereo_test.c's camera: z-up world, view rows right/up/back, c8 last row
 * (0, 0, -1, 0)), turns them with the shipped functions, and compares, for random points, with the blocks of a camera
 * rebuilt with the head's axes (new world axis j = sum_k R[k][j] * old axis k). Also: the quaternion against a direct
 * q v q* rotation, recentring, the lens tangents against the picture's edges, and turn-then-shift = an eye on the
 * head's right axis. Run with "wrong" to plant a mistake (R used where R^T belongs): the turn checks must then fail.
 * Exit 0 = all passed. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/headrot.h"
#include "../src/stereo_math.h"

#define CASES   500
#define POINTS  20
#define TOL     2e-3f

static int g_fail, g_checks, g_wrong;
static void expect(int ok, const char *what) {
    g_checks++;
    if (!ok && g_fail++ < 10) printf("FAIL %s\n", what);
}
static float frand(float a, float b) { return a + (b - a) * (float)rand() / (float)RAND_MAX; }

static void mul4(const float *a, const float *b, float *o) {
    int r, c, k;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++) {
            float s = 0;
            for (k = 0; k < 4; k++) s += a[r * 4 + k] * b[k * 4 + c];
            o[r * 4 + c] = s;
        }
}
static void apply(const float *m, const float *p, float *o) {
    int r;
    for (r = 0; r < 4; r++) o[r] = m[r * 4] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3] * p[3];
}
static float diff4(const float *a, const float *b) {
    float m = 0;
    int i;
    for (i = 0; i < 4; i++) m = fmaxf(m, fabsf(a[i] - b[i]) / fmaxf(1.0f, fabsf(b[i])));
    return m;
}

typedef struct { float rot[9], cam[3]; } Cam;   /* rows right, up, back (world) */

static Cam make_cam(float yaw, float pitch, const float *pos) {
    Cam c;
    float f[3] = { cosf(pitch) * sinf(yaw), cosf(pitch) * cosf(yaw), sinf(pitch) }, r[3], u[3], n;
    r[0] = f[1]; r[1] = -f[0]; r[2] = 0;               /* f x (0,0,1) */
    n = sqrtf(r[0] * r[0] + r[1] * r[1]);
    r[0] /= n; r[1] /= n;
    u[0] = r[1] * f[2] - r[2] * f[1];
    u[1] = r[2] * f[0] - r[0] * f[2];
    u[2] = r[0] * f[1] - r[1] * f[0];
    memcpy(c.rot, r, sizeof r);
    memcpy(c.rot + 3, u, sizeof u);
    c.rot[6] = -f[0]; c.rot[7] = -f[1]; c.rot[8] = -f[2];
    memcpy(c.cam, pos, sizeof c.cam);
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
static void proj_of(float p00, float p11, float p02, float p12, float *p) {   /* the measured c8 layout */
    memset(p, 0, 16 * sizeof(float));
    p[0] = p00; p[2] = p02; p[5] = p11; p[6] = p12; p[10] = -1.00001f; p[11] = -0.1f; p[14] = -1.0f;
}
/* the head's axes (columns of R, camera frame) turned into world axes: the camera the head looks through */
static Cam turned(const Cam *c, const float R[9]) {
    Cam t = *c;
    int j, k, w;
    for (j = 0; j < 3; j++)
        for (w = 0; w < 3; w++) {
            float s = 0;
            for (k = 0; k < 3; k++) s += R[k * 3 + j] * c->rot[k * 3 + w];
            t.rot[j * 3 + w] = s;
        }
    return t;
}
static void random_quat(float q[4]) {
    float n;
    int i;
    do {
        for (i = 0; i < 4; i++) q[i] = frand(-1, 1);
        n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    } while (n < 0.1f);
    for (i = 0; i < 4; i++) q[i] /= n;
}
static void quat_rotate(const float q[4], const float v[3], float o[3]) {   /* v + 2w(u x v) + 2 u x (u x v) */
    float u[3] = { q[0], q[1], q[2] }, w = q[3], t[3], c[3];
    t[0] = 2 * (u[1] * v[2] - u[2] * v[1]); t[1] = 2 * (u[2] * v[0] - u[0] * v[2]); t[2] = 2 * (u[0] * v[1] - u[1] * v[0]);
    c[0] = u[1] * t[2] - u[2] * t[1]; c[1] = u[2] * t[0] - u[0] * t[2]; c[2] = u[0] * t[1] - u[1] * t[0];
    o[0] = v[0] + w * t[0] + c[0]; o[1] = v[1] + w * t[1] + c[1]; o[2] = v[2] + w * t[2] + c[2];
}
static void transpose3(float R[9]) {
    float t;
    t = R[1]; R[1] = R[3]; R[3] = t;
    t = R[2]; R[2] = R[6]; R[6] = t;
    t = R[5]; R[5] = R[7]; R[7] = t;
}

int main(int argc, char **argv) {
    int k, j;
    g_wrong = argc > 1 && !strcmp(argv[1], "wrong");
    srand(777);
    for (k = 0; k < CASES; k++) {
        float pos[3] = { frand(-3000, 3000), frand(-3000, 3000), frand(-50, 300) }, q[4], R[9], Ru[9];
        Cam c = make_cam(frand(-3.1f, 3.1f), frand(-1.2f, 1.2f), pos), t;
        float p00 = frand(0.8f, 2.0f), P[16], V[16], C0[16], VP[16], IV[16], Vt[16], C0t[16], VPt[16], Rt[16];
        float jx = frand(-1e-3f, 1e-3f), jy = frand(-1e-3f, 1e-3f), d = frand(-0.05f, 0.05f);
        HrLens lens;
        random_quat(q);
        hr_rot_from_quat(q[0], q[1], q[2], q[3], R);
        for (j = 0; j < 3; j++) {               /* the quaternion: R's column j is q applied to axis j */
            float e[3] = { 0, 0, 0 }, o[3];
            e[j] = 1;
            quat_rotate(q, e, o);
            expect(fabsf(o[0] - R[j]) < 1e-4f && fabsf(o[1] - R[3 + j]) < 1e-4f && fabsf(o[2] - R[6 + j]) < 1e-4f,
                   "R's columns are the quaternion's turned axes");
        }
        proj_of(p00, p00 * 16.0f / 9.0f, jx, jy, P);
        view_of(&c, V, 1); view_of(&c, Rt, 0); inv_view_of(&c, IV);
        mul4(P, V, VP); mul4(P, Rt, C0);
        t = turned(&c, R);
        view_of(&t, Vt, 1); mul4(P, Vt, VPt);
        { float tmp[16]; view_of(&t, tmp, 0); mul4(P, tmp, C0t); }

        memcpy(Ru, R, sizeof R);
        if (g_wrong) transpose3(Ru);
        hr_rotate_view(V, Ru);
        hr_rotate_inv_view(IV, Ru);
        expect(hr_rotate_vp(VP, Ru, &lens), "c4 decomposes");
        expect(hr_rotate_vp(C0, Ru, NULL), "c0 decomposes");
        expect(fabsf(lens.xs - p00) < 1e-3f && fabsf(lens.p02 - jx) < 1e-4f && fabsf(lens.p12 - jy) < 1e-4f,
               "the lens comes back from the block");
        for (j = 0; j < POINTS; j++) {
            float p[4] = { pos[0] + frand(-500, 500), pos[1] + frand(-500, 500), pos[2] + frand(-100, 100), 1 };
            float rel[4] = { p[0] - pos[0], p[1] - pos[1], p[2] - pos[2], 1 }, a[4], b[4], back[4];
            apply(V, p, a); apply(Vt, p, b);
            expect(diff4(a, b) < TOL, "turned c12 = view of the turned camera");
            apply(VP, p, a); apply(VPt, p, b);
            expect(diff4(a, b) < TOL, "turned c4 = VP of the turned camera");
            apply(C0, rel, a);
            expect(diff4(a, b) < TOL, "turned c0 on (p - camera) = VP of the turned camera");
            apply(Vt, p, a); apply(IV, a, back);
            expect(diff4(back, p) < TOL, "turned c32 takes the turned camera's view space back to the world");
        }
        /* turn, then the per-eye shift: the eye must sit on the HEAD's right axis */
        {
            Cam e = t;
            float Ve[16], p[4] = { pos[0] + 30, pos[1] - 200, pos[2] + 5, 1 }, a[4], b[4];
            for (j = 0; j < 3; j++) e.cam[j] = pos[j] + d * t.rot[j];
            view_of(&e, Ve, 1);
            st_shift_view(V, d);
            apply(V, p, a); apply(Ve, p, b);
            expect(diff4(a, b) < TOL, "turn then shift = an eye on the head's right axis");
        }
    }
    {   /* recentring: a head turned yaw a and pitched b comes back as pitch b alone */
        for (k = 0; k < 200; k++) {
            float a = frand(-3.1f, 3.1f), b = frand(-1.2f, 1.2f), R[9], Ry[9], Rx[9];
            float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
            int r, c2, m;
            float y[9] = { ca, 0, sa, 0, 1, 0, -sa, 0, ca }, x[9] = { 1, 0, 0, 0, cb, -sb, 0, sb, cb };
            memcpy(Ry, y, sizeof y); memcpy(Rx, x, sizeof x);
            for (r = 0; r < 3; r++)
                for (c2 = 0; c2 < 3; c2++) {
                    float s = 0;
                    for (m = 0; m < 3; m++) s += Ry[r * 3 + m] * Rx[m * 3 + c2];
                    R[r * 3 + c2] = s;
                }
            expect(fabsf(remainderf(hr_yaw(R) - a, 6.2831853f)) < 1e-4f, "the head's heading is read back");
            hr_remove_yaw(R, hr_yaw(R));
            for (r = 0; r < 9; r++) expect(fabsf(R[r] - Rx[r]) < 1e-4f, "recentring leaves the pitch alone");
        }
    }
    {   /* the lens tangents land on the picture's edges */
        HrLens l = { 1.30323f, 2.31685f, 4e-4f, -3e-4f };
        float tn[4], P[16], o[4], pr[4] = { 0, 0, -10, 1 };
        proj_of(l.xs, l.ys, l.p02, l.p12, P);
        hr_lens_tangents(&l, tn);
        pr[0] = tn[1] * 10; apply(P, pr, o); expect(fabsf(o[0] / o[3] - 1) < 1e-4f, "right tangent = right edge");
        pr[0] = tn[0] * 10; apply(P, pr, o); expect(fabsf(o[0] / o[3] + 1) < 1e-4f, "left tangent = left edge");
        pr[0] = 0; pr[1] = tn[2] * 10; apply(P, pr, o); expect(fabsf(o[1] / o[3] - 1) < 1e-4f, "up tangent = top edge");
        pr[1] = tn[3] * 10; apply(P, pr, o); expect(fabsf(o[1] / o[3] + 1) < 1e-4f, "down tangent = bottom edge");
        expect(fabsf(2 * atanf(tn[2]) * 57.29578f - 46.7f) < 0.2f, "the measured lens is 46.7 degrees tall");
    }
    {   /* refused: a block whose last row is not a unit axis */
        float O[16] = { 0.01f, 0, 0, 0, 0, 0.01f, 0, 0, 0, 0, 0.001f, 0, 0, 0, 0, 1 }, R[9];
        hr_rot_from_quat(0, 0, 0, 1, R);
        expect(!hr_rotate_vp(O, R, NULL), "an orthographic block is refused");
    }
    printf("headrot_test%s: %d checks, %d failed\n", g_wrong ? " (PLANTED: R for R^T)" : "", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
