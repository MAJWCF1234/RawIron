---
tags:
  - rawiron
  - architecture
  - engine
---

# Architecture Direction

## Top-Level Split

- **RawIron.Core**
  Shared engine runtime library
- **RawIron.Player**
  Standalone runtime/game executable
- **RawIron.Editor**
  Native editor application using the same runtime
- **RawIron.Tools**
  Importers, cookers, packers, validators, and project tools
- **Games**
  Built-in game/runtime modules used to prove the engine path (`LiminalHall`, `WildernessRuins`)

## Platform Priorities

- primary desktop targets: **Windows** and **Linux**
- current non-targets: **macOS**, **iOS**, and **Android**

RawIron should stay desktop-portable without letting Apple/mobile concerns distort the first architecture.

## Engine-Level Subsystems

- platform layer
- scene/world model
- entity/component model
- asset system
- serialization
- rendering frontend
- rendering backend(s)
- input
- audio
- scripting
- console/logging
- editor integration

## Rendering Direction

The engine should not expose Vulkan directly as the engine architecture.

Preferred separation:

- render frontend: engine-facing scene/material/mesh/resource API
- render backend: Vulkan first, other backends later if needed
- presentation policy: native post-process preset catalog and stack composition owned above the backend

This keeps the engine from becoming "a Vulkan wrapper" instead of a real platform.

## Editor Principle

The editor should use the same runtime, same scene model, same components, and same asset pipeline as the shipped runtime whenever possible.

This is the shortest path toward a cohesive engine/tool ecosystem.

## Current Stack Direction

- language: **C++20**
- build system: **CMake**
- fast build backend: **Ninja**
- first graphics backend: **Vulkan**
- possible platform layer: **SDL3**
- first editor UI shell: **Dear ImGui**
- scripting layer: deferred until the core native engine is stable

## Current Implemented Shape

- `RawIron.Player`, `RawIron.Editor`, `RawIron.Preview`, and `RawIron.VisualShell` are active native executable hosts.
- `RawIron.Render.Vulkan` owns the Windows interactive preview path and diagnostics.
- `RawIron.Render.Software` owns deterministic headless preview snapshots.
- `RawIron.SceneUtilities` owns Scene Kit helpers, import smoke paths, starter scenes, scripted camera review, and the ten-example milestone gate.
- `RawIron.EditorPreview` and `RawIron.Editor.BundledGames` connect editor preview scenes to built-in game modules.
- `RawIron.Logic`, `RawIron.World`, `RawIron.Content`, and `RawIron.Structural` are now substantial native libraries rather than future placeholders.

## Companion Notes

- [[Platform Support]]
- [[Repository Layout]]

## First Major Milestones

1. Core app loop and platform layer
2. Scene/entity model and serialization
3. Asset system
4. Render abstraction
5. First backend
6. Native editor shell
7. Hot reload and live iteration
8. Project/game API layer

Several milestone foundations already exist. The next architecture risk is less "make a module exist" and more "make the editor/runtime/content pipeline use the same mature contracts end to end."
