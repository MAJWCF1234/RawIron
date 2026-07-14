#include "RawIron/Runtime/RuntimeNetcode.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ri::runtime {
namespace {

constexpr std::uint8_t kSnapshotPacketMarker = 0xA7U;
constexpr std::uint8_t kSnapshotPacketVersion = 1U;
constexpr std::size_t kSnapshotPacketHeaderSize = 14U;
constexpr std::size_t kMaxSnapshotPayloadBytes = 4U * 1024U * 1024U;

void WriteU32(std::vector<std::uint8_t>& out, const std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

bool ReadU32(const std::vector<std::uint8_t>& in, const std::size_t offset, std::uint32_t& value) {
    if (offset > in.size() || in.size() - offset < 4U) {
        return false;
    }
    value = static_cast<std::uint32_t>(in[offset]) |
            (static_cast<std::uint32_t>(in[offset + 1U]) << 8U) |
            (static_cast<std::uint32_t>(in[offset + 2U]) << 16U) |
            (static_cast<std::uint32_t>(in[offset + 3U]) << 24U);
    return true;
}

NetPacket EncodeSnapshotPacket(const SnapshotDeltaPacket& snapshot) {
    NetPacket packet{};
    packet.channel = 1U;
    packet.reliable = false;
    packet.payload.reserve(kSnapshotPacketHeaderSize + snapshot.encodedOps.size());
    packet.payload.push_back(kSnapshotPacketMarker);
    packet.payload.push_back(kSnapshotPacketVersion);
    WriteU32(packet.payload, snapshot.baseTick);
    WriteU32(packet.payload, snapshot.targetTick);
    WriteU32(packet.payload, static_cast<std::uint32_t>(snapshot.encodedOps.size()));
    packet.payload.insert(packet.payload.end(), snapshot.encodedOps.begin(), snapshot.encodedOps.end());
    return packet;
}

std::optional<SnapshotDeltaPacket> DecodeSnapshotPacket(const NetPacket& packet) {
    if (packet.payload.size() < kSnapshotPacketHeaderSize || packet.payload[0] != kSnapshotPacketMarker ||
        packet.payload[1] != kSnapshotPacketVersion) {
        return std::nullopt;
    }
    SnapshotDeltaPacket snapshot{};
    std::uint32_t encodedSize = 0U;
    if (!ReadU32(packet.payload, 2U, snapshot.baseTick) || !ReadU32(packet.payload, 6U, snapshot.targetTick) ||
        !ReadU32(packet.payload, 10U, encodedSize) || encodedSize > kMaxSnapshotPayloadBytes ||
        static_cast<std::size_t>(encodedSize) != packet.payload.size() - kSnapshotPacketHeaderSize) {
        return std::nullopt;
    }
    snapshot.encodedOps.assign(packet.payload.begin() + static_cast<std::ptrdiff_t>(kSnapshotPacketHeaderSize),
                               packet.payload.end());
    return snapshot;
}

std::vector<std::uint8_t> EncodeAuthoritativePosition(const std::uint32_t tick, const float x) {
    const std::int32_t qx = static_cast<std::int32_t>(std::lround(static_cast<double>(x) * 1000.0));
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

bool DecodeAuthoritativePosition(const std::vector<std::uint8_t>& bytes, std::uint32_t& tick, float& x) {
    if (bytes.size() < 8U) {
        return false;
    }
    tick = static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
    const std::int32_t qx = static_cast<std::int32_t>(bytes[4]) |
                            (static_cast<std::int32_t>(bytes[5]) << 8U) |
                            (static_cast<std::int32_t>(bytes[6]) << 16U) |
                            (static_cast<std::int32_t>(bytes[7]) << 24U);
    x = static_cast<float>(static_cast<double>(qx) / 1000.0);
    return true;
}

} // namespace

AuthoritativeNetModule::AuthoritativeNetModule(AuthoritativeNetConfig config,
                                               std::unique_ptr<INetTransport> authorityTransport)
    : config_(std::move(config)),
      authorityTransport_(std::move(authorityTransport)),
      latencySimulator_(0xBADC0DEu),
      netGraph_(512),
      rewindBuffer_(config_.rewindFrames),
      snapshotReplicator_(128) {}

std::string_view AuthoritativeNetModule::Name() const noexcept {
    return "rawiron.runtime.net.authoritative";
}

bool AuthoritativeNetModule::OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine&) {
    if (!config_.enabled) {
        config_.role = NetRole::None;
        ri::core::LogInfo("Net: offline mode enabled; authoritative transport is disabled.");
        EmitMigrationState(context, HostMigrationState::Idle, "offline");
        return true;
    }
    if (authorityTransport_ == nullptr) {
        authorityTransport_ = CreateEnetTransport();
    }
    if (authorityTransport_ == nullptr) {
        context.Fail("AuthoritativeNetModule: networking is unavailable; rebuild with RAWIRON_USE_ENET=ON.");
        return false;
    }
    rendezvous_ = CreateRendezvousProvider(config_.rendezvousProvider);
    if (rendezvous_ != nullptr && !rendezvous_->Startup()) {
        ri::core::LogInfo("Rendezvous provider unavailable; falling back to DirectToken join code provider.");
        rendezvous_ = CreateRendezvousProvider(RendezvousProviderKind::DirectToken);
        if (rendezvous_ == nullptr || !rendezvous_->Startup()) {
            context.Fail("AuthoritativeNetModule: rendezvous provider fallback failed startup.");
            return false;
        }
    }
    latencySimulator_.Configure(config_.latencySimulation);
    if (config_.enableP2PPlane && config_.mode == NetMode::HybridP2P) {
        p2pTransport_ = CreateEnetTransport();
        if (p2pTransport_ == nullptr) {
            context.Fail("AuthoritativeNetModule: P2P side-channel requested but networking is unavailable.");
            return false;
        }
    }

    ri::core::LogInfo(std::string("Net mode: ") + ModeName(config_.mode));

    if (!config_.joinCodeToResolve.empty()) {
        const auto resolved = ResolveJoinCode(config_.joinCodeToResolve);
        if (!resolved.has_value()) {
            context.Fail("AuthoritativeNetModule: failed to resolve join code.");
            return false;
        }
        config_.connectEndpoint = resolved->endpoint;
        ri::core::LogInfo("Net: join code resolved to " + config_.connectEndpoint.host + ":" +
                          std::to_string(config_.connectEndpoint.port));
    }

    switch (config_.mode) {
    case NetMode::Dedicated:
        config_.role = NetRole::DedicatedServer;
        break;
    case NetMode::ListenHost:
    case NetMode::HybridP2P:
        config_.role = NetRole::ListenServer;
        break;
    case NetMode::ClientOnly:
        config_.role = NetRole::Client;
        break;
    }
    if (config_.dedicatedServerFirst && config_.mode == NetMode::ListenHost) {
        ri::core::LogInfo("Net: dedicated-server-first policy active (listen host allowed for dev/local only).");
    }

    switch (config_.role) {
    case NetRole::DedicatedServer:
    case NetRole::ListenServer:
        if (!authorityTransport_->StartServer(config_.bindEndpoint, static_cast<std::size_t>(config_.maxPeers))) {
            context.Fail("AuthoritativeNetModule: failed to start server transport.");
            return false;
        }
        ri::core::LogInfo("Net: authoritative server started on " + config_.bindEndpoint.host + ":" +
                          std::to_string(config_.bindEndpoint.port));
        break;
    case NetRole::Client:
        if (!authorityTransport_->StartClient() || !authorityTransport_->Connect(config_.connectEndpoint)) {
            context.Fail("AuthoritativeNetModule: failed to start/connect client transport.");
            return false;
        }
        ri::core::LogInfo("Net: client connecting to " + config_.connectEndpoint.host + ":" +
                          std::to_string(config_.connectEndpoint.port));
        break;
    case NetRole::None:
        ri::core::LogInfo("Net: role=none; authoritative net module is dormant.");
        break;
    }

    if (p2pTransport_ != nullptr) {
        const bool ok = p2pTransport_->StartServer(config_.p2pBindEndpoint, static_cast<std::size_t>(config_.maxPeers));
        if (!ok) {
            context.Fail("AuthoritativeNetModule: failed to start P2P side-channel transport.");
            return false;
        }
        ri::core::LogInfo("Net: P2P side-channel listening on " + config_.p2pBindEndpoint.host + ":" +
                          std::to_string(config_.p2pBindEndpoint.port));
    }

    if (config_.issueJoinCodeOnStartup && rendezvous_ != nullptr &&
        (config_.role == NetRole::DedicatedServer || config_.role == NetRole::ListenServer)) {
        JoinCodeIssueRequest req{};
        req.hostEndpoint = config_.bindEndpoint;
        req.mode = config_.mode;
        req.ttlSeconds = 3600;
        activeJoinCode_ = rendezvous_->IssueJoinCode(req);
        if (activeJoinCode_.has_value()) {
            RuntimeEvent codeEv{};
            codeEv.fields["code"] = *activeJoinCode_;
            context.Events().Emit("net.join_code.issued", std::move(codeEv));
            ri::core::LogInfo("Net: join code issued: " + *activeJoinCode_);
        }
    }

    EmitMigrationState(context, HostMigrationState::Idle, "startup");
    return true;
}

bool AuthoritativeNetModule::OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame) {
    if (config_.role == NetRole::None || authorityTransport_ == nullptr) {
        return true;
    }

    const std::uint64_t nowMs = static_cast<std::uint64_t>(std::max(0.0, frame.realtimeSeconds * 1000.0));
    if (config_.role == NetRole::Client) {
        const float dt = static_cast<float>(frame.deltaSeconds);
        const float syntheticInput = std::sin(static_cast<float>(frame.frameIndex) * 0.05f);
        predictedPositionX_ += syntheticInput * dt * 6.0f;
        ++localPredictionTick_;
        predictedHistory_.push_back({localPredictionTick_, predictedPositionX_});
        while (predictedHistory_.size() > 512U) {
            predictedHistory_.pop_front();
        }
    }

    const auto inbound = authorityTransport_->PollReceive(128);
    serverTelemetry_.inboundPackets += inbound.size();
    for (const NetPacket& p : inbound) {
        (void)latencySimulator_.Enqueue(nowMs, p);
    }
    while (const auto ready = latencySimulator_.TryPopReady(nowMs)) {
        if (config_.role == NetRole::Client) {
            const auto snapshotPacket = DecodeSnapshotPacket(*ready);
            if (!snapshotPacket.has_value()) {
                RuntimeEvent ignored{};
                ignored.fields["peer"] = std::to_string(ready->peerId);
                ignored.fields["reason"] = "unknown_or_malformed_packet";
                context.Events().Emit("net.packet.ignored", std::move(ignored));
                continue;
            }
            const SnapshotBlob fallback{};
            const auto rebuilt = snapshotReplicator_.ApplyFromServer(ready->peerId, fallback, *snapshotPacket);
            if (!rebuilt.has_value()) {
                RuntimeEvent rejected{};
                rejected.fields["peer"] = std::to_string(ready->peerId);
                rejected.fields["tick"] = std::to_string(snapshotPacket->targetTick);
                context.Events().Emit("net.snapshot.rejected", std::move(rejected));
                continue;
            }
            std::uint32_t authTick = 0U;
            float authX = 0.0f;
            if (!DecodeAuthoritativePosition(rebuilt->bytes, authTick, authX)) {
                continue;
            }
            auto it = std::find_if(predictedHistory_.begin(), predictedHistory_.end(),
                                   [authTick](const auto& state) { return state.first == authTick; });
            if (it != predictedHistory_.end()) {
                const double error = std::abs(static_cast<double>(it->second - authX));
                predictionTelemetry_.avgPositionError =
                    (predictionTelemetry_.avgPositionError * 0.9) + (error * 0.1);
                predictionTelemetry_.maxPositionError = std::max(predictionTelemetry_.maxPositionError, error);
                if (error > 0.01) {
                    ++predictionTelemetry_.correctionCount;
                    predictedPositionX_ = authX;
                }
                ++predictionTelemetry_.reconciledFrames;
            }
            RuntimeEvent applied{};
            applied.fields["peer"] = std::to_string(ready->peerId);
            applied.fields["tick"] = std::to_string(authTick);
            context.Events().Emit("net.snapshot.applied", std::move(applied));
        } else {
            RuntimeEvent command{};
            command.fields["peer"] = std::to_string(ready->peerId);
            command.fields["channel"] = std::to_string(ready->channel);
            command.fields["bytes"] = std::to_string(ready->payload.size());
            context.Events().Emit("net.command.received", std::move(command));
        }
    }

    if (config_.role == NetRole::DedicatedServer || config_.role == NetRole::ListenServer) {
        // Time-based server snapshot cadence.
        snapshotCadenceAccumulatorSeconds_ += frame.deltaSeconds;
        const double snapshotStep = 1.0 / static_cast<double>(std::max(1, config_.serverTickRate));
        while (snapshotCadenceAccumulatorSeconds_ >= snapshotStep) {
            snapshotCadenceAccumulatorSeconds_ -= snapshotStep;
            lastBroadcastTick_ = static_cast<std::uint32_t>(frame.frameIndex);
            serverTelemetry_.lastSnapshotTick = lastBroadcastTick_;
            SnapshotBlob snapshot{};
            snapshot.tick = static_cast<std::uint32_t>(frame.frameIndex);
            const float authorityX = static_cast<float>(std::sin(static_cast<float>(frame.frameIndex) * 0.05f) * 10.0f);
            snapshot.bytes = EncodeAuthoritativePosition(snapshot.tick, authorityX);
            bool usedDelta = false;
            bool broadcasted = false;
            for (const std::size_t peerId : authorityTransport_->ConnectedPeers()) {
                bool usedPeerDelta = false;
                const SnapshotDeltaPacket packet = snapshotReplicator_.BuildForPeer(peerId, snapshot, usedPeerDelta);
                if (authorityTransport_->Send(peerId, EncodeSnapshotPacket(packet))) {
                    ++serverTelemetry_.outboundPackets;
                    broadcasted = true;
                    usedDelta = usedDelta || usedPeerDelta;
                }
            }
            if (broadcasted) {
                ++serverTelemetry_.snapshotsBroadcast;
            }
            RuntimeEvent ev{};
            ev.fields["tick"] = std::to_string(frame.frameIndex);
            ev.fields["role"] = "server";
            ev.fields["snapshot_count"] = std::to_string(serverTelemetry_.snapshotsBroadcast);
            ev.fields["snapshot_mode"] = usedDelta ? "delta" : "full";
            context.Events().Emit("net.snapshot.broadcast", std::move(ev));
        } 
    }

    if (p2pTransport_ != nullptr) {
        const auto p2pInbound = p2pTransport_->PollReceive(128);
        if (!p2pInbound.empty()) {
            RuntimeEvent p2pEv{};
            p2pEv.fields["packets"] = std::to_string(p2pInbound.size());
            context.Events().Emit("net.p2p.received", std::move(p2pEv));
        }
    }

    TickHostMigration(context);

    RuntimeEvent metrics{};
    const NetTransportStats stats = authorityTransport_->Stats();
    const auto snapshotStats = snapshotReplicator_.Stats();
    predictionTelemetry_.avgPositionError = predictionTelemetry_.correctionCount == 0
        ? 0.0
        : (predictionTelemetry_.avgPositionError * 0.98) + 0.02;
    NetGraphSample ng{};
    ng.tick = static_cast<std::uint32_t>(frame.frameIndex);
    ng.rttMs = static_cast<double>(config_.latencySimulation.baseDelayMs * 2);
    ng.jitterMs = static_cast<double>(config_.latencySimulation.jitterMs);
    ng.predictionError = predictionTelemetry_.avgPositionError;
    ng.packetLossPercent = config_.latencySimulation.packetLossPercent;
    netGraph_.Push(ng);
    metrics.fields["sent_packets"] = std::to_string(stats.packetsSent);
    metrics.fields["recv_packets"] = std::to_string(stats.packetsReceived);
    metrics.fields["dropped_sim"] = std::to_string(latencySimulator_.DroppedPackets());
    metrics.fields["snapshot_full"] = std::to_string(snapshotStats.fullSnapshots);
    metrics.fields["snapshot_delta"] = std::to_string(snapshotStats.deltaSnapshots);
    metrics.fields["snapshot_full_bytes"] = std::to_string(snapshotStats.bytesFull);
    metrics.fields["snapshot_delta_bytes"] = std::to_string(snapshotStats.bytesDelta);
    metrics.fields["snapshot_broadcasts"] = std::to_string(serverTelemetry_.snapshotsBroadcast);
    metrics.fields["inbound_packets"] = std::to_string(serverTelemetry_.inboundPackets);
    metrics.fields["last_snapshot_tick"] = std::to_string(serverTelemetry_.lastSnapshotTick);
    if (const auto latest = netGraph_.Latest(); latest.has_value()) {
        metrics.fields["netgraph_rtt_ms"] = std::to_string(latest->rttMs);
        metrics.fields["netgraph_jitter_ms"] = std::to_string(latest->jitterMs);
        metrics.fields["netgraph_loss_pct"] = std::to_string(latest->packetLossPercent);
        metrics.fields["prediction_error"] = std::to_string(latest->predictionError);
    }
    metrics.fields["mode"] = ModeName(config_.mode);
    metrics.fields["migration_state"] = MigrationStateName(migrationState_);
    context.Events().Emit("net.metrics", std::move(metrics));
    return true;
}

