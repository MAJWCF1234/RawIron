#include "RawIron/Spatial/SpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

namespace ri::spatial {
namespace {

constexpr std::memory_order kMetricMemoryOrder = std::memory_order_relaxed;
constexpr double kMinimumRayDirectionLengthSquared = 1e-20;

[[nodiscard]] bool TryNormalizeRayDirection(const ri::math::Vec3& direction,
                                            ri::math::Vec3& outNormalized) noexcept {
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z)) {
        return false;
    }

    // Float LengthSquared overflows for perfectly valid large directions (for example FLT_MAX on one
    // axis), after which the ordinary float Normalize collapses the vector to zero. Scaling by the largest
    // component keeps the ratio calculation bounded; double precision preserves the existing 1e-20
    // squared-length rejection even for very small finite vectors.
    const double maxComponent = (std::max)({
        std::fabs(static_cast<double>(direction.x)),
        std::fabs(static_cast<double>(direction.y)),
        std::fabs(static_cast<double>(direction.z)),
    });
    if (maxComponent == 0.0) {
        return false;
    }

    const double scaledX = static_cast<double>(direction.x) / maxComponent;
    const double scaledY = static_cast<double>(direction.y) / maxComponent;
    const double scaledZ = static_cast<double>(direction.z) / maxComponent;
    const double scaledLength = std::sqrt(
        (scaledX * scaledX) + (scaledY * scaledY) + (scaledZ * scaledZ));
    const double length = maxComponent * scaledLength;
    if ((length * length) < kMinimumRayDirectionLengthSquared) {
        return false;
    }

    const double inverseScaledLength = 1.0 / scaledLength;
    outNormalized = ri::math::Vec3{
        static_cast<float>(scaledX * inverseScaledLength),
        static_cast<float>(scaledY * inverseScaledLength),
        static_cast<float>(scaledZ * inverseScaledLength),
    };
    return true;
}

[[nodiscard]] float SaturatingRayEndpoint(const float origin,
                                          const float normalizedDirection,
                                          const float far) noexcept {
    const double endpoint = static_cast<double>(origin)
        + (static_cast<double>(normalizedDirection) * static_cast<double>(far));
    constexpr double maxFloat = static_cast<double>((std::numeric_limits<float>::max)());
    return static_cast<float>((std::clamp)(endpoint, -maxFloat, maxFloat));
}

} // namespace

BspSpatialIndex::ConcurrentMetrics::ConcurrentMetrics(const ConcurrentMetrics& other) noexcept {
    *this = other;
}

BspSpatialIndex::ConcurrentMetrics& BspSpatialIndex::ConcurrentMetrics::operator=(
    const ConcurrentMetrics& other) noexcept {
    rebuildCount.store(other.rebuildCount.load(kMetricMemoryOrder), kMetricMemoryOrder);
    lastRebuildEntryCount.store(other.lastRebuildEntryCount.load(kMetricMemoryOrder), kMetricMemoryOrder);
    boxQueries.store(other.boxQueries.load(kMetricMemoryOrder), kMetricMemoryOrder);
    rayQueries.store(other.rayQueries.load(kMetricMemoryOrder), kMetricMemoryOrder);
    boxCandidatesScanned.store(other.boxCandidatesScanned.load(kMetricMemoryOrder), kMetricMemoryOrder);
    rayCandidatesScanned.store(other.rayCandidatesScanned.load(kMetricMemoryOrder), kMetricMemoryOrder);
    return *this;
}

BspSpatialIndex::ConcurrentMetrics::ConcurrentMetrics(ConcurrentMetrics&& other) noexcept {
    *this = other;
}

BspSpatialIndex::ConcurrentMetrics& BspSpatialIndex::ConcurrentMetrics::operator=(
    ConcurrentMetrics&& other) noexcept {
    return *this = other;
}

BspSpatialIndex::BspSpatialIndex(std::vector<SpatialEntry> entries, SpatialIndexOptions options) {
    Rebuild(std::move(entries), options);
}

BspSpatialIndex::BspSpatialIndex(BspSpatialIndex&& other) noexcept
    : entries_(std::move(other.entries_)),
      nodes_(std::move(other.nodes_)),
      rootNode_(std::exchange(other.rootNode_, kInvalidNode)),
      metrics_(std::move(other.metrics_)) {
    // A moved-from index is a reusable empty value rather than a stale root paired with moved-out vectors.
    other.metrics_ = ConcurrentMetrics{};
}

BspSpatialIndex& BspSpatialIndex::operator=(BspSpatialIndex&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    entries_ = std::move(other.entries_);
    nodes_ = std::move(other.nodes_);
    rootNode_ = std::exchange(other.rootNode_, kInvalidNode);
    metrics_ = std::move(other.metrics_);
    other.metrics_ = ConcurrentMetrics{};
    return *this;
}

void BspSpatialIndex::Rebuild(std::vector<SpatialEntry> entries, SpatialIndexOptions options) {
    metrics_.rebuildCount.fetch_add(1, kMetricMemoryOrder);
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

    if (entries_.empty()) {
        metrics_.lastRebuildEntryCount.store(0, kMetricMemoryOrder);
        return;
    }

    metrics_.lastRebuildEntryCount.store(entries_.size(), kMetricMemoryOrder);
    std::vector<std::size_t> indices(entries_.size());
    std::iota(indices.begin(), indices.end(), 0U);
    rootNode_ = BuildNode(indices, 0, options);
}

