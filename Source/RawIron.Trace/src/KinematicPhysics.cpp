#include "RawIron/Trace/KinematicPhysics.h"

#include "RawIron/Math/Mat4.h"

#include <algorithm>
#include <cmath>

namespace ri::trace {
namespace {

float Clamp(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

ri::spatial::Aabb TranslateBox(const ri::spatial::Aabb& box, const ri::math::Vec3& delta) {
    if (ri::spatial::IsEmpty(box)) {
        return box;
    }
    return {
        .min = box.min + delta,
        .max = box.max + delta,
    };
}

void LockAxisToCenter(ri::spatial::Aabb& box, float originalCenterValue, char axis) {
    const ri::math::Vec3 size = ri::spatial::Size(box);
    if (axis == 'x') {
        box.min.x = originalCenterValue - (size.x * 0.5f);
        box.max.x = originalCenterValue + (size.x * 0.5f);
        return;
    }
    if (axis == 'y') {
        box.min.y = originalCenterValue - (size.y * 0.5f);
        box.max.y = originalCenterValue + (size.y * 0.5f);
        return;
    }
    box.min.z = originalCenterValue - (size.z * 0.5f);
    box.max.z = originalCenterValue + (size.z * 0.5f);
}

ri::spatial::Aabb OrientedBoxWorldBounds(const ri::math::Vec3& center,
                                         const ri::math::Vec3& halfExtents,
                                         const ri::math::Vec3& orientationDegrees) {
    const ri::math::Mat4 rot = ri::math::RotationXYZDegrees(orientationDegrees);
    ri::spatial::Aabb box = ri::spatial::MakeEmptyAabb();
    for (int ix = -1; ix <= 1; ix += 2) {
        for (int iy = -1; iy <= 1; iy += 2) {
            for (int iz = -1; iz <= 1; iz += 2) {
                const ri::math::Vec3 local{
                    static_cast<float>(ix) * halfExtents.x,
                    static_cast<float>(iy) * halfExtents.y,
                    static_cast<float>(iz) * halfExtents.z,
                };
                box = ri::spatial::ExpandByPoint(box, center + ri::math::TransformVector(rot, local));
            }
        }
    }
    return box;
}

bool ValidHalfExtents(const ri::math::Vec3& halfExtents) {
    return std::isfinite(halfExtents.x) && std::isfinite(halfExtents.y) && std::isfinite(halfExtents.z)
        && halfExtents.x > 1e-6f && halfExtents.y > 1e-6f && halfExtents.z > 1e-6f;
}

constexpr std::size_t kMaxKinematicDurationSlices = 128;

} // namespace

ri::spatial::Aabb ComputeOrientedBoxWorldBounds(const ri::math::Vec3& center,
                                               const ri::math::Vec3& halfExtents,
                                               const ri::math::Vec3& orientationDegrees) {
    return OrientedBoxWorldBounds(center, halfExtents, orientationDegrees);
}

KinematicStepResult SimulateKinematicBodyStep(
    const TraceScene& traceScene,
    const KinematicBodyState& state,
    float deltaSeconds,
    const KinematicPhysicsOptions& options,
    const KinematicVolumeModifiers& modifiers,
    const KinematicConstraintState& constraints) {
    KinematicStepResult result{};
    result.state = state;
    if (ri::spatial::IsEmpty(result.state.bounds) || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return result;
    }
    deltaSeconds = std::min(deltaSeconds, kKinematicMaxSimulationSliceSeconds);

    if (options.impactNotifyCooldownSeconds > 0.0f) {
        result.state.impactNotifyCooldownRemaining =
            std::max(0.0f, result.state.impactNotifyCooldownRemaining - deltaSeconds);
    }

    const ri::math::Vec3 startCenter = ri::spatial::Center(result.state.bounds);
    const ri::math::Vec3 extents = ri::spatial::Size(result.state.bounds) * 0.5f;
    const float rayLength = ri::math::Length(extents) + 0.2f;
    result.groundHit = traceScene.FindGroundHit(
        startCenter,
        GroundTraceOptions{
            .maxDistance = rayLength,
            .structuralOnly = true,
            .ignoreId = options.ignoreColliderId,
            .minNormalY = 0.5f,
        });

    const float maxGroundSnap = std::max(options.groundClearance + 0.08f, 0.18f);
    if (result.groundHit.has_value()) {
        const float groundDistance = result.state.bounds.min.y - result.groundHit->point.y;
        result.onGround = groundDistance <= maxGroundSnap;
        if (result.onGround && result.state.velocity.y < 0.0f) {
            result.state.velocity.y = 0.0f;
        }
    }

    if (!result.onGround && !modifiers.suppressAirGravity) {
        const float netGravityScale = Clamp(
            (options.gravityScale * modifiers.gravityScale) - modifiers.buoyancy,
            -2.0f,
            4.0f);
        float fallMult = 1.0f;
        if (result.state.velocity.y <= 0.0f && options.fallGravityMultiplier > 0.0f) {
            fallMult = options.fallGravityMultiplier;
        }
        result.state.velocity.y -= options.gravity * netGravityScale * fallMult * deltaSeconds;
    }
    result.state.velocity = result.state.velocity + (modifiers.flow * deltaSeconds);

    const float speed = ri::math::Length(result.state.velocity);
    const std::size_t maxSteps = std::max<std::size_t>(options.maxSubsteps, 1U);
    const std::size_t substeps = std::min<std::size_t>(
        maxSteps,
        std::max<std::size_t>(1U, static_cast<std::size_t>(std::ceil((speed * deltaSeconds) / 0.12f))));
    const float substepSeconds = deltaSeconds / static_cast<float>(substeps);

    for (std::size_t step = 0; step < substeps; ++step) {
        const ri::math::Vec3 stepDelta = result.state.velocity * substepSeconds;
        const SlideMoveResult slide = traceScene.SlideMoveBox(
            result.state.bounds,
            stepDelta,
            4U,
            0.001f,
            TraceOptions{.structuralOnly = true, .ignoreId = options.ignoreColliderId});
        result.state.bounds = slide.endBox;
        result.hits.insert(result.hits.end(), slide.hits.begin(), slide.hits.end());

        if (!slide.hits.empty()) {
            const TraceHit& hit = slide.hits.front();
            const float dot = ri::math::Dot(result.state.velocity, hit.normal);
            if (dot < 0.0f) {
                const float impactSpeed = std::fabs(dot);
                const float bounce = impactSpeed < options.bounceThreshold ? 0.0f : Clamp(options.bounciness, 0.0f, 1.0f);
                result.state.velocity = result.state.velocity - (hit.normal * (dot * (1.0f + bounce)));
                if (bounce <= 0.0f && std::fabs(hit.normal.y) > 0.5f) {
                    result.state.velocity.y = 0.0f;
                }
            }
            result.state.angularVelocity = result.state.angularVelocity
                + (ri::math::Cross(result.state.velocity, hit.normal) * options.angularImpactScale);
            if (!result.impact.has_value() && ri::math::Length(result.state.velocity) > 0.1f) {
                const bool cooldownAllows = options.impactNotifyCooldownSeconds <= 0.0f
                    || result.state.impactNotifyCooldownRemaining <= 0.0f;
                if (cooldownAllows) {
                    result.impact = KinematicImpact{
                        .position = hit.point,
                        .normal = hit.normal,
                        .velocity = result.state.velocity,
                        .speed = ri::math::Length(result.state.velocity),
                        .colliderId = hit.id,
                    };
                    if (options.impactNotifyCooldownSeconds > 0.0f) {
                        result.state.impactNotifyCooldownRemaining = options.impactNotifyCooldownSeconds;
                    }
                }
            }
        }
    }

    const float linearDamping = std::pow(Clamp(options.linearDamping, 0.0f, 1.0f), deltaSeconds * 60.0f);
    const float angularDamping = std::pow(Clamp(options.angularDamping, 0.0f, 1.0f), deltaSeconds * 60.0f);
    result.state.velocity = result.state.velocity * linearDamping;
    result.state.angularVelocity = result.state.angularVelocity * angularDamping;

    if (result.onGround) {
        const float groundFriction = std::pow(Clamp(options.surfaceFriction, 0.0f, 1.0f), deltaSeconds * 60.0f);
        const float rollingResistance = std::pow(Clamp(options.rollingResistance, 0.0f, 1.0f), deltaSeconds * 60.0f);
        result.state.velocity.x *= groundFriction;
        result.state.velocity.z *= groundFriction;
        result.state.angularVelocity = result.state.angularVelocity * rollingResistance;
    } else {
        const float airDrag = std::pow(Clamp(options.airDrag, 0.0f, 1.0f), deltaSeconds * 60.0f);
        result.state.velocity.x *= airDrag;
        result.state.velocity.z *= airDrag;
    }

    if (modifiers.drag > 0.0f) {
        const float volumeDrag = std::max(0.0f, 1.0f - (modifiers.drag * deltaSeconds * 0.35f));
        result.state.velocity = result.state.velocity * volumeDrag;
    }

    if (!result.onGround && options.maxFallSpeed > 0.0f) {
        result.state.velocity.y = std::max(result.state.velocity.y, -options.maxFallSpeed);
    }

    const float minVelocitySquared = options.minVelocity * options.minVelocity;
    if (ri::math::LengthSquared(result.state.velocity) < minVelocitySquared) {
        result.state.velocity = {};
    }
    if (ri::math::LengthSquared(result.state.angularVelocity) < minVelocitySquared) {
        result.state.angularVelocity = {};
    }

    if (constraints.lockX || constraints.lockY || constraints.lockZ) {
        if (constraints.lockX) {
            LockAxisToCenter(result.state.bounds, startCenter.x, 'x');
            result.state.velocity.x = 0.0f;
        }
        if (constraints.lockY) {
            LockAxisToCenter(result.state.bounds, startCenter.y, 'y');
            result.state.velocity.y = 0.0f;
        }
        if (constraints.lockZ) {
            LockAxisToCenter(result.state.bounds, startCenter.z, 'z');
            result.state.velocity.z = 0.0f;
        }
    }

    const ri::math::Vec3 endCenter = ri::spatial::Center(result.state.bounds);
    const std::optional<TraceHit> settleHit = traceScene.FindGroundHit(
        endCenter,
        GroundTraceOptions{
            .maxDistance = rayLength,
            .structuralOnly = true,
            .ignoreId = options.ignoreColliderId,
            .minNormalY = 0.5f,
        });
    if (settleHit.has_value()) {
        const float desiredBottom = settleHit->point.y + options.groundClearance;
        const float objectBottom = result.state.bounds.min.y;
        if (objectBottom < desiredBottom) {
            result.state.bounds = TranslateBox(result.state.bounds, {0.0f, desiredBottom - objectBottom, 0.0f});
            result.state.velocity.y = std::max(0.0f, result.state.velocity.y);
            result.onGround = true;
            result.groundHit = settleHit;
        } else if (objectBottom > desiredBottom && (objectBottom - desiredBottom) <= maxGroundSnap) {
            result.state.bounds = TranslateBox(result.state.bounds, {0.0f, desiredBottom - objectBottom, 0.0f});
            result.state.velocity.y = 0.0f;
            result.onGround = true;
            result.groundHit = settleHit;
        }
    }

    return result;
}

KinematicStepResult SimulateKinematicBodyForDuration(
    const TraceScene& traceScene,
    const KinematicBodyState& state,
    float totalDeltaSeconds,
    const KinematicPhysicsOptions& options,
    const KinematicVolumeModifiers& modifiers,
    const KinematicConstraintState& constraints,
    KinematicAdvanceStats* outStats) {
    KinematicAdvanceStats stackStats{};
    KinematicAdvanceStats& stats = outStats != nullptr ? *outStats : stackStats;
    stats.sliceCount = 0;
    stats.consumedSeconds = 0.0f;
    stats.hitSliceBudget = false;

    KinematicStepResult combined{};
    combined.state = state;
    if (!std::isfinite(totalDeltaSeconds) || totalDeltaSeconds <= 0.0f) {
        return combined;
    }

    float remaining = totalDeltaSeconds;
    while (remaining > 1e-6f && stats.sliceCount < kMaxKinematicDurationSlices) {
        const float step = std::min(remaining, kKinematicMaxSimulationSliceSeconds);
        const KinematicStepResult slice =
            SimulateKinematicBodyStep(traceScene, combined.state, step, options, modifiers, constraints);
        combined.hits.insert(combined.hits.end(), slice.hits.begin(), slice.hits.end());
        if (!combined.impact.has_value() && slice.impact.has_value()) {
            combined.impact = slice.impact;
        }
        combined.state = slice.state;
        combined.onGround = slice.onGround;
        combined.groundHit = slice.groundHit;
        remaining -= step;
        stats.consumedSeconds += step;
        stats.sliceCount += 1;
    }
    stats.hitSliceBudget = remaining > 1e-6f;
    return combined;
}

OrientedKinematicStepResult SimulateOrientedKinematicBodyStep(
    const TraceScene& traceScene,
    const OrientedKinematicBodyState& state,
    float deltaSeconds,
    const KinematicPhysicsOptions& options,
    const KinematicVolumeModifiers& modifiers,
    const KinematicConstraintState& constraints) {
    OrientedKinematicStepResult result{};
    result.state = state;
    result.worldBounds =
        OrientedBoxWorldBounds(result.state.center, result.state.halfExtents, result.state.orientationDegrees);

    if (!ValidHalfExtents(result.state.halfExtents) || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f
        || !std::isfinite(result.state.center.x) || !std::isfinite(result.state.center.y)
        || !std::isfinite(result.state.center.z)) {
        return result;
    }

    deltaSeconds = std::min(deltaSeconds, kKinematicMaxSimulationSliceSeconds);

    if (options.impactNotifyCooldownSeconds > 0.0f) {
        result.state.impactNotifyCooldownRemaining =
            std::max(0.0f, result.state.impactNotifyCooldownRemaining - deltaSeconds);
    }

    const ri::math::Vec3 startCenter = result.state.center;

    ri::spatial::Aabb bounds =
        OrientedBoxWorldBounds(result.state.center, result.state.halfExtents, result.state.orientationDegrees);
    const ri::math::Vec3 extentsForSampling = ri::spatial::Size(bounds) * 0.5f;
    const float rayLength = ri::math::Length(extentsForSampling) + 0.2f;

    result.groundHit = traceScene.FindGroundHit(
        result.state.center,
        GroundTraceOptions{
            .maxDistance = rayLength,
            .structuralOnly = true,
            .ignoreId = options.ignoreColliderId,
            .minNormalY = 0.5f,
        });

    const float maxGroundSnap = std::max(options.groundClearance + 0.08f, 0.18f);
    if (result.groundHit.has_value()) {
        const float groundDistance = bounds.min.y - result.groundHit->point.y;
        result.onGround = groundDistance <= maxGroundSnap;
        if (result.onGround && result.state.velocity.y < 0.0f) {
            result.state.velocity.y = 0.0f;
        }
    }

    if (!result.onGround && !modifiers.suppressAirGravity) {
        const float netG = Clamp(
            (options.gravityScale * modifiers.gravityScale) - modifiers.buoyancy,
            -2.0f,
            4.0f);
        float fallMult = 1.0f;
        if (result.state.velocity.y <= 0.0f && options.fallGravityMultiplier > 0.0f) {
            fallMult = options.fallGravityMultiplier;
        }
        result.state.velocity.y -= options.gravity * netG * fallMult * deltaSeconds;
    }
    result.state.velocity = result.state.velocity + (modifiers.flow * deltaSeconds);

    const float speed = ri::math::Length(result.state.velocity);
    const std::size_t maxSteps = std::max<std::size_t>(options.maxSubsteps, 1U);
    const std::size_t substeps = std::min<std::size_t>(
        maxSteps,
        std::max<std::size_t>(1U, static_cast<std::size_t>(std::ceil((speed * deltaSeconds) / 0.12f))));
    const float substepSeconds = deltaSeconds / static_cast<float>(substeps);

    for (std::size_t step = 0; step < substeps; ++step) {
        bounds = OrientedBoxWorldBounds(
            result.state.center, result.state.halfExtents, result.state.orientationDegrees);
        const ri::math::Vec3 stepDelta = result.state.velocity * substepSeconds;
        const SlideMoveResult slide = traceScene.SlideMoveBox(
            bounds,
            stepDelta,
            4U,
            0.001f,
            TraceOptions{.structuralOnly = true, .ignoreId = options.ignoreColliderId});
        result.state.center = ri::spatial::Center(slide.endBox);
        result.hits.insert(result.hits.end(), slide.hits.begin(), slide.hits.end());

        if (!slide.hits.empty()) {
            const TraceHit& hit = slide.hits.front();
            const float dot = ri::math::Dot(result.state.velocity, hit.normal);
            if (dot < 0.0f) {
                const float impactSpeed = std::fabs(dot);
                const float bounce =
                    impactSpeed < options.bounceThreshold ? 0.0f : Clamp(options.bounciness, 0.0f, 1.0f);
                result.state.velocity =
                    result.state.velocity - (hit.normal * (dot * (1.0f + bounce)));
                if (bounce <= 0.0f && std::fabs(hit.normal.y) > 0.5f) {
                    result.state.velocity.y = 0.0f;
                }
            }
            result.state.angularVelocity = result.state.angularVelocity
                + (ri::math::Cross(result.state.velocity, hit.normal) * options.angularImpactScale);
            if (!result.impact.has_value() && ri::math::Length(result.state.velocity) > 0.1f) {
                const bool cooldownAllows = options.impactNotifyCooldownSeconds <= 0.0f
                    || result.state.impactNotifyCooldownRemaining <= 0.0f;
                if (cooldownAllows) {
                    result.impact = KinematicImpact{
                        .position = hit.point,
                        .normal = hit.normal,
                        .velocity = result.state.velocity,
                        .speed = ri::math::Length(result.state.velocity),
                        .colliderId = hit.id,
                    };
                    if (options.impactNotifyCooldownSeconds > 0.0f) {
                        result.state.impactNotifyCooldownRemaining = options.impactNotifyCooldownSeconds;
                    }
                }
            }
        }

        result.state.orientationDegrees =
            result.state.orientationDegrees
            + ri::math::RadiansToDegrees(result.state.angularVelocity * substepSeconds);
    }

    const float linearD = std::pow(Clamp(options.linearDamping, 0.0f, 1.0f), deltaSeconds * 60.0f);
    const float angularD = std::pow(Clamp(options.angularDamping, 0.0f, 1.0f), deltaSeconds * 60.0f);
    result.state.velocity = result.state.velocity * linearD;
    result.state.angularVelocity = result.state.angularVelocity * angularD;

    if (result.onGround) {
        const float groundF = std::pow(Clamp(options.surfaceFriction, 0.0f, 1.0f), deltaSeconds * 60.0f);
        const float rollR = std::pow(Clamp(options.rollingResistance, 0.0f, 1.0f), deltaSeconds * 60.0f);
        result.state.velocity.x *= groundF;
        result.state.velocity.z *= groundF;
        result.state.angularVelocity = result.state.angularVelocity * rollR;
    } else {
        const float airD = std::pow(Clamp(options.airDrag, 0.0f, 1.0f), deltaSeconds * 60.0f);
        result.state.velocity.x *= airD;
        result.state.velocity.z *= airD;
    }

    if (modifiers.drag > 0.0f) {
        const float volumeDrag = std::max(0.0f, 1.0f - (modifiers.drag * deltaSeconds * 0.35f));
        result.state.velocity = result.state.velocity * volumeDrag;
    }

    if (!result.onGround && options.maxFallSpeed > 0.0f) {
        result.state.velocity.y = std::max(result.state.velocity.y, -options.maxFallSpeed);
    }

    const float minVelSq = options.minVelocity * options.minVelocity;
    if (ri::math::LengthSquared(result.state.velocity) < minVelSq) {
        result.state.velocity = {};
    }
    if (ri::math::LengthSquared(result.state.angularVelocity) < minVelSq) {
        result.state.angularVelocity = {};
    }

    if (constraints.lockX || constraints.lockY || constraints.lockZ) {
        if (constraints.lockX) {
            result.state.center.x = startCenter.x;
            result.state.velocity.x = 0.0f;
        }
        if (constraints.lockY) {
            result.state.center.y = startCenter.y;
            result.state.velocity.y = 0.0f;
        }
        if (constraints.lockZ) {
            result.state.center.z = startCenter.z;
            result.state.velocity.z = 0.0f;
        }
    }

    bounds =
        OrientedBoxWorldBounds(result.state.center, result.state.halfExtents, result.state.orientationDegrees);
    const std::optional<TraceHit> settleHit = traceScene.FindGroundHit(
        result.state.center,
        GroundTraceOptions{
            .maxDistance = rayLength,
            .structuralOnly = true,
            .ignoreId = options.ignoreColliderId,
            .minNormalY = 0.5f,
        });
    if (settleHit.has_value()) {
        const float desiredBottom = settleHit->point.y + options.groundClearance;
        const float objectBottom = bounds.min.y;
        if (objectBottom < desiredBottom) {
            result.state.center.y += desiredBottom - objectBottom;
            result.state.velocity.y = std::max(0.0f, result.state.velocity.y);
            result.onGround = true;
            result.groundHit = settleHit;
        } else if (objectBottom > desiredBottom && (objectBottom - desiredBottom) <= maxGroundSnap) {
            result.state.center.y -= objectBottom - desiredBottom;
            result.state.velocity.y = 0.0f;
            result.onGround = true;
            result.groundHit = settleHit;
        }
    }

    result.worldBounds =
        OrientedBoxWorldBounds(result.state.center, result.state.halfExtents, result.state.orientationDegrees);
    return result;
}

OrientedKinematicStepResult SimulateOrientedKinematicBodyForDuration(
    const TraceScene& traceScene,
    const OrientedKinematicBodyState& state,
    float totalDeltaSeconds,
    const KinematicPhysicsOptions& options,
    const KinematicVolumeModifiers& modifiers,
    const KinematicConstraintState& constraints,
    KinematicAdvanceStats* outStats) {
    KinematicAdvanceStats stackStats{};
    KinematicAdvanceStats& stats = outStats != nullptr ? *outStats : stackStats;
    stats.sliceCount = 0;
    stats.consumedSeconds = 0.0f;
    stats.hitSliceBudget = false;

    OrientedKinematicStepResult combined{};
    combined.state = state;
    combined.worldBounds =
        OrientedBoxWorldBounds(combined.state.center, combined.state.halfExtents, combined.state.orientationDegrees);
    if (!std::isfinite(totalDeltaSeconds) || totalDeltaSeconds <= 0.0f) {
        return combined;
    }

    float remaining = totalDeltaSeconds;
    while (remaining > 1e-6f && stats.sliceCount < kMaxKinematicDurationSlices) {
        const float step = std::min(remaining, kKinematicMaxSimulationSliceSeconds);
        const OrientedKinematicStepResult slice =
            SimulateOrientedKinematicBodyStep(traceScene, combined.state, step, options, modifiers, constraints);
        combined.hits.insert(combined.hits.end(), slice.hits.begin(), slice.hits.end());
        if (!combined.impact.has_value() && slice.impact.has_value()) {
            combined.impact = slice.impact;
        }
        combined.state = slice.state;
        combined.onGround = slice.onGround;
        combined.groundHit = slice.groundHit;
        combined.worldBounds = slice.worldBounds;
        remaining -= step;
        stats.consumedSeconds += step;
        stats.sliceCount += 1;
    }
    stats.hitSliceBudget = remaining > 1e-6f;
    return combined;
}

} // namespace ri::trace
