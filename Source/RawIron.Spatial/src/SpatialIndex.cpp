#include "RawIron/Spatial/SpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ri::spatial {

BspSpatialIndex::BspSpatialIndex(std::vector<SpatialEntry> entries, SpatialIndexOptions options) {
    Rebuild(std::move(entries), options);
}

void BspSpatialIndex::Rebuild(std::vector<SpatialEntry> entries, SpatialIndexOptions options) {
    metrics_.rebuildCount += 1;
    entries_.clear();
    nodes_.clear();
    rootNode_ = kInvalidNode;

    entries_.reserve(entries.size());
    for (std::size_t sourceIndex = 0; sourceIndex < entries.size(); ++sourceIndex) {
        SpatialEntry& entry = entries[sourceIndex];
        if (entry.id.empty() || IsEmpty(entry.bounds)) {
            continue;
        }
        entries_.push_back(IndexedEntry{
            .id = std::move(entry.id),
            .bounds = entry.bounds,
            .center = Center(entry.bounds),
            .sourceIndex = sourceIndex,
        });
    }

    visitStamps_.assign(entries_.size(), 0U);
    visitEpoch_ = 0;

    if (entries_.empty()) {
        metrics_.lastRebuildEntryCount = 0;
        return;
    }

    metrics_.lastRebuildEntryCount = entries_.size();
    std::vector<std::size_t> indices(entries_.size());
    std::iota(indices.begin(), indices.end(), 0U);
    rootNode_ = BuildNode(indices, 0, options);
}

bool BspSpatialIndex::Empty() const {
    return entries_.empty() || rootNode_ == kInvalidNode;
}

std::size_t BspSpatialIndex::EntryCount() const {
    return entries_.size();
}

Aabb BspSpatialIndex::Bounds() const {
    if (rootNode_ == kInvalidNode) {
        return MakeEmptyAabb();
    }
    return nodes_[rootNode_].bounds;
}

std::uint32_t BspSpatialIndex::BeginQueryEpoch() const {
    visitEpoch_ += 1;
    if (visitEpoch_ == 0U) {
        std::fill(visitStamps_.begin(), visitStamps_.end(), 0U);
        visitEpoch_ = 1U;
    }
    return visitEpoch_;
}

std::vector<std::size_t> BspSpatialIndex::CollectBoxCandidates(const Aabb& box) const {
    metrics_.boxQueries += 1;
    if (rootNode_ == kInvalidNode || IsEmpty(box)) {
        return {};
    }
    std::vector<std::size_t> out;
    QueryBoxNode(rootNode_, box, BeginQueryEpoch(), out);
    return out;
}

std::vector<BspSpatialIndex::InternalRayHit> BspSpatialIndex::CollectRayCandidates(
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    float far) const {
    metrics_.rayQueries += 1;
    if (rootNode_ == kInvalidNode
        || !std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z)
        || !std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z)
        || !std::isfinite(far) || far <= 0.0f || ri::math::LengthSquared(direction) < 1e-20f) {
        return {};
    }

    const ri::math::Vec3 normalizedDirection = ri::math::Normalize(direction);
    const ri::math::Vec3 end = origin + (normalizedDirection * far);
    const Ray ray{.origin = origin, .direction = normalizedDirection};
    const Aabb segmentBounds = BuildSegmentBounds(origin, end);

    std::vector<InternalRayHit> out;
    QueryRayNode(rootNode_, ray, far, segmentBounds, BeginQueryEpoch(), out);
    return out;
}

std::vector<std::string> BspSpatialIndex::QueryBox(const Aabb& box) const {
    const std::vector<std::size_t> candidates = CollectBoxCandidates(box);
    std::vector<std::string> out;
    out.reserve(candidates.size());
    for (const std::size_t entryIndex : candidates) {
        out.push_back(entries_[entryIndex].id);
    }
    return out;
}

