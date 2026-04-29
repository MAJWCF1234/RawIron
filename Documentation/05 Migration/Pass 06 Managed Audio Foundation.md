---
tags:
  - rawiron
  - migration
  - audio
---

# Pass 06 Managed Audio Foundation

## Prototype Sources

- `Q:\anomalous-echo\engine\audio\AudioManager.js`
- `Q:\anomalous-echo\obsidian\Engine\00 Engine Home.md`
- `Q:\anomalous-echo\obsidian\Engine\01 Runtime Flow.md`
- `Q:\anomalous-echo\obsidian\Engine\05 Debugging and Instrumentation.md`

## Goal

Lift the prototype's managed audio behavior into a native engine library without dragging `Howler` or browser timers into RawIron.

## Native Landing Zone

- `Source/RawIron.Audio`

## What Landed

- backend-agnostic audio backend/playback interfaces
- `ManagedSound` wrapper matching the prototype's play/pause/stop/unload and playback-state shape
- `AudioManager` support for:
  - loop creation
  - one-shot playback
  - active voice-line ownership and replacement
  - environment-profile normalization
  - environment-shaped volume/playback-rate application
  - delayed echo scheduling through engine time instead of `window.setTimeout`
  - engine-side audio metrics

Key files:

- `N:\RAWIRON\Source\RawIron.Audio\include\RawIron\Audio\AudioManager.h`
- `N:\RAWIRON\Source\RawIron.Audio\src\AudioManager.cpp`

## Why This Pass Matters

The prototype already treated audio as an engine service:

- voices are managed centrally
- local environment affects playback feel
- metrics exist so debug surfaces can explain what the audio layer is doing

Porting that logic into RawIron keeps the engine-side behavior while freeing it from browser-only APIs.

## Verification

`RawIron.EngineImport.Tests` now covers:

- environment-profile normalization and deduped environment-change tracking
- environment-shaped playback volume and rate
- managed loop creation without forced autoplay
- one-shot playback plus delayed echo scheduling
- active voice replacement and voice-finish cleanup
- mute behavior for one-shots and voices
- audio metrics for loops, one-shots, voices, and active environment state

Verified under:

- MSVC
- Clang

## Notes

This pass is backend-agnostic on purpose.

RawIron now owns the audio-management logic.
A real platform/backend implementation can plug into the `AudioBackend` interface later without rewriting the engine-side behavior again.
