#include "RawIron/World/LogicEntityIoTelemetry.h"

#include "RawIron/Runtime/EntityIoTelemetry.h"

#include <cmath>
#include <string>

namespace ri::world {
namespace {

std::string FormatFiniteDouble(double value) {
    if (!std::isfinite(value)) {
        return "0";
    }
    std::string text = std::to_string(value);
    if (const std::size_t dot = text.find('.'); dot != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    return text.empty() ? std::string("0") : text;
}

void AppendLogicContext(ri::runtime::RuntimeEventFields& fields, const ri::logic::LogicContext& ctx) {
    using namespace ri::runtime::entity_io;
    if (!ctx.instigatorId.empty()) {
        fields[std::string(kFieldInstigatorId)] = ctx.instigatorId;
    }
    if (ctx.parameter.has_value()) {
        fields[std::string(kFieldParameter)] = FormatFiniteDouble(*ctx.parameter);
    }
    if (ctx.analogSignal.has_value()) {
        fields[std::string(kFieldAnalogSignal)] = FormatFiniteDouble(*ctx.analogSignal);
    }
}

} // namespace

void EmitEntityIoForLogicOutput(ri::runtime::RuntimeEventBus& bus,
                                EntityIoTracker* tracker,
                                const ri::logic::LogicOutputEvent& ev) {
    using namespace ri::runtime::entity_io;
    ri::runtime::RuntimeEventFields fields;
    fields[std::string(kFieldKind)] = std::string(kKindOutput);
    fields[std::string(kFieldSourceId)] = ev.sourceId;
    fields[std::string(kFieldOutputName)] = ev.outputName;
    AppendLogicContext(fields, ev.context);
    EmitEntityIo(bus, std::move(fields));
    if (tracker != nullptr) {
        tracker->IncrementOutputsFired();
    }
}

void EmitEntityIoForLogicInput(ri::runtime::RuntimeEventBus& bus,
                               EntityIoTracker* tracker,
                               std::string_view targetId,
                               std::string_view inputNameNormalized,
                               const ri::logic::LogicContext& ctx) {
    using namespace ri::runtime::entity_io;
    ri::runtime::RuntimeEventFields fields;
    fields[std::string(kFieldKind)] = std::string(kKindInput);
    fields[std::string(kFieldTargetId)] = std::string(targetId);
    fields[std::string(kFieldInputName)] = std::string(inputNameNormalized);
    if (!ctx.sourceId.empty()) {
        fields[std::string(kFieldSourceId)] = ctx.sourceId;
    }
    AppendLogicContext(fields, ctx);
    EmitEntityIo(bus, std::move(fields));
    if (tracker != nullptr) {
        tracker->IncrementInputsDispatched();
    }
}

void AttachLogicGraphEntityIoTelemetry(ri::logic::LogicGraph& graph,
                                       ri::runtime::RuntimeEventBus& eventBus,
                                       EntityIoTracker* tracker,
                                       ri::logic::LogicGraph::OutputHandler userOutputHandler) {
    graph.SetOutputHandler([userOutputHandler = std::move(userOutputHandler), &eventBus, tracker](const ri::logic::LogicOutputEvent& ev) {
        if (userOutputHandler) {
            userOutputHandler(ev);
        }
        EmitEntityIoForLogicOutput(eventBus, tracker, ev);
    });
    graph.SetInputDispatchHandler([&eventBus, tracker](std::string_view targetId,
                                                       std::string_view inputNameNormalized,
                                                       const ri::logic::LogicContext& ctx) {
        EmitEntityIoForLogicInput(eventBus, tracker, targetId, inputNameNormalized, ctx);
    });
}

} // namespace ri::world
