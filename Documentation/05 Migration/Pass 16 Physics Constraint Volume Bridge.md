---
tags:
  - rawiron
  - migration
  - world
  - content
  - constraints
---

# Pass 16 Physics Constraint Volume Bridge

## Goal

Port the prototype's physics-constraint helper volumes into native RawIron world/content systems.

This pass kept the work on the RawIron side only.

`Q:\anomalous-echo` remained the read-only design reference.

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

### Typed Constraint Axes

RawIron now has a native `ConstraintAxis` enum instead of leaving this seam as a bag of loose strings.

Supported native axis locks:

- `X`
- `Y`
- `Z`

### Native Physics Constraint Volumes

RawIron now owns a typed runtime volume family:

- `PhysicsConstraintVolume`

These volumes carry:

- standard runtime volume bounds
- typed locked-axis lists

### Authored Content Bridge

`RawIron.Content` now translates authored `lockAxes` content into native `PhysicsConstraintVolume` descriptors.

That bridge:

- filters invalid axis labels
- deduplicates repeated axes
- preserves authored order
- keeps the prototype-style fallback volume size

### Runtime Constraint Query

`RuntimeEnvironmentService` now exposes native constraint-state queries:

- `GetPhysicsConstraintStateAt`

The combined runtime environment state now also includes constraint state so later player/editor systems can ask one question and get:

- post-process state
- audio state
- physics modifiers
- physics constraints

### Runtime Metrics

Helper metrics now count native physics-constraint volumes too.

That keeps the runtime snapshots and instrumentation layer aware of another real authored world family.

## Why This Matters

Physics constraints are small, but they are the kind of small system that makes an engine feel authored rather than improvised.

Porting them natively means:

- less mixed world logic trapped in JS
- cleaner future player movement hooks
- a stronger path for editor/runtime inspection

This is another case where RawIron is preserving engine behavior while improving the engine shape.

## Testing

Expanded coverage now verifies:

- typed constraint-axis parsing
- typed physics-constraint volume creation
- authored `lockAxes` conversion
- active runtime physics-constraint queries
- combined environment-state exposure of constraints
- helper-metric counts for the new volume family

## Result

RawIron now owns another prototype engine helper natively in `C++`:

- typed axis-lock authoring
- typed runtime constraint volumes
- native runtime constraint queries

This was a RawIron-native porting pass, not a prototype patch.

## Next Good Passes

1. Keep extracting additional authored helper families from the mixed prototype world runtime.
2. Start feeding native constraint and physics-helper volumes into broader player/editor runtime assembly.
3. Continue moving editor-facing inspection paths onto native world/runtime services.
