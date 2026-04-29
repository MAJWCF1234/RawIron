---
tags:
  - rawiron
  - migration
  - world
  - volumes
---

# Pass 11 World Volume Descriptor Foundation

## Goal

Port the prototype's typed runtime-volume helper logic out of `Q:\anomalous-echo\index.js` and into native `C++` so RawIron can own more of its world-authoring/runtime contract.

This pass focuses on the parts of the prototype that decide what a runtime volume means before game-specific behavior gets layered on top.

## Prototype Source

Primary source seam:

- `Q:\anomalous-echo\index.js`

Prototype helpers mirrored in this pass:

- `getClipVolumeModes`
- `getVolumeChannels`
- `traceTagMatchesVolume`
- `createRuntimeBoxVolume`
- `getActiveCameraModifierAt`
- `isPositionInSafeZone`

## Native Landing Zone

Expanded library:

- `Source/RawIron.World`

New native files:

- `Source/RawIron.World/include/RawIron/World/VolumeDescriptors.h`
- `Source/RawIron.World/src/VolumeDescriptors.cpp`

Also updated:

- `Source/RawIron.World/include/RawIron/World/RuntimeState.h`

## What Landed

### Typed Runtime Volume Descriptors

RawIron now has native typed world-volume descriptors for:

- filtered collision volumes
- clip-runtime volumes
- damage volumes
- camera modifier volumes
- safe zones

These now sit on top of the shared `RuntimeVolume` base instead of staying as loose prototype objects.

### Channel And Mode Parsing

RawIron now normalizes prototype-style string authoring into typed native enums for:

- clip volume modes
  - `physics`
  - `ai`
  - `visibility`
- collision channels
  - `player`
  - `physics`
  - `camera`
  - `bullet`
  - `ai`
  - `vehicle`

Current behavior mirrors the prototype:

- lowercase normalization
- invalid token rejection
- duplicate removal
- stable authoring order
- sensible defaults when authoring data is empty

### Runtime Volume Creation

RawIron now has a native helper for creating runtime volumes from authored seed data plus engine defaults.

Current behavior:

- runtime ID synthesis
- runtime type fallback
- shape resolution
- positive size normalization
- derived radius fallback
- derived height fallback

This gives the engine a native place to say “this authored box-like thing becomes this runtime volume” without depending on JavaScript object assembly.

### World Queries

This pass also landed a few small but important world-query helpers:

- trace-tag matching against filtered collision channels
- highest-priority active camera modifier lookup
- safe-zone inclusion queries

These are tiny, but they are engine code, not UI code.

## Why This Matters

The prototype did not just have “volumes.”
It had rules about what different volumes meant.

Those rules matter because the engine needs to understand distinctions like:

- physics clip versus AI clip
- player blockers versus camera blockers
- harmless spatial helpers versus damage volumes
- authored camera regions versus safe zones

That meaning now has a native home in RawIron.

## Testing

Coverage lives in:

- `Tests/RawIron.EngineImport.Tests/src/TestWorldVolumeDescriptors.cpp`

Verified behaviors:

- clip-mode parsing
- collision-channel parsing
- runtime ID and type defaults
- radius and height derivation
- filtered collision trace-tag matching
- clip-runtime enabled state
- damage-volume clamp behavior
- kill-volume lethal behavior
- camera-modifier priority selection
- safe-zone inclusion checks

## Extra Project Fix

While landing this pass, the workspace exposed an older project-graph issue:

- `RawIron.cgltf` is a `C` target
- the root project and presets had only been set up cleanly for `C++`

That is now fixed by:

- enabling `C` at the root project level
- pinning both `C` and `C++` compilers in `CMakePresets.json`

This was not new engine behavior, but it was necessary to keep future native passes buildable after CMake regeneration.

## Result

RawIron now owns more of the prototype's world-volume meaning in native `C++`.

That means:

- fewer loose runtime-object conventions
- more typed engine-side world data
- cleaner future bridges from authored content into runtime world services

## Next Good Passes

1. Bridge `RawIron.Content` authoring data directly into these typed world-volume descriptors.
2. Continue extracting more mixed world/runtime helpers out of `index.js` into `RawIron.World`.
3. Push these typed volume concepts upward into future editor inspection and `.ri_scene` assembly paths.
