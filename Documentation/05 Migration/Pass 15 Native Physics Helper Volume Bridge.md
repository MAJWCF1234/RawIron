---
tags:
  - rawiron
  - migration
  - world
  - content
  - physics
---

# Pass 15 Native Physics Helper Volume Bridge

## Goal

Extend RawIron's native physics-volume seam so more of the prototype's engine-only world helpers stop living as JS-only behavior.

This pass stayed fully on the RawIron side.

`Q:\anomalous-echo` remained the read-only reference.

## Native Landing Zone

Expanded native files:

- `Source/RawIron.World/include/RawIron/World/RuntimeState.h`
- `Source/RawIron.World/src/RuntimeState.cpp`
- `Source/RawIron.World/include/RawIron/World/VolumeDescriptors.h`
- `Source/RawIron.World/src/VolumeDescriptors.cpp`
- `Source/RawIron.Content/include/RawIron/Content/WorldVolumeContent.h`
- `Source/RawIron.Content/src/WorldVolumeContent.cpp`
- `Tests/RawIron.EngineImport.Tests/src/TestContentPhysicsVolumes.cpp`
- `Tests/RawIron.EngineImport.Tests/src/main.cpp`

## What Landed

### New Native Volume Families

RawIron now has typed native world-volume descriptors for:

- custom gravity volumes
- directional wind volumes
- buoyancy volumes
- surface velocity primitives
- radial force volumes

These are no longer trapped as loose mixed runtime helpers.

### Authored Content Bridge

`RawIron.Content` now translates authored content objects into those native runtime types.

That bridge preserves prototype-style defaults and clamps for:

- extents
- gravity and jump scaling
- drag and buoyancy
- directional flow
- radial-force strength and falloff

### Runtime Physics Aggregation

`RuntimeEnvironmentService::GetPhysicsVolumeModifiersAt` now aggregates more than fluid volumes.

It now merges:

- generic physics modifier volumes
- fluid simulation volumes
- surface velocity primitives
- radial force volumes

The native state now reports:

- active physics volume IDs
- active fluid volume IDs
- active surface velocity primitive IDs
- active radial force volume IDs
- merged flow, gravity, jump, drag, and buoyancy modifiers

### Runtime Metrics

Helper metrics now count the new native families too:

- physics modifier volumes
- surface velocity primitives
- radial force volumes

That keeps RawIron's debug/instrumentation view aligned with the actual runtime state it now owns.

## Why This Matters

Pass 14 created a native physics-modifier state.

Pass 15 makes that state meaningfully broader and more prototype-faithful.

RawIron now owns more of the world helper behavior that makes authored spaces feel alive:

- local gravity changes
- directional wind
- conveyor-like surface flow
- radial push and pull fields

That is engine behavior, not game-script residue.

## Testing

Expanded coverage now verifies:

- authored custom gravity conversion
- authored directional wind conversion
- authored buoyancy conversion
- authored surface velocity conversion
- authored radial force conversion
- runtime aggregation of physics, fluid, surface, and radial-force contributions
- active native ID reporting for those families
- helper-metric counts for the expanded runtime world service

## Result

RawIron now has a stronger native world/runtime seam in `C++`:

- broader authored physics helper support
- better runtime physics aggregation
- cleaner debug visibility into those systems

This was a RawIron-native improvement pass, not a prototype edit.

## Next Good Passes

1. Feed these native helper volumes into broader runtime/world assembly instead of only test seams.
2. Keep pulling more authored world helpers out of `index.js` and into `RawIron.Content` plus `RawIron.World`.
3. Start wiring these systems into editor/runtime inspection paths so the player and tools can see them live.