void AuthoritativeNetModule::OnRuntimeShutdown(RuntimeContext&) {
    if (authorityTransport_ != nullptr) {
        authorityTransport_->Shutdown();
    }
    if (p2pTransport_ != nullptr) {
        p2pTransport_->Shutdown();
    }
    if (rendezvous_ != nullptr) {
        rendezvous_->Shutdown();
    }
}

const AuthoritativeNetConfig& AuthoritativeNetModule::Config() const noexcept {
    return config_;
}

const PredictionTelemetry& AuthoritativeNetModule::PredictionStats() const noexcept {
    return predictionTelemetry_;
}

const ServerNetTelemetry& AuthoritativeNetModule::ServerStats() const noexcept {
    return serverTelemetry_;
}

HostMigrationState AuthoritativeNetModule::MigrationState() const noexcept {
    return migrationState_;
}

std::optional<std::string> AuthoritativeNetModule::ActiveJoinCode() const {
    return activeJoinCode_;
}

bool AuthoritativeNetModule::SendPacket(const std::size_t peerId, NetPacket packet, const NetChannelKind kind) {
    if (kind == NetChannelKind::Authority) {
        const bool ok = authorityTransport_ != nullptr && authorityTransport_->Send(peerId, packet);
        if (ok) {
            serverTelemetry_.outboundPackets += 1;
        }
        return ok;
    }
    if (p2pTransport_ != nullptr) {
        const bool ok = p2pTransport_->Send(peerId, packet);
        if (ok) {
            serverTelemetry_.outboundPackets += 1;
        }
        return ok;
    }
    // Fallback: if no P2P plane, route through authority transport reliably false.
    const bool ok = authorityTransport_ != nullptr && authorityTransport_->Send(peerId, packet);
    if (ok) {
        serverTelemetry_.outboundPackets += 1;
    }
    return ok;
}

