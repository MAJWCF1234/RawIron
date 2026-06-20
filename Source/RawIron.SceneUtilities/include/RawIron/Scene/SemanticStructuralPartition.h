#pragma once

#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Spatial/SpatialIndex.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::scene {

struct Ray;

struct SemanticStructuralPartitionEntry {
    std::string id;
    int nodeHandle = kInvalidHandle;
    ri::spatial::Aabb bounds;
    StructuralBrushMetadata metadata{};
};

struct SemanticStructuralPartitionQuery {
    std::optional<StructuralBrushOperation> operation{};
    std::optional<StructuralBrushSemanticRole> role{};
    std::optional<StructuralBrushCollisionPolicy> collision{};
    std::optional<StructuralBrushVisibilityPolicy> visibility{};
    std::optional<StructuralBrushNavigationPolicy> navigation{};
    std::optional<StructuralBrushRebuildScope> rebuildScope{};
    std::string_view brushId{};
    std::string_view region{};
};

struct SemanticStructuralPartitionHit {
    const SemanticStructuralPartitionEntry* entry = nullptr;
    float distance = 0.0f;
};

struct SemanticStructuralPickHit {
    SemanticStructuralPartitionEntry entry;
    float distance = 0.0f;
};

struct SemanticStructuralPartitionRoleCounts {
    std::size_t structure = 0;
    std::size_t wall = 0;
    std::size_t floor = 0;
    std::size_t ceiling = 0;
    std::size_t pillar = 0;
    std::size_t stair = 0;
    std::size_t portal = 0;
    std::size_t trim = 0;
    std::size_t cover = 0;
    std::size_t water = 0;
    std::size_t trigger = 0;
    std::size_t decor = 0;
    std::size_t volume = 0;
};

struct SemanticStructuralPartitionOperationCounts {
    std::size_t unspecified = 0;
    std::size_t solid = 0;
    std::size_t subtract = 0;
    std::size_t intersect = 0;
    std::size_t stamp = 0;
    std::size_t merge = 0;
    std::size_t detail = 0;
};

struct SemanticStructuralPartitionRebuildScopeCounts {
    std::size_t local = 0;
    std::size_t region = 0;
    std::size_t global = 0;
    std::size_t manual = 0;
};

struct SemanticStructuralPartitionMetrics {
    std::size_t entryCount = 0;
    std::size_t regionCount = 0;
    std::size_t boxQueries = 0;
    std::size_t rayQueries = 0;
    std::size_t boxCandidatesScanned = 0;
    std::size_t rayCandidatesScanned = 0;
    SemanticStructuralPartitionRoleCounts roleCounts{};
    SemanticStructuralPartitionOperationCounts operationCounts{};
    SemanticStructuralPartitionRebuildScopeCounts rebuildScopeCounts{};
};

class SemanticStructuralPartition {
public:
    void Rebuild(std::vector<SemanticStructuralPartitionEntry> entries,
                 ri::spatial::SpatialIndexOptions indexOptions = {});

    [[nodiscard]] std::vector<SemanticStructuralPartitionHit> QueryBox(
        const ri::spatial::Aabb& box,
        const SemanticStructuralPartitionQuery& query = {}) const;
    [[nodiscard]] std::vector<SemanticStructuralPartitionHit> QueryRay(
        const ri::math::Vec3& origin,
        const ri::math::Vec3& direction,
        float far,
        const SemanticStructuralPartitionQuery& query = {}) const;
    [[nodiscard]] std::optional<SemanticStructuralPartitionHit> QueryNearestRay(
        const ri::math::Vec3& origin,
        const ri::math::Vec3& direction,
        float far,
        const SemanticStructuralPartitionQuery& query = {}) const;

    [[nodiscard]] const SemanticStructuralPartitionEntry* FindEntry(std::string_view id) const;
    [[nodiscard]] SemanticStructuralPartitionMetrics Metrics() const noexcept;
    void ResetMetrics() noexcept;

private:
    [[nodiscard]] bool MatchesQuery(const SemanticStructuralPartitionEntry& entry,
                                    const SemanticStructuralPartitionQuery& query) const;
    void RebuildSideTables();

    std::vector<SemanticStructuralPartitionEntry> entries_;
    std::unordered_map<std::string, std::size_t> entryIndexById_;
    ri::spatial::BspSpatialIndex index_;
    SemanticStructuralPartitionMetrics metrics_{};
};

[[nodiscard]] std::vector<SemanticStructuralPartitionEntry> BuildSemanticStructuralPartitionEntries(
    const Scene& scene);
[[nodiscard]] SemanticStructuralPartition BuildSemanticStructuralPartition(
    const Scene& scene,
    ri::spatial::SpatialIndexOptions indexOptions = {});
[[nodiscard]] std::optional<SemanticStructuralPickHit> PickSemanticStructuralBrush(
    const Scene& scene,
    const Ray& ray,
    float far,
    const SemanticStructuralPartitionQuery& query = {},
    ri::spatial::SpatialIndexOptions indexOptions = {});

} // namespace ri::scene
