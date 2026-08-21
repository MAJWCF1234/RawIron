#pragma once

#include "RawIron/Runtime/LagCompensation.h"
#include "RawIron/Runtime/LatencyTools.h"
#include "RawIron/Runtime/NetModes.h"
#include "RawIron/Runtime/NetTransport.h"
#include "RawIron/Runtime/RendezvousProvider.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Runtime/SnapshotReplication.h"
#include "RawIron/Core/SessionExtensions.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::runtime {

enum class SessionExtensionPeerState : std::uint8_t {
    NotRequired = 0,
    Pending,
    Accepted,
    Rejected,
};

enum class NetChannelKind : std::uint8_t {
    Authority = 0, // gameplay-critical: movement, combat, inventory
    P2P,           // non-critical: voice, cosmetics, social
};

enum class HostMigrationState : std::uint8_t {
    Idle = 0,
    CandidateElection,
    SnapshotHandoff,
    Rebind,
    Complete,
    Failed,
};

/// Game-owned authority contract mounted into the transport layer. The runtime retains packet
/// limits, session agreement, baselines and resynchronisation; games own their domain payloads.
class IAuthoritativeSimulationBridge {
public:
    virtual ~IAuthoritativeSimulationBridge() = default;

    [[nodiscard]] virtual std::optional<SnapshotBlob> CaptureSnapshot(std::uint32_t tick) = 0;
    /// Must leave the world unchanged when returning false.
    virtual bool ApplySnapshot(const SnapshotBlob& snapshot, std::string* error) = 0;
    /// `peerId` originates at the transport and is the only trusted ownership identity.
    virtual bool HandleCommand(std::size_t peerId,
                               std::uint32_t channel,
                               std::span<const std::uint8_t> payload,
                               std::string* error) = 0;
};

struct AuthoritativeNetConfig {
    NetMode mode = NetMode::ClientOnly;
    NetRole role = NetRole::None;
    int tickRate = 60;
    int serverTickRate = 125;
    int maxPeers = 32;
    NetEndpoint bindEndpoint{};
    NetEndpoint connectEndpoint{};
    NetEndpoint p2pBindEndpoint{.host = "0.0.0.0", .port = 27115};
    /// Optional override for join-code advertising (from `--advertise-host`).
    /// When empty, unreachable bind hosts (0.0.0.0 / loopback) are replaced with a LAN IP.
    std::string advertiseHost;
    LatencySimulationConfig latencySimulation{};
    std::size_t rewindFrames = 128;
    bool enableP2PPlane = false;
    /// Explicitly disables all networking for local/offline runtime workflows.
    bool enabled = true;
    bool dedicatedServerFirst = true;
    bool enableHostMigration = true;
    RendezvousProviderKind rendezvousProvider = RendezvousProviderKind::EpicOnlineServices;
    bool issueJoinCodeOnStartup = false;
    std::string joinCodeToResolve;
    /// When enabled, every authority-plane peer must accept this exact package/extension contract
    /// before it can send gameplay commands or receive simulation snapshots.
    bool requireSessionExtensionAgreement = false;
    ri::core::SessionExtensionContract sessionExtensionContract{};
    /// Optional domain bridge. Without it the legacy deterministic position stream remains for
    /// the networking sandbox and its compatibility tests.
    std::shared_ptr<IAuthoritativeSimulationBridge> simulationBridge{};
};

struct PredictionTelemetry {
    double avgPositionError = 0.0;
    double maxPositionError = 0.0;
    std::uint64_t correctionCount = 0;
    std::uint64_t reconciledFrames = 0;
};

struct ServerNetTelemetry {
    std::uint64_t snapshotsBroadcast = 0;
    std::uint64_t inboundPackets = 0;
    std::uint64_t outboundPackets = 0;
    std::uint64_t simulatedDrops = 0;
    std::uint64_t oversizedInboundPacketsRejected = 0;
    std::uint64_t inboundPacketsDroppedByQueueBudget = 0;
    std::uint64_t inboundPacketsDroppedByPollBudget = 0;
    std::uint64_t oversizedOutboundPacketsRejected = 0;
    /// Packets dropped because the peer is under an escalating protocol-offense cooldown.
    std::uint64_t inboundPacketsDroppedByPeerCooldown = 0;
    std::uint32_t lastSnapshotTick = 0;
};

