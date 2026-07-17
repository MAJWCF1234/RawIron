#include "RawIron/Runtime/RuntimeEventBus.h"

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition "\n"; \
            return EXIT_FAILURE; \
        } \
    } while (false)

int main() {
    using ri::runtime::RuntimeEvent;
    using ri::runtime::RuntimeEventBus;

    RuntimeEventBus bus;
    CHECK(bus.On("", [](const RuntimeEvent&) {}) == RuntimeEventBus::kInvalidListenerId);
    CHECK(bus.On("event", {}) == RuntimeEventBus::kInvalidListenerId);
    CHECK(bus.GetMetrics().rejectedSubscriptions == 2U);

    std::size_t firstCalls = 0U;
    std::size_t lateCalls = 0U;
    bool callbackChecks = true;
    RuntimeEventBus::ListenerId firstId = RuntimeEventBus::kInvalidListenerId;
    firstId = bus.On("event", [&](const RuntimeEvent& event) {
        ++firstCalls;
        callbackChecks = callbackChecks && event.type == "event" && !event.id.empty()
            && event.fields.contains("sequence") && bus.Off("event", firstId);
        callbackChecks = callbackChecks
            && bus.On("event", [&](const RuntimeEvent&) { ++lateCalls; }) != RuntimeEventBus::kInvalidListenerId;
    });
    CHECK(firstId != RuntimeEventBus::kInvalidListenerId);

    RuntimeEvent mismatched{.type = "wrong"};
    bus.Emit("event", std::move(mismatched));
    CHECK(callbackChecks);
    CHECK(firstCalls == 1U);
    CHECK(lateCalls == 0U);
    bus.Emit("event");
    CHECK(firstCalls == 1U);
    CHECK(lateCalls == 1U);
    CHECK(!bus.Off("event", firstId));
    CHECK(bus.GetMetrics().listenersRemoved == 1U);

    RuntimeEvent scoped;
    scoped.fields["source_scope"] = "stale";
    bus.EmitScoped("scoped", "source", "target", std::move(scoped));
    const auto scopedRoute = bus.GetRecentSignalRoutes(1U);
    CHECK(scopedRoute.size() == 1U);
    CHECK(scopedRoute.front().sourceScope == "source");
    CHECK(scopedRoute.front().targetScope == "target");

    bus.Emit("");
    bus.Emit(std::string(257U, 'x'));
    CHECK(bus.GetMetrics().rejectedEmissions == 2U);

    for (std::size_t index = 0; index < 4200U; ++index) {
        bus.Emit("dynamic_" + std::to_string(index));
    }
    const auto metrics = bus.GetMetrics();
    CHECK(metrics.emittedByType.size() == 4096U);
    CHECK(metrics.untrackedEventTypes > 0U);
    CHECK(bus.GetRecentSignalRoutes(5000U).size() == 1024U);
    CHECK(bus.GetRecentSignalRoutes(0U).empty());
    const std::uint64_t sequenceBeforeClear = bus.GetRecentSignalRoutes(1U).front().sequence;
    const auto staleId = bus.On("fresh", [](const RuntimeEvent&) {});

    bus.Clear();
    const auto cleared = bus.GetMetrics();
    CHECK(cleared.emitted == 0U);
    CHECK(cleared.listenersAdded == 0U);
    CHECK(cleared.listenersRemoved == 0U);
    CHECK(cleared.activeListeners == 0U);
    CHECK(cleared.rejectedSubscriptions == 0U);
    CHECK(cleared.rejectedEmissions == 0U);
    CHECK(cleared.untrackedEventTypes == 0U);
    CHECK(cleared.emittedByType.empty());
    CHECK(bus.GetRecentSignalRoutes().empty());
    std::size_t freshCalls = 0U;
    const auto freshId = bus.On("fresh", [&](const RuntimeEvent&) { ++freshCalls; });
    CHECK(freshId > staleId);
    CHECK(!bus.Off("fresh", staleId));
    bus.Emit("fresh");
    CHECK(freshCalls == 1U);
    CHECK(bus.GetRecentSignalRoutes(1U).front().sequence > sequenceBeforeClear);
    return EXIT_SUCCESS;
}
