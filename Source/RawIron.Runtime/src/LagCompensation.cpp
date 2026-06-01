#include "RawIron/Runtime/LagCompensation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ri::runtime {
namespace {

ri::math::Vec3 Sub(const ri::math::Vec3& a, const ri::math::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float LenSq(const ri::math::Vec3& v) {
    return ri::math::Dot(v, v);
}

} // namespace

RewindBuffer::RewindBuffer(const std::size_t maxFrames)
    : maxFrames_(std::max<std::size_t>(1U, maxFrames)) {}

void RewindBuffer::Push(PoseFrame frame) {
    frames_.push_back(std::move(frame));
    while (frames_.size() > maxFrames_) {
        frames_.pop_front();
    }
}

std::optional<PoseFrame> RewindBuffer::FindNearest(const std::uint32_t tick) const {
    if (frames_.empty()) {
        return std::nullopt;
    }
    std::size_t best = 0;
    std::uint32_t bestDist = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        const std::uint32_t t = frames_[i].tick;
        const std::uint32_t dist = (t > tick) ? (t - tick) : (tick - t);
        if (dist < bestDist) {
            best = i;
            bestDist = dist;
        }
    }
    return frames_[best];
}

std::size_t RewindBuffer::Size() const noexcept {
    return frames_.size();
}

std::optional<std::string> VerifyHitscan(const PoseFrame& frame, const HitscanVerifyRequest& request) {
    std::optional<std::string> winner{};
    float bestT = request.maxDistance;
    for (const EntityPoseSample& pose : frame.poses) {
        const ri::math::Vec3 to = Sub(pose.position, request.origin);
        const float t = ri::math::Dot(to, request.directionUnit);
        if (t < 0.0f || t > bestT) {
            continue;
        }
        const ri::math::Vec3 closest{
            request.origin.x + request.directionUnit.x * t,
            request.origin.y + request.directionUnit.y * t,
            request.origin.z + request.directionUnit.z * t,
        };
        const float d2 = LenSq(Sub(pose.position, closest));
        if (d2 <= (request.hitRadius * request.hitRadius)) {
            bestT = t;
            winner = pose.entityId;
        }
    }
    return winner;
}

std::optional<std::string> VerifyProjectile(const PoseFrame& frame, const ProjectileVerifyRequest& request) {
    if (request.evalTick < request.fireTick) {
        return std::nullopt;
    }
    const float dtTicks = static_cast<float>(request.evalTick - request.fireTick);
    const ri::math::Vec3 position{
        request.origin.x + request.velocity.x * dtTicks,
        request.origin.y + request.velocity.y * dtTicks,
        request.origin.z + request.velocity.z * dtTicks,
    };
    const float combinedRadius = request.projectileRadius + request.targetRadius;
    const float combinedRadiusSq = combinedRadius * combinedRadius;
    for (const EntityPoseSample& pose : frame.poses) {
        if (LenSq(Sub(pose.position, position)) <= combinedRadiusSq) {
            return pose.entityId;
        }
    }
    return std::nullopt;
}

} // namespace ri::runtime
