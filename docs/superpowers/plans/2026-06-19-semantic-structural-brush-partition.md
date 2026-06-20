# Semantic Structural Brush Partition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish the first working semantic structural brush slice by preserving brush ownership and semantic metadata from structural brush spawn options into scene nodes, then parsing optional semantic columns from structural CSV rows.

**Architecture:** Keep existing structural mesh generation and `BspSpatialIndex` behavior unchanged. Add small metadata structs to existing scene/structural brush types so generated nodes retain brush id, role, region, operation, and policies; later partition builders can consume that metadata without reverse-engineering node names.

**Tech Stack:** C++20, CMake, RawIron.Core scene graph, RawIron.SceneUtilities structural brush spawning, CTest smoke executables.

---

## File Structure

- Modify: `Source/RawIron.Core/include/RawIron/Scene/Components.h`
  Add `StructuralBrushOperation`, `StructuralBrushSemanticRole`, policy enums, and a compact `StructuralBrushMetadata` value type.
- Modify: `Source/RawIron.Core/src/Scene.cpp`
  Add `ToString(...)` helpers for the new enums so diagnostics can print stable names.
- Modify: `Source/RawIron.Core/include/RawIron/Scene/Scene.h`
  Add `StructuralBrushMetadata structuralBrush` to `Node`.
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/StructuralBrush.h`
  Add `StructuralBrushMetadata metadata` to `StructuralBrushSpawnOptions`.
- Modify: `Source/RawIron.SceneUtilities/src/StructuralBrush.cpp`
  Copy `options.metadata` onto the spawned scene node.
- Modify: `Source/RawIron.SceneUtilities/src/StructuralAssemblyIO.cpp`
  Parse optional semantic CSV columns after the current material columns and write them into `brush.metadata`.
- Modify: `Source/RawIron.SceneUtilities/CMakeLists.txt`
  Register a lightweight smoke test when `RAWIRON_BUILD_TESTS` is enabled.
- Create: `Tests/StructuralBrushMetadataSmoke.cpp`
  Verify defaults, explicit spawn metadata preservation, and semantic CSV parsing.

## Task 1: Preserve Structural Brush Metadata On Spawn

**Files:**
- Modify: `Source/RawIron.Core/include/RawIron/Scene/Components.h`
- Modify: `Source/RawIron.Core/src/Scene.cpp`
- Modify: `Source/RawIron.Core/include/RawIron/Scene/Scene.h`
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/StructuralBrush.h`
- Modify: `Source/RawIron.SceneUtilities/src/StructuralBrush.cpp`
- Modify: `Source/RawIron.SceneUtilities/CMakeLists.txt`
- Create: `Tests/StructuralBrushMetadataSmoke.cpp`

- [x] **Step 1: Write the failing smoke test**

```cpp
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/StructuralBrush.h"

#include <cstdlib>
#include <string>

int main() {
    ri::scene::Scene scene{"StructuralBrushMetadataSmoke"};
    const int root = scene.CreateNode("Root");

    ri::scene::StructuralBrushSpawnOptions options{};
    options.nodeName = "SemanticWall";
    options.structuralType = "box";
    options.parent = root;
    options.metadata.brushId = "wall_a";
    options.metadata.role = ri::scene::StructuralBrushSemanticRole::Wall;
    options.metadata.region = "atrium";
    options.metadata.operation = ri::scene::StructuralBrushOperation::Solid;
    options.metadata.collision = ri::scene::StructuralBrushCollisionPolicy::Player;
    options.metadata.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder;
    options.metadata.navigation = ri::scene::StructuralBrushNavigationPolicy::Blocker;
    options.metadata.rebuildScope = ri::scene::StructuralBrushRebuildScope::Local;

    const int node = ri::scene::AddStructuralBrushNode(scene, options);
    if (node == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }

    const ri::scene::StructuralBrushMetadata& metadata = scene.GetNode(node).structuralBrush;
    if (metadata.brushId != "wall_a"
        || metadata.region != "atrium"
        || metadata.role != ri::scene::StructuralBrushSemanticRole::Wall
        || metadata.collision != ri::scene::StructuralBrushCollisionPolicy::Player
        || metadata.visibility != ri::scene::StructuralBrushVisibilityPolicy::Occluder
        || metadata.navigation != ri::scene::StructuralBrushNavigationPolicy::Blocker
        || metadata.rebuildScope != ri::scene::StructuralBrushRebuildScope::Local) {
        return EXIT_FAILURE;
    }

    if (ri::scene::ToString(metadata.role) != "wall"
        || ri::scene::ToString(metadata.collision) != "player"
        || ri::scene::ToString(metadata.visibility) != "occluder") {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
```

