---
tags:
  - rawiron
  - migration
  - pass-28
  - structural
  - boolean
---

# Pass 28 Structural Boolean Operator Foundation

## Goal

Port the prototype's native structural boolean operator logic into RawIron so authored `union`, `intersection`, and `difference` flows stop living only in `index.js`.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\engine\structuralPrimitives\compiler.js`

This pass focuses on the compile-time boolean operator layer above the convex clipper and primitive builders.

## Native Landing Zone

- `Source/RawIron.Structural/StructuralCompiler`

## What Landed

RawIron now has native boolean-operator compile helpers for:

- `boolean_union`
- `boolean_intersection`
- `boolean_difference`

The native structural compiler now owns:

- boolean target-ID resolution from `targetIds` and `childNodeList`
- supported additive-target filtering
- union fragment compilation that removes overlapping duplicate coverage from later targets
- intersection fragment compilation across convex target solids
- difference fragment compilation across convex target solids
- compiled fragment node emission from the resulting solids

## Why It Matters

This is the first real bridge from:

- native primitive generation

to:

- native structural authoring operations

That matters because the prototype engine is not just a list of primitive shapes. It is also a way of combining, suppressing, and refining those shapes into actual authored structural output.

With this pass, RawIron's structural compiler now owns the first real boolean authoring behavior instead of leaving that compile logic trapped in the prototype app layer.

## Honest Boundary

This is still a **foundation** pass for structural operators.

RawIron does not yet own the full higher-level prototype compile pipeline around:

- broader operator orchestration
- authoring modifiers above boolean flow
- richer scene/runtime assembly from authored structural nodes

So this pass is the native boolean core, not the entire authoring stack.

## Tests

The import/runtime suite now verifies:

- boolean target-ID resolution
- supported additive-target detection
- union fragment compilation and aggregate coverage bounds
- intersection fragment compilation and overlap-bounds correctness
- difference fragment compilation and surviving left/right slab coverage

## Result

RawIron now owns the prototype's first real structural boolean operator layer in native `C++`, which is a meaningful step toward honest structural compile parity instead of only primitive parity.
