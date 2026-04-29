---
tags:
  - rawiron
  - engine
  - scenekit
  - milestones
---

# Scene Kit Milestones

`RawIron Scene Kit` is now wired around a 10-example starter cohort instead of a one-off lit cube demo.

This is the current native usability gate for the Scene Kit layer:

1. orbit controls
2. geometry cube
3. interactive cubes
4. terrain raycasting
5. spot lights
6. glTF loader
7. animation keyframes
8. instancing performance
9. environment maps
10. positional audio orientation

## Current Status

As of this pass, all 10 starter examples are:

- buildable through `RawIron::SceneKit`
- renderable through the software preview path
- openable through `RawIron.Preview` with a Vulkan-presented interactive window on Windows
- browsable inside `RawIron.VisualShell` without embedding a software renderer into the shell itself
- checkable through `ri_tool --scenekit-checks`

The current labels mean:

- `foundation-live`: the native Scene Kit/runtime path already exists directly
- `preview-live`: Scene Kit can already build and preview a native analog in the shell, even if deeper renderer/runtime parity is still a next pass

## Native Entry Points

Scene Kit now exposes:

- `GetSceneKitExampleDefinitions()`
- `BuildSceneKitPreview(slug)`
- `BuildSceneKitMilestone(slug)`
- `RunSceneKitMilestoneChecks(...)`

That lets the engine use the same example registry for:

- tool checks
- shell browsing and launch
- documentation coverage
- future regression tests

## Visual Shell

`RawIron.VisualShell` now acts as the base-shell browser for the Scene Kit cohort.

Keyboard flow:

- `Up/Down`: select shell actions
- `Left/Right`: cycle the current Scene Kit example preview
- `Left/Right`: cycle the current Scene Kit example target
- `Enter` / `F5`: run the selected shell action

The shell no longer renders the example bitmap itself.
Instead, it keeps the retro desktop/browser role:

- shows the currently selected example metadata
- launches the selected example into `RawIron.Preview`
- keeps renderer ownership out of the shell

## Tooling

Current tool hooks:

- `ri_tool --scenekit-targets`
- `ri_tool --scenekit-checks`
- `ri_tool --scenekit-example <slug>`

Current preview app hooks:

- `RawIron.Preview --example <slug> --backend auto`
- `RawIron.Preview --example <slug> --backend vulkan`
- `RawIron.Preview --headless --example <slug> --output <path>`

`--scenekit-checks` writes preview images to:

- `Saved/Previews/SceneKit`

The current preview split is deliberate:

- software preview remains the deterministic headless/test renderer
- Vulkan now owns the interactive presentation path in the preview app on Windows
- `RawIron.VisualShell` stays retro and lightweight while launching the selected example into that native preview path
- interactive Scene Kit viewing no longer falls back to an embedded software preview window

## Current Asset

The native glTF example currently uses:

- `Assets/Source/scenekit_triangle.gltf`

It is intentionally tiny so Scene Kit can prove the import path without depending on a heavyweight external art drop.
