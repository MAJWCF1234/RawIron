#include "RawIron/Scene/SemanticStructuralPartition.h"

#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace ri::scene {
namespace {

constexpr std::uint64_t kSemanticPartitionFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kSemanticPartitionFnvPrime = 1099511628211ull;

void HashByte(std::uint64_t& hash, const unsigned char value) {
    hash ^= static_cast<std::uint64_t>(value);
    hash *= kSemanticPartitionFnvPrime;
}

void HashUint(std::uint64_t& hash, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        HashByte(hash, static_cast<unsigned char>(value & 0xffU));
        value >>= 8U;
    }
}

void HashString(std::uint64_t& hash, const std::string_view value) {
    for (const unsigned char c : value) {
        HashByte(hash, c);
    }
    HashByte(hash, 0xffU);
}

void HashFloat(std::uint64_t& hash, const float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    HashUint(hash, bits);
}

void HashAabb(std::uint64_t& hash, const ri::spatial::Aabb& bounds) {
    HashFloat(hash, bounds.min.x);
    HashFloat(hash, bounds.min.y);
    HashFloat(hash, bounds.min.z);
    HashFloat(hash, bounds.max.x);
    HashFloat(hash, bounds.max.y);
    HashFloat(hash, bounds.max.z);
}

[[nodiscard]] std::uint64_t ComputeSemanticStructuralSceneSignature(const Scene& scene) {
    std::uint64_t hash = kSemanticPartitionFnvOffset;
    std::uint64_t structuralNodeCount = 0;
    for (int handle = 0; handle < static_cast<int>(scene.NodeCount()); ++handle) {
        const Node& node = scene.GetNode(handle);
        if (node.structuralBrush.brushId.empty()) {
            continue;
        }
        ++structuralNodeCount;
        HashUint(hash, static_cast<std::uint64_t>(handle));
        HashString(hash, node.name);
        HashUint(hash, StructuralBrushMetadataSignature(node.structuralBrush));
        // Metadata alone is not enough: moving a brush (or any ancestor) changes its world-space
        // spatial entry while leaving the metadata signature untouched. Hash the resolved bounds so
        // cached Q-mesh partitions never answer from an old transform.
        const std::optional<ri::spatial::Aabb> bounds = TryComputeMeshNodeWorldAabb(scene, handle);
        HashByte(hash, bounds.has_value() ? 1U : 0U);
        if (bounds.has_value()) {
            HashAabb(hash, *bounds);
        }
    }
    HashUint(hash, structuralNodeCount);
    return hash;
}

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

void IncrementChannelCounts(SemanticStructuralPartitionChannelCounts& counts,
                            const StructuralBrushMetadata& metadata) {
    if (StructuralBrushParticipatesInChannel(metadata, StructuralBrushChannel::VisualMesh)) {
        ++counts.visualMesh;
    }
    if (StructuralBrushParticipatesInChannel(metadata, StructuralBrushChannel::PhysicsMesh)) {
        ++counts.physicsMesh;
    }
    if (StructuralBrushParticipatesInChannel(metadata, StructuralBrushChannel::QueryMesh)) {
        ++counts.queryMesh;
    }
    if (StructuralBrushParticipatesInChannel(metadata, StructuralBrushChannel::InformationLayer)) {
        ++counts.informationLayer;
    }
}

void IncrementQueryPurposeCounts(SemanticStructuralPartitionQueryPurposeCounts& counts,
                                 const StructuralBrushMetadata& metadata) {
    if (StructuralBrushSupportsQueryPurpose(metadata, StructuralBrushQueryPurpose::Raycast)) {
        ++counts.raycast;
    }
    if (StructuralBrushSupportsQueryPurpose(metadata, StructuralBrushQueryPurpose::Trace)) {
        ++counts.trace;
    }
    if (StructuralBrushSupportsQueryPurpose(metadata, StructuralBrushQueryPurpose::Placement)) {
        ++counts.placement;
    }
    if (StructuralBrushSupportsQueryPurpose(metadata, StructuralBrushQueryPurpose::Interaction)) {
        ++counts.interaction;
    }
}

} // namespace

void SemanticStructuralPartition::Rebuild(std::vector<SemanticStructuralPartitionEntry> entries,
                                          ri::spatial::SpatialIndexOptions indexOptions) {
    entries_ = std::move(entries);
    for (SemanticStructuralPartitionEntry& entry : entries_) {
        entry.metadataSignature = StructuralBrushMetadataSignature(entry.metadata);
    }
    RebuildSideTables();
    RebuildSubpartitions(indexOptions);
}

