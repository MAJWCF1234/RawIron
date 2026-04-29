---
tags:
  - rawiron
  - engine
  - scenekit
  - parity
---

# Scene Reference Coverage

## Purpose

RawIron needs a hard usability gate for the Scene Kit layer.

Current rule:

- do not call the Scene Kit layer usable until we can recreate at least **10 tracked scene references**

This is a compatibility target for workflows and engine services the project depends on.

## Status Labels

- `foundation-live`: the feature already exists in RawIron code today
- `preview-live`: Scene Kit can build and preview a native analog, with deeper renderer/runtime parity still pending
- `build-next`: model/path is clear, but runtime/renderer/tooling work is still needed
- `not-yet`: a subsystem still has to be designed and implemented

## Target Set

| Example | Reference ID | RawIron Track | Status |
| --- | --- | --- | --- |
| `scene_controls_orbit` | `reference://scene_controls_orbit` | orbit camera + helpers + viewport shell | `foundation-live` |
| `scene_geometry_cube` | `reference://scene_geometry_cube` | primitive mesh nodes + materials + transforms | `foundation-live` |
| `scene_interactive_cubes` | `reference://scene_interactive_cubes` | scene raycast utilities + primitive picking + input shell | `foundation-live` |
| `scene_terrain_raycast` | `reference://scene_terrain_raycast` | scene raycast utilities + custom terrain mesh preview | `preview-live` |
| `scene_lighting_spotlights` | `reference://scene_lighting_spotlights` | light descriptors + renderer spot-light path | `preview-live` |
| `scene_loader_gltf` | `reference://scene_loader_gltf` | asset import pipeline + scene instantiation | `preview-live` |
| `scene_animation_keyframes` | `reference://scene_animation_keyframes` | scene-authored keyframe sampling preview | `preview-live` |
| `scene_instancing_performance` | `reference://scene_instancing_performance` | repeated-node density preview for future instance submission | `preview-live` |
| `scene_materials_envmaps` | `reference://scene_materials_envmaps` | reflection-bay material staging preview | `preview-live` |
| `scene_audio_orientation` | `reference://scene_audio_orientation` | listener/source layout preview for future spatial audio | `preview-live` |

## Current Notes

- `RawIron.SceneUtilities` includes a 10-example registry, scene builders, and preview scenes for this cohort
- `ri_tool --scenekit-targets` prints the current parity gate
- `ri_tool --scenekit-example <slug>` renders one example preview to disk
- `RawIron.VisualShell` cycles the example cohort and launches the selected one as its own window
- `RawIron.Preview` opens the selected example through a Vulkan-presented interactive window on Windows, with software preview for headless output
