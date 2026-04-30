#pragma once

#include "RawIron/Spatial/SpatialIndex.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::trace {

struct TraceCollider {
    std::string id;
    ri::spatial::Aabb bounds;
    bool structural = true;
    bool dynamic = false;
    /// Optional gameplay/sim tags (e.g. `dynamicCollider`, `pickup`, layer hints); broadphase ignores unless you filter.
    std::vector<std::string> simulationTags{};
    /// Opaque bitmask for game collision / simulation routing (filters live in game code).
    std::uint32_t simulationFlags = 0U;
};

struct TraceOptions {
    bool structuralOnly = false;
    std::string ignoreId;
};

struct GroundTraceOptions {
    float maxDistance = 2.0f;
    bool structuralOnly = false;
    std::string ignoreId;
    float minNormalY = 0.5f;
};

struct TraceHit {
    std::string id;
    ri::spatial::Aabb bounds;
    ri::math::Vec3 point{};
    ri::math::Vec3 normal{};
    float penetration = 0.0f;
    float time = 0.0f;
    ri::spatial::Aabb endBox = ri::spatial::MakeEmptyAabb();
};

struct SlideMoveResult {
    ri::math::Vec3 positionDelta{};
    ri::math::Vec3 remainingDelta{};
    ri::spatial::Aabb endBox = ri::spatial::MakeEmptyAabb();
    std::vector<TraceHit> hits;
    bool blocked = false;
};

struct TraceSceneMetrics {
    std::size_t colliderCount = 0;
    std::size_t staticColliderCount = 0;
    std::size_t structuralStaticColliderCount = 0;
    std::size_t dynamicColliderCount = 0;
    std::size_t boxQueries = 0;
    std::size_t rayQueries = 0;
    std::size_t sweptBoxQueries = 0;
    std::size_t traceBoxQueries = 0;
    std::size_t traceRayQueries = 0;
    std::size_t staticCandidates = 0;
    std::size_t dynamicCandidates = 0;
};

class TraceScene {
public:
    TraceScene() = default;
    explicit TraceScene(std::vector<TraceCollider> colliders, ri::spatial::SpatialIndexOptions indexOptions = {});

    void SetColliders(std::vector<TraceCollider> colliders, ri::spatial::SpatialIndexOptions indexOptions = {});

    /// Erases colliders matching `pred` and rebuilds static/structural broadphase. Use for streaming teardown
    /// or when filter state is not expressible as a simple id set.
    [[nodiscard]] std::size_t EraseCollidersIf(const std::function<bool(const TraceCollider&)>& shouldRemove,
                                               ri::spatial::SpatialIndexOptions indexOptions = {});

    /// Erases colliders by id; O(m) in `ids` with hash lookup, O(n) list scan. Rebuilds broadphase.
    [[nodiscard]] std::size_t EraseCollidersWithIds(const std::vector<std::string_view>& ids,
                                                    ri::spatial::SpatialIndexOptions indexOptions = {});

    /// In-place bounds update for colliders marked `dynamic` (skips BSP rebuild). Used for moving kinematic props.
    [[nodiscard]] bool TrySetDynamicColliderBounds(std::string_view id, const ri::spatial::Aabb& bounds);

    [[nodiscard]] std::size_t ColliderCount() const;

    [[nodiscard]] std::vector<std::string> QueryCollidablesForBox(const ri::spatial::Aabb& box,
                                                                  bool structuralOnly = false) const;
    [[nodiscard]] std::vector<std::string> QueryCollidablesForRay(const ri::math::Vec3& origin,
                                                                  const ri::math::Vec3& direction,
                                                                  float far,
                                                                  bool structuralOnly = false) const;

    [[nodiscard]] std::optional<TraceHit> TraceBox(const ri::spatial::Aabb& queryBox,
                                                   const TraceOptions& options = {}) const;
    [[nodiscard]] std::optional<TraceHit> TraceRay(const ri::math::Vec3& origin,
                                                   const ri::math::Vec3& direction,
                                                   float far,
                                                   const TraceOptions& options = {}) const;
    [[nodiscard]] std::optional<TraceHit> TraceSweptBox(const ri::spatial::Aabb& queryBox,
                                                        const ri::math::Vec3& delta,
                                                        const TraceOptions& options = {}) const;
    [[nodiscard]] SlideMoveResult SlideMoveBox(const ri::spatial::Aabb& queryBox,
                                               const ri::math::Vec3& delta,
                                               std::size_t maxBumps = 4,
                                               float epsilon = 0.001f,
                                               const TraceOptions& options = {}) const;
    [[nodiscard]] std::optional<TraceHit> FindGroundHit(const ri::math::Vec3& origin,
                                                        const GroundTraceOptions& options = {}) const;
    [[nodiscard]] TraceSceneMetrics Metrics() const noexcept;
    [[nodiscard]] ri::spatial::SpatialIndexMetrics StaticIndexMetrics() const noexcept;
    [[nodiscard]] ri::spatial::SpatialIndexMetrics StructuralIndexMetrics() const noexcept;
    void ResetMetrics() noexcept;

private:
    const TraceCollider* FindCollider(std::string_view id) const;
    [[nodiscard]] std::vector<const TraceCollider*> CollectCandidatesForBox(const ri::spatial::Aabb& box,
                                                                            bool structuralOnly,
                                                                            std::string_view ignoreId) const;
    [[nodiscard]] std::vector<const TraceCollider*> CollectCandidatesForRay(const ri::math::Vec3& origin,
                                                                            const ri::math::Vec3& direction,
                                                                            float far,
                                                                            bool structuralOnly,
                                                                            std::string_view ignoreId) const;

    std::vector<TraceCollider> colliders_;
    std::vector<std::size_t> dynamicColliderIndices_;
    /// Broad-phase AABB BSP for non-dynamic colliders; candidates are refined with narrow-phase box/ray/sweep.
    ri::spatial::BspSpatialIndex staticIndex_;
    /// Same tree class, filtered to `structural` static colliders (gameplay/visibility queries).
    ri::spatial::BspSpatialIndex structuralIndex_;
    mutable TraceSceneMetrics metrics_{};
};

} // namespace ri::trace
