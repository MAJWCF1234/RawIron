---
tags:
  - rawiron
  - engine
  - layout
---

# Repository Layout

## Root

- `Apps`: native executable hosts.
- `Assets`: source and cooked assets. `Assets/Source` is kept local/ignored for GitHub because raw art can be large.
- `Config`: workspace-level configuration.
- `Documentation`: Obsidian vault and design notes.
- `Games`: game modules and game executables.
- `Saved`: generated logs, previews, scratch data, and tool output.
- `Scripts`: helper scripts and automation.
- `Source`: shared engine libraries.
- `Tests`: native test targets.
- `ThirdParty`: vendored external code.
- `Tools`: command-line and content/tooling utilities.
- `protoengine`: prototype/reference web engine material, not the native runtime target.

## Current Source Modules

### `Source/RawIron.Core`

Core runtime fundamentals:

- command-line parsing and logging
- host loop and fixed-step pacing
- starter math types
- scene graph, components, transforms, and camera helpers
- action binding and input-label helpers
- render command stream, recorder, submission-plan foundations
- post-process preset catalog and stack composition
- crash diagnostics and stacktrace integration

### `Source/RawIron.Audio`

Native managed audio layer:

- miniaudio backend and stub fallback
- managed sound wrappers
- active voice ownership
- environment-shaped playback
- delayed echo scheduling
- audio metrics

### `Source/RawIron.Content`

Authored-content and asset-document layer:

- generic content-value tree and value schema helpers
- asset documents and game manifests
- pipeline asset extraction inventory
- declarative model definitions
- scripted scalar helpers
- template inheritance and merge behavior
- prefab transform extraction, nesting, and recursion rejection
- authored world/environment/helper volume conversion into typed runtime descriptors

### `Source/RawIron.Debug`

Debug snapshot and report layer:

- helper-library metrics aggregation
- event-engine world-state snapshots
- spatial-index summaries
- compact machine-readable state formatting
- full native debug-report formatting

### `Source/RawIron.DevInspector`

Optional development observability side channel:

- named snapshot sources
- diagnostic streams
- pluggable transports
- guarded development commands

### `Source/RawIron.Runtime`

Reusable runtime services:

- runtime IDs
- runtime tuning
- runtime event bus
- experience presets
- entity-I/O event contract
- runtime metrics

### `Source/RawIron.Logic`

Logic graph and authoring layer:

- logic graph execution
- port schema and circuit signals
- world actor ports
- logic visual primitives
- logic authoring helpers
- logic kit manifests

### `Source/RawIron.World`

World/runtime helper layer:

- typed runtime-volume descriptors
- presentation, access, dialogue, pickup, level-flow, signal, inventory, NPC, hostile, and vitality state helpers
- checkpoint persistence
- trigger orchestration and trigger spatial index helpers
- helper telemetry and activity summaries
- entity-I/O bridge wiring between logic graphs and the runtime bus
- text overlay events/state
- headless module verification
- runtime instrumentation counters

### `Source/RawIron.Events`

Event runtime:

- event normalization
- world flag/value state
- hook processing
- target-group resolution
- action-group execution
- sequence scheduling
- named timers

### `Source/RawIron.Spatial`

Spatial-query foundation:

- axis-aligned bounds
- static broadphase indexing
- box and ray candidate queries
- volume containment helpers

### `Source/RawIron.Trace`

Trace, physics, and movement helpers:

- overlap, ray, and swept traces
- slide movement
- ground probing
- object and entity physics helpers
- kinematic physics
- locomotion tuning and movement controller helpers
- trace-scene metrics

### `Source/RawIron.Validation`

Schema and validation contracts:

- schema registry and validation reports
- scalar, string, object, collection, and primitive checks
- coercion, migration, and reference-integrity helpers
- path normalization
- ID format and color parsing

### `Source/RawIron.SceneUtilities`

Scene Kit and utility layer:

