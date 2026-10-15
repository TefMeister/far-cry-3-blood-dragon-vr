# 2026-10-15 (dev PC, /pd, no launch): head tracking, built

**What it does.** When the headset output runs with `HeadTrack=1`, the game's camera turns with the head. The game
still decides where the camera is and which way the mouse points it; the head turns on top of that. With
`Layers=projection` each eye's picture goes to the headset as a proper eye view, tagged with the head pose it was
drawn with, so the headset only has to make up for the little the head moved since. Until now the two pictures sat
on a fixed floating screen.

**Why it was cheap here.** The game's camera uses the same axis layout as OpenXR (right, up, and backward as +z),
read from the camera numbers logged on 2026-10-08. So the head's turn goes in as it comes, with no mirroring.

**How sure.** Not run in the game `[compile-verified 2026-10-15]`. The maths is tested against cameras rebuilt with
the head's own axes, 45,506 checks `[verified-numerically 2026-10-15]`; a planted mistake fails 40,478 of them.
The other three tests still pass.

**What is NOT established.**
- Whether the turn goes the right way in the game. The direction follows from the 2026-10-08 trace; if turning the
  head left turns the view right, the convention reading is wrong, not a setting.
- The mirrored water-reflection camera is turned the same way as the real one, probably wrong for a mirror
  `[hypothesis]`: watch the water while turning the head.
- The picture keeps the game's own 46.7-degree lens, so in the headset it fills the middle of the view, with black
  around it. Filling the eye means changing the lens (c0/c4/c8/c16), a separate step.
- Frame rate: the readback already halves it; turning adds a little maths per camera block, not measurable here.

**Build.** `dev-archive/proxy-d3d9`, d3d9.dll `12d232b7658f`, installed on the dev PC with the ini set to
`HeadTrack=1`, `Layers=projection` (old dll and ini kept as `*.bak-2026-10-15-pd-pre-headtrack`).
⚠️ A rebuild of the unchanged older source no longer gives the installed `9f4fa456b8df` (it gives `276e9d755f70`):
the compiler on this PC changed since 2026-10-11, the same thing seen on Enslaved today.

**The test (simulator, dev PC).** Put `fc3bd_vr_openxr.txt` and `fc3bd_vr_stereo_on.txt` beside the exe, start, reach
gameplay, then in the simulator turn the head 30 degrees left, back, 20 up. The log should show
`xr: head tracking ON` and a non-zero `blocks turned by the head`. View turns left = right; turns right = the
convention is read wrong; does not turn but the count is non-zero = the camera is set again after our edit; count
zero = the head pose never arrived (look for `xrLocateViews` trouble). Then check the simulator shows the eyes
(`get_projection_log`: each eye's fov should be about 46.7 degrees tall).
