#include "RawIron/Games/CubeTest/CubeTestAuthority.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Content/GameScriptBundle.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Games/GameNetworkTuning.h"
#include "RawIron/Games/GameConfigContracts.h"
#include "RawIron/Core/SessionExtensions.h"
#include <algorithm>
#include <stdexcept>

namespace ri::games::cubetest {
namespace {
ri::runtime::NetMode ParseNetworkMode(const std::string_view value, bool& enabled) {
    enabled = true;
    if (value == "listen") return ri::runtime::NetMode::ListenHost;
    if (value == "dedicated") return ri::runtime::NetMode::Dedicated;
    if (value == "client") return ri::runtime::NetMode::ClientOnly;
    enabled = false;
    return ri::runtime::NetMode::ClientOnly;
}

} // namespace

CubeTestAuthorityBridge::CubeTestAuthorityBridge(CubeTestWorld* world) { SetWorld(world); }

void CubeTestAuthorityBridge::SetWorld(CubeTestWorld* world) {
    SetPools(world ? &world->interactionProps : nullptr, world ? &world->projectileProps : nullptr,
        world ? Emitter{[world](const ri::math::Vec3& origin, const ri::math::Vec3& direction) {
            return EmitCubeTestProjectile(*world, origin, direction);
        }} : Emitter{});
}
ri::runtime::AuthoritativeNetConfig BuildCubeTestAuthorityConfig(
    const ri::core::CommandLine& commandLine,
    std::shared_ptr<CubeTestAuthorityBridge> bridge,
    const std::filesystem::path& gameRoot) {
    ri::runtime::AuthoritativeNetConfig config{};
    bool enabled = false;
    config.mode = ParseNetworkMode(commandLine.GetValue("--net-mode").value_or("offline"), enabled);
    config.enabled = enabled && !commandLine.HasFlag("--offline");
    config.bindEndpoint.host = commandLine.GetValue("--bind-host").value_or("0.0.0.0");
    config.bindEndpoint.port = static_cast<std::uint16_t>(
        std::clamp(commandLine.GetIntOr("--port", 27015), 1, 65535));
    config.connectEndpoint.host = commandLine.GetValue("--connect-host").value_or("127.0.0.1");
    config.connectEndpoint.port = static_cast<std::uint16_t>(
        std::clamp(commandLine.GetIntOr("--connect-port", 27015), 1, 65535));
    config.serverTickRate = std::clamp(commandLine.GetIntOr("--server-tick", 60), 20, 125);
    config.tickRate = config.serverTickRate;
    config.maxPeers = std::clamp(commandLine.GetIntOr("--max-peers", 8), 1, 32);
    std::filesystem::path resolvedGameRoot = gameRoot;
    if (resolvedGameRoot.empty()) {
        const auto workspace = commandLine.GetValue("--workspace-root");
        const auto manifest = ri::content::ResolveGameManifest(
            workspace ? std::filesystem::path(*workspace)
                      : ri::content::DetectWorkspaceRoot(std::filesystem::current_path()), "cube-test");
        if (manifest) resolvedGameRoot = manifest->rootPath;
    }
    if (!resolvedGameRoot.empty()) {
        if (gameRoot.empty()) {
            std::string error;
            if (!ri::games::EnforceGameConfigContracts(resolvedGameRoot,
                    {.mode=ri::games::GameConfigContractMode::Balanced, .networkTuningMounted=true}, &error)) {
                throw std::runtime_error(error);
            }
        }
        ri::games::ApplyGameNetworkTuning(config,
            ri::content::LoadGameScriptBundle(resolvedGameRoot, {.logMissing = false}).network, commandLine);
    }
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    config.requireSessionExtensionAgreement = config.enabled;
    config.simulationBridge = std::move(bridge);
    config.sessionExtensionContract.extensions.push_back({
        .id = "rawiron.cubetest.authority",
        .version = "1.0.0",
        .fingerprint = "fnv1a64:cubetest-props-v1",
        .kind = ri::core::SessionExtensionKind::Gameplay,
        .reloadPolicy = ri::core::SessionExtensionReloadPolicy::SessionRestart,
        .capabilities = {"physics.projectiles", "physics.props", "xr.shared-authority"},
    });
    static_cast<void>(ri::core::NormalizeSessionExtensionContract(config.sessionExtensionContract));
    return config;
}

} // namespace ri::games::cubetest
