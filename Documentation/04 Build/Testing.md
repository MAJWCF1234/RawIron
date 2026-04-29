---
tags:
  - rawiron
  - testing
  - build
---

# Testing

## Current Native Test Coverage

RawIron now has a native test target:

- `RawIron.Core.Tests`
- `RawIron.EngineImport.Tests`

It also has smoke tests for the current executable hosts and tools:

- `RawIron.Player.Smoke`
- `RawIron.Editor.Smoke`
- `RawIron.Preview.Smoke`
- `RawIron.VisualShell.Smoke`
- `RawIron.Tool.SampleScene`
- `RawIron.Tool.Formats`
- `RawIron.Tool.Workspace`
- `RawIron.Tool.SceneKitChecks`

Current coverage includes:

- command-line parsing and fallback behavior
- scene hierarchy and world-position composition
- cycle prevention in parent/child relationships
- re-parent bookkeeping and child-list cleanup
- helper builder behavior
- helper input sanitization for grid size, axis dimensions, and orbit distance
- scene description output and resource counts
- scene query utilities
- scene raycast queries for primitive picking
- Scene Kit milestone checks for lit scene setup, orbit controls, lighting, model loading, and picking
- starter scene structure and animation movement
- player host boot and expected log output
- editor host boot and expected log output
- preview host boot and expected image-save output
- visual shell target discovery and expected launch-surface output
- tool sample-scene output path and expected scene summary
- tool format list path and expected extension output
- workspace discovery path and expected layout output
- Scene Kit parity gate reporting
- Scene Kit milestone reporting and preview-file output
- Vulkan SDK and runtime diagnostics reporting
- engine-side shaded cube preview rendering and image-file output
- workspace creation and directory verification
- runtime ID prefix sanitization and suffix generation
- runtime event bus metrics and listener behavior
- structural graph ordering, unresolved dependencies, and cycle reporting
- native structural primitive recognition plus compiled mesh generation for `box`, `plane`, `hollow_box`, `ramp`, `wedge`, `cylinder`, `cone`, `pyramid`, `capsule`, `frustum`, `geodesic_sphere`, `arch`, `hexahedron`, `convex_hull`, `roof_gable`, and `hipped_roof`
- native structural boolean operator target resolution plus `union`, `intersection`, and `difference` fragment compilation
- native convex-hull aggregate compilation over authored target groups
- native `array_primitive` expansion and `symmetry_mirror_plane` authored-node mirroring
- native `structural_detail_modifier` tagging and `non_manifold_reconciler` hullization behavior
- convex-solid clipping, bounds, transforms, and compiled fragment generation
- event normalization, hook execution, world-state mutation, groups, sequences, and named timers
- BSP spatial-index build filtering, bounds, box queries, ray queries, and rebuild behavior
- trace-scene candidate routing, overlap/ray/swept traces, slide movement, and ground hits
- runtime tuning sanitization, level payload validation, and schema metrics
- authored-content sanitization, template inheritance, prefab expansion, nested prefab transforms, and recursion rejection
- authored world-volume seed extraction and typed native volume conversion for collision, blockers, damage, safe zones, and fog
- authored environment-volume conversion for post-process, reverb, occlusion, and fluid runtime data
- authored physics-helper conversion for custom gravity, wind, buoyancy, surface velocity, and radial-force volumes
- authored physics-constraint conversion for typed axis-lock volumes
- authored lighting-helper conversion for reflection probes, light-importance bounds, and light portals
- authored visibility-helper conversion for portals, anti-portals, and occlusion portals
- authored traversal-link and local-grid-snap conversion
- authored hint/confinement/lod/navmesh guidance-volume conversion
- authored ambient-audio volume and spline conversion
- authored trigger-helper conversion for streaming level, checkpoint spawn, teleport, launch, and analytics heatmap volumes
- authored generic-trigger and spatial-query volume conversion
- environment tint-color preservation and native fluid-physics modifier aggregation
- native runtime aggregation for authored physics helpers, surface velocity, and radial-force flow
- native runtime physics-constraint queries and combined environment-state exposure
- native occlusion-portal toggling and visibility-helper metric counting
- native traversal-link and local-grid-snap runtime queries
- native hint/confinement/lod/navmesh helper-volume runtime queries
- native ambient-audio contribution queries
- native trigger-helper runtime queries and analytics heatmap accumulation
- native trigger enter/stay/exit orchestration and typed trigger directive emission
- native trigger spatial-index rebuilds, indexed point queries, candidate counts, and reuse across repeated updates
- managed audio creation, voice replacement, environment shaping, echo scheduling, muting, and metrics
- helper-library metrics aggregation, event-engine world-state snapshots, spatial snapshot summaries, and native debug-report formatting
- typed world-volume descriptors, clip-mode parsing, collision-channel parsing, damage volume defaults, camera-modifier selection, and safe-zone inclusion
- runtime volume containment, environment-state aggregation, helper-activity summarization, and world/runtime helper metrics
- helper-activity observer wiring, entity-I/O counters and capped history, spatial-query instrumentation counters, and runtime-stats-overlay state

## Commands

Build:

```powershell
cpp-dev
cmake --preset dev-msvc
cmake --build --preset build-dev-msvc
```

Run with CTest:

```powershell
ctest --test-dir .\build\dev-msvc --output-on-failure -V
```

Current MSVC suite size:

- 14 tests

Clang build and test:

```powershell
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && cmake --preset dev-clang && cmake --build --preset build-dev-clang"
ctest --test-dir .\build\dev-clang --output-on-failure -V
```

Run the executable directly:

```powershell
.\build\dev-msvc\Tests\RawIron.Core.Tests\RawIron.Core.Tests.exe
.\build\dev-msvc\Tests\RawIron.EngineImport.Tests\RawIron.EngineImport.Tests.exe
```

## Why This Matters

The Scene Kit library layer is now being verified as engine code, not treated as throwaway prototype glue.

As of this checkpoint, the current suite passes under both:

- MSVC
- Clang

Linux note:

- the codebase is being shaped so desktop tests and builds can travel to Linux cleanly
- current verified test runs in this workspace are still Windows-hosted

## Related Notes

- [[Automated Review and Scripted Camera]] — planned optional *visual* regression / scene-review automation (distinct from CTest headless runs; see handbook for boundaries).
