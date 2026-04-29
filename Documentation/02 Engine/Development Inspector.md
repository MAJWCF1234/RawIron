---
tags:
  - rawiron
  - debugging
  - tooling
---

# Development Inspector / Live Debugging Channel

## Role

Optional **developer observability** layer that sits **beside** the runtime—not inside render, physics, ECS, or serialization. It aggregates **structured introspection** (often JSON text) so **external tools** can observe a running player, simulation, editor session, or headless harness.

CMake target: **`RawIron::DevInspector`** (`option RAWIRON_BUILD_DEV_INSPECTOR`, **ON** by default). Consumers define **`RAWIRON_WITH_DEV_INSPECTOR`** when linking the library.

## Core API (initial)

- **`ri::dev::DevelopmentInspector`** — registers named **snapshot sources** (`std::string()` → payload), queues **diagnostic stream** messages by **`InspectorChannel`**, optional **`IInspectorTransport`** (NDJSON-style lines on **`Pump()`**), and **prefixed development commands** when **`InspectorConfig::allowDevelopmentCommands`** is true.
- **`InspectorConfig::maxBufferedDiagnostics`** — bounded queue (default **8192**, **`0`** = use default). Oldest rows drop first when full; **`DiagnosticsDroppedCount()`** tracks losses.
- **Schema v2** — snapshots: `{"version":2,"seq":N,"sources":{...}}` ; NDJSON from **`Pump`**: `{"version":2,"channel":n,"seq":N,"msg":"..."}` .
- Snapshot callbacks that **throw** are converted into a small JSON **`{"error":"..."}`** payload for that key (other sources still run).
- **`SnapshotSourceIds()`**, **`ClearDiagnosticQueue()`** — introspection and manual flush without sending.
- Move-only instances (**non-copyable**); each inspector owns a **mutex** (no global lock).
- **`ri::dev::NullInspectorTransport`**, **`ri::dev::CallbackInspectorTransport`** — built-in sinks.
- **`TryHandleCommand`** — first token is the command name; if the remainder contains **`{`**, the substring from the first `{` is **`argsJson`** (supports `cmd {"x":1}`).
- Transports such as TCP, HTTP, WebSocket, or stdin/stdout JSON-RPC can be implemented **on top of** `IInspectorTransport` without pulling them into core.

## Intended capabilities (roadmap)

The implementation is expected to grow into:

- **Runtime telemetry** — frame timing, counters, memory hints, subsystem health.
- **World / scene inspection** — entity lists, components, hierarchy (hosts register snapshot providers).
- **Debug visualization feeds** — physics, volumes, frustums, traces (payloads from host systems).
- **Event / log streams** — warnings, traces, reload notifications (`PostDiagnostic` + transport).
- **Controlled commands** — toggles, reloads, selection, pause/step (`TryHandleCommand`), **off by default** in shipping configs via **`allowDevelopmentCommands`**.

## What it is not

Not a replacement for the renderer, game loop, physics, save format, editor document, or ECS—only a **side channel** for humans and tools.

## Summary

A **lightweight, optional** library for **live state and diagnostics** during development and automation, keeping heavy debug UI out of shipping runtimes while staying **transport- and language-agnostic** on the wire.

## Related Notes

- [[Automated Review and Scripted Camera]] — complementary *intent* for scripted scene traversal and review automation (orthogonal to the inspector wire protocol; useful when correlating scripted passes with streamed diagnostics).
