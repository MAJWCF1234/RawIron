---
tags:
  - rawiron
  - migration
  - pass-03
  - spatial
---

# Pass 03 Spatial Query Foundation

## Goal

Port the prototype broadphase spatial index out of the web runtime so RawIron has a native foundation for collision, interaction, trigger, and later trace systems.

## Prototype Source

- `Q:\anomalous-echo\spatialIndex.js`
- supporting world-query direction from `Q:\anomalous-echo\obsidian\Engine\02 World Systems.md`

## What Landed

### `RawIron.Spatial`

- axis-aligned bounds type
- ray type
- box/box intersection
- ray/AABB intersection
- segment bounds construction
- BSP-style spatial index with:
  - configurable leaf size
  - configurable max depth
  - box queries
  - ray queries
  - duplicate suppression
  - rebuild support

## Why This Pass Matters

The prototype world model explicitly separates:

- static world data suited for BSP-style broadphase indexing
- dynamic runtime data kept in live arrays

That split is engine architecture, not game logic.

By landing the broadphase layer in RawIron now, future ports like trace helpers, trigger queries, interaction candidates, and structural-only collision paths have a native place to attach.

## Verification

The migration suite now covers:

- spatial index build filtering for invalid entries
- aggregate world bounds
- box queries across BSP branches
- ray queries with far-distance limits
- vertical ray hits against tall volumes
- invalid ray rejection
- rebuild behavior

Toolchain status:

- `MSVC`: pass
- `Clang`: pass

## Next Steps From Here

1. port shared trace helpers on top of `RawIron.Spatial`
2. port structural-vs-dynamic query routing from the prototype world runtime
3. connect the future world layer to this broadphase foundation
