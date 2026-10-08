# Interactive props on TraceScene — 2026-10-08

Cube Test interaction and projectile props now integrate against the same
`TraceScene` the player uses, instead of bouncing only inside an AABB sandbox.

## Engine surface

- `RawIron.World` / `InteractivePropTracePhysics.h`
  - `StepInteractivePropFieldAgainstTrace`
  - `MakeInteractivePropTraceCollider` / `MakeInteractivePropColliderId`
- Props keep `InteractivePropState` for grab, authority, and replication.
- Integration uses `SimulateKinematicBodyForDuration` with
  `KinematicPhysicsOptions::structuralOnly = false` so props hit floors,
  non-structural arena walls, and other dynamic prop colliders.
- Player movement stays `structuralOnly = true`, so arena walls and prop hulls
  do not block walking.

## Cube Test wiring

- Each interaction/projectile prop registers a `dynamic` TraceCollider at world build.
- Arena walls/ceiling are non-structural containment colliders around the old field bounds.
- `AnimateCubeTestWorld(..., TraceScene*)` steps both pools through the Trace path.
- Desktop/VR hosts pass the live play `TraceScene`; smokes/editor may omit it and
  get a temporary scene built from `world.colliders`.

## Validation

```
cmake --build build/dev-msvc --config RelWithDebInfo --parallel 8 --target
  InteractivePropTracePhysicsSmoke RawIron.CubeTest.WorldSmoke
ctest --test-dir build/dev-msvc -C RelWithDebInfo -R
  "InteractivePropTracePhysics|CubeTest.WorldSmoke|ObjectPhysics" --output-on-failure
```

## Actor push (follow-up)

`PushInteractivePropsFromActor` resolves AABB overlap between a player/capsule hull
and interactive props. Contacted props are separated and receive a capped horizontal
impulse from the actor velocity. Cube Test applies this after movement for both the
interaction and projectile pools (local authority only).

Still not Havok/Source VPhysics: no joints, vehicles, cloth, or a full contact
solver. These slices close the showcase gaps where props ignored world geometry and
could not be shoved by walking.
