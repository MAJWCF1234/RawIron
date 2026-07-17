#include "RawIron/Logic/LogicGraph.h"

#include <cstdlib>
#include <cmath>
#include <iostream>
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

    ri::logic::LogicGraphSpec parsingSpec{};
    parsingSpec.nodes.push_back(ri::logic::IoAudioNode{.id = "audio", .def = {}});
    parsingSpec.nodes.push_back(ri::logic::FlowRandomNode{.id = "random", .def = {}});
    ri::logic::LogicGraph parsingGraph(std::move(parsingSpec));
    double audioVolume = -1.0;
    double randomMin = -1.0;
    double randomMax = -1.0;
    parsingGraph.SetOutputHandler([&](const ri::logic::LogicOutputEvent& event) {
        if (event.sourceId == "audio" && event.outputName == "done" && event.context.parameter.has_value()) {
            audioVolume = *event.context.parameter;
        } else if (event.sourceId == "random" && event.outputName == "min" && event.context.parameter.has_value()) {
            randomMin = *event.context.parameter;
        } else if (event.sourceId == "random" && event.outputName == "max" && event.context.parameter.has_value()) {
            randomMax = *event.context.parameter;
        }
    });
    parsingGraph.DispatchInput("audio", "SetVol", {.fields = {{"volume", "0.25"}}});
    if (std::abs(audioVolume - 0.25) > 1.0e-9) {
        std::cerr << "strict audio parse baseline failed: " << audioVolume << '\n';
        return EXIT_FAILURE;
    }
    parsingGraph.DispatchInput("audio", "SetVol", {.fields = {{"volume", "0.75junk"}}});
    if (std::abs(audioVolume - 0.25) > 1.0e-9) {
        std::cerr << "audio parser accepted trailing garbage: " << audioVolume << '\n';
        return EXIT_FAILURE;
    }
    parsingGraph.DispatchInput("audio", "SetVol", {.fields = {{"volume", "nan"}}});
    if (std::abs(audioVolume - 0.25) > 1.0e-9) {
        std::cerr << "audio parser accepted non-finite value: " << audioVolume << '\n';
        return EXIT_FAILURE;
    }
    parsingGraph.DispatchInput("random", "Trigger", {.fields = {{"min", "5junk"}, {"max", "6"}}});
    if (randomMin != 0.0 || randomMax != 6.0) {
        std::cerr << "random range parser accepted malformed bound: " << randomMin << ", " << randomMax << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
