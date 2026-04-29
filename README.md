# RawIron

RawIron is a native C++ game engine project focused on fast desktop iteration, clear runtime boundaries, and practical tooling for authored worlds.

The engine is currently Windows-first, with Linux kept in the build and architecture path. The repository includes the shared engine libraries, native application hosts, tooling, test coverage, documentation, and two built-in game/runtime modules used to exercise the engine.

## Highlights

- C++20 engine code organized into focused runtime libraries
- Native player, editor, preview, and visual shell applications
- Vulkan preview backend with deterministic software-rendered snapshots for tests
- Scene Kit utility layer for scene helpers, model import smoke tests, scripted camera review, and preview examples
- Runtime systems for events, logic graphs, world volumes, inventory, NPC state, triggers, checkpoints, audio, tracing, and instrumentation
- Tooling for workspace inspection, asset standardization, Scene Kit checks, Vulkan diagnostics, preview rendering, and scene-state save/load
- CTest-backed smoke and engine-import test coverage

## Repository Layout

- `Source/RawIron.Core`: core runtime, math, scene graph, host loop, input helpers, render command plumbing, and diagnostics
- `Source/RawIron.Audio`: managed audio and miniaudio-backed playback
- `Source/RawIron.Content`: asset documents, manifests, prefab/template expansion, and authored-content conversion
- `Source/RawIron.Debug`: runtime snapshots and debug report formatting
- `Source/RawIron.DevInspector`: optional development snapshot and diagnostic channel
- `Source/RawIron.Events`: hook, action, sequence, timer, and world-state event flow
- `Source/RawIron.Logic`: logic graph, port schema, visual primitives, and logic kit support
- `Source/RawIron.Runtime`: runtime IDs, event bus, tuning, experience presets, and entity-I/O telemetry
- `Source/RawIron.SceneUtilities`: Scene Kit helpers, starter scenes, importers, raycasts, animation, scripted camera review, and scene-state I/O
- `Source/RawIron.Spatial`: AABB primitives, broadphase indexing, and spatial queries
- `Source/RawIron.Structural`: structural graph, primitive builders, compiler helpers, boolean operators, cutter volumes, and deferred operations
- `Source/RawIron.Trace`: trace scene, movement, locomotion, kinematic, entity, and object physics helpers
- `Source/RawIron.Validation`: schema registry, constraints, coercion, reference checks, and validation reports
- `Source/RawIron.World`: world volumes, triggers, presentation state, inventory, NPC state, checkpoints, helper telemetry, text overlays, and runtime instrumentation
- `Source/RawIron.Render.Software`: deterministic software preview rendering
- `Source/RawIron.Render.Vulkan`: Vulkan bootstrap, diagnostics, preview presentation, and command submission foundations
- `Apps`: native player, editor, preview, and visual shell hosts
- `Games`: built-in Liminal Hall and Wilderness Ruins runtime modules and game executables
- `Tools/ri_tool`: command-line workspace, asset, preview, Scene Kit, and diagnostics tooling
- `Tests`: native test targets
- `Documentation`: Obsidian-friendly engine documentation
- `Assets/Cooked`: runtime-ready and standardized asset output
- `Assets/Source`: local source-asset workspace, intentionally excluded from GitHub because raw art drops can be large

## Build

From a configured C++ development shell:

```powershell
cmake --preset dev-msvc
cmake --build --preset build-dev-msvc
ctest --test-dir .\build\dev-msvc --output-on-failure -V
```

Useful tooling commands after a successful build:

```powershell
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --workspace
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --scenekit-targets
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --scenekit-checks
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --vulkan-diagnostics
.\build\dev-msvc\Tools\ri_tool\ri_tool.exe --render-cube
.\build\dev-msvc\Apps\RawIron.VisualShell\RawIron.VisualShell.exe
```

Clang build:

```powershell
cmake --preset dev-clang
cmake --build --preset build-dev-clang
ctest --test-dir .\build\dev-clang --output-on-failure -V
```

Linux-oriented preset:

```bash
cmake --preset dev-linux-clang
cmake --build --preset build-dev-linux-clang
ctest --test-dir build/dev-linux-clang --output-on-failure -V
```

## Applications

- `RawIron.Player`: starter runtime host
- `RawIron.Editor`: native editor shell
- `RawIron.Preview`: Scene Kit preview window and headless snapshot host
- `RawIron.VisualShell`: keyboard-first launch surface for previews, diagnostics, tests, and documentation
- `RawIron.LiminalGame`: Liminal Hall game host
- `RawIron.ForestRuinsGame`: Wilderness Ruins game host

Repository-root launchers are available for common local workflows:

- `Launch RawIron Editor.cmd`
- `Launch RawIron Visual Shell.cmd`
- `launch_rawiron_editor.sh`
- `launch_rawiron_visual_shell.sh`
- `play-liminal.cmd`
- `play-forest-ruins.cmd`

## Tooling

`ri_tool` currently supports:

- workspace discovery and workspace folder creation
- standard asset document generation with `.ri_asset.json`
- Scene Kit target listing, example rendering, and milestone checks
- Vulkan diagnostics
- post-process preset reporting
- software preview rendering
- sample scene reporting
- scene-state save/load

## Testing

The MSVC build currently exposes 17 generated CTest entries, including host smoke tests, tooling smoke tests, the core test suite, the engine-import suite, and stacktrace smoke coverage.

```powershell
ctest --test-dir .\build\dev-msvc -N
ctest --test-dir .\build\dev-msvc --output-on-failure -V
```

## Documentation

Project documentation lives in `Documentation` as an Obsidian-friendly vault. Start with:

- `Documentation/00 Home.md`
- `Documentation/02 Engine/Current Engine Review.md`
- `Documentation/02 Engine/Repository Layout.md`
- `Documentation/04 Build/Testing.md`
