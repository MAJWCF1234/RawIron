# Semantic Structural Brush Partition Design

## Purpose

RawIron should evolve structural primitives into the engine's modern brush system. A structural brush should own what a classic Source-style brush owned, plus the newer information RawIron needs for fast runtime queries, procedural mesh generation, smart editor behavior, and semantic world understanding.

The goal is a hybrid system:

- Authors place and edit structural primitives as brushes.
- Structural primitives remain the source of truth for shape, material, semantics, attachments, and boolean intent.
- The engine derives fast partitions from those authored brushes for trace, picking, visibility, streaming, navigation, and localized rebuilds.

This is an upgrade to BSP's role, not a direct replacement of every existing spatial structure. Classic BSP-style partitioning remains useful as a derived acceleration structure, while semantic structural data becomes the master model.

## Current Context

RawIron already has the foundation for this direction:

- `Source/RawIron.SceneUtilities/include/RawIron/Scene/StructuralBrush.h` exposes `StructuralBrushSpawnOptions`, `AddStructuralBrushNode`, and structural assembly helpers.
- `Source/RawIron.Structural/include/RawIron/Structural/StructuralGraph.h` defines `StructuralNode` with phase classification, ids, transform, targets, booleans, primitive parameters, and runtime volume concepts.
- `Source/RawIron.Structural/include/RawIron/Structural/StructuralCompiler.h` compiles structural nodes through primitive mesh generation, convex clipping, boolean operators, modifiers, and incremental signatures.
- `Source/RawIron.SceneUtilities/src/StructuralAssemblyIO.cpp` loads `assembly.structural.csv` rows into structural brush nodes.
- `Source/RawIron.Spatial/include/RawIron/Spatial/SpatialIndex.h` provides `BspSpatialIndex`, currently an axis-aligned broad-phase tree.
- `Source/RawIron.Trace/include/RawIron/Trace/TraceScene.h` already maintains static and structural-only BSP indexes.
- `Games/LiminalHall/levels/assembly.structural.csv` proves that authored structural primitives are already useful as level construction data.

The next step is to make structural primitives own brush identity and derive richer spatial products from them.

## Recommended Architecture

Use a `Semantic Brush Graph + Derived Fast Partitions` architecture.

The authored model is a graph of structural brushes. Each brush owns durable authoring intent:

- stable id and display name
- primitive type and shape parameters
- transform and local bounds
- material assignment, material workflow, UV policy, and detail textures
- brush operation, such as `solid`, `subtract`, `intersect`, `stamp`, `merge`, or `detail`
- semantic role, such as `floor`, `wall`, `ceiling`, `pillar`, `portal`, `trim`, `cover`, `water`, `trigger`, or `decor`
- attachment anchors and host rules for doors, windows, trims, stairs, cables, props, and gameplay volumes
- collision policy, navigation policy, visibility policy, lighting policy, audio policy, and streaming policy
- dependency links to children, targets, cutters, anchors, and generated fragments

The runtime model is a set of derived partitions. Each partition is optimized for a specific query family:

- `TracePartition`: broad-phase lookup for collision, ray, overlap, sweep, and editor picking.
- `VisibilityPartition`: rooms, portals, occluders, anti-portals, and structural shell membership.
- `EditPartition`: dirty-region rebuild tracking for brush edits and dependency invalidation.
- `SemanticPartition`: cells and regions labeled by gameplay, building role, navigation meaning, and authoring ownership.
- `RenderPartition`: static batching, instancing, LOD grouping, and material sort hints.

The existing `BspSpatialIndex` can remain the initial implementation for `TracePartition`, while new metadata and region tables are added around it.

## Structural Brush Ownership

Introduce a richer internal brush record, conceptually:

```cpp
struct StructuralBrushDefinition {
    std::string id;
    std::string name;
    std::string primitiveType;
    StructuralPrimitiveOptions shape;
    Transform transform;
    StructuralBrushOperation operation;
    StructuralBrushSemantics semantics;
    StructuralBrushMaterial material;
    StructuralBrushPolicies policies;
    std::vector<StructuralBrushAnchor> anchors;
    std::vector<std::string> targetIds;
    std::vector<std::string> childIds;
};
```

