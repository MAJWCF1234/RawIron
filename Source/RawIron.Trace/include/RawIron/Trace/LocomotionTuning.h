#pragma once

#include "RawIron/Trace/MovementController.h"

#include <cmath>
#include <optional>

namespace ri::trace {

// Canonical first-person locomotion scalars aligned with the legacy web prototype defaults
// (`DEFAULT_PLAYER_TUNING` / `applyTuning`). Use for headless tuning merge, tooling, and mapping
// into MovementControllerOptions. Stamina is separate: set `MovementControllerOptions::simulateStamina`
// (and drain/regen fields) only if your game uses the optional stamina feature.

struct LocomotionTuning {
    float walkSpeed = 5.0f;
    float sprintSpeed = 8.0f;
    float crouchSpeed = 2.5f;
    float proneSpeed = 1.5f;
    float gravity = 32.0f;
    float jumpForce = 9.6f;
    float fallGravityMultiplier = 1.4f;
    float lowJumpGravityMultiplier = 1.18f;
    float maxFallSpeed = 28.0f;
    /// Authored step-up height (proto `maxStepHeight`); reserved until the C++ controller exposes it.
    float maxStepHeight = 0.7f;
};

[[nodiscard]] constexpr LocomotionTuning DefaultLocomotionTuning() noexcept {
    return LocomotionTuning{};
}

struct LocomotionTuningPatch {
    std::optional<float> walkSpeed;
    std::optional<float> sprintSpeed;
    std::optional<float> crouchSpeed;
    std::optional<float> proneSpeed;
    std::optional<float> gravity;
    std::optional<float> jumpForce;
    std::optional<float> fallGravityMultiplier;
    std::optional<float> lowJumpGravityMultiplier;
    std::optional<float> maxFallSpeed;
    std::optional<float> maxStepHeight;
};

/// Applies optional overrides with the same ordering and floor clamps as the prototype
/// `Player.applyTuning` implementation. `sprintSpeed` is clamped to be no less than the
/// resulting `walkSpeed`.
void ApplyLocomotionTuningPatch(LocomotionTuning& base, const LocomotionTuningPatch& patch) noexcept;

/// Maps locomotion scalars onto MovementControllerOptions. Air speed cap follows the prototype
/// convention of allowing at least sprint-class horizontal speed while airborne.
[[nodiscard]] MovementControllerOptions ToMovementControllerOptions(const LocomotionTuning& tuning) noexcept;

} // namespace ri::trace
