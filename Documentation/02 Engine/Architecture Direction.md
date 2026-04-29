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
