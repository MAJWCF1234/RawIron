#include "RawIron/World/InteractivePropField.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ri::world {
namespace {

bool IsFinite(const ri::math::Vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float RayAabbDistance(const ri::math::Vec3& origin,
                      const ri::math::Vec3& direction,
                      const ri::spatial::Aabb& bounds) {
    float nearDistance = 0.0f;
    float farDistance = std::numeric_limits<float>::infinity();
    const auto testAxis = [&](const float rayOrigin,
                              const float rayDirection,
                              const float minimum,
                              const float maximum) {
        if (std::fabs(rayDirection) <= 1.0e-7f) {
            return rayOrigin >= minimum && rayOrigin <= maximum;
        }
        float first = (minimum - rayOrigin) / rayDirection;
        float second = (maximum - rayOrigin) / rayDirection;
        if (first > second) std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        return nearDistance <= farDistance;
    };
    if (!testAxis(origin.x, direction.x, bounds.min.x, bounds.max.x)
        || !testAxis(origin.y, direction.y, bounds.min.y, bounds.max.y)
        || !testAxis(origin.z, direction.z, bounds.min.z, bounds.max.z)) {
        return std::numeric_limits<float>::infinity();
    }
    return nearDistance;
}

} // namespace

InteractivePropStepReport StepInteractivePropField(
    const std::span<InteractivePropState> props,
    const float deltaSeconds,
    const InteractivePropFieldOptions& options) {
    InteractivePropStepReport report{};
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f
        || ri::spatial::IsEmpty(options.bounds)) return report;
    const float maxStep = std::clamp(options.maximumStepSeconds, 1.0f / 1000.0f, 0.1f);
    const std::uint32_t maxSubsteps = std::clamp(options.maximumSubsteps, 1U, 64U);
    const float acceptedDuration = std::min(deltaSeconds, maxStep * static_cast<float>(maxSubsteps));
    report.clampedDuration = acceptedDuration < deltaSeconds;
    report.substeps = std::max(1U, static_cast<std::uint32_t>(std::ceil(acceptedDuration / maxStep)));
    const float step = acceptedDuration / static_cast<float>(report.substeps);
    const float damping = std::clamp(1.0f - options.linearDampingPerSecond * step, 0.0f, 1.0f);
    const float restitution = std::clamp(options.restitution, 0.0f, 1.25f);
    for (std::uint32_t substep = 0; substep < report.substeps; ++substep) {
        for (InteractivePropState& prop : props) {
            if (!prop.active) continue;
            prop.ageSeconds += step;
            if (prop.lifetimeSeconds > 0.0f && prop.ageSeconds >= prop.lifetimeSeconds
                && !prop.grabbed) {
                prop.active = false;
                prop.velocity = {};
                continue;
            }
            if (prop.grabbed || !IsFinite(prop.position) || !IsFinite(prop.velocity)) continue;
            prop.velocity = (prop.velocity + options.gravity * step) * damping;
            prop.position = prop.position + prop.velocity * step;
            float* positions[]{&prop.position.x, &prop.position.y, &prop.position.z};
            float* velocities[]{&prop.velocity.x, &prop.velocity.y, &prop.velocity.z};
            const float minimums[]{options.bounds.min.x, options.bounds.min.y, options.bounds.min.z};
            const float maximums[]{options.bounds.max.x, options.bounds.max.y, options.bounds.max.z};
            const float extents[]{prop.halfExtents.x, prop.halfExtents.y, prop.halfExtents.z};
            for (int axis = 0; axis < 3; ++axis) {
                const float minimum = minimums[axis] + std::max(extents[axis], 0.001f);
                const float maximum = maximums[axis] - std::max(extents[axis], 0.001f);
                if (*positions[axis] < minimum || *positions[axis] > maximum) {
                    const float impactSpeed = std::fabs(*velocities[axis]);
                    *positions[axis] = std::clamp(*positions[axis], minimum, maximum);
                    *velocities[axis] = -*velocities[axis] * restitution;
                    ++report.boundaryContacts;
                    report.strongestImpactSpeed = std::max(report.strongestImpactSpeed, impactSpeed);
                }
            }
        }
        if (!options.resolvePropContacts) continue;
        for (std::size_t firstIndex = 0; firstIndex < props.size(); ++firstIndex) {
            InteractivePropState& first = props[firstIndex];
            if (!first.active || !IsFinite(first.position) || !IsFinite(first.velocity)
                || !IsFinite(first.halfExtents) || !std::isfinite(first.inverseMass)) continue;
            for (std::size_t secondIndex = firstIndex + 1U; secondIndex < props.size(); ++secondIndex) {
                InteractivePropState& second = props[secondIndex];
                if (!second.active || !IsFinite(second.position) || !IsFinite(second.velocity)
                    || !IsFinite(second.halfExtents) || !std::isfinite(second.inverseMass)) continue;
                const ri::math::Vec3 delta = second.position - first.position;
                const ri::math::Vec3 overlap{
                    first.halfExtents.x + second.halfExtents.x - std::fabs(delta.x),
                    first.halfExtents.y + second.halfExtents.y - std::fabs(delta.y),
                    first.halfExtents.z + second.halfExtents.z - std::fabs(delta.z)};
                if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) continue;
                int axis = overlap.y < overlap.x ? 1 : 0;
                if ((axis == 0 ? overlap.x : overlap.y) > overlap.z) axis = 2;
                ri::math::Vec3 normal{};
                const float axisDelta = axis == 0 ? delta.x : axis == 1 ? delta.y : delta.z;
                const float sign = axisDelta < 0.0f ? -1.0f : 1.0f;
                if (axis == 0) normal.x = sign;
                else if (axis == 1) normal.y = sign;
                else normal.z = sign;
                const float firstInverseMass = first.grabbed
                    ? 0.0f : std::max(first.inverseMass, 0.0f);
                const float secondInverseMass = second.grabbed
                    ? 0.0f : std::max(second.inverseMass, 0.0f);
                const float inverseMassSum = firstInverseMass + secondInverseMass;
                if (inverseMassSum <= 1.0e-7f) continue;
                const float penetration = axis == 0 ? overlap.x : axis == 1 ? overlap.y : overlap.z;
                const ri::math::Vec3 correction = normal * (penetration / inverseMassSum);
                first.position = first.position - correction * firstInverseMass;
                second.position = second.position + correction * secondInverseMass;
                const float closingSpeed = ri::math::Dot(second.velocity - first.velocity, normal);
                if (closingSpeed < 0.0f) {
                    const float impulseMagnitude = -(1.0f + restitution) * closingSpeed / inverseMassSum;
                    const ri::math::Vec3 impulse = normal * impulseMagnitude;
                    first.velocity = first.velocity - impulse * firstInverseMass;
                    second.velocity = second.velocity + impulse * secondInverseMass;
                    report.strongestImpactSpeed = std::max(
                        report.strongestImpactSpeed, -closingSpeed);
                }
                ++report.propContacts;
            }
        }
    }
    return report;
}

