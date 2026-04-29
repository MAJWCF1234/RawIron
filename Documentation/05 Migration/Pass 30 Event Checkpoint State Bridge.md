---
tags:
  - rawiron
  - migration
  - events
  - runtime
---

# Pass 30 Event Checkpoint State Bridge

## Goal

Port prototype checkpoint world-state behavior from `Q:\anomalous-echo\index.js` into native RawIron event runtime services.

This pass stays engine-only and keeps the prototype as read-only reference.

## Native Landing Zone

Expanded native files:

- `Source/RawIron.Events/include/RawIron/Events/EventEngine.h`
- `Source/RawIron.Events/src/EventEngine.cpp`
- `Tests/RawIron.EngineImport.Tests/src/main.cpp`
- `Documentation/05 Migration/Prototype Engine Audit.md`

## What Landed

### Native Checkpoint Snapshot Type

`RawIron.Events` now has `EventCheckpointState` with:

- world flags
- world numeric values
- completed event IDs

### Native Capture and Restore API

`EventEngine` now exposes:

- `CaptureCheckpointState()`
- `RestoreCheckpointState(...)`
- `ResetWorldState()`
- `GetCompletedEventIds()`

These APIs move checkpoint-relevant world/event state out of mixed prototype runtime flow and into native engine logic.

### Runtime Safety During Restore

Restore/reset now also clears scheduled timer state so stale delayed actions do not leak across checkpoint transitions.

## Why This Matters

The prototype tracks checkpoint world state (`flags`, `values`, `eventIds`) in app runtime code.

RawIron now owns this as native event-engine behavior, which is the right seam for player/editor/runtime integration.

## Testing

`RawIron.EngineImport.Tests` now verifies:

- checkpoint capture includes world flags, world values, and completed event IDs
- world-state reset clears those collections
- restore rehydrates those collections
- restored world flags affect subsequent event-condition evaluation

## Result

RawIron now has a stronger native event-runtime persistence seam for checkpoint-like flows, independent of game-specific code.
