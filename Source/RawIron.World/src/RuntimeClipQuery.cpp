#include "RawIron/World/RuntimeClipQuery.h"

#include "RawIron/World/RuntimeState.h"

#include <algorithm>
#include <cmath>

namespace ri::world {
namespace {

[[nodiscard]] ri::math::Vec3 ComponentMin(const ri::math::Vec3& a, const ri::math::Vec3& b) noexcept {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

[[nodiscard]] ri::math::Vec3 ComponentMax(const ri::math::Vec3& a, const ri::math::Vec3& b) noexcept {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

[[nodiscard]] std::uint32_t CollisionChannelsMask(const std::vector<std::string>& rawChannels) {
    const CollisionChannelResolveResult resolved = ResolveCollisionChannelAuthoring(rawChannels);
    return resolved.mask;
}

} // namespace

std::uint32_t ClipVolumeModesMask(const std::vector<ClipVolumeMode>& modes) {
    std::uint32_t mask = 0U;
    for (const ClipVolumeMode mode : modes) {
        mask |= ClipVolumeModeBit(mode);
    }
    return mask;
}

ri::spatial::Aabb RuntimeClipQueryBroadphase::ResolveBounds(const RuntimeVolume& volume,
                                                            RuntimeClipBoundsCache* cache) const {
    if (cache != nullptr) {
        if (const auto found = cache->boundsByVolumeId.find(volume.id); found != cache->boundsByVolumeId.end()) {
            return found->second;
        }
    }
    const ri::spatial::Aabb bounds = BuildRuntimeVolumeBounds(volume);
    if (cache != nullptr && !volume.id.empty()) {
        cache->boundsByVolumeId.insert_or_assign(volume.id, bounds);
    }
    return bounds;
}

ri::spatial::Aabb RuntimeClipQueryBroadphase::BuildSweptSegmentAabb(const ri::math::Vec3& origin,
                                                                    const ri::math::Vec3& direction,
                                                                    const float farDistance,
                                                                    const float sweptThickness) {
    if (!std::isfinite(farDistance) || farDistance <= 0.0f || ri::math::LengthSquared(direction) < 1e-24f) {
        return ri::spatial::MakeEmptyAabb();
    }
    const ri::math::Vec3 dir = ri::math::Normalize(direction);
    const ri::math::Vec3 end = origin + (dir * farDistance);
    ri::math::Vec3 mn = ComponentMin(origin, end);
    ri::math::Vec3 mx = ComponentMax(origin, end);
    if (sweptThickness > 0.0f && std::isfinite(sweptThickness)) {
        const ri::math::Vec3 pad{sweptThickness, sweptThickness, sweptThickness};
        mn = mn - pad;
        mx = mx + pad;
    }
    return ri::spatial::Aabb{.min = mn, .max = mx};
}

std::vector<std::string> RuntimeClipQueryBroadphase::QueryBox(const ri::spatial::Aabb& box,
                                                              const RuntimeClipBoxQueryOptions& options,
                                                              RuntimeClipBoundsCache* cache,
                                                              RuntimeClipQueryCounters* counters) const {
    if (counters != nullptr) {
        counters->boxQueries += 1;
    }
    if (ri::spatial::IsEmpty(box)) {
        return {};
    }

    std::vector<std::string> ids;
    ids.reserve(16);

    for (const ClippingRuntimeVolume& volume : environment_.GetClippingVolumes()) {
        if (!options.includeDisabledClipping && !volume.enabled) {
            continue;
        }
        const std::vector<ClipVolumeMode> modes = ParseClipVolumeModes(volume.modes);
        const std::uint32_t volumeMask = ClipVolumeModesMask(modes);
        if ((volumeMask & options.clipModeMask) == 0U) {
            continue;
        }
        const ri::spatial::Aabb bounds = ResolveBounds(volume, cache);
        if (!ri::spatial::Intersects(bounds, box)) {
            continue;
        }
        if (options.predicate && !options.predicate(volume.id, RuntimeClipVolumeKind::Clipping)) {
            continue;
        }
        ids.push_back(volume.id);
    }

    for (const FilteredCollisionRuntimeVolume& volume : environment_.GetFilteredCollisionVolumes()) {
        const std::uint32_t channelMask = CollisionChannelsMask(volume.channels);
        if ((channelMask & options.collisionChannelMask) == 0U) {
            continue;
        }
        const ri::spatial::Aabb bounds = ResolveBounds(volume, cache);
        if (!ri::spatial::Intersects(bounds, box)) {
            continue;
        }
        if (options.predicate && !options.predicate(volume.id, RuntimeClipVolumeKind::FilteredCollision)) {
            continue;
        }
        ids.push_back(volume.id);
    }

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

std::vector<std::string> RuntimeClipQueryBroadphase::QueryRaySweptBounds(const ri::math::Vec3& origin,
                                                                         const ri::math::Vec3& direction,
                                                                         const float farDistance,
                                                                         const RuntimeClipRayQueryOptions& options,
                                                                         RuntimeClipBoundsCache* cache,
                                                                         RuntimeClipQueryCounters* counters) const {
    if (counters != nullptr) {
        counters->sweptRayQueries += 1;
    }
    const ri::spatial::Aabb sweepBox =
        BuildSweptSegmentAabb(origin, direction, farDistance, options.sweptThickness);
    if (ri::spatial::IsEmpty(sweepBox)) {
        return {};
    }
    RuntimeClipBoxQueryOptions boxOpts{};
    boxOpts.clipModeMask = options.clipModeMask;
    boxOpts.collisionChannelMask = options.collisionChannelMask;
    boxOpts.includeDisabledClipping = options.includeDisabledClipping;
    boxOpts.predicate = options.predicate;
    return QueryBox(sweepBox, boxOpts, cache, nullptr);
}

} // namespace ri::world
