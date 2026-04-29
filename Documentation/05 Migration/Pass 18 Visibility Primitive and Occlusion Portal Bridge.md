---
tags:
  - rawiron
  - migration
  - world
  - content
  - visibility
---

# Pass 18 Visibility Primitive and Occlusion Portal Bridge

## Goal

Port the prototype's authored visibility-helper families into native RawIron systems without dragging renderer- or game-specific runtime glue across.

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

### Authored Visibility Primitives

RawIron now owns a typed native `VisibilityPrimitive` family with:

- `portal`
- `anti_portal`
- `occlusion_portal`

The native primitive keeps:

- authored ID
- visibility kind
- position
- rotation
- extents
- closed state for occlusion-backed primitives

### Authored Rotation Preservation

`RuntimeVolumeSeed` now preserves authored rotation as native data.

That matters because visibility helpers are the first world-authoring families in RawIron that really care about orientation instead of just volume containment.

### Occlusion Portal Runtime State

RawIron now owns a typed native `OcclusionPortalVolume`.

The runtime service can now:

- store occlusion portal volumes
- count closed portals
- toggle a portal by ID
- mirror that closed/open state onto matching visibility primitives

That matches the prototype's useful engine-side behavior without carrying over its rendering glue.

### Helper Metrics

Runtime helper metrics now count:

- portal primitives
- anti-portal primitives
- occlusion portal volumes
- closed occlusion portals

That keeps native instrumentation aligned with the new authored helper families RawIron understands.

## Why This Matters

These are engine-authoring and visibility-management helpers.

They are not game logic.

Porting them into typed `C++` means RawIron keeps absorbing the prototype's world-language vocabulary into native runtime services instead of leaving that meaning trapped inside mixed JS application code.

## Testing

Expanded coverage now verifies:

- authored runtime-volume seed rotation preservation
- portal and anti-portal content conversion
- occlusion portal content conversion
- typed native visibility-primitive descriptor creation
- typed native occlusion-portal descriptor creation
- runtime occlusion-portal toggling by ID
- helper-metric counts for the new visibility helper families

## Result

RawIron now owns more of the prototype's visibility-helper language natively in `C++`.

This was a RawIron-native engine pass, not a prototype edit.

## Next Good Passes

1. Continue extracting remaining authored world-helper families that are still embedded in prototype runtime glue.
2. Connect these native visibility helpers to future scene/runtime assembly instead of leaving them as stored descriptors only.
3. Start building the renderer/runtime-side consumers that can use these helpers without reintroducing JS-era coupling.
