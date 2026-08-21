#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Spatial/Aabb.h"

#include <span>
#include <string>

namespace ri::world {

/// A gameplay portal trigger and its destination. Geometry remains separate from rendering so
/// games may represent portals with any mesh, effect, or streaming boundary.
struct PortalTravelVolume {
    std::string id{};
    ri::spatial::Aabb triggerBounds = ri::spatial::MakeEmptyAabb();
    ri::math::Vec3 destinationFeet{};
    float destinationYawDegrees = 0.0f;
    float reentryCooldownSeconds = 0.35f;
    bool preserveVelocity = false;
    bool enabled = true;
};

struct PortalTravelerState {
    /// Portal occupied on the previous update; travel only fires on an overlap edge.
    std::string occupiedPortalId{};
    float cooldownRemainingSeconds = 0.0f;
};

struct PortalTravelResult {
    bool traveled = false;
    std::string portalId{};
    ri::math::Vec3 destinationFeet{};
    float destinationYawDegrees = 0.0f;
    bool preserveVelocity = false;
};

/// Evaluates portal overlap for one traveler. Disabled/malformed portals are ignored.
/// Entering a destination portal during cooldown arms its exit edge without bouncing back.
[[nodiscard]] PortalTravelResult UpdatePortalTraveler(
    std::span<const PortalTravelVolume> portals,
    const ri::spatial::Aabb& travelerBounds,
    float deltaSeconds,
    PortalTravelerState& state);

} // namespace ri::world
