#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Runtime/NetTransport.h"
#include "RawIron/Runtime/RuntimeNetcode.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TestLink {
    std::vector<ri::runtime::NetPacket> serverToClient;
    std::vector<ri::runtime::NetPacket> clientToServer;
};

class TestTransport final : public ri::runtime::INetTransport {
public:
    TestTransport(std::shared_ptr<TestLink> link, const bool server)
        : link_(std::move(link)), server_(server) {}

    bool StartServer(const ri::runtime::NetEndpoint&, std::size_t) override { return server_; }
    bool StartClient() override { return !server_; }
    bool Connect(const ri::runtime::NetEndpoint&) override { return !server_; }
    void Shutdown() override {}

    bool Send(const std::size_t peerId, const ri::runtime::NetPacket& packet) override {
        if (server_ && peerId != kClientPeerId) {
            return false;
        }
        ri::runtime::NetPacket delivered = packet;
        delivered.peerId = server_ ? 0U : kClientPeerId;
        if (server_) {
            link_->serverToClient.push_back(std::move(delivered));
        } else {
            link_->clientToServer.push_back(std::move(delivered));
        }
        stats_.packetsSent += 1U;
        stats_.bytesSent += packet.payload.size();
        return true;
    }

    std::vector<ri::runtime::NetPacket> PollReceive(const std::size_t maxPackets) override {
        std::vector<ri::runtime::NetPacket>& inbox = server_ ? link_->clientToServer : link_->serverToClient;
        const std::size_t count = std::min(maxPackets, inbox.size());
        std::vector<ri::runtime::NetPacket> out;
        out.insert(out.end(), std::make_move_iterator(inbox.begin()),
                   std::make_move_iterator(inbox.begin() + static_cast<std::ptrdiff_t>(count)));
        inbox.erase(inbox.begin(), inbox.begin() + static_cast<std::ptrdiff_t>(count));
        for (const ri::runtime::NetPacket& packet : out) {
            stats_.packetsReceived += 1U;
            stats_.bytesReceived += packet.payload.size();
        }
        return out;
    }

    [[nodiscard]] std::vector<std::size_t> ConnectedPeers() const override {
        return server_ ? std::vector<std::size_t>{kClientPeerId} : std::vector<std::size_t>{};
    }

    [[nodiscard]] ri::runtime::NetTransportStats Stats() const noexcept override { return stats_; }

private:
    static constexpr std::size_t kClientPeerId = 7U;
    std::shared_ptr<TestLink> link_;
    bool server_ = false;
    ri::runtime::NetTransportStats stats_{};
};

class TestAuthorityBridge final : public ri::runtime::IAuthoritativeSimulationBridge {
public:
    explicit TestAuthorityBridge(const bool authority) : authority_(authority) {}

    [[nodiscard]] std::optional<ri::runtime::SnapshotBlob> CaptureSnapshot(const std::uint32_t tick) override {
        if (!authority_) return std::nullopt;
        return ri::runtime::SnapshotBlob{.tick = tick, .bytes = {0x52U, 0x49U, static_cast<std::uint8_t>(tick)}};
    }

    bool ApplySnapshot(const ri::runtime::SnapshotBlob& snapshot, std::string* error) override {
        if (snapshot.bytes.size() != 3U || snapshot.bytes[0] != 0x52U || snapshot.bytes[1] != 0x49U) {
            if (error != nullptr) *error = "bad test authority snapshot";
            return false;
        }
        appliedSnapshotCount_ += 1U;
        lastAppliedTick_ = snapshot.tick;
        return true;
    }

    bool HandleCommand(const std::size_t peerId,
                       const std::uint32_t channel,
                       const std::span<const std::uint8_t> payload,
                       std::string* error) override {
        if (!authority_ || peerId != 7U || channel != 0U || payload.size() != 3U || payload[0] != 1U) {
            if (error != nullptr) *error = "bad test authority command";
            return false;
        }
        acceptedCommandCount_ += 1U;
        return true;
    }

    [[nodiscard]] std::uint32_t AppliedSnapshotCount() const noexcept { return appliedSnapshotCount_; }
    [[nodiscard]] std::uint32_t AcceptedCommandCount() const noexcept { return acceptedCommandCount_; }

private:
    bool authority_ = false;
    std::uint32_t appliedSnapshotCount_ = 0U;
    std::uint32_t acceptedCommandCount_ = 0U;
    std::uint32_t lastAppliedTick_ = 0U;
};

