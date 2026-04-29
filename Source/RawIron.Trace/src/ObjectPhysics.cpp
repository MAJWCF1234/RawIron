#include "RawIron/Trace/ObjectPhysics.h"

#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ri::trace {
namespace {

ri::math::Vec3 ResolveForward(const ri::math::Vec3& holderForward) {
    if (ri::math::LengthSquared(holderForward) < 1e-10f) {
        return ri::math::Vec3{0.0f, 0.0f, 1.0f};
    }
    return ri::math::Normalize(holderForward);
}

ri::spatial::Aabb RecenterBounds(const ri::spatial::Aabb& source, const ri::math::Vec3& center) {
    const ri::math::Vec3 halfExtents = ri::spatial::Size(source) * 0.5f;
    return ri::spatial::Aabb{
        .min = center - halfExtents,
        .max = center + halfExtents,
    };
}

} // namespace

void WakeKinematicObject(KinematicObjectSlot& slot) noexcept {
    slot.sleeping = false;
    slot.calmSeconds = 0.0f;
}

ObjectPhysicsBatchResult StepKinematicObjectBatch(
    const TraceScene& scene,
    std::vector<KinematicObjectSlot>& objects,
    float deltaSeconds,
    const KinematicPhysicsOptions& options,
    const KinematicVolumeModifiers& modifiers,
    const KinematicConstraintState& constraints,
    const ObjectPhysicsBatchOptions& batchOptions) {
    ObjectPhysicsBatchResult result{};
    result.steps.resize(objects.size());

    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        for (std::size_t index = 0; index < objects.size(); ++index) {
            result.steps[index].state = objects[index].state;
        }
        return result;
    }

    for (std::size_t index = 0; index < objects.size(); ++index) {
        KinematicObjectSlot& slot = objects[index];
        KinematicStepResult& outStep = result.steps[index];

        if (batchOptions.enableSleep && slot.sleeping) {
            outStep.state = slot.state;
            result.skippedSleeping += 1;
            continue;
        }

        KinematicPhysicsOptions merged = options;
        merged.ignoreColliderId = slot.objectColliderId;

        KinematicStepResult step =
            SimulateKinematicBodyStep(scene, slot.state, deltaSeconds, merged, modifiers, constraints);
        slot.state = step.state;
        outStep = std::move(step);
        result.simulatedCount += 1;

        if (outStep.impact.has_value()) {
            WakeKinematicObject(slot);
        }

        if (batchOptions.enableSleep) {
            const float linearSpeed = ri::math::Length(slot.state.velocity);
            const float angularSpeed = ri::math::Length(slot.state.angularVelocity);
            const bool atRestRates = linearSpeed < batchOptions.sleepLinearThreshold
                && angularSpeed < batchOptions.sleepAngularThreshold;
            const bool onGround = outStep.onGround;

            if (atRestRates && onGround) {
                slot.calmSeconds += deltaSeconds;
                if (slot.calmSeconds >= batchOptions.calmSecondsBeforeSleep) {
                    slot.sleeping = true;
                }
            } else {
                slot.calmSeconds = 0.0f;
                slot.sleeping = false;
            }
        }
    }

    return result;
}

