#include "RawIron/Logic/LogicPortSchema.h"

#include "RawIron/Logic/LogicKitManifest.h"
#include "RawIron/Logic/WorldActorPorts.h"

#include <type_traits>

namespace ri::logic {
namespace {

LogicNodePortSchema BuildSchema(std::string_view kind,
                                std::initializer_list<LogicPortDescriptor> inputs,
                                std::initializer_list<LogicPortDescriptor> outputs) {
    LogicNodePortSchema schema{};
    schema.kind = std::string(kind);
    schema.inputs.assign(inputs.begin(), inputs.end());
    schema.outputs.assign(outputs.begin(), outputs.end());
    return schema;
}

} // namespace

std::string_view GetLogicNodeKindName(const LogicNodeDefinition& definition) {
    return std::visit([](const auto& node) -> std::string_view {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, RelayNode>) return "logic_relay";
        if constexpr (std::is_same_v<T, TimerNode>) return "logic_timer";
        if constexpr (std::is_same_v<T, CounterNode>) return "logic_counter";
        if constexpr (std::is_same_v<T, CompareNode>) return "logic_compare";
        if constexpr (std::is_same_v<T, SequencerNode>) return "logic_sequencer";
        if constexpr (std::is_same_v<T, PulseNode>) return "logic_pulse";
        if constexpr (std::is_same_v<T, LatchNode>) return "logic_latch";
        if constexpr (std::is_same_v<T, ChannelNode>) return "logic_channel";
        if constexpr (std::is_same_v<T, MergeNode>) return "logic_merge";
        if constexpr (std::is_same_v<T, SplitNode>) return "logic_split";
        if constexpr (std::is_same_v<T, PredicateNode>) return "logic_predicate";
        if constexpr (std::is_same_v<T, InventoryGateNode>) return "logic_inventory_gate";
        if constexpr (std::is_same_v<T, TriggerDetectorNode>) return "logic_trigger_detector";
        if constexpr (std::is_same_v<T, GateAndNode>) return "gate_and";
        if constexpr (std::is_same_v<T, GateOrNode>) return "gate_or";
        if constexpr (std::is_same_v<T, GateNotNode>) return "gate_not";
        if constexpr (std::is_same_v<T, GateBufNode>) return "gate_buf";
        if constexpr (std::is_same_v<T, GateXnorNode>) return "gate_xnor";
        if constexpr (std::is_same_v<T, GateXorNode>) return "gate_xor";
        if constexpr (std::is_same_v<T, GateNandNode>) return "gate_nand";
        if constexpr (std::is_same_v<T, GateNorNode>) return "gate_nor";
        if constexpr (std::is_same_v<T, MathAbsNode>) return "math_abs";
        if constexpr (std::is_same_v<T, MathMinNode>) return "math_min";
        if constexpr (std::is_same_v<T, MathMaxNode>) return "math_max";
        if constexpr (std::is_same_v<T, MathClampNode>) return "math_clamp";
        if constexpr (std::is_same_v<T, MathRoundNode>) return "math_round";
        if constexpr (std::is_same_v<T, MathLerpNode>) return "math_lerp";
        if constexpr (std::is_same_v<T, MathSignNode>) return "math_sign";
        if constexpr (std::is_same_v<T, RouteTeeNode>) return "route_tee";
        if constexpr (std::is_same_v<T, RoutePassNode>) return "route_pass";
        if constexpr (std::is_same_v<T, RouteMuxNode>) return "route_mux";
        if constexpr (std::is_same_v<T, RouteDemuxNode>) return "route_demux";
        if constexpr (std::is_same_v<T, MathAddNode>) return "math_add";
        if constexpr (std::is_same_v<T, MathSubNode>) return "math_sub";
        if constexpr (std::is_same_v<T, MathMultNode>) return "math_mult";
        if constexpr (std::is_same_v<T, MathDivNode>) return "math_div";
        if constexpr (std::is_same_v<T, MathModNode>) return "math_mod";
        if constexpr (std::is_same_v<T, MathCompareNode>) return "math_compare";
        if constexpr (std::is_same_v<T, RouteSelectNode>) return "route_select";
        if constexpr (std::is_same_v<T, RouteMergeNode>) return "route_merge";
        if constexpr (std::is_same_v<T, RouteUnpackNode>) return "route_unpack";
        if constexpr (std::is_same_v<T, RoutePackNode>) return "route_pack";
        if constexpr (std::is_same_v<T, MemEdgeNode>) return "mem_edge";
        if constexpr (std::is_same_v<T, FlowRandomNode>) return "flow_random";
        if constexpr (std::is_same_v<T, RouteSplitNode>) return "route_split";
        if constexpr (std::is_same_v<T, FlowRiseNode>) return "flow_rise";
        if constexpr (std::is_same_v<T, FlowFallNode>) return "flow_fall";
        if constexpr (std::is_same_v<T, FlowDbncNode>) return "flow_dbnc";
        if constexpr (std::is_same_v<T, FlowOneshotNode>) return "flow_oneshot";
        if constexpr (std::is_same_v<T, TimeDelayNode>) return "time_delay";
        if constexpr (std::is_same_v<T, TimeClockNode>) return "time_clock";
        if constexpr (std::is_same_v<T, TimeWatchNode>) return "time_watch";
        if constexpr (std::is_same_v<T, MemSampleNode>) return "mem_sample";
        if constexpr (std::is_same_v<T, MemChatterNode>) return "mem_chatter";
        if constexpr (std::is_same_v<T, FlowDoOnceNode>) return "flow_do_once";
        if constexpr (std::is_same_v<T, FlowRelayNode>) return "flow_relay";
        if constexpr (std::is_same_v<T, IoButtonNode>) return "io_button";
        if constexpr (std::is_same_v<T, IoKeypadNode>) return "io_keypad";
        if constexpr (std::is_same_v<T, IoDisplayNode>) return "io_display";
        if constexpr (std::is_same_v<T, IoAudioNode>) return "io_audio";
        if constexpr (std::is_same_v<T, IoLoggerNode>) return "io_logger";
        if constexpr (std::is_same_v<T, IoTriggerNode>) return "io_trigger";
        return "logic_unknown";
    }, definition);
}

