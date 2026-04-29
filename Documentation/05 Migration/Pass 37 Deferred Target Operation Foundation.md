---
tags:
  - rawiron
  - migration
  - pass-37
  - structural
  - deferred
  - post-build
---

# Pass 37 Deferred Target Operation Foundation

## Goal

Port the prototype's deferred post-build target-operation seam into RawIron so terrain cutouts and other target-driven structural helpers have a native compile output instead of pretending to be normal shell geometry.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\obsidian\Engine\01 Runtime Flow.md`
- `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

This pass focuses on:

- `terrain_hole_cutout`
- `spline_mesh_deformer`
- `spline_decal_ribbon`
- `surface_scatter_volume`
- `scatter_surface_primitive`
- `shrinkwrap_modifier_primitive`

## Native Landing Zone

- `Source/RawIron.Structural/StructuralCompiler`

## What Landed

RawIron now has native support for:

- deferred target-operation collection during structural compile
- normalized deferred operation output for:
  - terrain hole cutouts
  - spline mesh deformers
  - spline decal ribbons
  - surface scatter volumes
  - scatter-surface aliases
  - shrinkwrap modifiers
- alias normalization from `scatter_surface_primitive` to `surface_scatter_volume`
- keeping these deferred operations out of normal passthrough structural shell output

The structural compile result now carries a native `deferredOperations` list so later scene/load/runtime assembly can apply those post-build operations honestly.

## Why It Matters

The prototype has a real split between:

- structural shell compile
- post-build target-driven operations

That split matters because things like terrain cutouts and spline deformers are not just “weird geometry.”
They are operations over already-existing target meshes or surfaces.

With this pass, RawIron now reflects that architecture natively instead of flattening everything into one geometry bucket.

## Honest Boundary

This is a foundation pass for deferred operations.

RawIron still needs later work for:

- actual scene/load/runtime application of these deferred operations
- deeper data normalization for spline paths and richer target settings
- more deferred helper families from the prototype

So this pass ports the output seam first, which is the right engine move before execution wiring.

## Tests

The import/runtime suite now verifies:

- deferred operation collection for terrain, spline, scatter, and shrinkwrap helpers
- preservation of target IDs in deferred compile output
- alias normalization for scatter-surface helpers
- exclusion of deferred operations from normal passthrough structural geometry

## Result

RawIron now owns the prototype's post-build target-operation seam in native `C++`, which gives the structural compiler a cleaner handoff into future scene/runtime assembly.
