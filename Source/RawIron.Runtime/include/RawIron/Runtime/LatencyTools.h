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
    explicit LatencySimulator(std::uint32_t seed = 0xC0FFEEu);

    void Configure(const LatencySimulationConfig& config);
    [[nodiscard]] LatencySimulationConfig Config() const noexcept;

    /// Returns false if packet was dropped by simulated loss.
    bool Enqueue(std::uint64_t nowMs, NetPacket packet);
    [[nodiscard]] std::optional<NetPacket> TryPopReady(std::uint64_t nowMs);
    [[nodiscard]] std::uint64_t DroppedPackets() const noexcept;

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

    LatencySimulationConfig config_{};
    std::mt19937 rng_;
    std::uniform_real_distribution<float> lossDist_{0.0f, 100.0f};
    std::priority_queue<DelayedPacket, std::vector<DelayedPacket>, CompareDeliverAt> queue_;
    std::uint64_t dropped_ = 0;
};

} // namespace ri::runtime
