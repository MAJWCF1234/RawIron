#pragma once

#include "RawIron/Trace/MovementController.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ri::trace {

struct WeaponTimingSpec {
    std::string id;
    int fireIntervalTicks = 6;
    int reloadTicks = 0;
    bool hitscan = true;
    float projectileSpeed = 0.0f;
    float maxRange = 4000.0f;
    float projectileRadius = 0.15f;
};

struct CompetitiveSimulationConfig {
    int serverTickRate = 125;
    int maxRewindTicks = 16;
    float playerHitRadius = 0.45f;
    std::vector<WeaponTimingSpec> weapons;
};

struct RewindPose {
    std::string entityId;
    ri::math::Vec3 position{};
    ri::math::Vec3 velocity{};
};

struct RewindFrame {
    std::uint32_t tick = 0;
    std::vector<RewindPose> entities;
};

struct WeaponFireRequest {
    std::string weaponId;
    std::string shooterId;
    std::uint32_t serverTick = 0;
    std::uint32_t clientShotTick = 0;
    ri::math::Vec3 origin{};
    ri::math::Vec3 directionUnit{};
};

struct WeaponFireResult {
    bool fired = false;
    bool rewound = false;
    bool wasHitscan = true;
    std::optional<std::string> hitEntityId{};
    /// Nearest blocking structural collider when a world trace was supplied. Entity hit tests are
    /// clipped just before this distance, preventing rewind shots from resolving through walls.
    std::optional<std::string> blockingWorldColliderId{};
    float blockingWorldDistance = 0.0f;
    std::uint32_t rewindTick = 0;
    int cooldownTicksRemaining = 0;
};

class TraceScene;

class CompetitiveWeaponSimulator {
public:
    explicit CompetitiveWeaponSimulator(CompetitiveSimulationConfig config);

    /// Resolves a lag-compensated weapon shot. When blockingWorldTrace is supplied, it is
    /// queried as a structural-only scene; build it from policy-filtered structural colliders so
    /// non-blocking/query/detail geometry cannot occlude combat.
    [[nodiscard]] WeaponFireResult TryFire(const WeaponFireRequest& request,
                                           const std::vector<RewindFrame>& history,
                                           const TraceScene* blockingWorldTrace = nullptr);
    void Tick();

private:
    struct WeaponRuntimeState {
        int cooldownTicks = 0;
    };

    [[nodiscard]] const WeaponTimingSpec* FindWeaponSpec(const std::string& id) const;

    CompetitiveSimulationConfig config_{};
    std::vector<std::pair<std::string, WeaponRuntimeState>> perWeaponState_{};
};

/// Reusable Quake-style movement defaults on top of `MovementControllerOptions`.
[[nodiscard]] MovementControllerOptions MakeQuakeCompetitiveMovementPreset();

/// Canonical high-tick competitive timings and baseline weapon specs.
[[nodiscard]] CompetitiveSimulationConfig MakeQuakeCompetitiveSimulationConfig();

/// Server-side rewind selection for lag-compensated hitscan checks.
[[nodiscard]] std::optional<RewindFrame> SelectRewindFrame(const std::vector<RewindFrame>& history,
                                                           std::uint32_t targetTick);

/// Very fast server-side hitscan test using rewound player capsules/spheres.
[[nodiscard]] std::optional<std::string> EvaluateRewoundHitscan(const RewindFrame& frame,
                                                                const ri::math::Vec3& rayOrigin,
                                                                const ri::math::Vec3& rayDirectionUnit,
                                                                float maxDistance,
                                                                float hitRadius);

[[nodiscard]] std::optional<std::string> EvaluateRewoundProjectile(const RewindFrame& frame,
                                                                   const ri::math::Vec3& projectileOrigin,
                                                                   const ri::math::Vec3& projectileVelocityPerTick,
                                                                   std::uint32_t ticksSinceFired,
                                                                   float projectileRadius,
                                                                   float targetHitRadius);

} // namespace ri::trace