- helper builders for primitives, lights, grids, axes, orbit cameras, and procedural terrain
- scene traversal, query, bounds, and camera confinement helpers
- raycast utilities and camera-to-ray helpers
- OBJ, glTF/GLB, and FBX import paths
- scene state save/load
- starter workspace scene construction and animation
- scripted orbit-camera review
- ten Scene Kit milestone examples and checks

Note: `Source/RawIron.SceneSamples` is an empty legacy directory and is not an active CMake module. Starter scenes now live in `RawIron.SceneUtilities`.

### `Source/RawIron.Render.Software`

Deterministic software preview renderer:

- scene preview rendering
- shaded cube and Scene Kit snapshot support
- BMP output helpers

### `Source/RawIron.Render.Vulkan`

Vulkan preview/render backend foundation:

- platform/startup diagnostics
- native Scene Kit preview window path on Windows
- software-upload fallback presenter
- command buffer/list/recorder helpers
- frame submission
- intent staging
- pipeline-state cache

### `Source/RawIron.Structural`

Structural runtime/compiler layer:

- structural dependency graph ordering
- convex clipping
- structural primitive mesh generation
- compiler orchestration
- boolean operators for `union`, `intersection`, and `difference`
- convex-hull aggregate compilation
- array/symmetry expansion helpers
- detail/reconciler helpers
- cutter-volume clipping
- deferred terrain, shrinkwrap, scatter, spline, and decal-ribbon operation helpers

## Games

### `Games/Common`

Shared game runtime diagnostics drawing helpers.

### `Games/LiminalHall`

- `RawIron.Game.LiminalHall`: game runtime module
- `RawIron.LiminalGame`: standalone game executable
- editor preview hooks for the bundled editor-preview path

### `Games/WildernessRuins`

- `RawIron.Game.ForestRuins`: game runtime module
- `RawIron.ForestRuinsGame`: standalone game executable
- editor preview hooks for the bundled editor-preview path

## Applications

Applications follow the same ownership principle as games: app-specific code belongs in `Apps/<AppName>/`, while
shared engine infrastructure belongs in `Source/`. See [[Application Package Format]].

### `Apps/RawIron.Player`

Starter runtime host:

- shared host loop
- starter workspace scene
- scripted camera review flags
- player-mode simulation tick

### `Apps/RawIron.Editor`

Native editor shell:

- shared host loop
- editor-specific startup/composition
- app-local preview scene registry
- app-local bundled game preview registration
- editor workspace view
- preview scene registry integration
- bundled game preview support when enabled

### `Apps/RawIron.Preview`

Scene Kit preview host:

- software BMP snapshot mode
- Windows Vulkan interactive preview mode
- Scene Kit example selection
- photo-mode FOV options

### `Apps/RawIron.VisualShell`

Keyboard-first launch shell:

- preview launch actions
- Scene Kit example browsing
- Vulkan diagnostics
- test/tool actions
- documentation and previews shortcuts
- live activity log

## Tools

### `Tools/ri_tool`

Current command surface:

- workspace discovery and workspace creation
- format-family reporting
- asset standardization into `.ri_asset.json`
- Scene Kit target/check/example commands
- post-process preset reporting
- Vulkan diagnostics
- software preview rendering
- sample scene reporting
- scene-state save/load

## Tests

- `Tests/RawIron.Core.Tests`: core runtime, Scene Kit, import, motion/trace, and dev-inspector coverage.
- `Tests/RawIron.EngineImport.Tests`: merged prototype-import suite covering runtime, content, world, logic, events, triggers, inventory, checkpointing, structural operations, and helper/volume families.

## Direction

The host executables should stay thin.
The shared libraries should own reusable engine behavior.
The game modules should prove the runtime without becoming engine dependencies.
The tools layer should own importer, cooker, validation, and workspace complexity.
The convenience layer should stay outside `RawIron.Core` unless a feature becomes fundamental engine state.