This does not need to replace `StructuralNode` immediately. The first practical step is to extend `StructuralNode` and spawn/import/export paths with a small semantic payload, then introduce a typed wrapper once the behavior stabilizes.

Required semantic payload:

- `role`: primary structural meaning, for example `wall`, `floor`, `ceiling`, `pillar`, `stair`, `portal`, `detail`, `volume`.
- `region`: optional room, floor, sector, zone, or building group id.
- `operation`: brush operation if it differs from the structural node type.
- `collision`: `none`, `solid`, `query`, `player`, `detail`, or `custom`.
- `visibility`: `occluder`, `portal`, `anti_portal`, `transparent`, or `ignored`.
- `navigation`: `walkable`, `blocker`, `jump`, `cover`, `ladder`, `ignored`.
- `rebuildScope`: `local`, `region`, `global`, or `manual`.

CSV can support this with optional columns after the existing material fields. A richer file format can follow later, but the first implementation should keep current levels loading.

## Derived Partition Model

The upgraded BSP should be called a partition family, because one tree cannot serve every query well.

### TracePartition

This starts as a thin wrapper around `ri::spatial::BspSpatialIndex`.

Entries should include:

- `id`
- `bounds`
- `brushId`
- `regionId`
- `role`
- `collisionPolicy`
- `dynamic/static` flag
- `candidateMask`

The existing BSP returns ids. The wrapper can map ids back to richer metadata before narrow-phase tests.

### SemanticPartition

This is a hierarchy of labeled regions:

- world
- level
- sector
- room or exterior area
- structural cell
- brush fragment

The first version should build semantic cells from brush AABBs and explicit `region` fields. Later versions can infer rooms from walls, floors, ceilings, portals, and enclosing shell brushes.

### EditPartition

This tracks brush dependencies and dirty bounds.

When one brush changes, the system should determine:

- which generated meshes are invalid
- which cutters or targets are affected
- which trace entries need rebuilding
- which semantic cells need refresh
- which editor thumbnails or previews need invalidation

The important rule is localized recompilation. Editing one wall should not force a full structural graph compile unless dependencies require it.

### VisibilityPartition

This derives coarse visibility data from semantic roles:

- `wall`, `ceiling`, `floor`, and `pillar` may become occluder candidates.
- `portal`, `door`, `arch`, and `window` may become portal candidates.
- transparent or detail-only brushes should not contribute as occluders.

The first version can produce diagnostic data and debug overlays only. Runtime culling can use it after validation.

## Data Flow

1. Author edits a structural brush in the editor.
2. Editor updates the structural brush source record.
3. Structural graph compiler generates or reuses mesh fragments.
4. Partition builder receives compiled fragments plus original brush metadata.
5. Partition builder emits trace, semantic, edit, visibility, and render records.
6. Runtime systems query the partition best suited to their task.
7. Debug snapshots expose partition counts, candidate scans, rebuild counts, and semantic region contents.

This keeps authored intent and compiled geometry connected. Generated mesh fragments should always retain their `authoringSourceKey` or equivalent `brushId`.

## Performance Strategy

The speed target comes from doing less work and querying better data:

- Use brush signatures to skip unchanged structural compiles.
- Track dirty bounds per brush and per dependency group.
- Rebuild only affected partition leaves when practical.
- Store compact ids and metadata indexes instead of repeatedly copying strings in hot query paths.
- Keep broad-phase partitions coarse and cheap, then filter with semantic masks before narrow-phase tests.
- Separate editor partitions from runtime partitions so editor-only metadata does not slow the player.
- Add metrics for query counts, candidate counts, rebuild counts, dirty region size, and compile reuse.

The current `BspSpatialIndex` can remain simple while the wrapper and metadata path prove the concept. Deeper tree improvements should be based on measured candidate counts.

Possible later improvements:

- surface-area heuristic splits for large scenes
- loose leaves for frequently edited brushes
- per-region subtrees
- static compact arrays for shipped builds
- SIMD-friendly AABB tests
- async partition rebuild jobs

## Authoring Format

Keep `assembly.structural.csv` compatible.