- [x] **Step 2: Register and run the test to verify it fails**

Run:

```powershell
cmake --build build --target StructuralBrushMetadataSmoke
```

Expected: build fails because `StructuralBrushMetadata` and related enums do not exist yet.

- [x] **Step 3: Add minimal metadata types and spawn wiring**

Add enum/string helpers in `Components.h` and `Scene.cpp`, add `StructuralBrushMetadata structuralBrush{}` to `Node`, add `StructuralBrushMetadata metadata{}` to `StructuralBrushSpawnOptions`, and set `scene.GetNode(nodeHandle).structuralBrush = options.metadata;` after node creation in `AddStructuralBrushNode`.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build --target StructuralBrushMetadataSmoke
.\build\Tests\StructuralBrushMetadataSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.Core/include/RawIron/Scene/Components.h Source/RawIron.Core/include/RawIron/Scene/Scene.h Source/RawIron.Core/src/Scene.cpp Source/RawIron.SceneUtilities/include/RawIron/Scene/StructuralBrush.h Source/RawIron.SceneUtilities/src/StructuralBrush.cpp Source/RawIron.SceneUtilities/CMakeLists.txt Tests/StructuralBrushMetadataSmoke.cpp
git commit -m "feat: preserve structural brush metadata"
```

## Task 2: Parse Optional Semantic Columns From Structural CSV

**Files:**
- Modify: `Source/RawIron.SceneUtilities/src/StructuralAssemblyIO.cpp`
- Modify: `Tests/StructuralBrushMetadataSmoke.cpp`

- [x] **Step 1: Extend the smoke test with CSV parsing behavior**

Add a temporary CSV row with existing columns plus optional semantic columns:

```cpp
const std::filesystem::path csvPath =
    std::filesystem::temp_directory_path() / "rawiron_structural_semantic_smoke.csv";
{
    std::ofstream csv(csvPath);
    csv << "# header\n";
    csv << "SemanticFloor,plane,0,0,0,2,1,2,0.7,0.7,0.7,lit,-,1,1,-90,0,0,,,,,,standard,MetalRough,-,-,-,-,0,0,0,1,0,"
           "floor,atrium,solid,solid,ignored,walkable,local\n";
}

ri::scene::Scene imported{"ImportedSemanticAssembly"};
const int importedRoot = imported.CreateNode("Root");
const ri::scene::StructuralAssemblySpawnResult result =
    ri::scene::SpawnStructuralAssemblyFromCsv(imported, csvPath, {.parent = importedRoot});
std::filesystem::remove(csvPath);

if (result.spawnedCount != 1 || imported.NodeCount() != 2) {
    return EXIT_FAILURE;
}
const ri::scene::StructuralBrushMetadata& importedMetadata = imported.GetNode(1).structuralBrush;
if (importedMetadata.brushId != "SemanticFloor"
    || importedMetadata.role != ri::scene::StructuralBrushSemanticRole::Floor
    || importedMetadata.region != "atrium"
    || importedMetadata.navigation != ri::scene::StructuralBrushNavigationPolicy::Walkable) {
    return EXIT_FAILURE;
}
```

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build --target StructuralBrushMetadataSmoke
.\build\Tests\StructuralBrushMetadataSmoke.exe
```

Expected: test fails because semantic CSV columns are ignored.

- [x] **Step 3: Add parsers for optional trailing columns**

