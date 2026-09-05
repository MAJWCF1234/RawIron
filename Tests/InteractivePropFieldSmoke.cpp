#include "RawIron/World/InteractivePropField.h"
#include "RawIron/World/InteractivePropReplication.h"
#include "RawIron/World/InteractivePropGrab.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
bool Require(const bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
}

int main() {
    std::vector<ri::world::InteractivePropState> props{{
        .id = "cube",
        .position = {0.0f, 1.0f, 0.0f},
        .halfExtents = {0.1f, 0.1f, 0.1f}}};
    bool ok = true;
    {
        auto invalidProps = props;
        ri::world::InteractivePropFieldOptions invalidOptions;
        invalidOptions.maximumStepSeconds = std::numeric_limits<float>::quiet_NaN();
        const auto rejected = ri::world::StepInteractivePropField(invalidProps, .016f, invalidOptions);
        ok &= Require(rejected.substeps == 0 && invalidProps[0].ageSeconds == 0,
                      "non-finite step options must be rejected without mutating props");
        invalidOptions = {};
        invalidProps[0].halfExtents = {10, 10, 10};
        (void)ri::world::StepInteractivePropField(invalidProps, .016f, invalidOptions);
        ok &= Require(invalidProps[0].position.x == 0 && invalidProps[0].position.y == 1.5f
                      && invalidProps[0].position.z == 0 && ri::math::LengthSquared(invalidProps[0].velocity) == 0,
                      "props larger than the field must be centered without reversed clamp bounds");
    }
    const ri::math::Vec3 oversizedDirection{std::numeric_limits<float>::max(), 0, 0};
    ok &= Require(ri::world::SelectInteractiveProp(props, props[0].position, oversizedDirection, 5).propIndex < 0,
                  "overflowing ray direction must not select a prop containing the origin");
    {
        auto invalidPool = props;
        invalidPool[0].active = false;
        const auto rejected = ri::world::EmitInteractiveProp(invalidPool,
            {.direction = oversizedDirection, .speed = 4});
        ok &= Require(rejected.propIndex < 0 && !invalidPool[0].active,
                      "overflowing emission direction must not consume a pool slot");
    }
    {
        auto grabbedProps = props;
        ri::world::InteractivePropGrab desktop, headset;
        ok &= Require(ri::world::BeginRayPropGrab(desktop, grabbedProps, 100U, {0,1,-2}, {0,0,1}), "desktop ray grab");
        ok &= Require(!ri::world::BeginRayPropGrab(headset, grabbedProps, 1U, {0,1,-2}, {0,0,1}), "headset cannot steal desktop ownership");
        ok &= Require(ri::world::UpdateRayPropGrab(desktop, grabbedProps, {0,1,-2}, {0,0,1}, .016f), "grab first sample");
        ok &= Require(ri::world::UpdateRayPropGrab(desktop, grabbedProps, {10,1,-2}, {0,0,1}, .016f), "grab moved sample");
        ri::world::ReleaseRayPropGrab(desktop, grabbedProps);
        ok &= Require(std::abs(ri::math::Length(grabbedProps[0].velocity)-12.0f)<.001f && !grabbedProps[0].grabbed,
            "throw velocity bounded to 12 m/s");
        grabbedProps = props;
        ok &= Require(ri::world::BeginRayPropGrab(headset, grabbedProps, 1U, {0,1,-2}, {0,0,1}), "headset uses same grab implementation");
        (void)ri::world::UpdateRayPropGrab(headset, grabbedProps, {0,1,-2}, {0,0,1}, .016f);
        (void)ri::world::UpdateRayPropGrab(headset, grabbedProps, {1,1,-2}, {0,0,1}, .016f);
        ri::world::ReleaseRayPropGrab(headset, grabbedProps, false);
        ok &= Require(ri::math::LengthSquared(grabbedProps[0].velocity)==0 && grabbedProps[0].owner==0,
            "focus/tracking loss releases without throwing");
        grabbedProps = props;
        (void)ri::world::BeginRayPropGrab(headset, grabbedProps, 1U, {0,1,-2}, {0,0,1});
        ok &= Require(!ri::world::UpdateRayPropGrab(headset, grabbedProps,
            {std::numeric_limits<float>::quiet_NaN(),0,0}, {0,0,1}, .016f) && !grabbedProps[0].grabbed,
            "non-finite tracking releases safely");
    }
    const ri::world::InteractivePropSelection hit = ri::world::SelectInteractiveProp(
        props, {0.0f, 1.0f, -2.0f}, {0.0f, 0.0f, 1.0f}, 5.0f);
    ok &= Require(hit.propIndex == 0 && hit.distance > 1.8f && hit.distance < 2.0f,
                  "ray selection should return the nearest prop surface");
    ok &= Require(ri::world::BeginInteractivePropGrab(props, 0, 7), "owner should acquire prop");
    ok &= Require(!ri::world::BeginInteractivePropGrab(props, 0, 8), "second owner must not steal prop");
    ok &= Require(ri::world::MoveInteractivePropGrab(props, 0, 7, {1.0f, 1.5f, 0.0f}),
                  "owner should move grabbed prop");
    ok &= Require(ri::world::EndInteractivePropGrab(props, 0, 7, {2.0f, 0.0f, 0.0f}),
                  "owner should release prop with velocity");
    ri::world::InteractivePropFieldOptions options{};
    options.bounds = {.min = {-1.2f, 0.0f, -1.2f}, .max = {1.2f, 2.0f, 1.2f}};
    const ri::world::InteractivePropStepReport report =
        ri::world::StepInteractivePropField(props, 1.0f, options);
    ok &= Require(report.clampedDuration && report.substeps == options.maximumSubsteps,
                  "extreme durations should use a bounded substep budget");
    ok &= Require(props[0].position.x <= 1.1f && std::isfinite(props[0].position.y),
                  "bounded simulation should remain finite and inside the field");

    std::vector<ri::world::InteractivePropState> pool(2);
    for (ri::world::InteractivePropState& pooled : pool) {
        pooled.active = false;
        pooled.halfExtents = {0.1f, 0.1f, 0.1f};
    }
    const ri::world::InteractivePropEmissionResult emitted = ri::world::EmitInteractiveProp(
        pool,
        {.position = {-0.5f, 1.0f, 0.0f},
         .direction = {1.0f, 0.0f, 0.0f},
         .speed = 4.0f,
         .lifetimeSeconds = 0.05f});
    ok &= Require(emitted.propIndex == 0 && pool[0].active && pool[0].velocity.x == 4.0f,
                  "emission should activate the first free deterministic pool slot");
    ri::world::InteractivePropFieldOptions contactOptions{};
    contactOptions.bounds = {.min = {-2.0f, 0.0f, -2.0f}, .max = {2.0f, 2.0f, 2.0f}};
    contactOptions.gravity = {};
    contactOptions.resolvePropContacts = true;
    pool[1].active = true;
    pool[1].position = {-0.25f, 1.0f, 0.0f};
    pool[1].velocity = {};
    const ri::world::InteractivePropStepReport contactReport =
        ri::world::StepInteractivePropField(pool, 1.0f / 60.0f, contactOptions);
    ok &= Require(contactReport.propContacts > 0U && pool[1].velocity.x > 0.0f,
                  "dynamic props should resolve overlap and transfer forward impulse");
    (void)ri::world::StepInteractivePropField(pool, 0.1f, contactOptions);
    ok &= Require(!pool[0].active, "finite-lifetime emitted props should return to the pool");

    const ri::world::InteractivePropEmissionResult invalid = ri::world::EmitInteractiveProp(
        pool,
        {.direction = {0.0f, 0.0f, 0.0f}, .speed = 1.0f});
    ok &= Require(invalid.propIndex < 0, "invalid emission directions must be rejected");

    std::vector<ri::world::InteractivePropState> authoritative(2);
    authoritative[0].id = "fixed-pool-0";
    authoritative[1].id = "fixed-pool-1";
    const ri::runtime::SnapshotBlob baseline =
        ri::world::BuildInteractivePropSnapshot(authoritative, 40U);
    authoritative[1].position = {2.5f, 1.25f, -0.5f};
    authoritative[1].velocity = {3.0f, 0.0f, 1.0f};
    authoritative[1].grabbed = true;
    authoritative[1].owner = 17U;
    const ri::runtime::SnapshotBlob target =
        ri::world::BuildInteractivePropSnapshot(authoritative, 41U);
    const std::optional<ri::runtime::SnapshotDeltaPacket> delta =
        ri::runtime::BuildSnapshotDelta(baseline, target);
    const std::optional<ri::runtime::SnapshotBlob> rebuilt = delta.has_value()
        ? ri::runtime::ApplySnapshotDelta(baseline, *delta)
        : std::nullopt;
    std::vector<ri::world::InteractivePropState> remote(2);
    remote[0].id = "fixed-pool-0";
    remote[1].id = "fixed-pool-1";
    std::string replicationError;
    ok &= Require(rebuilt.has_value()
                      && ri::world::ApplyInteractivePropSnapshot(
                          *rebuilt, remote, &replicationError),
                  "interactive prop state should use the existing snapshot delta lane");
    ok &= Require(remote[1].position.x == 2.5f && remote[1].owner == 17U
                      && remote[1].grabbed,
                  "PC and PC-VR hosts should decode identical authoritative prop state");
    if (rebuilt.has_value()) {
        ri::runtime::SnapshotBlob malformedSnapshot = *rebuilt;
        malformedSnapshot.bytes.push_back(0xFFU);
        const ri::math::Vec3 before = remote[1].position;
        ok &= Require(!ri::world::ApplyInteractivePropSnapshot(
                          malformedSnapshot, remote, &replicationError)
                          && remote[1].position.x == before.x,
                      "malformed network state must be rejected atomically");
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
