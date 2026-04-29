---
tags:
  - rawiron
  - migration
  - pass-01
---

# Pass 01 Runtime and Structural Foundation

## Goal

Port the most isolated engine systems out of `Q:\anomalous-echo` first so RawIron gains native equivalents for prototype-engine foundations without depending on game code.

## What Landed

### `RawIron.Runtime`

- `RuntimeId`
  - native prefix sanitization
  - native runtime ID generation with ten-character suffixes
- `RuntimeEventBus`
  - typed listener registration and removal
  - event emission
  - prototype-style bus metrics

### `RawIron.Structural`

- `StructuralGraph`
  - structural phase classification
  - dependency extraction
  - dependency ordering
  - unresolved dependency reporting
  - cycle reporting
  - phase bucket summaries
- `ConvexClipper`
  - point-to-plane classification
  - plane construction
  - axis-aligned box solid generation
  - polygon clipping
  - solid clipping
  - convex-solid mesh compilation
- `StructuralCompiler`
  - solid bounds
  - solid transforms
  - world-space box generation
  - convex plane extraction from triangles
  - convex subtraction and intersection helpers
  - compiled fragment node generation

## RawIron Files Added

- `Source/RawIron.Runtime`
- `Source/RawIron.Structural`
- `Tests/RawIron.EngineImport.Tests`

## Supporting Core Changes

- `RawIron.Math.Vec3`
  - division
  - cross product
  - distance helpers
  - lerp
- `RawIron.Math.Mat4`
  - point transform helper

## Verification

Pass 01 is verified under both toolchains.

- `MSVC`: `10/10` tests passed
- `Clang`: `10/10` tests passed

New native test target:

- `RawIron.EngineImport.Tests`

That target covers:

- runtime ID sanitization and suffix shape
- runtime event bus metrics and listener behavior
- structural dependency ordering
- unresolved dependency reporting
- cycle reporting
- convex clipping of an axis-aligned box
- solid bounds and transforms
- convex plane extraction from compiled triangles
- compiled fragment node generation

## Why This Pass Matters

This is the first time RawIron contains native equivalents for prototype-engine systems that were not just scene conveniences.

It gives the engine a real place to absorb more of the prototype in future passes without forcing everything into `RawIron.Core`.

## Next Pass Targets

1. `runtimeSchemas.js`
2. event engine flow from the prototype runtime docs and app runtime
3. world-query and spatial-index systems
4. audio manager concepts
