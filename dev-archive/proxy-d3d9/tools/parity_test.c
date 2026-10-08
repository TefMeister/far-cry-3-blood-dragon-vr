/* parity_test.c - the frame's eye label must match the eye its camera was actually shifted for (2026-10-08, /pd).
 * Runs the SHIPPED src/stereo.c: each frame uploads a 16:9 perspective VP at c4 (the sign of the shift says which eye
 * the frame was drawn with), then asks stereo_frame_eye() as the picture code will at Present, then stereo_on_present().
 * A label that disagrees with the shift is the Far Cry 2 trap: stereo with the depth inside out. Also: a frame with
 * no player camera (a menu) is labelled 0, and the stand-alone frames are alternately left and right.
 * Exit 0 = all passed. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../src/stereo.h"

#define FRAMES 200

static int g_fail, g_checks;
static void expect(int ok, const char *what, int frame) {
    g_checks++;
    if (!ok && g_fail++ < 10) printf("FAIL frame %d: %s\n", frame, what);
}
static void quiet_log(const char *fmt, ...) { (void)fmt; }

int main(int argc, char **argv) {
    char dir[MAX_PATH], flag[MAX_PATH];
    float vp[16] = { 1.3f, 0, 0, 5.0f, 0, 2.3111f, 0, 0, 0, 0, -1.0f, -0.1f, 0.6f, 0.2f, -0.77f, 0 };
    float scratch[16 * 4];
    int f, last_eye = 0, menus = 0;
    (void)argc;
    /* stereo_init() reads the on-switch beside the exe: plant it beside this test */
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    *(strrchr(dir, '\\') + 1) = 0;
    snprintf(flag, sizeof flag, "%sfc3bd_vr_stereo_on.txt", dir);
    fclose(fopen(flag, "w"));
    stereo_init(dir, quiet_log);
    DeleteFileA(flag);
    (void)argv;
    for (f = 0; f < FRAMES; f++) {
        int menu = (f % 17) == 5, label, drawn = 0;
        if (!menu) {
            const float *out = stereo_filter(4, vp, 4, scratch);
            float dw = vp[3] - out[3];          /* = P00 * d: positive = right eye */
            drawn = dw > 0 ? 1 : dw < 0 ? -1 : 0;
            expect(drawn != 0, "the player camera was shifted", f);
        } else {
            menus++;
        }
        label = stereo_frame_eye();
        if (menu) expect(label == 0, "a frame with no player camera is labelled 0", f);
        else expect(label == drawn, "the label is the eye the camera was drawn for", f);
        if (!menu && last_eye) expect(label == -last_eye, "camera frames alternate", f);
        if (!menu) last_eye = label;
        else last_eye = -last_eye;               /* the flip still happens on a menu frame */
        stereo_on_present();
    }
    printf("parity_test: %d checks (%d menu frames), %d failed\n", g_checks, menus, g_fail);
    return g_fail ? 1 : 0;
}
