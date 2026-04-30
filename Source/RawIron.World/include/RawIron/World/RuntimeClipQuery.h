#pragma once

#include "RawIron/World/RuntimeState.h"
#include "RawIron/World/VolumeDescriptors.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ri::world {

/// Per-volume AABB cache for repeat queries in a frame or across systems. Clear on level reload.
struct RuntimeClipBoundsCache {
    std::unordered_map<std::string, ri::spatial::Aabb> boundsByVolumeId;
    void Clear() {
        boundsByVolumeId.clear();
    }
};

enum class RuntimeClipVolumeKind {
    Clipping,
    FilteredCollision,
};

[[nodiscard]] constexpr std::uint32_t ClipVolumeModeBit(ClipVolumeMode mode) noexcept {
    return 1U << static_cast<std::uint32_t>(mode);
}

[[nodiscard]] std::uint32_t ClipVolumeModesMask(const std::vector<ClipVolumeMode>& modes);

struct RuntimeClipBoxQueryOptions {
    /// Bitmask over \ref ClipVolumeMode (AND with volume modes); default selects all.
    std::uint32_t clipModeMask = ~0U;
    /// Bitmask over collision channels (see \ref CollisionChannel); default selects all.
    std::uint32_t collisionChannelMask = ~0U;
    bool includeDisabledClipping = false;
    /// Optional extra pruning after layer filters (visibility/collision channel masks).
    std::function<bool(std::string_view volumeId, RuntimeClipVolumeKind kind)> predicate{};
};

struct RuntimeClipRayQueryOptions : RuntimeClipBoxQueryOptions {
    /// Half-extent padding applied to the swept segment AABB (fat ray / sphere cast broadphase).
    float sweptThickness = 0.0f;
};

struct RuntimeClipQueryCounters {
    std::size_t boxQueries = 0;
    std::size_t sweptRayQueries = 0;
};

/// Box and swept-ray broadphase over authoring clip (\ref ClippingRuntimeVolume) and filtered collision
/// (\ref FilteredCollisionRuntimeVolume) sets. Results are sorted by `id` for stable ordering.
class RuntimeClipQueryBroadphase {
public:
    explicit RuntimeClipQueryBroadphase(const RuntimeEnvironmentService& environment) : environment_(environment) {}

    [[nodiscard]] std::vector<std::string> QueryBox(const ri::spatial::Aabb& box,
                                                    const RuntimeClipBoxQueryOptions& options = {},
                                                    RuntimeClipBoundsCache* cache = nullptr,
                                                    RuntimeClipQueryCounters* counters = nullptr) const;

    /// Ray-oriented candidate collection using an AABB around the segment [\p origin, origin + dir * far],
    /// optionally inflated by \ref RuntimeClipRayQueryOptions::sweptThickness.
    [[nodiscard]] std::vector<std::string> QueryRaySweptBounds(const ri::math::Vec3& origin,
                                                                 const ri::math::Vec3& direction,
                                                                 float farDistance,
                                                                 const RuntimeClipRayQueryOptions& options = {},
                                                                 RuntimeClipBoundsCache* cache = nullptr,
                                                                 RuntimeClipQueryCounters* counters = nullptr) const;

private:
    const RuntimeEnvironmentService& environment_;

    [[nodiscard]] ri::spatial::Aabb ResolveBounds(const RuntimeVolume& volume,
                                                RuntimeClipBoundsCache* cache) const;
    [[nodiscard]] static ri::spatial::Aabb BuildSweptSegmentAabb(const ri::math::Vec3& origin,
                                                                 const ri::math::Vec3& direction,
                                                                 float farDistance,
                                                                 float sweptThickness);
};

} // namespace ri::world
