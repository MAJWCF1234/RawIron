#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxRuntime.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Games/GameConfigContracts.h"
#include "RawIron/Games/GameRuntimeCore.h"
#include "RawIron/Runtime/BotClients.h"
#include "RawIron/Runtime/RuntimeNetcode.h"

#include <algorithm>
#include <exception>
#include <filesystem>

namespace ri::games::multiplayersandbox {
namespace {

namespace fs = std::filesystem;

ri::runtime::RendezvousProviderKind ParseRendezvous(const std::string& v) {
    if (v == "direct") return ri::runtime::RendezvousProviderKind::DirectToken;
    if (v == "eos") return ri::runtime::RendezvousProviderKind::EpicOnlineServices;
    return ri::runtime::RendezvousProviderKind::None;
}

ri::runtime::NetMode ParseNetMode(const std::string& v) {
    if (v == "dedicated") return ri::runtime::NetMode::Dedicated;
    if (v == "listen") return ri::runtime::NetMode::ListenHost;
    if (v == "hybrid") return ri::runtime::NetMode::HybridP2P;
    return ri::runtime::NetMode::ClientOnly;
}

std::optional<ri::content::GameManifest> ResolveManifest(const StandaloneOptions& options) {
    if (!options.gameRoot.empty()) {
        return ri::content::LoadGameManifest(options.gameRoot / "manifest.json");
    }
    const fs::path workspace = options.workspaceRoot.empty()
        ? ri::content::DetectWorkspaceRoot(fs::current_path())
        : options.workspaceRoot;
    return ri::content::ResolveGameManifest(workspace, options.gameId);
}

} // namespace

bool RunStandalone(const StandaloneOptions& options,
                   const ri::core::CommandLine& commandLine,
                   std::string* error) {
    try {
        if (options.use3DStandalone) {
            return RunStandalone3D(options, commandLine, error);
        }

        const auto manifest = ResolveManifest(options);
        if (!manifest.has_value()) {
            if (error != nullptr) *error = "Unable to resolve manifest for " + options.gameId;
            return false;
        }
        const std::vector<std::string> issues = ri::content::ValidateGameProjectFormat(*manifest);
        if (!issues.empty()) {
            if (error != nullptr) {
                *error = "Game format validation failed:";
                for (const std::string& issue : issues) {
                    *error += " " + issue;
                }
            }
            return false;
        }
        if (!ri::games::EnforceGameConfigContracts(
                manifest->rootPath,
                ri::games::GameConfigContractOptions{.mode = ri::games::GameConfigContractMode::Balanced},
                error)) {
            return false;
        }

        auto manifestService = std::make_shared<ri::content::GameManifest>(*manifest);
        auto supportService = std::make_shared<ri::content::GameRuntimeSupportData>(
            ri::content::LoadGameRuntimeSupportData(manifest->rootPath));
        ri::games::LogGameRuntimeSupportSummary(*supportService);

        ri::runtime::RuntimeCore runtime = ri::games::CreateGameRuntimeCore(
            *manifest,
            "RawIron.Game.MultiplayerSandbox",
            ri::games::BuildGameRuntimePaths(*manifest, options.workspaceRoot),
            ri::games::GameRuntimeBootServices{.manifest = std::move(manifestService), .support = std::move(supportService)});

        ri::runtime::AuthoritativeNetConfig net{};
        net.enabled = !commandLine.HasFlag("--offline");
        net.mode = ParseNetMode(options.netMode);
        net.bindEndpoint.host = "0.0.0.0";
        net.bindEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--port", 27015));
        net.p2pBindEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--p2p-port", 27115));
        net.connectEndpoint.host = commandLine.GetValue("--connect-host").value_or("127.0.0.1");
        net.connectEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--connect-port", 27015));
        net.advertiseHost = commandLine.GetValue("--advertise-host").value_or("");
        net.rendezvousProvider = ParseRendezvous(options.rendezvous);
        net.issueJoinCodeOnStartup = options.issueJoinCode;
        if (options.joinCode.has_value()) {
            net.joinCodeToResolve = *options.joinCode;
        }
        net.enableP2PPlane = (net.mode == ri::runtime::NetMode::HybridP2P);
        net.tickRate = options.netTick;
        net.serverTickRate = options.serverTick;
        net.maxPeers = options.maxPeers;
        net.latencySimulation.enabled = options.simulateNet;
        net.latencySimulation.baseDelayMs = options.simDelayMs;
        net.latencySimulation.jitterMs = options.simJitterMs;
        net.latencySimulation.packetLossPercent = static_cast<float>(options.simLossPct);

        auto netModule = std::make_unique<ri::runtime::AuthoritativeNetModule>(net);
        ri::runtime::AuthoritativeNetModule* netPtr = netModule.get();
        runtime.AddModule(std::move(netModule));

        ri::runtime::BotSwarmConfig swarm{};
        swarm.botCount = options.bots;
        swarm.commandChannel = 0;
        swarm.reliableCommands = options.botsReliable;
        runtime.AddModule(std::make_unique<ri::runtime::BotSwarmModule>(netPtr, swarm));

        if (!ri::games::StartupGameRuntimeCore(runtime, commandLine, error)) {
            return false;
        }

        const int maxFrames = std::max(1, options.frames);
        const double dt = 1.0 / static_cast<double>(std::max(1, options.tickHz));
        for (int i = 0; i < maxFrames; ++i) {
            const double t = static_cast<double>(i) * dt;
            if (!runtime.Frame(ri::games::BuildGameRuntimeFrameContext(i, dt, t, t))) {
                break;
            }
            if (i > 0 && (i % std::max(1, options.tickHz)) == 0) {
                const auto stats = netPtr->ServerStats();
                ri::core::LogInfo(
                    "Sandbox net second=" + std::to_string(i / std::max(1, options.tickHz)) +
                    " snapshots=" + std::to_string(stats.snapshotsBroadcast) +
                    " inbound=" + std::to_string(stats.inboundPackets) +
                    " outbound=" + std::to_string(stats.outboundPackets));
            }
        }

        runtime.Shutdown();
        return true;
    } catch (const std::exception& ex) {
        if (error != nullptr) *error = ex.what();
        return false;
    }
}

} // namespace ri::games::multiplayersandbox
