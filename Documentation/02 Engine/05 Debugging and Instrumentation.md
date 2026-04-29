---
tags:
  - rawiron
  - engine
  - debugging
  - instrumentation
  - cpp
---

# Debugging and Instrumentation

## Purpose

RawIron already has enough moving parts that debugging has to be treated as a first-class engine system.

The engine should be able to answer questions like:

- what just fired
- what world state changed
- what the runtime thinks is active here
- how spatial queries are behaving

without forcing every investigation into ad hoc logging edits.

## Helper Telemetry Bus (concept)

Raw Iron documents a single umbrella model for **development-facing** helper observability—the **[[Helper Telemetry Bus]]**: summarized helper activity, metrics snapshots, `RuntimeEventBus` channels, entity-I/O and spatial trackers, and debug snapshot formatting. It is **not** part of core simulation; it exists so tools and overlays read engine-owned state instead of inventing parallel probes.

## Main Native Surfaces

RawIron already has meaningful native instrumentation layers.

### `RawIron.Debug`

Current responsibilities:

- helper-library metric aggregation
- event-engine world-state snapshots
- spatial-index summary snapshots
- compact state formatting
- full debug-report formatting

### `RawIron.World`

Current runtime instrumentation:

- helper activity tracking
- entity-I/O counters and capped recent history
- spatial-query counters
- stats-overlay state
- environment-state summaries

### Supporting Metric Sources

Other native libraries already feed the debug story:

- `RawIron.Runtime` for event-bus metrics
- `RawIron.Validation` for schema metrics
- `RawIron.Audio` for managed audio metrics
- `RawIron.Structural` for graph health summaries

## Current Report Shape

The native debug-report path can already summarize:

- world flags
- world values
- scheduled timer counts
- event runtime state counts
- spatial index summaries
- helper metrics
- active post-process labels
- active audio-environment labels

That means the native engine already has the beginnings of a trustworthy runtime report, not just raw counters.

## Tooling Surfaces Today

Before the full editor lands, RawIron already exposes useful verification entry points:

- `ri_tool --workspace`
- `ri_tool --sample-scene`
- `ri_tool --scenekit-targets`
- `ri_tool --vulkan-diagnostics`
- CTest under MSVC and Clang

These are not replacements for a future in-engine console or overlay, but they give the native runtime a real inspection path today.

## Native Direction

The future player and editor should expose debugging through the same engine-owned state instead of inventing parallel UI-only logic.

That means future native surfaces should read from the same runtime services for:

- helper activity
- world state
- spatial stats
- environment state
- structural graph health
- event and timer summaries

## Recommended Workflow

1. Reproduce the issue in a dev scene, sandbox project, or focused test case.
2. Check the native tests first if the issue smells foundational.
3. Read runtime snapshots and helper metrics before adding new logging.
4. Compare spatial and environment state if the issue is movement- or volume-related.
5. Compare event and timer state if the issue is sequencing-related.
6. Prefer engine-owned probes over shell-specific hacks.

## Why This Matters

Instrumentation is how RawIron stays understandable while the port gets deeper.

If the engine cannot explain itself, every new subsystem becomes slower to trust, slower to review, and slower to evolve.

## Related Notes

- [[Helper Telemetry Bus]]
- [[00 Engine Home]]
- [[01 Runtime Flow]]
- [[02 World Systems]]
- [[03 Event Engine]]
- [[Automated Review and Scripted Camera]]
