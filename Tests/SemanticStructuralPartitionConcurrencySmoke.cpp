#include "RawIron/Scene/SemanticStructuralPartition.h"
#include "RawIron/Spatial/SpatialIndex.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kEntryCount = 512;
constexpr std::size_t kThreadCount = 8;
constexpr std::size_t kIterations = 80;
constexpr float kMaxFloat = (std::numeric_limits<float>::max)();

ri::spatial::Aabb MakeBox(const float minX, const float maxX) {
    return ri::spatial::Aabb{
        .min = {minX, 0.0f, 0.0f},
        .max = {maxX, 1.0f, 1.0f},
    };
}

bool SameMetrics(const ri::spatial::SpatialIndexMetrics& lhs,
                 const ri::spatial::SpatialIndexMetrics& rhs) {
    return lhs.rebuildCount == rhs.rebuildCount
        && lhs.lastRebuildEntryCount == rhs.lastRebuildEntryCount
        && lhs.boxQueries == rhs.boxQueries
        && lhs.rayQueries == rhs.rayQueries
        && lhs.boxCandidatesScanned == rhs.boxCandidatesScanned
        && lhs.rayCandidatesScanned == rhs.rayCandidatesScanned;
}

bool ExerciseSpatialIndex() {
    std::vector<ri::spatial::SpatialEntry> entries;
    entries.reserve(kEntryCount);
    for (std::size_t index = 0; index < kEntryCount; ++index) {
        const float minX = static_cast<float>(index * 2);
        entries.push_back(ri::spatial::SpatialEntry{
            .id = "dense_" + std::to_string(index),
            .bounds = MakeBox(minX, minX + 1.0f),
        });
    }

    ri::spatial::BspSpatialIndex index(
        std::move(entries),
        ri::spatial::SpatialIndexOptions{.maxLeafSize = 4, .maxDepth = 16});
    const ri::spatial::Aabb allBounds = MakeBox(-1.0f, static_cast<float>(kEntryCount * 2));

    std::vector<std::size_t> expectedBox = index.QueryBoxSourceIndices(allBounds);
    std::sort(expectedBox.begin(), expectedBox.end());
    if (expectedBox.size() != kEntryCount) {
        return false;
    }
    for (std::size_t expected = 0; expected < expectedBox.size(); ++expected) {
        if (expectedBox[expected] != expected) {
            return false;
        }
    }

    std::vector<ri::spatial::SpatialRayCandidate> expectedRay =
        index.QueryRayCandidates({-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2048.0f);
    std::sort(expectedRay.begin(), expectedRay.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.sourceIndex < rhs.sourceIndex; });
    if (expectedRay.size() != kEntryCount) {
        return false;
    }

    // Copy/move behavior is part of the pre-existing value-type contract. Atomic diagnostics must not
    // accidentally make an index (or a containing TraceScene/semantic partition) immovable.
    ri::spatial::BspSpatialIndex copied = index;
    const ri::spatial::SpatialIndexMetrics copiedMetrics = copied.Metrics();
    ri::spatial::BspSpatialIndex moved = std::move(copied);
    if (!SameMetrics(moved.Metrics(), copiedMetrics)
        || !copied.Empty()
        || copied.EntryCount() != 0
        || !ri::spatial::IsEmpty(copied.Bounds())) {
        return false;
    }
    const ri::spatial::SpatialIndexMetrics movedFromInitialMetrics = copied.Metrics();
    if (movedFromInitialMetrics.rebuildCount != 0
        || movedFromInitialMetrics.lastRebuildEntryCount != 0
        || movedFromInitialMetrics.boxQueries != 0
        || movedFromInitialMetrics.rayQueries != 0
        || !copied.QueryBoxSourceIndices(allBounds).empty()
        || !copied.QueryRayCandidates(
            {-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2048.0f).empty()) {
        return false;
    }
    const ri::spatial::SpatialIndexMetrics movedFromQueriedMetrics = copied.Metrics();
    if (movedFromQueriedMetrics.boxQueries != 1 || movedFromQueriedMetrics.rayQueries != 1) {
        return false;
    }
    copied.Rebuild({
        {.id = "reused_after_move_construction", .bounds = MakeBox(10.0f, 11.0f)},
    });
    if (copied.Empty()
        || copied.EntryCount() != 1
        || copied.QueryBoxSourceIndices(MakeBox(10.0f, 11.0f)) != std::vector<std::size_t>{0}) {
        return false;
    }

    if (moved.QueryBoxSourceIndices(allBounds).size() != kEntryCount) {
        return false;
    }
    const ri::spatial::SpatialIndexMetrics movedMetrics = moved.Metrics();
    ri::spatial::BspSpatialIndex moveAssigned({
        {.id = "discarded_target", .bounds = MakeBox(-10.0f, -9.0f)},
    });
    moveAssigned = std::move(moved);
    if (!SameMetrics(moveAssigned.Metrics(), movedMetrics)
        || !moved.Empty()
        || !ri::spatial::IsEmpty(moved.Bounds())
        || !moved.QueryBoxSourceIndices(allBounds).empty()
        || !moved.QueryRayCandidates(
            {-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2048.0f).empty()) {
        return false;
    }
    moved.Rebuild({
        {.id = "reused_after_move_assignment", .bounds = MakeBox(20.0f, 21.0f)},
    });
    if (moved.QueryBoxSourceIndices(MakeBox(20.0f, 21.0f)) != std::vector<std::size_t>{0}) {
        return false;
    }

    const ri::spatial::SpatialIndexMetrics beforeSelfMove = moveAssigned.Metrics();
    ri::spatial::BspSpatialIndex* self = &moveAssigned;
    moveAssigned = std::move(*self);
    if (!SameMetrics(moveAssigned.Metrics(), beforeSelfMove)
        || moveAssigned.QueryBoxSourceIndices(allBounds).size() != kEntryCount) {
        return false;
    }

    index.ResetMetrics();
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (std::size_t threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        workers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
                std::vector<std::size_t> box = index.QueryBoxSourceIndices(allBounds);
                std::sort(box.begin(), box.end());
                if (box != expectedBox) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }

                std::vector<ri::spatial::SpatialRayCandidate> ray =
                    index.QueryRayCandidates({-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2048.0f);
                std::sort(ray.begin(), ray.end(),
                          [](const auto& lhs, const auto& rhs) { return lhs.sourceIndex < rhs.sourceIndex; });
                if (ray.size() != expectedRay.size()) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                for (std::size_t candidate = 0; candidate < ray.size(); ++candidate) {
                    if (ray[candidate].sourceIndex != expectedRay[candidate].sourceIndex
                        || std::fabs(ray[candidate].distance - expectedRay[candidate].distance) > 1e-4f) {
                        failed.store(true, std::memory_order_relaxed);
                        return;
                    }
                }

                // Exercise snapshots concurrently with other writers to the diagnostic counters.
                (void)index.Metrics();
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (std::thread& worker : workers) {
        worker.join();
    }

    const std::size_t expectedQueries = kThreadCount * kIterations;
    const ri::spatial::SpatialIndexMetrics metrics = index.Metrics();
    if (failed.load(std::memory_order_relaxed)
        || metrics.boxQueries != expectedQueries
        || metrics.rayQueries != expectedQueries
        || metrics.boxCandidatesScanned < expectedQueries
        || metrics.rayCandidatesScanned < expectedQueries) {
        return false;
    }

    // A finite ray whose mathematical endpoint exceeds float range used to generate an empty segment
    // broad phase and miss an otherwise representable, reachable AABB.
    constexpr float hugeOrigin = 3.0e38f;
    ri::spatial::BspSpatialIndex huge({
        {.id = "huge", .bounds = MakeBox(3.2e38f, 3.3e38f)},
    });
    const std::vector<ri::spatial::SpatialRayCandidate> hugeHit =
        huge.QueryRayCandidates({hugeOrigin, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 3.0e38f);
    if (hugeHit.size() != 1
        || hugeHit[0].sourceIndex != 0
        || !std::isfinite(hugeHit[0].distance)
        || hugeHit[0].distance <= 0.0f) {
        return false;
    }

    // Direction magnitude is not ray meaning. Both a huge single-axis vector and a huge diagonal vector
    // must normalize to the same ray as their small scaled equivalents without overflowing LengthSquared.
    ri::spatial::BspSpatialIndex directionMagnitude({
        {.id = "axis", .bounds = MakeBox(1.0f, 2.0f)},
    });
    const auto unitAxis = directionMagnitude.QueryRayCandidates(
        {0.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 10.0f);
    const auto hugeAxis = directionMagnitude.QueryRayCandidates(
        {0.0f, 0.5f, 0.5f}, {kMaxFloat, 0.0f, 0.0f}, 10.0f);
    if (unitAxis.size() != 1
        || hugeAxis.size() != unitAxis.size()
        || std::fabs(hugeAxis[0].distance - unitAxis[0].distance) > 1e-4f) {
        return false;
    }
    if (!directionMagnitude.QueryRayCandidates(
            {0.0f, 0.5f, 0.5f}, {1e-11f, 0.0f, 0.0f}, 10.0f).empty()) {
        return false;
    }

    ri::spatial::BspSpatialIndex diagonalMagnitude({
        {
            .id = "diagonal",
            .bounds = {{1.0f, 1.0f, 0.0f}, {2.0f, 2.0f, 1.0f}},
        },
    });
    const auto unitDiagonal = diagonalMagnitude.QueryRayCandidates(
        {0.0f, 0.0f, 0.5f}, {1.0f, 1.0f, 0.0f}, 10.0f);
    const auto hugeDiagonal = diagonalMagnitude.QueryRayCandidates(
        {0.0f, 0.0f, 0.5f}, {kMaxFloat, kMaxFloat, 0.0f}, 10.0f);
    if (unitDiagonal.size() != 1
        || hugeDiagonal.size() != unitDiagonal.size()
        || std::fabs(hugeDiagonal[0].distance - unitDiagonal[0].distance) > 1e-4f) {
        return false;
    }

    // Rebuild is deliberately a separately owned writer operation. Once that writer completes, queries
    // must expose only the new source layout and bounds.
    index.Rebuild({
        {.id = "moved", .bounds = MakeBox(-100.0f, -99.0f)},
    });
    return index.QueryBoxSourceIndices(allBounds).empty()
        && index.QueryBoxSourceIndices(MakeBox(-100.0f, -99.0f)) == std::vector<std::size_t>{0};
}

bool ExerciseSemanticPartition() {
    std::vector<ri::scene::SemanticStructuralPartitionEntry> entries;
    entries.reserve(kEntryCount);
    for (std::size_t index = 0; index < kEntryCount; ++index) {
        ri::scene::StructuralBrushMetadata metadata{};
        metadata.brushId = "brush_" + std::to_string(index);
        metadata.region = (index % 2 == 0) ? "even" : "odd";
        metadata.role = (index % 2 == 0)
            ? ri::scene::StructuralBrushSemanticRole::Wall
            : ri::scene::StructuralBrushSemanticRole::Floor;
        metadata.queryMesh.raycastable = true;

        const float minX = static_cast<float>(index * 2);
        entries.push_back(ri::scene::SemanticStructuralPartitionEntry{
            .id = "semantic_" + std::to_string(index),
            .nodeHandle = static_cast<int>(index),
            .bounds = MakeBox(minX, minX + 1.0f),
            .metadata = std::move(metadata),
        });
    }

    ri::scene::SemanticStructuralPartition partition;
    partition.Rebuild(
        std::move(entries),
        ri::spatial::SpatialIndexOptions{.maxLeafSize = 4, .maxDepth = 16});
    partition.ResetMetrics();

    const ri::spatial::Aabb allBounds = MakeBox(-1.0f, static_cast<float>(kEntryCount * 2));
    const ri::scene::SemanticStructuralPartitionQuery evenRegion{.region = "even"};
    const ri::scene::SemanticStructuralPartitionQuery evenWalls{
        .role = ri::scene::StructuralBrushSemanticRole::Wall,
        .region = "even",
    };
    const ri::scene::SemanticStructuralPartitionQuery oddRegion{.region = "odd"};
    constexpr std::size_t expectedRegionEntries = kEntryCount / 2;

    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (std::size_t threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        workers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
                const auto boxHits = partition.QueryBox(allBounds, evenRegion);
                const auto rayHits = partition.QueryRay(
                    {-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2048.0f, evenWalls);
                const auto nearest = partition.QueryNearestRay(
                    {-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2048.0f, oddRegion);
                const auto evenEntries = partition.FindEntriesByRegion("even");
                if (boxHits.size() != expectedRegionEntries
                    || rayHits.size() != expectedRegionEntries
                    || !nearest.has_value()
                    || nearest->entry == nullptr
                    || nearest->entry->nodeHandle != 1
                    || evenEntries.size() != expectedRegionEntries) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                (void)partition.Metrics();
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (std::thread& worker : workers) {
        worker.join();
    }

    const std::size_t queryIterations = kThreadCount * kIterations;
    const ri::scene::SemanticStructuralPartitionMetrics metrics = partition.Metrics();
    if (failed.load(std::memory_order_relaxed)) {
        return false;
    }
    if (metrics.boxQueries != queryIterations
        || metrics.rayQueries != queryIterations * 2
        || metrics.regionScopedBoxQueries != queryIterations
        || metrics.regionScopedRayQueries != queryIterations * 2
        || metrics.boxCandidatesScanned != queryIterations * expectedRegionEntries
        || metrics.rayCandidatesScanned != queryIterations * expectedRegionEntries * 2
        || metrics.boxCandidatesMatched != queryIterations * expectedRegionEntries
        || metrics.rayCandidatesMatched != queryIterations * expectedRegionEntries * 2) {
        return false;
    }

    const auto semanticUnitAxis = partition.QueryNearestRay(
        {-1.0f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, 2048.0f, evenRegion);
    const auto semanticHugeAxis = partition.QueryNearestRay(
        {-1.0f, 0.5f, 0.5f}, {kMaxFloat, 0.0f, 0.0f}, 2048.0f, evenRegion);
    if (!semanticUnitAxis.has_value()
        || !semanticHugeAxis.has_value()
        || semanticHugeAxis->entry == nullptr
        || semanticUnitAxis->entry == nullptr
        || semanticHugeAxis->entry->nodeHandle != semanticUnitAxis->entry->nodeHandle
        || std::fabs(semanticHugeAxis->distance - semanticUnitAxis->distance) > 1e-4f) {
        return false;
    }

    ri::scene::StructuralBrushMetadata diagonalMetadata{};
    diagonalMetadata.brushId = "diagonal";
    diagonalMetadata.region = "diagonal";
    diagonalMetadata.queryMesh.raycastable = true;
    ri::scene::SemanticStructuralPartition diagonalPartition;
    diagonalPartition.Rebuild({
        {
            .id = "semantic_diagonal",
            .nodeHandle = 77,
            .bounds = {{1.0f, 1.0f, 0.0f}, {2.0f, 2.0f, 1.0f}},
            .metadata = std::move(diagonalMetadata),
        },
    });
    const auto semanticUnitDiagonal = diagonalPartition.QueryRay(
        {0.0f, 0.0f, 0.5f}, {1.0f, 1.0f, 0.0f}, 10.0f);
    const auto semanticHugeDiagonal = diagonalPartition.QueryRay(
        {0.0f, 0.0f, 0.5f}, {kMaxFloat, kMaxFloat, 0.0f}, 10.0f);
    return semanticUnitDiagonal.size() == 1
        && semanticHugeDiagonal.size() == semanticUnitDiagonal.size()
        && semanticHugeDiagonal[0].entry != nullptr
        && semanticUnitDiagonal[0].entry != nullptr
        && semanticHugeDiagonal[0].entry->nodeHandle == semanticUnitDiagonal[0].entry->nodeHandle
        && std::fabs(semanticHugeDiagonal[0].distance - semanticUnitDiagonal[0].distance) <= 1e-4f;
}

} // namespace

int main() {
    return ExerciseSpatialIndex() && ExerciseSemanticPartition() ? EXIT_SUCCESS : EXIT_FAILURE;
}
