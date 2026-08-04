#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ri::runtime {

/// Hard payload ceiling shared by every transport and the runtime ingress queue. The current
/// snapshot protocol allows at most 4 MiB of encoded data; 64 bytes leave room for its envelope
/// without allowing a peer to turn one packet into an unbounded engine allocation.
inline constexpr std::size_t kMaxNetPacketPayloadBytes = (4U * 1024U * 1024U) + 64U;
/// Aggregate payload copied out of a transport by one poll is bounded independently of event count.
inline constexpr std::size_t kMaxNetPayloadBytesPerPoll = 32U * 1024U * 1024U;
/// Defensive ceiling for direct transport callers. RuntimeNetcode requests a smaller frame budget.
inline constexpr std::size_t kMaxNetEventsPerPoll = 4096U;

[[nodiscard]] constexpr bool IsNetPacketPayloadSizeAllowed(const std::size_t payloadBytes) noexcept {
    return payloadBytes <= kMaxNetPacketPayloadBytes;
}

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

/// True when `host` is a bind-any / loopback address that peers cannot reach.
[[nodiscard]] bool IsUnreachableAdvertiseHost(std::string_view host);

/// Picks a LAN IPv4 (prefer non-loopback) for join-code advertising.
[[nodiscard]] std::string DetectLocalAdvertiseHost();

/// Replaces unreachable bind hosts with an advertise host suitable for DirectToken/EOS join codes.
/// `overrideHost` wins when non-empty (from `--advertise-host`).
[[nodiscard]] NetEndpoint ResolveAdvertiseEndpoint(const NetEndpoint& bindEndpoint,
                                                   std::string_view overrideHost = {});

struct NetPacket {
    /// Transport-assigned sender identity. Outbound callers may leave this at zero.
    std::size_t peerId = 0;
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
    /// Packets refused before the transport copies or allocates another payload buffer.
    std::uint64_t oversizedPacketsRejected = 0;
    std::uint64_t oversizedBytesRejected = 0;
};

class INetTransport {
public:
    virtual ~INetTransport() = default;

    virtual bool StartServer(const NetEndpoint& endpoint, std::size_t maxPeers) = 0;
    virtual bool StartClient() = 0;
    virtual bool Connect(const NetEndpoint& endpoint) = 0;
    virtual void Shutdown() = 0;

    virtual bool Send(std::size_t peerId, const NetPacket& packet) = 0;
    /// Services at most `maxPackets` transport events and returns the accepted payload events.
    /// Rejected payloads and connection churn still consume the work budget. Implementations must
    /// also stop before starting another event once `kMaxNetPayloadBytesPerPoll` has been copied.
    /// Calls and all other transport methods are runtime-thread confined unless an implementation
    /// explicitly documents stronger synchronization.
    virtual std::vector<NetPacket> PollReceive(std::size_t maxPackets) = 0;
    [[nodiscard]] virtual std::vector<std::size_t> ConnectedPeers() const = 0;
    [[nodiscard]] virtual NetTransportStats Stats() const noexcept = 0;
};

/// Builds the ENet-backed transport when enabled at build-time; otherwise returns nullptr.
/// Callers must treat a null result as an unavailable networking feature, never as an
/// in-process simulation.
[[nodiscard]] std::unique_ptr<INetTransport> CreateEnetTransport();

} // namespace ri::runtime

