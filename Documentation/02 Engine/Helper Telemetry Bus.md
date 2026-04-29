---
tags:
  - rawiron
  - engine
  - debugging
  - instrumentation
  - telemetry
  - helpers
---

# Helper Telemetry Bus

## Overview

The **Helper Telemetry Bus** is an optional, **development-facing** telemetry and observability umbrella: it gathers, summarizes, and exposes **runtime helper activity**, **helper-library state**, and **related diagnostic metrics** for debugging, automation, and inspection tools.

Its purpose is to surface **structured** information about support systems that wrap the runtime so developers can monitor what helper subsystems are doing **without** embedding that visibility directly into simulation or gameplay code paths.

This concept belongs to the **tooling and diagnostics layer**, not to core simulation behavior.

## Purpose

As Raw Iron grows, more helper systems participate in loading, orchestration, runtime services, instrumentation, debugging, and tool-facing inspection. That work often happens behind the scenes. Without a **named, unified mental model** and **documented attachment points**, it is easy to scatter ad hoc logging or duplicate inspection logic.

The Helper Telemetry Bus names that unified path: helper-related **events**, **counters**, **snapshots**, and **summaries** should flow through the same observable surfaces so panels, text snapshots, dashboards, and automated review tools can subscribe once and stay aligned with engine-owned state.

## Core function

Depending on configuration and host wiring, “the bus” may surface:

| Kind of signal | Examples in Raw Iron today |
|----------------|---------------------------|
| Helper activity events | Last-noticed audio, state diff, trigger, entity I/O, message, level load, schema validation (`HelperActivityState`; listeners on `RuntimeEventBus`) |
| Summarized helper activity | `SummarizeHelperActivity`, `BuildRuntimeHelperMetricsSnapshot` |
| Helper library counts / metrics | `RuntimeHelperMetricsSnapshot` (schema, tuning, audio, event bus, volumes, structural graph, overlay FPS, …) |
| Observer-style notifications | `RuntimeEventBus` channels (`audioChanged`, `stateChanged`, `triggerChanged`, `entityIo`, …) |
| Info-panel metric bindings | `InfoPanelBinding`, `InfoPanelBindingContext`, `ResolveInfoPanelLines` |
| Entity I/O instrumentation | `EntityIoTracker`, `entityIo` events |
| Spatial instrumentation | `SpatialQueryTracker` |
| Text-oriented diagnostics | Native debug report / snapshot formatting (`RawIron.Debug`) |

Together these answer questions such as:

- Which helper-facing channels have been active recently?
- What summarized strings and counters should appear in an overlay or snapshot?
- How many helper-adjacent actions occurred during an interval (via metrics snapshots and bus metrics)?
- Are runtime observers wired (listeners receiving expected event types)?

## Role in Raw Iron

Treat the Helper Telemetry Bus as **development observability and automation support**. It exposes **helper-side** diagnostics for inspection and testing; it must **not** replace authoritative gameplay state or drive simulation decisions.

It is especially suited to:

- Developer info panels and HUD-adjacent overlays
- Runtime diagnostics overlays (e.g. stats overlay metrics)
- Text-based debug snapshots and automated review tooling
- Helper-library and instrumentation inspection
- Validation that observer wiring matches expectations
- Tooling that needs **summarized** state rather than raw engine internals

In short: it supports understanding the **environment around** the runtime—the instrumentation shell—not the simulation’s core logic loop.

## Native attachment points (today)

These types and functions are the concrete “bus segments” hosts connect today:

| Area | Header / symbols |
|------|------------------|
| Event transport | `RawIron/Runtime/RuntimeEventBus.h` — `RuntimeEventBus::Emit` / `On` |
| Activity listeners | `RawIron/World/Instrumentation.h` — `HelperActivityTracker`, `HelperActivityState` (entity I/O field keys align with `ri::runtime::entity_io::kField*`) |
| Metrics snapshot | `RawIron/World/RuntimeState.h` — `RuntimeHelperMetricsSnapshot`, `BuildRuntimeHelperMetricsSnapshot` |
| Activity string shaping | `RawIron/World/HelperActivitySummary.h` — `SummarizeHelperActivity` |
| Entity I/O | `RawIron/Runtime/EntityIoTelemetry.h`, `RawIron/World/LogicEntityIoTelemetry.h`, `EntityIoTracker` |
| Spatial stats | `SpatialQueryTracker` in `Instrumentation.h` |
| Debug reports | `RawIron/Debug` — snapshot and report formatters |

Optional umbrella include for discoverability (comments only): `RawIron/World/HelperTelemetryBus.h`.

## Related Notes

- [[05 Debugging and Instrumentation]]
- [[Library Layers]]
- [[02 World Systems]]
- [[Entity IO and Logic Graph]]
- [[00 Engine Home]]
