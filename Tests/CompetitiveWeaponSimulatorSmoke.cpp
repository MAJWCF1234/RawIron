#include "RawIron/Trace/CompetitiveModel.h"
#include "RawIron/Trace/TraceScene.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace {

ri::trace::CompetitiveSimulationConfig MakeTestConfig() {
    ri::trace::CompetitiveSimulationConfig config{};
    config.maxRewindTicks = 8;
    config.playerHitRadius = 0.5f;
    config.weapons = {
        {
            .id = "rail",
            .fireIntervalTicks = 1,
            .reloadTicks = 0,
            .hitscan = true,
            .projectileSpeed = 0.0f,
            .maxRange = 100.0f,
            .projectileRadius = 0.0f,
        },
        {
            .id = "rocket",
            .fireIntervalTicks = 1,
            .reloadTicks = 0,
            .hitscan = false,
            .projectileSpeed = 3.0f,
            .maxRange = 100.0f,
            .projectileRadius = 0.2f,
        },
    };
    return config;
}

ri::trace::WeaponFireRequest MakeRequest(const std::string& weaponId,
                                         const std::uint32_t serverTick,
                                         const std::uint32_t clientShotTick) {
    return {
        .weaponId = weaponId,
        .shooterId = "shooter",
        .serverTick = serverTick,
        .clientShotTick = clientShotTick,
        // Deliberately non-unit: hit resolution must normalize public input.
        .origin = {0.0f, 0.0f, 0.0f},
        .directionUnit = {0.0f, 0.0f, 4.0f},
    };
}

} // namespace

int main() {
    const std::vector<ri::trace::RewindFrame> history{
        {
            .tick = 20,
            .entities = {{.entityId = "target", .position = {0.0f, 0.0f, 10.0f}}},
        },
    };

    ri::trace::CompetitiveWeaponSimulator unobstructed{MakeTestConfig()};
    const ri::trace::WeaponFireResult unobstructedRail =
        unobstructed.TryFire(MakeRequest("rail", 20, 20), history);
    if (!unobstructedRail.fired
        || !unobstructedRail.rewound
        || !unobstructedRail.wasHitscan
        || unobstructedRail.hitEntityId != std::optional<std::string>{"target"}
        || unobstructedRail.blockingWorldColliderId.has_value()) {
        return EXIT_FAILURE;
    }

    ri::trace::TraceScene worldTrace{
        {
            {
                .id = "non_structural_fx",
                .bounds = {{-2.0f, -2.0f, 2.0f}, {2.0f, 2.0f, 3.0f}},
                .structural = false,
            },
            {
                .id = "structural_wall",
                .bounds = {{-2.0f, -2.0f, 4.0f}, {2.0f, 2.0f, 5.0f}},
                .structural = true,
                .dynamic = true,
            },
        }};

    ri::trace::CompetitiveWeaponSimulator blocked{MakeTestConfig()};
    const ri::trace::WeaponFireResult blockedRail =
        blocked.TryFire(MakeRequest("rail", 20, 20), history, &worldTrace);
    if (!blockedRail.fired
        || !blockedRail.rewound
        || blockedRail.hitEntityId.has_value()
        || blockedRail.blockingWorldColliderId != std::optional<std::string>{"structural_wall"}
        || blockedRail.blockingWorldDistance <= 0.0f
        || blockedRail.blockingWorldDistance >= 10.0f) {
        return EXIT_FAILURE;
    }

    ri::trace::CompetitiveWeaponSimulator blockedProjectile{MakeTestConfig()};
    const std::vector<ri::trace::RewindFrame> projectileHistory{
        {
            .tick = 20,
            .entities = {{.entityId = "target", .position = {0.0f, 0.0f, 6.0f}}},
        },
    };
    const ri::trace::WeaponFireResult rocket =
        blockedProjectile.TryFire(MakeRequest("rocket", 22, 20), projectileHistory, &worldTrace);
    if (!rocket.fired
        || rocket.wasHitscan
        || rocket.hitEntityId.has_value()
        || rocket.blockingWorldColliderId != std::optional<std::string>{"structural_wall"}) {
        return EXIT_FAILURE;
    }
    if (worldTrace.Metrics().traceRayQueries != 2) {
        return EXIT_FAILURE;
    }

    ri::trace::CompetitiveWeaponSimulator invalidAim{MakeTestConfig()};
    ri::trace::WeaponFireRequest invalidRequest = MakeRequest("rail", 20, 20);
    invalidRequest.directionUnit = {};
    if (invalidAim.TryFire(invalidRequest, history).fired) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
