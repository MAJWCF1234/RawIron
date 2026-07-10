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
    return EXIT_SUCCESS;
}
