---
tags:
  - rawiron
  - migration
  - content
  - world
  - environment
---

# Pass 13 Environment Volume Content Bridge

## Goal

Port more of the prototype's authored environment-volume registration logic out of `Q:\anomalous-echo\index.js` and into native `C++`.

This pass expands RawIron's authored content bridge so environment-shaping volume families can become native runtime descriptors without depending on prototype app code.

## Prototype Source

Primary source seam:

- `Q:\anomalous-echo\index.js`

Prototype helpers mirrored in this pass:

- `registerPostProcessVolume`
- `registerAudioReverbVolume`
- `registerAudioOcclusionVolume`
- `registerFluidSimulationVolume`

Supporting prototype helpers reused from earlier passes:

- `createRuntimeBoxVolume`
- finite vector sanitizers
- finite number clamps

## Native Landing Zone

Expanded libraries:

- `Source/RawIron.Content`
- `Source/RawIron.World`

New native file:

- `Tests/RawIron.EngineImport.Tests/src/TestContentEnvironmentVolumes.cpp`

Expanded native files:

- `Source/RawIron.Content/include/RawIron/Content/WorldVolumeContent.h`
- `Source/RawIron.Content/src/WorldVolumeContent.cpp`
- `Source/RawIron.World/include/RawIron/World/RuntimeState.h`
- `Source/RawIron.World/include/RawIron/World/VolumeDescriptors.h`
- `Source/RawIron.World/src/VolumeDescriptors.cpp`

## What Landed

### Native Content Builders For Environment Volumes

RawIron can now build native runtime descriptors from authored content objects for:

- post-process volumes
- audio reverb volumes
- audio occlusion volumes
- fluid simulation volumes

This means the content side of the engine now owns another chunk of “what this authored thing means at runtime.”

### Rounded-Out World Creation Helpers

`RawIron.World` now has native creation helpers for:

- post-process volume defaults and clamps
- audio reverb volume defaults and clamps
- audio occlusion volume defaults and clamps
- fluid simulation volume defaults and clamps

That keeps runtime meaning in the world layer instead of forcing `RawIron.Content` to invent those rules ad hoc.

### Fluid Runtime Data Grew Up

`FluidSimulationVolume` now carries more of the prototype's useful tuning data:

- `gravityScale`
- `jumpScale`
- `drag`
- `buoyancy`
- `flow`
- `tintStrength`
- `reverbMix`
- `echoDelayMs`

That gives the runtime a native place to preserve fluid-behavior tuning even before every later consumer has been ported.

### Prototype-Shaped Shape Defaults

This pass also preserved the prototype's more specific shape behavior:

- post-process and audio reverb volumes default to `sphere`
- fluid simulation volumes stay on supported prototype shapes and fall back to `box`
- authored explicit box and cylinder choices are preserved where appropriate

## Why This Matters

The prototype's environment system was not just a handful of runtime arrays.

It also had authoring rules about:

- which volume families default to sphere versus box
- how environment tuning gets clamped
- how occlusion and reverb behave differently
- how fluid tuning and flow get assembled from authored fields

Those rules are engine behavior.

RawIron now owns more of them natively.

## Testing

Coverage lives in:

- `Tests/RawIron.EngineImport.Tests/src/TestContentEnvironmentVolumes.cpp`

Verified behaviors:

- post-process classification and clamp behavior
- post-process default sphere shape
- audio reverb classification, explicit shape preservation, and clamp behavior
- audio occlusion fallback sizing and tuning clamps
- fluid classification
- fluid supported-shape fallback
- fluid gravity, jump, drag, buoyancy, and environment shaping clamps
- fluid flow vector assembly from direction and strength

## Result

RawIron's content bridge now covers another meaningful piece of the prototype engine:

- trigger and gameplay-adjacent authored volume families from pass 12
- environment and atmosphere-shaping authored volume families from pass 13

That is a stronger native foundation for future `.ri_scene` assembly and world-runtime loading.

## Next Good Passes

1. Feed these authored native environment volumes into future runtime-world assembly and `.ri_scene` loading paths.
2. Continue porting additional volume families such as reflection probes, light-importance bounds, and portals once their native runtime owners are ready.
3. Keep extracting more authored world helpers out of `index.js` into `RawIron.Content` instead of rebuilding them in shells or tools.
