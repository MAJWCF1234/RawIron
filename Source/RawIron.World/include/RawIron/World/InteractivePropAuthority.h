#pragma once

#include "RawIron/Runtime/RuntimeNetcode.h"
#include "RawIron/World/InteractivePropField.h"
#include <functional>
#include <unordered_map>

namespace ri::world {

// Shared two-pool authority codec. The experience supplies pools/emission authoring;
// transport authentication, packet limits and session agreement stay in Runtime.
class InteractivePropAuthorityBridge : public ri::runtime::IAuthoritativeSimulationBridge {
public:
    using Emitter = std::function<InteractivePropEmissionResult(const ri::math::Vec3&, const ri::math::Vec3&)>;
    void SetPools(std::vector<InteractivePropState>* interaction,
        std::vector<InteractivePropState>* projectiles, Emitter emitter);
    std::optional<ri::runtime::SnapshotBlob> CaptureSnapshot(std::uint32_t tick) override;
    bool ApplySnapshot(const ri::runtime::SnapshotBlob& snapshot, std::string* error) override;
    bool HandleCommand(std::size_t peerId, std::uint32_t channel,
        std::span<const std::uint8_t> payload, std::string* error) override;
    static std::vector<std::uint8_t> BuildProjectileCommand(const ri::math::Vec3& origin,
        const ri::math::Vec3& direction);
private:
    struct PeerCommandBudget { std::uint32_t tick = 0; std::uint32_t accepted = 0; };
    std::vector<InteractivePropState>* interaction_ = nullptr;
    std::vector<InteractivePropState>* projectiles_ = nullptr;
    Emitter emitter_{};
    std::uint32_t authorityTick_ = 0;
    std::unordered_map<std::size_t, PeerCommandBudget> peerCommandBudgets_{};
};
} // namespace ri::world
