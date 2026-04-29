---
tags:
  - rawiron
  - migration
  - pass-33
  - structural
  - array
  - symmetry
---

# Pass 33 Structural Array and Symmetry Expansion Foundation

## Goal

Port the prototype's authored structural expansion helpers into RawIron so repeated and mirrored structure can be expanded natively instead of living only in the prototype compile path.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on the structural expansion helpers for:

- `array_primitive`
- `symmetry_mirror_plane`

## Native Landing Zone

- `Source/RawIron.Structural/StructuralCompiler`
- `Source/RawIron.Structural/StructuralGraph`

## What Landed

RawIron now has native support for:

- authored array expansion through `count` and offset-step transforms
- authored symmetry expansion across axis-aligned mirror planes
- transform helpers for authored structural nodes
- mirrored-node identity tracking through `mirroredFrom`

The structural compiler now owns:

- node transform-matrix construction from authored position, rotation, and scale
- offset-step matrix construction for array expansion
- authored repeated node creation for `array_primitive`
- authored position and Euler rotation mirroring for `symmetry_mirror_plane`
- stable generated IDs for array and mirrored nodes

## Why It Matters

This is a real authored-world step, not just a geometry step.

It lets RawIron start doing the kind of structural authoring work the prototype already proved useful:

- repeat structural motifs cheaply
- mirror authored blockout and architecture without hand-duplicating everything

That improves both engine capability and future editor ergonomics.

## Honest Boundary

This is a foundation pass for structural expansion helpers.

RawIron still has more work ahead around:

- richer compile-pass orchestration above the helper level
- merge-hull array behavior
- more modifier/operator authoring layers from the prototype
- full scene/runtime assembly around expanded structural nodes

So this pass brings over the native expansion seam without claiming the whole structural compile stack is finished.

## Tests

The import/runtime suite now verifies:

- authored array expansion count and generated IDs
- composed array-node transform placement
- resolved primitive type for expanded array outputs
- authored symmetry expansion count and generated IDs
- mirrored source tracking via `mirroredFrom`
- mirrored position and Euler rotation behavior

## Result

RawIron now owns the prototype's first structural expansion helpers in native `C++`, which is another meaningful step from primitive parity toward authored structural workflow parity.
