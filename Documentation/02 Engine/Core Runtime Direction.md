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

- host loop and runtime bootstrap
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
