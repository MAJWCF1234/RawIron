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

- `RawIron.Runtime`: runtime IDs and runtime event bus
- `RawIron.Structural`: structural graph, convex clipper, compiler helpers, native structural primitives, and boolean operator compilation
- `RawIron.Events`: hook, action, timer, and sequence flow
- `RawIron.Spatial`: BSP-style broadphase foundations
- `RawIron.Trace`: overlap, ray, swept-box, slide, and ground queries
- `RawIron.Validation`: schema and validation contracts
- `RawIron.Content`: template, prefab, and authored-content expansion
- `RawIron.Audio`: managed audio and environment-shaped playback
- `RawIron.Debug`: snapshots and debug-report formatting
- `RawIron.World`: environment state, helper metrics, and instrumentation

## Why This Section Exists

The migration pass notes explain what was ported.

This handbook explains how RawIron is supposed to think.

Both matter:

- pass notes preserve port history
- handbook notes preserve engine intent

## Related Notes

- [[Engine Vision]]
- [[Architecture Direction]]
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
