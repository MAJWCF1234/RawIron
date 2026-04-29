#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Trace/KinematicPhysics.h"

#include <cstdint>

namespace ri::trace {

enum class MovementStance : std::uint8_t {
    Standing = 0,
    Crouching,
    Prone,
};

struct MovementInput {
    float moveForward = 0.0f;
    float moveRight = 0.0f;
    /// World-space camera forward flattened to XZ when non-zero; otherwise +Z is forward (legacy).
    ri::math::Vec3 viewForwardWorld{};
    /// Optional world-space camera right on XZ; if zero, derived from `viewForwardWorld` and world up.
    ri::math::Vec3 viewRightWorld{};
    bool sprintHeld = false;
    bool jumpPressed = false;
    /// When true while ascending, gravity uses `lowJumpGravityMultiplier` (proto-style variable jump).
    bool applyShortJumpGravity = false;
    /// Ladder / climb vertical intent: -1 down, +1 up. Only read when `traversalClimbSpeed` is enabled on options.
    float traversalClimbAxis = 0.0f;
};

struct MovementControllerOptions {
    /// Baseline cap while standing on the ground (proto `walkSpeed`).
    float maxGroundSpeed = 8.5f;
    /// Used when `sprintHeld`, standing, on ground, and (if `simulateStamina`) stamina remains (proto `sprintSpeed`).
    float maxSprintGroundSpeed = 12.0f;
    float maxCrouchGroundSpeed = 4.25f;
    float maxProneGroundSpeed = 2.55f;
    /// Caps airborne wish speed (raise with sprint so air matches proto `getCurrentSpeed` behavior).
    float maxAirSpeed = 12.0f;
    float groundAcceleration = 70.0f;
    float airAcceleration = 18.0f;
    /// When greater than zero and `MovementInput::traversalClimbAxis` is non-zero, vertical velocity is driven directly
    /// (proto traversal) and air gravity is suppressed. **Default 0 leaves RawIron on the standard kinematic path only.**
    float traversalClimbSpeed = 0.0f;
    /// Horizontal speed multiplier while traversal is active (prototype used ~0.58).
    float traversalHorizontalSpeedScale = 0.58f;
    float groundFriction = 8.0f;
    float stopSpeed = 2.0f;
    float jumpSpeed = 8.2f;
    /// Scales jump impulse (maps proto volume `jumpScale` when volumes are folded into the controller).
    float jumpVolumeScale = 1.0f;
    float gravity = 25.0f;
    float airControl = 0.28f;
    /// Additional in-air steering blend applied each frame (higher = more responsive direction changes in air).
    float airTurnResponsiveness = 0.38f;
    /// Scales acceleration when strafing in-air against travel direction (parkour strafe recovery).
    float airStrafeAccelerationBoost = 1.2f;
    /// Extra gravity while descending in air (proto default 1.4).
    float fallGravityMultiplier = 1.4f;
    /// Terminal fall speed magnitude; proto default 28.
    float maxFallSpeed = 28.0f;
    float staminaMax = 100.0f;
    float staminaDrainPerSecond = 25.0f;
    float staminaRegenPerSecond = 15.0f;
    /// When false, stamina is not integrated each step, sprint ignores the stamina scalar, and
    /// `AdvancePlayerStamina` / `ComputeAdvancedPlayerStamina` no-op. Games that do not want a
    /// stamina loop leave this off and may ignore `MovementControllerState::stamina` entirely.
    bool simulateStamina = true;
    /// Time after leaving supported ground where a jump input is still accepted (proto-style coyote).
    float coyoteTimeSeconds = 0.16f;
    /// Jump presses are remembered this long so an early tap still fires on landing (0 disables buffering).
    float jumpBufferTimeSeconds = 0.18f;
    /// While ascending, if jump is not held, gravity is scaled by this (1 disables short-hop cut).
    float lowJumpGravityMultiplier = 1.18f;
    /// If greater than zero, a downward structural ground probe within this vertical distance allows jump when not `onGround` (proto-style).
    /// Small non-zero default: allows jump when feet are slightly past a ledge but still near ground (platforming).
    float groundProbeJumpMaxDown = 0.28f;
    /// Downward stick force while grounded to reduce tiny edge/slope detach jitter (0 disables).
    float groundAdhesionSpeed = 2.2f;
    /// Enables a lightweight airborne wall-jump when jump is pressed near a vertical surface.
    bool enableWallJump = true;
    /// Max wall detection distance for wall-jump support.
    float wallJumpProbeDistance = 0.62f;
    /// Vertical impulse applied for wall-jumps.
    float wallJumpVerticalSpeed = 7.6f;
    /// Horizontal impulse away from the wall normal.
    float wallJumpAwaySpeed = 6.0f;
    /// Horizontal carry retained from pre-wall-jump velocity.
    float wallJumpCarry = 0.55f;
    /// Project camera wish onto the ground tangent plane using `groundNormal` (proto slope walking).
    bool projectMovementOntoGroundNormal = true;
    KinematicPhysicsOptions kinematic{};
};

struct MovementControllerState {
    KinematicBodyState body{};
    bool onGround = false;
    /// Last walkable contact normal (updated while grounded); kept across coyote air frames.
    ri::math::Vec3 groundNormal{0.0f, 1.0f, 0.0f};
    MovementStance stance = MovementStance::Standing;
    /// Only read/updated when `MovementControllerOptions::simulateStamina` is true; otherwise ignore.
    float stamina = 100.0f;
    float coyoteTimeRemaining = 0.0f;
    float jumpBufferTimeRemaining = 0.0f;
    /// Prevents immediate repeated wall-jumps in consecutive frames.
    float wallJumpCooldownRemaining = 0.0f;
    bool jumpPressedLastFrame = false;
};

struct MovementControllerResult {
    MovementControllerState state{};
    KinematicStepResult step{};
};

void AdvancePlayerStamina(float& stamina,
                          bool hasMoveInput,
                          bool sprintHeld,
                          MovementStance stance,
                          float deltaSeconds,
                          const MovementControllerOptions& options);

/// Non-mutating convenience for snapshots, prediction, and tests (mirrors `AdvancePlayerStamina`).
[[nodiscard]] float ComputeAdvancedPlayerStamina(float stamina,
                                                   bool hasMoveInput,
                                                   bool sprintHeld,
                                                   MovementStance stance,
                                                   float deltaSeconds,
                                                   const MovementControllerOptions& options);

[[nodiscard]] MovementControllerResult SimulateMovementControllerStep(
    const TraceScene& traceScene,
    const MovementControllerState& state,
    const MovementInput& input,
    float deltaSeconds,
    const MovementControllerOptions& options = {},
    const KinematicVolumeModifiers& volumeModifiers = {});

} // namespace ri::trace
