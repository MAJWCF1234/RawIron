---
tags:
  - rawiron
  - migration
  - content
  - world
  - volumes
---

# Pass 12 Authored Volume Content Bridge

## Goal

Bridge prototype-authored volume data out of `Q:\anomalous-echo\index.js` and into native `C++` so RawIron can translate content values into typed world-volume runtime descriptors without JavaScript glue.

This pass does not try to port every later game-side consumer of those volumes.

It focuses on the engine boundary where authored content becomes native runtime meaning.

## Prototype Source

Primary source seam:

- `Q:\anomalous-echo\index.js`

Prototype helpers mirrored in this pass:

- `registerFilteredCollisionVolume`
- `registerAiPerceptionBlockerVolume`
- `registerCameraBlockingVolume`
- `registerDamageVolume`
- `registerCameraModifierVolume`
- `registerSafeZoneVolume`
- `registerLocalizedFogVolume`
- `registerVolumetricFogBlocker`

Supporting prototype helpers already ported in earlier passes and reused here:

- `createRuntimeBoxVolume`
- `getClipVolumeModes`
- `getVolumeChannels`
- content sanitizers and finite-number clamps

## Native Landing Zone

Expanded libraries:

- `Source/RawIron.Content`
- `Source/RawIron.World`

New native files:

- `Source/RawIron.Content/include/RawIron/Content/WorldVolumeContent.h`
- `Source/RawIron.Content/src/WorldVolumeContent.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestContentWorldVolumes.cpp`

Also updated:

- `Source/RawIron.World/include/RawIron/World/VolumeDescriptors.h`
- `Source/RawIron.World/src/VolumeDescriptors.cpp`

## What Landed

### Native Authoring Bridge

RawIron can now turn generic authored content objects into typed native world volumes for:

- filtered collision volumes
- camera-blocking volumes
- AI perception blockers
- damage volumes
- camera modifier volumes
- safe zones
- localized fog volumes
- volumetric fog blockers

This means the engine now owns the authoring-to-runtime conversion rules for these volume families instead of leaving them trapped in prototype app code.

### Shared Runtime Seed Translation

`RawIron.Content` now has a small native bridge that extracts common authored volume data such as:

- `id`
- `type`
- `shape`
- `position`
- `size` or `scale`
- `radius`
- `height`

Current native behavior:

- size aliases resolve from `size` or `scale`
- size components are normalized to positive runtime extents
- invalid numeric overrides fall back cleanly
- authored IDs and authored explicit types are preserved where appropriate

### Bridge-Side Defaults

The content layer now applies prototype-shaped defaults when authoring data is incomplete, including:

- filtered collision fallback size
- AI blocker fallback size
- damage-volume fallback size and damage clamps
- camera-modifier fallback size, FOV, and priority clamps
- safe-zone fallback size and drop-aggro default
- localized-fog fallback size plus tint and blur clamps
- fog-blocker fallback size

This keeps the authored-content side aligned with the runtime volume rules already ported into `RawIron.World`.

### Rounded-Out World Helpers

To support the bridge cleanly, `RawIron.World` now also exposes native creation helpers for:

- localized fog volumes
- volumetric fog blockers

That keeps the authoring bridge from smuggling runtime rules into the wrong layer.

## Why This Matters

The prototype did not just create runtime volumes by hand.

It also knew how authored world data should become those volumes.

That translation step matters because it is where the engine decides:

- what defaults count as sane
- which aliases authoring is allowed to use
- which volume family a content object actually belongs to
- how much invalid authoring data gets tolerated before falling back

RawIron now owns more of that meaning natively.

## Testing

Coverage lives in:

- `Tests/RawIron.EngineImport.Tests/src/TestContentWorldVolumes.cpp`

Verified behaviors:

- runtime-seed extraction from authored IDs, shapes, sizes, radius, and height
- positive-size normalization from authored scale values
- filtered-collision channel parsing through authored content
- camera-blocking channel forcing
- AI blocker clip-mode forcing
- damage and kill-volume runtime defaults
- camera-modifier FOV and priority clamps
- safe-zone drop-aggro preservation
- localized-fog tint and blur clamps
- fog-blocker fallback sizing

## Result

RawIron now has a native bridge between:

- generic authored content values
- typed runtime world-volume descriptors

That is another real step away from “prototype shell code” and toward “engine-owned authored world assembly.”

## Next Good Passes

1. Feed these authored native volumes into future `.ri_scene` and runtime-world assembly paths.
2. Continue extracting more authored world helpers out of `index.js` into `RawIron.Content`.
3. Extend the bridge to other prototype volume families such as light-importance and reflection-probe bounds when their runtime owners are ready.
