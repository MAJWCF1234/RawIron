---
tags:
  - rawiron
  - migration
  - pass-34
  - structural
  - detail
  - reconciler
---

# Pass 34 Structural Detail and Reconciler Foundation

## Goal

Port the prototype's compile-time structural cleanup helpers into RawIron so authored geometry can be marked as non-structural detail and risky authored solids can be hullized natively.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on:

- `structural_detail_modifier`
- `non_manifold_reconciler`

## Native Landing Zone

- `Source/RawIron.Structural/StructuralCompiler`
- `Source/RawIron.Structural/StructuralGraph`

## What Landed

RawIron now has native support for:

- authored detail tagging through `structural_detail_modifier`
- authored hull-reconciliation through `non_manifold_reconciler`
- native bounds generation for authored structural nodes
- native mesh-based world-space hull conversion for reconciled nodes

The structural compiler now owns:

- geometry bounds creation for authored native structural nodes
- overlap and target-ID checks for detail modifiers
- overlap and target-ID checks for non-manifold reconcilers
- non-structural detail tagging fields on structural nodes
- hullization of targeted risky nodes into native world-space `convex_hull` data

## Why It Matters

These helpers are about structural reliability, not just structural variety.

They let RawIron carry over two very practical prototype authoring ideas:

- mark some geometry as present-but-non-structural so structural-only logic can ignore it
- force suspicious geometry into a safer convex form instead of letting it stay fragile

That improves both compile sanity and later runtime/query behavior.

## Honest Boundary

This is still a focused compile-helper pass.

RawIron still has more work ahead around:

- richer modifier/operator orchestration end-to-end
- full compile-pass assembly over the expanded structural node vocabulary
- more of the prototype's authored structural post-processing layer

So this pass ports a real safety/cleanup seam without claiming the whole structural compiler is finished.

## Tests

The import/runtime suite now verifies:

- detail tagging for intersecting authored nodes
- non-structural detail flags for visibility/navigation exclusion
- non-manifold reconciler hullization into a world-space convex hull
- preservation of usable reconciled hull bounds near the authored source geometry

## Result

RawIron now owns the prototype's first structural cleanup/safety helpers in native `C++`, which makes the authored structural pipeline sturdier as the compile layer keeps growing.
