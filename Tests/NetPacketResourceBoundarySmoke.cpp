#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Runtime/LatencyTools.h"
#include "RawIron/Runtime/NetTransport.h"
#include "RawIron/Runtime/RuntimeNetcode.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(RAWIRON_HAS_ENET)
#include <enet/enet.h>
#endif

namespace {

class InjectingTransport final : public ri::runtime::INetTransport {
public:
    bool StartServer(const ri::runtime::NetEndpoint&, std::size_t) override { return true; }
    bool StartClient() override { return true; }
    bool Connect(const ri::runtime::NetEndpoint&) override { return true; }
    void Shutdown() override {}

    bool Send(std::size_t, const ri::runtime::NetPacket& packet) override {
        ++sendCalls;
        sentBytes += packet.payload.size();
        return true;
    }

    std::vector<ri::runtime::NetPacket> PollReceive(const std::size_t maxPackets) override {
        const std::size_t count = ignorePollBudget
            ? inbox.size()
            : (std::min)(maxPackets, inbox.size());
        std::vector<ri::runtime::NetPacket> out;
        out.reserve(count);
        out.insert(out.end(), std::make_move_iterator(inbox.begin()),
                   std::make_move_iterator(inbox.begin() + static_cast<std::ptrdiff_t>(count)));
        inbox.erase(inbox.begin(), inbox.begin() + static_cast<std::ptrdiff_t>(count));
        stats_.packetsReceived += count;
        for (const ri::runtime::NetPacket& packet : out) {
            stats_.bytesReceived += packet.payload.size();
        }
        return out;
    }

    [[nodiscard]] std::vector<std::size_t> ConnectedPeers() const override { return {}; }
    [[nodiscard]] ri::runtime::NetTransportStats Stats() const noexcept override { return stats_; }