void SemanticStructuralPartition::RebuildSubpartitions(const ri::spatial::SpatialIndexOptions indexOptions) {
    const bool optionsChanged = indexOptions.maxLeafSize != indexOptions_.maxLeafSize
        || indexOptions.maxDepth != indexOptions_.maxDepth;
    indexOptions_ = indexOptions;

    // Group entries by authored region; regionless entries share the "" bucket. Group order follows
    // first appearance so subpartition-local entry order is deterministic.
    std::vector<RegionSubpartition> next;
    std::unordered_map<std::string, std::size_t, detail::TransparentStringHash, std::equal_to<>> nextIndexByRegion;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const std::string& region = entries_[index].metadata.region;
        const auto found = nextIndexByRegion.find(std::string_view{region});
        std::size_t subpartitionIndex;
        if (found != nextIndexByRegion.end()) {
            subpartitionIndex = found->second;
        } else {
            subpartitionIndex = next.size();
            next.push_back(RegionSubpartition{.region = region});
            nextIndexByRegion.emplace(region, subpartitionIndex);
        }
        next[subpartitionIndex].entryIndices.push_back(index);
    }

    lastRebuildSubpartitionsReused_ = 0;
    lastRebuildSubpartitionsRebuilt_ = 0;
    for (RegionSubpartition& subpartition : next) {
        // The spatial tree only depends on entry ids, bounds, and their order; metadata-only edits
        // (role, policies, channels) never force a re-split.
        std::uint64_t signature = kSemanticPartitionFnvOffset;
        HashUint(signature, subpartition.entryIndices.size());
        for (const std::size_t entryIndex : subpartition.entryIndices) {
            const SemanticStructuralPartitionEntry& entry = entries_[entryIndex];
            HashString(signature, entry.id);
            HashAabb(signature, entry.bounds);
        }
        subpartition.contentSignature = signature;

        if (!optionsChanged) {
            const auto previousIt = subpartitionIndexByRegion_.find(std::string_view{subpartition.region});
            if (previousIt != subpartitionIndexByRegion_.end()) {
                RegionSubpartition& previous = subpartitions_[previousIt->second];
                if (previous.contentSignature == signature
                    && previous.entryIndices.size() == subpartition.entryIndices.size()) {
                    // Same ids/bounds/order: the old tree's subpartition-local source indices still
                    // map 1:1 onto the new entryIndices vector, so the split tree can be kept.
                    subpartition.index = std::move(previous.index);
                    ++lastRebuildSubpartitionsReused_;
                    continue;
                }
            }
        }

        std::vector<ri::spatial::SpatialEntry> spatialEntries;
        spatialEntries.reserve(subpartition.entryIndices.size());
        for (const std::size_t entryIndex : subpartition.entryIndices) {
            const SemanticStructuralPartitionEntry& entry = entries_[entryIndex];
            spatialEntries.push_back(ri::spatial::SpatialEntry{
                .id = entry.id,
                .bounds = entry.bounds,
            });
        }
        subpartition.index.Rebuild(std::move(spatialEntries), indexOptions);
        ++lastRebuildSubpartitionsRebuilt_;
    }

    subpartitions_ = std::move(next);
    subpartitionIndexByRegion_ = std::move(nextIndexByRegion);
}

const SemanticStructuralPartition::RegionSubpartition* SemanticStructuralPartition::FindSubpartition(
    const std::string_view region) const {
    const auto found = subpartitionIndexByRegion_.find(region);
    if (found == subpartitionIndexByRegion_.end()) {
        return nullptr;
    }
    return &subpartitions_[found->second];
}

void SemanticStructuralPartition::CollectBoxCandidates(const RegionSubpartition& subpartition,
                                                       const ri::spatial::Aabb& box,
                                                       std::vector<std::size_t>& outEntryIndices) const {
    for (const std::size_t localIndex : subpartition.index.QueryBoxSourceIndices(box)) {
        outEntryIndices.push_back(subpartition.entryIndices[localIndex]);
    }
}

void SemanticStructuralPartition::CollectRayCandidates(
    const RegionSubpartition& subpartition,
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    const float far,
    std::vector<ri::spatial::SpatialRayCandidate>& outCandidates) const {
    for (const ri::spatial::SpatialRayCandidate& candidate :
         subpartition.index.QueryRayCandidates(origin, direction, far)) {
        outCandidates.push_back(ri::spatial::SpatialRayCandidate{
            .sourceIndex = subpartition.entryIndices[candidate.sourceIndex],
            .distance = candidate.distance,
        });
    }
}