std::optional<JoinCodeResolveResult> AuthoritativeNetModule::ResolveJoinCode(const std::string& code) {
    if (rendezvous_ == nullptr || code.empty()) {
        return std::nullopt;
    }
    return rendezvous_->ResolveJoinCode(code);
}

const char* AuthoritativeNetModule::ModeName(const NetMode mode) noexcept {
    switch (mode) {
    case NetMode::Dedicated: return "dedicated";
    case NetMode::ListenHost: return "listen_host";
    case NetMode::HybridP2P: return "hybrid_p2p";
    case NetMode::ClientOnly: return "client_only";
    }
    return "unknown";
}

const char* AuthoritativeNetModule::MigrationStateName(const HostMigrationState state) noexcept {
    switch (state) {
    case HostMigrationState::Idle: return "idle";
    case HostMigrationState::CandidateElection: return "candidate_election";
    case HostMigrationState::SnapshotHandoff: return "snapshot_handoff";
    case HostMigrationState::Rebind: return "rebind";
    case HostMigrationState::Complete: return "complete";
    case HostMigrationState::Failed: return "failed";
    }
    return "unknown";
}

void AuthoritativeNetModule::EmitMigrationState(RuntimeContext& context,
                                                const HostMigrationState newState,
                                                std::string reason) {
    migrationState_ = newState;
    RuntimeEvent ev{};
    ev.fields["state"] = MigrationStateName(newState);
    ev.fields["reason"] = std::move(reason);
    context.Events().Emit("net.host_migration.state", std::move(ev));
}

void AuthoritativeNetModule::TickHostMigration(RuntimeContext& context) {
    if (!config_.enableHostMigration || config_.mode == NetMode::Dedicated || config_.mode == NetMode::ClientOnly) {
        return;
    }
    if (migrationState_ == HostMigrationState::Idle) {
        return;
    }
    ++migrationFrames_;
    if (migrationState_ == HostMigrationState::CandidateElection && migrationFrames_ > 5) {
        migrationFrames_ = 0;
        EmitMigrationState(context, HostMigrationState::SnapshotHandoff, "election_resolved");
    } else if (migrationState_ == HostMigrationState::SnapshotHandoff && migrationFrames_ > 5) {
        migrationFrames_ = 0;
        EmitMigrationState(context, HostMigrationState::Rebind, "snapshot_sent");
    } else if (migrationState_ == HostMigrationState::Rebind && migrationFrames_ > 5) {
        migrationFrames_ = 0;
        EmitMigrationState(context, HostMigrationState::Complete, "rebound");
    } else if (migrationState_ == HostMigrationState::Complete) {
        EmitMigrationState(context, HostMigrationState::Idle, "settled");
    }
}

} // namespace ri::runtime