bool TryPickupNearestKinematicObject(
    std::vector<KinematicObjectSlot>& objects,
    const ri::math::Vec3& holderPosition,
    const ri::math::Vec3& holderForward,
    HeldObjectState& heldState,
    const ObjectCarryOptions& options) {
    if (heldState.heldObjectIndex.has_value()) {
        return false;
    }

    const ri::math::Vec3 forward = ResolveForward(holderForward);
    const float maxDistance = std::max(0.1f, options.maxPickupDistance);
    const float minAimDot = std::clamp(options.minPickupAimDot, -1.0f, 1.0f);

    std::optional<std::size_t> bestIndex;
    float bestDistance = std::numeric_limits<float>::infinity();
    float bestAimDot = -1.0f;
    for (std::size_t index = 0; index < objects.size(); ++index) {
        const KinematicObjectSlot& slot = objects[index];
        if (ri::spatial::IsEmpty(slot.state.bounds)) {
            continue;
        }

        const ri::math::Vec3 center = ri::spatial::Center(slot.state.bounds);
        const ri::math::Vec3 toObject = center - holderPosition;
        const float distance = ri::math::Length(toObject);
        if (distance > maxDistance) {
            continue;
        }
        const float aimDot = distance <= 1e-6f ? 1.0f : ri::math::Dot(toObject / distance, forward);
        if (aimDot < minAimDot) {
            continue;
        }

        if (!bestIndex.has_value() || distance < bestDistance - 1e-4f
            || (std::fabs(distance - bestDistance) <= 1e-4f && aimDot > bestAimDot)) {
            bestIndex = index;
            bestDistance = distance;
            bestAimDot = aimDot;
        }
    }

    if (!bestIndex.has_value()) {
        return false;
    }

    KinematicObjectSlot& picked = objects[*bestIndex];
    picked.state.velocity = {};
    picked.state.angularVelocity = {};
    picked.state.impactNotifyCooldownRemaining = 0.0f;
    WakeKinematicObject(picked);
    heldState.heldObjectIndex = *bestIndex;
    return true;
}

bool UpdateHeldKinematicObject(
    std::vector<KinematicObjectSlot>& objects,
    float deltaSeconds,
    const ri::math::Vec3& holderPosition,
    const ri::math::Vec3& holderForward,
    HeldObjectState& heldState,
    const ObjectCarryOptions& options) {
    if (!heldState.heldObjectIndex.has_value()) {
        return false;
    }
    if (*heldState.heldObjectIndex >= objects.size()) {
        heldState.heldObjectIndex.reset();
        return false;
    }

    KinematicObjectSlot& held = objects[*heldState.heldObjectIndex];
    if (ri::spatial::IsEmpty(held.state.bounds)) {
        heldState.heldObjectIndex.reset();
        return false;
    }

    const ri::math::Vec3 forward = ResolveForward(holderForward);
    const ri::math::Vec3 targetCenter = holderPosition
        + (forward * std::max(0.25f, options.holdDistance))
        + ri::math::Vec3{0.0f, options.holdHeightOffset, 0.0f};
    const ri::math::Vec3 currentCenter = ri::spatial::Center(held.state.bounds);

    float alpha = 1.0f;
    if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0f) {
        const float response = std::max(0.0f, options.holdFollowResponsiveness);
        alpha = 1.0f - std::exp(-response * deltaSeconds);
        alpha = std::clamp(alpha, 0.0f, 1.0f);
    }
    const ri::math::Vec3 nextCenter = ri::math::Lerp(currentCenter, targetCenter, alpha);
    held.state.bounds = RecenterBounds(held.state.bounds, nextCenter);
    held.state.velocity = {};
    held.state.angularVelocity = {};
    held.state.impactNotifyCooldownRemaining = 0.0f;
    held.sleeping = false;
    held.calmSeconds = 0.0f;
    return true;
}

bool ThrowHeldKinematicObject(
    std::vector<KinematicObjectSlot>& objects,
    const ri::math::Vec3& holderForward,
    const ri::math::Vec3& holderVelocity,
    HeldObjectState& heldState,
    const ObjectCarryOptions& options) {
    if (!heldState.heldObjectIndex.has_value()) {
        return false;
    }
    if (*heldState.heldObjectIndex >= objects.size()) {
        heldState.heldObjectIndex.reset();
        return false;
    }

    KinematicObjectSlot& held = objects[*heldState.heldObjectIndex];
    const ri::math::Vec3 forward = ResolveForward(holderForward);
    const ri::math::Vec3 throwVelocity =
        (forward * std::max(0.0f, options.throwSpeed))
        + (holderVelocity * std::max(0.0f, options.throwInheritHolderVelocityScale))
        + ri::math::Vec3{0.0f, std::max(0.0f, options.throwUpwardBoost), 0.0f};

    held.state.velocity = throwVelocity;
    held.sleeping = false;
    held.calmSeconds = 0.0f;
    heldState.heldObjectIndex.reset();
    return true;
}

} // namespace ri::trace
