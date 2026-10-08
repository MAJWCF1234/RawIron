# Interactive props on TraceScene — 2026-10-08

Cube Test interaction and projectile props integrate against the same `TraceScene`
the player uses. The player hull now collides with tagged dynamic props inside the
movement solver — not through a shove-only side channel.

## Engine surface

- `RawIron.World` / `InteractivePropTracePhysics.h`
  - `StepInteractivePropFieldAgainstTrace`
  - `MakeInteractivePropTraceCollider` / `MakeInteractivePropColliderId`
  - `ImpulseInteractivePropsFromTraceHits` — response after real Trace hits
  - `PushInteractivePropsFromActor` — depenetration only when already overlapping
- `RawIron.Trace` collision flags and filters:
  - `kTraceFlagInteractiveProp`, `kTraceFlagPropArena`, …
  - `TraceOptions::includeDynamicFlags` / `excludeFlags`
  - `KinematicPhysicsOptions::includeDynamicFlags` / `excludeFlags`
- Props keep `InteractivePropState` for grab, authority, and replication.
- Prop simulation uses `structuralOnly = false` so props hit floors, arena walls,
  and other dynamic prop colliders.
- Player movement stays `structuralOnly = true` for floors/walls, with
  `includeDynamicFlags = kTraceFlagInteractiveProp` so prop hulls block the player.
  Arena walls use `kTraceFlagPropArena` and are ignored by the player.

## Cube Test wiring

- Each interaction/projectile prop registers a dynamic TraceCollider flagged
  `kTraceFlagInteractiveProp`.
- Arena walls/ceiling are non-structural `kTraceFlagPropArena` containment colliders.
- After movement, Cube Test applies impulses from `MovementControllerResult::step.hits`.
  Blanket per-frame shove is gone; overlap push runs only for spawn/teleport penetration.

## Validation

```
cmake --build build/dev-msvc --config RelWithDebInfo --parallel 8 --target
  TraceDynamicFilterSmoke InteractivePropTracePhysicsSmoke RawIron.CubeTest.WorldSmoke
ctest --test-dir build/dev-msvc -C RelWithDebInfo -R
  "TraceDynamicFilter|InteractivePropTracePhysics|CubeTest.WorldSmoke|ObjectPhysics" --output-on-failure
```

Still not Havok/Source VPhysics: no joints, vehicles, cloth, or a full contact
solver. The closed gap is that player↔prop contact is now a Trace collision filter
feature, not an AABB shove covering missing hull tests.
