#include "RawIron/Trace/MovementController.h"

#include "RawIron/Spatial/Aabb.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace ri::trace {
namespace {

float Clamp(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

ri::math::Vec3 ProjectOntoPlane(const ri::math::Vec3& vector, const ri::math::Vec3& unitNormal) {
    return vector - (unitNormal * ri::math::Dot(vector, unitNormal));
}

ri::math::Vec3 ComputeHorizontalWish(const MovementInput& input) {
    ri::math::Vec3 forward = input.viewForwardWorld;
    forward.y = 0.0f;
    ri::math::Vec3 right = input.viewRightWorld;
    right.y = 0.0f;

    if (ri::math::LengthSquared(forward) < 1e-8f) {
        forward = {0.0f, 0.0f, 1.0f};
    } else {
        forward = ri::math::Normalize(forward);
    }

    if (ri::math::LengthSquared(right) < 1e-8f) {
        // worldUp × forward → +X when forward is +Z (matches legacy moveRight on +X).
        right = ri::math::Normalize(ri::math::Cross(ri::math::Vec3{0.0f, 1.0f, 0.0f}, forward));
        if (ri::math::LengthSquared(right) < 1e-8f) {
            right = {1.0f, 0.0f, 0.0f};
        }
    } else {
        right = ri::math::Normalize(right);
    }

    ri::math::Vec3 wish = forward * input.moveForward + right * input.moveRight;
    const float length = ri::math::Length(wish);
    if (length <= 1e-6f) {
        return {};
    }
    return wish / length;
}

ri::spatial::Aabb TranslateBox(const ri::spatial::Aabb& box, const ri::math::Vec3& delta) {
    if (ri::spatial::IsEmpty(box)) {
        return box;
    }
    return ri::spatial::Aabb{
        .min = box.min + delta,
        .max = box.max + delta,
    };
}

float ComputeWishMagnitude(const MovementInput& input) {
    const float magnitude = std::sqrt((input.moveForward * input.moveForward) + (input.moveRight * input.moveRight));
    return Clamp(magnitude, 0.0f, 1.0f);
}

void ApplyGroundFriction(ri::math::Vec3& velocity, float deltaSeconds, const MovementControllerOptions& options) {
    ri::math::Vec3 planar{velocity.x, 0.0f, velocity.z};
    const float speed = ri::math::Length(planar);
    if (speed <= 0.00001f) {
        return;
    }

    const float control = std::max(speed, options.stopSpeed);
    const float drop = control * options.groundFriction * deltaSeconds;
    const float newSpeed = std::max(0.0f, speed - drop);
    const float scale = speed > 0.0f ? (newSpeed / speed) : 0.0f;
    velocity.x *= scale;
    velocity.z *= scale;
}

void ApplyGroundFrictionSurface(ri::math::Vec3& velocity,
                                const ri::math::Vec3& groundNormal,
                                float deltaSeconds,
                                const MovementControllerOptions& options) {
    if (ri::math::LengthSquared(groundNormal) < 1e-10f) {
        ApplyGroundFriction(velocity, deltaSeconds, options);
        return;
    }
    const ri::math::Vec3 n = ri::math::Normalize(groundNormal);
    const float vn = ri::math::Dot(velocity, n);
    ri::math::Vec3 tangential = velocity - (n * vn);
    const float speed = ri::math::Length(tangential);
    if (speed <= 1e-6f) {
        return;
    }
    const float control = std::max(speed, options.stopSpeed);
    const float drop = control * options.groundFriction * deltaSeconds;
    const float newSpeed = std::max(0.0f, speed - drop);
    const float scale = speed > 1e-6f ? (newSpeed / speed) : 0.0f;
    tangential = tangential * scale;
    velocity = tangential + (n * vn);
}

void AcceleratePlanar(ri::math::Vec3& velocity,
                      const ri::math::Vec3& wishDirection,
                      float wishSpeed,
                      float acceleration,
                      float deltaSeconds) {
    if (wishSpeed <= 0.00001f) {
        return;
    }

    ri::math::Vec3 planar{velocity.x, 0.0f, velocity.z};
    const float currentSpeed = ri::math::Dot(planar, wishDirection);
    const float addSpeed = wishSpeed - currentSpeed;
    if (addSpeed <= 0.0f) {
        return;
    }

    const float accelSpeed = std::min(addSpeed, acceleration * wishSpeed * deltaSeconds);
    velocity.x += wishDirection.x * accelSpeed;
    velocity.z += wishDirection.z * accelSpeed;
}

void AccelerateSurface(ri::math::Vec3& velocity,
                       const ri::math::Vec3& groundNormal,
                       const ri::math::Vec3& wishDirTangent,
                       float wishSpeed,
                       float acceleration,
                       float deltaSeconds) {
    if (wishSpeed <= 1e-5f || ri::math::LengthSquared(wishDirTangent) < 1e-12f) {
        return;
    }
    if (ri::math::LengthSquared(groundNormal) < 1e-10f) {
        return;
    }
    const ri::math::Vec3 n = ri::math::Normalize(groundNormal);
    ri::math::Vec3 wish = ProjectOntoPlane(wishDirTangent, n);
    if (ri::math::LengthSquared(wish) < 1e-12f) {
        return;
    }
    wish = ri::math::Normalize(wish);
    const float vn = ri::math::Dot(velocity, n);
    const ri::math::Vec3 velT = velocity - (n * vn);
    const float currentSpeed = ri::math::Dot(velT, wish);
    const float addSpeed = wishSpeed - currentSpeed;
    if (addSpeed <= 0.0f) {
        return;
    }
    const float accelSpeed = std::min(addSpeed, acceleration * wishSpeed * deltaSeconds);
    velocity = velocity + (wish * accelSpeed);
}

float ComputePlanarSpeedCap(bool groundedForMove,
                            bool traversalActive,
                            const MovementControllerState& s,
                            const MovementInput& input,
                            const MovementControllerOptions& options) {
    const bool sprintEligible = input.sprintHeld && s.stance == MovementStance::Standing
        && (!options.simulateStamina || s.stamina > 0.0f);
    float cap = options.maxGroundSpeed;
    if (s.stance == MovementStance::Crouching) {
        cap = options.maxCrouchGroundSpeed;
    } else if (s.stance == MovementStance::Prone) {
        cap = options.maxProneGroundSpeed;
    } else if (sprintEligible) {
        cap = std::max(options.maxGroundSpeed, options.maxSprintGroundSpeed);
    }
    if (!groundedForMove) {
        cap = std::min(cap, options.maxAirSpeed);
    }
    if (traversalActive && options.traversalHorizontalSpeedScale > 0.0f) {
        cap *= options.traversalHorizontalSpeedScale;
    }
    return cap;
}

bool FeetNearStructuralGround(const TraceScene& traceScene,
                              const ri::spatial::Aabb& bounds,
                              float maxDrop,
                              const std::string& ignoreColliderId) {
    if (maxDrop <= 0.0f || ri::spatial::IsEmpty(bounds)) {
        return false;
    }
    const ri::math::Vec3 center = ri::spatial::Center(bounds);
    const ri::math::Vec3 probeOrigin{center.x, bounds.min.y + 0.04f, center.z};
    const std::optional<TraceHit> hit = traceScene.FindGroundHit(
        probeOrigin,
        GroundTraceOptions{
            .maxDistance = maxDrop + 0.12f,
            .structuralOnly = true,
            .ignoreId = ignoreColliderId,
            .minNormalY = 0.5f,
        });
    if (!hit.has_value()) {
        return false;
    }
    const float clearance = probeOrigin.y - hit->point.y;
    return clearance >= -0.02f && clearance <= maxDrop + 0.08f;
}

std::optional<ri::math::Vec3> ProbeWallNormal(const TraceScene& traceScene,
                                              const ri::spatial::Aabb& bounds,
                                              const ri::math::Vec3& probeDir,
                                              float probeDistance,
                                              const std::string& ignoreColliderId) {
    if (probeDistance <= 0.0f || ri::spatial::IsEmpty(bounds)) {
        return std::nullopt;
    }
    ri::math::Vec3 horizontalDir = probeDir;
    horizontalDir.y = 0.0f;
    if (ri::math::LengthSquared(horizontalDir) < 1e-10f) {
        return std::nullopt;
    }
    horizontalDir = ri::math::Normalize(horizontalDir);
    const ri::math::Vec3 center = ri::spatial::Center(bounds);
    const ri::math::Vec3 origin{center.x, bounds.min.y + ((bounds.max.y - bounds.min.y) * 0.55f), center.z};
    const std::optional<TraceHit> hit = traceScene.TraceRay(
        origin,
        horizontalDir,
        probeDistance,
        TraceOptions{
            .structuralOnly = true,
            .ignoreId = ignoreColliderId,
        });
    if (!hit.has_value()) {
        return std::nullopt;
    }
    ri::math::Vec3 n = hit->normal;
    if (ri::math::LengthSquared(n) < 1e-10f) {
        return std::nullopt;
    }
    n = ri::math::Normalize(n);
    if (std::fabs(n.y) > 0.45f) {
        return std::nullopt;
    }
    return n;
}

} // namespace

void AdvancePlayerStamina(float& stamina,
                          bool hasMoveInput,
                          bool sprintHeld,
                          MovementStance stance,
                          float deltaSeconds,
                          const MovementControllerOptions& options) {
    if (!options.simulateStamina) {
        return;
    }
    if (hasMoveInput && sprintHeld && stance == MovementStance::Standing && stamina > 0.0f) {
        stamina = std::max(0.0f, stamina - options.staminaDrainPerSecond * deltaSeconds);
    } else {
        stamina = std::min(options.staminaMax, stamina + options.staminaRegenPerSecond * deltaSeconds);
    }
}

float ComputeAdvancedPlayerStamina(float stamina,
                                   bool hasMoveInput,
                                   bool sprintHeld,
                                   MovementStance stance,
                                   float deltaSeconds,
                                   const MovementControllerOptions& options) {
    AdvancePlayerStamina(stamina, hasMoveInput, sprintHeld, stance, deltaSeconds, options);
    return stamina;
}

MovementControllerResult SimulateMovementControllerStep(
    const TraceScene& traceScene,
    const MovementControllerState& state,
    const MovementInput& input,
    float deltaSeconds,
    const MovementControllerOptions& options,
    const KinematicVolumeModifiers& volumeModifiers) {
    MovementControllerResult result{};
    result.state = state;
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return result;
    }

