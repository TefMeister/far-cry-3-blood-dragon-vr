# proxy-d3d9 — the Blood Dragon camera-register logger

A 32-bit `d3d9.dll` for the Direct3D 9 build of Far Cry 3: Blood Dragon (`bin\fc3_blooddragon.exe`,
which loads `FC3.dll`). **It changes nothing the game sends.** It only writes a log, so the first run
with it tells us where the camera goes.

Written 2026-09-30 by `/pd`. **Built and tested on paper; it has not been run in the game.**

## Why

Far Cry 2 (same Dunia engine) got stereo by rewriting the view matrix per eye inside
`SetVertexShaderConstantF`. There the view sat at `c12–c15`. Blood Dragon binds shader parameters **by
name** (`ViewMatrix`, `InvViewMatrix`, `ViewProjectionMatrix`, `ModelViewProj`, …), so the register
can differ per shader and has to be measured.

## What it logs (`bin\fc3bd_vr_log.txt`, or `%TEMP%` if that is not writable)

1. **Every distinct `name -> register` pair, once**, read from each vertex shader's own constant table
   when the game creates it: `ctab: ViewMatrix -> c12 x4`. This alone answers "where does the view
   land", for every shader, without guessing from values.
2. **Snapshot frames**: the *first* write to each register in that frame, with the current shader's name
   for it and up to four rows of values. First-write, not last-write, because the HUD draws last and
   overwrites the low registers (the Far Cry 2 trap). A snapshot runs at frame 600, then every 1,800
   frames, up to 20. Creating `bin\fc3bd_vr_snap.txt` asks for one on the next frame (the file is then
   deleted).

All numbers are in `src/settings.h`.

## Build

```
bash build.sh
```

Builds `build/d3d9.dll` (reproducible: `-Wl,--no-insert-timestamp`) and runs `tools/ctab_test.c`
against two shaders compiled by `fxc` from `tools/ctab_fixture.hlsl` (vs_3_0 and vs_2_0): **26/26**
checks, including cut-short and damaged bytecode. The DLL exports exactly `Direct3DCreate9`, the one
name `FC3.dll` imports from `d3d9.dll`.

## Install (only after the game has run once as shipped)

Copy `build/d3d9.dll` into the game's `bin\` folder, beside `fc3_blooddragon.exe`. Nothing is replaced
(the game ships no `d3d9.dll`). Remove it to undo.

## Reading the result

- `ctab:` lines show `ViewMatrix` (and friends) at one register everywhere → port Far Cry 2's per-eye
  rewrite at that register.
- `ViewMatrix` at several registers → the rewrite must look the register up per shader (the table is
  already kept per shader in `proxy.c`).
- No `ctab:` lines at all, but shaders created → the tables were stripped; fall back to reading values
  in the snapshots, as on Far Cry 2.
- No `CreateDevice` line → the game did not use this file (the D3D11 build was started, or it never
  got past the launcher).

## Credits

MinHook by Tsuda Kageyu (BSD-2, `third_party/minhook`). The constant-table approach follows this
account's Alan Wake proxy; the first-write trick is Far Cry 2's.

## Headset output (OpenXR) - this folder's addition, 2026-10-11

This copy (staging `proxy-d3d9-xr`) is the dev-archive proxy plus `src/xr_capture.c` and `src/xr_session.c`, ported
from Alan Wake's `aw_xr.c`. At each Present the finished back buffer is read back and stored as that eye's picture
(the eye from `stereo_frame_eye()`; a frame with no camera shift, such as a menu, goes to both eyes). A headset
thread shows each eye its own picture on a flat screen 2 m ahead (two quad layers, left-only and right-only),
fixed where the head was when the session began. No head tracking. The readback waits for the GPU, so frame rate
drops while it is on.

**Off unless** an empty `fc3bd_vr_openxr.txt` is beside `fc3_blooddragon.exe` (checked once a second). Also needed
there: the 32-bit `openxr_loader.dll` (OpenXR.Loader NuGet 1.0.10.2, `native/Win32/release/bin/`, SHA-256
`fb1e06de9653...`; a copy is in `staging/hard-reset-vr/proxy-d3d9/build/`). To choose the runtime for this game
only, `fc3bd_vr.ini`:

```
[vr]
RuntimeJson=D:\claude video game stuff\tools\OpenXR-Simulator\openxr_simulator-32.json
TestPattern=0
```

`TestPattern=1` shows left red / right blue instead of the game. Log lines start with `xr:`.

No-game test: copy `openxr_loader.dll` into `build/`, then `build/xr_selftest.exe <32-bit runtime json> [secs]
[pattern]`. It opens the runtime's window, so not while a game is being driven on the same PC.

## Head tracking and projection views - 2026-10-15 (`/pd`, not run yet)

`src/headrot.c` turns the player's camera blocks by the headset's head rotation, on top of the camera the game chose:
view c12 `V' = R^T V`, inverse view c32 `M' = M R`, and the view-projections c0 and c4 by taking the lens out of
the block, turning the view inside it and putting the lens back. This game's view is right-handed with -z forward,
the same as OpenXR's, so the headset's orientation is used as it comes. The per-eye shift runs after the turn, so
each eye sits on the head's right axis. `src/xr_views.c` shares the pose between the headset thread and the game:
each picture is filed with the pose and the game's own lens it was drawn with, and with `Layers=projection` the eyes
are shown as projection views at that pose (the runtime corrects only for head movement since), instead of the
floating screen. The game's lens is 46.7 degrees tall, so the picture fills the middle of the headset's view, not
all of it; filling the eye is a later step.

Settings, `[vr]` in `fc3bd_vr.ini`: `HeadTrack=1`, `Layers=projection` (both off when missing). Head tracking only
acts while the per-eye shift is on (numpad 5 or `fc3bd_vr_stereo_on.txt`). Numpad 2 re-centres: the way the headset
faces becomes the game's forward. The log's stereo line counts `blocks turned by the head`.

Tests (`build.sh`): `tools/headrot_test.c`, 45,506 checks against cameras rebuilt with the head's axes, the
quaternion against a direct rotation, re-centring, the lens edges, turn-then-shift; with `wrong` (R used for R^T)
40,478 fail, and the build stops if that planted mistake ever passes.
