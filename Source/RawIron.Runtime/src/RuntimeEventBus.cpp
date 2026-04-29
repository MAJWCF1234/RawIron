#include "RawIron/Runtime/RuntimeEventBus.h"

#include <algorithm>
#include <string>
#include <utility>

namespace ri::runtime {

RuntimeEventBus::ListenerId RuntimeEventBus::On(std::string_view type, Handler handler) {
    const ListenerId listenerId = nextListenerId_++;
    listeners_[std::string(type)].push_back(ListenerEntry{
        .id = listenerId,
        .handler = std::move(handler),
    });
    listenersAdded_ += 1;
    return listenerId;
}

bool RuntimeEventBus::Off(std::string_view type, ListenerId listenerId) {
    listenersRemoved_ += 1;

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
    if (entries.empty()) {
        listeners_.erase(found);
    }
    return removed;
}

void RuntimeEventBus::Emit(std::string_view type, RuntimeEvent event) {
    emitted_ += 1;
    const std::uint64_t sequence = nextEventSequence_++;

    const std::string key(type);
    emittedByType_[key] += 1;
    if (event.type.empty()) {
        event.type = key;
    }
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
        routeTrace_.erase(routeTrace_.begin(), routeTrace_.begin() + (routeTrace_.size() - maxRouteTraceCount_));
    }
    if (found == listeners_.end()) {
        return;
    }

    const auto listeners = found->second;
    for (const ListenerEntry& entry : listeners) {
        if (entry.handler) {
            entry.handler(event);
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
    listeners_.clear();
}

RuntimeEventBusMetrics RuntimeEventBus::GetMetrics() const {
    RuntimeEventBusMetrics metrics{};
    metrics.emitted = emitted_;
    metrics.listenersAdded = listenersAdded_;
    metrics.listenersRemoved = listenersRemoved_;
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
    return std::vector<RuntimeSignalRouteTrace>(routeTrace_.end() - static_cast<std::ptrdiff_t>(take), routeTrace_.end());
}

RuntimeEventBus CreateRuntimeEventBus() {
    return RuntimeEventBus{};
}

} // namespace ri::runtime
