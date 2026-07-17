#include "RawIron/Logic/LogicPortSchema.h"

#include "RawIron/Logic/LogicKitManifest.h"
#include "RawIron/Logic/WorldActorPorts.h"

#include <array>

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
    static constexpr std::array<std::string_view, std::variant_size_v<LogicNodeDefinition>> kKindNames{
        "logic_relay", "logic_timer", "logic_counter", "logic_compare", "logic_sequencer",
        "logic_pulse", "logic_latch", "logic_channel", "logic_merge", "logic_split",
        "logic_predicate", "logic_inventory_gate", "logic_trigger_detector", "gate_and", "gate_or",
        "gate_not", "gate_buf", "gate_xnor", "gate_xor", "gate_nand", "gate_nor", "math_abs",
        "math_min", "math_max", "math_clamp", "math_round", "math_lerp", "math_sign", "route_tee",
        "route_pass", "route_mux", "route_demux", "math_add", "math_sub", "math_mult", "math_div",
        "math_mod", "math_compare", "route_select", "route_merge", "route_unpack", "route_pack",
        "mem_edge", "flow_random", "route_split", "flow_rise", "flow_fall", "flow_dbnc",
        "flow_oneshot", "time_delay", "time_clock", "time_watch", "mem_sample", "mem_chatter",
        "flow_do_once", "flow_relay", "io_button", "io_keypad", "io_display", "io_audio", "io_logger",
        "io_trigger",
    };
    return definition.valueless_by_exception() ? std::string_view{"logic_unknown"}
                                               : kKindNames[definition.index()];
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
