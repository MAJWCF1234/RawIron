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
- `Projects`
  Local engine projects and sandboxes
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

## Sandbox Project

The first local project space is:

- `Projects/Sandbox`

Current child folders:

- `Projects/Sandbox/Config`
- `Projects/Sandbox/Content`
- `Projects/Sandbox/Scenes`
- `Projects/Sandbox/Saved`

## Tool Support

`ri_tool` now understands the workspace shape.

Useful commands:

- `ri_tool --workspace`
- `ri_tool --ensure-workspace`

## Direction

This layout is meant to keep the engine depot, the toolchain, imported content, cooked content, and local projects separated from each other from day one.
