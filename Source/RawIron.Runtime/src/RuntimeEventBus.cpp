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
    return metrics;
}

} // namespace ri::runtime
