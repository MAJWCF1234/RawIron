#include "RawIron/Math/Vec3.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/ObjectPhysics.h"

#include <cmath>
#include <cstdlib>
#include <vector>
#include <limits>

int main() {
    ri::trace::TraceScene scene{};
    std::vector<ri::trace::KinematicObjectSlot> objects{
        {
            .state = {
                .bounds = ri::spatial::Aabb{
                    .min = {-0.5f, 0.0f, -0.5f},
                    .max = {0.5f, 1.0f, 0.5f},
                },
                .velocity = {4.0f, 0.0f, 0.0f},
            },
        },
    };
    ri::trace::KinematicPhysicsOptions physics{};
    physics.gravity = 0.0f;
    physics.linearDamping = 1.0f;
    physics.angularDamping = 1.0f;
    physics.airDrag = 1.0f;
    physics.minVelocity = 0.0f;
    const ri::trace::ObjectPhysicsBatchResult result = ri::trace::StepKinematicObjectBatch(
        scene,
        objects,
        0.75f,
        physics,
        {},
        {},
        {.enableSleep = false});

    const float centerX = ri::spatial::Center(objects.front().state.bounds).x;
    if (result.simulatedCount != 1U || std::fabs(centerX - 3.0f) > 0.001f) {
        return EXIT_FAILURE;
    }

    ri::trace::KinematicPhysicsOptions poisonedOptions{};
    poisonedOptions.gravity = std::numeric_limits<float>::quiet_NaN();
    poisonedOptions.linearDamping = std::numeric_limits<float>::quiet_NaN();
    poisonedOptions.maxSubsteps = std::numeric_limits<std::size_t>::max();
    const ri::trace::KinematicPhysicsOptions sanitized =
        ri::trace::SanitizeKinematicPhysicsOptions(poisonedOptions);
    if (!std::isfinite(sanitized.gravity) || !std::isfinite(sanitized.linearDamping)
        || sanitized.maxSubsteps != ri::trace::kKinematicMaxSubstepsPerSlice) {
        return EXIT_FAILURE;
    }

    ri::trace::KinematicBodyState poisonedState = objects.front().state;
    poisonedState.velocity.x = std::numeric_limits<float>::quiet_NaN();
    const ri::trace::KinematicStepResult safeStep =
        ri::trace::SimulateKinematicBodyStep(scene, poisonedState, 0.016f, physics);
    if (!std::isfinite(safeStep.state.velocity.x)
        || !std::isfinite(ri::spatial::Center(safeStep.state.bounds).x)) {
        return EXIT_FAILURE;
    }

    ri::trace::KinematicBodyState extremeVelocityState = objects.front().state;
    extremeVelocityState.velocity = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
    };
    ri::trace::KinematicPhysicsOptions extremePhysics = physics;
    extremePhysics.gravity = std::numeric_limits<float>::max();
    const ri::trace::KinematicStepResult extremeVelocityStep = ri::trace::SimulateKinematicBodyStep(
        scene,
        extremeVelocityState,
        0.25f,
        extremePhysics,
        {.flow = {std::numeric_limits<float>::max(), 0.0f, 0.0f}});
    if (!std::isfinite(extremeVelocityStep.state.velocity.x)
        || !std::isfinite(extremeVelocityStep.state.velocity.y)
        || !std::isfinite(ri::spatial::Center(extremeVelocityStep.state.bounds).x)) {
        return EXIT_FAILURE;
    }

    const ri::spatial::Aabb invalidOriented = ri::trace::ComputeOrientedBoxWorldBounds(
        {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f},
        {0.5f, 0.5f, 0.5f},
        {});
    if (!ri::spatial::IsEmpty(invalidOriented)) {
        return EXIT_FAILURE;
    }

    std::vector<ri::trace::KinematicObjectSlot> pickupCandidates{
        objects.front(),
        objects.front(),
    };
    pickupCandidates[0].state.bounds.min.x = std::numeric_limits<float>::quiet_NaN();
    pickupCandidates[1].state.bounds = {
        .min = {-0.5f, 0.0f, 0.5f},
        .max = {0.5f, 1.0f, 1.5f},
    };
    ri::trace::HeldObjectState held{};
    ri::trace::ObjectCarryOptions poisonedCarry{};
    poisonedCarry.maxPickupDistance = std::numeric_limits<float>::quiet_NaN();
    poisonedCarry.minPickupAimDot = std::numeric_limits<float>::quiet_NaN();
    if (!ri::trace::TryPickupNearestKinematicObject(
            pickupCandidates, {}, {0.0f, 0.0f, 1.0f}, held, poisonedCarry)
        || held.heldObjectIndex != 1U) {
        return EXIT_FAILURE;
    }

    ri::trace::TraceScene impactScene({ri::trace::TraceCollider{
        .id = "wall",
        .bounds = {.min = {1.5f, -2.0f, -2.0f}, .max = {2.0f, 2.0f, 2.0f}},
        .structural = true,
    }});
    ri::trace::KinematicBodyState impactState{
        .bounds = {.min = {0.0f, 0.0f, -0.5f}, .max = {1.0f, 1.0f, 0.5f}},
        .velocity = {10.0f, 0.0f, 0.0f},
    };
    ri::trace::KinematicPhysicsOptions impactOptions = physics;
    impactOptions.bounciness = 0.0f;
    impactOptions.bounceThreshold = 100.0f;
    const ri::trace::KinematicStepResult impactStep =
        ri::trace::SimulateKinematicBodyStep(impactScene, impactState, 0.1f, impactOptions);
    if (!impactStep.impact.has_value() || impactStep.impact->colliderId != "wall"
        || impactStep.impact->speed < 9.9f || impactStep.impact->velocity.x < 9.9f) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
