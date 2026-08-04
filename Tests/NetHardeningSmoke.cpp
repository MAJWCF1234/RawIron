#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Runtime/LatencyTools.h"
#include "RawIron/Runtime/LevelScopedSchedulers.h"
#include "RawIron/Runtime/NetTransport.h"
#include "RawIron/Runtime/RendezvousProvider.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Runtime/RuntimeNetcode.h"
#include "RawIron/Runtime/SnapshotReplication.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

/// Always reports one connected peer and accepts every send.
class CountingTransport final : public ri::runtime::INetTransport {
public:
    static constexpr std::size_t kPeerId = 3U;

    bool StartServer(const ri::runtime::NetEndpoint&, std::size_t) override { return true; }
    bool StartClient() override { return true; }
    bool Connect(const ri::runtime::NetEndpoint&) override { return true; }
    void Shutdown() override {}

    bool Send(std::size_t, const ri::runtime::NetPacket& packet) override {
        stats_.packetsSent += 1U;
        stats_.bytesSent += packet.payload.size();
        return true;
    }

    std::vector<ri::runtime::NetPacket> PollReceive(std::size_t) override { return {}; }

    [[nodiscard]] std::vector<std::size_t> ConnectedPeers() const override {
        return std::vector<std::size_t>{kPeerId};
    }

    [[nodiscard]] ri::runtime::NetTransportStats Stats() const noexcept override { return stats_; }

private:
    ri::runtime::NetTransportStats stats_{};
};

ri::core::FrameContext Frame(const int index, const double delta, const double realtime) {
    return ri::core::FrameContext{
        .frameIndex = index,
        .deltaSeconds = delta,
        .elapsedSeconds = realtime,
        .realtimeSeconds = realtime,
        .realDeltaSeconds = delta,
    };
}

/// Peer ids are never reused, so baseline history must be released when peers leave.
bool PeerBaselinesAreReleased() {
    ri::runtime::SnapshotReplicator replicator{};
    const ri::runtime::SnapshotBlob blob{
        .tick = 1U,
        .bytes = std::vector<std::uint8_t>(256U, 7U),
    };
    for (std::size_t peer = 0U; peer < 64U; ++peer) {
        bool usedDelta = false;
        (void)replicator.BuildForPeer(peer, blob, usedDelta);
    }
    if (replicator.TrackedPeerCount() != 64U) {
        return false;
    }

    const std::vector<std::size_t> stillConnected{2U, 5U};
    replicator.RetainPeers(stillConnected);
    if (replicator.TrackedPeerCount() != 2U) {
        return false;
    }

    replicator.ForgetPeer(2U);
    if (replicator.TrackedPeerCount() != 1U) {
        return false;
    }

    replicator.RetainPeers({});
    return replicator.TrackedPeerCount() == 0U;
}

/// A peer flood must not grow the delay queue without bound.
bool LatencyQueueIsCapped() {
    ri::runtime::LatencySimulator simulator(1234U);
    ri::runtime::LatencySimulationConfig config{};
    config.enabled = false;
    simulator.Configure(config);

    ri::runtime::NetPacket packet{};
    packet.payload.assign(8U, 0U);

    constexpr std::size_t kOverflow = 256U;
    std::size_t accepted = 0U;
    for (std::size_t i = 0U; i < ri::runtime::LatencySimulator::kMaxQueuedPackets + kOverflow; ++i) {
        if (simulator.Enqueue(1000U, packet)) {
            ++accepted;
        }
    }
    return accepted == ri::runtime::LatencySimulator::kMaxQueuedPackets &&
           simulator.DroppedPackets() == kOverflow;
}

/// Join codes must never advertise a bind-any or loopback address.
bool AdvertiseEndpointIsReachable() {
    const ri::runtime::NetEndpoint bindAny{.host = "0.0.0.0", .port = 27015U};
    const ri::runtime::NetEndpoint resolved = ri::runtime::ResolveAdvertiseEndpoint(bindAny);
    if (resolved.port != 27015U || resolved.host.empty() || resolved.host == "0.0.0.0") {
        return false;
    }

    const ri::runtime::NetEndpoint overridden = ri::runtime::ResolveAdvertiseEndpoint(bindAny, "example.internal");
    if (overridden.host != "example.internal") {
        return false;
    }

    // Launchers forward --advertise-host from a positional argument, so an unreachable override is
    // easy to pass by accident. It must be ignored, leaving the same address detection the
    // no-override path uses, instead of being baked verbatim into the join code.
    for (const char* unreachable : {"0.0.0.0", "127.0.0.1", "localhost", "::"}) {
        const ri::runtime::NetEndpoint bad = ri::runtime::ResolveAdvertiseEndpoint(bindAny, unreachable);
        if (bad.port != resolved.port || bad.host != resolved.host) {
            return false;
        }
    }

    const ri::runtime::NetEndpoint lan{.host = "192.168.7.21", .port = 27016U};
    if (ri::runtime::ResolveAdvertiseEndpoint(lan).host != "192.168.7.21") {
        return false;
    }

    return ri::runtime::IsUnreachableAdvertiseHost("::") && ri::runtime::IsUnreachableAdvertiseHost("localhost") &&
           !ri::runtime::IsUnreachableAdvertiseHost("10.0.0.5");
}

