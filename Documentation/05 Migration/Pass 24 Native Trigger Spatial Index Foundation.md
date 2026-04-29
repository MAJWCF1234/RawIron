---
tags:
  - rawiron
  - migration
  - pass-24
  - world
  - triggers
  - spatial
---

# Pass 24 Native Trigger Spatial Index Foundation

## Goal

Pull the prototype's trigger-index rebuild and point-query path into native `C++` so RawIron stops doing full trigger-family scans every update.

## Prototype Reference

The source reference for this pass lives in:

- `Q:\anomalous-echo\index.js`

Important prototype behaviors:

- `rebuildTriggerVolumeIndex()`
- `queryTriggerVolumesForPoint(point)`
- trigger spatial stats:
  - `triggerIndexBuilds`
  - `lastTriggerRebuildMs`
  - `triggerPointQueries`
  - `triggerCandidates`

## Native Landing Zone

- `Source/RawIron.World/RuntimeState`
- `Source/RawIron.World/Instrumentation`
- `Source/RawIron.Spatial/SpatialIndex`

## What Landed

RawIron now has a native trigger-index/query seam:

- lazy trigger-index rebuilds
- BSP-backed trigger candidate queries
- trigger query padding around a point
- trigger point-query stats routed into `SpatialQueryTracker`
- trigger-index rebuild timing routed into `SpatialQueryTracker`
- trigger runtime updates that evaluate:
  - indexed candidates
  - already-active trigger volumes for clean exits

## Why It Matters

This is both a fidelity pass and a performance pass.

The prototype did not brute-force every trigger every update once the trigger index existed.

RawIron now follows the same shape:

- rebuild the trigger index when trigger-volume families change
- query a tiny box around the player point
- only evaluate candidate volumes plus already-active volumes

That keeps the native engine closer to the prototype's real runtime behavior and less like a test harness.

## Tests

The import/runtime suite now verifies:

- trigger index entry counts
- trigger index build count
- trigger point-query count
- trigger candidate accumulation
- index reuse across repeated trigger updates
- exit handling when no new indexed candidates are present

## Result

`RawIron.World` now owns more of the prototype's mixed trigger/world runtime logic natively, and it does it with a real spatial-query path instead of a blanket scan.