Add lowercase parsers in `StructuralAssemblyIO.cpp` for role, operation, collision, visibility, navigation, and rebuild scope. Use conservative defaults when a column is missing or unknown.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build --target StructuralBrushMetadataSmoke
.\build\Tests\StructuralBrushMetadataSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/src/StructuralAssemblyIO.cpp Tests/StructuralBrushMetadataSmoke.cpp
git commit -m "feat: parse structural brush semantic csv metadata"
```

## Task 3: Add Metadata-Aware Structural Partition Wrapper

**Files:**
- Create: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Create: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Source/RawIron.SceneUtilities/CMakeLists.txt`
- Create: `Tests/SemanticStructuralPartitionSmoke.cpp`

- [x] **Step 1: Write the failing smoke test**

```cpp
#include "RawIron/Scene/SemanticStructuralPartition.h"

#include <cstdlib>

int main() {
    ri::scene::SemanticStructuralPartition partition;

    ri::scene::StructuralBrushMetadata wall{};
    wall.brushId = "wall_a";
    wall.role = ri::scene::StructuralBrushSemanticRole::Wall;
    wall.region = "atrium";
    wall.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder;

    ri::scene::StructuralBrushMetadata floor{};
    floor.brushId = "floor_a";
    floor.role = ri::scene::StructuralBrushSemanticRole::Floor;
    floor.region = "atrium";
    floor.navigation = ri::scene::StructuralBrushNavigationPolicy::Walkable;

    partition.Rebuild({
        {.id = "wall_fragment", .bounds = {{-1.0f, 0.0f, -1.0f}, {1.0f, 3.0f, 1.0f}}, .metadata = wall},
        {.id = "floor_fragment", .bounds = {{-4.0f, -0.1f, -4.0f}, {4.0f, 0.1f, 4.0f}}, .metadata = floor},
    });

    const auto hits = partition.QueryBox({{-2.0f, -0.2f, -2.0f}, {2.0f, 0.2f, 2.0f}});
    if (hits.size() != 2) {
        return EXIT_FAILURE;
    }

    const auto floorHits = partition.QueryBox(
        {{-2.0f, -0.2f, -2.0f}, {2.0f, 0.2f, 2.0f}},
        {.role = ri::scene::StructuralBrushSemanticRole::Floor});
    if (floorHits.size() != 1
        || floorHits[0].entry == nullptr
        || floorHits[0].entry->metadata.brushId != "floor_a"
        || floorHits[0].entry->metadata.navigation != ri::scene::StructuralBrushNavigationPolicy::Walkable) {
        return EXIT_FAILURE;
    }

    const ri::scene::SemanticStructuralPartitionMetrics metrics = partition.Metrics();
    if (metrics.entryCount != 2 || metrics.regionCount != 1 || metrics.roleCounts.floor != 1 || metrics.roleCounts.wall != 1) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
```

- [x] **Step 2: Register and run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartition.h` does not exist.

- [x] **Step 3: Add the partition wrapper**

Create a `SemanticStructuralPartition` class that stores entries by id, builds a private `ri::spatial::BspSpatialIndex` from entry bounds, and returns metadata-bearing hits from box queries. Add metrics for entry count, region count, and role counts.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Source/RawIron.SceneUtilities/CMakeLists.txt Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: add semantic structural partition wrapper"
```

## Task 4: Build Partition Entries From Scene Brush Nodes

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`

- [x] **Step 1: Extend the smoke test with scene-fed entries**

The test spawns a structural brush node with `brush.metadata.brushId = "scene_wall"`, calls `BuildSemanticStructuralPartitionEntries(scene)`, and verifies the resulting partition can query the entry by region.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `BuildSemanticStructuralPartitionEntries` is not defined.

- [x] **Step 3: Add the scene-entry builder**

`BuildSemanticStructuralPartitionEntries` scans scene nodes, skips nodes without `structuralBrush.brushId`, computes tight mesh world AABBs with `TryComputeMeshNodeWorldAabb`, and emits `SemanticStructuralPartitionEntry` values carrying node name, bounds, and metadata.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: feed semantic partition from scene brushes"
```

## Task 5: Give Default-Spawns A Stable Brush Id

