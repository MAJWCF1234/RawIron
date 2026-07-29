#include "RawIron/World/NavMesh.h"

#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool Near(const float lhs, const float rhs) {
    return std::abs(lhs - rhs) < 0.001f;
}

} // namespace

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
        || path.waypoints.size() != 5U
        || path.visitedRegionCount == 0U
        || path.traversalCost <= 0.0f) {
        return EXIT_FAILURE;
    }
    // The old implementation routed through the plaza center. The new route exposes the
    // off-mesh exit/entry and the actual shared portal into the parkour region.
    if (!Near(path.waypoints[1].x, -28.0f)
        || !Near(path.waypoints[2].x, -22.0f)
        || !Near(path.waypoints[3].x, 22.0f)
        || !Near(path.waypoints[3].z, -30.0f)) {
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

    const ri::world::NavMeshPath budgetLimited = navmesh->FindPath(
        {-36.0f, 2.0f, 8.0f},
        {30.0f, 4.0f, -30.0f},
        {.maxVisitedRegions = 1U});
    if (budgetLimited.found || budgetLimited.visitedRegionCount != 1U
        || budgetLimited.diagnostic.find("visit limit") == std::string::npos) {
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

    const std::filesystem::path temporaryRoot =
        std::filesystem::temp_directory_path() / "RawIronNavMeshPathfindingSmoke";
    std::error_code filesystemError;
    std::filesystem::remove_all(temporaryRoot, filesystemError);
    std::filesystem::create_directories(temporaryRoot, filesystemError);
    if (filesystemError) {
        return EXIT_FAILURE;
    }
    const std::filesystem::path lPath = temporaryRoot / "l-shaped.navmesh";
    {
        std::ofstream descriptor(lPath, std::ios::trunc);
        descriptor
            << "a,0,0,0,4,1,4,1,walk\n"
            << "b,4,0,3,8,1,4,1,walk\n"
            << "c,7,0,4,8,1,8,1,walk\n"
            << "ab,a,b,1\n"
            << "ab-duplicate,a,b,1\n"
            << "bc,b,c,1\n";
    }
    const std::optional<ri::world::NavMesh> lMesh =
        ri::world::NavMesh::LoadDescriptor(lPath, &error);
    if (!lMesh.has_value()) {
        return EXIT_FAILURE;
    }
    const ri::world::NavMeshPath lRoute =
        lMesh->FindPath({1.0f, 0.5f, 1.0f}, {7.5f, 0.5f, 7.0f});
    if (!lRoute.found || lRoute.regionIds.size() != 3U || lRoute.waypoints.size() != 4U
        || !Near(lRoute.waypoints[1].x, 4.0f) || !Near(lRoute.waypoints[1].z, 4.0f)
        || !Near(lRoute.waypoints[2].x, 7.5f) || !Near(lRoute.waypoints[2].z, 4.0f)) {
        return EXIT_FAILURE;
    }

    const std::filesystem::path selfLinkPath = temporaryRoot / "self-link.navmesh";
    {
        std::ofstream descriptor(selfLinkPath, std::ios::trunc);
        descriptor
            << "a,0,0,0,4,1,4,1,walk\n"
            << "loop,a,a,1\n";
    }
    if (ri::world::NavMesh::LoadDescriptor(selfLinkPath, &error).has_value()
        || error.find("itself") == std::string::npos) {
        return EXIT_FAILURE;
    }
    std::filesystem::remove_all(temporaryRoot, filesystemError);
    return EXIT_SUCCESS;
}