    std::vector<ri::runtime::NetPacket> inbox;
    std::size_t sendCalls = 0U;
    std::size_t sentBytes = 0U;
    bool ignorePollBudget = false;

private:
    ri::runtime::NetTransportStats stats_{};
};

ri::core::FrameContext Frame() {
    return ri::core::FrameContext{
        .frameIndex = 1,
        .deltaSeconds = 0.0,
        .elapsedSeconds = 1.0,
        .realtimeSeconds = 1.0,
        .realDeltaSeconds = 0.0,
    };
}

bool DelayQueueHasAnAggregateByteBoundary() {
    using ri::runtime::LatencySimulator;
    using ri::runtime::NetPacket;

    LatencySimulator simulator(7U);
    ri::runtime::LatencySimulationConfig config{};
    config.enabled = false;
    simulator.Configure(config);

    NetPacket oversized{};
    oversized.payload.resize(ri::runtime::kMaxNetPacketPayloadBytes + 1U);
    if (simulator.Enqueue(100U, std::move(oversized))
        || simulator.DroppedOversizedPackets() != 1U
        || simulator.QueuedPacketCount() != 0U
        || simulator.QueuedPayloadBytes() != 0U) {
        return false;
    }

    NetPacket exactLimit{};
    exactLimit.payload.resize(ri::runtime::kMaxNetPacketPayloadBytes);
    if (!simulator.Enqueue(100U, std::move(exactLimit))
        || simulator.QueuedPayloadBytes() != ri::runtime::kMaxNetPacketPayloadBytes
        || !simulator.TryPopReady(100U).has_value()
        || simulator.QueuedPayloadBytes() != 0U) {
        return false;
    }

    constexpr std::size_t chunkBytes = 1024U * 1024U;
    constexpr std::size_t expectedChunks = LatencySimulator::kMaxQueuedPayloadBytes / chunkBytes;
    NetPacket chunk{};
    chunk.payload.resize(chunkBytes);
    for (std::size_t index = 0U; index < expectedChunks; ++index) {
        if (!simulator.Enqueue(200U, chunk)) {
            return false;
        }
    }
    if (simulator.Enqueue(200U, chunk)
        || simulator.DroppedByByteBudget() != 1U
        || simulator.QueuedPacketCount() != expectedChunks
        || simulator.QueuedPayloadBytes() != LatencySimulator::kMaxQueuedPayloadBytes) {
        return false;
    }

    while (simulator.TryPopReady(200U).has_value()) {
    }
    if (simulator.QueuedPacketCount() != 0U || simulator.QueuedPayloadBytes() != 0U) {
        return false;
    }

    if (!simulator.Enqueue(300U, chunk)) {
        return false;
    }
    simulator.Configure(config);
    if (simulator.QueuedPacketCount() != 0U
        || simulator.QueuedPayloadBytes() != 0U
        || simulator.DroppedPackets() != 2U
        || simulator.DroppedBySimulation() != 0U) {
        return false;
    }

    ri::runtime::LatencySimulationConfig fullLoss{};
    fullLoss.enabled = true;
    fullLoss.packetLossPercent = 100.0f;
    simulator.Configure(fullLoss);
    NetPacket simulatedDrop{};
    simulatedDrop.payload = {1U};
    if (simulator.Enqueue(400U, std::move(simulatedDrop))
        || simulator.DroppedPackets() != 3U
        || simulator.DroppedBySimulation() != 1U) {
        return false;
    }

    LatencySimulator packetBounded(11U);
    packetBounded.Configure(config);
    NetPacket empty{};
    for (std::size_t index = 0U; index < LatencySimulator::kMaxQueuedPackets; ++index) {
        if (!packetBounded.Enqueue(500U, empty)) {
            return false;
        }
    }
    if (packetBounded.Enqueue(500U, empty)
        || packetBounded.QueuedPacketCount() != LatencySimulator::kMaxQueuedPackets
        || packetBounded.DroppedByPacketBudget() != 1U) {
        return false;
    }

    LatencySimulator saturatingDelay(13U);
    ri::runtime::LatencySimulationConfig longDelay{};
    longDelay.enabled = true;
    longDelay.baseDelayMs = (std::numeric_limits<int>::max)();
    longDelay.packetLossPercent = std::numeric_limits<float>::quiet_NaN();
    saturatingDelay.Configure(longDelay);
    NetPacket delayed{};
    delayed.payload = {1U};
    const std::uint64_t nearMaximum = (std::numeric_limits<std::uint64_t>::max)() - 4U;
    return saturatingDelay.Enqueue(nearMaximum, std::move(delayed))
        && !saturatingDelay.TryPopReady(nearMaximum).has_value()
        && saturatingDelay.TryPopReady((std::numeric_limits<std::uint64_t>::max)()).has_value()
        && saturatingDelay.Config().packetLossPercent == 0.0f;
}

bool RuntimeDefendsAgainstDishonestTransportPolls() {
    auto transport = std::make_unique<InjectingTransport>();
    InjectingTransport* injected = transport.get();
    injected->ignorePollBudget = true;

    ri::runtime::AuthoritativeNetConfig config{};
    config.mode = ri::runtime::NetMode::Dedicated;
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    ri::runtime::AuthoritativeNetModule module(config, std::move(transport));
    ri::runtime::RuntimeContext context({}, {});
    std::size_t commandEvents = 0U;
    std::size_t truncatedEvents = 0U;
    std::size_t pollByteDrops = 0U;
    context.Events().On("net.command.received", [&](const ri::runtime::RuntimeEvent&) {
        ++commandEvents;
    });
    context.Events().On("net.poll.truncated", [&](const ri::runtime::RuntimeEvent&) {
        ++truncatedEvents;
    });
    context.Events().On("net.packet.ignored", [&](const ri::runtime::RuntimeEvent& event) {
        const auto reason = event.fields.find("reason");
        if (reason != event.fields.end() && reason->second == "inbound_poll_byte_limit") {
            ++pollByteDrops;
        }
    });

    char executable[] = "NetPacketResourceBoundarySmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (!module.OnRuntimeStartup(context, commandLine)) {
        return false;
    }

