---
tags:
  - rawiron
  - migration
  - audit
---

# Prototype Engine Audit

## Source Workspace

- Prototype source: `Q:\anomalous-echo`
- Goal: migrate the engine and tooling concepts into `N:\RAWIRON` without dragging game-specific code into RawIron runtime libraries

## What Counts As Engine For This Port

Keep:

- runtime IDs
- runtime event bus
- schema and validation contracts
- structural graph and structural primitive systems
- world query and trace systems
- audio runtime systems
- debug instrumentation and snapshot systems
- editor-facing helpers that are truly engine-side

Leave behind for now:

- game actors and encounter logic
- one-off level content
- enemy, player, and mission scripts
- game-specific balancing values

## Prototype Modules Audited

### Clear Engine Modules

- `engine/runtimeIds.js`
- `engine/runtimeEvents.js`
- `engine/runtimeSchemas.js`
- `engine/structuralGraph.js`
- `engine/structuralPrimitives/convexClipper.js`
- `engine/structuralPrimitives/compiler.js`
- `engine/audio/AudioManager.js`
- `engine/debug/runtimeSnapshots.js`
- `engine/dev/RuntimeStatsOverlay.js`

### Engine-Heavy Logic Still Mixed Into App Code

These areas appear to be engine-worthy, but they are embedded in larger prototype runtime files and will need extraction passes:

- event engine flow
- world systems and trace/query behavior
- spatial indexing
- level/runtime validation wiring
- debug and instrumentation paths

## Prototype Documentation That Matters