std::vector<SemanticStructuralPartitionHit> SemanticStructuralPartition::QueryBox(
    const ri::spatial::Aabb& box,
    const SemanticStructuralPartitionQuery& query) const {
    ++queryStats_.boxQueries;
    std::vector<std::size_t> candidates;
    if (!query.region.empty()) {
        ++queryStats_.regionScopedBoxQueries;
        if (const RegionSubpartition* subpartition = FindSubpartition(query.region);
            subpartition != nullptr) {
            CollectBoxCandidates(*subpartition, box, candidates);
        }
    } else {
        for (const RegionSubpartition& subpartition : subpartitions_) {
            CollectBoxCandidates(subpartition, box, candidates);
        }
    }
    queryStats_.boxCandidatesScanned += candidates.size();

    std::vector<SemanticStructuralPartitionHit> hits;
    hits.reserve(candidates.size());
    for (const std::size_t entryIndex : candidates) {
        const SemanticStructuralPartitionEntry& entry = entries_[entryIndex];
        if (!MatchesQuery(entry, query)) {
            continue;
        }
        hits.push_back(SemanticStructuralPartitionHit{.entry = &entry});
    }
    queryStats_.boxCandidatesMatched += hits.size();
    return hits;
}

std::vector<SemanticStructuralPartitionHit> SemanticStructuralPartition::QueryRay(
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    const float far,
    const SemanticStructuralPartitionQuery& query) const {
    ++queryStats_.rayQueries;
    std::vector<ri::spatial::SpatialRayCandidate> candidates;
    if (!query.region.empty()) {
        ++queryStats_.regionScopedRayQueries;
        if (const RegionSubpartition* subpartition = FindSubpartition(query.region);
            subpartition != nullptr) {
            CollectRayCandidates(*subpartition, origin, direction, far, candidates);
        }
    } else {
        for (const RegionSubpartition& subpartition : subpartitions_) {
            CollectRayCandidates(subpartition, origin, direction, far, candidates);
        }
    }
    queryStats_.rayCandidatesScanned += candidates.size();

    std::vector<SemanticStructuralPartitionHit> hits;
    hits.reserve(candidates.size());
    for (const ri::spatial::SpatialRayCandidate& candidate : candidates) {
        const SemanticStructuralPartitionEntry& entry = entries_[candidate.sourceIndex];
        if (!MatchesQuery(entry, query)) {
            continue;
        }
        hits.push_back(SemanticStructuralPartitionHit{
            .entry = &entry,
            .distance = candidate.distance,
        });
    }
    queryStats_.rayCandidatesMatched += hits.size();
    std::sort(hits.begin(), hits.end(), [](const SemanticStructuralPartitionHit& lhs,
                                           const SemanticStructuralPartitionHit& rhs) {
        if (lhs.distance != rhs.distance) {
            return lhs.distance < rhs.distance;
        }
        if (lhs.entry->nodeHandle != rhs.entry->nodeHandle) {
            return lhs.entry->nodeHandle < rhs.entry->nodeHandle;
        }
        return lhs.entry < rhs.entry;
    });
    return hits;
}

std::optional<SemanticStructuralPartitionHit> SemanticStructuralPartition::QueryNearestRay(
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    const float far,
    const SemanticStructuralPartitionQuery& query) const {
    ++queryStats_.rayQueries;
    std::vector<ri::spatial::SpatialRayCandidate> candidates;
    if (!query.region.empty()) {
        ++queryStats_.regionScopedRayQueries;
        if (const RegionSubpartition* subpartition = FindSubpartition(query.region);
            subpartition != nullptr) {
            CollectRayCandidates(*subpartition, origin, direction, far, candidates);
        }
    } else {
        for (const RegionSubpartition& subpartition : subpartitions_) {
            CollectRayCandidates(subpartition, origin, direction, far, candidates);
        }
    }
    queryStats_.rayCandidatesScanned += candidates.size();

    std::optional<SemanticStructuralPartitionHit> best;
    std::size_t matched = 0;
    for (const ri::spatial::SpatialRayCandidate& candidate : candidates) {
        const SemanticStructuralPartitionEntry& entry = entries_[candidate.sourceIndex];
        if (!MatchesQuery(entry, query)) {
            continue;
        }
        ++matched;
        const bool closer = !best.has_value()
            || candidate.distance < best->distance
            || (candidate.distance == best->distance
                && (entry.nodeHandle < best->entry->nodeHandle
                    || (entry.nodeHandle == best->entry->nodeHandle && &entry < best->entry)));
        if (closer) {
            best = SemanticStructuralPartitionHit{
                .entry = &entry,
                .distance = candidate.distance,
            };
        }
    }
    queryStats_.rayCandidatesMatched += matched;
    return best;
}

