---
tags:
  - rawiron
  - migration
  - instrumentation
---

# Pass 09 Runtime Instrumentation Foundation

## Goal

Pull the prototype's observer-driven helper activity and lightweight runtime counter wiring into RawIron as engine-owned C++ systems instead of leaving them buried in app glue.

## Prototype Inputs

- `Q:\anomalous-echo\index.js`
- `Q:\anomalous-echo\engine\dev\RuntimeStatsOverlay.js`

This pass focused on the runtime observer and instrumentation paths around:

- helper activity labels
- entity-I/O counters and history
- spatial-query counters
- runtime stats-overlay attachment and visibility state

## RawIron Targets

- `Source/RawIron.World/include/RawIron/World/Instrumentation.h`
- `Source/RawIron.World/src/Instrumentation.cpp`

## What Landed

### `HelperActivityTracker`

Engine-owned event-bus observers now capture the same kind of helper activity the prototype tracked in its runtime shell:

- `audioChanged`
- `stateChanged`
- `triggerChanged`
- `entityIo`
- `message`
- `levelLoaded`
- `schemaValidated`

The tracked state stays normalized through the existing `SummarizeHelperActivity` helper so the labels stay stable and compact.

### `EntityIoTracker`

RawIron now has a native tracker for entity-I/O activity:

- outputs fired
- inputs dispatched
- timers started
- timers cancelled
- counters changed
- newest-first capped event history

The history cap mirrors the prototype helper behavior instead of growing without bound.

### `SpatialQueryTracker`

RawIron now tracks lightweight spatial instrumentation separate from the actual query systems:

- collision-index rebuild count and last rebuild time
- interaction-index rebuild count and last rebuild time
- trigger-index rebuild count and last rebuild time
- box, ray, interaction, and trigger query counts
- accumulated candidate totals

This keeps the broadphase and trace systems clean while still giving the future editor and debug surfaces something truthful to read.

### `RuntimeStatsOverlayState`

RawIron now mirrors the prototype overlay state in a small engine-owned form:

- enabled
- attached
- visible

That gives future player/editor UI work a stable native state surface before there is a real polished overlay.

## Tests

`RawIron.EngineImport.Tests` now covers:

- helper-activity observer updates from runtime-bus events
- detach/reset behavior for helper-activity tracking
- entity-I/O counters and capped history ordering
- spatial-query rebuild/query counters
- runtime-stats-overlay state flags

Verified on:

- MSVC
- Clang

## Why This Pass Matters

This is one of the passes where "app glue" starts becoming actual engine infrastructure.

The prototype used these helpers to stay understandable while a lot of mixed systems were moving at once. Pulling them into RawIron means the native engine now owns that runtime introspection story instead of treating it like temporary scaffolding.

## What This Pass Did Not Do

This pass did not port:

- game-specific overlay widgets
- player- or encounter-specific counters
- mission/objective instrumentation
- browser-facing dev UI

Those still belong above the engine boundary.

## Next Likely Pass

Keep extracting the remaining mixed world/runtime behavior out of `Q:\anomalous-echo\index.js` and attach these new instrumentation helpers to broader engine-owned world services as they land.