std::vector<std::string> BspSpatialIndex::QueryRay(const ri::math::Vec3& origin,
                                                   const ri::math::Vec3& direction,
                                                   float far) const {
    const std::vector<InternalRayHit> candidates = CollectRayCandidates(origin, direction, far);
    std::vector<std::string> out;
    out.reserve(candidates.size());
    for (const InternalRayHit& hit : candidates) {
        out.push_back(entries_[hit.entryIndex].id);
    }
    return out;
}

std::vector<std::size_t> BspSpatialIndex::QueryBoxSourceIndices(const Aabb& box) const {
    std::vector<std::size_t> candidates = CollectBoxCandidates(box);
    for (std::size_t& entryIndex : candidates) {
        entryIndex = entries_[entryIndex].sourceIndex;
    }
    return candidates;
}

std::vector<SpatialRayCandidate> BspSpatialIndex::QueryRayCandidates(const ri::math::Vec3& origin,
                                                                     const ri::math::Vec3& direction,
                                                                     float far) const {
    const std::vector<InternalRayHit> candidates = CollectRayCandidates(origin, direction, far);
    std::vector<SpatialRayCandidate> out;
    out.reserve(candidates.size());
    for (const InternalRayHit& hit : candidates) {
        out.push_back(SpatialRayCandidate{
            .sourceIndex = entries_[hit.entryIndex].sourceIndex,
            .distance = hit.distance,
        });
    }
    return out;
}

SpatialIndexMetrics BspSpatialIndex::Metrics() const noexcept {
    return metrics_;
}

void BspSpatialIndex::ResetMetrics() noexcept {
    metrics_.boxQueries = 0;
    metrics_.rayQueries = 0;
    metrics_.boxCandidatesScanned = 0;
    metrics_.rayCandidatesScanned = 0;
}

std::size_t BspSpatialIndex::BuildNode(const std::vector<std::size_t>& entryIndices,
                                       std::size_t depth,
                                       const SpatialIndexOptions& options) {
    Node node{};
    for (std::size_t entryIndex : entryIndices) {
        node.bounds = Union(node.bounds, entries_[entryIndex].bounds);
    }

    const std::size_t nodeIndex = nodes_.size();
    nodes_.push_back(node);

    if (entryIndices.size() <= options.maxLeafSize || depth >= options.maxDepth) {
        nodes_[nodeIndex].entryIndices = entryIndices;
        return nodeIndex;
    }

    const auto axisValue = [](const ri::math::Vec3& value, char axis) {
        return axis == 'x' ? value.x : (axis == 'y' ? value.y : value.z);
    };

    char axis = 'x';
    float split = 0.0f;
    std::size_t bestImbalance = static_cast<std::size_t>(-1);
    float bestSpan = -1.0f;
    bool foundSplit = false;
    for (const char candidateAxis : {'x', 'y', 'z'}) {
        std::vector<float> centers;
        centers.reserve(entryIndices.size());
        for (std::size_t entryIndex : entryIndices) {
            centers.push_back(axisValue(entries_[entryIndex].center, candidateAxis));
        }
        std::sort(centers.begin(), centers.end());
        const float candidateSplit = centers[centers.size() / 2];

        std::size_t leftCount = 0;
        std::size_t rightCount = 0;
        for (std::size_t entryIndex : entryIndices) {
            const float center = axisValue(entries_[entryIndex].center, candidateAxis);
            if (center <= candidateSplit) {
                leftCount += 1;
            } else {
                rightCount += 1;
            }
        }
        if (leftCount == 0 || rightCount == 0) {
            continue;
        }

        const std::size_t imbalance = leftCount > rightCount ? (leftCount - rightCount) : (rightCount - leftCount);
        const float span = axisValue(node.bounds.max, candidateAxis) - axisValue(node.bounds.min, candidateAxis);
        if (!foundSplit || imbalance < bestImbalance || (imbalance == bestImbalance && span > bestSpan)) {
            foundSplit = true;
            bestImbalance = imbalance;
            bestSpan = span;
            axis = candidateAxis;
            split = candidateSplit;
        }
    }
    if (!foundSplit) {
        nodes_[nodeIndex].entryIndices = entryIndices;
        return nodeIndex;
    }

    std::vector<std::size_t> leftEntries;
    std::vector<std::size_t> rightEntries;
    leftEntries.reserve(entryIndices.size());
    rightEntries.reserve(entryIndices.size());

    for (std::size_t entryIndex : entryIndices) {
        const ri::math::Vec3& c = entries_[entryIndex].center;
        const float center = axis == 'x' ? c.x : axis == 'y' ? c.y : c.z;
        if (center <= split) {
            leftEntries.push_back(entryIndex);
        } else {
            rightEntries.push_back(entryIndex);
        }
    }

    if (leftEntries.empty() || rightEntries.empty()) {
        nodes_[nodeIndex].entryIndices = entryIndices;
        return nodeIndex;
    }

    nodes_[nodeIndex].axis = axis;
    nodes_[nodeIndex].split = split;
    nodes_[nodeIndex].left = BuildNode(leftEntries, depth + 1, options);
    nodes_[nodeIndex].right = BuildNode(rightEntries, depth + 1, options);
    return nodeIndex;
}

