#include "RawIron/Logic/LogicVisualPrimitives.h"
#include "RawIron/Logic/LogicKitManifest.h"
#include "RawIron/Logic/LogicPortSchema.h"

#include <algorithm>
#include <cmath>

namespace ri::logic {
namespace {

[[nodiscard]] std::array<float, 3> AddVec3(const std::array<float, 3>& lhs, const std::array<float, 3>& rhs) {
    return std::array<float, 3>{lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}

[[nodiscard]] LogicVisualPrimitiveDefinition MakePrimitive(std::string id,
                                                           const LogicVisualPrimitiveKind kind,
                                                           const std::array<float, 3>& localPosition,
                                                           const std::array<float, 3>& localScale,
                                                           const std::array<float, 3>& inactiveColor,
                                                           const std::array<float, 3>& activeColor) {
    LogicVisualPrimitiveDefinition primitive{};
    primitive.id = std::move(id);
    primitive.kind = kind;
    primitive.localPosition = localPosition;
    primitive.localScale = localScale;
    primitive.inactiveColor = inactiveColor;
    primitive.activeColor = activeColor;
    primitive.inactiveEmissive = std::array<float, 3>{0.02f, 0.02f, 0.03f};
    primitive.activeEmissive = std::array<float, 3>{0.18f, 0.20f, 0.06f};
    return primitive;
}

[[nodiscard]] LogicVisualNodeStyle BuildNodeStyleForKind(std::string_view kind) {
    const LogicNodePortSchema schema = GetLogicNodePortSchema(kind);
    LogicVisualNodeStyle style{};
    style.nodeKind = std::string(kind);

    // Node body
    style.primitives.push_back(MakePrimitive(
        "body", LogicVisualPrimitiveKind::NodeBody, std::array<float, 3>{0.0f, 0.0f, 0.0f},
        std::array<float, 3>{1.6f, 1.2f, 1.0f}, std::array<float, 3>{0.24f, 0.24f, 0.28f},
        std::array<float, 3>{0.85f, 0.9f, 0.25f}));
    style.primitives.push_back(MakePrimitive(
        "label_anchor", LogicVisualPrimitiveKind::LabelAnchor, std::array<float, 3>{0.0f, 0.9f, 0.0f},
        std::array<float, 3>{0.15f, 0.15f, 0.15f}, std::array<float, 3>{0.75f, 0.75f, 0.75f},
        std::array<float, 3>{1.0f, 1.0f, 1.0f}));

    const std::size_t inputCount = std::max<std::size_t>(1, schema.inputs.size());
    const std::size_t outputCount = std::max<std::size_t>(1, schema.outputs.size());
    const float inputStep = inputCount > 1 ? (1.6f / static_cast<float>(inputCount - 1U)) : 0.0f;
    const float outputStep = outputCount > 1 ? (1.6f / static_cast<float>(outputCount - 1U)) : 0.0f;

    for (std::size_t i = 0; i < schema.inputs.size(); ++i) {
        const float z = -0.8f + (inputStep * static_cast<float>(i));
        style.primitives.push_back(MakePrimitive(
            "in_stub_" + std::to_string(i), LogicVisualPrimitiveKind::InputStub, std::array<float, 3>{-1.0f, 0.0f, z},
            std::array<float, 3>{0.22f, 0.22f, 0.22f}, std::array<float, 3>{0.65f, 0.65f, 0.70f},
            std::array<float, 3>{1.0f, 1.0f, 1.0f}));
    }
    for (std::size_t i = 0; i < schema.outputs.size(); ++i) {
        const float z = -0.8f + (outputStep * static_cast<float>(i));
        style.primitives.push_back(MakePrimitive(
            "out_stub_" + std::to_string(i), LogicVisualPrimitiveKind::OutputStub,
            std::array<float, 3>{1.0f, 0.0f, z}, std::array<float, 3>{0.22f, 0.22f, 0.22f},
            std::array<float, 3>{0.65f, 0.65f, 0.70f}, std::array<float, 3>{1.0f, 1.0f, 1.0f}));
    }

    return style;
}

[[nodiscard]] LogicVisualPrimitiveInstance ToInstance(const LogicVisualPrimitiveDefinition& primitive,
                                                      std::string_view nodeId,
                                                      const std::array<float, 3>& worldPosition,
                                                      const bool active) {
    LogicVisualPrimitiveInstance instance{};
    instance.id = std::string(nodeId) + ":" + primitive.id;
    instance.kind = primitive.kind;
    instance.worldPosition = AddVec3(worldPosition, primitive.localPosition);
    instance.worldRotationDegrees = primitive.localRotationDegrees;
    instance.worldScale = primitive.localScale;
    instance.color = active ? primitive.activeColor : primitive.inactiveColor;
    instance.emissive = active ? primitive.activeEmissive : primitive.inactiveEmissive;
    return instance;
}

} // namespace

LogicVisualLibrary BuildDefaultLogicVisualLibrary() {
    LogicVisualLibrary library{};
    constexpr std::array<std::string_view, 62> kNodeKinds{
        "logic_relay",          "logic_timer",        "logic_counter",      "logic_compare",
        "logic_sequencer",      "logic_pulse",        "logic_latch",        "logic_channel",
        "logic_merge",          "logic_split",        "logic_predicate",    "logic_inventory_gate",
        "logic_trigger_detector",
        "gate_and",             "gate_or",            "gate_not",
        "gate_buf",             "gate_xnor",         "gate_xor",
        "gate_nand",            "gate_nor",          "math_abs",           "math_min",
        "math_max",             "math_clamp",        "math_round",         "math_lerp",
        "math_sign",            "math_add",          "math_sub",           "math_mult",
        "math_div",             "math_mod",          "math_compare",      "route_tee",
        "route_pass",           "route_mux",         "route_demux",       "route_select",
        "route_merge",          "route_unpack",      "route_pack",
        "mem_edge",             "flow_random",       "route_split",
        "flow_rise",            "flow_fall",         "flow_dbnc",
        "flow_oneshot",        "time_delay",        "time_clock",
        "time_watch",          "mem_sample",        "mem_chatter",
        "flow_do_once",
        "flow_relay",           "io_button",          "io_keypad",
        "io_display",           "io_audio",           "io_logger",
        "io_trigger"};
    for (std::string_view kind : kNodeKinds) {
        LogicVisualNodeStyle style = BuildNodeStyleForKind(kind);
        library.nodeStyles.emplace(style.nodeKind, std::move(style));
    }

    if (const LogicKitManifest* kit = ActiveLogicKitManifest()) {
        for (const LogicKitNodeManifestEntry& kitEntry : kit->nodes) {
            if (library.nodeStyles.contains(kitEntry.id)) {
                continue;
            }
            LogicVisualNodeStyle style = BuildNodeStyleForKind(kitEntry.id);
            library.nodeStyles.emplace(kitEntry.id, std::move(style));
        }
    }

    library.worldIoStyle.nodeKind = "world_io";
    library.worldIoStyle.primitives = {
        MakePrimitive("io_box", LogicVisualPrimitiveKind::IoDeviceBox, std::array<float, 3>{0.0f, 0.0f, 0.0f},
                      std::array<float, 3>{0.7f, 0.7f, 0.7f}, std::array<float, 3>{0.75f, 0.75f, 0.2f},
                      std::array<float, 3>{1.0f, 0.9f, 0.3f}),
        MakePrimitive("io_in_stub", LogicVisualPrimitiveKind::InputStub, std::array<float, 3>{-0.5f, 0.0f, 0.0f},
                      std::array<float, 3>{0.18f, 0.18f, 0.18f}, std::array<float, 3>{0.9f, 0.9f, 0.9f},
                      std::array<float, 3>{1.0f, 1.0f, 1.0f}),
        MakePrimitive("io_out_stub", LogicVisualPrimitiveKind::OutputStub, std::array<float, 3>{0.5f, 0.0f, 0.0f},
                      std::array<float, 3>{0.18f, 0.18f, 0.18f}, std::array<float, 3>{0.9f, 0.9f, 0.9f},
                      std::array<float, 3>{1.0f, 1.0f, 1.0f}),
    };

    library.wireSegmentStyle = MakePrimitive("wire_segment", LogicVisualPrimitiveKind::WireSegment,
                                             std::array<float, 3>{0.0f, 0.0f, 0.0f},
                                             std::array<float, 3>{1.0f, 0.12f, 0.12f},
                                             std::array<float, 3>{0.35f, 0.35f, 0.38f},
                                             std::array<float, 3>{0.95f, 0.85f, 0.25f});
    library.junctionStyle = MakePrimitive("junction", LogicVisualPrimitiveKind::Junction,
                                          std::array<float, 3>{0.0f, 0.0f, 0.0f},
                                          std::array<float, 3>{0.24f, 0.24f, 0.24f},
                                          std::array<float, 3>{0.7f, 0.7f, 0.7f},
                                          std::array<float, 3>{1.0f, 1.0f, 1.0f});
    return library;
}

const LogicVisualNodeStyle* FindLogicVisualNodeStyle(const LogicVisualLibrary& library, std::string_view nodeKind) {
    const auto it = library.nodeStyles.find(std::string(nodeKind));
    return it == library.nodeStyles.end() ? nullptr : &it->second;
}

std::vector<LogicVisualPrimitiveInstance> BuildLogicVisualNodeInstances(const LogicVisualLibrary& library,
                                                                        std::string_view nodeKind,
                                                                        std::string_view nodeId,
                                                                        const std::array<float, 3>& worldPosition,
                                                                        const bool active) {
    const LogicVisualNodeStyle* style = FindLogicVisualNodeStyle(library, nodeKind);
    if (style == nullptr) {
        return {};
    }
    std::vector<LogicVisualPrimitiveInstance> instances{};
    instances.reserve(style->primitives.size());
    for (const LogicVisualPrimitiveDefinition& primitive : style->primitives) {
        instances.push_back(ToInstance(primitive, nodeId, worldPosition, active));
    }
    return instances;
}

std::optional<LogicVisualPrimitiveInstance> BuildLogicVisualWireSegmentInstance(
    const LogicVisualLibrary& library,
    std::string_view segmentId,
    const std::array<float, 3>& worldPosition,
    const std::array<float, 3>& worldScale,
    const bool active) {
    LogicVisualPrimitiveInstance instance = ToInstance(library.wireSegmentStyle, segmentId, worldPosition, active);
    instance.worldScale = worldScale;
    return instance;
}

} // namespace ri::logic
