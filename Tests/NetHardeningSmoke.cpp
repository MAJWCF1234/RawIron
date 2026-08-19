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
        replicator.RememberPeerBaseline(peer, blob);
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

/// Oversized protocol offenders enter an escalating soft cooldown that drops further
/// inbound packets without treating cooldown drops as new strikes.
bool OffenseCooldownsEscalate() {
    class OffenseTransport final : public ri::runtime::INetTransport {
    public:
        bool StartServer(const ri::runtime::NetEndpoint&, std::size_t) override { return true; }
        bool StartClient() override { return true; }
        bool Connect(const ri::runtime::NetEndpoint&) override { return true; }
        void Shutdown() override {}
        bool Send(std::size_t, const ri::runtime::NetPacket&) override { return true; }
        std::vector<ri::runtime::NetPacket> PollReceive(std::size_t) override {
            std::vector<ri::runtime::NetPacket> out;
            out.swap(inbox);
            return out;
        }
        [[nodiscard]] std::vector<std::size_t> ConnectedPeers() const override { return {7U}; }
        [[nodiscard]] ri::runtime::NetTransportStats Stats() const noexcept override { return {}; }
        std::vector<ri::runtime::NetPacket> inbox;
    };

    auto* transport = new OffenseTransport();
    ri::runtime::AuthoritativeNetConfig config{};
    config.mode = ri::runtime::NetMode::Dedicated;
    config.serverTickRate = 30;
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    ri::runtime::AuthoritativeNetModule server(config, std::unique_ptr<ri::runtime::INetTransport>(transport));
    ri::runtime::RuntimeContext context({}, {});
    char executable[] = "NetHardeningSmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (!server.OnRuntimeStartup(context, commandLine)) {
        return false;
    }

    std::uint64_t cooldownEvents = 0U;
    std::uint64_t cooldownDrops = 0U;
    context.Events().On("net.peer.cooldown", [&](const ri::runtime::RuntimeEvent&) { ++cooldownEvents; });
    context.Events().On("net.packet.ignored", [&](const ri::runtime::RuntimeEvent& event) {
        const auto it = event.fields.find("reason");
        if (it != event.fields.end() && it->second == "peer_cooldown") {
            ++cooldownDrops;
        }
    });

    ri::runtime::NetPacket oversized{};
    oversized.peerId = 7U;
    oversized.payload.resize(ri::runtime::kMaxNetPacketPayloadBytes + 1U);
    transport->inbox.push_back(oversized);
    if (!server.OnRuntimeFrame(context, Frame(1, 0.0, 1.0)) || cooldownEvents != 1U) {
        server.OnRuntimeShutdown(context);
        return false;
    }

    // Same millisecond: peer is cooling down, so this must drop without another strike event.
    transport->inbox.push_back(oversized);
    if (!server.OnRuntimeFrame(context, Frame(2, 0.0, 1.0))
        || cooldownEvents != 1U
        || cooldownDrops == 0U
        || server.ServerStats().inboundPacketsDroppedByPeerCooldown == 0U) {
        server.OnRuntimeShutdown(context);
        return false;
    }

    // After the first cooldown window (250 ms), another offense must escalate.
    transport->inbox.push_back(oversized);
    if (!server.OnRuntimeFrame(context, Frame(3, 0.0, 1.3)) || cooldownEvents != 2U) {
        server.OnRuntimeShutdown(context);
        return false;
    }

    server.OnRuntimeShutdown(context);
    return true;
}

