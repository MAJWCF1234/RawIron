#pragma once

#include "RawIron/World/InteractivePropField.h"

namespace ri::world {

// Host-independent ray grab/throw state. Hosts provide input and ownership, not physics rules.
struct InteractivePropGrab {
    int propIndex = -1;
    std::uint32_t owner = 0;
    float distance = 1.0f;
    ri::math::Vec3 previousTarget{};
    ri::math::Vec3 releaseVelocity{};
    bool hasPreviousTarget = false;
};

bool BeginRayPropGrab(InteractivePropGrab& grab, std::span<InteractivePropState> props,
    std::uint32_t owner, const ri::math::Vec3& origin, const ri::math::Vec3& direction,
    float maximumDistance = 4.5f);
bool UpdateRayPropGrab(InteractivePropGrab& grab, std::span<InteractivePropState> props,
    const ri::math::Vec3& origin, const ri::math::Vec3& direction, float deltaSeconds);
// Tracking/focus loss must release without a synthetic throw.
void ReleaseRayPropGrab(InteractivePropGrab& grab, std::span<InteractivePropState> props,
    bool throwProp = true);

} // namespace ri::world