InteractivePropSelection SelectInteractiveProp(
    const std::span<const InteractivePropState> props,
    const ri::math::Vec3& rayOrigin,
    const ri::math::Vec3& rayDirection,
    const float maximumDistance) {
    InteractivePropSelection selection{};
    if (!IsFinite(rayOrigin) || !IsFinite(rayDirection) || !std::isfinite(maximumDistance)
        || maximumDistance <= 0.0f || ri::math::LengthSquared(rayDirection) <= 1.0e-10f) return selection;
    const ri::math::Vec3 direction = ri::math::Normalize(rayDirection);
    float nearest = maximumDistance;
    for (std::size_t index = 0; index < props.size(); ++index) {
        const InteractivePropState& prop = props[index];
        if (!prop.active || !IsFinite(prop.position) || !IsFinite(prop.halfExtents)) continue;
        const ri::spatial::Aabb bounds{
            .min = prop.position - prop.halfExtents,
            .max = prop.position + prop.halfExtents};
        const float distance = RayAabbDistance(rayOrigin, direction, bounds);
        if (distance <= nearest) {
            nearest = distance;
            selection = {static_cast<int>(index), distance};
        }
    }
    return selection;
}

bool BeginInteractivePropGrab(const std::span<InteractivePropState> props,
                              const int propIndex,
                              const std::uint32_t owner) {
    if (propIndex < 0 || static_cast<std::size_t>(propIndex) >= props.size() || owner == 0U) return false;
    InteractivePropState& prop = props[static_cast<std::size_t>(propIndex)];
    if (!prop.active || (prop.grabbed && prop.owner != owner)) return false;
    prop.grabbed = true;
    prop.owner = owner;
    prop.velocity = {};
    return true;
}

