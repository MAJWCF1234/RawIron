#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Runtime/NetTransport.h"
#include "RawIron/Runtime/RuntimeNetcode.h"

#include <cstdlib>
#include <memory>
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

ri::core::FrameContext Frame(const int index) {
    return ri::core::FrameContext{
        .frameIndex = index,
        .deltaSeconds = 1.0 / 30.0,
        .elapsedSeconds = static_cast<double>(index) / 30.0,
        .realtimeSeconds = static_cast<double>(index) / 30.0,
        .realDeltaSeconds = 1.0 / 30.0,
    };
}

} // namespace

int main() {
    auto link = std::make_shared<TestLink>();
    ri::runtime::AuthoritativeNetConfig serverConfig{};
    serverConfig.mode = ri::runtime::NetMode::Dedicated;
    serverConfig.serverTickRate = 30;
    serverConfig.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
    ri::runtime::AuthoritativeNetModule server(serverConfig, std::make_unique<TestTransport>(link, true));

    ri::runtime::AuthoritativeNetConfig clientConfig{};
    clientConfig.mode = ri::runtime::NetMode::ClientOnly;
    clientConfig.rendezvousProvider = ri::runtime::RendezvousProviderKind::DirectToken;
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
    if (!client.SendPacket(0U, std::move(command), ri::runtime::NetChannelKind::Authority)) {
        return EXIT_FAILURE;
    }

    if (!server.OnRuntimeFrame(serverContext, Frame(1)) || server.ServerStats().inboundPackets != 1U ||
        server.ServerStats().outboundPackets == 0U || link->serverToClient.empty()) {
        return EXIT_FAILURE;
    }
    if (!client.OnRuntimeFrame(clientContext, Frame(1)) || client.PredictionStats().reconciledFrames == 0U ||
        client.PredictionStats().correctionCount == 0U) {
        return EXIT_FAILURE;
    }

    server.OnRuntimeShutdown(serverContext);
    client.OnRuntimeShutdown(clientContext);
    return EXIT_SUCCESS;
}
