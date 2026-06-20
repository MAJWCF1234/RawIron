#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"
#include "RawIron/Scene/SemanticStructuralPartition.h"

#include <cstdlib>
#include <filesystem>

int main(int argc, char** argv) {
    if (argc < 2) {
        return EXIT_FAILURE;
    }

    const std::filesystem::path gameRoot = argv[1];
    ri::games::liminal::World world = ri::games::liminal::BuildWorld("liminal-hall", gameRoot);
    ri::scene::SemanticStructuralPartition partition =
        ri::scene::BuildSemanticStructuralPartition(world.scene);
    const ri::scene::SemanticStructuralPartitionMetrics metrics = partition.Metrics();

    if (metrics.entryCount < 20
        || metrics.regionCount < 4
        || metrics.roleCounts.floor < 4
        || metrics.roleCounts.wall < 4
        || metrics.roleCounts.ceiling < 1
        || metrics.roleCounts.portal < 1
        || metrics.operationCounts.solid < 10
        || metrics.operationCounts.subtract < 1
        || metrics.rebuildScopeCounts.region < 8
        || metrics.rebuildScopeCounts.global < 1) {
        return EXIT_FAILURE;
    }

    const auto walkableMainHits = partition.QueryBox(
        {{-8.0f, -0.5f, -8.0f}, {8.0f, 1.0f, 12.0f}},
        {
            .role = ri::scene::StructuralBrushSemanticRole::Floor,
            .navigation = ri::scene::StructuralBrushNavigationPolicy::Walkable,
            .region = "main",
        });
    if (walkableMainHits.empty()) {
        return EXIT_FAILURE;
    }

    const auto portalHits = partition.QueryBox(
        {{-5.0f, 0.0f, -24.0f}, {5.0f, 8.0f, -19.0f}},
        {
            .operation = ri::scene::StructuralBrushOperation::Subtract,
            .role = ri::scene::StructuralBrushSemanticRole::Portal,
            .visibility = ri::scene::StructuralBrushVisibilityPolicy::Portal,
        });
    if (portalHits.size() != 1 || portalHits[0].entry == nullptr) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
