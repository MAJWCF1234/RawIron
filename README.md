# RawIron

RawIron is a native, standalone game engine prototype.

Current direction:

- C++20 core
- standalone player and editor applications
- tools-first asset pipeline
- no Electron or browser dependency
- desktop target direction: Windows first, Linux alongside it
- Apple and mobile platforms intentionally out of scope for now
- early architecture priorities: fast startup, compact runtime, strong tooling, and clean host boundaries
- integrated scene helpers instead of prototype-only addon glue

Current built-in scene utilities:

- scene graph and transform hierarchy
- mesh, material, camera, and light descriptors
- grid helper
- axes helper
- orbit-camera utility
- orbit camera framing and reconstruction helpers
- world-bounds helpers for scene content
- scene query helpers
- scene raycast helpers for picking-style workflows
- camera-to-ray helpers for viewport selection
- Wavefront OBJ loading for external Scene Kit mesh content
- a ten-example Scene Kit usability gate with preview output

The convenience-heavy pieces now live in utility libraries instead of `RawIron.Core`.
The current helper layer is `RawIron.SceneUtilities`, treated as `RawIron Scene Kit` and exposed in CMake as `RawIron::SceneKit`.

RawIron also now tracks a hard Scene Kit replacement-library gate: the engine should be able to reproduce at least 10 official examples before we call the layer genuinely usable.

## Repository Layout

- `Source/RawIron.Core`: shared engine runtime
- `Source/RawIron.Audio`: backend-agnostic managed audio runtime imported from prototype audio systems
- `Source/RawIron.Debug`: debug snapshot and report foundation imported from prototype instrumentation systems
- `Source/RawIron.Runtime`: runtime services imported from the prototype engine foundation
- `Source/RawIron.World`: world/runtime environment, helper-metrics, and instrumentation foundation imported from prototype mixed runtime systems
- `Source/RawIron.Events`: event and sequence runtime imported from prototype hook/action systems
- `Source/RawIron.Spatial`: BSP-style broadphase queries imported from prototype world systems
- `Source/RawIron.Trace`: shared tracing and movement queries imported from prototype world systems
- `Source/RawIron.Validation`: schema and validation contracts imported from prototype runtime schemas
- `Source/RawIron.Content`: authored-content template, prefab, world-volume, and environment-volume expansion imported from prototype world-assembly helpers
- `Source/RawIron.SceneUtilities`: helper and query utilities over the core scene layer
- `Source/RawIron.SceneSamples`: sample/demo scene assembly over the utility layer
- `Source/RawIron.Render.Software`: deterministic headless preview rendering for smoke tests and image output
- `Source/RawIron.Structural`: structural graph, convex-solid tooling, and native primitive builders imported from prototype engine systems
- `Apps/RawIron.Player`: standalone runtime shell
- `Apps/RawIron.Editor`: native editor shell
- `Apps/RawIron.Preview`: native Scene Kit cube preview window
- `Apps/RawIron.VisualShell`: keyboard-first launch shell for previews, diagnostics, docs, and tests
- `Tools/ri_tool`: command-line asset and project tooling
- `Documentation`: Obsidian-friendly project documentation
- `Assets/Source`: imported source assets
- `Assets/Cooked`: cooked runtime-ready assets
- `Projects/Sandbox`: first local playground project
- `Config`: workspace-level config
- `Saved`: workspace-level logs and generated state
- `Scripts`: automation and helper scripts
- `ThirdParty`: vendored or mirrored external dependencies

## Build

Open a development shell with `cpp-dev`, then run:

```powershell
cmake --preset dev-msvc
cmake --build --preset build-dev-msvc
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --workspace
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --scenekit-targets
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --scenekit-checks
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --vulkan-diagnostics
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --render-cube
.\build\dev-msvc\Apps\RawIron.VisualShell\RawIron.VisualShell.exe
ctest --test-dir .\build\dev-msvc --output-on-failure -V

cmake --preset dev-clang
cmake --build --preset build-dev-clang
ctest --test-dir .\build\dev-clang --output-on-failure -V
```

Optional crash diagnostics can be toggled with
`-DRAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS=ON|OFF`.
RawIron currently vendors `backward-cpp` for symbolic stack traces on Windows,
so readable traces depend on debug symbols (`/Zi` and matching PDBs).

