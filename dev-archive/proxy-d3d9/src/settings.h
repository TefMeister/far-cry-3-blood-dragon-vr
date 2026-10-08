/* settings.h - every number the Blood Dragon d3d9 logger uses, in one place. */
#ifndef BD_SETTINGS_H
#define BD_SETTINGS_H

/* IDirect3D9 / IDirect3DDevice9 vtable slots (d3d9.h declaration order). */
#define VT_D3D9_CREATEDEVICE        16
#define VT_DEV_PRESENT              17
#define VT_DEV_CREATEVERTEXSHADER   91
#define VT_DEV_SETVERTEXSHADER      92
#define VT_DEV_SETVSCONSTANTF       94

/* Vertex-shader float registers tracked per frame. vs_3_0 has 256. */
#define BD_VS_REGS                  256

/* Snapshot frames: the first-write-per-register-per-frame log (Far Cry 2's
 * trick: the HUD draws last and overwrites low registers, so a last-write
 * view only ever shows UI). One snapshot at this frame, then every
 * SNAP_EVERY frames, up to SNAP_MAX; creating the arm file beside the log
 * asks for one on the next frame as well. */
#define SNAP_FIRST_FRAME            600
#define SNAP_EVERY                  1800
#define SNAP_MAX                    20
#define SNAP_MAX_LINES              600
#define SNAP_ARM_FILE               "fc3bd_vr_snap.txt"

/* Camera trace (2026-10-08, /lm): creating this file asks for ONE frame in which every DIFFERENT 4-row block written
 * at the camera registers below is logged, with the shader that was bound. The first-write snapshot catches the
 * shadow pass (a sun camera with a square projection) before the player's view, so the view has to be picked out of
 * all the passes by its 16:9 projection. */
#define CAMTRACE_ARM_FILE           "fc3bd_vr_camtrace.txt"
#define CAMTRACE_MAX_LINES          900
#define CAMTRACE_REG_COUNT          6
#define CAMTRACE_REGS               { 0, 4, 8, 12, 16, 32 }

/* Constant-table bookkeeping. */
#define SHADER_SLOTS                16384   /* shaders remembered, open addressing */
#define SHADER_CONSTS_MAX           24      /* named constants kept per shader */
#define NAME_TABLE_MAX              512     /* distinct (name, register, count) logged once each */

/* Log file, written beside the game's exe; %TEMP% if that fails. */
#define LOG_FILE_NAME               "fc3bd_vr_log.txt"

#endif
