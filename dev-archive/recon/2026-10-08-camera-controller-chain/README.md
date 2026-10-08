# The player's camera/look state, reached from a fixed global (static read, 2026-10-08)

Supersedes: ENGINE-DOSSIER.md §6 "A native camera-position offset (2026-09-30)", and `dev-archive/recon/2026-09-30-camera-offset-lua-api/lua-name-to-handler-table.txt`

From: the /lm static reader, 2026-10-08. Files only; the game was not launched or attached.
All addresses are in `bin\FC3.dll` (D3D9 build) at its preferred base `0x10000000`; RVA = address − `0x10000000`.
Both globals below live in FC3.dll's `.data` section (`0x11730000`–`0x11f3dc54`), not in the exe `[measured 2026-10-08]`.

## 1. Correction first: the 2026-09-30 Lua name table is shifted by one

`0x1007cade` is `lua_pushcclosure(L, f, n)` (builds a C closure, stores `f` at `+0x10`, type tag 6) and
`0x1007cd9b` is `lua_setfield(L, idx, name)` (interns the string, calls settable with the top value, pops 8 bytes)
`[inferred-static 2026-10-08]`. Each entry is *push closure, then setfield(name)*, so **a name belongs to the
handler pushed BEFORE it**. The 2026-09-30 script paired each name with the handler AFTER it. Corrected
(full list: `staging/far-cry-3-blood-dragon-vr/camera-chain/lua-name-to-handler-table-CORRECTED.txt`)
`[inferred-static 2026-10-08]`:

| Lua name | handler | old (wrong) |
| --- | --- | --- |
| GetEffectiveCameraOffset | `0x10bb7506` | `0x10bb7577` |
| GetDesiredCameraOffset | `0x10bb7577` | `0x10bb78cf` |
| SetEffectiveCameraOffset | `0x10bb78cf` | `0x10bb7972` |
| SetEffectiveCameraPositionOffset | `0x10bb7972` | `0x10bb7a15` |
| SetDesiredCameraOffset | `0x10bb7a15` | `0x10bb4724` |
| SetPlayerLookAngles | `0x10bb4f96` (takes TWO entity IDs: a look-at) | `0x10bb75e8` |
| GetPlayerLookAngles | `0x10bb75e8` (returns 3 numbers) | `0x10bb7659` |
| GetDesiredPlayerLookAngles | `0x10bb7659` (returns 3 numbers) | `0x10bb7ab8` |
| SetPlayerLookAnglesFromAngles | `0x10bb7ab8` (ID + vec3) | `0x10bb501e` |

The shapes confirm the new pairing: every "Get" handler pushes 3 results, every "Set…FromAngles/Offset" reads a vec3.
**So the 2026-09-30 chain (`0x10bb7a15` → `0x10bdf6cf` → setter writes `+0x98`) is really `SetDesiredCameraOffset`**,
and the struct it writes is the "desired" one. The real `SetEffectiveCameraPositionOffset` writes a different
field, unconditionally (§3).

## 2. The chain from a fixed global

| hop | value | proof (instruction) |
| --- | --- | --- |
| 1 | `PM = [0x11843330]` player manager; heap singleton of 0x48 bytes, stored once | `0x1020d88f mov [0x11843330], eax` (after `new 0x48` + ctor `0x1021e62a`) |
| 2 | if `[PM+0x0C] == 0` no player; else `P = [[PM+0x08]]` (element 0) | `0x10bf21dc..0x10bf21ea` (the `GetLocalPlayerId` worker `0x10bf21d8`) |
| 3 | handle `H = [[P+4]+0x0C]` (virtual `+0x10` of P) | call `0x10bf21f7 call [eax+0x10]`; candidate body `0x102a3bad: mov eax,[ecx+4]; mov ecx,[eax+0xc]` |
| 4 | `H` = `{u64 id @+0; refcount @+8; Entity* @+0x0C}`; local player ID = `[H]`,`[H+4]` | `0x10bf21fa..0x10bf2209`; refcount release `0x1006cf8c` (`lock xadd [ecx+8]`) |
| 4b | (alt.) by ID: `0x1021b980` thiscall on `[0x11843740]` (entity manager, 0x84 bytes, stored once at `0x1020da61`) returns the same kind of handle | `0x10bdf311..0x10bdf324` |
| 5 | `ENT = [H+0x0C]` | `0x10bdf321 mov esi,[ebp-4]; mov ecx,[esi+0xc]` |
| 6 | `C = 0x103dae45(ENT)` thiscall, component lookup by type key `0x11861efc`; may return 0 | `0x10bdf335`, body `0x103dae45` → `0x1029787b` |
| 7 | `S = [C+0x24]`; **effective** block `S+0x4e0`, **desired** block `S+0x2d0` | `0x1041968c`/`0x10419695` (`+0x4e0`), `0x1041967a` (`+0x2d0`) |

