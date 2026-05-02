---
tags:
  - rawiron
  - engine
  - direction
---

# Core Runtime Direction

## Intent

For the early rebuild, RawIron should optimize for a compact runtime and practical tools over heavyweight framework complexity.

Starting strengths:

- fast startup
- low ceremony
- native runtime
- strong command-line and console culture
- data-driven content pipeline
- engine and tools that stay close to core systems

## Early Priorities

- host loop and runtime startup
- filesystem and path conventions
- logging and console output
- asset import and cooked-runtime separation
- map and scene representation
- renderer abstraction
- editor shell that uses the real runtime

## Anti-Goals For The First Pass

- giant reflection systems before there is a solid game loop
- overbuilt editor chrome before tools and assets work
- web-stack compatibility layers
- treating Vulkan itself as the engine architecture

## Practical Translation To RawIron

For RawIron this means:

- clean native runtime boundaries
- strong toolchain identity
- small executable hosts over a reusable core
- world and asset formats owned by the engine
- fast iteration favored over framework sprawl

## Runtime Core Baseline

`RawIron.Runtime` is the shared home for app and game lifecycle behavior. It should be the first place a host reaches for startup, per-frame ticking, shutdown, runtime events, and cross-system services.

The baseline Runtime core owns:

- runtime identity: stable id, display name, mode, and generated instance id
- runtime paths: workspace, game, save, and config roots
- runtime phases: uninitialized, starting, loading, running, paused, stopping, stopped, failed
- module lifecycle hooks: startup, frame, shutdown
- shared typed services for subsystems that need to be discovered without every app inventing a registry
- runtime events for phase changes, startup, frames, and shutdown
- a `RuntimeHostAdapter` that plugs the Runtime core into `ri::core::RunMainLoop`

The intended split is:

- `RawIron.Core`: low-level primitives, command line, logging, frame loop, input buffers, clocks, arenas
- `RawIron.Runtime`: app/game lifecycle, shared runtime context, service ownership, runtime-level events
- `RawIron.Content`: game manifests, asset data, support-data loading, validation
- game modules: game-specific world construction, gameplay state, authored tuning, and presentation

New apps and games should prefer adding a `RuntimeModule` and running it through `RuntimeCore` instead of creating another standalone lifecycle convention.

Game manifests enforce this with:

- `runtimeContract = rawiron-runtime-v1`
- `runtimeHost = RuntimeCore`
- `runtimeModule = <game runtime module identity>`
- `runtimeServices` containing `lifecycle`, `events`, `services`, `paths`, and `frame-clock`

App packages enforce this through `rawiron_add_app(...)` and direct app CMake targets linking `RawIron::Runtime`.
