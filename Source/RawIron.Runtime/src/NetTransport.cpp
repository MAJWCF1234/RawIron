#include "RawIron/Runtime/NetTransport.h"

#include "RawIron/Core/Log.h"

#include <memory>
#include <unordered_map>

#if defined(RAWIRON_HAS_ENET)
#include <enet/enet.h>
#endif

namespace ri::runtime {
namespace {

#if defined(RAWIRON_HAS_ENET)
class EnetTransport final : public INetTransport {
public:
    EnetTransport() {
        if (enet_initialize() == 0) {
            initialized_ = true;
        }
    }

    ~EnetTransport() override {
        Shutdown();
        if (initialized_) {
            enet_deinitialize();
        }
    }

    bool StartServer(const NetEndpoint& endpoint, const std::size_t maxPeers) override {
        if (!initialized_) {
            return false;
        }
        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = endpoint.port;
        host_ = enet_host_create(&address, maxPeers, 2, 0, 0);
        server_ = host_ != nullptr;
        return host_ != nullptr;
    }

    bool StartClient() override {
        if (!initialized_) {
            return false;
        }
        host_ = enet_host_create(nullptr, 1, 2, 0, 0);
        server_ = false;
        return host_ != nullptr;
    }

    bool Connect(const NetEndpoint& endpoint) override {
        if (host_ == nullptr) {
            return false;
        }
        ENetAddress address{};
        if (enet_address_set_host(&address, endpoint.host.c_str()) != 0) {
            return false;
        }
        address.port = endpoint.port;
        peer_ = enet_host_connect(host_, &address, 2, 0);
        return peer_ != nullptr;
    }

    void Shutdown() override {
        if (host_ != nullptr) {
            enet_host_destroy(host_);
            host_ = nullptr;
        }
        peer_ = nullptr;
        server_ = false;
        serverPeers_.clear();
    }

    bool Send(const std::size_t peerId, const NetPacket& packet) override {
        ENetPeer* destination = peer_;
        if (server_) {
            const auto found = serverPeers_.find(peerId);
            destination = found == serverPeers_.end() ? nullptr : found->second;
        }
        if (destination == nullptr) {
            return false;
        }
        ENetPacket* p = enet_packet_create(packet.payload.data(), packet.payload.size(),
                                           packet.reliable ? ENET_PACKET_FLAG_RELIABLE : 0U);
        if (p == nullptr) {
            return false;
        }
        if (enet_peer_send(destination, static_cast<enet_uint8>(packet.channel), p) != 0) {
            enet_packet_destroy(p);
            return false;
        }
        stats_.packetsSent += 1;
        stats_.bytesSent += packet.payload.size();
        enet_host_flush(host_);
        return true;
    }

    std::vector<NetPacket> PollReceive(const std::size_t maxPackets) override {
        std::vector<NetPacket> out;
        if (host_ == nullptr || maxPackets == 0) {
            return out;
        }
        out.reserve(maxPackets);
        ENetEvent ev{};
        while (out.size() < maxPackets && enet_host_service(host_, &ev, 0) > 0) {
            if (ev.type == ENET_EVENT_TYPE_CONNECT && ev.peer != nullptr && server_) {
                const std::size_t peerId = nextPeerId_++;
                serverPeers_[peerId] = ev.peer;
                ev.peer->data = reinterpret_cast<void*>(peerId + 1U);
            } else if (ev.type == ENET_EVENT_TYPE_DISCONNECT && ev.peer != nullptr && server_) {
                const std::size_t peerId = reinterpret_cast<std::size_t>(ev.peer->data);
                if (peerId > 0U) {
                    serverPeers_.erase(peerId - 1U);
                }
                ev.peer->data = nullptr;
            } else if (ev.type == ENET_EVENT_TYPE_RECEIVE && ev.packet != nullptr) {
                NetPacket packet{};
                if (server_ && ev.peer != nullptr) {
                    const std::size_t peerId = reinterpret_cast<std::size_t>(ev.peer->data);
                    packet.peerId = peerId > 0U ? peerId - 1U : 0U;
                }
                packet.channel = ev.channelID;
                packet.reliable = (ev.packet->flags & ENET_PACKET_FLAG_RELIABLE) != 0U;
                packet.payload.assign(ev.packet->data, ev.packet->data + ev.packet->dataLength);
                stats_.packetsReceived += 1;
                stats_.bytesReceived += ev.packet->dataLength;
                out.push_back(std::move(packet));
                enet_packet_destroy(ev.packet);
            }
        }
        return out;
    }

    [[nodiscard]] std::vector<std::size_t> ConnectedPeers() const override {
        std::vector<std::size_t> peers;
        peers.reserve(serverPeers_.size());
        for (const auto& [peerId, ignored] : serverPeers_) {
            (void)ignored;
            peers.push_back(peerId);
        }
        return peers;
    }

    [[nodiscard]] NetTransportStats Stats() const noexcept override { return stats_; }

private:
    bool initialized_ = false;
    bool server_ = false;
    ENetHost* host_ = nullptr;
    ENetPeer* peer_ = nullptr;
    std::unordered_map<std::size_t, ENetPeer*> serverPeers_{};
    std::size_t nextPeerId_ = 0U;
    NetTransportStats stats_{};
};
#endif

} // namespace

std::unique_ptr<INetTransport> CreateEnetTransport() {
#if defined(RAWIRON_HAS_ENET)
    return std::make_unique<EnetTransport>();
#else
    ri::core::LogInfo("ENet backend unavailable: build without RAWIRON_HAS_ENET.");
    return {};
#endif
}

} // namespace ri::runtime
