# 2026-10-08 (`/lm`, dev PC, unattended): the view lands at c12–c15, like Far Cry 2

Two launches of the Direct3D 9 build with our read-only logger (`dev-archive/proxy-d3d9`), driven to gameplay by
scripted key presses, recorded (game window only): `logger-first-run_2026-10-08_14-14-08.mp4` and
`camtrace-run_2026-10-08_14-26-28.mp4` in `E:\OBS gameplay videos\far-cry-3-blood-dragon-vr\`. Logs:
`dev-archive/recon/2026-10-08-logger-first-run/`.

## What was found

1. **The shaders carry no constant tables.** 413 vertex shaders seen, 0 with a table `[measured 2026-10-08, n=2
   launches]`, so the logger's `ctab:` route gives nothing; this is the README's "tables stripped, read the values"
   case, as on Far Cry 2.
2. **The first-write snapshot catches the wrong camera.** In open-world frames the first block written at c12 is a
   sun camera (high above, looking 71° down, square lens), the shadow pass. Only in the helicopter scene did it look
   like the player's view. So the logger gained a **camera trace**: `fc3bd_vr_camtrace.txt` beside the exe asks for
   one frame in which every *different* block at c0/c4/c8/c12/c16/c32 is logged with the bound shader
   (build `1e6bf7ca26bc`).
3. **One frame, decoded** `[measured 2026-10-08, n=4 traces]`:

   | registers | what | evidence |
   | --- | --- | --- |
   | **c12–c15** | **view matrix** (world → view; rows right / up / back, translation in .w) | unit rows; turns with the mouse (below) |
   | c8–c11 | projection | 16:9, near 0.1; 46.7° vertical for the world, 39.4° for a second camera |
   | c16–c19 | inverse projection | 0.767 = 1/1.303, 0.432 = 1/2.317 |
   | c32–c35 | inverse view (camera → world; camera position in .w) | position matches c38/c40 |
   | c0–c3 | view-projection with **no translation** (camera-relative drawing) | c0 = c8 × rotation of c12, .w only the near term |
   | c4–c7 | view-projection **with** translation | same rows as c0 plus a translation that varies per draw group |

   In one on-foot frame c12 took **three** values: the sun (shadow cascades: square lenses of 129°, 166°, 177°), the
   **player's view**, and a copy of the player's view with the pitch mirrored (most likely a water-reflection
   camera `[hypothesis]`). In the helicopter frame it took two: the world view (46.7°) and a narrower 39.4° camera,
   probably the held gun `[hypothesis]`.
4. **c12 follows the mouse** `[measured 2026-10-08, n=2 turns]`: 300 counts right turned the player's view from
   13.27° to 63.61° (+50.34°); 600 counts left turned it to −37.10° (−100.71°). Linear, about 0.168° per count. The sun
   camera did not move.

## What it means

Far Cry 2's stereo (rewrite the view per eye at `SetVertexShaderConstantF`) ports, with two differences: the
camera-relative view-projection at c0 and the full one at c4 must move with the eye too, and the per-eye shift must
touch only the player's camera, not the sun or the reflection. A sideways eye shift of d in view space is one number per
matrix: c12 row 0 .w −= d; c0 and c4 row 0 .w −= P00·d (P00 = c8 row 0 x); c32 translation += d·right. Picking the
player's camera: its projection is 16:9 and it is not the mirrored copy.

## Not established

- Which shader passes besides the main scene use c0/c4 (particles, foliage, the gun).
- Whether the 39.4° camera is the gun and needs the eye shift as well.
- How the HUD is drawn (no HUD matrix was seen in these registers).

## Driving notes

- The menu tool's key is `esc`, not `escape`; the wrong name errors and does nothing.
- Esc does not open the pause menu during the scripted takedown tutorial; "hold Q to heal" blocks the camera until
  Q is held (4–7 s worked).
