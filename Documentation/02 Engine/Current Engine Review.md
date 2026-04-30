---
tags:
  - rawiron
  - engine
  - review
  - architecture
---

# Current Engine Review

Review date: 2026-04-29

This note is the current engine-state snapshot for the Obsidian vault. It reflects the native CMake workspace, source modules, executable targets, tools, tests, and the main documentation corrections made during this review.

## Summary

RawIron is now a native C++20 engine workspace with a meaningful engine-service stack, two game modules, native hosts, a preview path, tooling, and a growing test suite. The older docs were still partly describing an earlier bootstrap: some pages mentioned a separate `RawIron.SceneSamples` library, a fixed local `Projects/Sandbox`, a smaller test suite, and placeholder-only tooling. Those are no longer accurate.

The engine is currently best described as:

- native C++20, built with CMake presets
- Windows-first with Linux kept as the second desktop target
- Vulkan-backed for interactive preview on Windows
- software-rendered for deterministic headless preview snapshots
- Scene Kit centered for reusable scene helpers, example coverage, import smoke tests, and preview flows
- data-heavy world/runtime services ported out of the prototype into native libraries
- thin player/editor/preview hosts layered over shared runtime modules

## Source Modules

The active CMake source modules are:

- `RawIron.Core`: math, command line, host loop, logging, action bindings, scene graph, render command plumbing, photo mode, camera confinement, crash diagnostics, and post-process preset definitions.
- `RawIron.Audio`: miniaudio-backed native playback, environment shaping, managed sounds, voice ownership, echo scheduling, and audio metrics.
- `RawIron.Content`: content values, schemas, asset documents, game manifests, asset inventory, declarative model definitions, prefab/template expansion, scripted scalar helpers, and authored volume conversion into runtime descriptors.
- `RawIron.Debug`: runtime snapshot collection and compact/full debug report formatting across runtime, events, spatial, audio, and world metrics.
- `RawIron.DevInspector`: optional developer observability side channel for snapshots, diagnostic streams, transports, and guarded development commands.
- `RawIron.Runtime`: runtime IDs, runtime tuning, event bus, experience presets, and entity-I/O telemetry contracts.
- `RawIron.Logic`: logic graphs, ports, signal schema, world actor ports, visual primitives, authoring helpers, and logic kit manifests.
- `RawIron.World`: runtime volumes, helper telemetry, inventory, presentation states, NPC/hostile states, checkpoint persistence, trigger orchestration, text overlays, player vitality, headless verification, and entity-I/O bridge wiring.
- `RawIron.Events`: hook processing, event state, action groups, target groups, sequences, and named timers.
- `RawIron.Spatial`: AABB primitives, static broadphase indexing, volume containment, and query metrics.
- `RawIron.Trace`: object/entity physics, kinematic physics, movement control, locomotion tuning, spatial query helpers, and trace scene metrics.
- `RawIron.Validation`: schema registry, scalar/string/object/collection constraints, coercion, references, migration helpers, color parsing, ID format checks, and validation reports.
- `RawIron.SceneUtilities`: Scene Kit helpers, model import dispatch, glTF/FBX/OBJ import support, animation clips, raycasts, scene state I/O, scripted camera review, starter workspace scenes, and Scene Kit milestone examples.
- `RawIron.Render.Vulkan`: Vulkan bootstrap, preview presenter, native Scene Kit preview window path, command recording, frame submission, intent staging, and pipeline-state cache.
- `RawIron.Render.Software`: deterministic software Scene Kit preview rendering and BMP output.
- `RawIron.Structural`: structural graphs, convex clipping, primitive builders, compiler orchestration, boolean operators, aggregate hulls, array/symmetry expansion, detail modifiers, reconciler helpers, cutter volumes, and deferred-operation helpers.

## Game And Host Targets

The workspace now includes game/runtime targets in addition to the engine libraries:

