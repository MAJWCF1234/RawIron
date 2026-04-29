#include "RawIron/Trace/LocomotionTuning.h"

namespace ri::trace {
namespace {

bool Usable(float value) noexcept {
    return std::isfinite(value);
}

} // namespace

void ApplyLocomotionTuningPatch(LocomotionTuning& base, const LocomotionTuningPatch& patch) noexcept {
    if (patch.walkSpeed.has_value() && Usable(*patch.walkSpeed)) {
        base.walkSpeed = std::max(0.5f, *patch.walkSpeed);
    }
    if (patch.sprintSpeed.has_value() && Usable(*patch.sprintSpeed)) {
        base.sprintSpeed = std::max(base.walkSpeed, *patch.sprintSpeed);
    }
    if (patch.crouchSpeed.has_value() && Usable(*patch.crouchSpeed)) {
        base.crouchSpeed = std::max(0.2f, *patch.crouchSpeed);
    }
    if (patch.proneSpeed.has_value() && Usable(*patch.proneSpeed)) {
        base.proneSpeed = std::max(0.1f, *patch.proneSpeed);
    }
    if (patch.gravity.has_value() && Usable(*patch.gravity)) {
        base.gravity = std::max(1.0f, *patch.gravity);
    }
    if (patch.jumpForce.has_value() && Usable(*patch.jumpForce)) {
        base.jumpForce = std::max(0.5f, *patch.jumpForce);
    }
    if (patch.fallGravityMultiplier.has_value() && Usable(*patch.fallGravityMultiplier)) {
        base.fallGravityMultiplier = std::max(0.5f, *patch.fallGravityMultiplier);
    }
    if (patch.lowJumpGravityMultiplier.has_value() && Usable(*patch.lowJumpGravityMultiplier)) {
        base.lowJumpGravityMultiplier = std::max(0.5f, *patch.lowJumpGravityMultiplier);
    }
    if (patch.maxFallSpeed.has_value() && Usable(*patch.maxFallSpeed)) {
        base.maxFallSpeed = std::max(1.0f, *patch.maxFallSpeed);
    }
    if (patch.maxStepHeight.has_value() && Usable(*patch.maxStepHeight)) {
        base.maxStepHeight = std::max(0.1f, *patch.maxStepHeight);
    }
}

MovementControllerOptions ToMovementControllerOptions(const LocomotionTuning& tuning) noexcept {
    MovementControllerOptions options{};
    options.maxGroundSpeed = tuning.walkSpeed;
    options.maxSprintGroundSpeed = tuning.sprintSpeed;
    options.maxCrouchGroundSpeed = tuning.crouchSpeed;
    options.maxProneGroundSpeed = tuning.proneSpeed;
    options.maxAirSpeed = std::max(tuning.walkSpeed, tuning.sprintSpeed);
    options.jumpSpeed = tuning.jumpForce;
    options.gravity = tuning.gravity;
    options.fallGravityMultiplier = tuning.fallGravityMultiplier;
    options.lowJumpGravityMultiplier = tuning.lowJumpGravityMultiplier;
    options.maxFallSpeed = tuning.maxFallSpeed;
    return options;
}

} // namespace ri::trace
