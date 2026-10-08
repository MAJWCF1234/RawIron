#include "RawIron/World/InteractivePropTracePhysics.h"

#include "RawIron/Math/Vec3.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/KinematicPhysics.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ri::world {
namespace {

bool FiniteVec3(const ri::math::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

ri::spatial::Aabb BoundsFromProp(const InteractivePropState& prop) {
    const ri::math::Vec3 extents{
        std::max(0.001f, prop.halfExtents.x),
        std::max(0.001f, prop.halfExtents.y),
        std::max(0.001f, prop.halfExtents.z),
    };
    return ri::spatial::Aabb{
        .min = prop.position - extents,
        .max = prop.position + extents,
    };
}

void WritePropFromBounds(InteractivePropState& prop, const ri::spatial::Aabb& bounds, const ri::math::Vec3& velocity) {
    prop.position = ri::spatial::Center(bounds);
    prop.velocity = velocity;
}

void ClampPropToArena(InteractivePropState& prop,
                      const ri::spatial::Aabb& arena,
                      const float restitution,
                      InteractivePropStepReport& report) {
    if (ri::spatial::IsEmpty(arena) || !FiniteVec3(arena.min) || !FiniteVec3(arena.max)) {
        return;
    }
    const float rest = std::clamp(restitution, 0.0f, 1.25f);
    float* positions[]{&prop.position.x, &prop.position.y, &prop.position.z};
    float* velocities[]{&prop.velocity.x, &prop.velocity.y, &prop.velocity.z};
    const float minimums[]{arena.min.x, arena.min.y, arena.min.z};
    const float maximums[]{arena.max.x, arena.max.y, arena.max.z};
    const float extents[]{prop.halfExtents.x, prop.halfExtents.y, prop.halfExtents.z};
    for (int axis = 0; axis < 3; ++axis) {
        const float minimum = minimums[axis] + std::max(extents[axis], 0.001f);
        const float maximum = maximums[axis] - std::max(extents[axis], 0.001f);
        if (minimum > maximum) {
            *positions[axis] = minimums[axis] * 0.5f + maximums[axis] * 0.5f;
            *velocities[axis] = 0.0f;
            continue;
        }
        if (*positions[axis] < minimum || *positions[axis] > maximum) {
            const float impactSpeed = std::fabs(*velocities[axis]);
            *positions[axis] = std::clamp(*positions[axis], minimum, maximum);
            *velocities[axis] = -*velocities[axis] * rest;
            ++report.boundaryContacts;
            report.strongestImpactSpeed = std::max(report.strongestImpactSpeed, impactSpeed);
        }
    }
}

} // namespace

std::string MakeInteractivePropColliderId(const std::string_view pool, const std::size_t index) {
    return std::string(pool) + "-prop-" + std::to_string(index);
}

ri::trace::TraceCollider MakeInteractivePropTraceCollider(
    const std::string_view pool,
    const std::size_t index,
    const InteractivePropState& prop) {
    return ri::trace::TraceCollider{
        .id = MakeInteractivePropColliderId(pool, index),
        .bounds = BoundsFromProp(prop),
        .structural = false,
        .dynamic = true,
        .simulationTags = {"dynamicCollider", "interactive-prop", std::string(pool)},
        .simulationFlags = 4U,
    };
}

InteractivePropStepReport StepInteractivePropFieldAgainstTrace(
    ri::trace::TraceScene& scene,
    const std::span<InteractivePropState> props,
    const std::string_view pool,
    const float deltaSeconds,
    const InteractivePropTraceOptions& options) {
    InteractivePropStepReport report{};
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f || props.empty()) {
        return report;
    }

