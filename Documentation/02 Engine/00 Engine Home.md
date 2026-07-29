# Engine Overview

RawIron is organized as a shared engine with game-specific runtime modules mounted on top of it.

## Major subsystems

- [[02 Engine/01 Runtime Core|Runtime core]]
- [[02 Engine/02 Rendering|Rendering]]
- [[02 Engine/03 Content and Game Format|Content, config, and game format]]
- [[02 Engine/04 Multiplayer|Multiplayer and netcode]]
- [[02 Engine/05 Apps and Tools|Apps and tools]]
- [[02 Engine/06 Plugins and Mods|Plugins and mods]]
- [[02 Engine/07 Editor and Authoring|Editor and authoring]]
- [[02 Engine/08 World and Interaction|World and interaction]]
- [[02 Engine/09 AI, UI, and Data Surfaces|AI, UI, and data surfaces]]
- [[02 Engine/10 Mod and Plugin Authoring|Mod and plugin authoring]]
- [[02 Engine/11 Structural Primitives Fulfillment Plan|Structural primitives fulfillment plan]]
- [[02 Engine/12 Package Runtime|Package runtime]]

## Engine libraries in `Source/`

- `RawIron.Core`
- `RawIron.Audio`
- `RawIron.Content`
- `RawIron.Debug`
- `RawIron.DevInspector`
- `RawIron.Editor.BundledGames`
- `RawIron.EditorPreview`
- `RawIron.Events`
- `RawIron.Logic`
- `RawIron.Render.Software`
- `RawIron.Render.Vulkan`
- `RawIron.Runtime`
- `RawIron.SceneUtilities`
- `RawIron.Spatial`
- `RawIron.Structural`
- `RawIron.Trace`
- `RawIron.UI`
- `RawIron.Validation`
- `RawIron.World`

## Default build switches

The root `CMakeLists.txt` exposes `RAWIRON_BUILD_*` switches for apps, games, tools, tests, DevInspector, and transport providers such as ENet and EOS.

## Ownership rules

- Lifecycle, services, and frame stepping belong to `RuntimeCore`.
- Shared tuning contracts are enforced in engine/shared game code before gameplay boot continues.
- Games provide authored values, authored levels, authored plugin declarations, and game modules.
- Releases ship the workspace as a workspace, not as a source-only SDK.
