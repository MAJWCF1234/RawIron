#pragma once

#include "RawIron/Spatial/Aabb.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ri::spatial {

struct SpatialEntry {
    std::string id;
    Aabb bounds;
};

struct SpatialIndexOptions {
    std::size_t maxLeafSize = 12;
    std::size_t maxDepth = 10;
};

struct SpatialIndexMetrics {
    std::size_t rebuildCount = 0;
    std::size_t lastRebuildEntryCount = 0;
    std::size_t boxQueries = 0;
    std::size_t rayQueries = 0;
    std::size_t boxCandidatesScanned = 0;
    std::size_t rayCandidatesScanned = 0;
};

class BspSpatialIndex {
public:
    BspSpatialIndex() = default;
    explicit BspSpatialIndex(std::vector<SpatialEntry> entries, SpatialIndexOptions options = {});

    void Rebuild(std::vector<SpatialEntry> entries, SpatialIndexOptions options = {});
    [[nodiscard]] bool Empty() const;
    [[nodiscard]] std::size_t EntryCount() const;
    [[nodiscard]] Aabb Bounds() const;
    [[nodiscard]] std::vector<std::string> QueryBox(const Aabb& box) const;
    [[nodiscard]] std::vector<std::string> QueryRay(const ri::math::Vec3& origin,
                                                    const ri::math::Vec3& direction,
                                                    float far) const;
    [[nodiscard]] SpatialIndexMetrics Metrics() const noexcept;
    void ResetMetrics() noexcept;

private:
    struct IndexedEntry {
        std::string id;
        Aabb bounds;
        ri::math::Vec3 center{};
    };

    struct Node {
        Aabb bounds = MakeEmptyAabb();
        std::vector<std::size_t> entryIndices;
        std::size_t left = kInvalidNode;
        std::size_t right = kInvalidNode;
        char axis = '\0';
        float split = 0.0f;
    };

    static constexpr std::size_t kInvalidNode = static_cast<std::size_t>(-1);

    std::size_t BuildNode(const std::vector<std::size_t>& entryIndices,
                          std::size_t depth,
                          const SpatialIndexOptions& options);
    void QueryBoxNode(std::size_t nodeIndex,
                      const Aabb& box,
                      std::vector<std::string>& out,
                      std::vector<bool>& seen) const;
    void QueryRayNode(std::size_t nodeIndex,
                      const Ray& ray,
                      float far,
                      const Aabb& segmentBounds,
                      std::vector<std::string>& out,
                      std::vector<bool>& seen) const;

    std::vector<IndexedEntry> entries_;
    std::vector<Node> nodes_;
    std::size_t rootNode_ = kInvalidNode;
    mutable SpatialIndexMetrics metrics_{};
};

} // namespace ri::spatial
