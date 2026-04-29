---
tags:
  - rawiron
  - migration
  - pass-25
  - structural
  - primitives
---

# Pass 25 Structural Primitive Geometry Foundation

## Goal

Start closing the real structural-parity gap by giving RawIron native primitive builders instead of relying only on convex clipper/compiler helpers.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`

This pass focuses on the first useful primitive subset from the prototype engine's broader vocabulary.

## Native Landing Zone

- `Source/RawIron.Structural/StructuralPrimitives`

## What Landed

RawIron now has native primitive generation for:

- `box`
- `plane`
- `ramp`
- `wedge`
- `cylinder`
- `cone`
- `pyramid`

The native structural layer now owns:

- primitive-type recognition for this subset
- convex-solid generation where that makes sense
- compiled mesh generation for all of the above
- segment/sides control for cylinder and cone-style primitives

## Why It Matters

Before this pass, RawIron had:

- structural dependency ordering
- convex clipping
- convex compiler helpers

But it did not yet have real native primitive builders.

This pass changes that. RawIron can now generate a meaningful first structural primitive set in `C++` rather than only carrying the compile math around it.

## Honest Boundary

This is **not** full structural primitive parity.

The prototype still has a much wider primitive/operator vocabulary, including things like:

- arches
- hollow rooms
- lofts and sweeps
- hexahedra
- geodesic spheres
- roof helpers
- displacement/terrain families
- boolean/operator authoring flows above the primitive layer

So this pass is a foundation pass, not the end of structural porting.

## Tests

The import/runtime suite now verifies:

- primitive-type recognition
- convex-solid generation for box/ramp/wedge/cylinder/cone/pyramid
- compiled mesh generation for the first primitive subset
- expected triangle counts and bounds for those native builders

## Result

RawIron now owns its first real native structural primitive kit.

That moves the engine one step closer to honest structural parity instead of only porting helper volumes and runtime metadata.
