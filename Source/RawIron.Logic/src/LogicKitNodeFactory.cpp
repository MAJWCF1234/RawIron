#include "RawIron/Logic/LogicKitNodeFactory.h"

#include <unordered_map>

namespace ri::logic {
namespace {

template <typename NodeT>
[[nodiscard]] LogicNodeDefinition MakeDefaultNode(std::string nodeInstanceId) {
    NodeT node{};
    node.id = std::move(nodeInstanceId);
    return node;
}

[[nodiscard]] std::string NormalizeKitKey(std::string_view kitId) {
    std::string out;
    out.reserve(kitId.size());
    for (char ch : kitId) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

[[nodiscard]] const std::unordered_map<std::string, std::string>& KitAliasTable() {
    static const std::unordered_map<std::string, std::string> kAliases{
        {"mem_latch", "logic_latch"},
        {"mem_counter", "logic_counter"},
        {"flow_sequencer", "logic_sequencer"},
        {"logic_relay", "flow_relay"},
        {"logic_pulse", "flow_oneshot"},
        {"logic_trigger_detector", "io_trigger"},
    };
    return kAliases;
}

[[nodiscard]] std::string ResolveCanonicalKitId(std::string_view kitId) {
    const std::string normalized = NormalizeKitKey(kitId);
    const auto alias = KitAliasTable().find(normalized);
    if (alias != KitAliasTable().end()) {
        return alias->second;
    }
    return normalized;
}

[[nodiscard]] std::optional<LogicKitNodeFactoryResult> BuildExecutableNode(const std::string& canonicalId,
                                                                           std::string nodeInstanceId) {
    LogicKitNodeFactoryResult result{.canonicalKitId = canonicalId};

    if (canonicalId == "gate_and") {
        result.definition = MakeDefaultNode<GateAndNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "gate_or") {
        result.definition = MakeDefaultNode<GateOrNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "gate_xor") {
        result.definition = MakeDefaultNode<GateXorNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "gate_not") {
        result.definition = MakeDefaultNode<GateNotNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "gate_nand") {
        result.definition = MakeDefaultNode<GateNandNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "gate_nor") {
        result.definition = MakeDefaultNode<GateNorNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "gate_buf") {
        result.definition = MakeDefaultNode<GateBufNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "gate_xnor") {
        result.definition = MakeDefaultNode<GateXnorNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_add") {
        result.definition = MakeDefaultNode<MathAddNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_sub") {
        result.definition = MakeDefaultNode<MathSubNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_mult") {
        result.definition = MakeDefaultNode<MathMultNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_div") {
        result.definition = MakeDefaultNode<MathDivNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_mod") {
        result.definition = MakeDefaultNode<MathModNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_compare") {
        result.definition = MakeDefaultNode<MathCompareNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_abs") {
        result.definition = MakeDefaultNode<MathAbsNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_min") {
        result.definition = MakeDefaultNode<MathMinNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_max") {
        result.definition = MakeDefaultNode<MathMaxNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_clamp") {
        result.definition = MakeDefaultNode<MathClampNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_lerp") {
        result.definition = MakeDefaultNode<MathLerpNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_sign") {
        result.definition = MakeDefaultNode<MathSignNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "math_round") {
        result.definition = MakeDefaultNode<MathRoundNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "logic_latch") {
        result.definition = MakeDefaultNode<LatchNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "logic_counter") {
        result.definition = MakeDefaultNode<CounterNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "logic_sequencer") {
        result.definition = MakeDefaultNode<SequencerNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_mux") {
        result.definition = MakeDefaultNode<RouteMuxNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_demux") {
        result.definition = MakeDefaultNode<RouteDemuxNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_pack") {
        result.definition = MakeDefaultNode<RoutePackNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_unpack") {
        result.definition = MakeDefaultNode<RouteUnpackNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_merge") {
        result.definition = MakeDefaultNode<RouteMergeNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_split") {
        result.definition = MakeDefaultNode<RouteSplitNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_tee") {
        result.definition = MakeDefaultNode<RouteTeeNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_pass") {
        result.definition = MakeDefaultNode<RoutePassNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "route_select") {
        result.definition = MakeDefaultNode<RouteSelectNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "time_clock") {
        result.definition = MakeDefaultNode<TimeClockNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "time_delay") {
        result.definition = MakeDefaultNode<TimeDelayNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "time_watch") {
        result.definition = MakeDefaultNode<TimeWatchNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "flow_do_once") {
        result.definition = MakeDefaultNode<FlowDoOnceNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "flow_random") {
        result.definition = MakeDefaultNode<FlowRandomNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "flow_relay") {
        result.definition = MakeDefaultNode<FlowRelayNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "flow_rise") {
        result.definition = MakeDefaultNode<FlowRiseNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "flow_fall") {
        result.definition = MakeDefaultNode<FlowFallNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "flow_dbnc") {
        result.definition = MakeDefaultNode<FlowDbncNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "flow_oneshot") {
        result.definition = MakeDefaultNode<FlowOneshotNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "mem_edge") {
        result.definition = MakeDefaultNode<MemEdgeNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "mem_chatter") {
        result.definition = MakeDefaultNode<MemChatterNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "mem_sample") {
        result.definition = MakeDefaultNode<MemSampleNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "io_button") {
        result.definition = MakeDefaultNode<IoButtonNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "io_keypad") {
        result.definition = MakeDefaultNode<IoKeypadNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "io_display") {
        result.definition = MakeDefaultNode<IoDisplayNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "io_audio") {
        result.definition = MakeDefaultNode<IoAudioNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "io_logger") {
        result.definition = MakeDefaultNode<IoLoggerNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "io_trigger") {
        result.definition = MakeDefaultNode<IoTriggerNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "sense_tick") {
        result.definition = MakeDefaultNode<TimeClockNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "sense_prox" || canonicalId == "sense_zone" || canonicalId == "sense_overlap"
        || canonicalId == "sense_line" || canonicalId == "sense_ray" || canonicalId == "sense_tag"
        || canonicalId == "sense_scalar" || canonicalId == "sense_axis" || canonicalId == "sense_noise") {
        result.definition = MakeDefaultNode<PredicateNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "mem_flipflop") {
        LatchNode node{};
        node.id = std::move(nodeInstanceId);
        node.def.mode = LatchMode::DFlipFlop;
        result.definition = std::move(node);
        return result;
    }
    if (canonicalId == "mem_register" || canonicalId == "mem_ram_array") {
        result.definition = MakeDefaultNode<MemSampleNode>(std::move(nodeInstanceId));
        return result;
    }
    if (canonicalId == "mem_variable") {
        result.definition = MakeDefaultNode<CounterNode>(std::move(nodeInstanceId));
        return result;
    }

    if (canonicalId.rfind("sense_", 0) == 0) {
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace

std::optional<LogicKitNodeFactoryResult> CreateLogicNodeFromKitId(const std::string_view kitId,
                                                                std::string nodeInstanceId) {
    if (kitId.empty() || nodeInstanceId.empty()) {
        return std::nullopt;
    }
    const std::string canonicalId = ResolveCanonicalKitId(kitId);
    return BuildExecutableNode(canonicalId, std::move(nodeInstanceId));
}

bool LogicKitIdIsExecutable(const std::string_view kitId) {
    return CreateLogicNodeFromKitId(kitId, "probe").has_value();
}

std::string_view LogicKitIdCanonical(const std::string_view kitId) {
    static thread_local std::string storage;
    storage = ResolveCanonicalKitId(kitId);
    return storage;
}

} // namespace ri::logic