const SemanticStructuralPartitionEntry* SemanticStructuralPartition::FindEntry(const std::string_view id) const {
    const auto found = entryIndexById_.find(id);
    if (found == entryIndexById_.end()) {
        return nullptr;
    }
    return &entries_[found->second];
}

std::vector<const SemanticStructuralPartitionEntry*> SemanticStructuralPartition::FindEntriesByMetadataSignature(
    const std::uint64_t metadataSignature) const {
    std::vector<const SemanticStructuralPartitionEntry*> matches;
    const auto found = entryIndicesByMetadataSignature_.find(metadataSignature);
    if (found == entryIndicesByMetadataSignature_.end()) {
        return matches;
    }
    matches.reserve(found->second.size());
    for (const std::size_t entryIndex : found->second) {
        matches.push_back(&entries_[entryIndex]);
    }
    return matches;
}

std::size_t SemanticStructuralPartition::MetadataSignatureBucketCount() const noexcept {
    return entryIndicesByMetadataSignature_.size();
}

std::vector<const SemanticStructuralPartitionEntry*> SemanticStructuralPartition::FindEntriesByRegion(
    const std::string_view region) const {
    std::vector<const SemanticStructuralPartitionEntry*> matches;
    const auto found = entryIndicesByRegion_.find(region);
    if (found == entryIndicesByRegion_.end()) {
        return matches;
    }
    matches.reserve(found->second.size());
    for (const std::size_t entryIndex : found->second) {
        matches.push_back(&entries_[entryIndex]);
    }
    return matches;
}

std::size_t SemanticStructuralPartition::RegionBucketCount() const noexcept {
    return entryIndicesByRegion_.size();
}

SemanticStructuralPartitionMetrics SemanticStructuralPartition::Metrics() const noexcept {
    SemanticStructuralPartitionMetrics metrics = metrics_;
    metrics.boxQueries = queryStats_.boxQueries;
    metrics.rayQueries = queryStats_.rayQueries;
    metrics.boxCandidatesScanned = queryStats_.boxCandidatesScanned;
    metrics.rayCandidatesScanned = queryStats_.rayCandidatesScanned;
    metrics.boxCandidatesMatched = queryStats_.boxCandidatesMatched;
    metrics.rayCandidatesMatched = queryStats_.rayCandidatesMatched;
    metrics.regionScopedBoxQueries = queryStats_.regionScopedBoxQueries;
    metrics.regionScopedRayQueries = queryStats_.regionScopedRayQueries;
    metrics.subpartitionCount = subpartitions_.size();
    metrics.lastRebuildSubpartitionsReused = lastRebuildSubpartitionsReused_;
    metrics.lastRebuildSubpartitionsRebuilt = lastRebuildSubpartitionsRebuilt_;
    return metrics;
}

void SemanticStructuralPartition::ResetMetrics() noexcept {
    queryStats_ = {};
    for (RegionSubpartition& subpartition : subpartitions_) {
        subpartition.index.ResetMetrics();
    }
}

bool SemanticStructuralPartition::MatchesQuery(const SemanticStructuralPartitionEntry& entry,
                                               const SemanticStructuralPartitionQuery& query) const {
    if (query.channel.has_value()
        && !StructuralBrushParticipatesInChannel(entry.metadata, *query.channel)) {
        return false;
    }
    if (query.queryPurpose.has_value()
        && !StructuralBrushSupportsQueryPurpose(entry.metadata, *query.queryPurpose)) {
        return false;
    }
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
    entryIndicesByMetadataSignature_.clear();
    entryIndicesByMetadataSignature_.reserve(entries_.size());
    entryIndicesByRegion_.clear();
    entryIndicesByRegion_.reserve(entries_.size());
    metrics_ = {};
    metrics_.entryCount = entries_.size();

    std::unordered_set<std::string> regions;
    std::unordered_set<std::uint64_t> metadataSignatures;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const SemanticStructuralPartitionEntry& entry = entries_[index];
        if (!entry.id.empty()) {
            entryIndexById_[entry.id] = index;
        }
        if (!entry.metadata.region.empty()) {
            regions.insert(entry.metadata.region);
            entryIndicesByRegion_[entry.metadata.region].push_back(index);
        }
        IncrementRoleCount(metrics_.roleCounts, entry.metadata.role);
        IncrementOperationCount(metrics_.operationCounts, entry.metadata.operation);
        IncrementRebuildScopeCount(metrics_.rebuildScopeCounts, entry.metadata.rebuildScope);
        IncrementChannelCounts(metrics_.channelCounts, entry.metadata);
        IncrementQueryPurposeCounts(metrics_.queryPurposeCounts, entry.metadata);
        metadataSignatures.insert(entry.metadataSignature);
        entryIndicesByMetadataSignature_[entry.metadataSignature].push_back(index);
    }
    metrics_.regionCount = regions.size();
    metrics_.uniqueMetadataSignatureCount = metadataSignatures.size();
    if (metrics_.entryCount > metrics_.uniqueMetadataSignatureCount) {
        metrics_.duplicateMetadataSignatureCount =
            metrics_.entryCount - metrics_.uniqueMetadataSignatureCount;
    }
}