bool BspSpatialIndex::Empty() const {
    return entries_.empty() || rootNode_ == kInvalidNode || rootNode_ >= nodes_.size();
}

std::size_t BspSpatialIndex::EntryCount() const {
    return entries_.size();
}

Aabb BspSpatialIndex::Bounds() const {
    if (entries_.empty() || rootNode_ == kInvalidNode || rootNode_ >= nodes_.size()) {
        return MakeEmptyAabb();
    }
    return nodes_[rootNode_].bounds;
}

std::vector<std::size_t> BspSpatialIndex::CollectBoxCandidates(const Aabb& box) const {
    metrics_.boxQueries.fetch_add(1, kMetricMemoryOrder);
    if (entries_.empty() || rootNode_ == kInvalidNode || rootNode_ >= nodes_.size() || IsEmpty(box)) {
        return {};
    }
    std::vector<std::size_t> out;
    std::size_t candidatesScanned = 0;
    QueryBoxNode(rootNode_, box, candidatesScanned, out);
    metrics_.boxCandidatesScanned.fetch_add(candidatesScanned, kMetricMemoryOrder);
    return out;
}

std::vector<BspSpatialIndex::InternalRayHit> BspSpatialIndex::CollectRayCandidates(
    const ri::math::Vec3& origin,
    const ri::math::Vec3& direction,
    float far) const {
    metrics_.rayQueries.fetch_add(1, kMetricMemoryOrder);
    ri::math::Vec3 normalizedDirection{};
    if (entries_.empty() || rootNode_ == kInvalidNode || rootNode_ >= nodes_.size()
        || !std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z)
        || !std::isfinite(far) || far <= 0.0f
        || !TryNormalizeRayDirection(direction, normalizedDirection)) {
        return {};
    }

    const ri::math::Vec3 end{
        SaturatingRayEndpoint(origin.x, normalizedDirection.x, far),
        SaturatingRayEndpoint(origin.y, normalizedDirection.y, far),
        SaturatingRayEndpoint(origin.z, normalizedDirection.z, far),
    };
    const Ray ray{.origin = origin, .direction = normalizedDirection};
    const Aabb segmentBounds = BuildSegmentBounds(origin, end);

    std::vector<InternalRayHit> out;
    std::size_t candidatesScanned = 0;
    QueryRayNode(rootNode_, ray, far, segmentBounds, candidatesScanned, out);
    metrics_.rayCandidatesScanned.fetch_add(candidatesScanned, kMetricMemoryOrder);
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
    return SpatialIndexMetrics{
        .rebuildCount = metrics_.rebuildCount.load(kMetricMemoryOrder),
        .lastRebuildEntryCount = metrics_.lastRebuildEntryCount.load(kMetricMemoryOrder),
        .boxQueries = metrics_.boxQueries.load(kMetricMemoryOrder),
        .rayQueries = metrics_.rayQueries.load(kMetricMemoryOrder),
        .boxCandidatesScanned = metrics_.boxCandidatesScanned.load(kMetricMemoryOrder),
        .rayCandidatesScanned = metrics_.rayCandidatesScanned.load(kMetricMemoryOrder),
    };
}

void BspSpatialIndex::ResetMetrics() noexcept {
    metrics_.boxQueries.store(0, kMetricMemoryOrder);
    metrics_.rayQueries.store(0, kMetricMemoryOrder);
    metrics_.boxCandidatesScanned.store(0, kMetricMemoryOrder);
    metrics_.rayCandidatesScanned.store(0, kMetricMemoryOrder);
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
                                   std::size_t& candidatesScanned,
                                   std::vector<std::size_t>& out) const {
    if (nodeIndex >= nodes_.size()) {
        return;
    }
    const Node& node = nodes_[nodeIndex];
    if (!Intersects(node.bounds, box)) {
        return;
    }

    if (node.left == kInvalidNode && node.right == kInvalidNode) {
        for (std::size_t entryIndex : node.entryIndices) {
            ++candidatesScanned;
            if (entryIndex >= entries_.size() || !Intersects(entries_[entryIndex].bounds, box)) {
                continue;
            }
            out.push_back(entryIndex);
        }
        return;
    }

    if (node.left != kInvalidNode) {
        QueryBoxNode(node.left, box, candidatesScanned, out);
    }
    if (node.right != kInvalidNode) {
        QueryBoxNode(node.right, box, candidatesScanned, out);
    }
}

void BspSpatialIndex::QueryRayNode(std::size_t nodeIndex,
                                   const Ray& ray,
                                   float far,
                                   const Aabb& segmentBounds,
                                   std::size_t& candidatesScanned,
                                   std::vector<InternalRayHit>& out) const {
    if (nodeIndex >= nodes_.size()) {
        return;
    }
    const Node& node = nodes_[nodeIndex];
    if (!Intersects(node.bounds, segmentBounds)) {
        return;
    }

    if (node.left == kInvalidNode && node.right == kInvalidNode) {
        for (std::size_t entryIndex : node.entryIndices) {
            ++candidatesScanned;
            if (entryIndex >= entries_.size()) {
                continue;
            }
            float hitDistance = 0.0f;
            if (!IntersectRayAabb(ray, entries_[entryIndex].bounds, far, &hitDistance)) {
                continue;
            }
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
        QueryRayNode(firstChild, ray, far, segmentBounds, candidatesScanned, out);
    }
    if (secondChild != kInvalidNode) {
        QueryRayNode(secondChild, ray, far, segmentBounds, candidatesScanned, out);
    }
}

} // namespace ri::spatial