    injected->inbox.resize(130U);
    if (!module.OnRuntimeFrame(context, Frame())
        || commandEvents != 128U
        || truncatedEvents != 1U
        || module.ServerStats().inboundPackets != 130U
        || module.ServerStats().inboundPacketsDroppedByPollBudget != 2U) {
        module.OnRuntimeShutdown(context);
        return false;
    }

    constexpr std::size_t payloadBytes = 300U * 1024U;
    injected->inbox.resize(128U);
    for (ri::runtime::NetPacket& packet : injected->inbox) {
        packet.payload.resize(payloadBytes);
    }
    const std::size_t packetsWithinByteBudget =
        ri::runtime::kMaxNetPayloadBytesPerPoll / payloadBytes;
    if (!module.OnRuntimeFrame(context, Frame())
        || commandEvents != 128U + packetsWithinByteBudget
        || pollByteDrops != 128U - packetsWithinByteBudget
        || module.ServerStats().inboundPacketsDroppedByPollBudget
            != 2U + (128U - packetsWithinByteBudget)) {
        module.OnRuntimeShutdown(context);
        return false;
    }
    module.OnRuntimeShutdown(context);
    return true;
}

bool RuntimeRejectsOversizeBeforeDispatch() {
    auto transport = std::make_unique<InjectingTransport>();
    InjectingTransport* injected = transport.get();

    ri::runtime::AuthoritativeNetConfig config{};
    config.mode = ri::runtime::NetMode::Dedicated;
    config.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    ri::runtime::AuthoritativeNetModule module(config, std::move(transport));

    ri::runtime::RuntimeContext context({}, {});
    std::size_t commandEvents = 0U;
    std::size_t ignoredEvents = 0U;
    std::string ignoredReason;
    context.Events().On("net.command.received", [&](const ri::runtime::RuntimeEvent&) {
        ++commandEvents;
    });
    context.Events().On("net.packet.ignored", [&](const ri::runtime::RuntimeEvent& event) {
        ++ignoredEvents;
        if (const auto reason = event.fields.find("reason"); reason != event.fields.end()) {
            ignoredReason = reason->second;
        }
    });

    char executable[] = "NetPacketResourceBoundarySmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (!module.OnRuntimeStartup(context, commandLine)) {
        return false;
    }

    ri::runtime::NetPacket oversized{};
    oversized.peerId = 41U;
    oversized.payload.resize(ri::runtime::kMaxNetPacketPayloadBytes + 1U);
    injected->inbox.push_back(std::move(oversized));

    // Use a different peer for the follow-up valid packet: soft protocol-offense cooldowns
    // intentionally suppress further traffic from the offending peerId.
    ri::runtime::NetPacket valid{};
    valid.peerId = 42U;
    valid.payload = {1U, 2U, 3U};
    injected->inbox.push_back(std::move(valid));

    if (!module.OnRuntimeFrame(context, Frame())
        || commandEvents != 1U
        || ignoredEvents != 1U
        || ignoredReason != "payload_too_large"
        || module.ServerStats().inboundPackets != 2U
        || module.ServerStats().oversizedInboundPacketsRejected != 1U) {
        module.OnRuntimeShutdown(context);
        return false;
    }

    ri::runtime::NetPacket oversizedOutbound{};
    oversizedOutbound.payload.resize(ri::runtime::kMaxNetPacketPayloadBytes + 1U);
    if (module.SendPacket(41U, std::move(oversizedOutbound), ri::runtime::NetChannelKind::Authority)
        || injected->sendCalls != 0U
        || module.ServerStats().oversizedOutboundPacketsRejected != 1U) {
        module.OnRuntimeShutdown(context);
        return false;
    }

    ri::runtime::NetPacket exactOutbound{};
    exactOutbound.payload.resize(ri::runtime::kMaxNetPacketPayloadBytes);
    const bool exactAccepted = module.SendPacket(
        41U, std::move(exactOutbound), ri::runtime::NetChannelKind::Authority);
    module.OnRuntimeShutdown(context);
    return exactAccepted
        && injected->sendCalls == 1U
        && injected->sentBytes == ri::runtime::kMaxNetPacketPayloadBytes;
}

