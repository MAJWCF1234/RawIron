---
tags:
  - rawiron
  - migration
  - pass-36
  - structural
  - cutter
  - csg
---

# Pass 36 Structural Cutter Volume Foundation

## Goal

Port the prototype's compile-time cutter-volume behavior into RawIron so authored subtractive and intersect-style structural clipping can run inside the native compile pipeline.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\00 Engine Home.md`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on:

- `boolean_subtractor`
- intersect-style cutter volumes
- targeted compile-time clipping over additive structural solids

## Native Landing Zone

- `Source/RawIron.Structural/StructuralCompiler`
- `Source/RawIron.Structural/StructuralGraph`

## What Landed

RawIron now has native support for:

- authored `opType` on structural nodes
- compile-time cutter-volume detection in the structural compiler
- `boolean_subtractor` as a targeted subtractive cutter
- intersect-style cutter nodes driven either by `boolean_intersection` with a primitive payload or an authored `opType: intersect`
- compile-time clipping over remaining additive structural targets after the higher-level structural operator passes

The native compile path now:

- gathers cutter volumes during structural compile orchestration
- prebuilds convex cutter solids and planes
- applies subtractive or intersect clipping only to matching targets
- emits compiled fragment nodes for touched structural targets
- keeps untouched targets as passthrough structural nodes

## Why It Matters

This is the next real step past boolean operators over named additive target groups.

The prototype did not stop at unions and aggregates. It also used authored cutter-like primitives and op-style nodes to carve or constrain structural solids during compile time.

With this pass, RawIron can now do that natively instead of leaving those authoring ideas trapped in the prototype compile path.

## Honest Boundary

This is still a focused structural pass.

RawIron still has more work ahead around:

- additional cutter/helper families like `door_window_cutout`
- richer compile-time modifier families
- direct scene/load/runtime assembly over the native structural compile result

So this pass ports the core subtractive/intersect cutter seam without claiming the full prototype structural language is complete.

## Tests

The import/runtime suite now verifies:

- targeted subtractive clipping through `boolean_subtractor`
- intersect-style clipping through authored `opType`
- suppression of touched compile targets from passthrough output
- correct fragment bounds after subtractive and intersect clipping

## Result

RawIron now owns the prototype's first native compile-time cutter-volume orchestration in `C++`, which gives the structural compiler a much more honest CSG-style authoring path.
