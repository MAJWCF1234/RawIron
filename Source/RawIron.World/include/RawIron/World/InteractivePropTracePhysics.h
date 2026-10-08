#pragma once

#include "RawIron/Trace/TraceScene.h"
#include "RawIron/World/InteractivePropField.h"

#include <span>
#include <string>
#include <string_view>

namespace ri::world {

/// Options for stepping interactive props against the same TraceScene the player uses.
struct InteractivePropTraceOptions {
    /// Downward gravity magnitude (m/s^2). Matches `KinematicPhysicsOptions::gravity`.
    float gravity = 9.81f;
    float bounciness = 0.72f;
    float surfaceFriction = 0.86f;
    float linearDamping = 0.98f;
    float airDrag = 0.992f;
    float bounceThreshold = 1.2f;
    bool enableSleep = true;
    float sleepLinearThreshold = 0.05f;
    float calmSecondsBeforeSleep = 0.35f;
    /// When true, refreshes each prop's `dynamic` TraceCollider bounds after the step.
    bool updateDynamicColliders = true;
    /// Optional soft AABB used only to keep showcase props inside an arena after world contacts.
    bool clampToArenaBounds = false;
    ri::spatial::Aabb arenaBounds = ri::spatial::MakeEmptyAabb();
    float arenaRestitution = 0.72f;
};

/// Stable collider id for a prop index within a named pool (e.g. "interaction", "projectile").
[[nodiscard]] std::string MakeInteractivePropColliderId(std::string_view pool, std::size_t index);

/// Builds a dynamic, non-structural TraceCollider for the prop's current AABB.
[[nodiscard]] ri::trace::TraceCollider MakeInteractivePropTraceCollider(
    std::string_view pool,
    std::size_t index,
    const InteractivePropState& prop);

/// Integrates active, non-grabbed props with Trace-backed kinematic physics, then writes
/// centers/velocities back into `props`. Grabbed and inactive props keep their authored state
/// but still refresh dynamic collider bounds when requested.
[[nodiscard]] InteractivePropStepReport StepInteractivePropFieldAgainstTrace(
    ri::trace::TraceScene& scene,
    std::span<InteractivePropState> props,
    std::string_view pool,
    float deltaSeconds,
    const InteractivePropTraceOptions& options = {});

struct InteractivePropActorPushOptions {
    /// Mass ratio used when resolving actor vs prop (actor treated as this many kilograms).
    float actorMass = 80.0f;
    /// Scales how much of the actor's velocity transfers into the prop.
    float pushGain = 1.15f;
    /// Caps imparted horizontal push speed (m/s).
    float maxPushSpeed = 6.5f;
    /// Extra separation along the contact normal after overlap resolution (meters).
    float separationSlop = 0.01f;
    /// When true, refreshes each contacted prop's dynamic TraceCollider bounds.
    bool updateDynamicColliders = true;
};

struct InteractivePropActorPushReport {
    std::uint32_t contacts = 0;
    float strongestPushSpeed = 0.0f;
};

/// Resolves AABB overlaps between an actor hull (player/capsule proxy) and interactive props.
/// Props are pushed out of the actor and receive a horizontal impulse from actor velocity.
/// Grabbed props are skipped. Returns how many props were contacted.
[[nodiscard]] InteractivePropActorPushReport PushInteractivePropsFromActor(
    ri::trace::TraceScene* scene,
    std::span<InteractivePropState> props,
    std::string_view pool,
    const ri::spatial::Aabb& actorBounds,
    const ri::math::Vec3& actorVelocity,
    const InteractivePropActorPushOptions& options = {});

/// Applies impulses to props whose collider ids appear in movement/slide `hits`.
/// This is the response path once the actor hull truly collides with tagged dynamic props.
[[nodiscard]] InteractivePropActorPushReport ImpulseInteractivePropsFromTraceHits(
    ri::trace::TraceScene* scene,
    std::span<InteractivePropState> props,
    std::string_view pool,
    std::span<const ri::trace::TraceHit> hits,
    const ri::math::Vec3& actorVelocity,
    const InteractivePropActorPushOptions& options = {});

} // namespace ri::world
