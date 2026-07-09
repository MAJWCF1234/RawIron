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
- Structural brush metadata exposes a deterministic signature for cache invalidation and cheap change detection.
- Semantic structural partition cache uses structural-only scene signatures to auto-refresh after brush metadata edits without rebuilding for non-structural scene churn.
- Semantic structural partition cache exposes a non-mutating rebuild check for editor/debug dirty-state reporting.
- Semantic structural partition cache reports reuse counts so avoided rebuilds are visible in diagnostics.
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
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke|SceneStructuralTraceFeedSmoke)" --output-on-failure
```

Use a broader game-specific suite when editing Liminal Hall or other project data.

## Remaining Work

### Phase 1: Harden the Structural Primitive Contract

- Add compact integer ids or handles for hot-path semantic fields so trace and partition queries do not repeatedly compare strings.
- Separate editor-only information fields from runtime-shippable metadata.
- Add serialization tests for old CSV rows, semantic CSV rows, and future richer authored formats.

Exit criteria:

- Old structural CSV assets still load.
- New semantic rows preserve M/P/Q/I ownership.
- Runtime query paths can avoid string-heavy filtering in measured hot loops.

### Phase 2: Make Semantic BSP a Real Partition Family

- [x] Keep `BspSpatialIndex` as the first broad-phase implementation, but wrap it behind partition-specific builders (per-region `RegionSubpartition` builders inside `SemanticStructuralPartition::Rebuild`).
- [x] Add per-region subpartitions for large authored areas (one spatial tree per authored region plus a regionless bucket; region-scoped queries touch only their own tree).
- [x] Add dirty-region rebuild tracking for geometry and dependency edits (content signatures over ids/bounds keep unchanged trees on rebuild; `lastRebuildSubpartitionsReused` / `lastRebuildSubpartitionsRebuilt` expose locality).
- [x] Add candidate-count diagnostics comparing unfiltered BSP queries with semantic-filtered queries (`boxCandidatesMatched` / `rayCandidatesMatched` vs `boxCandidatesScanned` / `rayCandidatesScanned`).
- [ ] Add configurable split policy experiments only after candidate metrics justify them.

Exit criteria:

- Trace, visibility, edit, semantic, and render feeds can share authored structural ownership without sharing one overloaded tree. — Met for semantic queries: each region owns its tree and the regionless bucket is isolated.
- A single brush edit identifies affected partitions without a full-scene rebuild by default. — Met: moving one brush re-splits only its region's subpartition (covered in `SemanticStructuralPartitionSmoke`).

Result (2026-07-03): Phase 2 landed. `SemanticStructuralPartition` now builds a family of per-region subpartitions with content-signature reuse, region-scoped query routing, and matched-vs-scanned metrics; the cache rebuilds in place to inherit reuse. Next recommended step: Phase 3 — route a real movement or ballistics consumer through the structural trace feed with early semantic filtering and report candidate savings.

### Phase 3: Feed Performance-Critical Systems

- [x] Route a real movement consumer through the structural trace feed: the Multiplayer Sandbox merges its generated structural hall feed into the same trace scene used by first-person movement, logs source/emitted/filtered counts and filter reasons, and keeps a detail-only perforated visual divider out of blocking traces.
- Route ballistics through the structural trace feed where structural metadata can filter candidates early.
- Add pseudo-raytracing candidate feeds that can request only Q-mesh trace/raycast participants.
- Add editor placement queries that use Q-mesh placement participants and I-layer host rules.
- Add culling diagnostics using visibility roles before enabling runtime culling decisions.
- Add carrying/interaction filters that can query interaction-capable Q-meshes and semantic roles.

Exit criteria:

- Each consumer reports source candidate count, filtered candidate count, emitted count, and filter reasons.
- The system proves performance savings before adding heavier calculations.

Result (2026-07-09): CSG assembly fragments now keep generated structural ownership, and `StructuralTraceColliderFeedResult` allows a filtered structural subtree to be merged into a larger gameplay trace scene without dropping metrics. The Multiplayer Sandbox movement trace uses that path; its smoke test proves the emitted colliders retain semantic tags and that the detail-only perforated divider is filtered. Next recommended step: route the same feed through a ballistics or interaction consumer and make its candidate reduction visible in runtime diagnostics.

### Phase 4: Editor Authoring Experience

- Add inspector controls for role, region, operation, collision, visibility, navigation, rebuild scope, and M/P/Q/I channel flags.
- Add overlays for semantic regions, query participants, collision participants, visibility candidates, and dirty rebuild bounds.
- Make generated fragments selectable through their owning structural primitive.
- Add authoring warnings for missing ids, degenerate bounds, unknown policies, and disabled required channels.

Exit criteria:

- Designers can build on the system without editing CSV by hand.
- Debug overlays make performance behavior visible instead of magical.

### Phase 5: Runtime Packaging

- Define what authored structural data ships, what is stripped, and what is compressed into runtime partitions.
- Add build output validation for structural partition artifacts.
- Add loading tests for runtime-only partition data.
- Keep editor diagnostics available in development builds without bloating release builds.

Exit criteria:

- Shipped builds keep the fast derived data, not the entire editor-facing note graph.
- Runtime artifacts can be validated independently of the editor.

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

## Superseded Notes

The following implementation notes were consolidated into this plan and should not be recreated as separate plan files:

- `2026-06-07-liminal-hall-material-modernization.md`
- `2026-06-19-semantic-structural-brush-partition.md`
- `2026-06-20-semantic-partition-query-purpose.md`
- `2026-06-20-semantic-partition-query-purpose-metrics.md`
- `2026-06-20-structural-query-purpose-gates.md`
- `2026-06-20-structural-trace-collider-feed.md`
- `2026-06-20-structural-trace-collider-purpose-tags.md`
- `2026-06-20-structural-trace-scene-feed.md`
- `2026-06-20-structural-trace-scene-feed-result.md`
- `2026-06-20-structural-trace-feed-filter-metrics.md`
- `2026-06-20-structural-trace-feed-filter-reasons.md`
- `2026-06-20-structural-trace-feed-efficiency-ratios.md`
