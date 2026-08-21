#include "RawIron/Trace/TeleportTargeting.h"

#include <cstdlib>
#include <iostream>

namespace {
bool Require(const bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
}

int main() {
    const ri::trace::TraceScene openScene({{
        .id = "floor",
        .bounds = {.min = {-10.0f, -0.2f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}},
        .structural = true}});
    bool ok = true;
    const ri::trace::TeleportTargetingResult landing = ri::trace::ResolveTeleportTarget(
        openScene, {0.0f, 1.4f, 0.0f}, {0.0f, 0.15f, 1.0f});
    ok &= Require(landing.hit && landing.validLanding && landing.hitNormal.y > 0.9f,
                  "ballistic teleport arc should accept an open floor landing");
    ok &= Require(landing.arcPoints.size() > 2U && landing.destinationFeet.y > 0.0f,
                  "teleport result should preserve a renderable arc and inset feet destination");

    const ri::trace::TraceScene blockedScene({
        {.id = "floor",
         .bounds = {.min = {-10.0f, -0.2f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}},
         .structural = true},
        {.id = "wall",
         .bounds = {.min = {-1.0f, 0.0f, 2.0f}, .max = {1.0f, 3.0f, 2.2f}},
         .structural = true}});
    const ri::trace::TeleportTargetingResult wall = ri::trace::ResolveTeleportTarget(
        blockedScene, {0.0f, 1.4f, 0.0f}, {0.0f, 0.1f, 1.0f});
    ok &= Require(wall.hit && !wall.validLanding && wall.hitNormal.y < 0.5f,
                  "teleport targeting must reject a wall hit before the floor");
    const ri::trace::TeleportTargetingResult invalid = ri::trace::ResolveTeleportTarget(
        openScene, {}, {}, {});
    ok &= Require(!invalid.hit && invalid.arcPoints.empty(),
                  "invalid teleport input should fail without trace work");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
