#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::runtime {

using RuntimeEventFields = std::unordered_map<std::string, std::string>;

struct RuntimeEvent {
    std::string id;
    std::string type;
    RuntimeEventFields fields;
};

struct RuntimeEventBusMetrics {
    std::size_t emitted = 0;
    std::size_t listenersAdded = 0;
    std::size_t listenersRemoved = 0;
    std::size_t activeListeners = 0;
};

/// Single-threaded in-process event fan-out. `Emit` copies the current listener list before
/// invoking handlers, so registering or removing listeners from inside a handler is safe for that emit.
class RuntimeEventBus {
public:
    using ListenerId = std::uint64_t;
    using Handler = std::function<void(const RuntimeEvent& event)>;

    ListenerId On(std::string_view type, Handler handler);
    bool Off(std::string_view type, ListenerId listenerId);
    void Emit(std::string_view type, RuntimeEvent event = {});
    void Clear();
    [[nodiscard]] RuntimeEventBusMetrics GetMetrics() const;

private:
    struct ListenerEntry {
        ListenerId id = 0;
        Handler handler;
    };

    ListenerId nextListenerId_ = 1;
    std::uint64_t nextEventSequence_ = 1;
    std::size_t emitted_ = 0;
    std::size_t listenersAdded_ = 0;
    std::size_t listenersRemoved_ = 0;
    std::unordered_map<std::string, std::vector<ListenerEntry>> listeners_;
};

} // namespace ri::runtime
