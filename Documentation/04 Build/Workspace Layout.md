---
tags:
  - rawiron
  - workspace
  - build
---

# Workspace Layout

## Root

RawIron treats the repository root as the active workspace root, wherever the depot is checked out.

## Top-Level Folders

- `Apps`
  Native executable hosts such as `RawIron.Player` and `RawIron.Editor`
- `Assets/Source`
  Imported source assets before cooking
- `Assets/Cooked`
  Runtime-ready cooked assets
- `Config`
  Workspace-level config files
- `Documentation`
  Obsidian vault and design notes
- `Games`
  Built-in game modules and standalone game executables
- `Projects`
  Local engine projects and sandboxes created by tooling when needed
- `Saved`
  Workspace-level logs, caches, and generated state
- `Scripts`
  Helper scripts and automation
- `Source`
  Shared engine source code
- `ThirdParty`
  External libraries and mirrored dependencies
- `Tools`
  Command-line tools such as `ri_tool`
- `Tests`
  Native CTest-backed suites
- `protoengine`
  Prototype/reference web engine material, not the native runtime path

## Sandbox Project

The first local project space is:

- `Projects/Sandbox`

This folder may not exist in a fresh checkout. It is the workspace shape that `ri_tool --ensure-workspace` creates.

Expected child folders:

- `Projects/Sandbox/Config`
- `Projects/Sandbox/Content`
- `Projects/Sandbox/Scenes`
- `Projects/Sandbox/Saved`

## Tool Support

`ri_tool` now understands the workspace shape.

Useful commands:

- `ri_tool --workspace`
- `ri_tool --ensure-workspace`
- `ri_tool --save-scene-state`
- `ri_tool --load-scene-state`

## GitHub Publishing Note

`Assets/Source` is part of the local workspace shape but is ignored for GitHub publishing because raw art/source drops can be very large. Runtime-ready and standardized outputs belong under `Assets/Cooked`.

## Direction

This layout is meant to keep the engine depot, the toolchain, imported content, cooked content, and local projects separated from each other from day one.
