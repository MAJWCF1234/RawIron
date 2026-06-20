#include "RawIron/Scene/SemanticStructuralPartition.h"

#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
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

void IncrementOperationCount(SemanticStructuralPartitionOperationCounts& counts,
                             const StructuralBrushOperation operation) {
    switch (operation) {
        case StructuralBrushOperation::Unspecified:
            ++counts.unspecified;
            return;
        case StructuralBrushOperation::Solid:
            ++counts.solid;
            return;
        case StructuralBrushOperation::Subtract:
            ++counts.subtract;
            return;
        case StructuralBrushOperation::Intersect:
            ++counts.intersect;
            return;
        case StructuralBrushOperation::Stamp:
            ++counts.stamp;
            return;
        case StructuralBrushOperation::Merge:
            ++counts.merge;
            return;
        case StructuralBrushOperation::Detail:
            ++counts.detail;
            return;
    }
}

void IncrementRebuildScopeCount(SemanticStructuralPartitionRebuildScopeCounts& counts,
                                const StructuralBrushRebuildScope rebuildScope) {
    switch (rebuildScope) {
        case StructuralBrushRebuildScope::Local:
            ++counts.local;
            return;
        case StructuralBrushRebuildScope::Region:
            ++counts.region;
            return;
        case StructuralBrushRebuildScope::Global:
            ++counts.global;
            return;
        case StructuralBrushRebuildScope::Manual:
            ++counts.manual;
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

std::vector<SemanticStructuralPartitionHit> SemanticStructuralPartition::QueryRay(
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    const float far,
    const SemanticStructuralPartitionQuery& query) const {
    std::vector<SemanticStructuralPartitionHit> hits;
    const std::vector<std::string> ids = index_.QueryRay(origin, direction, far);
    hits.reserve(ids.size());
    const ri::spatial::Ray ray{.origin = origin, .direction = direction};
    for (const std::string& id : ids) {
        const SemanticStructuralPartitionEntry* entry = FindEntry(id);
        if (entry == nullptr || !MatchesQuery(*entry, query)) {
            continue;
        }
        float distance = 0.0f;
        if (!ri::spatial::IntersectRayAabb(ray, entry->bounds, far, &distance)) {
            continue;
        }
        hits.push_back(SemanticStructuralPartitionHit{
            .entry = entry,
            .distance = distance,
        });
    }
    std::sort(hits.begin(), hits.end(), [](const SemanticStructuralPartitionHit& lhs,
                                           const SemanticStructuralPartitionHit& rhs) {
        return lhs.distance < rhs.distance;
    });
    return hits;
}

std::optional<SemanticStructuralPartitionHit> SemanticStructuralPartition::QueryNearestRay(
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    const float far,
    const SemanticStructuralPartitionQuery& query) const {
    std::vector<SemanticStructuralPartitionHit> hits = QueryRay(origin, direction, far, query);
    if (hits.empty()) {
        return std::nullopt;
    }
    return hits.front();
}

const SemanticStructuralPartitionEntry* SemanticStructuralPartition::FindEntry(const std::string_view id) const {
    const auto found = entryIndexById_.find(std::string(id));
    if (found == entryIndexById_.end()) {
        return nullptr;
    }
    return &entries_[found->second];
}

SemanticStructuralPartitionMetrics SemanticStructuralPartition::Metrics() const noexcept {
    SemanticStructuralPartitionMetrics metrics = metrics_;
    const ri::spatial::SpatialIndexMetrics indexMetrics = index_.Metrics();
    metrics.boxQueries = indexMetrics.boxQueries;
    metrics.rayQueries = indexMetrics.rayQueries;
    metrics.boxCandidatesScanned = indexMetrics.boxCandidatesScanned;
    metrics.rayCandidatesScanned = indexMetrics.rayCandidatesScanned;
    return metrics;
}

void SemanticStructuralPartition::ResetMetrics() noexcept {
    index_.ResetMetrics();
}

bool SemanticStructuralPartition::MatchesQuery(const SemanticStructuralPartitionEntry& entry,
                                               const SemanticStructuralPartitionQuery& query) const {
    if (query.operation.has_value() && entry.metadata.operation != *query.operation) {
        return false;
    }
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
    if (query.rebuildScope.has_value() && entry.metadata.rebuildScope != *query.rebuildScope) {
        return false;
    }
    if (!query.brushId.empty() && entry.metadata.brushId != query.brushId) {
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
        IncrementOperationCount(metrics_.operationCounts, entry.metadata.operation);
        IncrementRebuildScopeCount(metrics_.rebuildScopeCounts, entry.metadata.rebuildScope);
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
            .nodeHandle = handle,
            .bounds = *bounds,
            .metadata = node.structuralBrush,
        });
    }
    return entries;
}

SemanticStructuralPartition BuildSemanticStructuralPartition(const Scene& scene,
                                                            ri::spatial::SpatialIndexOptions indexOptions) {
    SemanticStructuralPartition partition;
    partition.Rebuild(BuildSemanticStructuralPartitionEntries(scene), indexOptions);
    return partition;
}

std::optional<SemanticStructuralPickHit> PickSemanticStructuralBrush(
    const Scene& scene,
    const Ray& ray,
    const float far,
    const SemanticStructuralPartitionQuery& query,
    ri::spatial::SpatialIndexOptions indexOptions) {
    SemanticStructuralPartition partition = BuildSemanticStructuralPartition(scene, indexOptions);
    const std::optional<SemanticStructuralPartitionHit> hit =
        partition.QueryNearestRay(ray.origin, ray.direction, far, query);
    if (!hit.has_value() || hit->entry == nullptr) {
        return std::nullopt;
    }
    return SemanticStructuralPickHit{
        .entry = *hit->entry,
        .distance = hit->distance,
    };
}

} // namespace ri::scene
