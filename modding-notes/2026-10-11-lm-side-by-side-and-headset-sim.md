# 2026-10-11 (dev PC, `/lm`, three launches, one reader): side by side works, both eyes reach the headset simulator

Evidence: `dev-archive/recon/2026-10-11-side-by-side-and-headset-sim/`. Videos (game window only):
`E:\OBS gameplay videos\far-cry-3-blood-dragon-vr\` (`side-by-side`, `headset-sim`), copied to the MEGA transfer folder.

## Results

1. **Side by side** (`66a06df4a62d`): numpad 5, then `fc3bd_vr_sbs.txt`. Log: `captured left 147, right 147; composed 293;
   failures 0`, back buffer 1280x720 format 21, no multisampling. Measured between the halves (right minus left): rocks at the
   feet -12 px, middle rocks -4, far trees 0; in the helicopter the gun -30 to -37, the far palm 0 `[measured 2026-10-11, n=2
   frames each]`. That is the outcome table's "working" row: near things further right in the left picture.
2. **Numpad 8 modes:** left only gives `captured left 300, right 0`; the cycle is alternate, left only, right only.
3. **Headset output** (built by the reader, `9f4fa456b8df`): the test pattern showed left red, right blue; then the game in
   both eyes, `captured left 124, right 123` per 5 s, `failures 0`; near gun further right in the left eye
   `[verified-live 2026-10-11, n=1]`. The floating screen sits above eye level in the simulator.

## The reader (one background helper)

Ported the headset output from Alan Wake's `aw_xr.c` (one eye per frame, like Blood Dragon) rather than Hard Reset's
(both eyes in one frame); built it in staging; I installed and ran it, then moved it into `dev-archive/proxy-d3d9`
(rebuild gives the same hash, all old tests pass) and removed the staging copy.

## Installed now

`bin\d3d9.dll` `9f4fa456b8df`, `openxr_loader.dll` (32-bit, `fb1e06de9653`), `fc3bd_vr.ini` (simulator runtime,
`TestPattern=0`). The switch `fc3bd_vr_openxr.txt` was taken back out, so a normal launch does not open the simulator;
`fc3bd_vr_sbs.txt` also out. The side-by-side-only build is kept in `E:\claude video game stuff\builds\`.

## Automation (five capabilities)

Self-launch ✅ (`steam://rungameid/233270`, window after ~90 s); menu → gameplay ✅ by hand-sent keys (no route file yet);
commands N/A; character + camera not needed (still view); self-close ✅ three times (main menu and pause menu routes).
⚠️ A tutorial box ("Jump") can swallow the first Escape; one Escape too many during the story clips opens the pause menu.

## NOT established

- The cost of the per-frame readback on this PC; anything in a real headset; head tracking (not built for this game).
