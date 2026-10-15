/* settings.h - every number the Blood Dragon d3d9 logger uses, in one place. */
#ifndef BD_SETTINGS_H
#define BD_SETTINGS_H

/* IDirect3D9 / IDirect3DDevice9 vtable slots (d3d9.h declaration order). */
#define VT_D3D9_CREATEDEVICE        16
#define VT_DEV_RESET                16
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

/* Per-eye shift (stereo.c, 2026-10-08). Registers measured 2026-10-08 (dossier §6). Dunia units are about metres
 * (Far Cry 2), so the default separation is a real eye distance. */
#define STEREO_ON_FILE              "fc3bd_vr_stereo_on.txt"
#define STEREO_SEP_DEFAULT          0.065f
#define STEREO_SEP_STEP             0.005f
#define STEREO_SEP_MAX              0.5f
#define STEREO_STATS_MS             5000u
#define STEREO_MAX_REGS             256u
#define STEREO_ASPECT_MIN           1.2f    /* the player's lens is 16:9 (1.778); the sun's is square (1.0) */
#define STEREO_ASPECT_MAX           2.5f
#define STEREO_REG_VP_REL           0u      /* c0-c3: view-projection, no translation (camera-relative) */
#define STEREO_REG_VP               4u      /* c4-c7: view-projection with translation */
#define STEREO_REG_VIEW             12u     /* c12-c15: view */
#define STEREO_REG_INV_VIEW         32u     /* c32-c35: inverse view */

/* Side by side (sbs.c, 2026-10-08): switch file, checked once a second while the game runs. */
#define SBS_ON_FILE                 "fc3bd_vr_sbs.txt"
#define SBS_SWITCH_CHECK_MS         1000u
#define SBS_STATS_MS                5000u

/* Headset output (xr_capture.c + xr_session.c, 2026-10-11): each eye's finished frame to OpenXR as a world-locked
 * screen. OFF unless the switch file sits beside the exe (checked once a second; the headset thread starts the first
 * time it is seen; removing it stops the capture, the session stays). Runtime for this game only: [vr] RuntimeJson=
 * in the ini (must be a 32-bit runtime); [vr] TestPattern=1 shows left red / right blue instead of the game. */
#define XR_ON_FILE                  "fc3bd_vr_openxr.txt"
#define XR_INI_FILE                 "fc3bd_vr.ini"
#define XR_LOADER_FILE              "openxr_loader.dll"
#define XR_APP_NAME                 "far-cry-3-blood-dragon-vr"
#define XR_SWITCH_CHECK_MS          1000u
#define XR_SCREEN_DIST_M            2.0f    /* screen distance ahead of where the head was when it was placed */
#define XR_SCREEN_WIDTH_M           3.2f    /* screen width; height follows the frame's aspect */
#define XR_MAX_W                    4096
#define XR_MAX_H                    4096
#define XR_IDLE_SLEEP_MS            10u
#define XR_STATS_MS                 5000u
#define XR_MAX_SWAPCHAIN_FORMATS    64u
#define XR_EVENT_LOG_MAX            8       /* session-state lines logged before going quiet */

/* Head tracking and projection views (xr_views.c, 2026-10-15): [vr] keys in XR_INI_FILE, read when the headset
 * output starts. HeadTrack=1 turns the player's camera with the head (only while the per-eye shift is on);
 * Layers=projection shows each eye as a projection view at the pose it was drawn with, with the game's own lens. */
#define XR_INI_HEADTRACK            "HeadTrack"
#define XR_INI_LAYERS               "Layers"
#define XR_LAYERS_PROJECTION        "projection"
#define XR_RECENTRE_KEY             VK_NUMPAD2

/* Constant-table bookkeeping. */
#define SHADER_SLOTS                16384   /* shaders remembered, open addressing */
#define SHADER_CONSTS_MAX           24      /* named constants kept per shader */
#define NAME_TABLE_MAX              512     /* distinct (name, register, count) logged once each */

/* Log file, written beside the game's exe; %TEMP% if that fails. */
#define LOG_FILE_NAME               "fc3bd_vr_log.txt"

#endif
