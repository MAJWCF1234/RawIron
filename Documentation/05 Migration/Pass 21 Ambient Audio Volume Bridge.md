---
tags:
  - rawiron
  - migration
  - world
  - audio
---

# Pass 21 Ambient Audio Volume Bridge

## Goal

Port the prototype's authored ambient-audio helper volumes into native RawIron systems without coupling live playback directly into the world layer.

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

### Ambient Audio Volumes

RawIron now owns a typed native `AmbientAudioVolume` family with:

- `ambient_audio_volume`
- `ambient_audio_spline`

The native volume preserves:

- authored audio path
- base volume
- max distance
- label
- optional spline points

### Ambient Audio Contributions

`RuntimeEnvironmentService` can now compute native ambient-audio contributions at a point.

The runtime query returns:

- volume ID
- label
- audio path
- desired volume
- distance
- normalized falloff

That ports the useful engine-side spatial audio helper logic without forcing world state to own live playback handles yet.

### Spline Distance Support

RawIron now includes a native closest-point-on-polyline helper for ambient audio spline paths.

That preserves the prototype's spline-based ambient audio behavior in native `C++`.

### Helper Metrics

Runtime helper metrics now count ambient audio volumes.

## Why This Matters

Ambient audio volumes are engine-authoring helpers.

They define how a space sounds, not what the game rules are.

Porting them into typed `C++` moves more of the prototype's world-language vocabulary into native runtime services instead of leaving it embedded in JS-side update loops.

## Testing

Expanded coverage now verifies:

- authored ambient-audio volume conversion
- authored ambient-audio spline conversion
- typed native ambient-audio descriptor creation
- spline-point preservation
- runtime ambient-audio contribution queries
- helper-metric counts for ambient audio volumes

## Result

RawIron now owns more of the prototype's spatial audio helper language natively in `C++`.

This was a RawIron-native engine pass, not a prototype edit.

## Next Good Passes

1. Continue extracting remaining authored helper families such as analytics and selected world-side trigger helpers.
2. Connect these native ambient-audio descriptors to future runtime assembly so playback can be driven from typed engine state.
3. Keep moving the prototype's engine-side world language into typed RawIron services one family at a time.
