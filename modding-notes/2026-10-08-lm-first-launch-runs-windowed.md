# 2026-10-08 (dev PC, `/lm`, three launches, driven by Claude): it runs, and it runs in a window

*Recorded: `first-launch_2026-10-08_12-37-05.mp4` (as shipped, D3D11) and
`windowed-d3d9_2026-10-08_12-47-51.mp4`. Pictures: `dev-archive/recon/2026-10-08-first-launch-windowed/`.*

## In plain words

The game starts from Steam and plays. Ubisoft Connect signs in by itself, so the publisher's
launcher is not a wall. It now runs in a 1280x720 window, in the Direct3D 9 version our tools are
built for, with music switched off in its settings file. Nothing of ours is installed yet.

## What happened

1. **As shipped:** Steam ran Ubisoft Connect, which started `fc3_blooddragon_d3d11.exe` full screen
   at 1920x1080. Main menu, NEW GAME (no saves existed) into SAVE 1, MEDIUM, two story clips skipped,
   gameplay in the helicopter gun scene `[verified-live 2026-10-08, n=1]`. Quit through the pause menu.
2. **Settings edited, game closed:** `GamerProfile.xml` in `Documents\My Games\Far Cry 3 Blood
   Dragon\`: `Fullscreen="0"`, `ResolutionX/Y="1280/720"`, `UseD3D11="0"`, `MusicEnabled="0"`.
   On the next launch the file was back to the old values within seconds (Steam Cloud, most likely),
   and the game came up full screen again `[verified-live 2026-10-08, n=1]`. Quit from the main menu.
3. **Edited again and made read-only:** the Direct3D 9 exe started; client area 1280x720, window
   1286x749, desktop still 1920x1080 (`windowcheck` verdict: windowed) `[verified-live 2026-10-08, n=1]`.
   CONTINUE from the autosave, clips skipped, gameplay. Left running, paused, for Tefa to look at.

The online service is gone: an ERROR box over the main menu and over the pause menu (after a
~20 s "connecting" alert). Enter dismisses it.

## Not established

- That `MusicEnabled="0"` silences all music (nobody listened).
- What read-only does long-term: the game cannot save option changes made in its menus.
- Whether the window stays put through a level load or an alt-tab.
- Our logger (`dev-archive/proxy-d3d9/`) has not been installed; that is the next row.
