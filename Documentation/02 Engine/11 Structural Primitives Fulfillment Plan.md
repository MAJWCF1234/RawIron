# Structural Primitives Fulfillment Plan

RawIron structural primitives are the engine's upgraded brush model. They own authored shape intent, generated mesh outputs, physics/query surfaces, and semantic meaning, while derived partitions make runtime systems fast.

This file replaces the one-off implementation notes that previously lived under `docs/superpowers/plans/`. Keep future structural primitive planning here unless it belongs in a narrower public guide.

## Product Intent

Structural primitives should feel like smarter Source-style brushes, but not be limited to old BSP authoring. The source of truth is an authored structural primitive with M/P/Q/I ownership:

- `M-mesh / Visual mesh`: render geometry, materials, UVs, visible shape.
- `P-mesh / Physics mesh`: collision hulls, rigid body shape, simulation shape, physical material.
- `Q-mesh / Query mesh`: raycast, trace, placement, and interaction shape.
- `I-layer / Information`: semantic role, relations, reporting, structural spatial graph links, and gameplay meaning.

Derived systems then build fast data products from that authored source:

- Semantic structural partition for role, region, operation, policy, channel, and purpose filtering.
- Trace scene feed for movement, ballistics, pseudo-raytracing candidates, interaction traces, and editor picking.
- Visibility and culling feeds for portal, occluder, anti-portal, and region diagnostics.
- Edit/rebuild tracking so brush changes invalidate only the affected area.

## Current Shipped State

The first foundation is in place:

- Structural brush metadata is preserved on scene nodes.
- Optional semantic CSV columns can tag structural rows without breaking older files.
- Structural brushes expose role, region, operation, collision, visibility, navigation, and rebuild-scope policies.
- M/P/Q/I channel metadata exists for visual, physics, query, and information ownership.
- Semantic structural partition wraps the existing BSP-style spatial index and adds semantic filtering.
- Semantic structural partition entries carry compact metadata signatures for cheap diagnostics and cache comparisons.
- Semantic structural partition keeps a compact metadata-signature lookup table for duplicate/diagnostic tooling.
- Semantic structural partition keeps a region lookup table as a stepping stone toward per-region partitions and dirty-region rebuilds.
- Semantic structural partition is a partition family: one spatial subpartition per authored region plus a regionless bucket, all sharing the authored entry list.
- Region-scoped queries resolve against the region's own subpartition instead of walking every tree; metrics report how many queries were region-scoped.
- Partition rebuilds keep the spatial tree of any subpartition whose content (ids, bounds, order) is unchanged, so a single brush edit re-splits only the affected region; metrics report reused vs rebuilt subpartitions per rebuild.
- Metadata-only edits (roles, policies, channels) never force a spatial re-split because tree content signatures cover only ids and bounds.
- The partition cache rebuilds in place, so cache-driven refreshes inherit subpartition reuse.
- Partition metrics report matched-versus-scanned candidate counts for box and ray queries so semantic filter savings are measurable.
- Partition queries support role, region, operation, rebuild scope, M/P/Q/I channel, and query purpose.
- Partition metrics expose entry counts, metadata signature uniqueness, query counts, candidate scans, role counts, operation counts, rebuild counts, channel counts, and query-purpose counts.
- Scene subtree collider generation can respect structural collision policy, Q-mesh participation, and query purpose.
- Structural trace feed builds filtered `TraceCollider` lists and ready-to-query `TraceScene` instances.
- Trace feed metrics report source brush count, emitted collider count, filtered count, filter reasons, and efficiency ratios.
- Multiplayer Sandbox merges its filtered structural hall feed into the first-person movement trace scene and logs its source, emitted, and filtered counts.
- Competitive hitscan and projectile resolution accepts the same structural-only trace-scene contract, so rewind checks stop at blocking structural geometry rather than resolving through walls.
- Structural-only trace queries include both static and dynamic structural colliders while continuing to exclude non-structural dynamic content.
- Structural brush metadata exposes a deterministic signature for cache invalidation and cheap change detection.
- Semantic structural partition cache uses structural-only scene signatures to auto-refresh after brush metadata edits without rebuilding for non-structural scene churn.
- Semantic structural partition cache signatures include resolved world bounds, so direct brush or ancestor-transform edits refresh cached spatial entries rather than picking at stale positions.
- Semantic structural partition cache exposes a non-mutating rebuild check for editor/debug dirty-state reporting.
- Semantic structural partition cache reports reuse counts so avoided rebuilds are visible in diagnostics.
- Exact semantic Q-mesh raycasts now combine partition broad-phase filtering with source-mesh narrow-phase hits; callers receive the resolved point, normal, node, and copied semantic metadata.
- Editor structural catalog stamps route through cached Q-mesh placement raycasts, while generic create placement continues to use the accurate mesh/ground fallback.
- Camera-plot placement now raycasts exact mesh geometry rather than stopping at renderable AABBs.
- Liminal Hall has semantic structural smoke coverage for authored structural rows.
- Compiled CSG fragments retain stable structural M/P/Q/I metadata, including their authored source relation, so generated geometry participates in semantic trace feeds instead of becoming anonymous render-only meshes.

