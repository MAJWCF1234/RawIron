#include "RawIron/Runtime/RuntimeNetcode.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"

#include "SessionExtensionProtocol.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <unordered_set>

namespace ri::runtime {
namespace {

constexpr std::uint8_t kSnapshotPacketMarker = 0xA7U;
constexpr std::uint8_t kSnapshotPacketVersion = 2U;
constexpr std::size_t kSnapshotPacketHeaderSize = 18U;
constexpr std::uint8_t kSnapshotResyncMarker = 0xA8U;
constexpr std::uint8_t kSnapshotResyncVersion = 1U;
constexpr std::size_t kSnapshotResyncPacketSize = 2U;
constexpr std::size_t kMaxSnapshotPayloadBytes = 4U * 1024U * 1024U;
constexpr int kMaxSnapshotCatchUpSteps = 4;
constexpr std::size_t kRuntimeTransportEventBudget = 128U;
constexpr std::size_t kRuntimeReadyPacketBudget = 128U;
constexpr std::uint64_t kPeerCooldownBaseMs = 250U;
constexpr std::uint64_t kPeerCooldownMaxMs = 8000U;
constexpr std::uint32_t kPeerCooldownShiftCap = 5U;
constexpr std::uint64_t kSnapshotResyncMinIntervalMs = 250U;

[[nodiscard]] bool IsAuthorityHostRole(const NetRole role) noexcept {
    return role == NetRole::DedicatedServer || role == NetRole::ListenServer;
}

[[nodiscard]] bool IsSessionExtensionHostRole(const NetRole role) noexcept {
    return role == NetRole::DedicatedServer || role == NetRole::ListenServer;
}

[[nodiscard]] bool IsProtocolOffenseReason(const std::string_view reason) noexcept {
    return reason == "payload_too_large"
        || reason == "p2p_payload_too_large"
        || reason == "unknown_or_malformed_packet";
}

[[nodiscard]] bool IsP2pProtocolOffenseReason(const std::string_view reason) noexcept {
    return reason == "p2p_payload_too_large";
}

void SaturatingAdd(std::uint64_t& counter, const std::uint64_t amount) noexcept {
    counter = amount > std::numeric_limits<std::uint64_t>::max() - counter
        ? std::numeric_limits<std::uint64_t>::max()
        : counter + amount;
}

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

std::optional<NetPacket> EncodeSnapshotPacket(const SnapshotDeltaPacket& snapshot) {
    if (snapshot.encodedOps.size() > kMaxSnapshotPayloadBytes) {
        return std::nullopt;
    }
    NetPacket packet{};
    packet.channel = 1U;
    // Reliable delivery keeps RememberPeerBaseline coherent: Send success must mean the
    // peer can receive the baseline we just recorded (lossy UDP snapshots desync deltas).
    packet.reliable = true;
    packet.payload.reserve(kSnapshotPacketHeaderSize + snapshot.encodedOps.size());
    packet.payload.push_back(kSnapshotPacketMarker);
    packet.payload.push_back(kSnapshotPacketVersion);
    WriteU32(packet.payload, snapshot.baseTick);
    WriteU32(packet.payload, snapshot.targetTick);
    WriteU32(packet.payload, snapshot.payloadChecksum);
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
        !ReadU32(packet.payload, 10U, snapshot.payloadChecksum) || !ReadU32(packet.payload, 14U, encodedSize)
        || encodedSize > kMaxSnapshotPayloadBytes ||
        static_cast<std::size_t>(encodedSize) != packet.payload.size() - kSnapshotPacketHeaderSize) {
        return std::nullopt;
    }
    snapshot.encodedOps.assign(packet.payload.begin() + static_cast<std::ptrdiff_t>(kSnapshotPacketHeaderSize),
                               packet.payload.end());
    return snapshot;
}

[[nodiscard]] NetPacket EncodeSnapshotResyncRequest() {
    NetPacket packet{};
    packet.channel = 1U;
    packet.reliable = true;
    packet.payload = {kSnapshotResyncMarker, kSnapshotResyncVersion};
    return packet;
}

[[nodiscard]] bool IsSnapshotResyncRequest(const NetPacket& packet) noexcept {
    return packet.payload.size() == kSnapshotResyncPacketSize
        && packet.payload[0] == kSnapshotResyncMarker
        && packet.payload[1] == kSnapshotResyncVersion;
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
    // EncodeAuthoritativePosition always emits exactly 8 bytes; reject trailing garbage.
    if (bytes.size() != 8U) {
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
    if (config_.requireSessionExtensionAgreement) {
        const ri::core::SessionExtensionValidationReport report =
            ri::core::NormalizeSessionExtensionContract(config_.sessionExtensionContract);
        if (!report.valid) {
            context.Fail("AuthoritativeNetModule: invalid session-extension contract: "
                         + (report.issues.empty() ? std::string("unknown validation failure") : report.issues.front()));
            return false;
        }
        localSessionExtensionState_ = SessionExtensionPeerState::Pending;
    }
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
        ri::core::LogInfo("Net: resolving join code (this can take a few seconds)…");
        const auto resolved = ResolveJoinCode(config_.joinCodeToResolve);
        if (!resolved.has_value()) {
            // Nothing is left to advertise or resolve either way. Rollback only unwinds modules
            // that already started, so this module has to release its own rendezvous session
            // rather than leave an EOS platform ticking for a runtime that is no longer networked.
            if (rendezvous_ != nullptr) {
                rendezvous_->Shutdown();
                rendezvous_.reset();
            }
            const std::string advice =
                "Net: failed to resolve join code. "
                "Try --rendezvous=direct --join-code=RI1:host:port:mode on LAN.";
            // A player who asked to join a specific session must never be dropped into a silent
            // singleplayer world: a client has nothing to do offline, so this is fatal. Hosts,
            // which only carry a join code incidentally, keep the soft fallback.
            if (config_.mode == NetMode::ClientOnly) {
                context.Fail(advice);
                return false;
            }
            ri::core::LogInfo(advice + " Continuing offline.");
            config_.enabled = false;
            config_.role = NetRole::None;
            EmitMigrationState(context, HostMigrationState::Idle, "join_resolve_failed");
            return true;
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
        req.hostEndpoint = ResolveAdvertiseEndpoint(config_.bindEndpoint, config_.advertiseHost);
        req.mode = config_.mode;
        req.ttlSeconds = 3600;
        ri::core::LogInfo("Net: advertising join endpoint " + req.hostEndpoint.host + ":" +
                          std::to_string(req.hostEndpoint.port));
        activeJoinCode_ = rendezvous_->IssueJoinCode(req);
        if (activeJoinCode_.has_value()) {
            RuntimeEvent codeEv{};
            codeEv.fields["code"] = *activeJoinCode_;
            codeEv.fields["advertise_host"] = req.hostEndpoint.host;
            codeEv.fields["advertise_port"] = std::to_string(req.hostEndpoint.port);
            context.Events().Emit("net.join_code.issued", std::move(codeEv));
            ri::core::LogInfo("Net: join code issued: " + *activeJoinCode_);
        } else {
            ri::core::LogInfo("Net: join code issuance failed (check EOS credentials / rendezvous provider).");
        }
    }

    EmitMigrationState(context, HostMigrationState::Idle, "startup");
    return true;
}

bool AuthoritativeNetModule::OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame) {
    if (rendezvous_ != nullptr) {
        rendezvous_->Tick();
    }
    if (config_.role == NetRole::None || authorityTransport_ == nullptr) {
        return true;
    }

    const std::vector<std::size_t> connectedPeers = IsSessionExtensionHostRole(config_.role)
        ? authorityTransport_->ConnectedPeers()
        : std::vector<std::size_t>{};
    if (config_.requireSessionExtensionAgreement && IsSessionExtensionHostRole(config_.role)) {
        for (const std::size_t peerId : connectedPeers) {
            const auto [state, inserted] = sessionExtensionPeers_.try_emplace(peerId, SessionExtensionPeerState::Pending);
            if ((inserted || state->second == SessionExtensionPeerState::Pending)) {
                const std::optional<NetPacket> offer =
                    session_protocol::EncodeOffer(config_.sessionExtensionContract);
                if (offer.has_value()) {
                    static_cast<void>(authorityTransport_->Send(peerId, *offer));
                }
            }
        }
    }

    const std::uint64_t nowMs = static_cast<std::uint64_t>(std::max(0.0, frame.realtimeSeconds * 1000.0));
    if (config_.role == NetRole::Client
        && (!config_.requireSessionExtensionAgreement
            || localSessionExtensionState_ == SessionExtensionPeerState::Accepted)) {
        const float dt = static_cast<float>(frame.deltaSeconds);
        const float syntheticInput = std::sin(static_cast<float>(frame.frameIndex) * 0.05f);
        predictedPositionX_ += syntheticInput * dt * 6.0f;
        ++localPredictionTick_;
        predictedHistory_.push_back({localPredictionTick_, predictedPositionX_});
        while (predictedHistory_.size() > 512U) {
            predictedHistory_.pop_front();
        }
    }

    auto noteProtocolOffense = [this, &context, nowMs](const std::size_t peerId,
                                                       const std::string_view reason) {
        // Cooldowns defend the authority against hostile clients. On a client the
        // inbound peer is the host — striking it would mute snapshots.
        if (!IsAuthorityHostRole(config_.role) || !IsProtocolOffenseReason(reason)) {
            return;
        }
        auto& offenses = IsP2pProtocolOffenseReason(reason) ? p2pPeerOffenses_ : peerOffenses_;
        PeerOffenseState& state = offenses[peerId];
        if (state.strikes < std::numeric_limits<std::uint32_t>::max()) {
            ++state.strikes;
        }
        const std::uint32_t shift = std::min(state.strikes - 1U, kPeerCooldownShiftCap);
        const std::uint64_t durationMs = std::min(kPeerCooldownMaxMs, kPeerCooldownBaseMs << shift);
        state.cooldownUntilMs = nowMs + durationMs;
        RuntimeEvent cooldown{};
        cooldown.fields["peer"] = std::to_string(peerId);
        cooldown.fields["reason"] = std::string(reason);
        cooldown.fields["strikes"] = std::to_string(state.strikes);
        cooldown.fields["cooldown_ms"] = std::to_string(durationMs);
        context.Events().Emit("net.peer.cooldown", std::move(cooldown));
    };

    auto emitIgnoredPacket = [&context, &noteProtocolOffense](const std::size_t peerId,
                                                              const std::size_t payloadBytes,
                                                              const std::string_view reason) {
        RuntimeEvent ignored{};
        ignored.fields["peer"] = std::to_string(peerId);
        ignored.fields["bytes"] = std::to_string(payloadBytes);
        ignored.fields["reason"] = std::string(reason);
        context.Events().Emit("net.packet.ignored", std::move(ignored));
        noteProtocolOffense(peerId, reason);
    };

    auto peerIsCoolingDown = [this, nowMs](const std::size_t peerId, const bool p2pPlane) -> bool {
        if (!IsAuthorityHostRole(config_.role)) {
            return false;
        }
        const auto& offenses = p2pPlane ? p2pPeerOffenses_ : peerOffenses_;
        const auto it = offenses.find(peerId);
        return it != offenses.end() && nowMs < it->second.cooldownUntilMs;
    };

    auto inbound = authorityTransport_->PollReceive(kRuntimeTransportEventBudget);
    SaturatingAdd(serverTelemetry_.inboundPackets, static_cast<std::uint64_t>(inbound.size()));
    const std::size_t inboundInspectionCount = std::min(inbound.size(), kRuntimeTransportEventBudget);
    if (inbound.size() > inboundInspectionCount) {
        SaturatingAdd(
            serverTelemetry_.inboundPacketsDroppedByPollBudget,
            static_cast<std::uint64_t>(inbound.size() - inboundInspectionCount));
        RuntimeEvent truncated{};
        truncated.fields["plane"] = "authority";
        truncated.fields["returned"] = std::to_string(inbound.size());
        truncated.fields["accepted_for_inspection"] = std::to_string(inboundInspectionCount);
        context.Events().Emit("net.poll.truncated", std::move(truncated));
    }
    std::size_t inspectedPayloadBytes = 0U;
    for (std::size_t packetIndex = 0U; packetIndex < inboundInspectionCount; ++packetIndex) {
        NetPacket& packet = inbound[packetIndex];
        if (peerIsCoolingDown(packet.peerId, false)) {
            SaturatingAdd(serverTelemetry_.inboundPacketsDroppedByPeerCooldown, 1U);
            RuntimeEvent ignored{};
            ignored.fields["peer"] = std::to_string(packet.peerId);
            ignored.fields["bytes"] = std::to_string(packet.payload.size());
            ignored.fields["reason"] = "peer_cooldown";
            context.Events().Emit("net.packet.ignored", std::move(ignored));
            continue;
        }
        if (!IsNetPacketPayloadSizeAllowed(packet.payload.size())) {
            SaturatingAdd(serverTelemetry_.oversizedInboundPacketsRejected, 1U);
            emitIgnoredPacket(packet.peerId, packet.payload.size(), "payload_too_large");
            continue;
        }
        if (inspectedPayloadBytes > kMaxNetPayloadBytesPerPoll
            || packet.payload.size() > kMaxNetPayloadBytesPerPoll - inspectedPayloadBytes) {
            SaturatingAdd(serverTelemetry_.inboundPacketsDroppedByPollBudget, 1U);
            emitIgnoredPacket(packet.peerId, packet.payload.size(), "inbound_poll_byte_limit");
            continue;
        }
        inspectedPayloadBytes += packet.payload.size();

        const std::uint64_t resourceDropsBefore = latencySimulator_.DroppedByPacketBudget()
            + latencySimulator_.DroppedByByteBudget();
        const std::size_t peerId = packet.peerId;
        const std::size_t payloadBytes = packet.payload.size();
        if (!latencySimulator_.Enqueue(nowMs, std::move(packet))) {
            const std::uint64_t resourceDropsAfter = latencySimulator_.DroppedByPacketBudget()
                + latencySimulator_.DroppedByByteBudget();
            if (resourceDropsAfter != resourceDropsBefore) {
                SaturatingAdd(serverTelemetry_.inboundPacketsDroppedByQueueBudget, 1U);
                emitIgnoredPacket(peerId, payloadBytes, "inbound_queue_resource_limit");
            }
        }
    }
    std::size_t readyPacketCount = 0U;
    while (readyPacketCount < kRuntimeReadyPacketBudget) {
        const auto ready = latencySimulator_.TryPopReady(nowMs);
        if (!ready.has_value()) {
            break;
        }
        ++readyPacketCount;
        if (peerIsCoolingDown(ready->peerId, false)) {
            SaturatingAdd(serverTelemetry_.inboundPacketsDroppedByPeerCooldown, 1U);
            RuntimeEvent ignored{};
            ignored.fields["peer"] = std::to_string(ready->peerId);
            ignored.fields["bytes"] = std::to_string(ready->payload.size());
            ignored.fields["reason"] = "peer_cooldown";
            context.Events().Emit("net.packet.ignored", std::move(ignored));
            continue;
        }
        if (config_.requireSessionExtensionAgreement) {
            if (config_.role == NetRole::Client) {
                // Only extension control packets belong to the preflight decoder. Once the
                // host has accepted this client, gameplay snapshots share the same authority
                // transport and must flow through to the snapshot decoder below.
                if (!session_protocol::IsControlPacket(*ready)) {
                    if (localSessionExtensionState_ != SessionExtensionPeerState::Accepted) {
                        emitIgnoredPacket(ready->peerId, ready->payload.size(),
                                          "session_extension_preflight_pending");
                        continue;
                    }
                } else {
                ri::core::SessionExtensionContract offered{};
                if (!session_protocol::DecodeOffer(*ready, offered)) {
                    emitIgnoredPacket(ready->peerId, ready->payload.size(), "session_extension_offer_invalid");
                    continue;
                }
                const bool accepted = ri::core::SessionExtensionContractsMatch(
                    offered, config_.sessionExtensionContract);
                const std::optional<NetPacket> acknowledgement =
                    session_protocol::EncodeAcknowledgement(offered, accepted);
                if (acknowledgement.has_value()) {
                    static_cast<void>(authorityTransport_->Send(ready->peerId, *acknowledgement));
                }
                localSessionExtensionState_ = accepted
                    ? SessionExtensionPeerState::Accepted
                    : SessionExtensionPeerState::Rejected;
                RuntimeEvent event{};
                event.fields["peer"] = std::to_string(ready->peerId);
                event.fields["fingerprint"] = offered.fingerprint;
                event.fields["accepted"] = accepted ? "1" : "0";
                context.Events().Emit(accepted ? "net.session_extensions.accepted"
                                               : "net.session_extensions.rejected", std::move(event));
                continue;
                }
            }

            if (config_.role != NetRole::Client) {
            bool accepted = false;
            std::string fingerprint;
            if (session_protocol::DecodeAcknowledgement(*ready, accepted, fingerprint)) {
                const bool matches = accepted && fingerprint == config_.sessionExtensionContract.fingerprint;
                sessionExtensionPeers_[ready->peerId] = matches
                    ? SessionExtensionPeerState::Accepted
                    : SessionExtensionPeerState::Rejected;
                RuntimeEvent event{};
                event.fields["peer"] = std::to_string(ready->peerId);
                event.fields["fingerprint"] = std::move(fingerprint);
                event.fields["accepted"] = matches ? "1" : "0";
                context.Events().Emit(matches ? "net.session_extensions.accepted"
                                              : "net.session_extensions.rejected", std::move(event));
                continue;
            }
            if (session_protocol::IsControlPacket(*ready)) {
                emitIgnoredPacket(ready->peerId, ready->payload.size(), "session_extension_ack_invalid");
                continue;
            }
            if (PeerSessionExtensionState(ready->peerId) != SessionExtensionPeerState::Accepted) {
                emitIgnoredPacket(ready->peerId, ready->payload.size(), "session_extension_preflight_pending");
                continue;
            }
            }
        }
        if (config_.role == NetRole::Client) {
            const auto requestForceFullResync = [&](const std::uint32_t tick, const char* reason) {
                // Keep the last good local baseline. Forgetting it here makes every later
                // delta fail until the host accepts a rate-limited resync and sends a full
                // snapshot. The host still ForgetPeer when it accepts 0xA8.
                (void)authorityTransport_->Send(ready->peerId, EncodeSnapshotResyncRequest());
                RuntimeEvent rejected{};
                rejected.fields["peer"] = std::to_string(ready->peerId);
                rejected.fields["tick"] = std::to_string(tick);
                if (reason != nullptr) {
                    rejected.fields["reason"] = reason;
                }
                rejected.fields["resync"] = "full";
                context.Events().Emit("net.snapshot.rejected", std::move(rejected));
            };
            const auto snapshotPacket = DecodeSnapshotPacket(*ready);
            if (!snapshotPacket.has_value()) {
                emitIgnoredPacket(ready->peerId, ready->payload.size(), "unknown_or_malformed_packet");
                requestForceFullResync(0U, "unknown_or_malformed_packet");
                continue;
            }
            const SnapshotBlob fallback{};
            const auto rebuilt = snapshotReplicator_.ApplyFromServer(ready->peerId, fallback, *snapshotPacket);
            if (!rebuilt.has_value()) {
                requestForceFullResync(snapshotPacket->targetTick, "apply_failed");
                continue;
            }
            std::uint32_t authTick = rebuilt->tick;
            float authX = 0.0f;
            if (config_.simulationBridge != nullptr) {
                std::string applyError;
                bool applied = false;
                try {
                    applied = config_.simulationBridge->ApplySnapshot(*rebuilt, &applyError);
                } catch (const std::exception& exception) {
                    applyError = exception.what();
                } catch (...) {
                    applyError = "simulation bridge threw an unknown exception";
                }
                if (!applied) {
                    requestForceFullResync(
                        rebuilt->tick,
                        applyError.empty() ? "simulation_snapshot_apply_failed" : applyError.c_str());
                    continue;
                }
            } else if (!DecodeAuthoritativePosition(rebuilt->bytes, authTick, authX)) {
                requestForceFullResync(rebuilt->tick, "authoritative_position_decode_failed");
                continue;
            }
            // Commit baseline only after the domain payload validates.
            snapshotReplicator_.RememberPeerBaseline(ready->peerId, *rebuilt);
            if (config_.simulationBridge == nullptr) {
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
            }
            RuntimeEvent applied{};
            applied.fields["peer"] = std::to_string(ready->peerId);
            applied.fields["tick"] = std::to_string(authTick);
            context.Events().Emit("net.snapshot.applied", std::move(applied));
        } else if (IsSnapshotResyncRequest(*ready)) {
            // Client rejected a snapshot; clear server baseline so the next BuildForPeer is full.
            // Unauthenticated resync is cheap to spam, so accept at most one per interval.
            const auto lastAccepted = peerResyncAcceptedMs_.find(ready->peerId);
            if (lastAccepted != peerResyncAcceptedMs_.end()
                && nowMs < lastAccepted->second + kSnapshotResyncMinIntervalMs) {
                RuntimeEvent ignored{};
                ignored.fields["peer"] = std::to_string(ready->peerId);
                ignored.fields["bytes"] = std::to_string(ready->payload.size());
                ignored.fields["reason"] = "resync_rate_limited";
                context.Events().Emit("net.packet.ignored", std::move(ignored));
                continue;
            }
            peerResyncAcceptedMs_[ready->peerId] = nowMs;
            snapshotReplicator_.ForgetPeer(ready->peerId);
            RuntimeEvent resync{};
            resync.fields["peer"] = std::to_string(ready->peerId);
            resync.fields["reason"] = "client_resync_request";
            context.Events().Emit("net.snapshot.resync_requested", std::move(resync));
        } else {
            if (config_.simulationBridge != nullptr) {
                std::string commandError;
                bool accepted = false;
                try {
                    accepted = config_.simulationBridge->HandleCommand(
                        ready->peerId, ready->channel, ready->payload, &commandError);
                } catch (const std::exception& exception) {
                    commandError = exception.what();
                } catch (...) {
                    commandError = "simulation bridge threw an unknown exception";
                }
                RuntimeEvent commandResult{};
                commandResult.fields["peer"] = std::to_string(ready->peerId);
                commandResult.fields["channel"] = std::to_string(ready->channel);
                commandResult.fields["bytes"] = std::to_string(ready->payload.size());
                if (!commandError.empty()) {
                    commandResult.fields["reason"] = std::move(commandError);
                }
                context.Events().Emit(accepted ? "net.command.accepted" : "net.command.rejected",
                                      std::move(commandResult));
                if (!accepted) {
                    continue;
                }
            }
            RuntimeEvent command{};
            command.fields["peer"] = std::to_string(ready->peerId);
            command.fields["channel"] = std::to_string(ready->channel);
            command.fields["bytes"] = std::to_string(ready->payload.size());
            context.Events().Emit("net.command.received", std::move(command));
        }
    }

    if (config_.role == NetRole::DedicatedServer || config_.role == NetRole::ListenServer) {
        // Time-based server snapshot cadence.
        const double snapshotStep = 1.0 / static_cast<double>(std::max(1, config_.serverTickRate));
        // A non-finite delta would poison the accumulator permanently (NaN never satisfies
        // the loop condition), silently killing all future snapshots.
        const double deltaSeconds = std::isfinite(frame.deltaSeconds) ? std::max(0.0, frame.deltaSeconds) : 0.0;
        snapshotCadenceAccumulatorSeconds_ += deltaSeconds;
        // Bound catch-up so a long hitch cannot burst hundreds of snapshots at every peer
        // in a single frame.
        const double maxCatchUpSeconds = snapshotStep * static_cast<double>(kMaxSnapshotCatchUpSteps);
        snapshotCadenceAccumulatorSeconds_ = std::min(snapshotCadenceAccumulatorSeconds_, maxCatchUpSeconds);

        // Peer ids are never reused, so baseline history for departed peers must be dropped
        // or the replicator grows without bound across reconnect churn.
        snapshotReplicator_.RetainPeers(connectedPeers);
        {
            const std::unordered_set<std::size_t> active(connectedPeers.begin(), connectedPeers.end());
            const auto pruneMap = [](auto& offenses, const std::unordered_set<std::size_t>& keep) {
                for (auto it = offenses.begin(); it != offenses.end();) {
                    if (keep.find(it->first) == keep.end()) {
                        it = offenses.erase(it);
                    } else {
                        ++it;
                    }
                }
            };
            pruneMap(peerOffenses_, active);
            pruneMap(peerResyncAcceptedMs_, active);
            for (auto it = sessionExtensionPeers_.begin(); it != sessionExtensionPeers_.end();) {
                if (std::find(connectedPeers.begin(), connectedPeers.end(), it->first) == connectedPeers.end()) {
                    it = sessionExtensionPeers_.erase(it);
                } else {
                    ++it;
                }
            }
            if (p2pTransport_ != nullptr) {
                const std::vector<std::size_t> p2pPeers = p2pTransport_->ConnectedPeers();
                const std::unordered_set<std::size_t> p2pActive(p2pPeers.begin(), p2pPeers.end());
                pruneMap(p2pPeerOffenses_, p2pActive);
            } else {
                p2pPeerOffenses_.clear();
            }
        }

        while (snapshotCadenceAccumulatorSeconds_ >= snapshotStep) {
            snapshotCadenceAccumulatorSeconds_ -= snapshotStep;
            lastBroadcastTick_ = static_cast<std::uint32_t>(frame.frameIndex);
            serverTelemetry_.lastSnapshotTick = lastBroadcastTick_;
            const std::uint32_t snapshotTick = static_cast<std::uint32_t>(frame.frameIndex);
            std::optional<SnapshotBlob> domainSnapshot{};
            if (config_.simulationBridge != nullptr) {
                try {
                    domainSnapshot = config_.simulationBridge->CaptureSnapshot(snapshotTick);
                } catch (const std::exception& exception) {
                    RuntimeEvent rejected{};
                    rejected.fields["tick"] = std::to_string(snapshotTick);
                    rejected.fields["reason"] = exception.what();
                    context.Events().Emit("net.snapshot.rejected", std::move(rejected));
                } catch (...) {
                    RuntimeEvent rejected{};
                    rejected.fields["tick"] = std::to_string(snapshotTick);
                    rejected.fields["reason"] = "simulation bridge threw an unknown exception";
                    context.Events().Emit("net.snapshot.rejected", std::move(rejected));
                }
                if (!domainSnapshot.has_value()
                    || domainSnapshot->tick != snapshotTick
                    || domainSnapshot->bytes.size() > kMaxSnapshotPayloadBytes) {
                    RuntimeEvent rejected{};
                    rejected.fields["tick"] = std::to_string(snapshotTick);
                    rejected.fields["reason"] = !domainSnapshot.has_value()
                        ? "simulation_snapshot_unavailable"
                        : (domainSnapshot->tick != snapshotTick
                            ? "simulation_snapshot_tick_mismatch"
                            : "simulation_snapshot_too_large");
                    context.Events().Emit("net.snapshot.rejected", std::move(rejected));
                    continue;
                }
            } else {
                SnapshotBlob fallback{};
                fallback.tick = snapshotTick;
                const float authorityX = static_cast<float>(
                    std::sin(static_cast<float>(frame.frameIndex) * 0.05f) * 10.0f);
                fallback.bytes = EncodeAuthoritativePosition(fallback.tick, authorityX);
                domainSnapshot = std::move(fallback);
            }
            const SnapshotBlob& snapshot = *domainSnapshot;
            bool usedDelta = false;
            bool broadcasted = false;
            for (const std::size_t peerId : connectedPeers) {
                if (config_.requireSessionExtensionAgreement
                    && PeerSessionExtensionState(peerId) != SessionExtensionPeerState::Accepted) {
                    continue;
                }
                bool usedPeerDelta = false;
                const SnapshotDeltaPacket packet = snapshotReplicator_.BuildForPeer(peerId, snapshot, usedPeerDelta);
                const std::optional<NetPacket> encodedPacket = EncodeSnapshotPacket(packet);
                if (!encodedPacket.has_value()) {
                    SaturatingAdd(serverTelemetry_.oversizedOutboundPacketsRejected, 1U);
                    continue;
                }
                if (authorityTransport_->Send(peerId, *encodedPacket)) {
                    // Only advance the peer baseline after encode+send succeed; otherwise the
                    // next delta would reference a snapshot the peer never received.
                    snapshotReplicator_.RememberPeerBaseline(peerId, snapshot);
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
        const auto p2pInbound = p2pTransport_->PollReceive(kRuntimeTransportEventBudget);
        std::size_t acceptedP2pPackets = 0U;
        std::size_t acceptedP2pBytes = 0U;
        const std::size_t p2pInspectionCount = std::min(p2pInbound.size(), kRuntimeTransportEventBudget);
        if (p2pInbound.size() > p2pInspectionCount) {
            SaturatingAdd(
                serverTelemetry_.inboundPacketsDroppedByPollBudget,
                static_cast<std::uint64_t>(p2pInbound.size() - p2pInspectionCount));
            RuntimeEvent truncated{};
            truncated.fields["plane"] = "p2p";
            truncated.fields["returned"] = std::to_string(p2pInbound.size());
            truncated.fields["accepted_for_inspection"] = std::to_string(p2pInspectionCount);
            context.Events().Emit("net.poll.truncated", std::move(truncated));
        }
        for (std::size_t packetIndex = 0U; packetIndex < p2pInspectionCount; ++packetIndex) {
            const NetPacket& packet = p2pInbound[packetIndex];
            if (peerIsCoolingDown(packet.peerId, true)) {
                SaturatingAdd(serverTelemetry_.inboundPacketsDroppedByPeerCooldown, 1U);
                RuntimeEvent ignored{};
                ignored.fields["peer"] = std::to_string(packet.peerId);
                ignored.fields["bytes"] = std::to_string(packet.payload.size());
                ignored.fields["reason"] = "peer_cooldown";
                context.Events().Emit("net.packet.ignored", std::move(ignored));
                continue;
            }
            if (!IsNetPacketPayloadSizeAllowed(packet.payload.size())) {
                SaturatingAdd(serverTelemetry_.oversizedInboundPacketsRejected, 1U);
                emitIgnoredPacket(packet.peerId, packet.payload.size(), "p2p_payload_too_large");
                continue;
            }
            if (acceptedP2pBytes > kMaxNetPayloadBytesPerPoll
                || packet.payload.size() > kMaxNetPayloadBytesPerPoll - acceptedP2pBytes) {
                SaturatingAdd(serverTelemetry_.inboundPacketsDroppedByPollBudget, 1U);
                emitIgnoredPacket(packet.peerId, packet.payload.size(), "p2p_poll_byte_limit");
                continue;
            }
            acceptedP2pBytes += packet.payload.size();
            ++acceptedP2pPackets;
        }
        if (acceptedP2pPackets > 0U) {
            RuntimeEvent p2pEv{};
            p2pEv.fields["packets"] = std::to_string(acceptedP2pPackets);
            context.Events().Emit("net.p2p.received", std::move(p2pEv));
        }
    }

    TickHostMigration(context);

    RuntimeEvent metrics{};
    const NetTransportStats stats = authorityTransport_->Stats();
    const auto snapshotStats = snapshotReplicator_.Stats();
    serverTelemetry_.simulatedDrops = latencySimulator_.DroppedBySimulation();
    NetGraphSample ng{};
    ng.tick = static_cast<std::uint32_t>(frame.frameIndex);
    ng.rttMs = static_cast<double>(config_.latencySimulation.baseDelayMs) * 2.0;
    ng.jitterMs = static_cast<double>(config_.latencySimulation.jitterMs);
    ng.predictionError = predictionTelemetry_.avgPositionError;
    ng.packetLossPercent = config_.latencySimulation.packetLossPercent;
    netGraph_.Push(ng);
    metrics.fields["sent_packets"] = std::to_string(stats.packetsSent);
    metrics.fields["recv_packets"] = std::to_string(stats.packetsReceived);
    metrics.fields["dropped_sim"] = std::to_string(latencySimulator_.DroppedBySimulation());
    metrics.fields["dropped_total"] = std::to_string(latencySimulator_.DroppedPackets());
    metrics.fields["transport_oversized_rejected"] = std::to_string(stats.oversizedPacketsRejected);
    metrics.fields["transport_oversized_bytes_rejected"] = std::to_string(stats.oversizedBytesRejected);
    metrics.fields["runtime_oversized_inbound_rejected"] =
        std::to_string(serverTelemetry_.oversizedInboundPacketsRejected);
    metrics.fields["runtime_oversized_outbound_rejected"] =
        std::to_string(serverTelemetry_.oversizedOutboundPacketsRejected);
    metrics.fields["inbound_queue_resource_drops"] =
        std::to_string(serverTelemetry_.inboundPacketsDroppedByQueueBudget);
    metrics.fields["inbound_poll_resource_drops"] =
        std::to_string(serverTelemetry_.inboundPacketsDroppedByPollBudget);
    metrics.fields["inbound_peer_cooldown_drops"] =
        std::to_string(serverTelemetry_.inboundPacketsDroppedByPeerCooldown);
    metrics.fields["latency_queue_packets"] = std::to_string(latencySimulator_.QueuedPacketCount());
    metrics.fields["latency_queue_payload_bytes"] = std::to_string(latencySimulator_.QueuedPayloadBytes());
    metrics.fields["latency_queue_packet_budget_drops"] =
        std::to_string(latencySimulator_.DroppedByPacketBudget());
    metrics.fields["latency_queue_byte_budget_drops"] =
        std::to_string(latencySimulator_.DroppedByByteBudget());
    metrics.fields["snapshot_full"] = std::to_string(snapshotStats.fullSnapshots);
    metrics.fields["snapshot_delta"] = std::to_string(snapshotStats.deltaSnapshots);
    metrics.fields["snapshot_full_bytes"] = std::to_string(snapshotStats.bytesFull);
    metrics.fields["snapshot_delta_bytes"] = std::to_string(snapshotStats.bytesDelta);
    metrics.fields["snapshot_broadcasts"] = std::to_string(serverTelemetry_.snapshotsBroadcast);
    metrics.fields["inbound_packets"] = std::to_string(serverTelemetry_.inboundPackets);
    metrics.fields["last_snapshot_tick"] = std::to_string(serverTelemetry_.lastSnapshotTick);
    metrics.fields["tracked_peers"] = std::to_string(snapshotReplicator_.TrackedPeerCount());
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

SessionExtensionPeerState AuthoritativeNetModule::SessionExtensionState() const noexcept {
    return config_.requireSessionExtensionAgreement ? localSessionExtensionState_
                                                    : SessionExtensionPeerState::NotRequired;
}

SessionExtensionPeerState AuthoritativeNetModule::PeerSessionExtensionState(const std::size_t peerId) const noexcept {
    if (!config_.requireSessionExtensionAgreement) {
        return SessionExtensionPeerState::NotRequired;
    }
    const auto state = sessionExtensionPeers_.find(peerId);
    return state == sessionExtensionPeers_.end() ? SessionExtensionPeerState::Pending : state->second;
}

bool AuthoritativeNetModule::SendPacket(
    const std::size_t peerId,
    const NetPacket& packet,
    const NetChannelKind kind) {
    if (!IsNetPacketPayloadSizeAllowed(packet.payload.size())) {
        SaturatingAdd(serverTelemetry_.oversizedOutboundPacketsRejected, 1U);
        return false;
    }
    if (config_.requireSessionExtensionAgreement && kind == NetChannelKind::Authority) {
        if (config_.role == NetRole::Client
            && localSessionExtensionState_ != SessionExtensionPeerState::Accepted) {
            return false;
        }
        if (IsSessionExtensionHostRole(config_.role)
            && PeerSessionExtensionState(peerId) != SessionExtensionPeerState::Accepted) {
            return false;
        }
    }
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
