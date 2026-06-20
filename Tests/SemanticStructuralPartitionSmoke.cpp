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

    const ri::scene::SemanticStructuralPartitionMetrics metrics = partition.Metrics();
    if (metrics.entryCount != 2
        || metrics.regionCount != 1
        || metrics.roleCounts.floor != 1
        || metrics.roleCounts.wall != 1) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
