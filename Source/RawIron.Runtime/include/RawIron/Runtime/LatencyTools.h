#pragma once

#include "RawIron/Runtime/NetTransport.h"

#include <cstdint>
#include <optional>
#include <queue>
#include <random>
#include <deque>

namespace ri::runtime {

struct LatencySimulationConfig {
    int baseDelayMs = 0;
    int jitterMs = 0;
    float packetLossPercent = 0.0f;
    bool enabled = false;
};

struct LatencyTelemetry {
    double smoothedRttMs = 0.0;
    double smoothedJitterMs = 0.0;
    double predictionError = 0.0;
    std::uint64_t lostPackets = 0;
    std::uint64_t deliveredPackets = 0;
};

struct NetGraphSample {
    std::uint32_t tick = 0;
    double rttMs = 0.0;
    double jitterMs = 0.0;
    double predictionError = 0.0;
    float packetLossPercent = 0.0f;
};

class NetGraphTracker {
public:
    explicit NetGraphTracker(std::size_t history = 256);
    void Push(NetGraphSample sample);
    [[nodiscard]] std::optional<NetGraphSample> Latest() const;
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    std::size_t history_ = 256;
    std::deque<NetGraphSample> samples_{};
};

class LatencySimulator {
public:
    /// Hard ceiling on in-flight delayed packets so a peer flood cannot grow the queue
    /// without bound. Overflow is counted as a drop.
    static constexpr std::size_t kMaxQueuedPackets = 4096U;
    /// Packet count alone is not a memory bound: 4096 legal snapshot-sized packets would retain
    /// more than 16 GiB. This byte budget is enforced before a payload enters the delay queue.
    static constexpr std::size_t kMaxQueuedPayloadBytes = 32U * 1024U * 1024U;

    explicit LatencySimulator(std::uint32_t seed = 0xC0FFEEu);

    void Configure(const LatencySimulationConfig& config);
    [[nodiscard]] LatencySimulationConfig Config() const noexcept;

    /// Returns false if the packet was dropped by simulated loss or a packet/queue resource limit.
    /// Lvalue packets are copied only after every size/queue/loss check succeeds.
    bool Enqueue(std::uint64_t nowMs, const NetPacket& packet);
    bool Enqueue(std::uint64_t nowMs, NetPacket&& packet);
    [[nodiscard]] std::optional<NetPacket> TryPopReady(std::uint64_t nowMs);
    [[nodiscard]] std::uint64_t DroppedPackets() const noexcept;
    [[nodiscard]] std::uint64_t DroppedBySimulation() const noexcept;
    [[nodiscard]] std::uint64_t DroppedOversizedPackets() const noexcept;
    [[nodiscard]] std::uint64_t DroppedByPacketBudget() const noexcept;
    [[nodiscard]] std::uint64_t DroppedByByteBudget() const noexcept;
    [[nodiscard]] std::size_t QueuedPacketCount() const noexcept;
    [[nodiscard]] std::size_t QueuedPayloadBytes() const noexcept;

private:
    struct DelayedPacket {
        std::uint64_t deliverAtMs = 0;
        NetPacket packet;
    };

    struct CompareDeliverAt {
        bool operator()(const DelayedPacket& lhs, const DelayedPacket& rhs) const {
            return lhs.deliverAtMs > rhs.deliverAtMs;
        }
    };

    [[nodiscard]] std::optional<std::uint64_t> PrepareDelivery(
        std::uint64_t nowMs, std::size_t payloadBytes);
    void PushPrepared(std::uint64_t deliverAtMs, NetPacket&& packet);

    LatencySimulationConfig config_{};
    std::mt19937 rng_;
    std::uniform_real_distribution<float> lossDist_{0.0f, 100.0f};
    std::priority_queue<DelayedPacket, std::vector<DelayedPacket>, CompareDeliverAt> queue_;
    std::uint64_t dropped_ = 0;
    std::uint64_t droppedBySimulation_ = 0;
    std::uint64_t droppedOversized_ = 0;
    std::uint64_t droppedByPacketBudget_ = 0;
    std::uint64_t droppedByByteBudget_ = 0;
    std::size_t queuedPayloadBytes_ = 0;
};

} // namespace ri::runtime
