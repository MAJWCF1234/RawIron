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

    ri::logic::LogicGraphSpec debounceSpec{};
    debounceSpec.nodes.push_back(ri::logic::FlowDbncNode{.id = "debounce", .def = {}});
    ri::logic::LogicGraph debounceGraph(std::move(debounceSpec));
    std::size_t debouncedOutputCount = 0U;
    debounceGraph.SetOutputHandler([&](const ri::logic::LogicOutputEvent& event) {
        if (event.outputName == "out") {
            ++debouncedOutputCount;
        }
    });
    debounceGraph.DispatchInput("debounce", "Ms", {.parameter = std::numeric_limits<double>::max()});
    debounceGraph.DispatchInput("debounce", "In", {.analogSignal = 1.0});
    if (debouncedOutputCount != 0U) {
        return EXIT_FAILURE;
    }
    debounceGraph.AdvanceTime(ri::logic::kMaxLogicDelayMs - 1U);
    if (debouncedOutputCount != 0U) {
        return EXIT_FAILURE;
    }
    debounceGraph.AdvanceTime(1U);
    if (debouncedOutputCount != 1U) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