class AuthoritativeNetModule final : public RuntimeModule {
public:
    explicit AuthoritativeNetModule(AuthoritativeNetConfig config,
                                    std::unique_ptr<INetTransport> authorityTransport = {});

    [[nodiscard]] std::string_view Name() const noexcept override;
    bool OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine& commandLine) override;
    bool OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame) override;
    void OnRuntimeShutdown(RuntimeContext& context) override;

    [[nodiscard]] const AuthoritativeNetConfig& Config() const noexcept;
    [[nodiscard]] const PredictionTelemetry& PredictionStats() const noexcept;
    [[nodiscard]] const ServerNetTelemetry& ServerStats() const noexcept;
    [[nodiscard]] HostMigrationState MigrationState() const noexcept;
    [[nodiscard]] std::optional<std::string> ActiveJoinCode() const;
    [[nodiscard]] SessionExtensionPeerState SessionExtensionState() const noexcept;
    [[nodiscard]] SessionExtensionPeerState PeerSessionExtensionState(std::size_t peerId) const noexcept;

    /// Packet routing helper so games can keep authority and side-channel traffic cleanly separated.
    bool SendPacket(std::size_t peerId, const NetPacket& packet, NetChannelKind kind);
    [[nodiscard]] std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string& code);

private:
    [[nodiscard]] static const char* ModeName(NetMode mode) noexcept;
    [[nodiscard]] static const char* MigrationStateName(HostMigrationState state) noexcept;
    void EmitMigrationState(RuntimeContext& context, HostMigrationState newState, std::string reason = {});
    void TickHostMigration(RuntimeContext& context);

    struct PeerOffenseState {
        std::uint32_t strikes = 0;
        std::uint64_t cooldownUntilMs = 0;
    };

    AuthoritativeNetConfig config_{};
    std::unique_ptr<INetTransport> authorityTransport_{};
    std::unique_ptr<INetTransport> p2pTransport_{};
    std::unique_ptr<IRendezvousProvider> rendezvous_{};
    LatencySimulator latencySimulator_{};
    NetGraphTracker netGraph_{};
    RewindBuffer rewindBuffer_{};
    SnapshotReplicator snapshotReplicator_{};
    PredictionTelemetry predictionTelemetry_{};
    ServerNetTelemetry serverTelemetry_{};
    /// Authority-plane protocol offenders. Never applied on clients (the peer is the host).
    std::unordered_map<std::size_t, PeerOffenseState> peerOffenses_{};
    /// P2P-plane protocol offenders; kept separate so a side-channel strike cannot mute gameplay.
    std::unordered_map<std::size_t, PeerOffenseState> p2pPeerOffenses_{};
    /// Last accepted client resync request per peer, to bound unauthenticated full-snapshot spam.
    std::unordered_map<std::size_t, std::uint64_t> peerResyncAcceptedMs_{};
    std::uint32_t lastBroadcastTick_ = 0;
    double snapshotCadenceAccumulatorSeconds_ = 0.0;
    HostMigrationState migrationState_ = HostMigrationState::Idle;
    int migrationFrames_ = 0;
    std::optional<std::string> activeJoinCode_{};
    std::uint32_t localPredictionTick_ = 0;
    float predictedPositionX_ = 0.0f;
    std::deque<std::pair<std::uint32_t, float>> predictedHistory_{};
    std::unordered_map<std::size_t, SessionExtensionPeerState> sessionExtensionPeers_{};
    SessionExtensionPeerState localSessionExtensionState_ = SessionExtensionPeerState::NotRequired;
};

} // namespace ri::runtime
