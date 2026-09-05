#include "RawIron/World/InteractivePropGrab.h"

#include <algorithm>
#include <cmath>

namespace ri::world {
namespace {
bool Finite(const ri::math::Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
}

bool BeginRayPropGrab(InteractivePropGrab& grab, std::span<InteractivePropState> props,
    std::uint32_t owner, const ri::math::Vec3& origin, const ri::math::Vec3& direction,
    float maximumDistance) {
    if (grab.propIndex >= 0 || owner == 0 || !Finite(origin) || !Finite(direction)
        || !std::isfinite(maximumDistance) || maximumDistance < 0.25f) return false;
    const auto selection = SelectInteractiveProp(props, origin, direction, maximumDistance);
    if (selection.propIndex < 0 || !BeginInteractivePropGrab(props, selection.propIndex, owner)) return false;
    grab = {.propIndex = selection.propIndex, .owner = owner,
        .distance = std::clamp(selection.distance, 0.25f, maximumDistance)};
    return true;
}

bool UpdateRayPropGrab(InteractivePropGrab& grab, std::span<InteractivePropState> props,
    const ri::math::Vec3& origin, const ri::math::Vec3& direction, float deltaSeconds) {
    if (grab.propIndex < 0) return false;
    const float length = ri::math::Length(direction);
    if (!Finite(origin) || !Finite(direction) || !std::isfinite(length) || length < 1e-6f
        || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        ReleaseRayPropGrab(grab, props, false);
        return false;
    }
    const auto target = origin + direction * (grab.distance / length);
    grab.releaseVelocity = {};
    if (grab.hasPreviousTarget && deltaSeconds > 1e-4f) {
        grab.releaseVelocity = (target - grab.previousTarget) / deltaSeconds;
        const float speed = ri::math::Length(grab.releaseVelocity);
        if (!std::isfinite(speed)) grab.releaseVelocity = {};
        else if (speed > 12.0f) grab.releaseVelocity = grab.releaseVelocity * (12.0f / speed);
    }
    if (!MoveInteractivePropGrab(props, grab.propIndex, grab.owner, target)) {
        ReleaseRayPropGrab(grab, props, false);
        return false;
    }
    grab.previousTarget = target;
    grab.hasPreviousTarget = true;
    return true;
}

void ReleaseRayPropGrab(InteractivePropGrab& grab, std::span<InteractivePropState> props, bool throwProp) {
    if (grab.propIndex >= 0)
        (void)EndInteractivePropGrab(props, grab.propIndex, grab.owner,
            throwProp ? grab.releaseVelocity : ri::math::Vec3{});
    grab = {};
}
} // namespace ri::world
