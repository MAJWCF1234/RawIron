---
tags:
  - rawiron
  - migration
  - pass-26
  - structural
  - primitives
---

# Pass 26 Structural Primitive Geometry Expansion

## Goal

Extend the first native structural primitive kit so RawIron starts covering more of the prototype's real geometry vocabulary instead of stopping at the initial box/ramp/cone layer.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass follows the prototype's broader primitive direction while keeping the implementation native to RawIron.

## Native Landing Zone

- `Source/RawIron.Structural/StructuralPrimitives`

## What Landed

RawIron's native primitive kit now also covers:

- `hollow_box`
- `capsule`
- `frustum`
- `geodesic_sphere`

The native structural layer now owns:

- hollow room-shell mesh generation
- capsule mesh generation
- frustum solid + mesh generation
- geodesic-sphere solid + mesh generation

## Why It Matters

This pass improves two things at once:

- structural primitive parity
- future boolean/compiler usefulness

In particular:

- `frustum` and `geodesic_sphere` now exist as real convex native shapes
- `hollow_box` now exists as a practical authored shell shape instead of a future note
- `capsule` now exists as a proper native primitive mesh instead of only a concept in the prototype docs

## Honest Boundary

RawIron still does **not** have full structural primitive parity.

Important geometry families still ahead:

- `arch`
- `hexahedron`
- `convex_hull`
- `roof_gable`
- `hipped_roof`
- sweeps, lofts, terrain/displacement, and the wider operator vocabulary

So this pass is another real step, not the final structural pass.

## Tests

The import/runtime suite now verifies:

- primitive recognition for the expanded subset
- hollow-box mesh generation
- frustum solid + mesh generation
- geodesic-sphere solid + mesh generation
- capsule mesh generation

## Result

RawIron now owns a meaningfully broader native structural primitive kit, and the structural port is no longer just “future compiler groundwork.”
