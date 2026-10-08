# 2026-10-08 (`/pd`, dev PC): both eyes side by side in the window — built, not run

**The game was not launched, and nothing here has been run in the game.**

`dev-archive/proxy-d3d9` (`5def297`), `d3d9.dll` `66a06df4a62d`, installed in `bin\` `[compile-verified 2026-10-08]`.
Ported from Hard Reset's d3d9 proxy (`ensure_sbs` / `capture` / `compose` / `drop_sbs`), changed for one eye per frame.

- **The frame's eye** (`stereo_frame_eye()`): read at Present BEFORE the eye flips; 0 when no player camera was
  shifted in the frame (menus, loading), and such frames are left untouched.
- **Side by side** (`sbs.c`, switch `fc3bd_vr_sbs.txt` beside the exe, checked once a second): the finished back buffer
  is copied into its eye's half of a double-width surface; once both halves hold a picture, the whole surface is
  stretched back over the back buffer, so the window shows left | right, each squeezed to half width. The back buffer
  is described once in the log; a Reset hook releases the surface first.
- **Parity test** (`tools/parity_test.c`, runs the shipped `stereo.c`): over 200 frames with menu frames mixed in, the
  label always equals the eye the camera was shifted for, camera frames alternate, menu frames read 0 — 575 checks
  pass; a copy with the label flipped fails 188 of them `[verified-numerically 2026-10-08]`.
- The file imports no `d3d9.dll` of its own (checked), so it cannot load itself.

## The live check (FLAT, still camera)

Numpad 5 (eyes on), then add `fc3bd_vr_sbs.txt` beside the exe. The log: `sbs: back buffer ...`, `sbs: side-by-side
surface ...`, and every 5 s `captured left N, right N; composed ~2N`.

| seen | means |
| --- | --- |
| two squeezed pictures, near things further apart than far ones, left picture's near things further right | working: next, the headset simulator (Hard Reset's OpenXR files) |
| the two halves flicker or swap each frame | the eye label and the shift disagree in the game (the test says they cannot; look at the Present order) |
| near things further LEFT in the left picture | eyes crossed: the label sign, not the separation |
| `failures` climbing with an error code | StretchRect refused (multisampled back buffer or format); the description line says which |
| black or frozen halves | the surface was lost (a Reset with no log line?) |

## Not established

- Anything live. Half-rate per eye (each eye updates every other frame) is inherent to alternating eyes; whether it
  reads as a judder on the dev PC is for the flat run.