/// Clients must not treat the authority as a protocol offender. Malformed snapshots
/// request a full resync instead of muting the host.
bool ClientDoesNotMuteAuthority() {
    class ClientTransport final : public ri::runtime::INetTransport {
    public:
        bool StartServer(const ri::runtime::NetEndpoint&, std::size_t) override { return true; }
        bool StartClient() override { return true; }
        bool Connect(const ri::runtime::NetEndpoint&) override { return true; }
        void Shutdown() override {}
        bool Send(std::size_t, const ri::runtime::NetPacket& packet) override {
            sent.push_back(packet);
            return true;
        }
        std::vector<ri::runtime::NetPacket> PollReceive(std::size_t) override {
            std::vector<ri::runtime::NetPacket> out;
            out.swap(inbox);
            return out;
        }
        [[nodiscard]] std::vector<std::size_t> ConnectedPeers() const override { return {7U}; }
        [[nodiscard]] ri::runtime::NetTransportStats Stats() const noexcept override { return {}; }
        std::vector<ri::runtime::NetPacket> inbox;
        std::vector<ri::runtime::NetPacket> sent;
    };

    auto* transport = new ClientTransport();
    ri::runtime::AuthoritativeNetConfig config{};
    config.mode = ri::runtime::NetMode::ClientOnly;
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    ri::runtime::AuthoritativeNetModule client(config, std::unique_ptr<ri::runtime::INetTransport>(transport));
    ri::runtime::RuntimeContext context({}, {});
    char executable[] = "NetHardeningSmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (!client.OnRuntimeStartup(context, commandLine)) {
        return false;
    }

    std::uint64_t cooldownEvents = 0U;
    std::uint64_t rejectedEvents = 0U;
    context.Events().On("net.peer.cooldown", [&](const ri::runtime::RuntimeEvent&) { ++cooldownEvents; });
    context.Events().On("net.snapshot.rejected", [&](const ri::runtime::RuntimeEvent& event) {
        const auto it = event.fields.find("resync");
        if (it != event.fields.end() && it->second == "full") {
            ++rejectedEvents;
        }
    });

    ri::runtime::NetPacket malformed{};
    malformed.peerId = 7U;
    malformed.payload = {0x00U, 0x01U};
    transport->inbox.push_back(malformed);
    if (!client.OnRuntimeFrame(context, Frame(1, 0.0, 1.0))
        || cooldownEvents != 0U
        || client.ServerStats().inboundPacketsDroppedByPeerCooldown != 0U
        || rejectedEvents != 1U
        || transport->sent.size() != 1U
        || transport->sent[0].payload.size() != 2U
        || transport->sent[0].payload[0] != 0xA8U
        || transport->sent[0].payload[1] != 0x01U) {
        client.OnRuntimeShutdown(context);
        return false;
    }

    // A second malformed envelope in the same millisecond must still be handled, not dropped
    // as peer_cooldown (the authority is not an offender).
    transport->inbox.push_back(malformed);
    if (!client.OnRuntimeFrame(context, Frame(2, 0.0, 1.0))
        || cooldownEvents != 0U
        || rejectedEvents != 2U
        || transport->sent.size() != 2U) {
        client.OnRuntimeShutdown(context);
        return false;
    }

    client.OnRuntimeShutdown(context);
    return true;
}

