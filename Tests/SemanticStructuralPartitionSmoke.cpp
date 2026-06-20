#include "RawIron/Scene/SemanticStructuralPartition.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/StructuralBrush.h"

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
        {
            .id = "wall_fragment",
            .bounds = {{-1.0f, 0.0f, -1.0f}, {1.0f, 3.0f, 1.0f}},
            .metadata = wall,
        },
        {
            .id = "floor_fragment",
            .bounds = {{-4.0f, -0.1f, -4.0f}, {4.0f, 0.1f, 4.0f}},
            .metadata = floor,
        },
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

    const auto wallRayHits = partition.QueryRay(
        {0.0f, 1.5f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        10.0f,
        {.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder});
    if (wallRayHits.size() != 1
        || wallRayHits[0].entry == nullptr
        || wallRayHits[0].entry->metadata.brushId != "wall_a") {
        return EXIT_FAILURE;
    }

    const ri::scene::SemanticStructuralPartitionMetrics metrics = partition.Metrics();
    if (metrics.entryCount != 2
        || metrics.regionCount != 1
        || metrics.roleCounts.floor != 1
        || metrics.roleCounts.wall != 1) {
        return EXIT_FAILURE;
    }

    ri::scene::Scene scene{"SemanticPartitionSceneFeed"};
    const int root = scene.CreateNode("Root");
    ri::scene::StructuralBrushSpawnOptions brush{};
    brush.nodeName = "SceneWall";
    brush.parent = root;
    brush.structuralType = "box";
    brush.transform.position = {3.0f, 1.5f, 0.0f};
    brush.transform.scale = {2.0f, 3.0f, 1.0f};
    brush.metadata.brushId = "scene_wall";
    brush.metadata.role = ri::scene::StructuralBrushSemanticRole::Wall;
    brush.metadata.region = "scene_region";
    const int sceneWall = ri::scene::AddStructuralBrushNode(scene, brush);
    if (sceneWall == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }

    const std::vector<ri::scene::SemanticStructuralPartitionEntry> sceneEntries =
        ri::scene::BuildSemanticStructuralPartitionEntries(scene);
    if (sceneEntries.size() != 1
        || sceneEntries[0].id != "SceneWall"
        || sceneEntries[0].metadata.brushId != "scene_wall"
        || sceneEntries[0].metadata.role != ri::scene::StructuralBrushSemanticRole::Wall) {
        return EXIT_FAILURE;
    }

    ri::scene::SemanticStructuralPartition scenePartition;
    scenePartition.Rebuild(sceneEntries);
    const auto sceneHits = scenePartition.QueryBox(
        {{2.5f, 1.0f, -0.25f}, {3.5f, 2.0f, 0.25f}},
        {.region = "scene_region"});
    if (sceneHits.size() != 1 || sceneHits[0].entry->metadata.brushId != "scene_wall") {
        return EXIT_FAILURE;
    }

    const auto sceneRayHits = scenePartition.QueryRay(
        {3.0f, 1.5f, -3.0f},
        {0.0f, 0.0f, 1.0f},
        6.0f,
        {.region = "scene_region"});
    if (sceneRayHits.size() != 1 || sceneRayHits[0].entry->metadata.brushId != "scene_wall") {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
