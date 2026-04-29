---
tags:
  - rawiron
  - migration
  - validation
---

# Pass 05 Schema and Validation Foundation

## Prototype Sources

- `Q:\anomalous-echo\engine\runtimeSchemas.js`
- `Q:\anomalous-echo\obsidian\Engine\00 Engine Home.md`
- `Q:\anomalous-echo\obsidian\Engine\01 Runtime Flow.md`

## Goal

Lift the prototype's schema and validation behavior into a native library so RawIron can own authored-data contracts without relying on browser-side helper code.

## Native Landing Zone

- `Source/RawIron.Validation`

## What Landed

- `RuntimeTuning` sanitization for stored runtime values
- `LevelPayload` and `WorldspawnDefinition` contracts for engine-owned level validation
- worldspawn output/input validation
- event/action/sequence validation hooks layered over `RawIron.Events`
- validation metrics mirroring the prototype helper counters

Key files:

- `N:\RAWIRON\Source\RawIron.Validation\include\RawIron\Validation\Schemas.h`
- `N:\RAWIRON\Source\RawIron.Validation\src\Schemas.cpp`

## Why This Pass Matters

The prototype used `zod` as a helper layer to stop bad data before the runtime consumed it.

RawIron needs the same discipline in native code.

This pass means engine-owned tuning values and level payloads can now be checked inside RawIron instead of assuming upstream tools were perfect.

## Verification

`RawIron.EngineImport.Tests` now covers:

- valid and invalid stored runtime-tuning parses
- empty-level rejection
- invalid event hook rejection
- invalid worldspawn connection rejection
- valid level payload acceptance
- schema metric increments for both success and failure paths

Verified under:

- MSVC
- Clang

## Notes

This is intentionally a foundation pass, not a complete serializer/importer.

The native contracts now exist.
Future importer and asset-cooker work can target those contracts instead of re-inventing validation each time.
