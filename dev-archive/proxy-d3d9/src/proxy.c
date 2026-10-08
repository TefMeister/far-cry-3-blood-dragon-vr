/* proxy.c - Blood Dragon d3d9.dll proxy: a camera-register logger.
 *
 * Goal: one flat run shows where Blood Dragon's ViewMatrix lands in the
 * vertex-shader constant registers, so the Far Cry 2 stereo method (rewrite
 * the view per eye inside SetVertexShaderConstantF) can be ported.
 *
 * It does two things, both read-only:
 *   1. Reads every vertex shader's constant table at CreateVertexShader and
 *      logs each distinct (name, register, count) once - "ViewMatrix -> c12 x4".
 *   2. On snapshot frames, logs the FIRST write to each register in that
 *      frame, with the current shader's name for it and the values.
 * Nothing the game sends is changed.
 *
 * Load path: FC3.dll imports d3d9.dll!Direct3DCreate9 only, so a d3d9.dll
 * beside fc3_blooddragon.exe (in bin\) is loaded first. The real one is
 * loaded from the system folder by full path. */
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "MinHook.h"
#include "ctab.h"
#include "settings.h"

/* ---------------- log ---------------- */

static FILE *g_log;
static char g_dir[MAX_PATH];
static CRITICAL_SECTION g_cs;

static void log_open(void) {
    char path[MAX_PATH];
    char *slash;

    GetModuleFileNameA(NULL, g_dir, MAX_PATH);
    slash = strrchr(g_dir, '\\');
    if (slash) slash[1] = 0;
    snprintf(path, sizeof path, "%s%s", g_dir, LOG_FILE_NAME);
    g_log = fopen(path, "w");
    if (!g_log) {
        GetTempPathA(MAX_PATH, g_dir);
        snprintf(path, sizeof path, "%s%s", g_dir, LOG_FILE_NAME);
        g_log = fopen(path, "w");
    }
}

