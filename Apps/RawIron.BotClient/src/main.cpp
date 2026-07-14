#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Core/MainLoop.h"
#include "RawIron/Runtime/BotClients.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Runtime/RuntimeNetcode.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <optional>
#include <string>

namespace {

ri::runtime::RendezvousProviderKind ParseRendezvous(const ri::core::CommandLine& commandLine) {
    const auto v = commandLine.GetValue("--rendezvous");
    if (!v.has_value()) {
        return ri::runtime::RendezvousProviderKind::EpicOnlineServices;
    }
    if (*v == "direct") {
        return ri::runtime::RendezvousProviderKind::DirectToken;
    }
    if (*v == "eos") {
        return ri::runtime::RendezvousProviderKind::EpicOnlineServices;
    }
    return ri::runtime::RendezvousProviderKind::None;
}

ri::runtime::NetMode ParseMode(const ri::core::CommandLine& commandLine) {
    const auto v = commandLine.GetValue("--net-mode");
    if (!v.has_value()) {
        return ri::runtime::NetMode::ClientOnly;
    }
    if (*v == "dedicated") return ri::runtime::NetMode::Dedicated;
    if (*v == "listen") return ri::runtime::NetMode::ListenHost;
    if (*v == "hybrid") return ri::runtime::NetMode::HybridP2P;
    if (*v == "client") return ri::runtime::NetMode::ClientOnly;
    return ri::runtime::NetMode::ClientOnly;
}

class BotHost final : public ri::core::Host {
public:
    BotHost() {
        ri::runtime::RuntimeIdentity identity{};
        identity.id = "rawiron-bot-client";
        identity.displayName = "RawIron Bot Client";
        identity.mode = "bot";
        runtime_ = std::make_unique<ri::runtime::RuntimeCore>(std::move(identity), ri::runtime::DetectRuntimePaths());
    }

    [[nodiscard]] std::string_view GetName() const noexcept override { return "RawIron.BotClient"; }
    [[nodiscard]] std::string_view GetMode() const noexcept override { return "bot"; }

    void OnStartup(const ri::core::CommandLine& commandLine) override {
        ri::runtime::AuthoritativeNetConfig net{};
        net.mode = ParseMode(commandLine);
        net.bindEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--host-port", 27015));
        net.p2pBindEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--p2p-port", 27115));
        net.connectEndpoint.host = commandLine.GetValue("--connect-host").value_or("127.0.0.1");
        net.connectEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--connect-port", 27015));
        net.rendezvousProvider = ParseRendezvous(commandLine);
        net.issueJoinCodeOnStartup = commandLine.HasFlag("--issue-join-code");
        if (const auto code = commandLine.GetValue("--join-code"); code.has_value()) {
            net.joinCodeToResolve = *code;
        }
        net.enableP2PPlane = (net.mode == ri::runtime::NetMode::HybridP2P);
        net.enabled = !commandLine.HasFlag("--offline");
        net.tickRate = commandLine.GetIntOr("--net-tick", 60);
        net.serverTickRate = commandLine.GetIntOr("--server-tick", 125);
        net.maxPeers = commandLine.GetIntOr("--max-peers", 128);
        net.dedicatedServerFirst = !commandLine.HasFlag("--allow-listen-first");
        net.latencySimulation.enabled = commandLine.HasFlag("--sim-net");
        net.latencySimulation.baseDelayMs = commandLine.GetIntOr("--sim-delay-ms", 0);
        net.latencySimulation.jitterMs = commandLine.GetIntOr("--sim-jitter-ms", 0);
        net.latencySimulation.packetLossPercent = static_cast<float>(commandLine.GetIntOr("--sim-loss-pct", 0));

        auto netModule = std::make_unique<ri::runtime::AuthoritativeNetModule>(net);
        netModule_ = netModule.get();
        runtime_->AddModule(std::move(netModule));

        ri::runtime::BotSwarmConfig swarm{};
        swarm.botCount = commandLine.GetIntOr("--bots", 16);
        swarm.commandChannel = commandLine.GetIntOr("--bot-channel", 0);
        swarm.reliableCommands = commandLine.HasFlag("--bot-reliable");
        runtime_->AddModule(std::make_unique<ri::runtime::BotSwarmModule>(netModule_, swarm));

        runtime_->AddDefaultModules();
        if (!runtime_->Startup(commandLine)) {
            failed_ = true;
            ri::core::LogInfo("Bot runtime startup failed.");
            return;
        }
        ri::core::LogInfo("Bot host online.");
    }

    [[nodiscard]] bool OnFrame(const ri::core::FrameContext& frame) override {
        if (failed_) {
            return false;
        }
        return runtime_->Frame(frame);
    }

    void OnShutdown() override {
        if (runtime_ != nullptr) {
            runtime_->Shutdown();
        }
        ri::core::LogInfo("Bot host shutdown complete.");
    }

private:
    std::unique_ptr<ri::runtime::RuntimeCore> runtime_{};
    ri::runtime::AuthoritativeNetModule* netModule_ = nullptr;
    bool failed_ = false;
};

} // namespace

int main(int argc, char** argv) {
    ri::core::InitializeCrashDiagnostics();
    try {
        ri::core::CommandLine commandLine(argc, argv);
        BotHost host;

        ri::core::MainLoopOptions options{};
        options.maxFrames = commandLine.GetIntOr("--frames", 1800);
        options.fixedDeltaSeconds = 1.0 / static_cast<double>(std::max(1, commandLine.GetIntOr("--tick-hz", 60)));
        options.paceToFixedDelta = !commandLine.HasFlag("--unpaced");
        options.verboseFrames = commandLine.HasFlag("--verbose-frames");

        return ri::core::RunMainLoop(host, commandLine, options);
    } catch (const std::exception&) {
        ri::core::LogCurrentExceptionWithStackTrace("BotClient Failure");
        return 1;
    }
}
