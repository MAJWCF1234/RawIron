---
tags:
  - rawiron
  - migration
  - validation
  - checkpoint
---

# Pass 31 Checkpoint Schema Validation Foundation

## Goal

Port checkpoint payload shape validation and sanitization from prototype runtime flow into native RawIron validation services.

This pass is engine-only. `Q:\anomalous-echo` stayed read-only.

## Native Landing Zone

Expanded native files:

- `Source/RawIron.Validation/include/RawIron/Validation/Schemas.h`
- `Source/RawIron.Validation/src/Schemas.cpp`
- `Tests/RawIron.EngineImport.Tests/src/main.cpp`
- `Documentation/05 Migration/Prototype Engine Audit.md`

## What Landed

### Native Checkpoint Schema Type

Added `RuntimeCheckpointState` in `RawIron.Validation` with engine-relevant checkpoint fields:

- `level`
- `checkpointId`
- `flags`
- `values`
- `eventIds`

### Native Parse/Sanitize API

Added:

- `ParseStoredCheckpointState(...)`

It now:

- clears empty optional IDs
- drops empty flags/event IDs
- deduplicates flags/event IDs while preserving first-seen order
- drops unnamed/non-finite world values

### Native Validation API

Added:

- `ValidateCheckpointState(...)`

It enforces:

- non-empty `level`
- non-empty flag/event ID entries
- finite named world values

## Why This Matters

Prototype checkpoint shape checks lived in `index.js` and were mixed with app runtime code.

RawIron now has a native validation seam for checkpoint world/event state payloads, which keeps persistence rules in engine libraries.

## Testing

`RawIron.EngineImport.Tests` now verifies:

- invalid checkpoint payloads are rejected
- checkpoint parse/sanitize behavior for empty/duplicate entries
- finite-value filtering for world value maps
- sanitized payloads pass validation

## Result

Checkpoint payload contracts are now native and reusable across future player/editor runtime flows without JS-only validation glue.
