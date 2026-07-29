#include "RawIron/World/NavMesh.h"

#include <cstdlib>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        return EXIT_FAILURE;
    }
    const std::filesystem::path workspace = argv[1];
    std::string error;
    const std::optional<ri::world::NavMesh> navmesh =
        ri::world::NavMesh::LoadDescriptor(workspace / "Games/LiminalHall/levels/assembly.navmesh", &error);
    if (!navmesh.has_value() || !error.empty() || navmesh->Regions().size() != 6U || navmesh->Links().size() != 5U) {
        return EXIT_FAILURE;
    }

    const ri::world::NavMeshPath path = navmesh->FindPath(
        {-36.0f, 2.0f, 8.0f},
        {30.0f, 4.0f, -30.0f});
    if (!path.found || path.regionIds.size() != 3U
        || path.regionIds.front() != "portal-pocket"
        || path.regionIds[1] != "plaza-core"
        || path.regionIds.back() != "parkour-chain"
        || path.waypoints.size() != 3U
        || path.traversalCost <= 0.0f) {
        return EXIT_FAILURE;
    }

    const ri::world::NavMeshPath outside = navmesh->FindPath(
        {-200.0f, 0.0f, -200.0f},
        {0.0f, 2.0f, 0.0f});
    if (outside.found || outside.diagnostic.empty()) {
        return EXIT_FAILURE;
    }

    const ri::world::NavMeshPath requiredMissing = navmesh->FindPath(
        {-36.0f, 2.0f, 8.0f},
        {30.0f, 4.0f, -30.0f},
        {.requiredFlag = "swim"});
    if (requiredMissing.found || requiredMissing.diagnostic.empty()) {
        return EXIT_FAILURE;
    }

    // Exercise immutable cached topology repeatedly. Results must remain deterministic and
    // query-specific filtering must never mutate the shared graph.
    for (int queryIndex = 0; queryIndex < 256; ++queryIndex) {
        const ri::world::NavMeshPath repeated = navmesh->FindPath(
            {-36.0f, 2.0f, 8.0f},
            {30.0f, 4.0f, -30.0f},
            {.requiredFlag = "walk", .excludedFlag = "blocked"});
        if (!repeated.found || repeated.regionIds != path.regionIds || repeated.waypoints.size() != path.waypoints.size()) {
            return EXIT_FAILURE;
        }
    }

    std::string placeholderError;
    if (ri::world::NavMesh::LoadDescriptor(
            workspace / "Games/EditorUiSmoke/levels/assembly.navmesh", &placeholderError).has_value()
        || placeholderError.empty()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
