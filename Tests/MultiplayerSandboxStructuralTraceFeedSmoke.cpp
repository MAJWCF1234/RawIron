#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxWorld.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] bool HasStructuralBrushTag(const ri::trace::TraceCollider& collider) {
    constexpr std::string_view kStructuralBrushPrefix = "structural.brush:";
    for (const std::string& tag : collider.simulationTags) {
        if (std::string_view(tag).starts_with(kStructuralBrushPrefix)) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return EXIT_FAILURE;
    }

    const ri::games::multiplayersandbox::World world =
        ri::games::multiplayersandbox::BuildWorld("MultiplayerSandboxStructuralTraceFeedSmoke", argv[1]);
    const ri::scene::StructuralTraceSceneFeedMetrics& metrics = world.structuralTraceFeedMetrics;
    if (world.brushHallRoot == ri::scene::kInvalidHandle
        || metrics.sourceStructuralBrushCount == 0
        || metrics.colliderCount == 0
        || metrics.filteredStructuralBrushCount == 0
        || metrics.collisionPolicyFilteredCount == 0
        || metrics.staticColliderCount != metrics.colliderCount
        || metrics.structuralStaticColliderCount != metrics.colliderCount) {
        return EXIT_FAILURE;
    }

    std::size_t taggedStructuralColliderCount = 0;
    for (const ri::trace::TraceCollider& collider : world.colliders) {
        if (HasStructuralBrushTag(collider)) {
            ++taggedStructuralColliderCount;
        }
    }
    return taggedStructuralColliderCount == metrics.colliderCount ? EXIT_SUCCESS : EXIT_FAILURE;
}
