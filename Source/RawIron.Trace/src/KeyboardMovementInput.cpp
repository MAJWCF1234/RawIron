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
    return BuildKeyboardMovementInput(KeyboardMovementSample{
        .focused = focus.Focused(),
        .forward = focus.IsKeyDown(bindings.forward), .back = focus.IsKeyDown(bindings.back),
        .right = focus.IsKeyDown(bindings.right), .left = focus.IsKeyDown(bindings.left),
        .jump = focus.IsKeyDownSettled(bindings.jump), .sprint = focus.IsKeyDown(bindings.sprint)}, yawDegrees, edges);
}

MovementInput BuildKeyboardMovementInput(const KeyboardMovementSample& sampled,
    const float yawDegrees, KeyboardMovementEdges& edges) {
    const auto sample = sampled.focused ? sampled : KeyboardMovementSample{};
    if (!std::isfinite(yawDegrees)) { edges = {}; return {}; }
    const float yawRadians = ri::math::DegreesToRadians(yawDegrees);
    const ri::math::Vec3 forward{std::sin(yawRadians), 0.0f, std::cos(yawRadians)};
    const ri::math::Vec3 right =
        ri::math::Normalize(ri::math::Cross(ri::math::Vec3{0.0f, 1.0f, 0.0f}, forward));

    const bool jumpHeldNow = sample.jump;
    const bool jumpPressedEdge = jumpHeldNow && !edges.jumpHeldLastFrame;
    edges.jumpHeldLastFrame = jumpHeldNow;

    return MovementInput{
        .moveForward = static_cast<float>(sample.forward) - static_cast<float>(sample.back),
        .moveRight = static_cast<float>(sample.right) - static_cast<float>(sample.left),
        .viewForwardWorld = forward,
        .viewRightWorld = right,
        .sprintHeld = sample.sprint,
        .jumpPressed = jumpPressedEdge,
        .applyShortJumpGravity = !jumpHeldNow,
    };
}

} // namespace ri::trace
