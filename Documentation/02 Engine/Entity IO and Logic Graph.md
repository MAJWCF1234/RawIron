---
tags:
  - rawiron
  - engine
  - runtime
  - logic
  - entities
  - telemetry
---

# Entity IO and Logic Graph Wiring

## Problem this solves

Logic entities (relays, timers, counters, routed edges, world actors) execute inside `ri::logic::LogicGraph`, while **debug HUDs**, **session snapshots**, and **host tooling** listen on `ri::runtime::RuntimeEventBus`. Historically the bus carried an `entityIo` channel, but **nothing in `LogicGraph` emitted to it**, so developers had to rediscover field names and wire handlers by hand.

This note documents the **shared contract** and the **`AttachLogicGraphEntityIoTelemetry`** bridge that connects graph execution to the runtime bus **without** replacing your gameplay `SetOutputHandler` callback.

## Runtime channel: `entityIo`

| Item | Value |
|------|-------|
| Event type string | `"entityIo"` (`ri::runtime::entity_io::kEventType`) |

Emit through `ri::runtime::entity_io::EmitEntityIo` so payloads stay aligned with instrumentation.

### Standard string fields (`RuntimeEvent.fields`)

| Field key (`entity_io::…`) | When set | Purpose |
|---------------------------|----------|---------|
| `kFieldKind` | Always | `"output"` or `"input"` for logic telemetry (`EmitEntityIoForLogicOutput` / `EmitEntityIoForLogicInput`). |
| `kFieldSourceId` | Outputs; optional on routed inputs | Emitting node id (`LogicOutputEvent::sourceId`) or upstream hint on input events. |
| `kFieldTargetId` | Inputs | Destination node id. |
| `kFieldOutputName` | Outputs | Normalized output port name. |
| `kFieldInputName` | Inputs | Normalized input port name. |
| `kFieldInstigatorId` | Optional | From `LogicContext` when present. |
| `kFieldParameter` | Optional | String form of `LogicContext::parameter`. |
| `kFieldAnalogSignal` | Optional | String form of `LogicContext::analogSignal`. |

Custom hosts may add more keys; `HelperActivityTracker` summarizes using `sourceId` / `targetId`, `inputName` / `outputName`, then falls back to `kind` (see `Instrumentation.cpp`).

## Bridging `LogicGraph` → `RuntimeEventBus`

1. Construct `LogicGraph` and **install your gameplay output handler** inside the bridge call (the last argument).
2. Optional: pass `EntityIoTracker` to accumulate stats and ring history for tooling.

```cpp
#include "RawIron/World/LogicEntityIoTelemetry.h"

ri::logic::LogicGraph graph(spec);
ri::runtime::RuntimeEventBus bus;
ri::world::EntityIoTracker ioTracker;

ri::world::AttachLogicGraphEntityIoTelemetry(
    graph,
    bus,
    &ioTracker,
    [&](const ri::logic::LogicOutputEvent& ev) {
        /* existing fan-out: audio, movers, scripted events, … */
    });
```

The bridge:

- Wraps your output handler and emits **one** `entityIo` row per logic output (`EmitEntityIoForLogicOutput`).
- Registers `LogicGraph::SetInputDispatchHandler` so **every** `DispatchInput` (including delayed deliveries) produces a matching input event (`EmitEntityIoForLogicInput`).

Call it **once** during setup: it replaces the graph’s output handler and input-dispatch hook. If you need multiple consumers, fan out inside your handler or attach additional bus listeners—do not call `SetOutputHandler` afterward without also re-invoking the bridge.

## Counters and history

`EntityIoTracker` tracks aggregate counts (`outputsFired`, `inputsDispatched`, …). The logic bridge increments those counts per emit. Use `EntityIoTracker::RecordEvent` directly when you need ad-hoc rows for custom tooling.

## How this relates to triggers and volumes

- Bus events such as `triggerChanged` describe **volume overlap** transitions.
- `entityIo` describes **logic graph activations** (routes, ports, payloads).

A pressure plate may emit `triggerChanged` when stepped on **and** later show `entityIo` when the relay→door chain runs; both are complementary.

## Tests and source references

- Contract: `Source/RawIron.Runtime/include/RawIron/Runtime/EntityIoTelemetry.h`
- Bridge: `Source/RawIron.World/include/RawIron/World/LogicEntityIoTelemetry.h`, `Source/RawIron.World/src/LogicEntityIoTelemetry.cpp`
- Engine test: `Tests/RawIron.EngineImport.Tests/src/EngineImportTests.cpp` (`TestLogicEntityIoTelemetry`)
- Helper activity listener: `Tests/RawIron.EngineImport.Tests/src/EngineImportTests.cpp` (`TestSummarizeHelperActivity` and related world instrumentation coverage)

## Related Notes

- [[NPC Behavior Support]]
- [[02 World Systems]]
- [[Library Layers]]
- [[00 Engine Home]]