const SemanticStructuralPartition& SemanticStructuralPartitionCache::GetOrRebuild(
    const Scene& scene,
    ri::spatial::SpatialIndexOptions indexOptions) {
    const std::uint64_t sceneSignature = ComputeSemanticStructuralSceneSignature(scene);
    if (dirty_ || sceneSignature != sceneSignature_) {
        // Rebuild in place so region subpartitions whose content did not change keep their
        // spatial trees; a single brush edit re-splits only the affected region.
        partition_.Rebuild(BuildSemanticStructuralPartitionEntries(scene), indexOptions);
        sceneSignature_ = sceneSignature;
        dirty_ = false;
        ++rebuildCount_;
    } else {
        ++reuseCount_;
    }
    return partition_;
}

void SemanticStructuralPartitionCache::Invalidate() noexcept {
    dirty_ = true;
}

bool SemanticStructuralPartitionCache::IsDirty() const noexcept {
    return dirty_;
}

bool SemanticStructuralPartitionCache::NeedsRebuild(const Scene& scene) const {
    return dirty_ || ComputeSemanticStructuralSceneSignature(scene) != sceneSignature_;
}

std::size_t SemanticStructuralPartitionCache::RebuildCount() const noexcept {
    return rebuildCount_;
}

std::size_t SemanticStructuralPartitionCache::ReuseCount() const noexcept {
    return reuseCount_;
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
            .metadataSignature = StructuralBrushMetadataSignature(node.structuralBrush),
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

std::optional<SemanticStructuralRaycastHit> RaycastSemanticStructuralPartition(
    const Scene& scene,
    const SemanticStructuralPartition& partition,
    const Ray& ray,
    const float far,
    const SemanticStructuralPartitionQuery& query) {
    if (far <= 0.0f || ri::math::LengthSquared(ray.direction) <= 0.000001f) {
        return std::nullopt;
    }

    const Ray normalizedRay{
        .origin = ray.origin,
        .direction = ri::math::Normalize(ray.direction),
    };

    const std::vector<SemanticStructuralPartitionHit> candidates =
        partition.QueryRay(normalizedRay.origin, normalizedRay.direction, far, query);
    std::optional<SemanticStructuralRaycastHit> best;
    for (const SemanticStructuralPartitionHit& candidate : candidates) {
        if (candidate.entry == nullptr) {
            continue;
        }
        const std::optional<RaycastHit> preciseHit =
            RaycastNode(scene, candidate.entry->nodeHandle, normalizedRay);
        if (!preciseHit.has_value() || preciseHit->distance > far) {
            continue;
        }

        const bool closer = !best.has_value()
            || preciseHit->distance + 0.0001f < best->hit.distance
            || (std::abs(preciseHit->distance - best->hit.distance) <= 0.0001f
                && candidate.entry->nodeHandle < best->entry.nodeHandle);
        if (closer) {
            best = SemanticStructuralRaycastHit{
                .entry = *candidate.entry,
                .hit = *preciseHit,
            };
        }
    }
    return best;
}

std::optional<SemanticStructuralRaycastHit> RaycastSemanticStructuralBrush(
    const Scene& scene,
    const Ray& ray,
    const float far,
    const SemanticStructuralPartitionQuery& query,
    const ri::spatial::SpatialIndexOptions indexOptions) {
    const SemanticStructuralPartition partition = BuildSemanticStructuralPartition(scene, indexOptions);
    return RaycastSemanticStructuralPartition(scene, partition, ray, far, query);
}

} // namespace ri::scene
