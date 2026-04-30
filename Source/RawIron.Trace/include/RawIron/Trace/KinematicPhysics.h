#pragma once

#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/TraceScene.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ri::trace {

/// Maximum simulated time per single `SimulateKinematicBodyStep` / `SimulateOrientedKinematicBodyStep` call.
inline constexpr float kKinematicMaxSimulationSliceSeconds = 0.25f;

struct KinematicPhysicsOptions {
    float gravity = 25.0f;
    float gravityScale = 1.0f;
    /// Applied when airborne and vertical velocity is not rising (proto `fallGravityMultiplier`). 1 disables.
    float fallGravityMultiplier = 1.0f;
    /// Maximum downward speed (positive magnitude); 0 disables terminal velocity clamp.
    float maxFallSpeed = 0.0f;
    float bounciness = 0.4f;
    float linearDamping = 0.98f;
    float angularDamping = 0.95f;
    float surfaceFriction = 0.86f;
    float rollingResistance = 0.82f;
    float airDrag = 0.992f;
    float bounceThreshold = 1.2f;
    float angularImpactScale = 0.1f;
    float groundClearance = 0.02f;
    /// When > 0 and the body started grounded, try a bounded upward compose before slide when horizontal motion is
    /// blocked by a steep obstacle (Quake / Source-style step climbing). 0 disables (default for props).
    float maxStepUpHeight = 0.0f;
    float minVelocity = 0.01f;
    /// When greater than zero, `KinematicStepResult::impact` is not set until this many seconds pass since the last notify (proto `PhysicsObject` impact cooldown). 0 keeps every hit eligible.
    float impactNotifyCooldownSeconds = 0.0f;
    std::size_t maxSubsteps = 4;
    std::string ignoreColliderId;
};

struct KinematicVolumeModifiers {
    float gravityScale = 1.0f;
    float drag = 0.0f;
    float buoyancy = 0.0f;
    ri::math::Vec3 flow{};
    /// Multiplies jump impulse when applied by gameplay (proto volume `jumpScale`).
    float jumpScale = 1.0f;
    /// When true and airborne, skips integrated gravity for this step (proto coyote hang).
    bool suppressAirGravity = false;
};

struct KinematicConstraintState {
    bool lockX = false;
    bool lockY = false;
    bool lockZ = false;
};

struct KinematicImpact {
    ri::math::Vec3 position{};
    ri::math::Vec3 normal{};
    ri::math::Vec3 velocity{};
    float speed = 0.0f;
    std::string colliderId;
};

struct KinematicBodyState {
    ri::spatial::Aabb bounds = ri::spatial::MakeEmptyAabb();
    ri::math::Vec3 velocity{};
    ri::math::Vec3 angularVelocity{};
    /// Counts down with simulation delta when `impactNotifyCooldownSeconds` is enabled.
    float impactNotifyCooldownRemaining = 0.0f;
};

struct KinematicStepResult {
    KinematicBodyState state{};
    bool onGround = false;
    std::optional<TraceHit> groundHit;
    std::vector<TraceHit> hits;
    std::optional<KinematicImpact> impact;
};

struct OrientedKinematicBodyState {
    ri::math::Vec3 center{};
    ri::math::Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    ri::math::Vec3 orientationDegrees{};
    ri::math::Vec3 velocity{};
    /// Integrated per sub-step as axis-aligned Euler deltas from radians/second components.
    ri::math::Vec3 angularVelocity{};
    float impactNotifyCooldownRemaining = 0.0f;
};

struct OrientedKinematicStepResult {
    OrientedKinematicBodyState state{};
    bool onGround = false;
    std::optional<TraceHit> groundHit;
    std::vector<TraceHit> hits;
    std::optional<KinematicImpact> impact;
    ri::spatial::Aabb worldBounds{};
};

struct KinematicAdvanceStats {
    std::size_t sliceCount = 0;
    /// Time advanced (may be less than requested if the slice safety budget is exceeded).
    float consumedSeconds = 0.0f;
    /// True when `...ForDuration` hit the safety slice budget before consuming all requested time.
    bool hitSliceBudget = false;
};

[[nodiscard]] ri::spatial::Aabb ComputeOrientedBoxWorldBounds(
    const ri::math::Vec3& center,
    const ri::math::Vec3& halfExtents,
    const ri::math::Vec3& orientationDegrees);

// deltaSeconds is clamped to kKinematicMaxSimulationSliceSeconds per call (use ...ForDuration for longer spans).
[[nodiscard]] KinematicStepResult SimulateKinematicBodyStep(
    const TraceScene& traceScene,
    const KinematicBodyState& state,
    float deltaSeconds,
    const KinematicPhysicsOptions& options = {},
    const KinematicVolumeModifiers& modifiers = {},
    const KinematicConstraintState& constraints = {});

/// Advances `totalDeltaSeconds` using repeated slice steps (same integrator as `SimulateKinematicBodyStep`).
[[nodiscard]] KinematicStepResult SimulateKinematicBodyForDuration(
    const TraceScene& traceScene,
    const KinematicBodyState& state,
    float totalDeltaSeconds,
    const KinematicPhysicsOptions& options = {},
    const KinematicVolumeModifiers& modifiers = {},
    const KinematicConstraintState& constraints = {},
    KinematicAdvanceStats* outStats = nullptr);

// Uses the same deltaSeconds clamp and integration model as SimulateKinematicBodyStep.
[[nodiscard]] OrientedKinematicStepResult SimulateOrientedKinematicBodyStep(
    const TraceScene& traceScene,
    const OrientedKinematicBodyState& state,
    float deltaSeconds,
    const KinematicPhysicsOptions& options = {},
    const KinematicVolumeModifiers& modifiers = {},
    const KinematicConstraintState& constraints = {});

[[nodiscard]] OrientedKinematicStepResult SimulateOrientedKinematicBodyForDuration(
    const TraceScene& traceScene,
    const OrientedKinematicBodyState& state,
    float totalDeltaSeconds,
    const KinematicPhysicsOptions& options = {},
    const KinematicVolumeModifiers& modifiers = {},
    const KinematicConstraintState& constraints = {},
    KinematicAdvanceStats* outStats = nullptr);

} // namespace ri::trace