void BspSpatialIndex::QueryBoxNode(std::size_t nodeIndex,
                                   const Aabb& box,
                                   const std::uint32_t epoch,
                                   std::vector<std::size_t>& out) const {
    const Node& node = nodes_[nodeIndex];
    if (!Intersects(node.bounds, box)) {
        return;
    }

    if (node.left == kInvalidNode && node.right == kInvalidNode) {
        for (std::size_t entryIndex : node.entryIndices) {
            metrics_.boxCandidatesScanned += 1;
            if (visitStamps_[entryIndex] == epoch || !Intersects(entries_[entryIndex].bounds, box)) {
                continue;
            }
            visitStamps_[entryIndex] = epoch;
            out.push_back(entryIndex);
        }
        return;
    }

    if (node.left != kInvalidNode) {
        QueryBoxNode(node.left, box, epoch, out);
    }
    if (node.right != kInvalidNode) {
        QueryBoxNode(node.right, box, epoch, out);
    }
}

void BspSpatialIndex::QueryRayNode(std::size_t nodeIndex,
                                   const Ray& ray,
                                   float far,
                                   const Aabb& segmentBounds,
                                   const std::uint32_t epoch,
                                   std::vector<InternalRayHit>& out) const {
    const Node& node = nodes_[nodeIndex];
    if (!Intersects(node.bounds, segmentBounds)) {
        return;
    }

    if (node.left == kInvalidNode && node.right == kInvalidNode) {
        for (std::size_t entryIndex : node.entryIndices) {
            metrics_.rayCandidatesScanned += 1;
            if (visitStamps_[entryIndex] == epoch) {
                continue;
            }
            float hitDistance = 0.0f;
            if (!IntersectRayAabb(ray, entries_[entryIndex].bounds, far, &hitDistance)) {
                continue;
            }
            visitStamps_[entryIndex] = epoch;
            out.push_back(InternalRayHit{
                .entryIndex = entryIndex,
                .distance = hitDistance,
            });
        }
        return;
    }

    const auto axisValue = [](const ri::math::Vec3& value, char axis) {
        return axis == 'x' ? value.x : (axis == 'y' ? value.y : value.z);
    };

    std::size_t firstChild = node.left;
    std::size_t secondChild = node.right;
    if (node.axis == 'x' || node.axis == 'y' || node.axis == 'z') {
        const float originAxis = axisValue(ray.origin, node.axis);
        const float dirAxis = axisValue(ray.direction, node.axis);
        const bool originOnLeft = originAxis <= node.split;
        if ((dirAxis > 0.0f && !originOnLeft) || (dirAxis < 0.0f && originOnLeft)) {
            firstChild = node.right;
            secondChild = node.left;
        }
    }

    if (firstChild != kInvalidNode) {
        QueryRayNode(firstChild, ray, far, segmentBounds, epoch, out);
    }
    if (secondChild != kInvalidNode) {
        QueryRayNode(secondChild, ray, far, segmentBounds, epoch, out);
    }
}

} // namespace ri::spatial
