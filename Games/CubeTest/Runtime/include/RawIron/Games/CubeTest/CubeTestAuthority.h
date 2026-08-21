#pragma once

#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Runtime/RuntimeNetcode.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace ri::games::cubetest {

/// Shared desktop/XR authority adapter for Cube Test's dynamic physics pools. Static authored
/// primitives stay in the package contract; only the bounded dynamic state crosses the network.
class CubeTestAuthorityBridge final : public ri::runtime::IAuthoritativeSimulationBridge {
public:
    explicit CubeTestAuthorityBridge(CubeTestWorld* world = nullptr) noexcept;

    void SetWorld(CubeTestWorld* world) noexcept;

    [[nodiscard]] std::optional<ri::runtime::SnapshotBlob> CaptureSnapshot(std::uint32_t tick) override;
    bool ApplySnapshot(const ri::runtime::SnapshotBlob& snapshot, std::string* error) override;
    bool HandleCommand(std::size_t peerId,
                       std::uint32_t channel,
                       std::span<const std::uint8_t> payload,
                       std::string* error) override;

    /// Serializes a bounded, validated projectile request for an authority-plane packet.
    [[nodiscard]] static std::vector<std::uint8_t> BuildProjectileCommand(
        const ri::math::Vec3& origin,
        const ri::math::Vec3& direction);

private:
    struct PeerCommandBudget {
        std::uint32_t tick = 0;
        std::uint32_t accepted = 0;
    };

    CubeTestWorld* world_ = nullptr;
    std::uint32_t authorityTick_ = 0;
    std::unordered_map<std::size_t, PeerCommandBudget> peerCommandBudgets_{};
};

[[nodiscard]] ri::runtime::AuthoritativeNetConfig BuildCubeTestAuthorityConfig(
    const ri::core::CommandLine& commandLine,
    std::shared_ptr<CubeTestAuthorityBridge> bridge);

} // namespace ri::games::cubetest
