---
tags:
  - rawiron
  - migration
  - debug
---

# Pass 07 Debug Snapshot Foundation

## Prototype Sources

- `Q:\anomalous-echo\engine\debug\runtimeSnapshots.js`
- `Q:\anomalous-echo\obsidian\Engine\00 Engine Home.md`
- `Q:\anomalous-echo\obsidian\Engine\01 Runtime Flow.md`
- `Q:\anomalous-echo\obsidian\Engine\05 Debugging and Instrumentation.md`

## Goal

Lift the engine-side snapshot and report behavior into RawIron without dragging the prototype's game-specific actor, inventory, or campaign state into the engine runtime.

## Native Landing Zone

- `Source/RawIron.Debug`

## What Landed

- helper-library metrics aggregation for:
  - schema validation
  - runtime event bus
  - managed audio
  - recent helper activity labels
- event-engine world-state snapshots:
  - authored event count
  - runtime-state count
  - scheduled timer count
  - world flags
  - world values
- spatial-index summary snapshots:
  - entry counts
  - non-empty bounds
- compact machine-readable state formatting
- full native debug-report formatting

Key files:

- `N:\RAWIRON\Source\RawIron.Debug\include\RawIron\Debug\RuntimeSnapshots.h`
- `N:\RAWIRON\Source\RawIron.Debug\src\RuntimeSnapshots.cpp`

Supporting engine accessors added in:

- `N:\RAWIRON\Source\RawIron.Events\include\RawIron\Events\EventEngine.h`
- `N:\RAWIRON\Source\RawIron.Events\src\EventEngine.cpp`

## What Stayed Out On Purpose

The prototype module also included game-facing state such as:

- player movement state
- enemy and NPC snapshots
- inventory details
- campaign-specific objective and asset status reporting

Those pieces are not engine-only, so they were intentionally left out of this pass.

## Why This Pass Matters

The prototype already proved that debugging needs to be a real system, not a scattered pile of `console.log` edits.

RawIron now has the start of that same native instrumentation spine:

- engine subsystems can publish stable metrics
- world-state can be summarized without UI code
- reports can be tested as engine behavior

## Verification

`RawIron.EngineImport.Tests` now covers:

- helper-library metric aggregation
- event-engine debug-state capture
- spatial-index summary capture
- compact state snapshot formatting
- full debug-report formatting

Verified under:

- MSVC
- Clang

## Notes

This is a foundation pass.

The next instrumentation pass should hook these snapshot/report utilities into a broader engine-world layer once more live runtime counters and environment systems have been migrated.
