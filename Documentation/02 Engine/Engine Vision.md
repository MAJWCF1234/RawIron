---
tags:
  - rawiron
  - engine
  - vision
---

# Engine Vision

## Statement

RawIron is intended to be a **standalone native game engine**, not a web app port and not an Electron shell.

## Core Goals

- standalone runtime
- native editor
- native tools
- cross-platform architecture
- fast iteration
- strong workflow and tooling
- its own identity instead of imitating Unity or Unreal

## Design Taste

RawIron should preserve the parts that felt right in the prototype:

- the workflow
- the editor philosophy
- the object/world model
- the parts that favored practical in-house tooling over "black box" engines

## What Gets Discarded

- Electron
- browser-specific assumptions
- third-party web framework dependencies as a foundation layer
- any architecture that exists only because of the web stack

## What Gets Preserved

- design intent
- engine feel
- editing flow
- gameplay and content authoring concepts that proved themselves in the prototype

## Technical Direction

- C++ for the core runtime and engine systems
- the current engine port is native C++, not a JavaScript runtime transplant
- any future scripting layer is deferred and should not shape the core engine architecture
- renderer should be engine-owned, with Vulkan as a likely first backend
- editor should run the real engine runtime, not a fake or separate data model

## Product Shape

- runtime library
- editor application
- player executable
- asset tools
- import/cook/build pipeline
