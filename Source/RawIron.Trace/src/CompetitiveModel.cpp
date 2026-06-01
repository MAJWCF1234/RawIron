#include "RawIron/Trace/CompetitiveModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ri::trace {
namespace {

ri::math::Vec3 Sub(const ri::math::Vec3& a, const ri::math::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float LenSq(const ri::math::Vec3& v) {
    return ri::math::Dot(v, v);
}

ri::math::Vec3 Scale(const ri::math::Vec3& v, const float s) {
    return {v.x * s, v.y * s, v.z * s};
}

ri::math::Vec3 Add(const ri::math::Vec3& a, const ri::math::Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

} // namespace

MovementControllerOptions MakeQuakeCompetitiveMovementPreset() {
    MovementControllerOptions o{};
    o.maxGroundSpeed = 9.2f;
    o.maxSprintGroundSpeed = 9.2f;
    o.maxAirSpeed = 16.0f;
    o.groundAcceleration = 85.0f;
    o.airAcceleration = 26.0f;
    o.groundFriction = 6.0f;
    o.stopSpeed = 2.0f;
    o.jumpSpeed = 8.6f;
    o.gravity = 25.0f;
    o.airControl = 0.35f;
    o.airTurnResponsiveness = 0.55f;
    o.airStrafeAccelerationBoost = 1.5f;
    o.fallGravityMultiplier = 1.25f;
    o.maxFallSpeed = 32.0f;
    o.simulateStamina = false;
    o.coyoteTimeSeconds = 0.08f;
    o.jumpBufferTimeSeconds = 0.10f;
    o.lowJumpGravityMultiplier = 1.12f;
    o.groundProbeJumpMaxDown = 0.24f;
    o.groundAdhesionSpeed = 1.8f;
    o.enableWallJump = false;
    return o;
}

CompetitiveSimulationConfig MakeQuakeCompetitiveSimulationConfig() {
    CompetitiveSimulationConfig c{};
    c.serverTickRate = 125;
    c.maxRewindTicks = 16;
    c.playerHitRadius = 0.45f;
    c.weapons = {
        WeaponTimingSpec{.id = "rail", .fireIntervalTicks = 100, .reloadTicks = 0, .hitscan = true, .projectileSpeed = 0.0f, .maxRange = 6000.0f, .projectileRadius = 0.0f},
        WeaponTimingSpec{.id = "shotgun", .fireIntervalTicks = 50, .reloadTicks = 0, .hitscan = true, .projectileSpeed = 0.0f, .maxRange = 1800.0f, .projectileRadius = 0.0f},
        WeaponTimingSpec{.id = "rocket", .fireIntervalTicks = 100, .reloadTicks = 0, .hitscan = false, .projectileSpeed = 38.0f, .maxRange = 3000.0f, .projectileRadius = 0.20f},
    };
    return c;
}

std::optional<RewindFrame> SelectRewindFrame(const std::vector<RewindFrame>& history, const std::uint32_t targetTick) {
    if (history.empty()) {
        return std::nullopt;
    }
    std::size_t best = 0;
    std::uint32_t bestDist = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t i = 0; i < history.size(); ++i) {
        const std::uint32_t t = history[i].tick;
        const std::uint32_t d = (t > targetTick) ? (t - targetTick) : (targetTick - t);
        if (d < bestDist) {
            best = i;
            bestDist = d;
        }
    }
    return history[best];
}

std::optional<std::string> EvaluateRewoundHitscan(const RewindFrame& frame,
                                                  const ri::math::Vec3& rayOrigin,
                                                  const ri::math::Vec3& rayDirectionUnit,
                                                  const float maxDistance,
                                                  const float hitRadius) {
    std::optional<std::string> winner{};
    float bestT = maxDistance;

    for (const RewindPose& pose : frame.entities) {
        const ri::math::Vec3 to = Sub(pose.position, rayOrigin);
        const float t = ri::math::Dot(to, rayDirectionUnit);
        if (t < 0.0f || t > bestT) {
            continue;
        }
        const ri::math::Vec3 closest{
            rayOrigin.x + rayDirectionUnit.x * t,
            rayOrigin.y + rayDirectionUnit.y * t,
            rayOrigin.z + rayDirectionUnit.z * t,
        };
        const float d2 = LenSq(Sub(pose.position, closest));
        if (d2 <= (hitRadius * hitRadius)) {
            bestT = t;
            winner = pose.entityId;
        }
    }
    return winner;
}

std::optional<std::string> EvaluateRewoundProjectile(const RewindFrame& frame,
                                                     const ri::math::Vec3& projectileOrigin,
                                                     const ri::math::Vec3& projectileVelocityPerTick,
                                                     const std::uint32_t ticksSinceFired,
                                                     const float projectileRadius,
                                                     const float targetHitRadius) {
    const ri::math::Vec3 projectilePos = Add(projectileOrigin, Scale(projectileVelocityPerTick, static_cast<float>(ticksSinceFired)));
    const float r = std::max(0.0f, projectileRadius) + std::max(0.0f, targetHitRadius);
    const float r2 = r * r;
    for (const RewindPose& pose : frame.entities) {
        if (LenSq(Sub(pose.position, projectilePos)) <= r2) {
            return pose.entityId;
        }
    }
    return std::nullopt;
}

CompetitiveWeaponSimulator::CompetitiveWeaponSimulator(CompetitiveSimulationConfig config)
    : config_(std::move(config)) {}

const WeaponTimingSpec* CompetitiveWeaponSimulator::FindWeaponSpec(const std::string& id) const {
    const auto it = std::find_if(config_.weapons.begin(), config_.weapons.end(),
                                 [&id](const WeaponTimingSpec& spec) { return spec.id == id; });
    return (it == config_.weapons.end()) ? nullptr : &(*it);
}

WeaponFireResult CompetitiveWeaponSimulator::TryFire(const WeaponFireRequest& request,
                                                     const std::vector<RewindFrame>& history) {
    WeaponFireResult out{};
    const WeaponTimingSpec* spec = FindWeaponSpec(request.weaponId);
    if (spec == nullptr) {
        return out;
    }

    auto stateIt = std::find_if(perWeaponState_.begin(), perWeaponState_.end(),
                                [&request](const auto& p) { return p.first == request.weaponId; });
    if (stateIt == perWeaponState_.end()) {
        perWeaponState_.push_back({request.weaponId, WeaponRuntimeState{}});
        stateIt = std::prev(perWeaponState_.end());
    }
    if (stateIt->second.cooldownTicks > 0) {
        out.cooldownTicksRemaining = stateIt->second.cooldownTicks;
        return out;
    }

    stateIt->second.cooldownTicks = std::max(1, spec->fireIntervalTicks) + std::max(0, spec->reloadTicks);
    out.cooldownTicksRemaining = stateIt->second.cooldownTicks;
    out.fired = true;
    out.wasHitscan = spec->hitscan;

    std::uint32_t rewindTick = request.clientShotTick;
    if (request.serverTick > request.clientShotTick) {
        const std::uint32_t maxLag = static_cast<std::uint32_t>(std::max(1, config_.maxRewindTicks));
        const std::uint32_t lag = request.serverTick - request.clientShotTick;
        rewindTick = request.serverTick - std::min(maxLag, lag);
    }
    out.rewindTick = rewindTick;

    const auto rewound = SelectRewindFrame(history, rewindTick);
    if (!rewound.has_value()) {
        return out;
    }
    out.rewound = true;

    if (spec->hitscan) {
        out.hitEntityId = EvaluateRewoundHitscan(*rewound,
                                                 request.origin,
                                                 request.directionUnit,
                                                 spec->maxRange,
                                                 config_.playerHitRadius);
    } else {
        const std::uint32_t dt = request.serverTick > request.clientShotTick
            ? request.serverTick - request.clientShotTick
            : 0U;
        out.hitEntityId = EvaluateRewoundProjectile(*rewound,
                                                    request.origin,
                                                    Scale(request.directionUnit, spec->projectileSpeed),
                                                    dt,
                                                    spec->projectileRadius,
                                                    config_.playerHitRadius);
    }

    return out;
}

void CompetitiveWeaponSimulator::Tick() {
    for (auto& [_, state] : perWeaponState_) {
        if (state.cooldownTicks > 0) {
            --state.cooldownTicks;
        }
    }
}

} // namespace ri::trace
