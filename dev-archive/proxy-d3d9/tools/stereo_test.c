/* stereo_test.c - checks src/stereo_math.c against a camera ACTUALLY moved sideways (2026-10-08, /pd).
 * Builds the blocks the way Blood Dragon uploads them (measured 2026-10-08): view c12, projection c8, camera-relative
 * view-projection c0 (used with p - camera), full view-projection c4, inverse view c32. Shifts them with the shipped
 * functions and compares, for random points, with the blocks of a camera rebuilt at camera + d * right.
 * Exit 0 = all passed. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/stereo_math.h"
#include "test_camera.h"

#define CASES       200
#define POINTS      20
#define TOL         2e-3f
#define ASPECT_MIN  1.2f
#define ASPECT_MAX  2.5f

static int g_fail, g_checks;
static void expect(int ok, const char *what) {
    g_checks++;
    if (!ok && g_fail++ < 10) printf("FAIL %s\n", what);
}
static void proj_of(float p00, float p11, float *p) {   /* the measured c8 layout: near 0.1 */
    memset(p, 0, 16 * sizeof(float));
    p[0] = p00; p[5] = p11; p[10] = -1.00001f; p[11] = -0.1f; p[14] = -1.0f;
}

int main(void) {
    int k, j;
    srand(12345);
    for (k = 0; k < CASES; k++) {
        float pos[3] = { frand(-3000, 3000), frand(-3000, 3000), frand(-50, 300) };
        Cam c = make_cam(frand(-3.1f, 3.1f), frand(-1.2f, 1.2f), pos), e;
        float d = frand(-0.05f, 0.05f), p00 = frand(0.8f, 2.0f), P[16], V[16], R[16], VP[16], C0[16], IV[16];
        float Ve[16], VPe[16], IVe[16], eye[3];
        proj_of(p00, p00 * 16.0f / 9.0f, P);
        view_of(&c, V, 1); view_of(&c, R, 0); inv_view_of(&c, IV);
        mul4(P, V, VP); mul4(P, R, C0);
        for (j = 0; j < 3; j++) eye[j] = c.cam[j] + d * c.rot[j];   /* + d along right */
        e = make_cam(0, 0, eye);
        memcpy(e.rot, c.rot, sizeof c.rot);
        view_of(&e, Ve, 1); mul4(P, Ve, VPe); inv_view_of(&e, IVe);

        {   /* control: the UNshifted VP must NOT match the moved camera, or this test proves nothing */
            float p[4] = { pos[0] + 10, pos[1] + 400, pos[2], 1 }, a[4], b[4];
            apply(VP, p, a); apply(VPe, p, b);
            if (fabsf(d) > 0.01f) expect(diff4(a, b) > 1e-5f, "control: the unshifted c4 differs from the moved camera");
        }
        expect(st_is_persp_vp(C0, ASPECT_MIN, ASPECT_MAX), "c0 is a 16:9 perspective VP");
        expect(st_is_persp_vp(VP, ASPECT_MIN, ASPECT_MAX), "c4 is a 16:9 perspective VP");
        expect(st_is_rigid(V) && st_is_rigid(IV), "view and inverse view are rigid");
        expect(!st_is_rigid(VP), "a VP is not rigid");

        st_shift_view(V, d); st_shift_vp(VP, d); st_shift_vp(C0, d); st_shift_inv_view(IV, d);
        for (j = 0; j < POINTS; j++) {
            float p[4] = { pos[0] + frand(-500, 500), pos[1] + frand(-500, 500), pos[2] + frand(-100, 100), 1 };
            float rel[4] = { p[0] - c.cam[0], p[1] - c.cam[1], p[2] - c.cam[2], 1 };
            float a[4], b[4], vs[4], back[4];
            apply(V, p, a); apply(Ve, p, b);
            expect(diff4(a, b) < TOL, "shifted c12 = view of the moved camera");
            apply(VP, p, a); apply(VPe, p, b);
            expect(diff4(a, b) < TOL, "shifted c4 = VP of the moved camera");
            apply(C0, rel, a);
            expect(diff4(a, b) < TOL, "shifted c0 on (p - ORIGINAL camera) = VP of the moved camera");
            apply(Ve, p, vs); apply(IV, vs, back);
            expect(diff4(back, p) < TOL, "shifted c32 takes the moved camera's view space back to the world");
            (void)IVe;
        }
    }
    {   /* what must be refused */
        float P[16], V[16], VP[16], O[16] = { 0.01f, 0, 0, 0, 0, 0.01f, 0, 0, 0, 0, 0.001f, 0, 0, 0, 0, 1 };
        float pos[3] = { 0, 0, 2000 };
        Cam s = make_cam(0.9f, -1.2f, pos);
        proj_of(0.4759f, 0.4759f, P);   /* the sun camera's square lens, measured */
        view_of(&s, V, 1);
        mul4(P, V, VP);
        expect(!st_is_persp_vp(VP, ASPECT_MIN, ASPECT_MAX), "the square shadow camera is refused");
        expect(!st_is_persp_vp(O, ASPECT_MIN, ASPECT_MAX), "an orthographic block is refused");
    }
    printf("stereo_test: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