**Files:**
- Modify: `Source/RawIron.SceneUtilities/src/StructuralBrush.cpp`
- Modify: `Tests/StructuralBrushMetadataSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with default ownership**

The test spawns `DefaultOwnedBrush` without explicit metadata and verifies the spawned node gets `structuralBrush.brushId == "DefaultOwnedBrush"` with conservative default role and collision policy.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target StructuralBrushMetadataSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\StructuralBrushMetadataSmoke.exe
```

Expected: executable exits non-zero because the default brush id is empty.

- [x] **Step 3: Add fallback brush id assignment**

In `AddStructuralBrushNode`, copy `options.metadata`, set `metadata.brushId = options.nodeName` when the brush id is empty, then store the metadata on the scene node.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target StructuralBrushMetadataSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\StructuralBrushMetadataSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/src/StructuralBrush.cpp Tests/StructuralBrushMetadataSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: give structural brush spawns default ownership ids"
```

## Task 6: Add Filtered Ray Queries To Semantic Partition

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with ray queries**

The test queries a wall by ray using `.visibility = Occluder`, then queries a scene-fed brush by ray using `.region = "scene_region"`.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartition::QueryRay` is not defined.

- [x] **Step 3: Add the ray-query wrapper**

`QueryRay(origin, direction, far, query)` delegates to `BspSpatialIndex::QueryRay`, resolves ids through `FindEntry`, and applies the same semantic filter used by `QueryBox`.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: add semantic structural partition ray queries"
```

## Task 7: Expose Semantic Partition Query Metrics

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with query metrics**

The test verifies `Metrics()` reports `boxQueries == 2`, `rayQueries == 1`, and non-zero scanned candidate counts after semantic box and ray queries.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionMetrics` does not expose query counter fields.

- [x] **Step 3: Populate metrics from the underlying BSP**

Add `boxQueries`, `rayQueries`, `boxCandidatesScanned`, and `rayCandidatesScanned` to `SemanticStructuralPartitionMetrics`. In `SemanticStructuralPartition::Metrics()`, copy those values from `index_.Metrics()`.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: expose semantic structural partition query metrics"
```

## Task 8: Sort Ray Hits By Distance

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with two ray occluders**

The test adds a second wall behind the first, expects `QueryRay(..., visibility=Occluder)` to return two hits, and verifies the near wall appears first with a smaller distance.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionHit` does not expose a `distance` field.

- [x] **Step 3: Add hit distances and nearest-first sorting**

Add `float distance` to `SemanticStructuralPartitionHit`. In `QueryRay`, compute the candidate AABB hit distance with `ri::spatial::IntersectRayAabb`, store it on the hit, and sort hits by ascending distance.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: sort semantic structural ray hits"
```

## Task 9: Add Nearest Ray Hit Helper

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with nearest ray hit lookup**

The test calls `QueryNearestRay(..., visibility=Occluder)` and verifies it returns the nearer wall brush.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartition::QueryNearestRay` does not exist.

- [x] **Step 3: Add the helper**

Add `QueryNearestRay` returning `std::optional<SemanticStructuralPartitionHit>`. Implement it by calling sorted `QueryRay` and returning the first hit when present.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: add semantic structural nearest ray query"
```

## Task 10: Filter Partition Queries By Brush Owner

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with owner-id filtering**

The test calls `QueryBox(..., {.brushId = "wall_b"})` and verifies only the far-wall fragment owned by that brush is returned.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionQuery` does not expose `brushId`.

- [x] **Step 3: Add brush id filtering**

Add `std::string_view brushId` to `SemanticStructuralPartitionQuery` and reject entries whose `metadata.brushId` does not match when it is provided.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: filter semantic structural queries by brush id"
```

## Task 11: Preserve Source Scene Node Handles

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with node-handle assertions**

The test verifies `BuildSemanticStructuralPartitionEntries(scene)` stores the source node handle and that a ray hit from the scene-fed partition exposes the same handle.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionEntry` does not expose `nodeHandle`.

- [x] **Step 3: Add node handle propagation**