    deltaSeconds = std::min(deltaSeconds, 0.1f);
    const bool jumpEdge = input.jumpPressed && !state.jumpPressedLastFrame;

    float coyoteRemaining = state.coyoteTimeRemaining;
    if (state.onGround) {
        coyoteRemaining = options.coyoteTimeSeconds;
    } else if (options.coyoteTimeSeconds > 0.0f) {
        coyoteRemaining = std::max(0.0f, state.coyoteTimeRemaining - deltaSeconds);
    } else {
        coyoteRemaining = 0.0f;
    }

    float jumpBufferRemaining = state.jumpBufferTimeRemaining;
    if (options.jumpBufferTimeSeconds > 0.0f && jumpEdge) {
        jumpBufferRemaining = options.jumpBufferTimeSeconds;
    }
    float wallJumpCooldownRemaining = std::max(0.0f, state.wallJumpCooldownRemaining - deltaSeconds);

    ri::math::Vec3 velocity = result.state.body.velocity;
    const float climbAxis = Clamp(input.traversalClimbAxis, -1.0f, 1.0f);
    const bool traversalActive =
        options.traversalClimbSpeed > 1e-5f && std::fabs(climbAxis) > 1e-5f;
    if (traversalActive) {
        velocity.y = climbAxis * options.traversalClimbSpeed;
    }

