#include "RawIron/Trace/KinematicPhysics.h"
#include "RawIron/Trace/MovementController.h"
#include "RawIron/Trace/TraceScene.h"

#include <cmath>
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

} // namespace

int main() {
    bool ok = true;

    const ri::trace::TraceCollider floor{
        .id = "floor",
        .bounds = {.min = {-8.0f, -0.2f, -8.0f}, .max = {8.0f, 0.0f, 8.0f}},
        .structural = true,
        .dynamic = false,
        .simulationFlags = ri::trace::kTraceFlagWalkable,
    };
    const ri::trace::TraceCollider arenaWall{
        .id = "arena",
        .bounds = {.min = {1.0f, 0.0f, -2.0f}, .max = {1.2f, 2.0f, 2.0f}},
        .structural = false,
        .dynamic = false,
        .simulationFlags = ri::trace::kTraceFlagPropArena,
    };
    // Taller than MovementControllerOptions::maxStepUpHeight so the hull cannot step over it.
    const ri::trace::TraceCollider prop{
        .id = "prop",
        .bounds = {.min = {0.8f, 0.0f, -0.2f}, .max = {1.2f, 1.4f, 0.2f}},
        .structural = false,
        .dynamic = true,
        .simulationFlags = ri::trace::kTraceFlagInteractiveProp,
    };

    ri::trace::TraceScene scene({floor, arenaWall, prop});

    const ri::spatial::Aabb playerBox{
        .min = {0.0f, 0.0f, -0.25f},
        .max = {0.5f, 1.8f, 0.25f},
    };

    const auto legacy = scene.TraceSweptBox(
        playerBox, {1.0f, 0.0f, 0.0f}, {.structuralOnly = true});
    ok &= Require(!legacy.has_value(),
                  "legacy structural-only sweep must ignore dynamic props and arena walls");

    const auto withProps = scene.TraceSweptBox(
        playerBox,
        {1.0f, 0.0f, 0.0f},
        {.structuralOnly = true, .includeDynamicFlags = ri::trace::kTraceFlagInteractiveProp});
    ok &= Require(withProps.has_value() && withProps->id == "prop",
                  "structural hull with includeDynamicFlags must collide with interactive props");

    const auto excludeArena = scene.TraceSweptBox(
        playerBox,
        {1.5f, 0.0f, 0.0f},
        {.structuralOnly = false, .excludeFlags = ri::trace::kTraceFlagPropArena});
    ok &= Require(excludeArena.has_value() && excludeArena->id == "prop",
                  "excludeFlags must skip prop-arena walls while still hitting props");

    ri::trace::MovementControllerState movement{};
    movement.body.bounds = playerBox;
    movement.onGround = true;
    ri::trace::MovementControllerOptions options{};
    options.simulateStamina = false;
    options.gravity = 0.0f;
    options.maxGroundSpeed = 8.0f;
    options.maxStepUpHeight = 0.0f;
    options.kinematic.structuralOnly = true;
    options.kinematic.includeDynamicFlags = ri::trace::kTraceFlagInteractiveProp;
    options.kinematic.maxStepUpHeight = 0.0f;
    options.kinematic.gravity = 0.0f;

    ri::trace::MovementInput input{};
    input.moveForward = 0.0f;
    input.moveRight = 1.0f;
    input.viewForwardWorld = {0.0f, 0.0f, 1.0f};
    input.viewRightWorld = {1.0f, 0.0f, 0.0f};

    const float startX = ri::spatial::Center(movement.body.bounds).x;
    for (int step = 0; step < 30; ++step) {
        const auto result =
            ri::trace::SimulateMovementControllerStep(scene, movement, input, 1.0f / 60.0f, options);
        movement = result.state;
    }
    const float endX = ri::spatial::Center(movement.body.bounds).x;
    ok &= Require(endX < 0.75f,
                  "player movement must be blocked by a dynamic interactive prop collider");
    ok &= Require(endX > startX + 0.05f,
                  "player should still advance until contacting the prop");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
