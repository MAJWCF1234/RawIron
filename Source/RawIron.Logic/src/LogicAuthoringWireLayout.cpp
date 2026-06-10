#include "RawIron/Logic/LogicAuthoringWireLayout.h"

#include "RawIron/Logic/LogicPortSchema.h"
#include "RawIron/Logic/LogicVisualPrimitives.h"
#include "RawIron/Logic/WorldActorPorts.h"

#include <algorithm>
#include <cmath>

namespace ri::logic {
namespace {

[[nodiscard]] std::array<float, 3> QuadraticBezier(const std::array<float, 3>& p0,
                                                     const std::array<float, 3>& p1,
                                                     const std::array<float, 3>& p2,
                                                     const float t) {
    const float u = 1.0f - t;
    return {
        p0[0] * (u * u) + p1[0] * (2.0f * u * t) + p2[0] * (t * t),
        p0[1] * (u * u) + p1[1] * (2.0f * u * t) + p2[1] * (t * t),
        p0[2] * (u * u) + p1[2] * (2.0f * u * t) + p2[2] * (t * t),
    };
}

[[nodiscard]] float Length3(const std::array<float, 3>& v) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

[[nodiscard]] std::array<float, 3> Sub3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

[[nodiscard]] std::array<float, 3> Add3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

[[nodiscard]] std::array<float, 3> Scale3(const std::array<float, 3>& a, const float s) {
    return {a[0] * s, a[1] * s, a[2] * s};
}

void ExtractPortAnchors(const std::string& actorId,
                        const std::string_view kitId,
                        const std::vector<LogicVisualPrimitiveInstance>& instances,
                        LogicEditorPortLayout& layout) {
    const LogicNodePortSchema schema = GetLogicNodePortSchema(kitId);
    for (const LogicVisualPrimitiveInstance& instance : instances) {
        const std::size_t colon = instance.id.rfind(':');
        const std::string stubId = colon == std::string::npos ? instance.id : instance.id.substr(colon + 1);
        if (instance.kind == LogicVisualPrimitiveKind::InputStub) {
            if (stubId.rfind("in_stub_", 0) == 0) {
                try {
                    const std::size_t index = static_cast<std::size_t>(std::stoul(stubId.substr(8)));
                    if (index < schema.inputs.size()) {
                        layout.inputPortsByActor[actorId][schema.inputs[index].name] = instance.worldPosition;
                    }
                } catch (...) {
                }
            }
        } else if (instance.kind == LogicVisualPrimitiveKind::OutputStub) {
            if (stubId.rfind("out_stub_", 0) == 0) {
                try {
                    const std::size_t index = static_cast<std::size_t>(std::stoul(stubId.substr(9)));
                    if (index < schema.outputs.size()) {
                        layout.outputPortsByActor[actorId][schema.outputs[index].name] = instance.worldPosition;
                    }
                } catch (...) {
                }
            }
        }
    }
}

[[nodiscard]] std::optional<std::string> FirstPortName(const LogicNodePortSchema& schema, const bool input) {
    const std::vector<LogicPortDescriptor>& ports = input ? schema.inputs : schema.outputs;
    if (ports.empty()) {
        return std::nullopt;
    }
    return ports.front().name;
}

} // namespace

LogicEditorPortLayout BuildLogicEditorPortLayout(const LogicAuthoringEditorFile& file) {
    LogicEditorPortLayout layout{};
    const LogicVisualLibrary library = BuildDefaultLogicVisualLibrary();
    for (const LogicAuthoringEditorNodeRecord& record : file.nodes) {
        const std::array<float, 3> worldPos = record.position;
        const std::vector<LogicVisualPrimitiveInstance> instances =
            BuildLogicVisualNodeInstances(library, record.kitId, record.logicNodeId, worldPos, false);
        ExtractPortAnchors(record.logicNodeId, record.kitId, instances, layout);
    }
    for (const LogicAuthoringEditorTriggerRecord& trigger : file.triggers) {
        layout.outputPortsByActor[trigger.triggerId][std::string(ports::kTriggerOnStartTouch)] = {
            trigger.position[0] + 0.6f, trigger.position[1], trigger.position[2]};
        layout.inputPortsByActor[trigger.triggerId]["OnStartTouch"] = {
            trigger.position[0] - 0.6f, trigger.position[1], trigger.position[2]};
    }
    return layout;
}

std::optional<std::array<float, 3>> ResolveLogicEditorWireEndpoint(const LogicEditorPortLayout& layout,
                                                                    const LogicAuthoringEditorFile& file,
                                                                    const std::string_view actorId,
                                                                    const std::string_view portName,
                                                                    const bool input) {
    const std::string actorKey(actorId);
    if (input) {
        const auto actorIt = layout.inputPortsByActor.find(actorKey);
        if (actorIt != layout.inputPortsByActor.end()) {
            const auto portIt = actorIt->second.find(std::string(portName));
            if (portIt != actorIt->second.end()) {
                return portIt->second;
            }
        }
    } else {
        const auto actorIt = layout.outputPortsByActor.find(actorKey);
        if (actorIt != layout.outputPortsByActor.end()) {
            const auto portIt = actorIt->second.find(std::string(portName));
            if (portIt != actorIt->second.end()) {
                return portIt->second;
            }
        }
    }

    for (const LogicAuthoringEditorNodeRecord& record : file.nodes) {
        if (record.logicNodeId != actorId) {
            continue;
        }
        const LogicNodePortSchema schema = GetLogicNodePortSchema(record.kitId);
        const std::optional<std::string> fallback = FirstPortName(schema, input);
        const std::array<float, 3> offset = input ? std::array<float, 3>{-1.0f, 0.0f, 0.0f}
                                                  : std::array<float, 3>{1.0f, 0.0f, 0.0f};
        return std::array<float, 3>{
            record.position[0] + offset[0],
            record.position[1] + offset[1],
            record.position[2] + offset[2],
        };
    }
    for (const LogicAuthoringEditorTriggerRecord& trigger : file.triggers) {
        if (trigger.triggerId != actorId) {
            continue;
        }
        const std::array<float, 3> offset = input ? std::array<float, 3>{-0.6f, 0.0f, 0.0f}
                                                  : std::array<float, 3>{0.6f, 0.0f, 0.0f};
        return std::array<float, 3>{
            trigger.position[0] + offset[0],
            trigger.position[1] + offset[1],
            trigger.position[2] + offset[2],
        };
    }
    return std::nullopt;
}

std::vector<std::array<float, 3>> BuildLogicWireBezierBeadPositions(const std::array<float, 3>& from,
                                                                       const std::array<float, 3>& to) {
    const std::array<float, 3> delta = Sub3(to, from);
    const float span = Length3(delta);
    if (span < 0.04f) {
        return {};
    }
    const float lift = std::clamp(0.24f * span, 0.28f, 1.85f);
    const std::array<float, 3> p1 = Add3(Scale3(Add3(from, to), 0.5f), std::array<float, 3>{0.0f, lift, 0.0f});
    const int beads = std::clamp(static_cast<int>(span / 0.18f), 10, 48);
    std::vector<std::array<float, 3>> out;
    out.reserve(static_cast<std::size_t>(beads + 1));
    for (int i = 0; i <= beads; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(beads);
        out.push_back(QuadraticBezier(from, p1, to, t));
    }
    return out;
}

} // namespace ri::logic
