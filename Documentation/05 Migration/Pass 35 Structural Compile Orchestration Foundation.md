---
tags:
  - rawiron
  - migration
  - pass-35
  - structural
  - compiler
  - bevel
---

# Pass 35 Structural Compile Orchestration Foundation

## Goal

Port the prototype's higher-level structural compile flow into RawIron so the native compiler can process authored nodes through one deliberate sequence instead of only through isolated helper calls.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\00 Engine Home.md`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on:

- `compileStructuralGeometryNodes(...)`
- `bevel_modifier_primitive`
- orchestration of array, symmetry, aggregate, detail, and reconcile helpers

## Native Landing Zone

- `Source/RawIron.Structural/StructuralCompiler`
- `Source/RawIron.Structural/StructuralGraph`

## What Landed

RawIron now has native support for:

- authored bevel metadata application through `bevel_modifier_primitive`
- boolean-target detection directly from authored structural nodes
- native solid generation for boolean-additive authored nodes
- a structured compile result that carries:
  - expanded nodes
  - passthrough nodes
  - boolean targets
  - compiled geometry output
  - suppressed target IDs
- a higher-level compile entry point that runs:
  - symmetry expansion
  - array expansion
  - bevel application
  - detail tagging
  - non-manifold reconciliation
  - convex hull aggregate compilation
  - boolean union / intersection / difference compilation

## Why It Matters

This is the first pass where RawIron starts behaving like a structural compiler instead of a bag of structural helpers.

That matters because the prototype's power is not only in the existence of individual operators. It is in the order they run and how authored nodes flow through them.

With this pass, RawIron can now take a mixed authored structural list and process it through a native compile pipeline that resembles the prototype's real engine-side compile logic.

## Honest Boundary

This is still a foundation pass, not the final structural authoring system.

RawIron still has more work ahead around:

- cutter-volume orchestration for subtractive/intersect helper volumes
- wider structural modifier/operator families
- feeding this compile result into scene/load/runtime assembly directly

So this pass ports the structural orchestration seam without claiming the full prototype compiler is finished.

## Tests

The import/runtime suite now verifies:

- bevel metadata application on overlapping targeted boxes
- compile-time orchestration of array and symmetry expansion
- aggregate compilation inside the higher-level structural pass
- boolean union compilation from expanded authored nodes
- suppression of boolean-consumed targets from passthrough output
- passthrough preservation of detail-tagged and reconciled authored nodes

## Result

RawIron now owns the prototype's first real structural compile pipeline in native `C++`, which gives the structural layer a real compiler-shaped backbone instead of only standalone helper primitives.
