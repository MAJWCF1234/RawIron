---
tags:
  - rawiron
  - migration
  - world
  - content
  - lighting
---

# Pass 17 Lighting and Probe Helper Volume Bridge

## Goal

Port more of the prototype's authored lighting-helper world volumes into native RawIron systems.

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

### New Native Helper Volume Families

RawIron now owns typed native runtime families for:

- reflection probe volumes
- light importance volumes
- light portals

### Probe-Grid Bounds Support

`LightImportanceVolume` now carries a native `probeGridBounds` flag.

That preserves the prototype distinction between:

- normal light-importance bounds
- `probe_grid_bounds`

without leaving that meaning trapped in a stringly app helper.

### Authored Content Bridge

`RawIron.Content` now translates authored content into those native helper volume descriptors.

The bridge preserves:

- prototype-style fallback sizes
- authored type distinctions
- non-zero extent clamping

### Runtime Storage And Metrics

`RuntimeEnvironmentService` now stores the new volume families and exposes native getters for them.

Helper metrics now count:

- reflection probe volumes
- light importance volumes
- light portal volumes

That keeps RawIron's runtime visibility aligned with the authored helper families it now understands.

## Why This Matters

These are not gameplay systems.

They are engine-authoring systems.

Porting them natively means RawIron is continuing to absorb the prototype's rendering/world helper language into clean `C++` engine boundaries instead of leaving it in mixed JS runtime glue.

## Testing

Expanded coverage now verifies:

- reflection probe content conversion
- light importance content conversion
- probe-grid-bounds preservation
- light portal content conversion
- typed native descriptor creation
- helper-metric counts for the new runtime families

## Result

RawIron now owns more of the prototype's authored lighting-helper vocabulary natively in `C++`.

This was a RawIron-native engine pass, not a prototype edit.

## Next Good Passes

1. Keep extracting remaining authored helper families like visibility and occlusion portal systems.
2. Start connecting these helper volumes to future renderer/runtime assembly instead of keeping them as stored descriptors only.
3. Continue moving the prototype's engine-side world language into typed RawIron services.
