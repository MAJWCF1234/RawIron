#pragma once

#include "RawIron/Trace/KinematicPhysics.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ri::trace {

/// Runtime slot for one kinematic prop / physics object sharing the same integrator as the player capsule.
struct KinematicObjectSlot {
    /// When non-empty, merged into `KinematicPhysicsOptions::ignoreColliderId` so sweeps ignore this object's own collider.
    std::string objectColliderId{};
    KinematicBodyState state{};
    /// When true and batch sleep is enabled, integration is skipped until `WakeKinematicObject`.
    bool sleeping = false;
    /// Accumulates time spent nearly at rest on ground (used to enter sleep).
    float calmSeconds = 0.0f;
};

struct ObjectPhysicsBatchOptions {
    bool enableSleep = true;
    float sleepLinearThreshold = 0.05f;
    float sleepAngularThreshold = 0.05f;
    /// Time at rest on ground before marking the slot asleep.
    float calmSecondsBeforeSleep = 0.4f;
};

struct ObjectPhysicsBatchResult {
    /// One entry per input slot (same index); sleeping slots copy the previous state without re-simulating.
    std::vector<KinematicStepResult> steps;
    std::size_t simulatedCount = 0;
    std::size_t skippedSleeping = 0;
};

struct ObjectCarryOptions {
    float maxPickupDistance = 2.5f;
    /// Dot threshold between look direction and candidate direction (1 = strict center-screen).
    float minPickupAimDot = 0.25f;
    float holdDistance = 1.4f;
    float holdHeightOffset = 0.25f;
    /// Exponential follow responsiveness in 1/seconds (higher snaps faster toward target anchor).
    float holdFollowResponsiveness = 18.0f;
    float throwSpeed = 10.0f;
    float throwUpwardBoost = 1.25f;
    float throwInheritHolderVelocityScale = 0.5f;
};

struct HeldObjectState {
    std::optional<std::size_t> heldObjectIndex;
};

void WakeKinematicObject(KinematicObjectSlot& slot) noexcept;

/// Integrates every slot for `deltaSeconds` using `SimulateKinematicBodyStep`, merging each slot's `objectColliderId`
/// into physics options. Optional sleep skips work for props that have come to rest on structural geometry.
[[nodiscard]] ObjectPhysicsBatchResult StepKinematicObjectBatch(
    const TraceScene& scene,
    std::vector<KinematicObjectSlot>& objects,
    float deltaSeconds,
    const KinematicPhysicsOptions& options,
    const KinematicVolumeModifiers& modifiers = {},
    const KinematicConstraintState& constraints = {},
    const ObjectPhysicsBatchOptions& batchOptions = {});

/// Picks the nearest object in front of `holderPosition` and marks it as held.
[[nodiscard]] bool TryPickupNearestKinematicObject(
    std::vector<KinematicObjectSlot>& objects,
    const ri::math::Vec3& holderPosition,
    const ri::math::Vec3& holderForward,
    HeldObjectState& heldState,
    const ObjectCarryOptions& options = {});

/// While held, keeps the object anchored in front of the holder and suppresses simulation drift.
[[nodiscard]] bool UpdateHeldKinematicObject(
    std::vector<KinematicObjectSlot>& objects,
    float deltaSeconds,
    const ri::math::Vec3& holderPosition,
    const ri::math::Vec3& holderForward,
    HeldObjectState& heldState,
    const ObjectCarryOptions& options = {});

/// Releases the held object with a throw impulse.
[[nodiscard]] bool ThrowHeldKinematicObject(
    std::vector<KinematicObjectSlot>& objects,
    const ri::math::Vec3& holderForward,
    const ri::math::Vec3& holderVelocity,
    HeldObjectState& heldState,
    const ObjectCarryOptions& options = {});

} // namespace ri::trace
