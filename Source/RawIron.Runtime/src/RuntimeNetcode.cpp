#include "RawIron/Runtime/RuntimeNetcode.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"

#include <algorithm>
#include <cmath>

namespace ri::runtime {
namespace {

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

AuthoritativeNetModule::AuthoritativeNetModule(AuthoritativeNetConfig config)
    : config_(std::move(config)),
      latencySimulator_(0xBADC0DEu),
      netGraph_(512),
      rewindBuffer_(config_.rewindFrames),
      snapshotReplicator_(128) {}

std::string_view AuthoritativeNetModule::Name() const noexcept {
    return "rawiron.runtime.net.authoritative";
}

bool AuthoritativeNetModule::OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine&) {
    authorityTransport_ = CreateEnetTransport();
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
    const auto inbound = authorityTransport_->PollReceive(128);
    serverTelemetry_.inboundPackets += inbound.size();
    for (const NetPacket& p : inbound) {
        (void)latencySimulator_.Enqueue(nowMs, p);
    }
    while (latencySimulator_.TryPopReady(nowMs).has_value()) {
        // Placeholder: hook command processing / snapshot ingest here.
    }

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

    if (config_.role == NetRole::DedicatedServer || config_.role == NetRole::ListenServer) {
        // Time-based server snapshot cadence.
        snapshotCadenceAccumulatorSeconds_ += frame.deltaSeconds;
        const double snapshotStep = 1.0 / static_cast<double>(std::max(1, config_.serverTickRate));
        while (snapshotCadenceAccumulatorSeconds_ >= snapshotStep) {
            snapshotCadenceAccumulatorSeconds_ -= snapshotStep;
            lastBroadcastTick_ = static_cast<std::uint32_t>(frame.frameIndex);
            serverTelemetry_.snapshotsBroadcast += 1;
            serverTelemetry_.lastSnapshotTick = lastBroadcastTick_;
            SnapshotBlob snapshot{};
            snapshot.tick = static_cast<std::uint32_t>(frame.frameIndex);
            const float authorityX = static_cast<float>(std::sin(static_cast<float>(frame.frameIndex) * 0.05f) * 10.0f);
            snapshot.bytes = EncodeAuthoritativePosition(snapshot.tick, authorityX);
            bool usedDelta = false;
            const SnapshotDeltaPacket packet = snapshotReplicator_.BuildForPeer(0U, snapshot, usedDelta);
            if (config_.role == NetRole::ListenServer) {
                const SnapshotBlob fallback{};
                const auto rebuilt = snapshotReplicator_.ApplyFromServer(1U, fallback, packet);
                if (rebuilt.has_value()) {
                    std::uint32_t authTick = 0;
                    float authX = 0.0f;
                    if (DecodeAuthoritativePosition(rebuilt->bytes, authTick, authX)) {
                        auto it = std::find_if(predictedHistory_.begin(), predictedHistory_.end(),
                                               [authTick](const auto& s) { return s.first == authTick; });
                        if (it != predictedHistory_.end()) {
                            const double err = std::abs(static_cast<double>(it->second - authX));
                            predictionTelemetry_.avgPositionError =
                                (predictionTelemetry_.avgPositionError * 0.9) + (err * 0.1);
                            predictionTelemetry_.maxPositionError =
                                std::max(predictionTelemetry_.maxPositionError, err);
                            if (err > 0.01) {
                                ++predictionTelemetry_.correctionCount;
                                predictedPositionX_ = authX;
                            }
                            ++predictionTelemetry_.reconciledFrames;
                        }
                    }
                }
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