    if (traversalActive) {
        jumpBufferRemaining = 0.0f;
    }

    const float wishMagnitude = ComputeWishMagnitude(input);
    const bool hasInput = wishMagnitude > 0.00001f;
    const ri::math::Vec3 horizontalWish = hasInput ? ComputeHorizontalWish(input) : ri::math::Vec3{};
    ri::math::Vec3 groundN = state.groundNormal;
    if (ri::math::LengthSquared(groundN) < 1e-10f) {
        groundN = {0.0f, 1.0f, 0.0f};
    } else {
        groundN = ri::math::Normalize(groundN);
    }
    ri::math::Vec3 groundWish = horizontalWish;
    if (hasInput && options.projectMovementOntoGroundNormal) {
        groundWish = ProjectOntoPlane(horizontalWish, groundN);
        if (ri::math::LengthSquared(groundWish) > 1e-10f) {
            groundWish = ri::math::Normalize(groundWish);
        } else {
            groundWish = {};
        }
    }
    ri::math::Vec3 xzWish = horizontalWish;
    xzWish.y = 0.0f;
    if (ri::math::LengthSquared(xzWish) > 1e-10f) {
        xzWish = ri::math::Normalize(xzWish);
    } else {
        xzWish = {};
    }
    bool jumpedThisFrame = false;

