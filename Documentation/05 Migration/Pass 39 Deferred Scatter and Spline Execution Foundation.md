---
tags:
  - rawiron
  - migration
  - pass-39
  - structural
  - deferred
  - spline
  - scatter
---

# Pass 39 Deferred Scatter and Spline Execution Foundation

## Goal

Port more of the prototype's deferred structural target-operation execution into RawIron so the native engine can do more than terrain cutouts and shrinkwrap colliders.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on:

- `surface_scatter_volume`
- `spline_mesh_deformer`

## Native Landing Zone

- `Source/RawIron.Structural/StructuralDeferredOperations`

## What Landed

RawIron now has native support for:

- deterministic surface-scatter compiled-geometry generation over targeted structural bounds
- spline-driven compiled-geometry clone generation over authored control points
- stable generated IDs for scatter and spline outputs
- `keepSource` behavior for spline-mesh deformation so the authored source geometry can stay or be replaced
- exposed helper entry points for scatter and spline deferred execution

## Why It Matters

The previous pass proved RawIron could execute the first deferred helpers.

This pass extends that from destructive post-processing into generative post-processing. That is important because the prototype uses these helpers to build authored structure after the initial compile, not just to trim it.

RawIron now owns more of that behavior in native `C++` instead of leaving it as prototype-only runtime glue.

## Honest Boundary

This is a foundation pass, not the full deferred world.

RawIron still needs later work for:

- `spline_decal_ribbon` execution
- `topological_uv_remapper` execution
- richer spline settings like explicit curve-type parity and projection-aware behavior
- feeding deferred execution directly into the real scene/load/runtime assembly path

So this pass focuses on clean, deterministic helpers that already fit the current structural compiler and compiled-geometry model.

## Tests

The import/runtime suite now verifies:

- surface scatter appends generated compiled geometry over targeted structural bounds
- scatter results stay deterministic and keep the original targets intact
- spline mesh deformation emits stable generated clone IDs along authored spline endpoints
- spline mesh deformation replaces or preserves the source geometry according to `keepSource`

## Result

RawIron now owns another meaningful slice of the prototype's deferred structural post-build behavior in native `C++`, which keeps shrinking the gap between the prototype compiler flow and the standalone engine.