## Developer Fulfillment Workflow

Use this flow for every remaining structural primitive increment:

1. State the specific runtime or authoring problem in this file before coding.
2. Add or update a smoke test that proves the behavior is missing.
3. Run the focused target and confirm the test fails for the expected reason.
4. Implement the smallest production change that makes the test pass.
5. Run the focused structural suite.
6. Update this plan with the result and next recommended step.
7. Commit source changes separately from documentation-only bookkeeping when useful.

Focused structural suite:

```powershell
ctest --test-dir build\dev-msvc -C RelWithDebInfo --output-on-failure -R "RawIron\.(Trace\.CompetitiveWeaponSimulatorSmoke|SceneUtilities\.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)|MultiplayerSandbox\.StructuralTraceFeedSmoke)"
```

Use a broader game-specific suite when editing Liminal Hall or other project data.

## Active Backlog

### 1. Harden the Runtime Structural Contract

- Replace hot-path string comparisons for semantic fields with compact runtime ids or handles, while retaining strings for authoring and diagnostics.
- Define an explicit editor-only metadata boundary and a smaller runtime-shippable structural record.
- Add compatibility coverage for legacy CSV rows, semantic CSV rows, and the next richer authored format.

Done means old levels remain loadable, new M/P/Q/I ownership survives serialization, and measured runtime queries can use compact fields.

### 2. Complete the Remaining Query Consumers

- Build Q-mesh candidate feeds for pseudo-raytracing/raycast consumers.
- Add game-runtime interaction/carrying filters for interaction-capable Q-meshes and semantic roles, with target ownership rules that avoid treating every structural surface as an activatable target.
- Expose visibility-role diagnostics before making visibility data affect runtime culling.
- Report source, emitted, filtered, and query-candidate counts from each newly wired consumer.

Done means every additional consumer proves its filtering benefit before it adds expensive narrow-phase work.

### 3. Deliver Editor-First Structural Authoring

- Add inspector controls for role, region, operation, collision, visibility, navigation, rebuild scope, and M/P/Q/I channel flags.
- Add overlays for semantic regions, query/collision participants, visibility candidates, and dirty rebuild bounds.
- Resolve a generated CSG fragment selection to its authored structural owner.
- Warn on missing ids, degenerate bounds, unknown policies, and disabled required channels.

Done means designers can author and diagnose structural content without editing CSV by hand.

### 4. Define Runtime Packaging

- Specify which authored fields ship, which are stripped, and which are compressed into runtime partitions.
- Validate generated partition artifacts during the build.
- Add loading coverage for runtime-only structural data while preserving development diagnostics.

Done means shipping builds carry validated derived data without the full editor-facing note graph.

## Deferred Until Metrics Justify It

- Experiment with configurable BSP split policies only after the existing candidate metrics identify a scene that needs them.

The next recommended implementation is a game-runtime interaction gate that compares an interaction target's distance with the nearest `QueryMesh + Interaction` hit, then reports a clear blocked/eligible result without changing legacy target selection behavior.

## Quality Gates

Structural primitive work is acceptable only when:

- It preserves authored ownership from brush to generated mesh, collider, query shape, and semantic record.
- It has a failing test before production code changes.
- It keeps old structural files compatible.
- It reports metrics for any new optimization path.
- It avoids broad full-scene rebuilds unless explicitly required.
- It proves performance improvements with candidate counts or timing before adding more computation.
- It keeps disposable implementation notes out of the repository.

## Source Map

- `Source/RawIron.Core/include/RawIron/Scene/Components.h`: structural metadata, policies, M/P/Q/I types, channel and query-purpose helpers.
- `Source/RawIron.SceneUtilities/include/RawIron/Scene/StructuralBrush.h`: structural brush spawn surface.
- `Source/RawIron.SceneUtilities/src/StructuralBrush.cpp`: default ownership and channel population.
- `Source/RawIron.SceneUtilities/src/StructuralAssemblyIO.cpp`: structural CSV import.
- `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`: semantic BSP wrapper API.
- `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`: semantic partition implementation.
- `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneSubtreeColliders.h`: subtree trace collider options.
- `Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp`: collider filtering and semantic tag stamping.
- `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneStructuralTraceFeed.h`: structural trace feed API.
- `Source/RawIron.SceneUtilities/src/SceneStructuralTraceFeed.cpp`: structural trace feed implementation and metrics.
- `Tests/StructuralBrushMetadataSmoke.cpp`: metadata and M/P/Q/I coverage.
- `Tests/SemanticStructuralPartitionSmoke.cpp`: partition filtering, metrics, cache, and picking coverage.
- `Tests/SceneSubtreeCollidersSmoke.cpp`: collider policy, Q-mesh, and tag coverage.
- `Tests/SceneStructuralTraceFeedSmoke.cpp`: trace feed, trace scene, filter reason, and ratio coverage.
- `Tests/MultiplayerSandboxStructuralTraceFeedSmoke.cpp`: real movement-consumer feed merge and metric coverage.
- `Tests/CompetitiveWeaponSimulatorSmoke.cpp`: rewind weapon resolution, structural world obstruction, and direction-validation coverage.
