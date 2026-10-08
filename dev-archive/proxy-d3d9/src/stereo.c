/* stereo.c - see stereo.h (2026-10-08, /pd). Render thread only. */
#include <stdio.h>
#include <string.h>

#include "settings.h"
#include "stereo.h"
#include "stereo_math.h"

enum { MODE_ALTERNATE = 0, MODE_LEFT, MODE_RIGHT, MODE_COUNT };
static const char *const k_mode_name[MODE_COUNT] = { "alternate", "left only", "right only" };

static StereoLogFn g_log;
static int g_on, g_mode = MODE_ALTERNATE;
static float g_sep = STEREO_SEP_DEFAULT;
static int g_eye = 1;                 /* +1 right, -1 left: the eye of the frame now being drawn */
static int g_cam_ok;                  /* the last c0/c4 block was the player's camera */
static unsigned long g_frames, g_shifted[4], g_refused;
static DWORD g_last_stats;
static SHORT g_prev_keys[4];

static int key_pressed(int idx, int vk) {
    SHORT now = GetAsyncKeyState(vk);
    int edge = (now & 0x8000) && !(g_prev_keys[idx] & 0x8000);
    g_prev_keys[idx] = now;
    return edge;
}

void stereo_init(const char *exe_dir, StereoLogFn log) {
    char path[MAX_PATH];
    g_log = log;
    snprintf(path, sizeof path, "%s%s", exe_dir, STEREO_ON_FILE);
    g_on = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
    g_log("stereo: %s (numpad 5 on/off, 4/6 separation, 8 mode); separation %.3f, mode %s",
          g_on ? "ON from the start (fc3bd_vr_stereo_on.txt)" : "off", g_sep, k_mode_name[g_mode]);
}

void stereo_on_present(void) {
    DWORD now;
    if (key_pressed(0, VK_NUMPAD5)) { g_on = !g_on; g_log("stereo: %s", g_on ? "ON" : "off"); }
    if (key_pressed(1, VK_NUMPAD4) && g_sep > STEREO_SEP_STEP) {
        g_sep -= STEREO_SEP_STEP;
        g_log("stereo: separation %.3f", g_sep);
    }
    if (key_pressed(2, VK_NUMPAD6) && g_sep < STEREO_SEP_MAX) {
        g_sep += STEREO_SEP_STEP;
        g_log("stereo: separation %.3f", g_sep);
    }
    if (key_pressed(3, VK_NUMPAD8)) {
        g_mode = (g_mode + 1) % MODE_COUNT;
        g_log("stereo: mode %s", k_mode_name[g_mode]);
    }
    g_eye = g_mode == MODE_LEFT ? -1 : g_mode == MODE_RIGHT ? 1 : -g_eye;
    g_cam_ok = 0;
    g_frames++;
    now = GetTickCount();
    if (now - g_last_stats >= STEREO_STATS_MS) {
        if (g_on || g_shifted[0] || g_refused)
            g_log("stereo: %lu frames; shifted c0 %lu, c4 %lu, c12 %lu, c32 %lu; other cameras left alone %lu",
                  g_frames, g_shifted[0], g_shifted[1], g_shifted[2], g_shifted[3], g_refused);
        g_last_stats = now;
        g_frames = g_refused = 0;
        memset(g_shifted, 0, sizeof g_shifted);
    }
}

/* Block at register `reg` inside this upload, or NULL. */
static float *block(UINT start, UINT count, float *buf, UINT reg) {
    if (reg < start || reg + 4 > start + count) return NULL;
    return buf + (reg - start) * 4;
}

const float *stereo_filter(UINT start, const float *data, UINT count, float *scratch) {
    static const UINT vp_regs[2] = { STEREO_REG_VP_REL, STEREO_REG_VP };
    float d, *m;
    int i, touched = 0;
    if (!g_on || !data || count == 0 || count > STEREO_MAX_REGS) return data;
    if (start > STEREO_REG_INV_VIEW || start + count <= STEREO_REG_VP_REL) return data;
    d = g_eye * g_sep * 0.5f;
    memcpy(scratch, data, count * 4 * sizeof(float));
    for (i = 0; i < 2; i++) {
        m = block(start, count, scratch, vp_regs[i]);
        if (!m) continue;
        g_cam_ok = st_is_persp_vp(m, STEREO_ASPECT_MIN, STEREO_ASPECT_MAX);
        if (g_cam_ok) { st_shift_vp(m, d); g_shifted[i]++; touched = 1; }
        else g_refused++;
    }
    if (g_cam_ok && (m = block(start, count, scratch, STEREO_REG_VIEW)) && st_is_rigid(m)) {
        st_shift_view(m, d);
        g_shifted[2]++;
        touched = 1;
    }
    if (g_cam_ok && (m = block(start, count, scratch, STEREO_REG_INV_VIEW)) && st_is_rigid(m)) {
        st_shift_inv_view(m, d);
        g_shifted[3]++;
        touched = 1;
    }
    return touched ? scratch : data;
}
