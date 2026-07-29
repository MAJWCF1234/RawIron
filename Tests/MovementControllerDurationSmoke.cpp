#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/MovementController.h"

#include <cmath>
#include <cstdlib>

int main() {
    ri::trace::TraceScene scene{};
    ri::trace::MovementControllerState state{
        .body = {
            .bounds = {
                .min = {-0.5f, 0.0f, -0.5f},
                .max = {0.5f, 1.0f, 0.5f},
            },
            .velocity = {4.0f, 0.0f, 0.0f},
        },
        .onGround = false,
    };
    ri::trace::MovementControllerOptions options{};
    options.gravity = 0.0f;
    options.fallGravityMultiplier = 1.0f;
    options.simulateStamina = false;
    options.kinematic.linearDamping = 1.0f;
    options.kinematic.angularDamping = 1.0f;
    options.kinematic.airDrag = 1.0f;
    options.kinematic.minVelocity = 0.0f;

    const ri::trace::MovementControllerResult advanced =
        ri::trace::SimulateMovementControllerStep(scene, state, {}, 0.35f, options);
    const float centerX = ri::spatial::Center(advanced.state.body.bounds).x;
    if (std::fabs(centerX - 1.4f) > 0.001f
        || advanced.sliceCount != 4U
        || std::fabs(advanced.consumedSeconds - 0.35f) > 0.0001f
        || advanced.hitSliceBudget) {
        return EXIT_FAILURE;
    }

    const ri::trace::MovementControllerResult rejected =
        ri::trace::SimulateMovementControllerStep(scene, state, {}, -1.0f, options);
    if (rejected.sliceCount != 0U || rejected.consumedSeconds != 0.0f || rejected.hitSliceBudget) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
