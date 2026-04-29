---
tags:
  - rawiron
  - migration
  - pass-40
  - structural
  - deferred
  - ribbon
  - decal
---

# Pass 40 Deferred Spline Decal Ribbon Execution

## Goal

Port another deferred structural helper from the prototype into RawIron so the native engine can execute authored ribbon-style post-build geometry instead of treating it as prototype-only runtime decoration.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on:

- `spline_decal_ribbon`

## Native Landing Zone

- `Source/RawIron.Structural/StructuralDeferredOperations`

## What Landed

RawIron now has native support for:

- spline-decal ribbon mesh generation over authored control points
- projection of ribbon samples down onto compiled structural bounds
- support for target-less ribbon execution by projecting against the full compiled structural set
- native authored fields for ribbon execution such as:
  - `segments`
  - `width`
  - `projectionHeight`
  - `projectionDistance`
  - `offsetY`

## Why It Matters

The prototype uses spline ribbons as a post-build authored helper, not as a core primitive.

By executing this in native `C++`, RawIron takes another real step away from depending on the prototype runtime for authoring-time world assembly behavior. It also gives the structural pipeline a more useful output shape for future editor and scene assembly work.

## Honest Boundary

This pass executes the geometric part of spline ribbons.

RawIron still needs later work for:

- richer material/shader metadata for decal rendering
- projection against true triangle traces instead of current compiled-bounds approximation
- `topological_uv_remapper` execution
- wiring deferred structural execution directly into full scene/load/runtime assembly

So this pass is about native execution of the geometry and placement behavior first.

## Tests

The import/runtime suite now verifies:

- spline-decal execution generates a ribbon mesh with the expected triangle count
- ribbon samples project onto compiled structural bounds with the authored offset
- ribbon execution works even when no explicit target IDs are provided

## Result

RawIron now owns another meaningful deferred structural helper in native `C++`, which keeps shrinking the gap between the prototype's post-build authoring flow and the standalone engine.
