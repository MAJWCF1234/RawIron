---
tags:
  - rawiron
  - engine
  - layout
---

# Repository Layout

## Root

- `Source`: shared engine code
- `Apps`: executable hosts
- `Tools`: command-line tools and import/cook utilities
- `Documentation`: project notes and design docs

## Current Code Modules

### `Source/RawIron.Core`

Shared core runtime code.

Current contents:

- command-line parsing
- host interface
- simple logging
- fixed-step main loop bootstrap
- starter math types
- scene graph and transform composition

### `Source/RawIron.Runtime`

Runtime-service library for engine-side systems imported from the prototype.

Current contents:

- runtime ID generation
- runtime event bus
- runtime bus metrics

### `Source/RawIron.Validation`

Schema and validation library for prototype-derived authored data contracts.

Current contents:

- stored runtime-tuning sanitization
- level payload validation
- validation metrics

### `Source/RawIron.Content`

Authored-content expansion library for prototype-derived templates, prefabs, and transform normalization.

Current contents:

- generic content-value tree
- vec2, vec3, quaternion, and scale sanitization
- finite-number and finite-integer clamp helpers
- entity-template merge and resolution flow
- prefab transform extraction and node transformation
- nested prefab expansion and recursion rejection
- authored world-volume conversion into typed native runtime descriptors
- authored environment-volume conversion into typed native runtime descriptors

### `Source/RawIron.Events`

Event-runtime library for prototype-derived hook, action, and timer flow.

Current contents:

- event normalization
- world flag and value state
- hook processing
- target-group resolution
- action-group execution
- sequence scheduling
- named timers

### `Source/RawIron.Spatial`

Spatial-query foundation library for prototype-derived broadphase systems.

Current contents:

- axis-aligned bounds
- ray helpers
- BSP-style spatial indexing
- box candidate queries
- ray candidate queries

### `Source/RawIron.Trace`

Trace and movement-query library layered over the spatial broadphase.

Current contents:

- overlap box tracing
- ray tracing
- swept box tracing
- slide movement helpers
- ground-hit probing

### `Source/RawIron.Audio`

Managed audio library for prototype-derived engine-side playback behavior.

Current contents:

- managed sound wrappers
- active voice-line ownership
- environment profile normalization and playback shaping
- delayed echo scheduling
- audio metrics

### `Source/RawIron.Debug`

Debug and instrumentation library for prototype-derived snapshot/report behavior.

Current contents:

- helper-library metrics aggregation
- event-engine world flag/value snapshots
- spatial-index summary snapshots
- compact state snapshot formatting
- full debug-report formatting

### `Source/RawIron.World`

World/runtime helper library for prototype-derived mixed runtime systems.

Current contents:

- typed runtime-volume descriptors
- post-process, reverb, occlusion, and fluid runtime creation helpers
- localized-fog and fog-blocker runtime creation helpers
- clip-volume mode parsing
- collision-channel parsing
- safe-zone and camera-modifier queries
- runtime volume containment for box, cylinder, and sphere helpers
- active post-process aggregation
- active audio reverb and occlusion aggregation
- localized-fog, fog-blocker, and fluid-environment merges
- helper-activity summarization
- world/runtime helper metrics
- helper-activity observer tracking over the runtime event bus
- entity-I/O counters and capped event history
- spatial-query instrumentation counters
- runtime-stats-overlay state mirroring

### `Source/RawIron.SceneUtilities`

Utility library layered on top of the core scene model.

Current contents:

- helper builders
- helper input sanitization
- scene query utilities
- orbit-camera utility

### `Source/RawIron.SceneSamples`

Sample/demo library layered on top of scene utilities.

Current contents:

- starter scene construction
- starter scene animation helpers

### `Source/RawIron.Render.Software`

Software preview renderer for early engine-side image generation and deterministic render smoke tests.

Current contents:

- shaded cube preview rendering
- BMP image output helpers
- early engine-owned render-output path outside the Vulkan backend

### `Source/RawIron.Structural`

Structural runtime/compiler library for prototype-engine world-building systems.

Current contents:

- structural dependency graph ordering
- structural phase classification
- convex polygon and solid clipping
- convex-solid mesh compilation
- structural compiler helpers for bounds, transforms, and fragment generation
- native primitive builders for `box`, `plane`, `hollow_box`, `ramp`, `wedge`, `cylinder`, `cone`, `pyramid`, `capsule`, `frustum`, `geodesic_sphere`, `arch`, `hexahedron`, `convex_hull`, `roof_gable`, and `hipped_roof`
- boolean-operator compile helpers for `union`, `intersection`, and `difference`
- convex-hull aggregate compilation over authored target groups
- authored array and symmetry expansion helpers
- authored detail-marking and hull-reconciliation helpers

### `Apps/RawIron.Player`

Standalone runtime shell.

Current contents:

- thin host executable
- frame loop bootstrap
- player-mode sample scene and simulation tick

### `Apps/RawIron.Editor`

Native editor shell.

Current contents:

- thin host executable
- frame loop bootstrap
- editor-mode sample scene inspection

### `Apps/RawIron.Preview`

Native preview window for engine-side render smoke testing.

Current contents:

- shaded cube preview window
- headless BMP output mode for tests and automation

### `Apps/RawIron.VisualShell`

Temporary launch shell for the current engine workspace.

Current contents:

- X68000-inspired keyboard-first visual shell
- native preview launch action
- diagnostics and test-run actions
- documentation and previews folder shortcuts
- live activity log inside the shell window

### `Tools/ri_tool`

Command-line tools bootstrap.

Current contents:

- planned import/cook/pack command surface
- format-family placeholder output
- sample scene inspection command
- shaded cube preview render command

## Direction

This layout is meant to stay understandable even as the engine grows.

The host executables should stay thin.
The core should absorb reusable systems.
The tools layer should own importer and cooker complexity instead of leaking that complexity into the runtime.
The convenience layer should stay outside the core whenever it does not need to be fundamental engine state.
