#include "RawIron/World/PortalTravel.h"

#include <algorithm>
#include <cmath>

namespace ri::world {

namespace {

bool IsFinite(const ri::math::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

const PortalTravelVolume* FindOverlappingPortal(
    const std::span<const PortalTravelVolume> portals,
    const ri::spatial::Aabb& travelerBounds) {
    if (ri::spatial::IsEmpty(travelerBounds)) {
        return nullptr;
    }
    for (const PortalTravelVolume& portal : portals) {
        if (!portal.enabled || portal.id.empty() || ri::spatial::IsEmpty(portal.triggerBounds)
            || !IsFinite(portal.destinationFeet) || !std::isfinite(portal.destinationYawDegrees)) {
            continue;
        }
        if (ri::spatial::Intersects(travelerBounds, portal.triggerBounds)) {
            return &portal;
        }
    }
    return nullptr;
}

} // namespace

PortalTravelResult UpdatePortalTraveler(
    const std::span<const PortalTravelVolume> portals,
    const ri::spatial::Aabb& travelerBounds,
    const float deltaSeconds,
    PortalTravelerState& state) {
    const float safeDelta = std::isfinite(deltaSeconds) ? std::max(deltaSeconds, 0.0f) : 0.0f;
    state.cooldownRemainingSeconds = std::max(0.0f, state.cooldownRemainingSeconds - safeDelta);

    const PortalTravelVolume* portal = FindOverlappingPortal(portals, travelerBounds);
    if (portal == nullptr) {
        state.occupiedPortalId.clear();
        return {};
    }

    if (state.occupiedPortalId == portal->id) {
        return {};
    }

    // Remember every entered trigger, including a destination entered during cooldown. This makes
    // travel edge-triggered and requires the traveler to leave before that portal can activate.
    state.occupiedPortalId = portal->id;
    if (state.cooldownRemainingSeconds > 0.0f) {
        return {};
    }

    state.cooldownRemainingSeconds = std::clamp(
        std::isfinite(portal->reentryCooldownSeconds) ? portal->reentryCooldownSeconds : 0.35f,
        0.0f,
        10.0f);
    return PortalTravelResult{
        .traveled = true,
        .portalId = portal->id,
        .destinationFeet = portal->destinationFeet,
        .destinationYawDegrees = portal->destinationYawDegrees,
        .preserveVelocity = portal->preserveVelocity,
    };
}

} // namespace ri::world