Add `int nodeHandle = kInvalidHandle` to `SemanticStructuralPartitionEntry`. Populate it in `BuildSemanticStructuralPartitionEntries` using the scene node index being scanned.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [x] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: preserve semantic partition source node handles"
```

## Task 12: Build Ready Semantic Partition From Scene

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with a one-call scene builder**

The test calls `BuildSemanticStructuralPartition(scene)`, then queries the built partition by ray and brush id.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `BuildSemanticStructuralPartition` does not exist.

- [x] **Step 3: Add the helper**

Add `BuildSemanticStructuralPartition(scene, indexOptions)` that creates a partition, rebuilds it from `BuildSemanticStructuralPartitionEntries(scene)`, and returns it.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: build semantic structural partition from scene"
```

## Task 13: Reset Semantic Partition Query Metrics

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with metrics reset behavior**

The test runs several box/ray queries, verifies non-zero query metrics, calls `ResetMetrics()`, then verifies entry/region counts remain while query and candidate counters return to zero.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartition::ResetMetrics` does not exist.

- [x] **Step 3: Add the reset method**

Add `void ResetMetrics() noexcept` to `SemanticStructuralPartition` and implement it by calling `index_.ResetMetrics()`.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: reset semantic structural partition metrics"
```

## Self-Review Notes

- This plan covers the first implementation slice from the design spec: semantic fields, default-compatible CSV parsing, and ownership metadata on generated nodes.
- It adds the first `SemanticStructuralPartition` wrapper after metadata exists, while deferring trace integration to a later slice.
- Old CSV rows remain valid because new columns are optional and defaulted.

## Task 14: Scene-Level Semantic Brush Picking Helper

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with scene semantic picking behavior**

The test builds a scene structural brush, casts a scene ray through it, filters by brush id, and verifies the owning pick hit preserves the source node handle and semantic role without dangling partition pointers.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `PickSemanticStructuralBrush` does not exist.

- [x] **Step 3: Add the helper**

Add `PickSemanticStructuralBrush(scene, ray, far, query, indexOptions)` that builds the semantic structural partition from the scene and returns an owning copy of the nearest entry plus distance.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: add semantic structural scene pick helper"
```

## Task 15: Filter Semantic Queries by Operation and Rebuild Scope

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with operation and rebuild-scope filters**

The test marks brushes as solid, subtractive, stamped, regional, global, and manual, then verifies box/ray queries can isolate subtractive geometry and global rebuild candidates.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionQuery` does not expose `operation` or `rebuildScope`.

- [x] **Step 3: Add the filters**

Add optional `operation` and `rebuildScope` fields to `SemanticStructuralPartitionQuery`, and update `MatchesQuery` to reject entries that do not match them.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: filter semantic structural queries by rebuild policy"
```

## Task 16: Add Operation and Rebuild-Scope Metrics

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with operation and rebuild-scope metric assertions**

The test verifies the partition reports solid, subtractive, stamped, regional, global, and manual brush counts from its semantic side tables.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionMetrics` has no operation or rebuild-scope count fields.

- [x] **Step 3: Add the metrics**

Add operation and rebuild-scope count structs to `SemanticStructuralPartitionMetrics` and populate them during side-table rebuild.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
.\build\semantic-metadata\Source\RawIron.SceneUtilities\SemanticStructuralPartitionSmoke.exe
```

Expected: executable exits with code `0`.

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: expose semantic structural rebuild metrics"
```

## Task 17: Apply Semantic Structural Metadata to Liminal Hall

**Files:**
- Modify: `Games/LiminalHall/levels/assembly.structural.csv`
- Modify: `Games/LiminalHall/Runtime/CMakeLists.txt`
- Add: `Tests/LiminalHallSemanticStructuralSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Add a Liminal Hall semantic structural smoke test**

The test builds the Liminal world, builds a semantic structural partition from the scene, verifies role/operation/rebuild metrics, and queries walkable main-floor and portal-cut structural brushes.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\liminal-semantic --target LiminalHallSemanticStructuralSmoke
.\build\liminal-semantic\Games\LiminalHall\Runtime\LiminalHallSemanticStructuralSmoke.exe Games\LiminalHall
```

Expected: executable exits non-zero because the Liminal structural CSV has not been semantically annotated.

