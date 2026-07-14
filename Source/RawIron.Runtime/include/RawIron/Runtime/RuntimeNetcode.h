#pragma once

#include "RawIron/Runtime/LagCompensation.h"
#include "RawIron/Runtime/LatencyTools.h"
#include "RawIron/Runtime/NetModes.h"
#include "RawIron/Runtime/NetTransport.h"
#include "RawIron/Runtime/RendezvousProvider.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Runtime/SnapshotReplication.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <deque>
#include <vector>

namespace ri::runtime {

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

struct AuthoritativeNetConfig {
    NetMode mode = NetMode::ClientOnly;
    NetRole role = NetRole::None;
    int tickRate = 60;
    int serverTickRate = 125;
    int maxPeers = 32;
    NetEndpoint bindEndpoint{};
    NetEndpoint connectEndpoint{};
    NetEndpoint p2pBindEndpoint{.host = "0.0.0.0", .port = 27115};
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

    /// Packet routing helper so games can keep authority and side-channel traffic cleanly separated.
    bool SendPacket(std::size_t peerId, NetPacket packet, NetChannelKind kind);
    [[nodiscard]] std::optional<JoinCodeResolveResult> ResolveJoinCode(const std::string& code);

private:
    [[nodiscard]] static const char* ModeName(NetMode mode) noexcept;
    [[nodiscard]] static const char* MigrationStateName(HostMigrationState state) noexcept;
    void EmitMigrationState(RuntimeContext& context, HostMigrationState newState, std::string reason = {});
    void TickHostMigration(RuntimeContext& context);

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
    std::uint32_t lastBroadcastTick_ = 0;
    double snapshotCadenceAccumulatorSeconds_ = 0.0;
    HostMigrationState migrationState_ = HostMigrationState::Idle;
    int migrationFrames_ = 0;
    std::optional<std::string> activeJoinCode_{};
    std::uint32_t localPredictionTick_ = 0;
    float predictedPositionX_ = 0.0f;
    std::deque<std::pair<std::uint32_t, float>> predictedHistory_{};
};

} // namespace ri::runtime
