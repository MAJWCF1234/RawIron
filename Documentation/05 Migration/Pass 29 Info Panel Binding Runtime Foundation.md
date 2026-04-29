---
tags:
  - rawiron
  - migration
  - world
  - instrumentation
---

# Pass 29 Info Panel Binding Runtime Foundation

## Goal

Port prototype info-panel runtime binding logic from `Q:\anomalous-echo\index.js` into native RawIron runtime helpers.

This pass keeps the prototype as read-only reference and lands engine-only behavior in RawIron.

## Native Landing Zone

Expanded native files:

- `Source/RawIron.World/include/RawIron/World/Instrumentation.h`
- `Source/RawIron.World/src/Instrumentation.cpp`
- `Tests/RawIron.EngineImport.Tests/src/main.cpp`
- `Documentation/05 Migration/Prototype Engine Audit.md`

## What Landed

### Native Info-Panel Binding Types

`RawIron.World` instrumentation now has native types for info-panel value binding:

- `InfoPanelValue`
- `InfoPanelBinding`
- `InfoPanelDefinition`
- `InfoPanelBindingContext`
- `RuntimeInfoPanelCounts`

### Native Binding Resolution

RawIron now resolves binding values in C++ for engine runtime state:

- logic-entity property bindings with prototype-like boolean conversion (`ON`/`OFF`)
- world-value bindings with finite-number handling and fallbacks
- world-flag bindings (`SET`/`CLEAR`)
- runtime metric bindings for counters and helper metrics

### Native Line Composition

RawIron now composes info-panel lines in native code:

- static lines + bound lines
- optional replace mode
- prototype-style uppercased labels

## Why This Matters

Prototype info-panel behavior lived inside mixed JS runtime flow.

This pass moves the binding-resolution seam into engine-side C++, which makes it reusable by native player/editor/runtime shells without JS glue.

## Testing

`RawIron.EngineImport.Tests` now verifies:

- logic-entity bool binding conversion
- world flag and world value binding behavior
- helper-metric and runtime-count metric bindings
- line composition with uppercased labels and append behavior

## Result

RawIron now owns another engine-side runtime helper path that was previously mixed into prototype application code.

This pass is strictly engine migration work, with no game-content dependency.
