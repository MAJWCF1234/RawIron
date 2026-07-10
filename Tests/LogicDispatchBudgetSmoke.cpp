#include "RawIron/Logic/LogicGraph.h"

#include <cstdlib>
#include <limits>
#include <utility>

int main() {
    ri::logic::LogicGraphSpec spec{};
    spec.nodes.push_back(ri::logic::RelayNode{.id = "relay_a", .def = {}});
    spec.nodes.push_back(ri::logic::RelayNode{.id = "relay_b", .def = {}});
    spec.routes.push_back(ri::logic::LogicRoute{
        .sourceId = "relay_a",
        .outputName = "OnTrigger",
        .targets = {{.targetId = "relay_b", .inputName = "Trigger"}},
    });
    spec.routes.push_back(ri::logic::LogicRoute{
        .sourceId = "relay_b",
        .outputName = "OnTrigger",
        .targets = {{.targetId = "relay_a", .inputName = "Trigger"}},
    });

    ri::logic::LogicGraph graph(std::move(spec));
    std::size_t outputCount = 0;
    graph.SetOutputHandler([&](const ri::logic::LogicOutputEvent&) { ++outputCount; });
    graph.DispatchInput("relay_a", "Trigger", {});

    const ri::logic::LogicGraphRuntimeMetrics metrics = graph.RuntimeMetrics();
    if (metrics.immediateDispatchCount != ri::logic::kLogicMaxImmediateDispatchDepth
        || metrics.maxImmediateDispatchDepth != ri::logic::kLogicMaxImmediateDispatchDepth
        || metrics.droppedImmediateDispatchCount != 1U
        || outputCount != ri::logic::kLogicMaxImmediateDispatchDepth) {
        return EXIT_FAILURE;
    }

    graph.ResetRuntimeMetrics();
    const ri::logic::LogicGraphRuntimeMetrics reset = graph.RuntimeMetrics();
    if (reset.immediateDispatchCount != 0U || reset.droppedImmediateDispatchCount != 0U
        || reset.maxImmediateDispatchDepth != 0U) {
        return EXIT_FAILURE;
    }
    graph.AdvanceTime(std::numeric_limits<std::uint64_t>::max() - 5U);
    graph.AdvanceTime(10U);
    if (graph.NowMs() != std::numeric_limits<std::uint64_t>::max()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
