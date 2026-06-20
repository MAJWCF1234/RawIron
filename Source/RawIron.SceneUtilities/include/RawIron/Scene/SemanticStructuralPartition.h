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

struct SemanticStructuralPartitionEntry {
    std::string id;
    ri::spatial::Aabb bounds;
    StructuralBrushMetadata metadata{};
};

struct SemanticStructuralPartitionQuery {
    std::optional<StructuralBrushSemanticRole> role{};
    std::optional<StructuralBrushCollisionPolicy> collision{};
    std::optional<StructuralBrushVisibilityPolicy> visibility{};
    std::optional<StructuralBrushNavigationPolicy> navigation{};
    std::string_view region{};
};

struct SemanticStructuralPartitionHit {
    const SemanticStructuralPartitionEntry* entry = nullptr;
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

struct SemanticStructuralPartitionMetrics {
    std::size_t entryCount = 0;
    std::size_t regionCount = 0;
    SemanticStructuralPartitionRoleCounts roleCounts{};
};

class SemanticStructuralPartition {
public:
    void Rebuild(std::vector<SemanticStructuralPartitionEntry> entries,
                 ri::spatial::SpatialIndexOptions indexOptions = {});

    [[nodiscard]] std::vector<SemanticStructuralPartitionHit> QueryBox(
        const ri::spatial::Aabb& box,
        const SemanticStructuralPartitionQuery& query = {}) const;

    [[nodiscard]] const SemanticStructuralPartitionEntry* FindEntry(std::string_view id) const;
    [[nodiscard]] SemanticStructuralPartitionMetrics Metrics() const noexcept;

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

} // namespace ri::scene