    ri::trace::KinematicPhysicsOptions physics{};
    physics.gravity = std::max(0.0f, std::isfinite(options.gravity) ? options.gravity : 9.81f);
    physics.bounciness = std::clamp(std::isfinite(options.bounciness) ? options.bounciness : 0.72f, 0.0f, 1.25f);
    physics.surfaceFriction =
        std::clamp(std::isfinite(options.surfaceFriction) ? options.surfaceFriction : 0.86f, 0.0f, 1.0f);
    physics.linearDamping =
        std::clamp(std::isfinite(options.linearDamping) ? options.linearDamping : 0.98f, 0.0f, 1.0f);
    physics.airDrag = std::clamp(std::isfinite(options.airDrag) ? options.airDrag : 0.992f, 0.0f, 1.0f);
    physics.bounceThreshold =
        std::max(0.0f, std::isfinite(options.bounceThreshold) ? options.bounceThreshold : 1.2f);
    physics.maxStepUpHeight = 0.0f;
    physics.structuralOnly = false;

    // Step one prop at a time and publish dynamic bounds between bodies so later props see earlier
    // contacts in the same frame — approximate stacking without a full rigid-body solver.
    // Sleep is intentionally not tracked here: `InteractivePropState` has no persistent calm
    // counters; callers that need sleep should keep `KinematicObjectSlot`s beside the props.
    (void)options.enableSleep;
    (void)options.sleepLinearThreshold;
    (void)options.calmSecondsBeforeSleep;

    for (std::size_t index = 0; index < props.size(); ++index) {
        InteractivePropState& prop = props[index];
        if (!prop.active || !FiniteVec3(prop.position) || !FiniteVec3(prop.halfExtents)) {
            continue;
        }

        const std::string colliderId = MakeInteractivePropColliderId(pool, index);
        if (prop.grabbed) {
            if (options.updateDynamicColliders) {
                (void)scene.TrySetDynamicColliderBounds(colliderId, BoundsFromProp(prop));
            }
            continue;
        }

        ri::trace::KinematicBodyState body{};
        body.bounds = BoundsFromProp(prop);
        body.velocity = FiniteVec3(prop.velocity) ? prop.velocity : ri::math::Vec3{};
        // Cosmetic spin stays on InteractivePropState (degrees). Keep kinematic angular calm.
        body.angularVelocity = {};

        ri::trace::KinematicPhysicsOptions merged = physics;
        merged.ignoreColliderId = colliderId;
        const ri::trace::KinematicStepResult step =
            ri::trace::SimulateKinematicBodyForDuration(scene, body, deltaSeconds, merged);
        WritePropFromBounds(prop, step.state.bounds, step.state.velocity);
        report.substeps = std::max(report.substeps, 1U);
        if (step.impact.has_value()) {
            ++report.boundaryContacts;
            report.strongestImpactSpeed = std::max(report.strongestImpactSpeed, step.impact->speed);
        }
        if (!step.hits.empty()) {
            report.propContacts += static_cast<std::uint32_t>(step.hits.size());
        }

        if (options.clampToArenaBounds) {
            ClampPropToArena(prop, options.arenaBounds, options.arenaRestitution, report);
        }

        if (prop.lifetimeSeconds > 0.0f) {
            prop.ageSeconds += deltaSeconds;
            if (prop.ageSeconds >= prop.lifetimeSeconds) {
                prop.active = false;
                prop.velocity = {};
            }
        }

        if (options.updateDynamicColliders) {
            (void)scene.TrySetDynamicColliderBounds(colliderId, BoundsFromProp(prop));
        }
    }

    return report;
}

