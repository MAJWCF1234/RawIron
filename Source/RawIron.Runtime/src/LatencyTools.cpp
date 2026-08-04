#include "RawIron/Runtime/LatencyTools.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ri::runtime {
namespace {

void SaturatingIncrement(std::uint64_t& counter) noexcept {
    if (counter != std::numeric_limits<std::uint64_t>::max()) {
        ++counter;
    }
}

} // namespace

LatencySimulator::LatencySimulator(const std::uint32_t seed)
    : rng_(seed) {}

void LatencySimulator::Configure(const LatencySimulationConfig& config) {
    config_ = config;
    config_.baseDelayMs = std::max(0, config_.baseDelayMs);
    config_.jitterMs = std::max(0, config_.jitterMs);
    config_.packetLossPercent = std::isfinite(config_.packetLossPercent)
        ? std::clamp(config_.packetLossPercent, 0.0f, 100.0f)
        : 0.0f;
    if (!config_.enabled) {
        while (!queue_.empty()) {
            queue_.pop();
        }
        queuedPayloadBytes_ = 0U;
    }
}

LatencySimulationConfig LatencySimulator::Config() const noexcept {
    return config_;
}

std::optional<std::uint64_t> LatencySimulator::PrepareDelivery(
    const std::uint64_t nowMs,
    const std::size_t payloadBytes) {
    if (!IsNetPacketPayloadSizeAllowed(payloadBytes)) {
        SaturatingIncrement(dropped_);
        SaturatingIncrement(droppedOversized_);
        return std::nullopt;
    }
    if (queue_.size() >= kMaxQueuedPackets) {
        SaturatingIncrement(dropped_);
        SaturatingIncrement(droppedByPacketBudget_);
        return std::nullopt;
    }
    if (queuedPayloadBytes_ > kMaxQueuedPayloadBytes
        || payloadBytes > kMaxQueuedPayloadBytes - queuedPayloadBytes_) {
        SaturatingIncrement(dropped_);
        SaturatingIncrement(droppedByByteBudget_);
        return std::nullopt;
    }
    if (!config_.enabled) {
        return nowMs;
    }
    if (lossDist_(rng_) < config_.packetLossPercent) {
        SaturatingIncrement(dropped_);
        SaturatingIncrement(droppedBySimulation_);
        return std::nullopt;
    }

    int jitter = 0;
    if (config_.jitterMs > 0) {
        std::uniform_int_distribution<int> jitterDist(-config_.jitterMs, config_.jitterMs);
        jitter = jitterDist(rng_);
    }
    const std::uint64_t delay = static_cast<std::uint64_t>(std::max<std::int64_t>(
        0, static_cast<std::int64_t>(config_.baseDelayMs) + static_cast<std::int64_t>(jitter)));
    return delay > std::numeric_limits<std::uint64_t>::max() - nowMs
        ? std::numeric_limits<std::uint64_t>::max()
        : nowMs + delay;
}

void LatencySimulator::PushPrepared(const std::uint64_t deliverAtMs, NetPacket&& packet) {
    const std::size_t payloadBytes = packet.payload.size();
    queue_.push(DelayedPacket{deliverAtMs, std::move(packet)});
    queuedPayloadBytes_ += payloadBytes;
}

bool LatencySimulator::Enqueue(const std::uint64_t nowMs, const NetPacket& packet) {
    const std::optional<std::uint64_t> deliverAt = PrepareDelivery(nowMs, packet.payload.size());
    if (!deliverAt.has_value()) {
        return false;
    }
    NetPacket owned = packet;
    PushPrepared(*deliverAt, std::move(owned));
    return true;
}

bool LatencySimulator::Enqueue(const std::uint64_t nowMs, NetPacket&& packet) {
    const std::optional<std::uint64_t> deliverAt = PrepareDelivery(nowMs, packet.payload.size());
    if (!deliverAt.has_value()) {
        return false;
    }
    PushPrepared(*deliverAt, std::move(packet));
    return true;
}

std::optional<NetPacket> LatencySimulator::TryPopReady(const std::uint64_t nowMs) {
    if (queue_.empty() || queue_.top().deliverAtMs > nowMs) {
        return std::nullopt;
    }
    const std::size_t payloadBytes = queue_.top().packet.payload.size();
    NetPacket out = std::move(const_cast<DelayedPacket&>(queue_.top()).packet);
    queue_.pop();
    queuedPayloadBytes_ -= payloadBytes;
    return out;
}

std::uint64_t LatencySimulator::DroppedPackets() const noexcept {
    return dropped_;
}

std::uint64_t LatencySimulator::DroppedBySimulation() const noexcept {
    return droppedBySimulation_;
}

std::uint64_t LatencySimulator::DroppedOversizedPackets() const noexcept {
    return droppedOversized_;
}

std::uint64_t LatencySimulator::DroppedByPacketBudget() const noexcept {
    return droppedByPacketBudget_;
}

std::uint64_t LatencySimulator::DroppedByByteBudget() const noexcept {
    return droppedByByteBudget_;
}

std::size_t LatencySimulator::QueuedPacketCount() const noexcept {
    return queue_.size();
}

std::size_t LatencySimulator::QueuedPayloadBytes() const noexcept {
    return queuedPayloadBytes_;
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
