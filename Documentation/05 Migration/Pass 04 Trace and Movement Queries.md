---
tags:
  - rawiron
  - migration
  - pass-04
  - trace
---

# Pass 04 Trace and Movement Queries

## Goal

Port the prototype's shared trace helpers on top of the spatial broadphase so RawIron has a reusable native layer for collision tests, swept movement, and ground probing.

## Prototype Source

- `Q:\anomalous-echo\index.js`
  - `traceBox`
  - `traceRay`
  - `traceSweptBox`
  - `slideMoveBox`
  - `findGroundHit`
- supporting world-query direction from `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

## What Landed

### `RawIron.Trace`

- trace-scene collider model
- static vs dynamic collider routing
- structural-only query mode
- overlap box tracing
- nearest-hit ray tracing
- swept AABB tracing
- slide-move resolution for boxed movement
- downward ground-hit probing

## Engine Boundary

This pass keeps the generic query math and movement helpers in RawIron while leaving game-specific interpretation outside the library.

That means RawIron now owns:

- broadphase routing
- hit selection
- hit normal and contact point estimation
- movement clipping and slide resolution

But it does not hardcode:

- player logic
- enemy logic
- AI sight rules
- physics-prop side effects
- trigger side effects

## Verification

The migration suite now verifies:

- trace-scene candidate queries
- structural-only filtering
- overlap tracing
- nearest ray hits
- swept box impact timing
- slide motion against blocking walls
- downward ground-hit queries
- ignore-id filtering

Toolchain status:

- `MSVC`: pass
- `Clang`: pass

## Why This Pass Matters

The prototype world runtime treats tracing as a shared service rather than an app-local trick.

That is the right engine instinct.

By landing trace helpers as a native library now, RawIron has a stronger foundation for:

- movement controllers
- collision-driven editor tools
- interaction tests
- spawn validation
- later world/runtime systems

## Best Next Steps

1. port runtime schemas and validation
2. connect trace helpers to a future world/runtime layer
3. port audio manager concepts into a native audio layer