void AppendU32Le(std::vector<std::uint8_t>& out, const std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

std::vector<std::uint8_t> EncodeAuthPosition(const std::uint32_t tick, const float x) {
    const std::int32_t qx = static_cast<std::int32_t>(x * 1000.0f);
    std::vector<std::uint8_t> out(8U, 0U);
    out[0] = static_cast<std::uint8_t>(tick & 0xFFU);
    out[1] = static_cast<std::uint8_t>((tick >> 8U) & 0xFFU);
    out[2] = static_cast<std::uint8_t>((tick >> 16U) & 0xFFU);
    out[3] = static_cast<std::uint8_t>((tick >> 24U) & 0xFFU);
    out[4] = static_cast<std::uint8_t>(qx & 0xFFU);
    out[5] = static_cast<std::uint8_t>((qx >> 8U) & 0xFFU);
    out[6] = static_cast<std::uint8_t>((qx >> 16U) & 0xFFU);
    out[7] = static_cast<std::uint8_t>((qx >> 24U) & 0xFFU);
    return out;
}

std::uint32_t SnapshotFnv1a(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t hash = 2166136261U;
    for (const std::uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 16777619U;
    }
    return hash;
}

ri::runtime::NetPacket EnvelopeSnapshot(
    const std::size_t peerId,
    const ri::runtime::SnapshotDeltaPacket& snapshot) {
    ri::runtime::NetPacket packet{};
    packet.peerId = peerId;
    packet.channel = 1U;
    packet.reliable = true;
    packet.payload.push_back(0xA7U);
    packet.payload.push_back(2U);
    AppendU32Le(packet.payload, snapshot.baseTick);
    AppendU32Le(packet.payload, snapshot.targetTick);
    AppendU32Le(packet.payload, snapshot.payloadChecksum);
    AppendU32Le(packet.payload, static_cast<std::uint32_t>(snapshot.encodedOps.size()));
    packet.payload.insert(packet.payload.end(), snapshot.encodedOps.begin(), snapshot.encodedOps.end());
    return packet;
}

ri::runtime::NetPacket FullAuthSnapshotPacket(const std::size_t peerId, const std::uint32_t tick, const float x) {
    ri::runtime::SnapshotDeltaPacket snapshot{};
    snapshot.encodedOps = EncodeAuthPosition(tick, x);
    snapshot.baseTick = tick;
    snapshot.targetTick = tick;
    snapshot.payloadChecksum = SnapshotFnv1a(snapshot.encodedOps);
    return EnvelopeSnapshot(peerId, snapshot);
}

/// A malformed envelope must request resync without wiping the last good baseline, so a later
/// delta against that baseline can still apply while the host rate-limits 0xA8.
bool ClientKeepsBaselineAfterMalformedPacket() {
    class ClientTransport final : public ri::runtime::INetTransport {
    public:
        bool StartServer(const ri::runtime::NetEndpoint&, std::size_t) override { return true; }
        bool StartClient() override { return true; }
        bool Connect(const ri::runtime::NetEndpoint&) override { return true; }
        void Shutdown() override {}
        bool Send(std::size_t, const ri::runtime::NetPacket& packet) override {
            sent.push_back(packet);
            return true;
        }
        std::vector<ri::runtime::NetPacket> PollReceive(std::size_t) override {
            std::vector<ri::runtime::NetPacket> out;
            out.swap(inbox);
            return out;
        }
        [[nodiscard]] std::vector<std::size_t> ConnectedPeers() const override { return {7U}; }
        [[nodiscard]] ri::runtime::NetTransportStats Stats() const noexcept override { return {}; }
        std::vector<ri::runtime::NetPacket> inbox;
        std::vector<ri::runtime::NetPacket> sent;
    };

    auto* transport = new ClientTransport();
    ri::runtime::AuthoritativeNetConfig config{};
    config.mode = ri::runtime::NetMode::ClientOnly;
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    ri::runtime::AuthoritativeNetModule client(config, std::unique_ptr<ri::runtime::INetTransport>(transport));
    ri::runtime::RuntimeContext context({}, {});
    char executable[] = "NetHardeningSmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (!client.OnRuntimeStartup(context, commandLine)) {
        return false;
    }

    std::uint64_t appliedEvents = 0U;
    std::uint64_t rejectedEvents = 0U;
    context.Events().On("net.snapshot.applied", [&](const ri::runtime::RuntimeEvent&) { ++appliedEvents; });
    context.Events().On("net.snapshot.rejected", [&](const ri::runtime::RuntimeEvent&) { ++rejectedEvents; });

    transport->inbox.push_back(FullAuthSnapshotPacket(7U, 1U, 1.0f));
    if (!client.OnRuntimeFrame(context, Frame(1, 0.0, 1.0)) || appliedEvents != 1U || rejectedEvents != 0U) {
        client.OnRuntimeShutdown(context);
        return false;
    }

    ri::runtime::NetPacket malformed{};
    malformed.peerId = 7U;
    malformed.payload = {0x00U, 0x01U};
    transport->inbox.push_back(malformed);
    if (!client.OnRuntimeFrame(context, Frame(2, 0.0, 1.0)) || rejectedEvents != 1U || transport->sent.size() != 1U) {
        client.OnRuntimeShutdown(context);
        return false;
    }

    const ri::runtime::SnapshotBlob baseline{
        .tick = 1U,
        .bytes = EncodeAuthPosition(1U, 1.0f),
    };
    const ri::runtime::SnapshotBlob next{
        .tick = 2U,
        .bytes = EncodeAuthPosition(2U, 2.0f),
    };
    const auto delta = ri::runtime::BuildSnapshotDelta(baseline, next);
    if (!delta.has_value()) {
        client.OnRuntimeShutdown(context);
        return false;
    }
    ri::runtime::NetPacket deltaPacket = EnvelopeSnapshot(7U, *delta);
    transport->inbox.push_back(deltaPacket);
    if (!client.OnRuntimeFrame(context, Frame(3, 0.0, 1.0)) || appliedEvents != 2U || rejectedEvents != 1U) {
        client.OnRuntimeShutdown(context);
        return false;
    }

    client.OnRuntimeShutdown(context);
    return true;
}

