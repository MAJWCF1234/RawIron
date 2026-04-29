---
tags:
  - rawiron
  - migration
  - pass-27
  - structural
  - primitives
---

# Pass 27 Advanced Structural Primitive Geometry

## Goal

Push RawIron's native structural primitive kit further into the prototype's real authored geometry language instead of stopping at the earlier box/capsule/frustum layer.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on the next serious structural primitives the prototype uses for authored architecture and convex shape authoring.

## Native Landing Zone

- `Source/RawIron.Structural/StructuralPrimitives`

## What Landed

RawIron's native primitive kit now also covers:

- `arch`
- `hexahedron`
- `convex_hull`
- `roof_gable`
- `hipped_roof`

The native structural layer now owns:

- round and gothic arch mesh generation
- explicit hexahedron solid and mesh generation from authored vertices
- convex-hull solid and mesh generation from point clouds
- native gable-roof solid and mesh generation
- native hipped-roof solid and mesh generation

## Why It Matters

This pass moves RawIron out of the "mostly boxes and helper shapes" stage.

In particular:

- `arch` gives the engine a genuinely architectural primitive instead of only hard-edged solids
- `hexahedron` and `convex_hull` give the structural layer more honest authored polyhedron support
- the roof helpers start matching the prototype's real building-language vocabulary

That makes the structural port feel a lot more like an engine and a lot less like a collection of math utilities.

## Honest Boundary

RawIron still does **not** have full primitive/operator parity.

Important structural families still ahead include things like:

- sweeps and lofts
- terrain/displacement families
- broader primitive/operator assembly flows
- deeper compile/runtime wiring for authored structural worlds

So this pass closes another big chunk of the geometry gap without claiming the whole problem is finished.

## Tests

The import/runtime suite now verifies:

- primitive recognition for the advanced structural subset
- round and gothic arch mesh generation
- hexahedron solid and mesh generation
- convex-hull solid and mesh generation
- gable-roof solid and mesh generation
- hipped-roof solid and mesh generation

## Result

RawIron now owns a much more serious authored geometry vocabulary in native `C++`, which makes the next structural compile and editor-facing passes far more worthwhile.
