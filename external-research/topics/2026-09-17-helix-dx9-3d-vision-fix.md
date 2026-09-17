# HeliX's DX9-only 3D Vision fix for Blood Dragon: a reference for the camera-register comparison

**Status:** 🆕 new · **Priority:** medium.

## What is public

- **Helix Mod: "FarCry3: Blood Dragon (DX9 only) - 3D Vision fix"** fixes lights, shadows, water, some
  screen effects and the HUD, with a HUD-depth hotkey (Y), depth presets (U) and a hold-to-aim low
  separation `[reported]`. It is launched through `HeliXmodLauncher.exe`; non-Steam installs put the
  files in the game folder rather than `Bin` `[reported]`.
- No dedicated Blood Dragon VR mod was found; vorpX forum threads report problems `[reported]`.
- fholger's open-source **Far Cry** (2004, CryEngine 1) VR mod is a different engine, useful only as
  a general example `[reported]`.

## Why it matters here

The board's `[PD]` row compares `FC3.dll`'s D3D9 vertex-shader constant uploads against Far Cry 2. A
HeliX DX9 fix edits specific shaders and constants for this exact renderer, so its `DX9Settings.ini`
and shader overrides are a second, independent pointer to where Dunia 2 keeps the view and projection
`[hypothesis]`.

## Next step

Read the fix's settings file and override list alongside the Far Cry 2 dossier's register notes.

## Sources

- Helix Mod — <https://helixmod.blogspot.com/2013/05/farcry3-blood-dragon-dx9-only-3d-vision.html>
- vorpX forum, "Far Cry : Blood Dragon problems" — <https://www.vorpx.com/forums/topic/far-cry-blood-dragon-problems/>
- fholger, Far Cry VR — <https://github.com/fholger/farcry_vrmod>
