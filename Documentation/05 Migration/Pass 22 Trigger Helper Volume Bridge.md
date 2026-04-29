---
tags:
  - rawiron
  - migration
  - pass-22
---

# Pass 22 Trigger Helper Volume Bridge

## Summary

This pass ports the prototype engine's trigger-oriented helper volumes into native `C++` inside RawIron.

The reference behavior came from `Q:\anomalous-echo\index.js`, but the implementation landed entirely in `N:\RAWIRON`.

## What Landed

### `RawIron.World`

Native typed trigger/helper runtime volumes now exist for:

- `streaming_level_volume`
- `checkpoint_spawn_volume`
- `teleport_volume`
- `launch_volume`
- `analytics_heatmap_volume`

The runtime environment service now supports:

- storing those trigger/helper families
- point queries for active streaming, checkpoint, teleport, and launch volumes
- overlap queries for active analytics heatmap volumes
- analytics heatmap entry counting
- analytics heatmap time accumulation
- helper metrics for all five new families

### `RawIron.Content`

The authored-content bridge now translates prototype-style data into native RawIron descriptors for:

- streaming level volumes
- checkpoint spawn volumes
- teleport volumes
- launch volumes
- analytics heatmap volumes

Preserved authored behavior includes:

- target-level aliases
- checkpoint respawn position and rotation
- teleport target ID, target position, target rotation, and offset
- launch impulse normalization and fallback vertical launch strength
- analytics heatmap runtime counter initialization

## Files

- `Source/RawIron.World/include/RawIron/World/RuntimeState.h`
- `Source/RawIron.World/src/RuntimeState.cpp`
- `Source/RawIron.World/include/RawIron/World/VolumeDescriptors.h`
- `Source/RawIron.World/src/VolumeDescriptors.cpp`
- `Source/RawIron.Content/include/RawIron/Content/WorldVolumeContent.h`
- `Source/RawIron.Content/src/WorldVolumeContent.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestContentTriggerVolumes.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestRuntimeTriggerVolumes.cpp`

## Verification

Verified in both build trees:

- `MSVC: 14/14` CTest entries passed
- `Clang: 14/14` CTest entries passed

## Notes

This pass keeps the earlier port boundary intact:

- the prototype in `Q:\anomalous-echo` remains the read-only reference
- RawIron in `N:\RAWIRON` is the real native engine implementation

The remaining work is no longer “do we have typed trigger helper volumes?”
It is now “how much broader mixed trigger/runtime orchestration still needs to move into native RawIron services?”
