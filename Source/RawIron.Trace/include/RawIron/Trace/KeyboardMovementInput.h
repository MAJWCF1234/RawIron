#pragma once

#include "RawIron/Core/KeyboardFocus.h"
#include "RawIron/Trace/MovementController.h"

namespace ri::trace {

/// Edge state for jump / interact keys owned by the caller across frames.
struct KeyboardMovementEdges {
    bool jumpHeldLastFrame = false;
};

struct KeyboardMovementBindings {
    int forward = 'W';
    int back = 'S';
    int right = 'D';
    int left = 'A';
    int jump = 0x20;   // VK_SPACE
    int sprint = 0x10; // VK_SHIFT
};

/// Build a standard WASD + jump/sprint \ref MovementInput from an engine focus gate.
/// Games supply yaw and edge latch state; they do not poll GetAsyncKeyState themselves.
[[nodiscard]] MovementInput BuildKeyboardMovementInput(const ri::core::KeyboardFocusGate& focus,
                                                       float yawDegrees,
                                                       KeyboardMovementEdges& edges,
                                                       const KeyboardMovementBindings& bindings = {});

/// Axis helper: +1 / -1 / 0 from a pair of focus-gated keys.
[[nodiscard]] float KeyboardAxis(const ri::core::KeyboardFocusGate& focus,
                                 int positiveKey,
                                 int negativeKey) noexcept;

} // namespace ri::trace
