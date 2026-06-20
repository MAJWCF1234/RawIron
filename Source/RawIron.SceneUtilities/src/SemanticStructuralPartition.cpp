#include "RawIron/Scene/SemanticStructuralPartition.h"

#include "RawIron/Scene/SceneUtils.h"

#include <optional>
#include <unordered_set>
#include <utility>

namespace ri::scene {
namespace {

void IncrementRoleCount(SemanticStructuralPartitionRoleCounts& counts,
                        const StructuralBrushSemanticRole role) {
    switch (role) {
        case StructuralBrushSemanticRole::Structure:
            ++counts.structure;
            return;
        case StructuralBrushSemanticRole::Wall:
            ++counts.wall;
            return;
        case StructuralBrushSemanticRole::Floor:
            ++counts.floor;
            return;
        case StructuralBrushSemanticRole::Ceiling:
            ++counts.ceiling;
            return;
        case StructuralBrushSemanticRole::Pillar:
            ++counts.pillar;
            return;
        case StructuralBrushSemanticRole::Stair:
            ++counts.stair;
            return;
        case StructuralBrushSemanticRole::Portal:
            ++counts.portal;
            return;
        case StructuralBrushSemanticRole::Trim:
            ++counts.trim;
            return;
        case StructuralBrushSemanticRole::Cover:
            ++counts.cover;
            return;
        case StructuralBrushSemanticRole::Water:
            ++counts.water;
            return;
        case StructuralBrushSemanticRole::Trigger:
            ++counts.trigger;
            return;
        case StructuralBrushSemanticRole::Decor:
            ++counts.decor;
            return;
        case StructuralBrushSemanticRole::Volume:
            ++counts.volume;
            return;
    }
}

} // namespace

void SemanticStructuralPartition::Rebuild(std::vector<SemanticStructuralPartitionEntry> entries,
                                          ri::spatial::SpatialIndexOptions indexOptions) {
    entries_ = std::move(entries);
    RebuildSideTables();

    std::vector<ri::spatial::SpatialEntry> spatialEntries;
    spatialEntries.reserve(entries_.size());
    for (const SemanticStructuralPartitionEntry& entry : entries_) {
        spatialEntries.push_back(ri::spatial::SpatialEntry{
            .id = entry.id,
            .bounds = entry.bounds,
        });
    }
    index_.Rebuild(std::move(spatialEntries), indexOptions);
}

std::vector<SemanticStructuralPartitionHit> SemanticStructuralPartition::QueryBox(
    const ri::spatial::Aabb& box,
    const SemanticStructuralPartitionQuery& query) const {
    std::vector<SemanticStructuralPartitionHit> hits;
    const std::vector<std::string> ids = index_.QueryBox(box);
    hits.reserve(ids.size());
    for (const std::string& id : ids) {
        const SemanticStructuralPartitionEntry* entry = FindEntry(id);
        if (entry == nullptr || !MatchesQuery(*entry, query)) {
            continue;
        }
        hits.push_back(SemanticStructuralPartitionHit{.entry = entry});
    }
    return hits;
}

const SemanticStructuralPartitionEntry* SemanticStructuralPartition::FindEntry(const std::string_view id) const {
    const auto found = entryIndexById_.find(std::string(id));
    if (found == entryIndexById_.end()) {
        return nullptr;
    }
    return &entries_[found->second];
}

SemanticStructuralPartitionMetrics SemanticStructuralPartition::Metrics() const noexcept {
    return metrics_;
}

bool SemanticStructuralPartition::MatchesQuery(const SemanticStructuralPartitionEntry& entry,
                                               const SemanticStructuralPartitionQuery& query) const {
    if (query.role.has_value() && entry.metadata.role != *query.role) {
        return false;
    }
    if (query.collision.has_value() && entry.metadata.collision != *query.collision) {
        return false;
    }
    if (query.visibility.has_value() && entry.metadata.visibility != *query.visibility) {
        return false;
    }
    if (query.navigation.has_value() && entry.metadata.navigation != *query.navigation) {
        return false;
    }
    if (!query.region.empty() && entry.metadata.region != query.region) {
        return false;
    }
    return true;
}

void SemanticStructuralPartition::RebuildSideTables() {
    entryIndexById_.clear();
    entryIndexById_.reserve(entries_.size());
    metrics_ = {};
    metrics_.entryCount = entries_.size();

    std::unordered_set<std::string> regions;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const SemanticStructuralPartitionEntry& entry = entries_[index];
        if (!entry.id.empty()) {
            entryIndexById_[entry.id] = index;
        }
        if (!entry.metadata.region.empty()) {
            regions.insert(entry.metadata.region);
        }
        IncrementRoleCount(metrics_.roleCounts, entry.metadata.role);
    }
    metrics_.regionCount = regions.size();
}

std::vector<SemanticStructuralPartitionEntry> BuildSemanticStructuralPartitionEntries(const Scene& scene) {
    std::vector<SemanticStructuralPartitionEntry> entries;
    entries.reserve(scene.NodeCount());
    for (int handle = 0; handle < static_cast<int>(scene.NodeCount()); ++handle) {
        const Node& node = scene.GetNode(handle);
        if (node.structuralBrush.brushId.empty()) {
            continue;
        }
        const std::optional<ri::spatial::Aabb> bounds = TryComputeMeshNodeWorldAabb(scene, handle);
        if (!bounds.has_value()) {
            continue;
        }
        entries.push_back(SemanticStructuralPartitionEntry{
            .id = node.name,
            .bounds = *bounds,
            .metadata = node.structuralBrush,
        });
    }
    return entries;
}

} // namespace ri::scene
