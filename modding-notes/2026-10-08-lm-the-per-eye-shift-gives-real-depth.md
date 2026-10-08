# 2026-10-08 evening (dev PC, `/lm`, one launch, driven by Claude): the per-eye shift gives real depth

*Recorded: `per-eye-shift_2026-10-08_18-37-20.mp4`. Pictures and log lines:
`dev-archive/recon/2026-10-08-per-eye-shift-live/`.*

## In plain words

The left/right eye shift built this morning works in the game. Near things move between the eyes, far
things do not, each eye sits evenly either side of the normal picture, and the direction is right. The
sun's shadow cameras are left alone. Next is getting the two pictures out: side by side in the window,
then the headset simulator.

## Measured (on foot, standing still, first rocks after the helicopter; `d3d9.dll` `abd6b4e78deb`)

- Numpad 5: `stereo: ON`; every 5 s about 11,000-15,000 shifts each of c0, c4, c12 and c32 (about 40-50
  a frame) `[verified-live 2026-10-08, n=1]`.
- Other cameras left alone: 0 in the helicopter scene, 2,160-2,392 per 5 s on foot (the sun's shadow
  cameras, refused as meant) `[verified-live 2026-10-08, n=1]`.
- Left only vs right only, horizontal shift by block matching `[measured 2026-10-08, n=1]`:

| region | left to right | left vs off | right vs off |
| --- | --- | --- | --- |
| near ground | 23 px | 12 px | 11 px |
| mid rocks | 12 px | 6 px | 6 px |
| far trees | 1 px | 0 | 0 |
| sky | 0 | 0 | 0 |

  Near more than far, symmetric about the unshifted picture. The left eye sees near things further right,
  which is the right sign. The alternating picture shows no tearing or broken lighting (one frame, by eye).
- Numpad 8 cycles alternate / left only / right only; numpad 5 off. All logged.

## Small things

- A story clip interrupted the first attempt (the helicopter scene ends in one); the test was redone on foot.
- The pause menu's "service not available" box can take ~25 s to appear; keys are ignored until it is
  dismissed.

## Not established

- Shadows "steady" is judged from one alternating frame and the difference picture, not a moving comparison.
- The gun (a 39.4° camera, dossier) and reflections under the shift: not looked at closely.
- No two-eye picture exists yet; nothing in a headset.

## Next (reader's plan, static)

Side by side first: port Hard Reset's `ensure_sbs` / `capture` / `compose` / `drop_sbs` into a new `sbs.c`, latch
the finished frame's eye in Present before the flip (Far Cry 2's `stereo_frame_eye()` contract and its parity
test), add a Reset hook that drops the surface, log the back-buffer description once; switch file
`fc3bd_vr_sbs.txt`. Then Hard Reset's OpenXR files (`hxr*.c`, `d3d9_readback.c`) unchanged, plus a small logging
shim, for the simulator. Do not reuse Far Cry 2's OpenVR `vr_bridge.c` (never run, slow).