bool MoveInteractivePropGrab(const std::span<InteractivePropState> props,
                             const int propIndex,
                             const std::uint32_t owner,
                             const ri::math::Vec3& targetPosition) {
    if (propIndex < 0 || static_cast<std::size_t>(propIndex) >= props.size() || !IsFinite(targetPosition)) return false;
    InteractivePropState& prop = props[static_cast<std::size_t>(propIndex)];
    if (!prop.grabbed || prop.owner != owner) return false;
    prop.position = targetPosition;
    prop.velocity = {};
    return true;
}

bool EndInteractivePropGrab(const std::span<InteractivePropState> props,
                            const int propIndex,
                            const std::uint32_t owner,
                            const ri::math::Vec3& releaseVelocity) {
    if (propIndex < 0 || static_cast<std::size_t>(propIndex) >= props.size() || !IsFinite(releaseVelocity)) return false;
    InteractivePropState& prop = props[static_cast<std::size_t>(propIndex)];
    if (!prop.grabbed || prop.owner != owner) return false;
    prop.grabbed = false;
    prop.owner = 0;
    prop.velocity = releaseVelocity;
    return true;
}

InteractivePropEmissionResult EmitInteractiveProp(
    const std::span<InteractivePropState> props,
    const InteractivePropEmission& emission) {
    InteractivePropEmissionResult result{};
    if (!IsFinite(emission.position) || !IsFinite(emission.direction)
        || !IsFinite(emission.angularVelocityDegrees) || !std::isfinite(emission.speed)
        || !std::isfinite(emission.lifetimeSeconds)
        || emission.speed < 0.0f || emission.lifetimeSeconds < 0.0f
        || ri::math::LengthSquared(emission.direction) <= 1.0e-10f) return result;
    for (std::size_t index = 0; index < props.size(); ++index) {
        if (!props[index].active) {
            result.propIndex = static_cast<int>(index);
            break;
        }
    }
    if (result.propIndex < 0 && emission.recycleOldest) {
        float oldestAge = -1.0f;
        for (std::size_t index = 0; index < props.size(); ++index) {
            if (props[index].grabbed || !std::isfinite(props[index].ageSeconds)
                || props[index].ageSeconds <= oldestAge) continue;
            oldestAge = props[index].ageSeconds;
            result.propIndex = static_cast<int>(index);
            result.recycled = true;
        }
    }
    if (result.propIndex < 0) return result;
    InteractivePropState& prop = props[static_cast<std::size_t>(result.propIndex)];
    prop.position = emission.position;
    prop.velocity = ri::math::Normalize(emission.direction) * emission.speed;
    prop.angularVelocityDegrees = emission.angularVelocityDegrees;
    prop.ageSeconds = 0.0f;
    prop.lifetimeSeconds = emission.lifetimeSeconds;
    prop.owner = 0U;
    prop.active = true;
    prop.grabbed = false;
    return result;
}

} // namespace ri::world
