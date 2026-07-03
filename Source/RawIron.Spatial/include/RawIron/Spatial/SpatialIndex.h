#pragma once

#include "RawIron/Spatial/Aabb.h"

#include <cstddef>
#include <cstdint>
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

/// Ray broad-phase candidate keyed by the entry's position in the vector passed to `Rebuild`.
/// `distance` is the ray-entry AABB entry distance along the normalized ray direction.
struct SpatialRayCandidate {
    std::size_t sourceIndex = 0;
    float distance = 0.0f;
};

/// Axis-aligned BSP broad-phase for static collider bounds. The native trace scene (`ri::trace::TraceScene`)
/// keeps two trees (all static vs structural-only) for movement and collision; split planes use the
/// longest world axis (X, Y, or Z) with median center partitioning.
///
/// Queries come in two flavors:
/// - id queries (`QueryBox` / `QueryRay`) return entry ids for legacy string-keyed consumers;
/// - source-index queries (`QueryBoxSourceIndices` / `QueryRayCandidates`) return each candidate's position in
///   the vector originally passed to `Rebuild`, avoiding string allocation on hot paths and staying correct
///   when ids are duplicated. Entries skipped at rebuild (empty id or empty bounds) are never returned.
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
    [[nodiscard]] std::vector<std::size_t> QueryBoxSourceIndices(const Aabb& box) const;
    [[nodiscard]] std::vector<SpatialRayCandidate> QueryRayCandidates(const ri::math::Vec3& origin,
                                                                      const ri::math::Vec3& direction,
                                                                      float far) const;
    [[nodiscard]] SpatialIndexMetrics Metrics() const noexcept;
    void ResetMetrics() noexcept;

private:
    struct IndexedEntry {
        std::string id;
        Aabb bounds;
        ri::math::Vec3 center{};
        std::size_t sourceIndex = 0;
    };

    struct InternalRayHit {
        std::size_t entryIndex = 0;
        float distance = 0.0f;
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
    /// Bumps the per-query visit epoch; entries stamped with the current epoch are already reported.
    std::uint32_t BeginQueryEpoch() const;
    void QueryBoxNode(std::size_t nodeIndex,
                      const Aabb& box,
                      std::uint32_t epoch,
                      std::vector<std::size_t>& out) const;
    void QueryRayNode(std::size_t nodeIndex,
                      const Ray& ray,
                      float far,
                      const Aabb& segmentBounds,
                      std::uint32_t epoch,
                      std::vector<InternalRayHit>& out) const;
    [[nodiscard]] std::vector<std::size_t> CollectBoxCandidates(const Aabb& box) const;
    [[nodiscard]] std::vector<InternalRayHit> CollectRayCandidates(const ri::math::Vec3& origin,
                                                                   const ri::math::Vec3& direction,
                                                                   float far) const;

    std::vector<IndexedEntry> entries_;
    std::vector<Node> nodes_;
    std::size_t rootNode_ = kInvalidNode;
    /// Reused visit stamps (epoch per query) so queries do not allocate a fresh seen-buffer each call.
    mutable std::vector<std::uint32_t> visitStamps_;
    mutable std::uint32_t visitEpoch_ = 0;
    mutable SpatialIndexMetrics metrics_{};
};

} // namespace ri::spatial
