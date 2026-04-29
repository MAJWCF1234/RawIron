---
tags:
  - rawiron
  - migration
  - pass-32
  - structural
  - aggregate
---

# Pass 32 Structural Convex Hull Aggregate Foundation

## Goal

Port the prototype's `convex_hull_aggregate` compile behavior into RawIron so authored target groups can produce a simplified native convex hull instead of relying on prototype-side structural glue.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on the aggregate compile helper that wraps multiple authored targets in one simplified hull.

## Native Landing Zone

- `Source/RawIron.Structural/StructuralCompiler`

## What Landed

RawIron now has native support for:

- `convex_hull_aggregate`

The structural compiler now owns:

- aggregate target-ID resolution from authored target lists
- supported-target collection for aggregate compilation
- convex hull generation across the combined world-space solids of those targets
- compiled fragment-node emission for the resulting aggregate hull
- stable fragment-ID generation even when the base source node has no ID

## Why It Matters

This is a practical structural-authoring helper rather than a pure math feature.

It lets RawIron do something the prototype already proved useful:

- keep authored structural detail expressive
- then wrap it in a simpler aggregate collider or structural shell when needed

That is exactly the kind of small but important engine authoring utility that makes the world pipeline feel real instead of theoretical.

## Honest Boundary

This is still a focused compile helper, not the whole higher-level structural authoring stack.

RawIron still has more work ahead around:

- broader compile-pass orchestration
- richer modifier/operator authoring
- full scene/load/runtime assembly from authored structural nodes

So this pass closes one more structural utility seam without pretending the compile layer is finished.

## Tests

The import/runtime suite now verifies:

- fragment-ID prefix behavior when the source node has no ID
- convex-hull aggregate compilation over authored target groups
- aggregate output bounds across the combined target span
- usable compiled mesh output from the aggregate hull

## Result

RawIron now owns the prototype's convex-hull aggregate helper in native `C++`, which is another real step from primitive parity toward honest structural authoring parity.
