#include "RawIron/Runtime/RuntimeEventBus.h"

#include "RawIron/Core/Log.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

namespace ri::runtime {
namespace {

constexpr std::size_t kMaxEventTypeBytes = 256U;
constexpr std::size_t kMaxMetricEventTypes = 4096U;

bool IsValidEventType(const std::string_view type) {
    return !type.empty() && type.size() <= kMaxEventTypeBytes;
}

} // namespace

RuntimeEventBus::ListenerId RuntimeEventBus::On(std::string_view type, Handler handler) {
    if (!IsValidEventType(type) || !handler) {
        ++rejectedSubscriptions_;
        return kInvalidListenerId;
    }
    if (nextListenerId_ == kInvalidListenerId) {
        nextListenerId_ = 1;
    }
    const ListenerId listenerId = nextListenerId_++;
    listeners_[std::string(type)].push_back(ListenerEntry{
        .id = listenerId,
        .handler = std::move(handler),
    });
    listenersAdded_ += 1;
    return listenerId;
}

bool RuntimeEventBus::Off(std::string_view type, ListenerId listenerId) {
    if (!IsValidEventType(type) || listenerId == kInvalidListenerId) {
        return false;
    }

    const auto found = listeners_.find(std::string(type));
    if (found == listeners_.end()) {
        return false;
    }

    auto& entries = found->second;
    const auto removeIt = std::remove_if(entries.begin(), entries.end(), [listenerId](const ListenerEntry& entry) {
        return entry.id == listenerId;
    });
    const bool removed = removeIt != entries.end();
    entries.erase(removeIt, entries.end());
    if (removed) {
        listenersRemoved_ += 1;
    }
    if (entries.empty()) {
        listeners_.erase(found);
    }
    return removed;
}

void RuntimeEventBus::Emit(std::string_view type, RuntimeEvent event) {
    if (!IsValidEventType(type)) {
        ++rejectedEmissions_;
        return;
    }
    emitted_ += 1;
    const std::uint64_t sequence = nextEventSequence_++;

    const std::string key(type);
    auto metric = emittedByType_.find(key);
    if (metric != emittedByType_.end()) {
        ++metric->second;
    } else if (emittedByType_.size() < kMaxMetricEventTypes) {
        emittedByType_.emplace(key, 1U);
    } else {
        ++untrackedEventTypes_;
    }
    event.type = key;
    if (event.id.empty()) {
        event.id = "evt_" + std::to_string(sequence);
    }
    if (!event.fields.contains("sequence")) {
        event.fields.emplace("sequence", std::to_string(sequence));
    }

    const auto found = listeners_.find(key);
    const std::size_t listenerCount = found == listeners_.end() ? 0U : found->second.size();
    routeTrace_.push_back(RuntimeSignalRouteTrace{
        .sequence = sequence,
        .type = key,
        .sourceScope = event.fields.contains("source_scope") ? event.fields.at("source_scope") : std::string{},
        .targetScope = event.fields.contains("target_scope") ? event.fields.at("target_scope") : std::string{},
        .listenerCount = listenerCount,
    });
    if (routeTrace_.size() > maxRouteTraceCount_) {
        routeTrace_.pop_front();
    }
    if (found == listeners_.end()) {
        return;
    }

    const auto listeners = found->second;
    for (const ListenerEntry& entry : listeners) {
        if (entry.handler) {
            try {
                entry.handler(event);
            } catch (const std::exception& ex) {
                ++listenerExceptions_;
                ri::core::LogInfo("Runtime event listener exception for " + key + ": " + ex.what());
            } catch (...) {
                ++listenerExceptions_;
                ri::core::LogInfo("Runtime event listener exception for " + key + ": unknown");
            }
        }
    }
}

void RuntimeEventBus::EmitScoped(std::string_view type,
                                 std::string_view sourceScope,
                                 std::string_view targetScope,
                                 RuntimeEvent event) {
    event.fields.insert_or_assign("source_scope", std::string(sourceScope));
    event.fields.insert_or_assign("target_scope", std::string(targetScope));
    Emit(type, std::move(event));
}

void RuntimeEventBus::Clear() {
    decltype(listeners_){}.swap(listeners_);
    decltype(emittedByType_){}.swap(emittedByType_);
    decltype(routeTrace_){}.swap(routeTrace_);
    emitted_ = 0;
    listenersAdded_ = 0;
    listenersRemoved_ = 0;
    rejectedSubscriptions_ = 0;
    rejectedEmissions_ = 0;
    listenerExceptions_ = 0;
    untrackedEventTypes_ = 0;
}

RuntimeEventBusMetrics RuntimeEventBus::GetMetrics() const {
    RuntimeEventBusMetrics metrics{};
    metrics.emitted = emitted_;
    metrics.listenersAdded = listenersAdded_;
    metrics.listenersRemoved = listenersRemoved_;
    metrics.rejectedSubscriptions = rejectedSubscriptions_;
    metrics.rejectedEmissions = rejectedEmissions_;
    metrics.listenerExceptions = listenerExceptions_;
    metrics.untrackedEventTypes = untrackedEventTypes_;
    for (const auto& [type, entries] : listeners_) {
        (void)type;
        metrics.activeListeners += entries.size();
    }
    metrics.emittedByType = emittedByType_;
    return metrics;
}

std::vector<RuntimeSignalRouteTrace> RuntimeEventBus::GetRecentSignalRoutes(const std::size_t maxCount) const {
    if (maxCount == 0U || routeTrace_.empty()) {
        return {};
    }
    const std::size_t take = std::min(maxCount, routeTrace_.size());
    return std::vector<RuntimeSignalRouteTrace>(
        std::next(routeTrace_.begin(), static_cast<std::ptrdiff_t>(routeTrace_.size() - take)),
        routeTrace_.end());
}

RuntimeEventBus CreateRuntimeEventBus() {
    return RuntimeEventBus{};
}

} // namespace ri::runtime