    if (result.state.onGround && !traversalActive) {
        if (options.groundAdhesionSpeed > 0.0f && velocity.y > -options.groundAdhesionSpeed) {
            velocity.y = -options.groundAdhesionSpeed;
        }
        if (options.projectMovementOntoGroundNormal) {
            ApplyGroundFrictionSurface(velocity, groundN, deltaSeconds, options);
        } else {
            ApplyGroundFriction(velocity, deltaSeconds, options);
        }
    }

    const bool canCoyoteJump = options.coyoteTimeSeconds > 0.0f && coyoteRemaining > 0.0f;
    const bool probeJump = options.groundProbeJumpMaxDown > 0.0f
        && FeetNearStructuralGround(
            traceScene,
            result.state.body.bounds,
            options.groundProbeJumpMaxDown,
            options.kinematic.ignoreColliderId);
    const bool canJump = result.state.onGround || canCoyoteJump || probeJump;
    const bool jumpFromBuffer = options.jumpBufferTimeSeconds > 0.0f && jumpBufferRemaining > 0.0f;
    const bool jumpFromInstant = options.jumpBufferTimeSeconds <= 0.0f && jumpEdge;
    if (!traversalActive && (jumpFromBuffer || jumpFromInstant) && canJump) {
        const float jumpMul = options.jumpVolumeScale * volumeModifiers.jumpScale;
        velocity.y = options.jumpSpeed * jumpMul;
        jumpedThisFrame = true;
        jumpBufferRemaining = 0.0f;
        coyoteRemaining = 0.0f;
    }
    if (!traversalActive
        && !jumpedThisFrame
        && options.enableWallJump
        && wallJumpCooldownRemaining <= 0.0f
        && jumpEdge
        && !canJump) {
        ri::math::Vec3 probeDir = xzWish;
        if (ri::math::LengthSquared(probeDir) < 1e-10f) {
            probeDir = ri::math::Vec3{velocity.x, 0.0f, velocity.z};
        }
        if (const std::optional<ri::math::Vec3> wallNormal = ProbeWallNormal(
                traceScene,
                result.state.body.bounds,
                probeDir,
                options.wallJumpProbeDistance,
                options.kinematic.ignoreColliderId);
            wallNormal.has_value()) {
            const ri::math::Vec3 planarCarry{velocity.x, 0.0f, velocity.z};
            const ri::math::Vec3 awayImpulse = *wallNormal * options.wallJumpAwaySpeed;
            const ri::math::Vec3 planarResult = (planarCarry * Clamp(options.wallJumpCarry, 0.0f, 1.0f)) + awayImpulse;
            velocity.x = planarResult.x;
            velocity.z = planarResult.z;
            velocity.y = std::max(velocity.y, options.wallJumpVerticalSpeed);
            jumpedThisFrame = true;
            jumpBufferRemaining = 0.0f;
            coyoteRemaining = 0.0f;
            wallJumpCooldownRemaining = 0.2f;
        }
    }