/// Unauthenticated snapshot resync requests are accepted at most once per interval.
bool SnapshotResyncRequestsAreRateLimited() {
    class ResyncTransport final : public ri::runtime::INetTransport {
    public:
        bool StartServer(const ri::runtime::NetEndpoint&, std::size_t) override { return true; }
        bool StartClient() override { return true; }
        bool Connect(const ri::runtime::NetEndpoint&) override { return true; }
        void Shutdown() override {}
        bool Send(std::size_t, const ri::runtime::NetPacket&) override { return true; }
        std::vector<ri::runtime::NetPacket> PollReceive(std::size_t) override {
            std::vector<ri::runtime::NetPacket> out;
            out.swap(inbox);
            return out;
        }
        [[nodiscard]] std::vector<std::size_t> ConnectedPeers() const override { return {7U}; }
        [[nodiscard]] ri::runtime::NetTransportStats Stats() const noexcept override { return {}; }
        std::vector<ri::runtime::NetPacket> inbox;
    };

    auto* transport = new ResyncTransport();
    ri::runtime::AuthoritativeNetConfig config{};
    config.mode = ri::runtime::NetMode::Dedicated;
    config.serverTickRate = 30;
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    ri::runtime::AuthoritativeNetModule server(config, std::unique_ptr<ri::runtime::INetTransport>(transport));
    ri::runtime::RuntimeContext context({}, {});
    char executable[] = "NetHardeningSmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (!server.OnRuntimeStartup(context, commandLine)) {
        return false;
    }

    std::uint64_t resyncAccepted = 0U;
    std::uint64_t resyncLimited = 0U;
    context.Events().On("net.snapshot.resync_requested", [&](const ri::runtime::RuntimeEvent&) { ++resyncAccepted; });
    context.Events().On("net.packet.ignored", [&](const ri::runtime::RuntimeEvent& event) {
        const auto it = event.fields.find("reason");
        if (it != event.fields.end() && it->second == "resync_rate_limited") {
            ++resyncLimited;
        }
    });

    ri::runtime::NetPacket resync{};
    resync.peerId = 7U;
    resync.payload = {0xA8U, 0x01U};
    transport->inbox.push_back(resync);
    transport->inbox.push_back(resync);
    if (!server.OnRuntimeFrame(context, Frame(1, 0.0, 1.0))
        || resyncAccepted != 1U
        || resyncLimited != 1U) {
        server.OnRuntimeShutdown(context);
        return false;
    }

    server.OnRuntimeShutdown(context);
    return true;
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
    if (!OffenseCooldownsEscalate()) {
        return EXIT_FAILURE;
    }
    if (!ClientDoesNotMuteAuthority()) {
        return EXIT_FAILURE;
    }
    if (!ClientKeepsBaselineAfterMalformedPacket()) {
        return EXIT_FAILURE;
    }
    if (!SnapshotResyncRequestsAreRateLimited()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
