---

## tags:
  - rawiron
  - engine
  - architecture
  - libraries

# Library Layers

## Intent

RawIron is separating the scene system into cleaner layers so the pure engine core does not get crowded with convenience code.

## Current Split

### `RawIron.Core`

Owns the fundamentals:

- math primitives
- scene graph
- transforms
- node/component storage
- host loop and core runtime services
- native action-binding registries, conflict-aware rebinding, and input-label formatting helpers
- native render-facing post-process preset definitions and stack composition helpers

### `RawIron.Runtime`

Owns reusable runtime services that are engine-level but not scene-graph fundamentals:

- runtime IDs
- runtime event bus
- runtime service metrics

### `RawIron.Validation`

Owns prototype-derived schema and validation services that keep authored data contracts outside the scene core:

- stored tuning sanitization
- level payload validation
- validation metrics

### `RawIron.Content`

Owns prototype-derived authored-content expansion that should stay engine-side instead of living in app glue:

- generic content values for authoring expansion
- vec2, vec3, quaternion, and scale sanitization
- clamped numeric helper parsing
- entity-template inheritance and merge behavior
- prefab instance transforms and nested prefab expansion
- authored ID prefixing and recursion detection
- authored world-volume translation into typed native runtime descriptors
- authored environment-volume translation into typed native runtime descriptors
- pipeline-side asset extraction inventory / manifest bookkeeping (development and validation; see [[Asset Extraction Inventory]])
- declarative model definition interchange for data-driven compositions (see [[Declarative Model Definition]])

### `RawIron.Events`

Owns reusable event and sequence flow that should remain independent of any one game:

- hook processing
- world flags and numeric values
- event cooldown and run-limit state
- action groups
- target groups
- sequences and named timers

### `RawIron.Spatial`

Owns reusable broadphase world-query foundations:

- axis-aligned bounds
- ray/AABB tests
- BSP-style static broadphase indexing
- box and ray candidate queries

### `RawIron.Trace`

Owns reusable collision and movement-query helpers built on top of the broadphase layer:

- overlap traces
- ray traces
- swept traces
- slide resolution
- ground probing

### `RawIron.Audio`

Owns prototype-derived managed audio services without depending on browser audio APIs:

- managed sound wrappers
- active voice replacement
- environment-driven playback shaping
- delayed echo scheduling
- audio metrics
- native output via **miniaudio** (`AudioBackend`): UTF-8 paths open correctly on Windows (wide APIs), decoded clip playback defaults to **non-spatialized** stereo for predictable mix until listener/sound positioning is wired, optional mixer period sizing via `MiniaudioBackendSettings`

### `RawIron.Debug`

Owns prototype-derived instrumentation helpers that can summarize engine state without pulling game code into the runtime:

- helper-library metric aggregation (Helper Telemetry Bus umbrella — [[Helper Telemetry Bus]])
- event-engine world-state snapshots
- spatial-index summary snapshots
- compact machine-readable state formatting
- full native debug-report formatting

### `RawIron.World`

Owns prototype-derived world/runtime helper logic that sits between pure engine systems and debug/report surfaces:

- typed runtime-volume descriptors
- native presentation-state helpers for objectives, messages, subtitles, and capped presentation history
- optional native access-feedback state for granted/denied use responses, unlock objectives, and locked/unlocked follow-through hints
- optional native dialogue-cue state for authored NPC interaction lines, repeat gating, guidance-hint suppression, and objective follow-through
- optional native NPC-agent state for patrol paths, authored animation intents, speak-once interaction gating, and runtime history (see [[NPC Behavior Support]])
- optional native pickup-feedback state for item pickup callouts, objective follow-through, and hint suppression
- optional native inventory loadout for hotbar/backpack/off-hand, stacking, and logic inventory gates ([[Inventory and Possession]])
- optional native level-flow presentation state for load/ready status, level toasts, story intro callouts, and checkpoint restore notices
- optional native signal-broadcast state for unknown-voice callouts, subtitle pairing, and guidance-hint suppression
- native interaction-prompt state helpers for action labels, verb-aware prompt text, and suppression flags
- post-process, reverb, occlusion, and fluid runtime creation helpers
- localized-fog and fog-blocker runtime volume creation helpers
- active physics-volume modifier aggregation for fluid behavior
- tint-color-aware local environment state
- clip-mode and collision-channel parsing
- camera-modifier and safe-zone queries
- runtime volume containment
- active post-process and audio-environment aggregation
- helper-activity summarization
- world/runtime helper metrics
- helper-activity observer tracking
- entity-I/O instrumentation counters and capped history (logic graph ↔ bus contract: [[Entity IO and Logic Graph]])
- spatial-query instrumentation counters
- runtime-stats-overlay attachment and visibility state

### `RawIron.SceneUtilities`

Owns convenience behavior that is useful but not fundamental:

- helper builders
- helper/query utilities
- scene traversal and typed collection helpers
- orbit-camera support
- scripted **automated camera review** sequences for development (orbit rig + JSON/programmatic steps; see [[Automated Review and Scripted Camera]])
- convenience scene assembly helpers

Public-facing identity:

- plain name: `RawIron Scene Kit`
- CMake alias: `RawIron::SceneKit`

### `RawIron.SceneSamples`

Owns sample/demo content assembly:

- starter scene construction
- starter scene animation helpers

### `RawIron.Render.Software`

Owns early engine-side software rendering helpers that are useful for previews, smoke tests, and deterministic image output before the full runtime renderer is mature:

- shaded cube preview rendering
- bitmap output helpers
- render-output smoke-test foundations

### `RawIron.Structural`

Owns prototype-derived structural systems that should stay reusable and renderer-agnostic:

- structural dependency graphing
- convex-solid clipping
- structural compile helpers
- fragment mesh generation
- native primitive builders for `box`, `plane`, `hollow_box`, `ramp`, `wedge`, `cylinder`, `cone`, `pyramid`, `capsule`, `frustum`, `geodesic_sphere`, `arch`, `hexahedron`, `convex_hull`, `roof_gable`, and `hipped_roof`
- boolean-operator compile helpers for `union`, `intersection`, and `difference`
- convex-hull aggregate compilation over authored target groups
- authored array and symmetry expansion helpers
- authored detail-marking and hull-reconciliation helpers

### `RawIron.DevInspector`

Optional developer observability side-channel (snapshot registration, diagnostic streams, pluggable transports, guarded dev commands). See [[Development Inspector]].

## Why This Split Matters

This keeps the engine honest.

The things that felt too prototype-like or too convenience-oriented can still exist as first-party code without polluting the base runtime layer.

## Current Rule

If a feature is foundational engine state, it belongs in `RawIron.Core`.

If it is useful but convenience-heavy, it should live in a utility library.

If it is demo/sample assembly, it should live in a sample library.

If it is a reusable engine service or structural-system import from the prototype, it should live in a dedicated library instead of getting stuffed into `RawIron.Core`.