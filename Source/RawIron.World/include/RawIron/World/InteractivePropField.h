#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Spatial/Aabb.h"

#include <cstdint>
#include <span>
#include <string>

namespace ri::world {

struct InteractivePropState {
    std::string id{};
    ri::math::Vec3 position{};
    ri::math::Vec3 halfExtents{0.1f, 0.1f, 0.1f};
    ri::math::Vec3 velocity{};
    ri::math::Vec3 angularVelocityDegrees{};
    float inverseMass = 1.0f;
    float ageSeconds = 0.0f;
    float lifetimeSeconds = 0.0f;
    std::uint32_t owner = 0;
    bool active = true;
    bool grabbed = false;
};

struct InteractivePropFieldOptions {
    ri::spatial::Aabb bounds = {.min = {-3.0f, 0.0f, -3.0f}, .max = {3.0f, 3.0f, 3.0f}};
    ri::math::Vec3 gravity{0.0f, -9.81f, 0.0f};
    float linearDampingPerSecond = 0.18f;
    float restitution = 0.72f;
    float maximumStepSeconds = 1.0f / 60.0f;
    std::uint32_t maximumSubsteps = 8;
    bool resolvePropContacts = false;
};

struct InteractivePropStepReport {
    std::uint32_t substeps = 0;
    std::uint32_t boundaryContacts = 0;
    std::uint32_t propContacts = 0;
    float strongestImpactSpeed = 0.0f;
    bool clampedDuration = false;
};

struct InteractivePropEmission {
    ri::math::Vec3 position{};
    ri::math::Vec3 direction{0.0f, 0.0f, 1.0f};
    ri::math::Vec3 angularVelocityDegrees{};
    float speed = 8.0f;
    float lifetimeSeconds = 8.0f;
    bool recycleOldest = true;
};

struct InteractivePropEmissionResult {
    int propIndex = -1;
    bool recycled = false;
};

struct InteractivePropSelection {
    int propIndex = -1;
    float distance = 0.0f;
};

[[nodiscard]] InteractivePropStepReport StepInteractivePropField(
    std::span<InteractivePropState> props,
    float deltaSeconds,
    const InteractivePropFieldOptions& options = {});

[[nodiscard]] InteractivePropSelection SelectInteractiveProp(
    std::span<const InteractivePropState> props,
    const ri::math::Vec3& rayOrigin,
    const ri::math::Vec3& rayDirection,
    float maximumDistance);

[[nodiscard]] bool BeginInteractivePropGrab(
    std::span<InteractivePropState> props,
    int propIndex,
    std::uint32_t owner);

[[nodiscard]] bool MoveInteractivePropGrab(
    std::span<InteractivePropState> props,
    int propIndex,
    std::uint32_t owner,
    const ri::math::Vec3& targetPosition);

[[nodiscard]] bool EndInteractivePropGrab(
    std::span<InteractivePropState> props,
    int propIndex,
    std::uint32_t owner,
    const ri::math::Vec3& releaseVelocity);

[[nodiscard]] InteractivePropEmissionResult EmitInteractiveProp(
    std::span<InteractivePropState> props,
    const InteractivePropEmission& emission);

} // namespace ri::world