- `Games/Common`: shared runtime diagnostics drawing helpers.
- `Games/LiminalHall`: `RawIron.Game.LiminalHall` plus `RawIron.LiminalGame`.
- `Games/WildernessRuins`: `RawIron.Game.ForestRuins` plus `RawIron.ForestRuinsGame`.
- `Apps/RawIron.Player`: starter runtime host.
- `Apps/RawIron.Editor`: native editor shell with app-local preview registration and bundled-game preview wiring.
- `Apps/RawIron.Preview`: Scene Kit preview host with software snapshot output and Vulkan interactive preview on Windows.
- `Apps/RawIron.VisualShell`: keyboard-first native launch shell for previews, docs, diagnostics, and tests.
- `Tools/ri_tool`: workspace, Scene Kit, Vulkan, post-process, scene-state, preview, and asset-standardization commands.

## Tooling State

`ri_tool` is no longer just a placeholder. Current command surfaces include:

- `--workspace`
- `--ensure-workspace`
- `--formats`
- `--asset-standardize <source-path>`
- `--asset-standardize-dir <source-dir>`
- `--scenekit-targets`
- `--scenekit-checks`
- `--scenekit-example <slug>`
- `--postprocess-presets`
- `--vulkan-diagnostics`
- `--render-cube`
- `--sample-scene`
- `--save-scene-state`
- `--load-scene-state`

The current standard asset document is `.ri_asset.json`. Older extension families such as `.ri_model`, `.ri_mesh`, `.ri_scene`, `.ri_mat`, `.ri_tex`, `.ri_audio`, and `.ri_meshc` remain experimental or legacy aliases in the tooling output.

## Scene Kit State

Scene Kit is the active convenience and review layer inside `RawIron.SceneUtilities`.

Current coverage includes:

- helper builders for primitives, lights, grids, axes, orbit cameras, and procedural terrain
- scene traversal and typed collection helpers
- camera confinement queries
- raycast and perspective camera ray helpers
- model loading through OBJ, glTF/GLB, and FBX paths
- starter workspace scene construction and animation
- scripted orbit-camera review sequences
- ten Scene Kit milestone examples with software preview output
- Windows interactive preview through `RawIron.Preview`

The old `Source/RawIron.SceneSamples` folder exists only as an empty legacy directory. It is not an active CMake module; starter scene and example behavior now live in `RawIron.SceneUtilities`.

## Test Reality

The current generated MSVC CTest inventory has 17 tests:

- host smoke tests for player, editor, preview, and visual shell
- tool smoke tests for sample scene, formats, workspace, Scene Kit targets/checks, Vulkan diagnostics, workspace creation, render-cube, save/load scene state
- native suites for `RawIron.Core.Tests` and `RawIron.EngineImport.Tests`
- stacktrace smoke coverage through `RawIron.EngineImport.StacktraceSmoke`

The source test binaries are much broader than the CTest count suggests: `RawIron.Core.Tests` contains dozens of cases for core runtime, Scene Kit, importers, motion/trace helpers, and the development inspector, while `RawIron.EngineImport.Tests` is a merged prototype-import suite covering runtime, content, logic, world, triggers, inventory, checkpointing, structural operations, and many volume/helper families.

## Documentation Fixes From This Review

- Removed active-library claims for `RawIron.SceneSamples`.
- Updated the repository map to reflect the current app package model, including app-local editor preview wiring inside `Apps/RawIron.Editor`.
- Updated workspace docs to describe `Projects/Sandbox` as created by `ri_tool --ensure-workspace`, not present in every checkout.
- Updated testing docs from 14 to 17 generated CTest entries.
- Updated Scene Kit/helper docs from five checks to the current ten-example gate.
- Updated asset-format docs to make `.ri_asset.json` the current standard document while leaving older extension families as experimental aliases.

## Current Boundaries

- `Assets/Source` is intentionally ignored for GitHub publishing because it contains heavyweight raw art/source assets; local work can still use it.
- `Projects/Sandbox` is a workspace shape understood by tooling, created on demand.
- Linux is an architectural target, but Windows has the most verified interactive/runtime coverage today.
- The preview app owns Windows interactive presentation today; non-Windows preview still falls back to saved software output.
- The editor is a real native shell, but it is still a workbench over shared runtime/preview modules rather than a complete production editor.

## Related Notes

- [[00 Engine Home]]
- [[Repository Layout]]
- [[Library Layers]]
- [[Built-In Helpers]]
- [[Scene Kit Milestones]]
- [[Testing]]
- [[Workspace Layout]]
- [[Asset Pipeline]]
- [[File Formats]]
