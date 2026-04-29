---
tags:
  - rawiron
  - migration
  - pass-38
  - structural
  - deferred
  - terrain
  - shrinkwrap
---

# Pass 38 Deferred Terrain and Shrinkwrap Execution

## Goal

Port the first executable deferred target operations from the prototype into RawIron so the engine can do more than just collect deferred ops during structural compile.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\01 Runtime Flow.md`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on:

- `terrain_hole_cutout`
- `shrinkwrap_modifier_primitive`

## Native Landing Zone

- `Source/RawIron.Structural/StructuralDeferredOperations`

## What Landed

RawIron now has native support for:

- terrain-hole cutout execution over compiled structural meshes
- shrinkwrap convex-hull generation from targeted compiled geometry
- deferred-operation execution results that report:
  - final node list
  - modified target IDs
  - replaced target IDs
- replacement of targeted source colliders when shrinkwrap wants to stand in for them

## Why It Matters

The previous pass gave RawIron a native deferred-operation list.

This pass makes that list useful by proving the engine can now execute the first deferred structural helpers in native `C++` instead of stopping at metadata.

That is a real step from “compiler bookkeeping” toward “compiler plus structural post-build processing.”

## Honest Boundary

This is still the beginning of deferred execution.

RawIron still needs later work for:

- spline mesh/decal execution
- surface-scatter execution
- UV remap / tri-planar deferred helper execution
- feeding deferred execution into the real scene/load/runtime pipeline

So this pass executes the first cleanly testable helpers without pretending the whole deferred system is complete.

## Tests

The import/runtime suite now verifies:

- box-volume terrain cutout execution removes targeted triangles by centroid test
- shrinkwrap execution creates a convex hull collider around targeted source geometry
- shrinkwrap can replace targeted source colliders while leaving untouched geometry alone

## Result

RawIron now owns the prototype's first real deferred structural execution helpers in native `C++`, which moves the structural port from collection into actual post-build behavior.
