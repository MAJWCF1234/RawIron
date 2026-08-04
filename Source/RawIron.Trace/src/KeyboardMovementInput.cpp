#include "RawIron/Trace/KeyboardMovementInput.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"

#include <cmath>

namespace ri::trace {

float KeyboardAxis(const ri::core::KeyboardFocusGate& focus,
                   const int positiveKey,
                   const int negativeKey) noexcept {
    const bool positive = focus.IsKeyDown(positiveKey);
    const bool negative = focus.IsKeyDown(negativeKey);
    if (positive == negative) {
        return 0.0f;
    }
    return positive ? 1.0f : -1.0f;
}

MovementInput BuildKeyboardMovementInput(const ri::core::KeyboardFocusGate& focus,
                                         const float yawDegrees,
                                         KeyboardMovementEdges& edges,
                                         const KeyboardMovementBindings& bindings) {
    const float yawRadians = ri::math::DegreesToRadians(yawDegrees);
    const ri::math::Vec3 forward{std::sin(yawRadians), 0.0f, std::cos(yawRadians)};
    const ri::math::Vec3 right =
        ri::math::Normalize(ri::math::Cross(ri::math::Vec3{0.0f, 1.0f, 0.0f}, forward));

    const bool jumpHeldNow = focus.IsKeyDownSettled(bindings.jump);
    const bool jumpPressedEdge = jumpHeldNow && !edges.jumpHeldLastFrame;
    edges.jumpHeldLastFrame = jumpHeldNow;

    return MovementInput{
        .moveForward = KeyboardAxis(focus, bindings.forward, bindings.back),
        .moveRight = KeyboardAxis(focus, bindings.right, bindings.left),
        .viewForwardWorld = forward,
        .viewRightWorld = right,
        .sprintHeld = focus.IsKeyDown(bindings.sprint),
        .jumpPressed = jumpPressedEdge,
        .applyShortJumpGravity = !jumpHeldNow,
    };
}

} // namespace ri::trace