- [x] **Step 3: Annotate Liminal Hall structural rows**

Add optional semantic columns to the structural CSV header and tag primary floor, wall, ceiling, portal, catwalk, bridge, and retaining-mass rows with semantic roles, regions, operation policy, collision policy, visibility policy, navigation policy, and rebuild scope.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\liminal-semantic -R "RawIron\.(LiminalHall\.(SemanticStructuralSmoke|MaterialAudit)|SceneUtilities\.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke))" --output-on-failure
```

Expected: all four tests pass.

- [ ] **Step 5: Commit**

```powershell
git add Games/LiminalHall/levels/assembly.structural.csv Games/LiminalHall/Runtime/CMakeLists.txt Tests/LiminalHallSemanticStructuralSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: annotate liminal hall structural semantics"
```

## Task 18: Filter Scene Trace Colliders by Structural Collision Policy

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneSubtreeColliders.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp`
- Modify: `Source/RawIron.SceneUtilities/CMakeLists.txt`
- Add: `Tests/SceneSubtreeCollidersSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Add a smoke test for semantic trace-collider filtering**

The test builds structural brushes with `Solid`, `Player`, `Query`, `None`, and `Detail` collision policies, then verifies opt-in subtree collider generation emits only blocking structural colliders.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneSubtreeCollidersSmoke
```

Expected: build fails because `SubtreeColliderBuildOptions` does not expose `respectStructuralBrushCollisionPolicy`.

- [x] **Step 3: Add the filtering option**

Add `respectStructuralBrushCollisionPolicy` to subtree collider options and skip structural brush nodes with `None`, `Query`, or `Detail` collision policy when enabled.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three SceneUtilities smoke tests pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneSubtreeColliders.h Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp Source/RawIron.SceneUtilities/CMakeLists.txt Tests/SceneSubtreeCollidersSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: filter structural trace colliders by semantics"
```

## Task 19: Stamp Structural Semantic Tags on Trace Colliders

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneSubtreeColliders.h`
- Modify: `Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp`
- Modify: `Tests/SceneSubtreeCollidersSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the subtree collider smoke test with semantic tag assertions**

The test enables semantic tag stamping and verifies generated trace colliders contain brush id, region, operation, role, collision, visibility, navigation, and rebuild-scope tags.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SceneSubtreeCollidersSmoke
```

Expected: build fails because `SubtreeColliderBuildOptions` does not expose `appendStructuralBrushSemanticTags`.

- [x] **Step 3: Add semantic tag stamping**

Add an opt-in option that appends stable `structural.*` tags from `StructuralBrushMetadata` to generated trace colliders while preserving caller-provided tags.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three SceneUtilities smoke tests pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SceneSubtreeColliders.h Source/RawIron.SceneUtilities/src/SceneSubtreeColliders.cpp Tests/SceneSubtreeCollidersSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: tag structural trace colliders with semantics"
```

## Task 20: Cache Semantic Structural Partitions

**Files:**
- Modify: `Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h`
- Modify: `Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp`
- Modify: `Tests/SemanticStructuralPartitionSmoke.cpp`
- Modify: `docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md`

- [x] **Step 1: Extend the smoke test with cache reuse and invalidation behavior**

The test builds a semantic partition cache, verifies repeated access does not rebuild, mutates the scene, verifies stale reuse until explicit invalidation, then verifies the next access rebuilds and sees the new brush.

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build\semantic-metadata --target SemanticStructuralPartitionSmoke
```

Expected: build fails because `SemanticStructuralPartitionCache` does not exist.

- [x] **Step 3: Add the cache**

Add a small explicit invalidation cache around `BuildSemanticStructuralPartition` with `GetOrRebuild`, `Invalidate`, `IsDirty`, and `RebuildCount`.

- [x] **Step 4: Run focused verification**

Run:

```powershell
ctest --test-dir build\semantic-metadata -R "RawIron.SceneUtilities.(StructuralBrushMetadataSmoke|SemanticStructuralPartitionSmoke|SceneSubtreeCollidersSmoke)" --output-on-failure
```

Expected: all three SceneUtilities smoke tests pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: cache semantic structural partitions"
```
