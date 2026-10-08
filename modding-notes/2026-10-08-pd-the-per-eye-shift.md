# 2026-10-08 (`/pd`, dev PC): the per-eye camera shift, Far Cry 2's way — built, not run

**The game was not launched, and nothing here has been run in the game.**

`dev-archive/proxy-d3d9` (`da36771`), `d3d9.dll` `abd6b4e78deb`, installed in `bin\` `[compile-verified 2026-10-08]`.
Off unless `fc3bd_vr_stereo_on.txt` is beside the exe or numpad 5 is pressed. Numpad 4/6 change the eye separation
(default 0.065, about a real eye distance if Dunia units are metres, as on Far Cry 2), numpad 8 cycles
alternate / left only / right only.

## How it works

At `SetVertexShaderConstantF`, after the loggers (so snapshots still show what the game sent), the upload is copied and
the camera blocks are shifted by d = ±separation/2 along the camera's right axis, alternating each Present:

| block | change |
| --- | --- |
| c0 (VP, camera-relative) and c4 (VP) | row 0 .w −= P00·d, P00 = \|row 0 xyz\| |
| c12 (view) | row 0 .w −= d |
| c32 (inverse view) | translation += d·right |

Only the player's cameras: a c0/c4 block must be a perspective view-projection with picture aspect 1.2–2.5 (16:9 =
1.78); the sun's square shadow cameras and orthographic blocks are refused, and c12/c32 follow the last c0/c4 verdict
and must be rigid. Camera positions at c38/c40 are left alone: shifting them as well would move the eye twice if the
shaders subtract them before c0 `[hypothesis]`.

## Tested without the game

`tools/stereo_test.c` builds the blocks as measured (c8 layout, z-up, rows right / up / back), shifts them with the
shipped functions and compares, for random points, with the blocks of a camera actually rebuilt at camera + d·right,
c0 used on (point − ORIGINAL camera). 200 cameras × 20 points, plus a control that the unshifted block does not match:
**16,962 checks, 0 failed** `[verified-numerically 2026-10-08]`. The measured square sun lens and an orthographic block
are refused.

## The live check (FLAT, still camera)

Load the save, stand still, numpad 5. In `fc3bd_vr_log.txt` every 5 s: `stereo: ... shifted c0 N, c4 N, c12 N, c32 N;
other cameras left alone M`. On screen (alternate mode) near things should shimmer sideways more than far ones;
numpad 8 to "left only" / "right only" and compare two screenshots.

| seen | means |
| --- | --- |
| shimmer, near more than far; shadows steady | the shift reaches the player's camera only: next, two eye pictures (side by side or the headset bridge) |
| nothing moves, counts 0 | the classifier refuses the player's camera: read the camtrace for the actual aspect |
| the world moves but lighting or reflections tear | c38/c40 or another block also needs the eye: trace which |
| shadows swim | a shadow camera passed the aspect test: tighten it |
| the picture breaks | a block was mis-identified: the derivation, not the separation |

## Not established

- Anything live. Whether the 39.4° camera (probably the gun) and the mirrored reflection camera look right shifted.
