# First static look (2026-09-15)

Read from the installed Steam copy on the home PC (`C:\Steam\steamapps\common\Far Cry 3 Blood Dragon`), without launching the game.
Every claim below is `[inferred-static 2026-09-15]` unless tagged otherwise: it comes from reading file
headers and strings, not from running anything.

- **Install:** 3.5 GB.
- **Identity:** Far Cry 3: Blood Dragon, Steam build (app 233270). Two tiny launchers in `bin\` (linked 2013-06-04) each load a 27 MB game DLL (linked 2019-10-17): `fc3_blooddragon.exe` → `FC3.dll`, and `fc3_blooddragon_d3d11.exe` → `FC3_d3d11.dll`.
- **Engine:** **Dunia** — the DLL still describes itself as `Dunia Engine/Far Cry 2 Dynamic Link Library` `[inferred-static 2026-09-15]`. That is the engine of this account's **Far Cry 2** project, which already has stereo and head rotation working on Direct3D 9. Havok, Bink `[inferred-static 2026-09-15]`.
- **Binary:** **32-bit** (PE32). Game DLLs prefer `0x10000000` with ASLR on and relocations kept, so the address can move between runs `[inferred-static 2026-09-15]`.
- **Renderer:** **Two builds.** `FC3.dll` imports `d3d9.dll` + `d3dx9_43.dll`; `FC3_d3d11.dll` imports `d3d11.dll`, `dxgi.dll`, `d3dx11_43.dll` `[inferred-static 2026-09-15]`. The Direct3D 9 build is where the Far Cry 2 method can be tried first.
- **Protection:** ⚠️ **Ubisoft Connect.** Both DLLs import `uplay_r1_loader.dll` and `ubiorbitapi_r2_loader.dll`, and `UbisoftConnectInstaller.exe` ships in the folder `[inferred-static 2026-09-15]`. So the game very probably needs Ubisoft Connect installed and signed in to start `[hypothesis]` — the same kind of publisher-launcher gate that stopped Burnout Paradise. Not tested live.
- **Other:** Data under `data_win32\`, not yet looked at.

## Method

PE headers and import tables read with `pefile`: machine type, link timestamp, image base, ASLR
flag, section names, sizes and entropy. Then a case-insensitive search of each binary for renderer
DLL names (`d3d9`, `d3d11`, `d3d12`, `dxgi`, `vulkan-1`, `opengl32`), headset runtimes (`openvr`,
`openxr`, `oculus`), protection markers (`denuvo`, `securom`, `.bind`) and middleware names, with
readable strings pulled around the interesting hits. A string match shows a name is present in the
file, not that the code path is used. A `.text` section with entropy near 8.0 is encrypted or
compressed, not normal code.

## Risks noted

- ⚠️ **Publisher launcher gate (Ubisoft Connect).** Check it launches and signs in before planning work on it.
- Otherwise the cheapest start of this batch: same engine family as Far Cry 2, whose Direct3D 9 camera is already found.
