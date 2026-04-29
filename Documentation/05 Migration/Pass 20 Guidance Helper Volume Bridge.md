---
tags:
  - rawiron
  - migration
  - world
  - content
  - guidance
---

# Pass 20 Guidance Helper Volume Bridge

## Goal

Port more of the prototype's authored guidance/helper world volumes into native RawIron systems.

This pass stayed fully inside RawIron.

`Q:\anomalous-echo` remained the read-only source reference.

## Native Landing Zone

Expanded native files:

- `Source/RawIron.World/include/RawIron/World/RuntimeState.h`
- `Source/RawIron.World/src/RuntimeState.cpp`
- `Source/RawIron.World/include/RawIron/World/VolumeDescriptors.h`
- `Source/RawIron.World/src/VolumeDescriptors.cpp`
- `Source/RawIron.Content/include/RawIron/Content/WorldVolumeContent.h`
- `Source/RawIron.Content/src/WorldVolumeContent.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestContentWorldVolumes.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestWorldVolumeDescriptors.cpp`
- `Tests/RawIron.EngineImport.Tests/src/main.cpp`

## What Landed

### Hint / Partition Helpers

RawIron now owns a typed native `HintPartitionVolume`.

The native volume preserves:

- authored `hint_skip_brush` classification
- hint-vs-skip mode
- runtime extents

### Camera Confinement Helpers

RawIron now owns a typed native `CameraConfinementVolume`.

The runtime service can now answer which confinement volume is active at a point.

### LOD Override Helpers

RawIron now owns a typed native `LodOverrideVolume`.

The native volume preserves:

- target IDs
- forced `near` / `far` LOD state
- runtime extents

The runtime service can now collect active LOD override volumes at a point.

### Navmesh Modifier Helpers

RawIron now owns a typed native `NavmeshModifierVolume`.

The native volume preserves:

- traversal cost
- authored tag / area type
- runtime extents

The runtime service can now collect active navmesh modifier volumes at a point.

### Helper Metrics

Runtime helper metrics now count:

- hint partition volumes
- camera confinement volumes
- lod override volumes
- navmesh modifier volumes

## Why This Matters

These are engine-authoring and editor/runtime guidance helpers.

They are not game logic.

Porting them into typed `C++` means RawIron keeps absorbing more of the prototype's level-authoring language into native runtime services instead of leaving it in JS registration code.

## Testing

Expanded coverage now verifies:

- authored hint partition conversion
- authored camera confinement conversion
- authored lod override conversion
- authored navmesh modifier conversion
- typed native descriptor creation for all four helper families
- runtime camera-confinement, hint-partition, lod-override, and navmesh-modifier queries
- helper-metric counts for the new helper families

## Result

RawIron now owns more of the prototype's guidance/helper volume language natively in `C++`.

This was a RawIron-native engine pass, not a prototype edit.

## Next Good Passes

1. Continue extracting remaining authored helper families like ambient audio and related world-side utilities.
2. Connect these native helper volumes to future scene/runtime assembly instead of leaving them as stored descriptors only.
3. Keep moving the prototype's engine-side world language into typed RawIron services one family at a time.
