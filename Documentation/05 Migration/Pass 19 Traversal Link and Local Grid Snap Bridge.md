---
tags:
  - rawiron
  - migration
  - world
  - content
  - traversal
---

# Pass 19 Traversal Link and Local Grid Snap Bridge

## Goal

Port more of the prototype's authored movement-helper and editor-helper world volumes into native RawIron systems.

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

### Traversal Link Volumes

RawIron now owns a typed native `TraversalLinkVolume` family with:

- `traversal_link_volume`
- `ladder_volume`
- `climb_volume`

The native volume keeps:

- authored ID
- typed traversal-link kind
- position and extents
- climb speed

### Local Grid Snap Volumes

RawIron now owns a typed native `LocalGridSnapVolume`.

That preserves the prototype's local snap-grid helper behavior as engine-owned world data instead of leaving it embedded in mixed JS/editor glue.

### Runtime Queries

`RuntimeEnvironmentService` can now answer:

- the active traversal link at a point
- the active local grid snap volume at a point

That gives editor/runtime systems a native place to look up traversal and local-grid helper state.

### Helper Metrics

Runtime helper metrics now count:

- traversal link volumes
- local grid snap volumes

That keeps native instrumentation aligned with the new authored helper families RawIron understands.

## Why This Matters

These are world-authoring and movement-tooling helpers.

They are not game logic.

Porting them into typed `C++` means RawIron keeps absorbing the prototype's world-language vocabulary into native runtime services instead of leaving it trapped in JS-side registration helpers.

## Testing

Expanded coverage now verifies:

- authored traversal-link content conversion
- authored local-grid-snap content conversion
- typed native traversal-link descriptor creation
- typed native local-grid-snap descriptor creation
- runtime traversal-link queries
- runtime local-grid-snap queries
- helper-metric counts for the new helper families

## Result

RawIron now owns more of the prototype's traversal and editor-helper language natively in `C++`.

This was a RawIron-native engine pass, not a prototype edit.

## Next Good Passes

1. Continue extracting remaining authored helper families like hint/partition helpers and related editor/runtime guidance volumes.
2. Connect these native traversal and snap helpers to future scene/runtime assembly instead of leaving them as stored descriptors only.
3. Keep moving the prototype's engine-side world language into typed RawIron services one family at a time.