Short term:

- Keep existing columns unchanged.
- Add optional trailing columns for semantic role, region, operation, collision, visibility, navigation, and rebuild scope.
- Treat missing columns as conservative defaults.
- Preserve existing import behavior for old files.

Medium term:

- Add a richer `.ri_structural.json` or `.ri_brushgraph.json` format for full brush graphs, attachments, anchors, and nested assemblies.
- Continue exporting CSV for simple levels, smoke tests, and human-readable fixtures.
- Allow the editor to save both the authored graph and derived primitive CSV during transition.

## Editor Behavior

Structural primitives should feel like smarter brushes:

- spawning creates a real structural brush record, not only a mesh node
- brush inspector edits shape, material, semantic role, policies, and operation
- selecting generated mesh fragments selects the owning brush
- moving a brush shows affected cutters, targets, anchors, and semantic cells
- placement uses anchors and semantic host rules
- debug overlays show trace cells, semantic cells, dirty regions, and visibility candidates

The first editor milestone should expose role, region, collision policy, and visibility policy. More advanced attachment rules can follow.

## Runtime Behavior

Runtime systems should consume derived data rather than raw editor details:

- Trace uses `TracePartition`.
- Visibility uses `VisibilityPartition`.
- Navigation and gameplay systems use semantic role and region metadata.
- Streaming uses region and dependency grouping.
- Diagnostics can show source brush ids for any generated runtime object.

For shipped builds, the authored graph may be stripped or compressed after derived runtime partitions are built.

## Error Handling

The compiler and partition builder should fail soft where possible:

- Unknown semantic roles fall back to `structure`.
- Unknown policies fall back to conservative runtime behavior.
- Missing brush ids are generated deterministically from source row and name.
- Invalid target links produce compile warnings.
- Degenerate meshes can still emit semantic metadata if bounds exist.
- Empty bounds are skipped from trace partitions and reported in diagnostics.

Warnings should include brush id, source file or row when available, and the subsystem that ignored the record.

## Testing

Add coverage in phases:

- Unit tests for parsing optional semantic CSV columns.
- Unit tests for mapping `StructuralNode` or brush records into partition entries.
- Unit tests for dirty dependency detection.
- Regression tests that old `assembly.structural.csv` files still load.
- Performance smoke tests that compare query candidate counts before and after metadata filtering.
- Editor smoke tests for spawning a semantic structural brush and selecting its generated fragments.
- Runtime diagnostics tests for trace and semantic partition metrics.

## First Implementation Slice

The first slice should avoid replacing the entire trace system. It should establish ownership and metadata flow.

1. Add semantic fields to the structural graph/import path with defaults.
2. Preserve semantic metadata through structural compile results.
3. Add a `StructuralPartitionEntry` type that binds bounds to brush id, role, region, and policies.
4. Build a `SemanticStructuralPartition` wrapper that uses `BspSpatialIndex` for bounds queries and stores metadata side tables.
5. Feed structural trace/debug paths from the new wrapper while keeping existing query behavior equivalent.
6. Add diagnostics for structural partition entry count, region count, role counts, query counts, and candidate counts.
7. Extend one level CSV with optional semantic columns as a fixture.

This proves the brush ownership model, keeps behavior compatible, and creates a measurable path for faster query filtering.

## Open Decisions Resolved

- The source of truth stays structural primitives, not generated meshes.
- The first partition implementation builds around existing BSP instead of replacing it.
- CSV remains compatible in the first implementation.
- A richer authored graph format is deferred until semantic fields and partition flow are proven.
- Smart brush behavior starts with role, policy, region, ownership, and local rebuild tracking before advanced attachment inference.

## Success Criteria

The design is successful when:

- a structural primitive can be identified as the owner of generated mesh, collision, trace, and semantic records
- old structural CSV files continue to load
- new optional semantic data affects partition metadata without breaking rendering
- trace behavior remains compatible while exposing richer metadata
- diagnostics show structural partition counts and query metrics
- editing or changing one brush can identify the affected local partition region
- the system has a clear path to smarter attachments, visibility, streaming, and navigation without making classic BSP the authored format
