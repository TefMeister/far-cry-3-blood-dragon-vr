# The camera-offset functions are Lua script bindings — 2026-09-30

Static read of `bin\FC3.dll` (Steam build, preferred base `0x10000000`; ASLR moves it, so use the
addresses below as offsets from the module base). **The game was not launched.**

## How the names are registered

One registration function (around `0x10bb8000`–`0x10bb8800`) walks a list of ~80 script functions, each
as: push the name string → call `0x1007cd9b` → push the C handler → call `0x1007cade`. That is the
shape of registering C functions for **Lua** `[inferred-static 2026-09-30]`. The full name → handler
list is `lua-name-to-handler-table.txt` (made by `camoff2.py`).

Camera-related entries include `SetEffectiveCameraPositionOffset`, `SetEffectiveCameraOffset`,
`SetDesiredCameraOffset`, `Enable/DisableCameraOffset`, `SetPlayerLookAngles`, `GetPlayerLookAngles`,
`SetPlayerFOV`, `SetPlayerNearFOV`, `SwitchCamera` `[inferred-static 2026-09-30]`.

## `SetEffectiveCameraPositionOffset`, followed to the end

| step | address | what it does |
| --- | --- | --- |
| Lua handler | `0x10bb7a15` | reads an entity ID (two DWORDs) and an x/y/z vector from the Lua stack |
| plain C worker | `0x10bb6fba` | `cdecl (id_lo, id_hi, x, y, z)`; builds the ID and calls on |
| entity method | `0x10bdf6cf` | looks the entity up through the global manager at `[0x11843740]` (`0x1021b980`), takes its component at `+0x0c`, asks it for the player part (`0x103dae45`), then the camera controller (`0x1041967a`: `[obj+0x24] + 0x2d0`) |
| camera setter | `0x1052817b` | `thiscall (vec3 *offset, bool force)`: copies the three floats to **controller `+0x98`**, always when `force` is set, otherwise only past a condition (a threshold at `[0x11506440]` against `+0xcc`, or `0x1040f666`) |

The script path passes `force = 0`, so it can be refused. Writing `+0x98` directly would not be
`[inferred-static 2026-09-30]`.

## What this is and is not

- **Is:** a named, single place where the game keeps an extra camera **position** offset, reachable
  from native code by following the manager global, with no Lua needed.
- **Is not yet known:** whether the offset is in world or camera-local space; whether it is applied
  every frame or only while "offset enabled"; which entity ID is the player; whether a per-frame write
  causes a visible shift. All `[hypothesis]` until a run.
- Why it matters: a per-eye sideways shift (for alternate-eye rendering) or head **position** (6DoF)
  without editing a matrix. `SetPlayerLookAngles` (`0x10bb75e8`) is the same kind of lead for head
  **rotation**.
