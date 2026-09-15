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
- DRM (CEG/Denuvo/GOG/none); launch-time-debugger behaviour: ⚠️ **Ubisoft Connect.** Both DLLs import `uplay_r1_loader.dll` and `ubiorbitapi_r2_loader.dll`, and `UbisoftConnectInstaller.exe` ships in the folder `[inferred-static 2026-09-15]`. So the game very probably needs Ubisoft Connect installed and signed in to start `[hypothesis]` — the same kind of publisher-launcher gate that stopped Burnout Paradise. Not tested live.
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
- The per-eye override maths (`K_eye = …`):

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
- Launch to a known scene (commands used):
- In-process input / camera drive method that worked:
- Frame-capture method; where images land:

## 11. Dead ends & false leads (save future time)
- none yet.

## 12. Open risks toward the North Star
- ⚠️ **Publisher launcher gate (Ubisoft Connect).** Check it launches and signs in before planning work on it.
- Otherwise the cheapest start of this batch: same engine family as Far Cry 2, whose Direct3D 9 camera is already found.
