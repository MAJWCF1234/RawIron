---
tags:
  - rawiron
  - launcher
  - shell
---

# Visual Shell

`RawIron.VisualShell` is the temporary native front door for the current workspace.

It exists to make the engine feel launchable and self-hosted even before the full editor is ready.

## Purpose

- provide a real launchable `.exe`
- give RawIron a keyboard-first control surface
- surface previews, diagnostics, and tests without bouncing through ad hoc commands
- keep the experience visually closer to an old computer shell than a modern terminal

## Current Style

The shell is intentionally inspired by late-80s and early-90s home-computer/workstation shells:

- Sharp X68000 / X68000 Human-style color framing
- Amiga / Commodore-like fixed-width interface feel
- keyboard-only navigation
- simple panel-based layout

## Current Controls

- `Up` / `Down`: move selection
- `Enter`: run the selected action
- `F5`: run the selected action
- `Esc`: close the shell

## Current Actions

- open the native preview window
- save a shaded cube snapshot
- run the ten Scene Kit milestone checks
- run Vulkan diagnostics
- list the tracked Scene Kit parity targets
- dump the sample scene report
- run core tests
- run engine-import tests
- run the full `ctest` sweep
- open the documentation vault folder
- open the previews folder

## Build Paths

MSVC build:

- `build/dev-msvc/Apps/RawIron.VisualShell/RawIron.VisualShell.exe`

Clang build:

- `build/dev-clang/Apps/RawIron.VisualShell/RawIron.VisualShell.exe`

Repository-root launchers:

- `RawIron Visual Shell.lnk`
- `Launch RawIron Visual Shell.cmd`

## Notes

This is a temporary launch shell, not the final editor UI.

Its job is to give RawIron a believable native front end while the larger editor, player, and rendering paths keep maturing.

The shell preview panel and the preview window now use the Scene Kit cube scene instead of the earlier hardcoded-only cube bitmap path.

The shell now follows the same app-package ownership rule as the other native apps: app-specific code lives under
`Apps/RawIron.VisualShell/`, while shared engine systems stay in `Source/`.