Tags: hops 1, 2, 4, 4b, 5, 6, 7 `[inferred-static 2026-10-08]`. Hop 3 is the weakest: the vtable of the
player object is not known statically; `0x102a3bad` is slot `+0x10` of vtable `0x11522318`, whose slot `+0x0c`
(`0x102a47e1`, add to list at `[[this+4]+0x1c0]`) and the direct call `0x102a3d2b` (remove from the same list)
are exactly what other code calls on `P` (`0x1080d64b`, `0x1080d671`) — consistent, not proven `[hypothesis]`.
A run should read `[P]` and compare with `FC3+0x1522318`.
Hop 6 must be called (a hash lookup, not a fixed offset); call it on the game thread `[inferred-static 2026-10-08]`.

## 3. Fields in each block (effective = `S+0x4e0`, desired = `S+0x2d0`)

| field | meaning | proof |
| --- | --- | --- |
| `+0x00` | back-pointer to the component `C` | `0x10528181 mov ecx,[esi]` then `0x104196bf` (`[C+0x24]+0x180`) |
| `+0x98` vec3 | "camera offset" (Get/SetEffectiveCameraOffset on eff, SetDesiredCameraOffset on des) | read `0x10bdd3c5`; write via `0x1052817b` (`0x105281b3`) |
| `+0xa4` vec3 | **look angles**: Get reads eff, GetDesired reads des, SetFromAngles writes both | `0x10bdc676`, `0x10bdc6fb`; writes `0x10bdf36b`/`0x10bdf37d` → `0x103df493` |
| `+0xc4` vec3 | **position offset** (SetEffectiveCameraPositionOffset), written with no condition | `0x10bdd4c4`, `0x10bdd4d1`, `0x10bdd4de` |

So, as absolute offsets from `S`: effective look angles `S+0x584`, effective camera offset `S+0x578`,
effective position offset `S+0x5a4`; desired look angles `S+0x374`, desired offset `S+0x368` `[inferred-static 2026-10-08]`.
The names "effective/desired" come from the Lua names only `[inferred-static 2026-10-08]`. Angle units
(radians or degrees) and component order are unknown `[hypothesis]`.

The setter `0x1052817b(vec3*, force)` writes `+0x98` when `force` is set, or when the blend time
`[S+0x180+0xcc]` is ≤ `1e-6` (`[0x11506440]`), or when the new value differs from zero (`0x1040f666` is
vec3 ≠ vec3, `[0x11732e94]` = 0,0,0) — i.e. it only refuses writing **zero** during a blend `[inferred-static 2026-10-08]`.

## 4. Who consumes the offsets (why a one-off write may not stick)

`0x10431bbb` (called from the camera update at `0x1043228f`, which passes a time step from `[ebx+8]`) reads
eff `+0x98` and eff `+0xc4` into the camera object (`+0xf8`, `+0x110`), then on its blend path clears the
request: `0x10431d82` sets eff `+0x98` to zero and `0x10431d9f..0x10431daf` set eff `+0xc4` to zero. The camera's
copy then decays toward zero over the blend time (`0x10431e61..`) `[inferred-static 2026-10-08]`. Plain reading:
**the offsets behave like one-shot "kick and blend back" requests, not a held value**; a VR per-eye or head-position
offset will likely need rewriting every frame, or a hook on the camera object's own `+0xf8/+0x110` fields
instead `[hypothesis]`. Event handlers at `0x103e43c1` and `0x103ef3f8` also zero both `+0xc4` fields
(attach/mount-type events) `[inferred-static 2026-10-08]`.

The look angles `+0xa4` are read in ~33 places right after the effective getter and written through `0x103df493`
from ~109 call sites `[measured 2026-10-08]` (counted by `staging/.../tools/use.py` and `calls.py`); which of those
is the per-frame mouse/pad path is not yet known.

## 5. Code ready for the next run

`staging/far-cry-3-blood-dragon-vr/camera-chain/fc3_camera_chain.h`: walks hops 1–7 with byte-signature checks
on every function it relies on (`verify()`), returns pointers to the effective/desired look angles and offsets.
Compiles as 32-bit C++17 with `i686-w64-mingw32-g++ -Wall -Wextra`, no warnings `[compile-verified 2026-10-08]`;
signatures and the embedded global address match the FC3.dll on disk (`tools/sigcheck.py`) `[verified-numerically 2026-10-08]`.
Not run in the game.

## 6. Still uncertain — what a run should check

1. `[P]` == `FC3+0x1522318` (proves hop 3), and `[H]` equals what Lua `GetLocalPlayerId` returns.
2. Read eff `+0xa4` while turning the mouse: does it change, which component is yaw, radians or degrees.
3. Write eff `+0xa4` once per frame: does the view turn (head rotation lever) or get overwritten.
4. Write eff `+0xc4` once per frame: does the camera shift, and in world or camera space.
