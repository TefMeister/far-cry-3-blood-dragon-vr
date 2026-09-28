# 2026-09-28 — /pd: Blood Dragon speaks Far Cry 2's camera vocabulary, and compiles its shaders at run time

Dev PC, `/pd`, no game launched, nothing run. Game at `E:\SteamLibrary\steamapps\common\Far Cry 3 Blood Dragon`.

## Compared against Far Cry 2

Shader-parameter names found as text in `bin\FC3.dll` (D3D9 build) and `bin\FC3_d3d11.dll` (D3D11 build), both
32-bit, against Far Cry 2's `Dunia.dll` `[inferred-static 2026-09-28]`:

| | names |
| --- | --- |
| in both games | `ViewMatrix`, `InvViewMatrix`, `ViewProjectionMatrix`, `CullingViewProjectionMatrix`, `ModelViewProj`, `CameraNearPlaneSize`, `CameraPositionFractions`, `CameraPosition_DistanceScale` |
| Blood Dragon only | `WorldViewProjectionMatrix`, `PreviousViewMatrix`, `PreviousViewProjectionMatrix`, `InvViewRotProjectionMatrix` (motion blur and sky) |

The two Blood Dragon renderer DLLs carry the same list. The `Stereo` string is audio (it sits in a list of sound
formats), not rendering.

**What it means:** the Dunia renderer that Far Cry 2 used carried over with its parameter names intact. On Far Cry 2
the view matrix reached the GPU through `SetVertexShaderConstantF` at **c12–c15**, its inverse at c36–c39 and the
projection at c16–c19 (`far-cry-2-vr` dossier, `[verified-live]` there). The same logger is the natural first
instrument here. ⚠️ **The registers themselves are not guaranteed to match**: the engine binds these parameters by
name, so the shader compiler decides the register.

## Shaders are built on the player's PC

`data_win32\engine\shaders\obj11\` holds an index-only cache written on **2026-09-15 at 18:53–18:54** (every
`index.*` file is 550,980 bytes of index with no DXBC in it). So the D3D11 build **has been started once on the dev
PC**, and the game compiles its shaders at run time from sources packed in `common.dat` (zlib'd, no `CTAB`/`DXBC`
visible) `[inferred-static 2026-09-28]`. Reading the shader sources needs a Dunia `.fat/.dat` unpacker (public
community tools exist for Far Cry 3 `[reported]`, unchecked).

## A camera-offset API the game's scripts can call

Next to each other in `FC3.dll`: `EnableCameraOffset`, `DisableCameraOffset`, `IsCameraOffsetEnabled`,
`SetDesiredCameraOffset`, `SetEffectiveCameraOffset`, `SetEffectiveCameraPositionOffset`, `GetDesiredCamera`
`[inferred-static 2026-09-28]`. These read like script-exposed functions. If they are reachable, a per-eye camera
shift might not need a matrix edit at all `[hypothesis]`.

## NOT established

- Which build Tefa will play (D3D9 `fc3_blooddragon.exe` or D3D11 `fc3_blooddragon_d3d11.exe`).
- The registers or constant buffers in Blood Dragon. Only names were compared.
- Whether the camera-offset functions are callable from anywhere we can reach.