    if (options.jumpBufferTimeSeconds > 0.0f) {
        jumpBufferRemaining = std::max(0.0f, jumpBufferRemaining - deltaSeconds);
    }

    AdvancePlayerStamina(result.state.stamina, hasInput, input.sprintHeld, result.state.stance, deltaSeconds, options);

    const bool groundedForMove = result.state.onGround && !jumpedThisFrame && !traversalActive;
    const float planarCap =
        ComputePlanarSpeedCap(groundedForMove, traversalActive, result.state, input, options)
        * wishMagnitude;

    if (hasInput) {
        const float acceleration =
            groundedForMove ? options.groundAcceleration : options.airAcceleration;
        if (groundedForMove && options.projectMovementOntoGroundNormal) {
            AccelerateSurface(velocity, groundN, groundWish, planarCap, acceleration, deltaSeconds);
        } else {
            AcceleratePlanar(velocity, xzWish, planarCap, acceleration, deltaSeconds);
        }

        if (!groundedForMove && options.airControl > 0.0f) {
            ri::math::Vec3 planar{velocity.x, 0.0f, velocity.z};
            const float planarSpeed = ri::math::Length(planar);
            if (planarSpeed > 0.00001f) {
                const ri::math::Vec3 planarDir = planar / planarSpeed;
                const float align = ri::math::Dot(planarDir, xzWish);
                if (align > 0.0f) {
                    const float control = options.airControl * align * align * deltaSeconds;
                    const ri::math::Vec3 steered =
                        ri::math::Normalize((planarDir * (1.0f - control)) + (xzWish * control));
                    velocity.x = steered.x * planarSpeed;
                    velocity.z = steered.z * planarSpeed;
                }
            }
        }
    }

    KinematicPhysicsOptions kinematic = options.kinematic;
    kinematic.gravity = options.gravity;
    kinematic.fallGravityMultiplier = options.fallGravityMultiplier;
    kinematic.maxFallSpeed = options.maxFallSpeed;
    kinematic.surfaceFriction = 1.0f;
    kinematic.rollingResistance = 1.0f;
    kinematic.linearDamping = 1.0f;
    kinematic.airDrag = 1.0f;
    kinematic.bounciness = 0.0f;
    kinematic.bounceThreshold = 9999.0f;

    KinematicVolumeModifiers volume = volumeModifiers;
    const bool rising = velocity.y > 0.0f;
    const bool shortHop = rising && input.applyShortJumpGravity && options.lowJumpGravityMultiplier > 1.0f
        && (jumpedThisFrame || !state.onGround);
    if (shortHop) {
        volume.gravityScale *= options.lowJumpGravityMultiplier;
    }
    const bool coyoteAir =
        !state.onGround && coyoteRemaining > 0.0f && options.coyoteTimeSeconds > 0.0f;
    if (traversalActive || coyoteAir) {
        volume.suppressAirGravity = true;
    }

    result.state.body.velocity = velocity;
    if (jumpedThisFrame) {
        // Lift the body clear of the ground snap band so jump impulse is preserved.
        result.state.body.bounds = TranslateBox(result.state.body.bounds, ri::math::Vec3{0.0f, 0.2f, 0.0f});
    }
    result.step = SimulateKinematicBodyStep(traceScene, result.state.body, deltaSeconds, kinematic, volume);
    result.state.body = result.step.state;
    result.state.onGround = result.step.onGround;
    if (result.state.onGround && result.step.groundHit.has_value()) {
        ri::math::Vec3 hitN = result.step.groundHit->normal;
        if (ri::math::LengthSquared(hitN) > 1e-10f) {
            result.state.groundNormal = ri::math::Normalize(hitN);
        }
    }
    result.state.coyoteTimeRemaining = coyoteRemaining;
    result.state.jumpBufferTimeRemaining = jumpBufferRemaining;
    result.state.wallJumpCooldownRemaining = wallJumpCooldownRemaining;
    result.state.jumpPressedLastFrame = input.jumpPressed;
    result.step.onGround = result.state.onGround;
    return result;
}

} // namespace ri::trace
