#pragma once

#include "RawIron/Math/Vec3.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace ri::runtime {

struct EntityPoseSample {
    std::string entityId;
    ri::math::Vec3 position{};
    ri::math::Vec3 velocity{};
};

struct PoseFrame {
    std::uint32_t tick = 0;
    std::vector<EntityPoseSample> poses;
};

struct HitscanVerifyRequest {
    std::uint32_t shotTick = 0;
    ri::math::Vec3 origin{};
    ri::math::Vec3 directionUnit{};
    float maxDistance = 1000.0f;
    float hitRadius = 0.45f;
};

struct ProjectileVerifyRequest {
    std::uint32_t fireTick = 0;
    std::uint32_t evalTick = 0;
    ri::math::Vec3 origin{};
    ri::math::Vec3 velocity{};
    float projectileRadius = 0.15f;
    float targetRadius = 0.45f;
};

class RewindBuffer {
public:
    explicit RewindBuffer(std::size_t maxFrames = 128);

    void Push(PoseFrame frame);
    [[nodiscard]] std::optional<PoseFrame> FindNearest(std::uint32_t tick) const;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    std::size_t maxFrames_ = 128;
    std::deque<PoseFrame> frames_;
};

[[nodiscard]] std::optional<std::string> VerifyHitscan(const PoseFrame& frame, const HitscanVerifyRequest& request);
[[nodiscard]] std::optional<std::string> VerifyProjectile(const PoseFrame& frame,
                                                          const ProjectileVerifyRequest& request);

} // namespace ri::runtime
