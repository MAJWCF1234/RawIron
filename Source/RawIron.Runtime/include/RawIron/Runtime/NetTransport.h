#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ri::runtime {

enum class NetRole : std::uint8_t {
    None = 0,
    DedicatedServer,
    ListenServer,
    Client,
};

struct NetEndpoint {
    std::string host = "127.0.0.1";
    std::uint16_t port = 27015;
};

struct NetPacket {
    std::uint32_t channel = 0;
    bool reliable = false;
    std::vector<std::uint8_t> payload;
};

struct NetTransportStats {
    std::uint64_t packetsSent = 0;
    std::uint64_t packetsReceived = 0;
    std::uint64_t bytesSent = 0;
    std::uint64_t bytesReceived = 0;
    std::uint64_t droppedBySimulator = 0;
};

class INetTransport {
public:
    virtual ~INetTransport() = default;

    virtual bool StartServer(const NetEndpoint& endpoint, std::size_t maxPeers) = 0;
    virtual bool StartClient() = 0;
    virtual bool Connect(const NetEndpoint& endpoint) = 0;
    virtual void Shutdown() = 0;

    virtual bool Send(std::size_t peerId, const NetPacket& packet) = 0;
    virtual std::vector<NetPacket> PollReceive(std::size_t maxPackets) = 0;
    [[nodiscard]] virtual NetTransportStats Stats() const noexcept = 0;
};

/// Builds the ENet-backed transport when enabled at build-time; otherwise returns nullptr.
[[nodiscard]] std::unique_ptr<INetTransport> CreateEnetTransport();

} // namespace ri::runtime