static void logf_(const char *fmt, ...) {
    va_list ap;
    if (!g_log) return;
    fprintf(g_log, "[%lu] ", (unsigned long)GetTickCount());
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

/* ---------------- constant-table registry ---------------- */

typedef struct {
    short name_id; /* index into g_names */
    short reg;
    short count;
} reg_entry;

typedef struct {
    void *shader; /* IDirect3DVertexShader9*, the key */
    int n;
    reg_entry e[SHADER_CONSTS_MAX];
} shader_slot;

static shader_slot *g_shaders; /* SHADER_SLOTS entries */
static char g_names[NAME_TABLE_MAX][BD_CTAB_NAME_MAX];
static int g_name_count;
static unsigned g_seen_triples[NAME_TABLE_MAX]; /* name_id<<20 | reg<<8 | count */
static int g_seen_count;
static int g_triples_full_logged;
static long g_shaders_seen, g_shaders_with_ctab;

static int name_id(const char *name) {
    int i;
    for (i = 0; i < g_name_count; i++) {
        if (strcmp(g_names[i], name) == 0) return i;
    }
    if (g_name_count >= NAME_TABLE_MAX) return -1;
    lstrcpynA(g_names[g_name_count], name, BD_CTAB_NAME_MAX);
    return g_name_count++;
}

static shader_slot *slot_for(void *shader, int create) {
    unsigned h = (unsigned)(((ULONG_PTR)shader >> 4) * 2654435761u) % SHADER_SLOTS;
    unsigned i;
    if (!g_shaders || !shader) return NULL;
    for (i = 0; i < SHADER_SLOTS; i++) {
        shader_slot *s = &g_shaders[(h + i) % SHADER_SLOTS];
        if (s->shader == shader) return s;
        if (s->shader == NULL) {
            if (!create) return NULL;
            s->shader = shader;
            s->n = 0;
            return s;
        }
    }
    return NULL; /* table full: that shader simply goes unnamed */
}

static void note_triple(int id, int reg, int count) {
    unsigned key = ((unsigned)id << 20) | ((unsigned)reg << 8) | ((unsigned)count & 0xFF);
    int i;
    for (i = 0; i < g_seen_count; i++) {
        if (g_seen_triples[i] == key) return;
    }
    if (g_seen_count >= NAME_TABLE_MAX) {
        if (!g_triples_full_logged) {
            logf_("ctab: distinct-constant table full (%d); later ones not logged", NAME_TABLE_MAX);
            g_triples_full_logged = 1;
        }
        return;
    }
    g_seen_triples[g_seen_count++] = key;
    logf_("ctab: %s -> c%d x%d (first seen in shader #%ld)", g_names[id], reg, count, g_shaders_seen);
}

static void register_shader(void *shader, const DWORD *bytecode) {
    bd_const c[SHADER_CONSTS_MAX];
    int n, i, isv;
    shader_slot *s;

    g_shaders_seen++;
    n = bd_ctab_parse(bytecode, 0, c, SHADER_CONSTS_MAX, &isv);
    if (n < 0) return;
    g_shaders_with_ctab++;
    s = slot_for(shader, 1);
    if (s) s->n = 0;
    for (i = 0; i < n; i++) {
        int id;
        if (c[i].regset != BD_RS_FLOAT4) continue;
        id = name_id(c[i].name);
        if (id < 0) continue;
        note_triple(id, c[i].reg, c[i].count);
        if (s && s->n < SHADER_CONSTS_MAX) {
            s->e[s->n].name_id = (short)id;
            s->e[s->n].reg = (short)c[i].reg;
            s->e[s->n].count = (short)c[i].count;
            s->n++;
        }
    }
}

static const char *name_at(void *shader, int reg) {
    shader_slot *s = slot_for(shader, 0);
    int i;
    if (!s) return "?";
    for (i = 0; i < s->n; i++) {
        if (reg >= s->e[i].reg && reg < s->e[i].reg + s->e[i].count) return g_names[s->e[i].name_id];
    }
    return "-";
}

/* ---------------- snapshot state ---------------- */

static long g_frame;
static int g_snapping, g_snaps_done, g_snap_lines;
static unsigned char g_written[BD_VS_REGS];
static void *g_cur_vs;

/* camera trace state (settings.h, CAMTRACE_*) */
static const UINT g_ct_regs[CAMTRACE_REG_COUNT] = CAMTRACE_REGS;
static int g_ct_on, g_ct_lines;
static float g_ct_last[CAMTRACE_REG_COUNT][16];
static int g_ct_have[CAMTRACE_REG_COUNT];
static unsigned long g_ct_writes[CAMTRACE_REG_COUNT], g_ct_distinct[CAMTRACE_REG_COUNT];

static int file_present_then_delete(const char *name) {
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s%s", g_dir, name);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return 0;
    DeleteFileA(path);
    return 1;
}

static void camtrace_constants(UINT start, const float *data, UINT count) {
    int i, r;
    if (!data || count < 4) return;
    for (i = 0; i < CAMTRACE_REG_COUNT; i++) {
        if (g_ct_regs[i] != start) continue;
        g_ct_writes[i]++;
        if (g_ct_have[i] && memcmp(g_ct_last[i], data, sizeof g_ct_last[i]) == 0) return;
        memcpy(g_ct_last[i], data, sizeof g_ct_last[i]);
        g_ct_have[i] = 1;
        g_ct_distinct[i]++;
        if (g_ct_lines >= CAMTRACE_MAX_LINES) return;
        g_ct_lines++;
        logf_("cam f=%ld c%u write #%lu (distinct #%lu) shader=%p", g_frame, start, g_ct_writes[i], g_ct_distinct[i],
              g_cur_vs);
        for (r = 0; r < 4; r++)
            logf_("   c%-3u % .5f % .5f % .5f % .5f", start + r, data[r * 4], data[r * 4 + 1], data[r * 4 + 2],
                  data[r * 4 + 3]);
        return;
    }
}

static int arm_file_present(void) {
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s%s", g_dir, SNAP_ARM_FILE);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return 0;
    DeleteFileA(path);
    return 1;
}

static void frame_boundary(void) {
    int want;
    if (g_snapping) {
        logf_("snap: end of frame %ld (%d lines)", g_frame, g_snap_lines);
        g_snapping = 0;
    }
    if (g_ct_on) {
        int i;
        for (i = 0; i < CAMTRACE_REG_COUNT; i++)
            logf_("camtrace: c%u written %lu times, %lu different values", g_ct_regs[i], g_ct_writes[i],
                  g_ct_distinct[i]);
        logf_("camtrace: end of frame %ld", g_frame);
        g_ct_on = 0;
    }
    g_frame++;
    if (file_present_then_delete(CAMTRACE_ARM_FILE)) {
        g_ct_on = 1;
        g_ct_lines = 0;
        memset(g_ct_have, 0, sizeof g_ct_have);
        memset(g_ct_writes, 0, sizeof g_ct_writes);
        memset(g_ct_distinct, 0, sizeof g_ct_distinct);
        logf_("camtrace: frame %ld begins", g_frame);
    }
    want = g_snaps_done < SNAP_MAX &&
           (g_frame == SNAP_FIRST_FRAME ||
            (g_frame > SNAP_FIRST_FRAME && (g_frame - SNAP_FIRST_FRAME) % SNAP_EVERY == 0));
    if (arm_file_present()) want = 1;
    if (want) {
        g_snapping = 1;
        g_snaps_done++;
        g_snap_lines = 0;
        memset(g_written, 0, sizeof g_written);
        logf_("snap: frame %ld begins (snapshot %d; shaders seen %ld, with a constant table %ld)", g_frame,
              g_snaps_done, g_shaders_seen, g_shaders_with_ctab);
    }
}

static void snap_constants(UINT start, const float *data, UINT count) {
    UINT r, first = 0, rows;
    int any = 0;
    if (!data || start >= BD_VS_REGS || g_snap_lines >= SNAP_MAX_LINES) return;
    for (r = start; r < start + count && r < BD_VS_REGS; r++) {
        if (!g_written[r]) {
            g_written[r] = 1;
            if (!any) first = r;
            any = 1;
        }
    }
    if (!any) return;
    rows = count < 4 ? count : 4;
    g_snap_lines++;
    logf_("snap f=%ld c%u x%u first-new=c%u name=%s", g_frame, start, count, first, name_at(g_cur_vs, (int)start));
    for (r = 0; r < rows; r++) {
        const float *v = data + r * 4;
        logf_("   c%-3u % .5f % .5f % .5f % .5f", start + r, v[0], v[1], v[2], v[3]);
    }
}

/* ---------------- hooks ---------------- */

typedef HRESULT(WINAPI *CreateDevice_t)(void *, UINT, DWORD, HWND, DWORD, void *, void **);
typedef HRESULT(WINAPI *Present_t)(void *, const RECT *, const RECT *, HWND, const void *);
typedef HRESULT(WINAPI *CreateVS_t)(void *, const DWORD *, void **);
typedef HRESULT(WINAPI *SetVS_t)(void *, void *);
typedef HRESULT(WINAPI *SetVSConstF_t)(void *, UINT, const float *, UINT);
typedef void *(WINAPI *Direct3DCreate9_t)(UINT);

static CreateDevice_t o_CreateDevice;
static Present_t o_Present;
static CreateVS_t o_CreateVS;
static SetVS_t o_SetVS;
static SetVSConstF_t o_SetVSConstF;
static Direct3DCreate9_t o_Direct3DCreate9;
static int g_mh_ready, g_d3d9_hooked, g_dev_hooked;

static int hook(void *target, void *detour, void **orig, const char *name) {
    MH_STATUS st = MH_CreateHook(target, detour, orig);
    if (st == MH_OK) st = MH_EnableHook(target);
    logf_("hook %s at %p: %s", name, target, MH_StatusToString(st));
    return st == MH_OK;
}

static HRESULT WINAPI h_Present(void *dev, const RECT *a, const RECT *b, HWND w, const void *r) {
    EnterCriticalSection(&g_cs);
    frame_boundary();
    LeaveCriticalSection(&g_cs);
    return o_Present(dev, a, b, w, r);
}

static HRESULT WINAPI h_CreateVS(void *dev, const DWORD *fn, void **out) {
    HRESULT hr = o_CreateVS(dev, fn, out);
    if (SUCCEEDED(hr) && out && *out && fn) {
        EnterCriticalSection(&g_cs);
        register_shader(*out, fn);
        LeaveCriticalSection(&g_cs);
    }
    return hr;
}

static HRESULT WINAPI h_SetVS(void *dev, void *vs) {
    g_cur_vs = vs;
    return o_SetVS(dev, vs);
}

static HRESULT WINAPI h_SetVSConstF(void *dev, UINT start, const float *data, UINT count) {
    if (g_snapping || g_ct_on) {
        EnterCriticalSection(&g_cs);
        if (g_snapping) snap_constants(start, data, count);
        if (g_ct_on) camtrace_constants(start, data, count);
        LeaveCriticalSection(&g_cs);
    }
    return o_SetVSConstF(dev, start, data, count);
}

static void hook_device(void *dev) {
    void **vt = *(void ***)dev;
    if (g_dev_hooked) return;
    g_dev_hooked = 1;
    hook(vt[VT_DEV_PRESENT], (void *)h_Present, (void **)&o_Present, "Present");
    hook(vt[VT_DEV_CREATEVERTEXSHADER], (void *)h_CreateVS, (void **)&o_CreateVS, "CreateVertexShader");
    hook(vt[VT_DEV_SETVERTEXSHADER], (void *)h_SetVS, (void **)&o_SetVS, "SetVertexShader");
    hook(vt[VT_DEV_SETVSCONSTANTF], (void *)h_SetVSConstF, (void **)&o_SetVSConstF, "SetVertexShaderConstantF");
}

static HRESULT WINAPI h_CreateDevice(void *d3d, UINT adapter, DWORD type, HWND wnd, DWORD flags, void *pp,
                                     void **out) {
    HRESULT hr = o_CreateDevice(d3d, adapter, type, wnd, flags, pp, out);
    logf_("CreateDevice adapter=%u type=%lu flags=0x%08lx -> hr=0x%08lx device=%p", adapter, (unsigned long)type,
          (unsigned long)flags, (unsigned long)hr, out ? *out : NULL);
    if (SUCCEEDED(hr) && out && *out) {
        EnterCriticalSection(&g_cs);
        hook_device(*out);
        LeaveCriticalSection(&g_cs);
    }
    return hr;
}

/* ---------------- export ---------------- */

void *WINAPI Direct3DCreate9(UINT sdk) {
    void *d3d;
    if (!o_Direct3DCreate9) return NULL;
    d3d = o_Direct3DCreate9(sdk);
    logf_("Direct3DCreate9(sdk=%u) -> %p", sdk, d3d);
    if (d3d && g_mh_ready) {
        EnterCriticalSection(&g_cs);
        if (!g_d3d9_hooked) {
            void **vt = *(void ***)d3d;
            g_d3d9_hooked = 1;
            hook(vt[VT_D3D9_CREATEDEVICE], (void *)h_CreateDevice, (void **)&o_CreateDevice, "CreateDevice");
        }
        LeaveCriticalSection(&g_cs);
    }
    return d3d;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char sys[MAX_PATH];
        HMODULE real;
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_cs);
        log_open();
        logf_("Blood Dragon d3d9 camera logger attached (pid %lu); read-only, nothing is changed",
              (unsigned long)GetCurrentProcessId());
        GetSystemDirectoryA(sys, MAX_PATH);
        lstrcatA(sys, "\\d3d9.dll");
        real = LoadLibraryA(sys);
        o_Direct3DCreate9 = real ? (Direct3DCreate9_t)GetProcAddress(real, "Direct3DCreate9") : NULL;
        logf_("real d3d9: %s -> %p, Direct3DCreate9=%p", sys, (void *)real, (void *)o_Direct3DCreate9);
        g_shaders = (shader_slot *)VirtualAlloc(NULL, sizeof(shader_slot) * SHADER_SLOTS, MEM_COMMIT | MEM_RESERVE,
                                                PAGE_READWRITE);
        g_mh_ready = MH_Initialize() == MH_OK;
        logf_("MinHook %s; shader table %s", g_mh_ready ? "ready" : "FAILED - logging only the create call",
              g_shaders ? "allocated" : "FAILED");
    } else if (reason == DLL_PROCESS_DETACH) {
        logf_("detach after %ld frames, %d snapshots", g_frame, g_snaps_done);
        if (g_log) fclose(g_log);
    }
    return TRUE;
}