bool EnetRejectsOversizeBeforeReassemblyAllocation() {
#if defined(RAWIRON_HAS_ENET)
    std::unique_ptr<ri::runtime::INetTransport> server = ri::runtime::CreateEnetTransport();
    if (server == nullptr) {
        return false;
    }

    ri::runtime::NetEndpoint endpoint{.host = "127.0.0.1", .port = 0U};
    bool listening = false;
    for (std::uint16_t port = 43100U; port < 43200U; ++port) {
        endpoint.port = port;
        if (server->StartServer(endpoint, 1U)) {
            listening = true;
            break;
        }
    }
    if (!listening) {
        return false;
    }

    ENetHost* client = enet_host_create(nullptr, 1U, 2U, 0U, 0U);
    if (client == nullptr) {
        server->Shutdown();
        return false;
    }

    ENetAddress address{};
    address.port = endpoint.port;
    ENetPeer* peer = nullptr;
    if (enet_address_set_host(&address, endpoint.host.c_str()) == 0) {
        peer = enet_host_connect(client, &address, 2U, 0U);
    }
    if (peer == nullptr) {
        enet_host_destroy(client);
        server->Shutdown();
        return false;
    }

    bool connected = false;
    const auto connectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!connected && std::chrono::steady_clock::now() < connectDeadline) {
        (void)server->PollReceive(16U);
        ENetEvent event{};
        while (enet_host_service(client, &event, 0U) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                connected = true;
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE && event.packet != nullptr) {
                enet_packet_destroy(event.packet);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    bool rejected = false;
    if (connected) {
        std::vector<std::uint8_t> payload(ri::runtime::kMaxNetPacketPayloadBytes + 1U, 0xA5U);
        ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
        if (packet != nullptr) {
            if (enet_peer_send(peer, 0U, packet) != 0) {
                enet_packet_destroy(packet);
            } else {
                enet_host_flush(client);
                const auto receiveDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
                std::optional<std::chrono::steady_clock::time_point> fullySentAt;
                bool acceptedOversize = false;
                while (std::chrono::steady_clock::now() < receiveDeadline) {
                    const std::vector<ri::runtime::NetPacket> accepted = server->PollReceive(16U);
                    if (!accepted.empty()) {
                        acceptedOversize = true;
                        break;
                    }
                    const ri::runtime::NetTransportStats stats = server->Stats();
                    if (stats.packetsReceived != 0U || stats.bytesReceived != 0U) {
                        acceptedOversize = true;
                        break;
                    }
                    // Pre-reassembly drop (maximumPacketSize) or post-receive size gate both count.
                    if (stats.oversizedPacketsRejected != 0U || stats.oversizedBytesRejected != 0U) {
                        rejected = true;
                        break;
                    }
                    ENetEvent event{};
                    while (enet_host_service(client, &event, 0U) > 0) {
                        if (event.type == ENET_EVENT_TYPE_RECEIVE && event.packet != nullptr) {
                            enet_packet_destroy(event.packet);
                        }
                    }
                    if (!fullySentAt.has_value()
                        && enet_list_empty(&peer->outgoingSendReliableCommands) != 0) {
                        fullySentAt = std::chrono::steady_clock::now();
                    }
                    if (fullySentAt.has_value()
                        && std::chrono::steady_clock::now() - *fullySentAt
                            >= std::chrono::milliseconds(500)) {
                        rejected = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                // Reliable fragment resends can keep the client send queue non-empty when the
                // server silently drops oversize reassembly. No accepted payload still means reject.
                if (!rejected && !acceptedOversize) {
                    const ri::runtime::NetTransportStats stats = server->Stats();
                    rejected = stats.packetsReceived == 0U && stats.bytesReceived == 0U;
                }
            }
        }
    }

    enet_host_destroy(client);
    server->Shutdown();
    return rejected;
#else
    return true;
#endif
}

} // namespace

int main() {
    return DelayQueueHasAnAggregateByteBoundary()
            && RuntimeDefendsAgainstDishonestTransportPolls()
            && RuntimeRejectsOversizeBeforeDispatch()
            && EnetRejectsOversizeBeforeReassemblyAllocation()
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
