---
tags:
  - rawiron
  - engine
  - runtime
  - cpp
---

# Engine Home

This section documents how the native RawIron runtime works at a high level so the C++ port keeps the prototype's best engine ideas without inheriting the web shell.

## Core Reading Order

- [[Current Engine Review]]
- [[01 Runtime Flow]]
- [[02 World Systems]]
- [[03 Event Engine]]
- [[04 Level Design Patterns]]
- [[05 Debugging and Instrumentation]]
- [[06 Content Assembly]]

## Current Engine Thesis

RawIron is strongest when it behaves like a small authored native runtime instead of a generic sandbox or a browser app with engine ambitions.

- authored scenes define structure
- events and local logic define sequencing
- spatial and trace services define what the world actually means
- environment and audio services shape the space around the player
- instrumentation keeps the runtime inspectable instead of mysterious
- tooling and editor workflows are part of the engine, not an afterthought

## Native Port Rule

The port rule is simple:

- keep the engine logic
- keep the authoring patterns
- keep the debugging surfaces
- rebuild them in C++

The prototype JavaScript is design input and reference behavior, not the runtime RawIron intends to ship.

That means:

- no Electron shell
- no browser-only assumptions
- no dragging game-specific code into engine libraries
- no pretending the old implementation is the final architecture

## Current Native Module Stack

The native engine already has real C++ landing zones for the first major prototype systems:

- `RawIron.Core`: math, host loop, scene graph, render command plumbing, action bindings, post-process presets, and crash diagnostics
- `RawIron.Runtime`: runtime IDs and runtime event bus
- `RawIron.Logic`: logic graph authoring, ports, visual primitives, world actor ports, and logic kit manifests
- `RawIron.Structural`: structural graph, convex clipper, compiler helpers, native structural primitives, boolean operators, cutter volumes, and deferred operations
- `RawIron.Events`: hook, action, timer, and sequence flow
- `RawIron.Spatial`: BSP-style broadphase foundations
- `RawIron.Trace`: overlap, ray, swept-box, slide, and ground queries
- `RawIron.Validation`: schema and validation contracts
- `RawIron.Content`: template, prefab, authored-content expansion, asset documents, manifests, asset inventory, and declarative model definitions
- `RawIron.Audio`: managed audio and environment-shaped playback
- `RawIron.Debug`: snapshots and debug-report formatting
- `RawIron.World`: runtime volumes, helper metrics, presentation/NPC/inventory states, trigger orchestration, checkpointing, text overlays, and instrumentation
- `RawIron.SceneUtilities`: Scene Kit helpers, importers, starter scenes, scripted camera review, scene state I/O, and the ten-example milestone gate
- `RawIron.DevInspector`: optional snapshot/diagnostic side channel
- `RawIron.Render.Software`: deterministic preview snapshots
- `RawIron.Render.Vulkan`: Vulkan bootstrap, command/presentation path, and native preview window support

App-owned editor preview registration now lives in `Apps/RawIron.Editor` rather than a shared engine module.

## Why This Section Exists

The migration pass notes explain what was ported.

This handbook explains how RawIron is supposed to think.

Both matter:

- pass notes preserve port history
- handbook notes preserve engine intent

## Related Notes

- [[Engine Vision]]
- [[Architecture Direction]]
- [[Current Engine Review]]
- [[Library Layers]]
- [[Repository Layout]]
- [[01 Runtime Flow]]
- [[02 World Systems]]
- [[03 Event Engine]]
- [[06 Content Assembly]]
- [[05 Debugging and Instrumentation]]
- [[Asset Extraction Inventory]]
- [[Declarative Model Definition]]
- [[Automated Review and Scripted Camera]]
- [[NPC Behavior Support]]
- [[Entity IO and Logic Graph]]
- [[Inventory and Possession]]
- [[Helper Telemetry Bus]]
