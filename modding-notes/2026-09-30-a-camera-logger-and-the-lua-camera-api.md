# 2026-09-30 — a camera logger, and the Lua camera API

`/pd`, dev PC. **The game was not launched, and nothing here has been run in the game.**

## 1. The camera-register logger is built

`dev-archive/proxy-d3d9/`: a 32-bit `d3d9.dll` for the Direct3D 9 build. It changes nothing the game
sends; it logs:

- every vertex shader's `name -> register` pairs, read from the shader's own constant table when the
  game creates it (so "`ViewMatrix -> c12 x4`" comes straight from the game's shaders, not from guessing
  at values), and
- on snapshot frames, the **first** write to each register that frame, with the name and values (the
  Far Cry 2 trick; the HUD overwrites the low registers last).

Evidence: the constant-table reader passes **26/26** checks against real shaders compiled by `fxc`
(vs_3_0 and vs_2_0), including damaged and cut-short bytecode `[verified-numerically 2026-09-30]`. The
DLL builds reproducibly and exports exactly `Direct3DCreate9`, the one name `FC3.dll` imports from
`d3d9.dll` `[compile-verified 2026-09-30]`.

**Not installed**, on purpose: the first-launch rule wants one run as shipped first. It is ready to be
the "runs with our file" step.

## 2. The camera-offset functions are Lua bindings with a native path underneath

`EnableCameraOffset`, `SetEffectiveCameraPositionOffset` and friends are registered for the game's Lua
scripts, alongside `SetPlayerLookAngles` and `SetPlayerFOV` `[inferred-static 2026-09-30]`. Followed to
the end, `SetEffectiveCameraPositionOffset` finds the player's camera controller through a global
entity manager and copies an x/y/z vector to **controller `+0x98`**. The script path can be refused by a
condition; a direct write would not be `[inferred-static 2026-09-30]`. Detail:
`dev-archive/recon/2026-09-30-camera-offset-lua-api/README.md`.

## What is NOT established

- Anything about the running game: which build Tefa plays, whether Ubisoft Connect lets it start, what
  the log will show.
- Whether `+0x98` is world or camera space, whether it applies every frame, and which entity is the
  player.

## Next run (the board's `[FLAT]` rows, in order)

1. Start `bin\fc3_blooddragon.exe` as shipped; reach the menu; make it a 1280×720 window.
2. Copy `dev-archive/proxy-d3d9/build/d3d9.dll` (rebuild with `bash build.sh`) beside it; reach gameplay;
   read `bin\fc3bd_vr_log.txt`. See the proxy README for what each result means.
