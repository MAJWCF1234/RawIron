#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Core/MainLoop.h"
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
    if (*v == "direct") return ri::runtime::RendezvousProviderKind::DirectToken;
    if (*v == "eos") return ri::runtime::RendezvousProviderKind::EpicOnlineServices;
    return ri::runtime::RendezvousProviderKind::None;
}

class DedicatedServerHost final : public ri::core::Host {
public:
    DedicatedServerHost() {
        ri::runtime::RuntimeIdentity identity{};
        identity.id = "rawiron-dedicated-server";
        identity.displayName = "RawIron Dedicated Server";
        identity.mode = "server";
        runtime_ = std::make_unique<ri::runtime::RuntimeCore>(std::move(identity), ri::runtime::DetectRuntimePaths());
    }

    [[nodiscard]] std::string_view GetName() const noexcept override { return "RawIron.DedicatedServer"; }
    [[nodiscard]] std::string_view GetMode() const noexcept override { return "server"; }

    void OnStartup(const ri::core::CommandLine& commandLine) override {
        ri::runtime::AuthoritativeNetConfig net{};
        net.mode = ri::runtime::NetMode::Dedicated;
        net.bindEndpoint.host = commandLine.GetValue("--host").value_or("0.0.0.0");
        net.bindEndpoint.port = static_cast<std::uint16_t>(commandLine.GetIntOr("--port", 27015));
        net.rendezvousProvider = ParseRendezvous(commandLine);
        net.issueJoinCodeOnStartup = commandLine.HasFlag("--issue-join-code");
        net.tickRate = commandLine.GetIntOr("--net-tick", 60);
        net.serverTickRate = commandLine.GetIntOr("--server-tick", 125);
        net.maxPeers = commandLine.GetIntOr("--max-peers", 128);
        net.dedicatedServerFirst = !commandLine.HasFlag("--allow-listen-first");
        net.enableP2PPlane = commandLine.HasFlag("--enable-p2p-side-plane");
        net.latencySimulation.enabled = commandLine.HasFlag("--sim-net");
        net.latencySimulation.baseDelayMs = commandLine.GetIntOr("--sim-delay-ms", 0);
        net.latencySimulation.jitterMs = commandLine.GetIntOr("--sim-jitter-ms", 0);
        net.latencySimulation.packetLossPercent = static_cast<float>(commandLine.GetIntOr("--sim-loss-pct", 0));

        runtime_->AddModule(std::make_unique<ri::runtime::AuthoritativeNetModule>(std::move(net)));
        runtime_->AddDefaultModules();
        if (!runtime_->Startup(commandLine)) {
            failed_ = true;
            ri::core::LogInfo("Dedicated server runtime startup failed.");
            return;
        }
        ri::core::LogInfo("Dedicated server online.");
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
        ri::core::LogInfo("Dedicated server shutdown complete.");
    }

private:
    std::unique_ptr<ri::runtime::RuntimeCore> runtime_{};
    bool failed_ = false;
};

} // namespace

int main(int argc, char** argv) {
    ri::core::InitializeCrashDiagnostics();
    try {
        ri::core::CommandLine commandLine(argc, argv);
        DedicatedServerHost host;

        ri::core::MainLoopOptions options{};
        options.maxFrames = commandLine.GetIntOr("--frames", 0);
        options.fixedDeltaSeconds = 1.0 / static_cast<double>(std::max(1, commandLine.GetIntOr("--tick-hz", 60)));
        options.paceToFixedDelta = !commandLine.HasFlag("--unpaced");
        options.verboseFrames = commandLine.HasFlag("--verbose-frames");

        return ri::core::RunMainLoop(host, commandLine, options);
    } catch (const std::exception&) {
        ri::core::LogCurrentExceptionWithStackTrace("DedicatedServer Failure");
        return 1;
    }
}
