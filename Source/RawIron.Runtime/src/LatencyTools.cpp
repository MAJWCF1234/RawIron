#include "RawIron/Runtime/LatencyTools.h"

#include <algorithm>

namespace ri::runtime {

LatencySimulator::LatencySimulator(const std::uint32_t seed)
    : rng_(seed) {}

void LatencySimulator::Configure(const LatencySimulationConfig& config) {
    config_ = config;
    if (!config_.enabled) {
        while (!queue_.empty()) {
            queue_.pop();
        }
    }
}

LatencySimulationConfig LatencySimulator::Config() const noexcept {
    return config_;
}

bool LatencySimulator::Enqueue(const std::uint64_t nowMs, NetPacket packet) {
    if (!config_.enabled) {
        queue_.push(DelayedPacket{nowMs, std::move(packet)});
        return true;
    }
    if (lossDist_(rng_) < config_.packetLossPercent) {
        ++dropped_;
        return false;
    }

    int jitter = 0;
    if (config_.jitterMs > 0) {
        std::uniform_int_distribution<int> jitterDist(-config_.jitterMs, config_.jitterMs);
        jitter = jitterDist(rng_);
    }
    const int delay = std::max(0, config_.baseDelayMs + jitter);
    queue_.push(DelayedPacket{nowMs + static_cast<std::uint64_t>(delay), std::move(packet)});
    return true;
}

std::optional<NetPacket> LatencySimulator::TryPopReady(const std::uint64_t nowMs) {
    if (queue_.empty() || queue_.top().deliverAtMs > nowMs) {
        return std::nullopt;
    }
    NetPacket out = std::move(const_cast<DelayedPacket&>(queue_.top()).packet);
    queue_.pop();
    return out;
}

std::uint64_t LatencySimulator::DroppedPackets() const noexcept {
    return dropped_;
}

NetGraphTracker::NetGraphTracker(const std::size_t history)
    : history_(std::max<std::size_t>(1U, history)) {}

void NetGraphTracker::Push(NetGraphSample sample) {
    samples_.push_back(std::move(sample));
    while (samples_.size() > history_) {
        samples_.pop_front();
    }
}

std::optional<NetGraphSample> NetGraphTracker::Latest() const {
    if (samples_.empty()) {
        return std::nullopt;
    }
    return samples_.back();
}

std::size_t NetGraphTracker::Size() const noexcept {
    return samples_.size();
}

} // namespace ri::runtime
