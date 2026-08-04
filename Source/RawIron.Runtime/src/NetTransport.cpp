#include "RawIron/Runtime/NetTransport.h"

#include "RawIron/Core/Log.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif

#if defined(RAWIRON_HAS_ENET)
#include <enet/enet.h>
#endif

namespace ri::runtime {

namespace {

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsPrivateLanIpv4(const std::string& ip) {
    if (ip.starts_with("192.168.") || ip.starts_with("10.")) {
        return true;
    }
    // 172.16.0.0/12
    if (!ip.starts_with("172.")) {
        return false;
    }
    const std::size_t secondDot = ip.find('.', 4);
    if (secondDot == std::string::npos) {
        return false;
    }
    try {
        const int secondOctet = std::stoi(ip.substr(4, secondDot - 4));
        return secondOctet >= 16 && secondOctet <= 31;
    } catch (...) {
        return false;
    }
}

} // namespace

bool IsUnreachableAdvertiseHost(const std::string_view host) {
    if (host.empty()) {
        return true;
    }
    if (host == "0.0.0.0" || host == "::" || host == "*" || EqualsIgnoreCase(host, "localhost")) {
        return true;
    }
    return host == "127.0.0.1" || host == "::1";
}

std::string DetectLocalAdvertiseHost() {
#if defined(_WIN32)
    WSADATA wsa{};
    const bool needCleanup = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;

    std::string preferredLan;
    std::string anyRoutable;

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG bufLen = 16 * 1024;
    std::vector<unsigned char> buffer(bufLen);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufLen);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bufLen);
    }

    if (ret == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
            if (adapter->OperStatus != IfOperStatusUp) {
                continue;
            }
            if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                continue;
            }
            for (IP_ADAPTER_UNICAST_ADDRESS* uni = adapter->FirstUnicastAddress; uni != nullptr; uni = uni->Next) {
                if (uni->Address.lpSockaddr == nullptr ||
                    uni->Address.lpSockaddr->sa_family != AF_INET) {
                    continue;
                }
                const auto* sa = reinterpret_cast<sockaddr_in*>(uni->Address.lpSockaddr);
                char ip[INET_ADDRSTRLEN] = {};
                if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip)) == nullptr) {
                    continue;
                }
                const std::string candidate = ip;
                if (IsUnreachableAdvertiseHost(candidate)) {
                    continue;
                }
                if (preferredLan.empty() && IsPrivateLanIpv4(candidate)) {
                    preferredLan = candidate;
                }
                if (anyRoutable.empty()) {
                    anyRoutable = candidate;
                }
            }
            if (!preferredLan.empty()) {
                break;
            }
        }
    }

    if (needCleanup) {
        WSACleanup();
    }

    if (!preferredLan.empty()) {
        return preferredLan;
    }
    if (!anyRoutable.empty()) {
        return anyRoutable;
    }
    // Absolute last resort when no up adapters expose a unicast IPv4 address.
    return "127.0.0.1";
#else
    return "127.0.0.1";
#endif
}

NetEndpoint ResolveAdvertiseEndpoint(const NetEndpoint& bindEndpoint, const std::string_view overrideHost) {
    NetEndpoint out = bindEndpoint;
    if (!overrideHost.empty()) {
        // An override that no remote peer can dial (wildcard or loopback) would be baked straight
        // into the join code, so it is worse than no override at all. Launchers pass this through
        // from an optional positional argument, which makes it easy to get wrong.
        if (IsUnreachableAdvertiseHost(overrideHost)) {
            ri::core::LogInfo("Net: ignoring unreachable --advertise-host '" + std::string(overrideHost) +
                              "'; detecting a routable address instead.");
        } else {
            out.host = std::string(overrideHost);
            return out;
        }
    }
    if (IsUnreachableAdvertiseHost(out.host)) {
        out.host = DetectLocalAdvertiseHost();
    }
    return out;
}

namespace {

#if defined(RAWIRON_HAS_ENET)

/// ENet's init/deinit manage process-global state, so they must be reference counted.
/// Hybrid P2P builds two transports; letting the first destructor call
/// enet_deinitialize() would tear ENet down underneath the surviving host.
class EnetLibraryGuard {
public:
    EnetLibraryGuard() {
        const std::lock_guard<std::mutex> lock(Mutex());
        std::size_t& count = RefCount();
        if (count == 0U) {
            initializedGlobally_ = enet_initialize() == 0;
            if (!initializedGlobally_) {
                return;
            }
        }
        ++count;
        acquired_ = true;
    }

    ~EnetLibraryGuard() {
        if (!acquired_) {
            return;
        }
        const std::lock_guard<std::mutex> lock(Mutex());
        std::size_t& count = RefCount();
        if (count > 0U && --count == 0U) {
            enet_deinitialize();
        }
    }