LogicNodePortSchema GetLogicNodePortSchema(std::string_view kind) {
    if (const LogicKitManifest* kit = ActiveLogicKitManifest()) {
        if (const LogicKitNodeManifestEntry* kitEntry = FindLogicKitNodeManifestEntry(*kit, kind)) {
            return BuildLogicNodePortSchemaFromKitEntry(*kitEntry);
        }
    }
    if (kind == "logic_relay") {
        return BuildSchema(kind,
                           {{"Trigger"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnTrigger"}});
    }
    if (kind == "logic_timer") {
        return BuildSchema(kind,
                           {{"Trigger"}, {"Start"}, {"Reset"}, {"Stop"}, {"Cancel"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnTimer"}, {"OnFinished"}});
    }
    if (kind == "logic_counter") {
        return BuildSchema(kind,
                           {{"Trigger", true}, {"Increment", true}, {"Add", true}, {"Decrement", true}, {"Subtract", true},
                            {"SetValue", true}, {"Set", true}, {"Reset"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnChanged"}, {"OnIncrement"}, {"OnDecrement"}, {"OnZero"}, {"OnHitMin"}, {"OnHitMax"}});
    }
    if (kind == "logic_compare") {
        return BuildSchema(kind,
                           {{"Trigger"}, {"Evaluate"}, {"Compare"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnTrue"}, {"OnFalse"}, {"OnBecomeTrue"}, {"OnBecomeFalse"}});
    }
    if (kind == "logic_sequencer") {
        return BuildSchema(kind,
                           {{"Trigger"}, {"Advance"}, {"Reset"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnStep"}, {"OnComplete"}});
    }
    if (kind == "logic_pulse") {
        return BuildSchema(kind,
                           {{"Trigger"}, {"Pulse"}, {"Cancel"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnActive"}, {"OnRise"}, {"OnFall"}});
    }
    if (kind == "logic_latch") {
        return BuildSchema(kind,
                           {{"Set"}, {"Reset"}, {"Toggle"}, {"Enable"}, {"Disable"}},
                           {{"OnTrue"}, {"OnFalse"}, {"OnChanged"}});
    }
    if (kind == "logic_channel") {
        return BuildSchema(kind,
                           {{"Send", true}, {"Trigger"}, {"Enable"}, {"Disable"}},
                           {{"OnMessage"}});
    }
    if (kind == "logic_merge") {
        return BuildSchema(kind,
                           {{"Trigger"}, {"In0"}, {"In1"}, {"In2"}, {"In3"}, {"Reset"}, {"Enable"}, {"Disable"}},
                           {{"OnTrigger"}});
    }
    if (kind == "logic_split") {
        return BuildSchema(kind,
                           {{"Trigger", true}, {"Enable"}, {"Disable"}},
                           {{"Branch0", true}, {"Branch1", true}, {"Branch2", true}, {"Branch3", true}});
    }
    if (kind == "logic_predicate") {
        return BuildSchema(kind,
                           {{"Trigger"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnPass"}, {"OnFail"}});
    }
    if (kind == "logic_inventory_gate") {
        return BuildSchema(kind,
                           {{"Evaluate"}, {"Trigger"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnPass"}, {"OnFail"}, {"OnBecomeTrue"}, {"OnBecomeFalse"}});
    }
    if (kind == "logic_trigger_detector") {
        return BuildSchema(kind,
                           {{"Trigger"}, {"Reset"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"OnPass"}, {"OnReject"}});
    }
    if (kind == "gate_and") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Out"}});
    }
    if (kind == "gate_or") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Out"}});
    }
    if (kind == "gate_not") {
        return BuildSchema(kind, {{"In"}}, {{"Out"}});
    }
    if (kind == "gate_buf") {
        return BuildSchema(kind, {{"In"}}, {{"Out"}});
    }
    if (kind == "gate_xnor") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Out"}});
    }
    if (kind == "gate_xor") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Out"}});
    }
    if (kind == "gate_nand") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Out"}});
    }
    if (kind == "gate_nor") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Out"}});
    }
    if (kind == "math_abs") {
        return BuildSchema(kind, {{"In"}}, {{"Out"}});
    }
    if (kind == "math_min") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Out"}});
    }
    if (kind == "math_max") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Out"}});
    }
    if (kind == "math_clamp") {
        return BuildSchema(kind, {{"Val"}, {"Lo"}, {"Hi"}}, {{"Out"}});
    }
    if (kind == "math_round") {
        return BuildSchema(kind, {{"In"}}, {{"Out"}});
    }
    if (kind == "route_tee") {
        return BuildSchema(kind, {{"In"}, {"Enable"}, {"Disable"}, {"Toggle"}}, {{"A"}, {"B"}});
    }
    if (kind == "math_lerp") {
        return BuildSchema(kind, {{"A"}, {"B"}, {"T"}}, {{"Out"}});
    }
    if (kind == "math_sign") {
        return BuildSchema(kind, {{"In"}}, {{"Sign"}, {"Zero"}});
    }
    if (kind == "route_pass") {
        return BuildSchema(kind, {{"In"}, {"En"}, {"Enable"}, {"Disable"}, {"Toggle"}}, {{"Out"}});
    }
    if (kind == "route_mux") {
        return BuildSchema(kind, {{"Sel"}, {"A"}, {"B"}, {"Enable"}, {"Disable"}, {"Toggle"}}, {{"Out"}});
    }
    if (kind == "route_demux") {
        return BuildSchema(kind, {{"Sel"}, {"In"}, {"Enable"}, {"Disable"}, {"Toggle"}}, {{"A"}, {"B"}});
    }
    if (kind == "math_add") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Sum"}, {"Carry"}});
    }
    if (kind == "math_sub") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Diff"}, {"Borrow"}});
    }
    if (kind == "math_mult") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Prod"}});
    }
    if (kind == "math_div") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"Quot"}, {"Rem"}});
    }
    if (kind == "math_mod") {
        return BuildSchema(kind, {{"Val"}, {"Mod"}}, {{"Rem"}});
    }
    if (kind == "math_compare") {
        return BuildSchema(kind, {{"A"}, {"B"}}, {{"A>B"}, {"A==B"}, {"A<B"}});
    }
    if (kind == "route_select") {
        return BuildSchema(kind,
                           {{"Sel"}, {"In0"}, {"In1"}, {"In2"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"Out"}});
    }
    if (kind == "route_merge") {
        return BuildSchema(kind,
                           {{"In_1"}, {"In_2"}, {"In_3"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"Out"}});
    }
    if (kind == "route_unpack") {
        return BuildSchema(kind, {{"BusIn"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"B0"}, {"B1"}, {"B2"}, {"B3"}});
    }
    if (kind == "route_pack") {
        return BuildSchema(kind,
                           {{"B0"}, {"B1"}, {"B2"}, {"B3"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"BusOut"}});
    }
    if (kind == "mem_edge") {
        return BuildSchema(kind, {{"Sig"}, {"Reset"}}, {{"Rise"}, {"Fall"}});
    }
    if (kind == "flow_random") {
        return BuildSchema(kind, {{"Trigger"}}, {{"Val"}, {"Min"}, {"Max"}});
    }
    if (kind == "route_split") {
        return BuildSchema(kind, {{"In"}, {"Enable"}, {"Disable"}, {"Toggle"}},
                           {{"Out_1"}, {"Out_2"}, {"Out_3"}});
    }
    if (kind == "flow_rise") {
        return BuildSchema(kind, {{"In"}, {"Arm"}}, {{"Pulse"}});
    }
    if (kind == "flow_fall") {
        return BuildSchema(kind, {{"In"}, {"Arm"}}, {{"Pulse"}});
    }
    if (kind == "flow_dbnc") {
        return BuildSchema(kind, {{"In"}, {"Ms"}, {"Rst"}}, {{"Out"}});
    }
    if (kind == "flow_oneshot") {
        return BuildSchema(kind, {{"Trig"}, {"Ms"}}, {{"Out"}, {"Busy"}});
    }
    if (kind == "time_delay") {
        return BuildSchema(kind, {{"In"}, {"Set_Ms"}}, {{"Out"}});
    }
    if (kind == "time_clock") {
        return BuildSchema(kind, {{"Enable"}, {"Disable"}, {"Toggle"}, {"Set_Hz"}}, {{"Tick"}});
    }
    if (kind == "time_watch") {
        return BuildSchema(kind, {{"Start"}, {"Stop"}, {"Rst"}}, {{"Ms"}, {"Run"}});
    }
    if (kind == "mem_sample") {
        return BuildSchema(kind, {{"Sig"}, {"Cap"}, {"Hold"}}, {{"Out"}});
    }
    if (kind == "mem_chatter") {
        return BuildSchema(kind, {{"Sig"}, {"Ms"}, {"Rst"}}, {{"Stable"}, {"Raw"}});
    }
    if (kind == "flow_do_once") {
        return BuildSchema(kind, {{"Trigger"}, {"Reset"}, {"Enable"}, {"Disable"}, {"Toggle"}}, {{"Fired"}});
    }
    if (kind == "flow_relay") {
        return BuildSchema(kind, {{"Trig"}, {"En"}, {"Dis"}}, {{"Out"}});
    }
    if (kind == "io_button") {
        return BuildSchema(kind, {{"Enable"}, {"Disable"}}, {{"Press"}, {"Release"}});
    }
    if (kind == "io_keypad") {
        return BuildSchema(kind, {{"Enable"}, {"Reset"}}, {{"Val"}, {"Enter"}});
    }
    if (kind == "io_display") {
        return BuildSchema(kind, {{"SetText"}, {"SetColor"}}, {{"Done"}});
    }
    if (kind == "io_audio") {
        return BuildSchema(kind, {{"Play"}, {"Stop"}, {"SetVol"}}, {{"Done"}});
    }
    if (kind == "io_logger") {
        return BuildSchema(kind, {{"Log"}, {"Warn"}, {"Err"}}, {});
    }
    if (kind == "io_trigger") {
        return BuildSchema(kind, {{"Arm"}, {"Disarm"}}, {{"Touch"}, {"Untouch"}});
    }
    return BuildSchema(kind, {}, {});
}

LogicNodePortSchema GetLogicNodePortSchema(const LogicNodeDefinition& definition) {
    return GetLogicNodePortSchema(GetLogicNodeKindName(definition));
}

LogicNodePortSchema GetWorldActorPortSchema(std::string_view actorKind) {
    using namespace ports;
    if (actorKind == "trigger_volume" || actorKind == "generic_trigger_volume") {
        return BuildSchema(actorKind,
                           {{std::string(kTriggerEnable)}, {std::string(kTriggerDisable)}},
                           {{std::string(kTriggerOnStartTouch)}, {std::string(kTriggerOnEndTouch)}, {std::string(kTriggerOnStay)}});
    }
    if (actorKind == "spawner") {
        return BuildSchema(actorKind,
                           {{std::string(kSpawnerSpawn)}, {std::string(kSpawnerDespawn)}, {"Enable"}, {"Disable"}},
                           {{std::string(kSpawnerOnSpawned)}, {std::string(kSpawnerOnDespawned)}, {std::string(kSpawnerOnFailed)}});
    }
    if (actorKind == "door") {
        return BuildSchema(actorKind,
                           {{std::string(kDoorOpen)}, {std::string(kDoorClose)}, {std::string(kDoorLock)},
                            {std::string(kDoorUnlock)}, {std::string(kDoorToggle)}, {"Enable"}, {"Disable"}},
                           {{std::string(kDoorOnOpened)}, {std::string(kDoorOnClosed)}, {std::string(kDoorOnLocked)}});
    }
    if (actorKind == "interactable" || actorKind == "keycard_reader") {
        return BuildSchema(actorKind,
                           {{"Enable"}, {"Disable"}},
                           {{std::string(kInteractOnInteract)}, {std::string(kInteractOnScan)}});
    }
    if (actorKind == "fx" || actorKind == "light" || actorKind == "audio") {
        return BuildSchema(actorKind,
                           {{std::string(kFxEnable)}, {std::string(kFxDisable)}, {std::string(kFxSetIntensity), true},
                            {std::string(kFxPlay)}},
                           {});
    }
    return BuildSchema(actorKind, {}, {});
}

} // namespace ri::logic