/// A frame hitch must not burst snapshots, and a non-finite delta must not
/// permanently poison the cadence accumulator.
bool SnapshotCadenceIsBounded() {
    ri::runtime::AuthoritativeNetConfig config{};
    config.mode = ri::runtime::NetMode::Dedicated;
    config.serverTickRate = 30;
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;

    ri::runtime::AuthoritativeNetModule server(config, std::make_unique<CountingTransport>());
    ri::runtime::RuntimeContext context({}, {});
    char executable[] = "NetHardeningSmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (!server.OnRuntimeStartup(context, commandLine)) {
        return false;
    }

    if (!server.OnRuntimeFrame(context, Frame(1, 100.0, 100.0))) {
        return false;
    }
    const std::uint64_t afterHitch = server.ServerStats().snapshotsBroadcast;
    if (afterHitch == 0U || afterHitch > 4U) {
        return false;
    }

    if (!server.OnRuntimeFrame(context, Frame(2, std::numeric_limits<double>::quiet_NaN(), 101.0))) {
        return false;
    }
    const std::uint64_t afterNonFinite = server.ServerStats().snapshotsBroadcast;

    for (int i = 0; i < 30; ++i) {
        if (!server.OnRuntimeFrame(context, Frame(3 + i, 1.0 / 30.0, 102.0 + (static_cast<double>(i) / 30.0)))) {
            return false;
        }
    }
    const bool recovered = server.ServerStats().snapshotsBroadcast > afterNonFinite;

    server.OnRuntimeShutdown(context);
    return recovered;
}

/// Ports outside the uint16 range must be rejected, not silently truncated.
bool JoinCodePortsAreValidated() {
    const std::unique_ptr<ri::runtime::IRendezvousProvider> provider =
        ri::runtime::CreateRendezvousProvider(ri::runtime::RendezvousProviderKind::DirectToken);
    if (provider == nullptr) {
        return false;
    }

    const auto valid = provider->ResolveJoinCode("RI1:192.168.1.40:27015:1");
    if (!valid.has_value() || valid->endpoint.port != 27015U || valid->endpoint.host != "192.168.1.40") {
        return false;
    }

    // 70000 truncates to 4464 in uint16; it must be refused instead.
    if (provider->ResolveJoinCode("RI1:192.168.1.40:70000:1").has_value()) {
        return false;
    }
    if (provider->ResolveJoinCode("RI1:192.168.1.40:0:1").has_value()) {
        return false;
    }
    if (provider->ResolveJoinCode("RI1::27015:1").has_value()) {
        return false;
    }
    if (provider->ResolveJoinCode("RI1:192.168.1.40:27015:9").has_value()) {
        return false;
    }
    // Trailing garbage after a valid mode must fail closed (stoul alone would accept "1extra").
    return !provider->ResolveJoinCode("RI1:192.168.1.40:27015:1extra").has_value();
}

/// A module that starts successfully must be shut down when a later module fails.
class RecordingModule final : public ri::runtime::RuntimeModule {
public:
    RecordingModule(std::string name, const bool startupResult, bool& shutdownFlag)
        : name_(std::move(name)), startupResult_(startupResult), shutdownFlag_(shutdownFlag) {}

    [[nodiscard]] std::string_view Name() const noexcept override { return name_; }

    bool OnRuntimeStartup(ri::runtime::RuntimeContext&, const ri::core::CommandLine&) override {
        return startupResult_;
    }

    void OnRuntimeShutdown(ri::runtime::RuntimeContext&) override { shutdownFlag_ = true; }

private:
    std::string name_;
    bool startupResult_ = true;
    bool& shutdownFlag_;
};

bool FailedStartupRollsBackStartedModules() {
    bool firstWasShutDown = false;
    bool failingWasShutDown = false;

    ri::runtime::RuntimeCore core(ri::runtime::RuntimeIdentity{.id = "rollback", .displayName = "Rollback"});
    core.AddModule(std::make_unique<RecordingModule>("first", true, firstWasShutDown));
    core.AddModule(std::make_unique<RecordingModule>("failing", false, failingWasShutDown));

    char executable[] = "NetHardeningSmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (core.Startup(commandLine)) {
        return false;
    }
    return firstWasShutDown;
}

/// Compensating drift must not replay an unbounded number of missed periods.
bool IntervalCatchUpIsBounded() {
    ri::runtime::LevelScopedIntervalScheduler scheduler;
    int fires = 0;
    const std::uint64_t token = scheduler.ScheduleEvery(
        1.0 / 60.0, [&fires]() { ++fires; }, 0.0,
        ri::runtime::LevelScopedIntervalScheduler::DriftPolicy::Compensate);
    if (token == 0U) {
        return false;
    }

    // An hour of missed 60 Hz periods would be ~216000 callbacks without a cap.
    scheduler.Tick(3600.0, 3600.0);
    if (fires == 0 || fires > 16) {
        return false;
    }

    // The entry must remain usable afterwards rather than being stuck in the past.
    const int afterHitch = fires;
    scheduler.Tick(3600.0 + (1.0 / 60.0) + 1e-6, 1.0 / 60.0);
    return fires > afterHitch && scheduler.ActiveCount() == 1U;
}

} // namespace

int main() {
    if (!PeerBaselinesAreReleased()) {
        return EXIT_FAILURE;
    }
    if (!LatencyQueueIsCapped()) {
        return EXIT_FAILURE;
    }
    if (!AdvertiseEndpointIsReachable()) {
        return EXIT_FAILURE;
    }
    if (!SnapshotCadenceIsBounded()) {
        return EXIT_FAILURE;
    }
    if (!JoinCodePortsAreValidated()) {
        return EXIT_FAILURE;
    }
    if (!FailedStartupRollsBackStartedModules()) {
        return EXIT_FAILURE;
    }
    if (!IntervalCatchUpIsBounded()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