Repository-root launchers:

- `RawIron Visual Shell.lnk`
- `Launch RawIron Visual Shell.cmd`
- `Launch RawIron Editor.cmd`

Linux root launcher:

- `./launch_rawiron_editor.sh`
- `./launch_rawiron_visual_shell.sh`
- pass `--frames <N>` to either launcher for bounded headless-style runs

On Linux, the intended build shape is:

```bash
cmake --preset dev-linux-clang
cmake --build --preset build-dev-linux-clang
ctest --test-dir build/dev-linux-clang --output-on-failure -V
```

## Current Status

This repository is the active landing zone for porting the prototype engine ideas into native C++ passes.

Migration passes currently landed:

- `RawIron.Runtime` now covers runtime IDs and runtime event-bus metrics
- `RawIron.Validation` now covers stored tuning sanitization, level payload validation, and schema metrics
- `RawIron.Content` now covers content-value storage, authoring sanitization, template expansion, prefab transforms, nested prefab expansion, and authored world/environment/trigger volume conversion into typed native descriptors
- `RawIron.Structural` now covers structural dependency ordering, convex-solid compile helpers, native primitive builders for `box`, `plane`, `hollow_box`, `ramp`, `wedge`, `cylinder`, `cone`, `pyramid`, `capsule`, `frustum`, `geodesic_sphere`, `arch`, `hexahedron`, `convex_hull`, `roof_gable`, and `hipped_roof`, plus native boolean operator compilation for `union`, `intersection`, and `difference`, convex-hull aggregate compilation over authored target groups, authored `array_primitive` / `symmetry_mirror_plane` expansion helpers, native `bevel_modifier_primitive` metadata application, native `structural_detail_modifier` / `non_manifold_reconciler` behavior, a higher-level structural compile pipeline that runs those helpers together, compile-time cutter-volume clipping for targeted subtractive/intersect structural authoring, and native deferred-operation execution helpers for terrain cutouts, shrinkwrap collider generation, deterministic surface scatter, spline-driven mesh placement, and projected spline decal ribbons
- `RawIron.Events` now covers hook execution, conditions, groups, sequences, and named timers
- `RawIron.Spatial` now covers axis-aligned bounds, BSP broadphase indexing, box queries, and ray queries
- `RawIron.Trace` now covers overlap traces, ray traces, swept box traces, slide movement, and ground probes
- `RawIron.Audio` now covers managed sounds, active voice replacement, environment-driven playback shaping, and echo scheduling
- `RawIron.Debug` now covers helper-library metric aggregation, event-engine world-state snapshots, spatial snapshot summaries, and native debug-report formatting
- `RawIron.World` now covers typed world-volume descriptors, post-process/reverb/occlusion/fluid creation helpers, localized-fog and fog-blocker volume creation helpers, native custom-gravity/wind/buoyancy/surface-velocity/radial-force helper volumes, typed physics constraints, traversal links, ladder/climb helpers, local grid snap volumes, hint/skip brushes, camera confinement volumes, lod override volumes, navmesh modifier volumes, ambient audio volumes and spline helpers, generic trigger volumes, spatial query volumes, streaming level volumes, checkpoint spawn volumes, teleport volumes, launch volumes, analytics heatmap volumes, reflection probes, light-importance bounds, light portals, typed visibility primitives for portals/anti-portals/occlusion-portals, native occlusion-portal closed/open state, tint-color-aware environment-state aggregation, authored physics-helper aggregation, native trigger-helper runtime queries, native trigger spatial-index rebuilds, indexed trigger candidate queries, trigger enter/stay/exit orchestration, typed trigger directives, analytics heatmap counter accumulation, runtime volume containment, helper-activity summarization, world/runtime helper metrics, runtime observer tracking, entity-I/O counters, spatial-query counters, and stats-overlay state
- `RawIron.Render.Vulkan` now reports richer Vulkan diagnostics such as instance extensions, instance layers, validation-layer availability, and selected-device summaries shared by the player and tooling paths
- `RawIron.Core` now defaults to paced fixed-step host loops with quieter frame logging so the bootstrap apps behave less like runaway debug harnesses
- `RawIron.EngineImport.Tests` verifies the imported prototype-engine systems under both MSVC and Clang