    EnetLibraryGuard(const EnetLibraryGuard&) = delete;
    EnetLibraryGuard& operator=(const EnetLibraryGuard&) = delete;

    [[nodiscard]] bool Ready() const noexcept { return acquired_; }

private:
    static std::mutex& Mutex() {
        static std::mutex mutex;
        return mutex;
    }

    static std::size_t& RefCount() {
        static std::size_t count = 0U;
        return count;
    }

    bool acquired_ = false;
    bool initializedGlobally_ = false;
};

class EnetTransport final : public INetTransport {
public:
    EnetTransport() { initialized_ = library_.Ready(); }

    ~EnetTransport() override { Shutdown(); }

    bool StartServer(const NetEndpoint& endpoint, const std::size_t maxPeers) override {
        if (!initialized_ || host_ != nullptr) {
            return false;
        }
        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        // An explicit bind host must actually restrict the listening interface; only
        // wildcard forms fall through to ENET_HOST_ANY.
        const bool bindAny = endpoint.host.empty() || endpoint.host == "0.0.0.0" || endpoint.host == "*" ||
                             endpoint.host == "::";
        if (!bindAny && enet_address_set_host(&address, endpoint.host.c_str()) != 0) {
            ri::core::LogInfo("Net: could not resolve bind host '" + endpoint.host + "'; refusing to start server.");
            return false;
        }
        address.port = endpoint.port;
        host_ = enet_host_create(&address, maxPeers, 2, 0, 0);
        ConfigureHostResourceLimits();
        server_ = host_ != nullptr;
        return host_ != nullptr;
    }

    bool StartClient() override {
        if (!initialized_ || host_ != nullptr) {
            return false;
        }
        host_ = enet_host_create(nullptr, 1, 2, 0, 0);
        ConfigureHostResourceLimits();
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
        if (!IsNetPacketPayloadSizeAllowed(packet.payload.size())) {
            ++stats_.oversizedPacketsRejected;
            stats_.oversizedBytesRejected += static_cast<std::uint64_t>(packet.payload.size());
            return false;
        }
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
        const std::size_t eventBudget = (std::min)(maxPackets, kMaxNetEventsPerPoll);
        out.reserve(eventBudget);
        ENetEvent ev{};
        // Count every serviced event, not only accepted payloads. Otherwise a stream of rejected
        // oversized packets (or connect/disconnect churn) can keep one PollReceive call spinning
        // without honoring its work budget.
        std::size_t servicedEvents = 0U;
        std::size_t acceptedPayloadBytes = 0U;
        static_assert(kMaxNetPayloadBytesPerPoll >= kMaxNetPacketPayloadBytes);
        constexpr std::size_t kLastSafePollStartBytes =
            kMaxNetPayloadBytesPerPoll - kMaxNetPacketPayloadBytes;
        while (servicedEvents < eventBudget
               && acceptedPayloadBytes <= kLastSafePollStartBytes
               && enet_host_service(host_, &ev, 0) > 0) {
            ++servicedEvents;
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
                if (!IsNetPacketPayloadSizeAllowed(ev.packet->dataLength)) {
                    ++stats_.oversizedPacketsRejected;
                    stats_.oversizedBytesRejected += static_cast<std::uint64_t>(ev.packet->dataLength);
                    enet_packet_destroy(ev.packet);
                    continue;
                }
                NetPacket packet{};
                if (server_ && ev.peer != nullptr) {
                    const std::size_t peerId = reinterpret_cast<std::size_t>(ev.peer->data);
                    packet.peerId = peerId > 0U ? peerId - 1U : 0U;
                }
                packet.channel = ev.channelID;
                packet.reliable = (ev.packet->flags & ENET_PACKET_FLAG_RELIABLE) != 0U;
                packet.payload.assign(ev.packet->data, ev.packet->data + ev.packet->dataLength);
                acceptedPayloadBytes += ev.packet->dataLength;
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
    void ConfigureHostResourceLimits() noexcept {
        if (host_ == nullptr) {
            return;
        }
        // ENet's defaults permit a 32 MiB packet to be allocated during fragment reassembly.
        // Lower this before the first service call so RawIron's limit applies pre-allocation.
        host_->maximumPacketSize = kMaxNetPacketPayloadBytes;
        static_assert(kMaxNetPayloadBytesPerPoll > kMaxNetPacketPayloadBytes);
        // ENet tests totalWaitingData before adding the next packet. Subtract one maximum packet
        // so accepted waiting data cannot overshoot the intended aggregate ceiling.
        host_->maximumWaitingData = kMaxNetPayloadBytesPerPoll - kMaxNetPacketPayloadBytes;
    }

    EnetLibraryGuard library_{};
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