ri::core::FrameContext Frame(const int index) {
    return ri::core::FrameContext{
        .frameIndex = index,
        .deltaSeconds = 1.0 / 30.0,
        .elapsedSeconds = static_cast<double>(index) / 30.0,
        .realtimeSeconds = static_cast<double>(index) / 30.0,
        .realDeltaSeconds = 1.0 / 30.0,
    };
}

ri::core::SessionExtensionContract MakeSessionExtensions(const std::string_view version = "1.0.0") {
    ri::core::SessionExtensionContract contract{};
    contract.extensions.push_back({
        .id = "studio.shared-physics",
        .version = std::string(version),
        .fingerprint = "fnv1a64:0123456789abcdef",
        .kind = ri::core::SessionExtensionKind::Gameplay,
        .reloadPolicy = ri::core::SessionExtensionReloadPolicy::SimulationBoundary,
        .capabilities = {"physics.props", "world.damage"},
    });
    static_cast<void>(ri::core::NormalizeSessionExtensionContract(contract));
    return contract;
}

bool Check(const bool condition, const char* const message) {
    if (!condition) {
        std::cerr << "RuntimeNetcodeSmoke: " << message << '\n';
    }
    return condition;
}

} // namespace

int main() {
#if defined(RAWIRON_HAS_ENET)
    {
        std::unique_ptr<ri::runtime::INetTransport> enet = ri::runtime::CreateEnetTransport();
        if (!enet || !enet->StartClient()) {
            return EXIT_FAILURE;
        }
        enet->Shutdown();
    }
#endif

    auto link = std::make_shared<TestLink>();
    ri::runtime::AuthoritativeNetConfig serverConfig{};
    serverConfig.mode = ri::runtime::NetMode::Dedicated;
    serverConfig.serverTickRate = 30;
    serverConfig.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    serverConfig.requireSessionExtensionAgreement = true;
    serverConfig.sessionExtensionContract = MakeSessionExtensions();
    auto serverBridge = std::make_shared<TestAuthorityBridge>(true);
    serverConfig.simulationBridge = serverBridge;
    ri::runtime::AuthoritativeNetModule server(serverConfig, std::make_unique<TestTransport>(link, true));

    ri::runtime::AuthoritativeNetConfig clientConfig{};
    clientConfig.mode = ri::runtime::NetMode::ClientOnly;
    clientConfig.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    clientConfig.requireSessionExtensionAgreement = true;
    clientConfig.sessionExtensionContract = MakeSessionExtensions();
    auto clientBridge = std::make_shared<TestAuthorityBridge>(false);
    clientConfig.simulationBridge = clientBridge;
    ri::runtime::AuthoritativeNetModule client(clientConfig, std::make_unique<TestTransport>(link, false));

    ri::runtime::RuntimeContext serverContext({}, {});
    ri::runtime::RuntimeContext clientContext({}, {});
    char executable[] = "RuntimeNetcodeSmoke";
    char* argv[] = {executable};
    const ri::core::CommandLine commandLine(1, argv);
    if (!server.OnRuntimeStartup(serverContext, commandLine) || !client.OnRuntimeStartup(clientContext, commandLine)) {
        return EXIT_FAILURE;
    }

    ri::runtime::NetPacket command{};
    command.channel = 0U;
    command.reliable = true;
    command.payload = {1U, 2U, 3U};
    // Gameplay is refused until the exact host contract has been received and accepted.
    if (!Check(!client.SendPacket(0U, command, ri::runtime::NetChannelKind::Authority), "preflight did not gate command")
        || !Check(server.OnRuntimeFrame(serverContext, Frame(1)), "server offer frame failed")
        || !Check(client.OnRuntimeFrame(clientContext, Frame(1)), "client offer frame failed")
        || !Check(client.SessionExtensionState() == ri::runtime::SessionExtensionPeerState::Accepted, "client did not accept offer")
        || !Check(server.OnRuntimeFrame(serverContext, Frame(2)), "server acknowledgement frame failed")
        || !Check(server.PeerSessionExtensionState(7U) == ri::runtime::SessionExtensionPeerState::Accepted,
                  "server did not accept acknowledgement")) {
        return EXIT_FAILURE;
    }
    // Drain the repeated offer and first accepted snapshot.
    if (!Check(client.OnRuntimeFrame(clientContext, Frame(2)), "client accepted snapshot frame failed")) {
        return EXIT_FAILURE;
    }

    if (!Check(client.SendPacket(0U, command, ri::runtime::NetChannelKind::Authority), "accepted client command rejected")) {
        return EXIT_FAILURE;
    }

    if (!Check(server.OnRuntimeFrame(serverContext, Frame(3)), "server command frame failed")
        || !Check(server.ServerStats().inboundPackets >= 2U, "server did not receive acknowledgement and command")
        || !Check(serverBridge->AcceptedCommandCount() == 1U, "domain bridge did not accept command")
        || !Check(server.ServerStats().outboundPackets > 0U, "server sent no snapshots")
        || !Check(!link->serverToClient.empty(), "client did not receive snapshots")) {
        return EXIT_FAILURE;
    }
    if (!Check(client.OnRuntimeFrame(clientContext, Frame(3)), "client snapshot frame failed")) {
        return EXIT_FAILURE;
    }
    for (int frameIndex = 4; frameIndex <= 10 && clientBridge->AppliedSnapshotCount() == 0U; ++frameIndex) {
        if (!Check(server.OnRuntimeFrame(serverContext, Frame(frameIndex)), "server domain snapshot frame failed")
            || !Check(client.OnRuntimeFrame(clientContext, Frame(frameIndex)), "client domain snapshot frame failed")) {
            return EXIT_FAILURE;
        }
    }
    if (!Check(clientBridge->AppliedSnapshotCount() > 0U, "domain bridge did not apply snapshot")) {
        return EXIT_FAILURE;
    }

    server.OnRuntimeShutdown(serverContext);
    client.OnRuntimeShutdown(clientContext);

    // A client with a different package revision must be kept out of gameplay.
    auto rejectedLink = std::make_shared<TestLink>();
    ri::runtime::AuthoritativeNetConfig rejectedServerConfig = serverConfig;
    ri::runtime::AuthoritativeNetConfig rejectedClientConfig = clientConfig;
    rejectedClientConfig.sessionExtensionContract = MakeSessionExtensions("1.0.1");
    ri::runtime::AuthoritativeNetModule rejectedServer(
        rejectedServerConfig, std::make_unique<TestTransport>(rejectedLink, true));
    ri::runtime::AuthoritativeNetModule rejectedClient(
        rejectedClientConfig, std::make_unique<TestTransport>(rejectedLink, false));
    ri::runtime::RuntimeContext rejectedServerContext({}, {});
    ri::runtime::RuntimeContext rejectedClientContext({}, {});
    if (!Check(rejectedServer.OnRuntimeStartup(rejectedServerContext, commandLine), "mismatch server startup failed")
        || !Check(rejectedClient.OnRuntimeStartup(rejectedClientContext, commandLine), "mismatch client startup failed")
        || !Check(rejectedServer.OnRuntimeFrame(rejectedServerContext, Frame(1)), "mismatch server offer frame failed")
        || !Check(rejectedClient.OnRuntimeFrame(rejectedClientContext, Frame(1)), "mismatch client offer frame failed")
        || !Check(rejectedServer.OnRuntimeFrame(rejectedServerContext, Frame(2)), "mismatch server acknowledgement frame failed")
        || !Check(rejectedClient.SessionExtensionState() == ri::runtime::SessionExtensionPeerState::Rejected,
                  "mismatch client was not rejected")
        || !Check(rejectedServer.PeerSessionExtensionState(7U) == ri::runtime::SessionExtensionPeerState::Rejected,
                  "mismatch server did not reject client")
        || !Check(!rejectedClient.SendPacket(0U, command, ri::runtime::NetChannelKind::Authority),
                  "mismatch client command was accepted")) {
        return EXIT_FAILURE;
    }
    rejectedServer.OnRuntimeShutdown(rejectedServerContext);
    rejectedClient.OnRuntimeShutdown(rejectedClientContext);
    return EXIT_SUCCESS;
}
