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
