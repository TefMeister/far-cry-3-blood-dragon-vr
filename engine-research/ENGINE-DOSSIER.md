# Engine Dossier — Far Cry 3: Blood Dragon (Dunia Engine 2)

> One consolidated, living reference for this game's engine, filled in as the
> `PLAYBOOK.md` phases are worked. Chronological blow-by-blow belongs in the
> `dev-archive/` and `modding-notes/` folders; this file is the *distilled current
> truth*. Update it whenever a fact changes; correct false leads in place.

**Status:** M0, first static look (2026-09-15); the game has not been launched yet. · **VR-readiness verdict:** TBD.

## 1. Identity
- Game / build / version: Far Cry 3: Blood Dragon, Steam build (app 233270). Two tiny launchers in `bin\` (linked 2013-06-04) each load a 27 MB game DLL (linked 2019-10-17): `fc3_blooddragon.exe` → `FC3.dll`, and `fc3_blooddragon_d3d11.exe` → `FC3_d3d11.dll`.
- Platform & store; unofficial port? (extra fragility/legal notes): Steam (PC). Official release, not a fan port.
- Legitimacy: owned copy confirmed.

## 2. Engine lineage
- Family / base engine and how it was modified: **Dunia** — the DLL still describes itself as `Dunia Engine/Far Cry 2 Dynamic Link Library` `[inferred-static 2026-09-15]`. That is the engine of this account's **Far Cry 2** project, which already has stereo and head rotation working on Direct3D 9. Havok, Bink `[inferred-static 2026-09-15]`.
- Middleware (animation, audio, physics, megatexture, CUDA, etc.):
- Distinctive file formats / build tags / symbol naming: Data under `data_win32\`, not yet looked at.

## 3. Binary & memory
- 32/64-bit, size, module base, ASLR behaviour (stable base? relocations?): **32-bit** (PE32). Game DLLs prefer `0x10000000` with ASLR on and relocations kept, so the address can move between runs `[inferred-static 2026-09-15]`.
- Renderer API (D3D11/12, DXGI, GL, Vulkan) with evidence: **Two builds.** `FC3.dll` imports `d3d9.dll` + `d3dx9_43.dll`; `FC3_d3d11.dll` imports `d3d11.dll`, `dxgi.dll`, `d3dx11_43.dll` `[inferred-static 2026-09-15]`. The Direct3D 9 build is where the Far Cry 2 method can be tried first.
- Developer console / cvar system present? how opened?: not yet investigated.

## 4. DRM / anti-debug & injection foothold
- DRM (CEG/Denuvo/GOG/none); launch-time-debugger behaviour: ⚠️ **Ubisoft Connect.** Both DLLs import `uplay_r1_loader.dll` and `ubiorbitapi_r2_loader.dll`, and `UbisoftConnectInstaller.exe` ships in the folder `[inferred-static 2026-09-15]`. So the game very probably needs Ubisoft Connect installed and signed in to start `[hypothesis]` — the same kind of publisher-launcher gate that stopped Burnout Paradise. **Not a gate in practice (2026-10-08, dev PC):** launched from Steam, Ubisoft Connect signed in by itself and started the game, no dialog to answer `[verified-live 2026-10-08, n=3 launches]`. The game's online service is gone: an ERROR box ("service is not available") appears over the main menu and over the pause menu, Enter dismisses it, play is unaffected.
- **Window, renderer and music: all in `Documents\My Games\Far Cry 3 Blood Dragon\GamerProfile.xml`.** `<RenderProfile ResolutionX="1280" ResolutionY="720" Fullscreen="0" UseD3D11="0">` → client area exactly 1280×720 (window 1286×749 at 0,0), desktop left at 1920×1080, and the **Direct3D 9** exe starts `[verified-live 2026-10-08, n=1]` (the shipped profile had `UseD3D11="1"`, which is why the first launch ran the D3D11 exe). Music: `<SoundProfile MusicEnabled="0">`, the only music switch (no volume) `[inferred-static 2026-10-08]`; silence not checked by ear. ⚠️ **Steam Cloud put the old file back on the next launch** `[verified-live 2026-10-08, n=1]`; the edited file is now **read-only**, which kept the settings `[verified-live 2026-10-08, n=1]`. Backups beside it: `GamerProfile.xml.bak-2026-10-08` (as shipped) and `.bak-2026-10-08-cloud`. Command-line switches seen in FC3.dll (`-xres -yres -skipintro -offline`) untested `[inferred-static 2026-10-08]`.
- Attach workflow that works: not yet tested.
- Injection vector that works (proxy DLL name / injector / framework): not yet tested.

## 5. Threading & frame structure
- Immediate context only, or deferred contexts + command lists?:
- Which thread(s) do what; render-thread name(s):
- One-frame walkthrough (record → replay → present):

## 6. Camera & projection delivery (the crucial section)
- How the world transform reaches the GPU (shared VP buffer / per-draw MVP /
  other), with **shader-reflection / disassembly evidence**:
- Exact constant-buffer slot, parameter name(s), byte offset(s), layout,
  handedness, row/column convention:
- Where projection `P` / FOV comes from:
- **Compared with Far Cry 2 (2026-09-28, `/pd`):** both renderer DLLs (`FC3.dll` D3D9, `FC3_d3d11.dll` D3D11, 32-bit)
  carry Far Cry 2's shader-parameter names — `ViewMatrix`, `InvViewMatrix`, `ViewProjectionMatrix`, `ModelViewProj`,
  `CameraPositionFractions` and others — plus previous-frame matrices for motion blur `[inferred-static 2026-09-28]`.
  On Far Cry 2 the view sits at `c12–c15` (inverse `c36–c39`, projection `c16–c19`); here parameters are bound by name,
  so registers must be measured, not assumed. Shaders compile at run time from sources in `common.dat` (the
  `obj11` cache holds only an index, written 2026-09-15 18:53, so the D3D11 build has run once on the dev PC).
  Script-style camera-offset functions exist (`EnableCameraOffset`, `SetEffectiveCameraPositionOffset`, …)
  — a possible per-eye lever `[hypothesis]`. Detail: `dev-archive/recon/2026-09-28-camera-names-vs-far-cry-2/`.
- **Measuring the registers (2026-09-30):** `dev-archive/proxy-d3d9/` is a read-only `d3d9.dll` that logs
  every vertex shader's `name -> register` pairs from its constant table at creation, plus first-write
  snapshots. Reader tested 26/26 on `fxc`-compiled shaders `[verified-numerically 2026-09-30]`; the DLL
  exports exactly `Direct3DCreate9` `[compile-verified 2026-09-30]`. Not yet run.
- **A native camera-position offset (2026-09-30) — ⚠️ CORRECTED 2026-10-08.** The 2026-09-30 name-to-handler
  table was off by one (each handler is pushed with `lua_pushcclosure` `0x1007cade`, then named by the NEXT
  `lua_setfield` `0x1007cd9b`; checked by hand at `0x10bb849a..c4`: `0x10bb4f96` = SetPlayerLookAngles,
  `0x10bb75e8` = **Get**PlayerLookAngles) `[inferred-static 2026-10-08]`. So the `+0x98` write is
  **SetDesiredCameraOffset**, and the angle setter is **SetPlayerLookAnglesFromAngles** (`0x10bb7ab8`).
  **The chain from a global** (reader, `[inferred-static 2026-10-08]` except where marked): player manager
  `PM = [0x11843330]` → `P = [[PM+8]]` (none while `[PM+0xC] == 0`) → handle `H = [[P+4]+0xC]` (`[hypothesis]`: no
  RTTI) → entity `[H+0xC]` → component lookup `0x103dae45(ENT)` (thiscall, may return 0, game thread) → `S = [C+0x24]`;
  **effective** block `S+0x4e0`, **desired** `S+0x2d0`; in each, `+0x98` camera offset, **`+0xa4` look angles**,
  `+0xc4` position offset. The camera update (`0x10431bbb`) zeroes `+0x98`/`+0xc4` while a blend runs, so these are
  "kick then blend back" requests: a VR offset probably has to be rewritten every frame `[hypothesis]`. Walker with
  byte-signature checks: `staging/far-cry-3-blood-dragon-vr/camera-chain/fc3_camera_chain.h` `[compile-verified
  2026-10-08]`. Detail: `dev-archive/recon/2026-10-08-camera-controller-chain/README.md`.
- **⭐ MEASURED 2026-10-08 (`/lm`, two launches, camera trace): the view lands at `c12–c15`, as on Far Cry 2.**
  No shader carries a constant table (413 seen, 0 with one) `[measured 2026-10-08, n=2 launches]`, so the registers
  were read from values. One frame `[measured 2026-10-08, n=4 traces]`: `c12–c15` view (world → view), `c8–c11`
  projection (16:9, near 0.1, 46.7° for the world), `c16–c19` inverse projection, `c32–c35` inverse view, `c0–c3`
  view-projection without translation (camera-relative), `c4–c7` view-projection with translation. c12 takes several
  values per frame: the sun (shadow cascades, square lenses), the player's view, a pitch-mirrored copy (probably water
  reflection `[hypothesis]`), and in the helicopter a 39.4° camera (probably the gun `[hypothesis]`). **The player's
  c12 turns with the mouse: +50.34° for 300 counts, −100.71° for 600** `[measured 2026-10-08, n=2]`. ⚠️ The
  first-write snapshot catches the SUN camera in open-world frames, not the player's: use the camera trace
  (`fc3bd_vr_camtrace.txt`). Note: `modding-notes/2026-10-08-lm-the-view-lands-at-c12-like-far-cry-2.md`.
- The per-eye override maths (`K_eye = …`): a sideways eye shift d in view space is c12 row 0 .w −= d; c0 and c4
  row 0 .w −= P00·d (P00 = c8 row 0 x); c32 translation += d·right `[inferred-static 2026-10-08]`, applied only to
  the player's camera (16:9 projection, not the mirrored copy).
- **Built 2026-10-08 (`/pd`):** the shift above runs in `dev-archive/proxy-d3d9` (`abd6b4e78deb`, numpad 5, off by default); tested against a camera actually moved sideways, 16,962 checks `[verified-numerically 2026-10-08]`; Note `modding-notes/2026-10-08-pd-the-per-eye-shift.md`.
- **⭐ LIVE 2026-10-08 evening (`/lm`): the shift gives real depth.** Left only vs right only on foot: near ground 23 px apart, mid rocks 12, far trees and sky 0, symmetric about the unshifted picture, left eye sees near things further right (right sign) `[measured 2026-10-08, n=1]`. c0/c4/c12/c32 each shifted ~40-50 times a frame; the sun's shadow cameras refused (~2,200 per 5 s on foot) `[verified-live 2026-10-08, n=1]`; no tearing in an alternating frame (by eye, n=1). Note `modding-notes/2026-10-08-lm-the-per-eye-shift-gives-real-depth.md`.

## 7. Constant-buffer fill mechanism
- Map/DISCARD ring / UpdateSubresource / D3D11.1 offset / **persistent map +
  memcpy** (trap):
- Can source contents be read cheaply (captured CPU pointer) or need staging
  read-back?:
- The chosen override patch point and why:

## 8. Pass inventory (by render target)
- Main scene (res/formats):
- Shadow passes (depth-only sizes):
- Post / AA chain (SMAA/TAA/motion vectors; downscale sizes):
- UI / HUD (how it's kept separate):

## 9. cvar / console cheat sheet
| command / cvar | effect | use |
|---|---|---|
| | | |

## 10. Autonomous harness recipe (this game)
- Launch to a known scene (commands used): `steam://rungameid/233270`, then the menu route in `ai-game-control-profiles/profiles/far-cry-3-blood-dragon.json` (dismiss the service error, CAMPAIGN → CONTINUE, skip two story clips with Enter then Esc) → the helicopter gun scene `[verified-live 2026-10-08, n=2]`. Quit: pause → Down ×8 → QUIT TO DESKTOP → OK.
- In-process input / camera drive method that worked: menus obey SendInput scancodes (Menu-o-matiC `press`) `[verified-live 2026-10-08]`; in-game movement not tried yet.
- Frame-capture method; where images land:

## 11. Dead ends & false leads (save future time)
- none yet.

## 12. Open risks toward the North Star
- ⚠️ **Publisher launcher gate (Ubisoft Connect).** Check it launches and signs in before planning work on it.
- Otherwise the cheapest start of this batch: same engine family as Far Cry 2, whose Direct3D 9 camera is already found.
