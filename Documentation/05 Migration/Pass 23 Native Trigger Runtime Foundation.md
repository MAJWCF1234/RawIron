---
tags:
  - rawiron
  - migration
  - pass-23
---

# Pass 23 Native Trigger Runtime Foundation

## Summary

This pass ports the prototype engine's trigger runtime behavior into native `C++` inside RawIron.

Pass 22 gave RawIron typed trigger/helper volume families.
This pass gives those volumes a native state machine.

## What Landed

### `RawIron.Content`

The authored-content bridge now supports two more trigger-adjacent helper families:

- `generic_trigger_volume`
- `spatial_query_volume`

The bridge preserves:

- generic trigger broadcast frequency
- spatial query broadcast frequency
- spatial query filter mask
- prototype default generic-trigger radius behavior

### `RawIron.World`

The runtime layer now owns native trigger orchestration for:

- generic trigger volumes
- spatial query volumes
- streaming level volumes
- checkpoint spawn volumes
- teleport volumes
- launch volumes
- analytics heatmap volumes

The native trigger update path now supports:

- enter transitions
- stay transitions driven by broadcast frequency
- exit transitions
- typed trigger directives for streaming/checkpoint/teleport/launch helper families
- analytics heatmap entry counting on enter
- analytics heatmap time accumulation while active
- optional `triggerChanged` runtime-event emission for enter/exit transitions

## Files

- `Source/RawIron.World/include/RawIron/World/RuntimeState.h`
- `Source/RawIron.World/src/RuntimeState.cpp`
- `Source/RawIron.World/include/RawIron/World/VolumeDescriptors.h`
- `Source/RawIron.World/src/VolumeDescriptors.cpp`
- `Source/RawIron.Content/include/RawIron/Content/WorldVolumeContent.h`
- `Source/RawIron.Content/src/WorldVolumeContent.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestContentTriggerVolumes.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestRuntimeTriggerVolumes.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestWorldVolumeDescriptors.cpp`

## Verification

Verified in both build trees:

- `MSVC: 14/14` CTest entries passed
- `Clang: 14/14` CTest entries passed

## Notes

This pass stays inside the same boundary as the earlier ones:

- `Q:\anomalous-echo` remains the reference implementation
- `N:\RAWIRON` is where the native engine behavior lands

The remaining mixed world/runtime bucket is getting narrower.
RawIron now owns not just trigger helper data, but the first real native trigger lifecycle around that data.
