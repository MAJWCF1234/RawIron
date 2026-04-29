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
    for (SpatialEntry& entry : entries) {
        if (entry.id.empty() || IsEmpty(entry.bounds)) {
            continue;
        }
        entries_.push_back(IndexedEntry{
            .id = std::move(entry.id),
            .bounds = entry.bounds,
            .center = Center(entry.bounds),
        });
    }

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

std::vector<std::string> BspSpatialIndex::QueryBox(const Aabb& box) const {
    metrics_.boxQueries += 1;
    if (rootNode_ == kInvalidNode || IsEmpty(box)) {
        return {};
    }
    std::vector<std::string> out;
    std::vector<bool> seen(entries_.size(), false);
    QueryBoxNode(rootNode_, box, out, seen);
    return out;
}

std::vector<std::string> BspSpatialIndex::QueryRay(const ri::math::Vec3& origin,
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

    std::vector<std::string> out;
    std::vector<bool> seen(entries_.size(), false);
    QueryRayNode(rootNode_, ray, far, segmentBounds, out, seen);
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

    const ri::math::Vec3 size = Size(node.bounds);
    const char axis = size.x >= size.z ? 'x' : 'z';

    std::vector<float> centers;
    centers.reserve(entryIndices.size());
    for (std::size_t entryIndex : entryIndices) {
        centers.push_back(axis == 'x' ? entries_[entryIndex].center.x : entries_[entryIndex].center.z);
    }
    std::sort(centers.begin(), centers.end());
    const float split = centers[centers.size() / 2];

    std::vector<std::size_t> leftEntries;
    std::vector<std::size_t> rightEntries;
    leftEntries.reserve(entryIndices.size());
    rightEntries.reserve(entryIndices.size());

    for (std::size_t entryIndex : entryIndices) {
        const float center = axis == 'x' ? entries_[entryIndex].center.x : entries_[entryIndex].center.z;
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
                                   std::vector<std::string>& out,
                                   std::vector<bool>& seen) const {
    const Node& node = nodes_[nodeIndex];
    if (!Intersects(node.bounds, box)) {
        return;
    }

    if (node.left == kInvalidNode && node.right == kInvalidNode) {
        for (std::size_t entryIndex : node.entryIndices) {
            metrics_.boxCandidatesScanned += 1;
            if (seen[entryIndex] || !Intersects(entries_[entryIndex].bounds, box)) {
                continue;
            }
            seen[entryIndex] = true;
            out.push_back(entries_[entryIndex].id);
        }
        return;
    }

    if (node.left != kInvalidNode) {
        QueryBoxNode(node.left, box, out, seen);
    }
    if (node.right != kInvalidNode) {
        QueryBoxNode(node.right, box, out, seen);
    }
}

void BspSpatialIndex::QueryRayNode(std::size_t nodeIndex,
                                   const Ray& ray,
                                   float far,
                                   const Aabb& segmentBounds,
                                   std::vector<std::string>& out,
                                   std::vector<bool>& seen) const {
    const Node& node = nodes_[nodeIndex];
    if (!Intersects(node.bounds, segmentBounds)) {
        return;
    }

    if (node.left == kInvalidNode && node.right == kInvalidNode) {
        for (std::size_t entryIndex : node.entryIndices) {
            metrics_.rayCandidatesScanned += 1;
            if (seen[entryIndex]) {
                continue;
            }
            float hitDistance = 0.0f;
            if (!IntersectRayAabb(ray, entries_[entryIndex].bounds, far, &hitDistance)) {
                continue;
            }
            seen[entryIndex] = true;
            out.push_back(entries_[entryIndex].id);
        }
        return;
    }

    if (node.left != kInvalidNode) {
        QueryRayNode(node.left, ray, far, segmentBounds, out, seen);
    }
    if (node.right != kInvalidNode) {
        QueryRayNode(node.right, ray, far, segmentBounds, out, seen);
    }
}

} // namespace ri::spatial
