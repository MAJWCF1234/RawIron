#include "RawIron/World/InteractivePropTracePhysics.h"

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
        .bounds = {.min = {-4.0f, -0.2f, -4.0f}, .max = {4.0f, 0.0f, 4.0f}},
        .structural = true,
        .dynamic = false,
    };
    const ri::trace::TraceCollider wall{
        .id = "wall",
        .bounds = {.min = {1.5f, 0.0f, -2.0f}, .max = {1.7f, 2.0f, 2.0f}},
        .structural = true,
        .dynamic = false,
    };

    std::vector<ri::world::InteractivePropState> props{{
        .id = "drop",
        .position = {0.0f, 1.4f, 0.0f},
        .halfExtents = {0.15f, 0.15f, 0.15f},
        .velocity = {0.0f, 0.0f, 0.0f},
    }};

    std::vector<ri::trace::TraceCollider> colliders{floor, wall};
    colliders.push_back(ri::world::MakeInteractivePropTraceCollider("test", 0, props[0]));
    ri::trace::TraceScene scene(colliders);

    ri::world::InteractivePropTraceOptions options{};
    options.gravity = 18.0f;
    options.bounciness = 0.05f;
    options.clampToArenaBounds = false;

    for (int step = 0; step < 90; ++step) {
        (void)ri::world::StepInteractivePropFieldAgainstTrace(
            scene, props, "test", 1.0f / 60.0f, options);
    }

    ok &= Require(props[0].position.y < 0.35f && props[0].position.y > 0.10f,
                  "prop dropped onto TraceScene floor should rest near floor top");
    ok &= Require(std::fabs(props[0].velocity.y) < 0.75f,
                  "settled prop vertical speed should calm after Trace contacts");

    props[0].position = {0.0f, 0.4f, 0.0f};
    props[0].velocity = {8.0f, 0.0f, 0.0f};
    (void)scene.TrySetDynamicColliderBounds(
        ri::world::MakeInteractivePropColliderId("test", 0),
        {.min = props[0].position - props[0].halfExtents,
         .max = props[0].position + props[0].halfExtents});

    ri::world::InteractivePropStepReport report{};
    for (int step = 0; step < 45; ++step) {
        report = ri::world::StepInteractivePropFieldAgainstTrace(
            scene, props, "test", 1.0f / 60.0f, options);
    }
    ok &= Require(props[0].position.x < 1.45f,
                  "prop thrown into a TraceScene wall should stop short of penetrating it");
    ok &= Require(report.boundaryContacts > 0 || props[0].velocity.x < 7.0f,
                  "wall contact should register impact or reverse/kill horizontal speed");

    props[0].grabbed = true;
    props[0].position = {0.5f, 1.0f, 0.5f};
    props[0].velocity = {3.0f, 0.0f, 0.0f};
    const ri::math::Vec3 grabbedVelocity = props[0].velocity;
    (void)ri::world::StepInteractivePropFieldAgainstTrace(
        scene, props, "test", 1.0f / 60.0f, options);
    ok &= Require(props[0].position.x == 0.5f && props[0].velocity.x == grabbedVelocity.x,
                  "grabbed props must not be integrated by Trace stepping");

    props[0].grabbed = false;
    props[0].position = {0.2f, 0.3f, 0.0f};
    props[0].velocity = {};
    const ri::spatial::Aabb actorBounds{
        .min = {-0.25f, 0.0f, -0.25f},
        .max = {0.25f, 1.8f, 0.25f},
    };
    const ri::world::InteractivePropActorPushReport push = ri::world::PushInteractivePropsFromActor(
        &scene,
        props,
        "test",
        actorBounds,
        {4.0f, 0.0f, 0.0f});
    ok &= Require(push.contacts == 1U, "overlapping actor hull should contact the prop");
    ok &= Require(props[0].position.x > 0.25f, "actor push should separate the prop outside the hull");
    ok &= Require(props[0].velocity.x > 0.5f, "actor velocity should impart a horizontal push impulse");

    props[0].grabbed = true;
    props[0].position = {0.2f, 0.3f, 0.0f};
    props[0].velocity = {};
    const ri::world::InteractivePropActorPushReport skipped = ri::world::PushInteractivePropsFromActor(
        &scene, props, "test", actorBounds, {4.0f, 0.0f, 0.0f});
    ok &= Require(skipped.contacts == 0U && props[0].position.x == 0.2f,
                  "grabbed props must ignore actor push resolution");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
