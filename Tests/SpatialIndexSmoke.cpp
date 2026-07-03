#include "RawIron/Spatial/SpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

bool NearlyEqual(const float lhs, const float rhs, const float tolerance = 1e-4f) {
    return std::fabs(lhs - rhs) <= tolerance;
}

ri::spatial::Aabb MakeBox(const float minX, const float maxX) {
    return ri::spatial::Aabb{
        .min = {minX, 0.0f, 0.0f},
        .max = {maxX, 1.0f, 1.0f},
    };
}

} // namespace

int main() {
    // Source layout: index 1 (empty id) and index 4 (empty bounds) must be filtered at rebuild;
    // the two "dup" entries share an id but must stay individually addressable via source indices.
    std::vector<ri::spatial::SpatialEntry> entries = {
        {.id = "a", .bounds = MakeBox(0.0f, 1.0f)},
        {.id = "", .bounds = MakeBox(10.0f, 11.0f)},
        {.id = "dup", .bounds = MakeBox(2.0f, 3.0f)},
        {.id = "dup", .bounds = MakeBox(4.0f, 5.0f)},
        {.id = "b", .bounds = ri::spatial::MakeEmptyAabb()},
        {.id = "c", .bounds = MakeBox(6.0f, 7.0f)},
    };

    ri::spatial::BspSpatialIndex index(entries);
    if (index.Empty() || index.EntryCount() != 4) {
        return EXIT_FAILURE;
    }

    const ri::spatial::Aabb everything{{-1.0f, -1.0f, -1.0f}, {8.0f, 2.0f, 2.0f}};

    std::vector<std::size_t> boxSources = index.QueryBoxSourceIndices(everything);
    std::sort(boxSources.begin(), boxSources.end());
    if (boxSources != std::vector<std::size_t>{0, 2, 3, 5}) {
        return EXIT_FAILURE;
    }

    const std::vector<std::string> boxIds = index.QueryBox(everything);
    if (boxIds.size() != 4
        || std::count(boxIds.begin(), boxIds.end(), "dup") != 2
        || std::count(boxIds.begin(), boxIds.end(), "a") != 1
        || std::count(boxIds.begin(), boxIds.end(), "c") != 1) {
        return EXIT_FAILURE;
    }

    // Repeat queries must return identical results (validates the reusable visit-epoch buffer).
    for (int repeat = 0; repeat < 3; ++repeat) {
        std::vector<std::size_t> again = index.QueryBoxSourceIndices(everything);
        std::sort(again.begin(), again.end());
        if (again != boxSources) {
            return EXIT_FAILURE;
        }
    }

    // Targeted box query only returns the second "dup" entry.
    std::vector<std::size_t> narrowSources = index.QueryBoxSourceIndices(MakeBox(4.25f, 4.75f));
    if (narrowSources != std::vector<std::size_t>{3}) {
        return EXIT_FAILURE;
    }

    const std::vector<ri::spatial::SpatialRayCandidate> rayCandidates =
        index.QueryRayCandidates({-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 20.0f);
    if (rayCandidates.size() != 4) {
        return EXIT_FAILURE;
    }
    std::vector<ri::spatial::SpatialRayCandidate> sortedRay = rayCandidates;
    std::sort(sortedRay.begin(), sortedRay.end(),
              [](const ri::spatial::SpatialRayCandidate& lhs, const ri::spatial::SpatialRayCandidate& rhs) {
                  return lhs.distance < rhs.distance;
              });
    if (sortedRay[0].sourceIndex != 0 || !NearlyEqual(sortedRay[0].distance, 1.0f)
        || sortedRay[1].sourceIndex != 2 || !NearlyEqual(sortedRay[1].distance, 3.0f)
        || sortedRay[2].sourceIndex != 3 || !NearlyEqual(sortedRay[2].distance, 5.0f)
        || sortedRay[3].sourceIndex != 5 || !NearlyEqual(sortedRay[3].distance, 7.0f)) {
        return EXIT_FAILURE;
    }

    const std::vector<std::string> rayIds = index.QueryRay({-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 20.0f);
    if (rayIds.size() != 4 || std::count(rayIds.begin(), rayIds.end(), "dup") != 2) {
        return EXIT_FAILURE;
    }

    // Ray range clipping: far distance of 2.5 only reaches "a".
    const std::vector<ri::spatial::SpatialRayCandidate> shortRay =
        index.QueryRayCandidates({-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2.5f);
    if (shortRay.size() != 1 || shortRay[0].sourceIndex != 0) {
        return EXIT_FAILURE;
    }

    const ri::spatial::SpatialIndexMetrics metrics = index.Metrics();
    if (metrics.rebuildCount != 1
        || metrics.lastRebuildEntryCount != 4
        || metrics.boxQueries != 6
        || metrics.rayQueries != 3
        || metrics.boxCandidatesScanned == 0
        || metrics.rayCandidatesScanned == 0) {
        return EXIT_FAILURE;
    }

    index.ResetMetrics();
    const ri::spatial::SpatialIndexMetrics resetMetrics = index.Metrics();
    if (resetMetrics.boxQueries != 0
        || resetMetrics.rayQueries != 0
        || resetMetrics.boxCandidatesScanned != 0
        || resetMetrics.rayCandidatesScanned != 0
        || resetMetrics.rebuildCount != 1) {
        return EXIT_FAILURE;
    }

    // Rebuild with fresh entries: source indices refer to the new vector.
    index.Rebuild({
        {.id = "solo", .bounds = MakeBox(0.0f, 2.0f)},
    });
    if (index.EntryCount() != 1
        || index.QueryBoxSourceIndices(MakeBox(0.5f, 1.5f)) != std::vector<std::size_t>{0}) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