- `Q:\anomalous-echo\obsidian\Engine\00 Engine Home.md`
- `Q:\anomalous-echo\obsidian\Engine\01 Runtime Flow.md`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`
- `Q:\anomalous-echo\obsidian\Engine\03 Event Engine.md`
- `Q:\anomalous-echo\obsidian\Engine\05 Debugging and Instrumentation.md`

These notes are strong enough to treat as design-spec input, not just commentary.

## Migration Map

| Prototype Module | RawIron Target | Status |
| --- | --- | --- |
| `runtimeIds.js` | `Source/RawIron.Runtime/RuntimeId` | `ported-pass-01` |
| `runtimeEvents.js` | `Source/RawIron.Runtime/RuntimeEventBus` | `ported-pass-01` |
| `spatialIndex.js` | `Source/RawIron.Spatial/SpatialIndex` | `ported-pass-03` |
| shared trace helpers in `index.js` | `Source/RawIron.Trace/TraceScene` | `ported-pass-04` |
| `structuralGraph.js` | `Source/RawIron.Structural/StructuralGraph` | `ported-pass-01` |
| `structuralPrimitives/convexClipper.js` | `Source/RawIron.Structural/ConvexClipper` | `ported-pass-01` |
| `structuralPrimitives/compiler.js` | `Source/RawIron.Structural/StructuralCompiler` | `ported-pass-01` |
| `runtimeSchemas.js` | `Source/RawIron.Validation/Schemas` | `ported-pass-05` |
| `audio/AudioManager.js` | `Source/RawIron.Audio/AudioManager` | `ported-pass-06` |
| `debug/runtimeSnapshots.js` | `Source/RawIron.Debug/RuntimeSnapshots` | `ported-pass-07` |
| mixed helper/environment runtime in `index.js` | `Source/RawIron.World/RuntimeState` | `ported-pass-08` |
| mixed runtime observers and counters in `index.js` plus `engine/dev/RuntimeStatsOverlay.js` | `Source/RawIron.World/Instrumentation` | `ported-pass-09` |
| event engine flow from docs and `index.js` | `Source/RawIron.Events/EventEngine` | `ported-pass-02` |
| template, prefab, and authored-content expansion helpers in `index.js` | `Source/RawIron.Content/PrefabExpansion` | `ported-pass-10` |
| runtime volume mode/channel parsing and typed world-volume helpers in `index.js` | `Source/RawIron.World/VolumeDescriptors` | `ported-pass-11` |
| authored volume registration helpers in `index.js` | `Source/RawIron.Content/WorldVolumeContent` | `ported-pass-12` |
| authored environment-volume registration helpers in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/VolumeDescriptors` | `ported-pass-13` |
| runtime environment tint and physics modifier fidelity gaps after the environment bridge | `Source/RawIron.World/RuntimeState` plus `Source/RawIron.Content/WorldVolumeContent` | `ported-pass-14` |
| custom gravity, wind, buoyancy, surface velocity, and radial-force helpers in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-15` |
| physics constraint volumes in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-16` |
| reflection probes, light importance bounds, and light portals in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-17` |
| visibility primitives and occlusion portals in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-18` |
| traversal links, ladder/climb helpers, and local grid snap volumes in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-19` |
| hint/skip brushes, camera confinement, lod override, and navmesh modifier volumes in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-20` |
| ambient audio volumes and spline helpers in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-21` |
| `streaming_level_volume`, `checkpoint_spawn_volume`, `teleport_volume`, `launch_volume`, and `analytics_heatmap_volume` in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-22` |
| generic trigger volumes, spatial query volumes, and trigger runtime orchestration in `index.js` | `Source/RawIron.Content/WorldVolumeContent` plus `Source/RawIron.World/RuntimeState` and `Source/RawIron.World/VolumeDescriptors` | `ported-pass-23` |
| trigger spatial indexing, point-query candidate collection, and trigger-query stats in `index.js` | `Source/RawIron.World/RuntimeState` plus `Source/RawIron.Spatial/SpatialIndex` and `Source/RawIron.World/Instrumentation` | `ported-pass-24` |
| foundational structural primitive geometry generation in `index.js` | `Source/RawIron.Structural/StructuralPrimitives` | `ported-pass-25` |
| expanded structural primitive geometry generation in `index.js` | `Source/RawIron.Structural/StructuralPrimitives` | `ported-pass-26` |
| advanced structural primitive geometry generation in `index.js` | `Source/RawIron.Structural/StructuralPrimitives` | `ported-pass-27` |
| native structural boolean operator compilation in `index.js` | `Source/RawIron.Structural/StructuralCompiler` | `ported-pass-28` |
| info-panel runtime binding resolution (`resolveInfoPanelBindingValue` and `resolveInfoPanelLines`) in `index.js` | `Source/RawIron.World/Instrumentation` | `ported-pass-29` |
| checkpoint world-state capture/restore for flags, values, and completed event IDs in `index.js` | `Source/RawIron.Events/EventEngine` | `ported-pass-30` |
| checkpoint payload parse/validation seam (`getCheckpoint` shape checks and restore fields) in `index.js` | `Source/RawIron.Validation/Schemas` | `ported-pass-31` |
| `convex_hull_aggregate` compile behavior in `index.js` | `Source/RawIron.Structural/StructuralCompiler` | `ported-pass-32` |
| `array_primitive` and `symmetry_mirror_plane` expansion helpers in `index.js` | `Source/RawIron.Structural/StructuralCompiler` | `ported-pass-33` |
| `structural_detail_modifier` and `non_manifold_reconciler` compile behavior in `index.js` | `Source/RawIron.Structural/StructuralCompiler` | `ported-pass-34` |
| structural compile orchestration and `bevel_modifier_primitive` behavior in `index.js` | `Source/RawIron.Structural/StructuralCompiler` | `ported-pass-35` |
| structural cutter-volume orchestration including `boolean_subtractor`, intersect-style clipping, and targeted compile-time clipping in `index.js` | `Source/RawIron.Structural/StructuralCompiler` | `ported-pass-36` |
| deferred target-operation collection for `terrain_hole_cutout`, spline deform/ribbon, surface scatter, and shrinkwrap helpers in `index.js` | `Source/RawIron.Structural/StructuralCompiler` | `ported-pass-37` |
| deferred target-operation execution for `terrain_hole_cutout` and `shrinkwrap_modifier_primitive` in `index.js` | `Source/RawIron.Structural/StructuralDeferredOperations` | `ported-pass-38` |
| deferred target-operation execution for `surface_scatter_volume` and `spline_mesh_deformer` in `index.js` | `Source/RawIron.Structural/StructuralDeferredOperations` | `ported-pass-39` |
| deferred target-operation execution for `spline_decal_ribbon` in `index.js` | `Source/RawIron.Structural/StructuralDeferredOperations` | `ported-pass-40` |
| mixed world systems in `index.js` | future `RawIron.World` style runtime layer | `later-pass` |

## Recommended Next Passes

1. Continue extracting the remaining mixed world/runtime logic out of `index.js` into broader `RawIron.World` services.
2. Connect `RawIron.Content` and `RawIron.Structural` to future scene/load-save paths so authored prefabs, templates, primitive nodes, and typed runtime volumes can flow into the runtime without JS glue.
3. Keep pushing editor-facing world/runtime inspection paths onto the native RawIron side as the player and editor shells grow up.
