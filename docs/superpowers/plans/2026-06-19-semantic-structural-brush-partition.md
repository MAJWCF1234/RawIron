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

- [ ] **Step 5: Commit**

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

- [ ] **Step 5: Commit**

```powershell
git add Source/RawIron.SceneUtilities/include/RawIron/Scene/SemanticStructuralPartition.h Source/RawIron.SceneUtilities/src/SemanticStructuralPartition.cpp Tests/SemanticStructuralPartitionSmoke.cpp docs/superpowers/plans/2026-06-19-semantic-structural-brush-partition.md
git commit -m "feat: feed semantic partition from scene brushes"
```

## Self-Review Notes

- This plan covers the first implementation slice from the design spec: semantic fields, default-compatible CSV parsing, and ownership metadata on generated nodes.
- It adds the first `SemanticStructuralPartition` wrapper after metadata exists, while deferring trace integration to a later slice.
- Old CSV rows remain valid because new columns are optional and defaulted.