InteractivePropActorPushReport PushInteractivePropsFromActor(
    ri::trace::TraceScene* scene,
    const std::span<InteractivePropState> props,
    const std::string_view pool,
    const ri::spatial::Aabb& actorBounds,
    const ri::math::Vec3& actorVelocity,
    const InteractivePropActorPushOptions& options) {
    InteractivePropActorPushReport report{};
    if (ri::spatial::IsEmpty(actorBounds) || !FiniteVec3(actorBounds.min) || !FiniteVec3(actorBounds.max)
        || props.empty()) {
        return report;
    }

    const float actorMass = std::max(1.0f, std::isfinite(options.actorMass) ? options.actorMass : 80.0f);
    const float pushGain = std::clamp(std::isfinite(options.pushGain) ? options.pushGain : 1.15f, 0.0f, 4.0f);
    const float maxPushSpeed =
        std::max(0.0f, std::isfinite(options.maxPushSpeed) ? options.maxPushSpeed : 6.5f);
    const float separationSlop =
        std::max(0.0f, std::isfinite(options.separationSlop) ? options.separationSlop : 0.01f);
    const ri::math::Vec3 safeActorVelocity = FiniteVec3(actorVelocity) ? actorVelocity : ri::math::Vec3{};
    const ri::math::Vec3 actorCenter = ri::spatial::Center(actorBounds);

    for (std::size_t index = 0; index < props.size(); ++index) {
        InteractivePropState& prop = props[index];
        if (!prop.active || prop.grabbed || !FiniteVec3(prop.position) || !FiniteVec3(prop.halfExtents)
            || !std::isfinite(prop.inverseMass) || prop.inverseMass <= 0.0f) {
            continue;
        }

        const ri::spatial::Aabb propBounds = BoundsFromProp(prop);
        const ri::math::Vec3 overlap{
            std::min(actorBounds.max.x, propBounds.max.x) - std::max(actorBounds.min.x, propBounds.min.x),
            std::min(actorBounds.max.y, propBounds.max.y) - std::max(actorBounds.min.y, propBounds.min.y),
            std::min(actorBounds.max.z, propBounds.max.z) - std::max(actorBounds.min.z, propBounds.min.z),
        };
        if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) {
            continue;
        }

        int axis = overlap.y < overlap.x ? 1 : 0;
        if ((axis == 0 ? overlap.x : overlap.y) > overlap.z) {
            axis = 2;
        }
        // Prefer horizontal push for standing overlaps so walking through props feels Source-like.
        if (axis == 1 && overlap.x > 1.0e-4f && overlap.z > 1.0e-4f) {
            axis = overlap.x < overlap.z ? 0 : 2;
        }

        ri::math::Vec3 normal{};
        const float axisDelta = (axis == 0 ? prop.position.x : axis == 1 ? prop.position.y : prop.position.z)
            - (axis == 0 ? actorCenter.x : axis == 1 ? actorCenter.y : actorCenter.z);
        const float sign = axisDelta < 0.0f ? -1.0f : 1.0f;
        if (axis == 0) {
            normal.x = sign;
        } else if (axis == 1) {
            normal.y = sign;
        } else {
            normal.z = sign;
        }

        const float penetration = (axis == 0 ? overlap.x : axis == 1 ? overlap.y : overlap.z) + separationSlop;
        prop.position = prop.position + normal * penetration;

        const float propMass = 1.0f / std::max(prop.inverseMass, 1.0e-4f);
        const float massSum = actorMass + propMass;
        const float closing = ri::math::Dot(safeActorVelocity - prop.velocity, normal);
        if (closing > 0.0f) {
            const float impulse = closing * pushGain * (actorMass / massSum);
            ri::math::Vec3 deltaV = normal * impulse;
            // Keep vertical kick mild so walking into a cube does not launch it.
            deltaV.y *= 0.25f;
            prop.velocity = prop.velocity + deltaV;
            const float horizontalSpeed = std::sqrt(prop.velocity.x * prop.velocity.x + prop.velocity.z * prop.velocity.z);
            if (horizontalSpeed > maxPushSpeed && horizontalSpeed > 1.0e-5f) {
                const float scale = maxPushSpeed / horizontalSpeed;
                prop.velocity.x *= scale;
                prop.velocity.z *= scale;
            }
            report.strongestPushSpeed = std::max(report.strongestPushSpeed, horizontalSpeed);
        }

        ++report.contacts;
        if (options.updateDynamicColliders && scene != nullptr) {
            (void)scene->TrySetDynamicColliderBounds(
                MakeInteractivePropColliderId(pool, index), BoundsFromProp(prop));
        }
    }

    return report;
}

} // namespace ri::world
