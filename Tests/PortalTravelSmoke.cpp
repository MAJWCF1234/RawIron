#include "RawIron/World/PortalTravel.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

bool Require(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

ri::spatial::Aabb TravelerAt(const float x) {
    return {.min = {x - 0.25f, 0.0f, -0.25f}, .max = {x + 0.25f, 1.8f, 0.25f}};
}

} // namespace

int main() {
    const std::vector<ri::world::PortalTravelVolume> portals{
        {.id = "a", .triggerBounds = {.min = {-1.0f, 0.0f, -1.0f}, .max = {1.0f, 2.0f, 1.0f}},
         .destinationFeet = {10.0f, 0.0f, 0.0f}, .destinationYawDegrees = 90.0f},
        {.id = "b", .triggerBounds = {.min = {9.0f, 0.0f, -1.0f}, .max = {11.0f, 2.0f, 1.0f}},
         .destinationFeet = {2.0f, 0.0f, 0.0f}, .destinationYawDegrees = -90.0f},
    };
    ri::world::PortalTravelerState state{};
    bool ok = true;

    const auto first = ri::world::UpdatePortalTraveler(portals, TravelerAt(0.0f), 1.0f / 60.0f, state);
    ok &= Require(first.traveled && first.portalId == "a", "entering portal A should travel once");
    ok &= Require(first.destinationFeet.x == 10.0f && first.destinationYawDegrees == 90.0f,
                  "portal A should return its authored destination");

    const auto destinationEntry = ri::world::UpdatePortalTraveler(portals, TravelerAt(10.0f), 0.05f, state);
    ok &= Require(!destinationEntry.traveled, "destination overlap during cooldown must not bounce back");
    const auto heldInside = ri::world::UpdatePortalTraveler(portals, TravelerAt(10.0f), 1.0f, state);
    ok &= Require(!heldInside.traveled, "remaining inside a portal must not retrigger after cooldown");

    (void)ri::world::UpdatePortalTraveler(portals, TravelerAt(5.0f), 0.01f, state);
    const auto returnTrip = ri::world::UpdatePortalTraveler(portals, TravelerAt(10.0f), 0.01f, state);
    ok &= Require(returnTrip.traveled && returnTrip.portalId == "b",
                  "leaving and re-entering portal B should permit return travel");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
