#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace ri::logic {

/// Maximum delay for routed activations (24 hours).
inline constexpr std::uint64_t kMaxLogicDelayMs = 86'400'000ULL;

struct LogicContext {
    /// Last node that produced this activation (updated as the graph walks).
    std::string sourceId;
    /// Stable id for the actor that caused the pulse (player, AI, etc.).
    std::string instigatorId;
    /// Opaque string fields (tags, kinds, debug).
    std::unordered_map<std::string, std::string> fields;
    /// Optional scalar carried on edges or node inputs (e.g. counter delta).
    std::optional<double> parameter;
    /// Optional analog “how hard is this pulse” (voltage, pressure, any game unit). No engine-imposed maximum.
    std::optional<double> analogSignal;
};

struct LogicRouteTarget {
    std::string targetId;
    std::string inputName;
    std::uint32_t delayMs = 0;
    std::optional<double> edgeParameter;
};

struct LogicRoute {
    std::string sourceId;
    std::string outputName;
    std::vector<LogicRouteTarget> targets;
};

struct LogicOutputEvent {
    std::string sourceId;
    std::string outputName;
    LogicContext context;
};

// --- Node definitions (authoring / compile target) ---------------------------------

struct RelayDef {
    bool startEnabled = true;
};

struct TimerDef {
    std::uint64_t intervalMs = 0;
    bool repeating = false;
    bool autoStart = false;
    bool startEnabled = true;
};

struct CounterDef {
    double startValue = 0;
    std::optional<double> minValue;
    std::optional<double> maxValue;
    double step = 1;
    bool startEnabled = true;
};

struct CompareDef {
    std::optional<std::string> sourceLogicEntityId;
    std::optional<std::string> sourceWorldValueKey;
    std::string sourceProperty = "value";
    std::optional<double> equalsValue;
    std::optional<double> minValue;
    std::optional<double> maxValue;
    /// Constant numeric observed value when no external source (JSON `value` number).
    std::optional<double> constantValue;
    /// Constant observed value as text when no external source (JSON `value` string); used for truthy tests.
    std::optional<std::string> constantText;
    bool evaluateOnSpawn = false;
    bool startEnabled = true;
};

struct SequencerDef {
    int stepCount = 1;
    bool wrap = true;
    bool startEnabled = true;
};

enum class PulseRetriggerMode { Extend, Ignore, Restart };

struct PulseDef {
    std::uint64_t holdMs = 0;
    PulseRetriggerMode retrigger = PulseRetriggerMode::Extend;
    bool startEnabled = true;
};

enum class LatchMode { Sr, Toggle, PrioritySet };

struct LatchDef {
    bool startValue = false;
    LatchMode mode = LatchMode::Sr;
};

enum class ChannelRole { Publish, Subscribe };

struct ChannelDef {
    std::string channelName;
    ChannelRole role = ChannelRole::Publish;
    bool startEnabled = true;
};

enum class MergeMode { Any, All };

struct MergeDef {
    MergeMode mode = MergeMode::Any;
    int expectedInputs = 2;
    bool startEnabled = true;
};

struct SplitDef {
    int branchCount = 2;
    bool startEnabled = true;
    /// Per `BranchK` output: scales `context.parameter` (or 1) by this factor when present.
    std::vector<double> branchScales;
    /// Added to route `delayMs` for that branch’s edges (intrinsic per-branch delay).
    std::vector<std::uint32_t> branchIntrinsicDelayMs;
};

struct PredicateDef {
    /// Simplified rules: key "instigatorMustBe" -> "player" / "npc", "hasFlag" -> flag name.
    std::vector<std::pair<std::string, std::string>> rules;
    bool startEnabled = true;
};

struct InventoryGateDef {
    std::string itemId;
    bool consumeOnPass = false;
    int quantity = 1;
    bool startEnabled = true;
};

enum class TriggerInstigatorFilter { Any, Player, Npc, Tag };

struct TriggerDetectorDef {
    bool oncePerInstigator = false;
    std::uint64_t cooldownMs = 0;
    TriggerInstigatorFilter instigatorFilter = TriggerInstigatorFilter::Any;
    /// When `instigatorFilter == Tag`, matched against comma-separated `context.fields["tags"]`.
    std::string instigatorTag;
    bool requireExitBeforeRetrigger = false;
    bool startEnabled = true;
};

struct RelayNode {
    std::string id;
    RelayDef def;
};

struct TimerNode {
    std::string id;
    TimerDef def;
};

struct CounterNode {
    std::string id;
    CounterDef def;
};

struct CompareNode {
    std::string id;
    CompareDef def;
};

struct SequencerNode {
    std::string id;
    SequencerDef def;
};

struct PulseNode {
    std::string id;
    PulseDef def;
};

struct LatchNode {
    std::string id;
    LatchDef def;
};

struct ChannelNode {
    std::string id;
    ChannelDef def;
};

struct MergeNode {
    std::string id;
    MergeDef def;
};

struct SplitNode {
    std::string id;
    SplitDef def;
};

struct PredicateNode {
    std::string id;
    PredicateDef def;
};

struct InventoryGateNode {
    std::string id;
    InventoryGateDef def;
};

struct TriggerDetectorNode {
    std::string id;
    TriggerDetectorDef def;
};

struct GateAndDef {
    bool startEnabled = true;
};

struct GateOrDef {
    bool startEnabled = true;
};

struct GateNotDef {
    bool startEnabled = true;
};

struct GateBufDef {
    bool startEnabled = true;
};

struct GateAndNode {
    std::string id;
    GateAndDef def;
};

struct GateOrNode {
    std::string id;
    GateOrDef def;
};

struct GateNotNode {
    std::string id;
    GateNotDef def;
};

struct GateBufNode {
    std::string id;
    GateBufDef def;
};

struct GateXnorDef {
    bool startEnabled = true;
};

struct GateXnorNode {
    std::string id;
    GateXnorDef def;
};

struct GateXorDef {
    bool startEnabled = true;
};

struct GateXorNode {
    std::string id;
    GateXorDef def;
};

struct GateNandDef {
    bool startEnabled = true;
};

struct GateNandNode {
    std::string id;
    GateNandDef def;
};

struct GateNorDef {
    bool startEnabled = true;
};

struct GateNorNode {
    std::string id;
    GateNorDef def;
};

struct MathAbsDef {
    bool startEnabled = true;
};

struct MathAbsNode {
    std::string id;
    MathAbsDef def;
};

struct MathMinDef {
    bool startEnabled = true;
};

struct MathMinNode {
    std::string id;
    MathMinDef def;
};

struct MathMaxDef {
    bool startEnabled = true;
};

struct MathMaxNode {
    std::string id;
    MathMaxDef def;
};

struct MathClampDef {
    bool startEnabled = true;
};

struct MathClampNode {
    std::string id;
    MathClampDef def;
};

struct MathRoundDef {
    bool startEnabled = true;
};

struct MathRoundNode {
    std::string id;
    MathRoundDef def;
};

struct RouteTeeDef {
    bool startEnabled = true;
};

struct RouteTeeNode {
    std::string id;
    RouteTeeDef def;
};

struct MathLerpDef {
    bool startEnabled = true;
};

struct MathLerpNode {
    std::string id;
    MathLerpDef def;
};

struct MathSignDef {
    bool startEnabled = true;
};

struct MathSignNode {
    std::string id;
    MathSignDef def;
};

struct RoutePassDef {
    bool startEnabled = true;
};

struct RoutePassNode {
    std::string id;
    RoutePassDef def;
};

struct RouteMuxDef {
    bool startEnabled = true;
};

struct RouteMuxNode {
    std::string id;
    RouteMuxDef def;
};

struct RouteDemuxDef {
    bool startEnabled = true;
};

struct RouteDemuxNode {
    std::string id;
    RouteDemuxDef def;
};

struct MathAddDef {
    bool startEnabled = true;
};

struct MathAddNode {
    std::string id;
    MathAddDef def;
};

struct MathSubDef {
    bool startEnabled = true;
};

struct MathSubNode {
    std::string id;
    MathSubDef def;
};

struct MathMultDef {
    bool startEnabled = true;
};

struct MathMultNode {
    std::string id;
    MathMultDef def;
};

struct MathDivDef {
    bool startEnabled = true;
};

struct MathDivNode {
    std::string id;
    MathDivDef def;
};

struct MathModDef {
    bool startEnabled = true;
};

struct MathModNode {
    std::string id;
    MathModDef def;
};

struct MathCompareDef {
    bool startEnabled = true;
};

struct MathCompareNode {
    std::string id;
    MathCompareDef def;
};

struct RouteSelectDef {
    bool startEnabled = true;
};

struct RouteSelectNode {
    std::string id;
    RouteSelectDef def;
};

struct RouteMergeDef {
    bool startEnabled = true;
};

struct RouteMergeNode {
    std::string id;
    RouteMergeDef def;
};

struct RouteUnpackDef {
    bool startEnabled = true;
};

struct RouteUnpackNode {
    std::string id;
    RouteUnpackDef def;
};

struct RoutePackDef {
    bool startEnabled = true;
};

struct RoutePackNode {
    std::string id;
    RoutePackDef def;
};

struct MemEdgeDef {
    bool startEnabled = true;
};

struct MemEdgeNode {
    std::string id;
    MemEdgeDef def;
};

struct FlowRandomDef {
    bool startEnabled = true;
};

struct FlowRandomNode {
    std::string id;
    FlowRandomDef def;
};

struct RouteSplitDef {
    bool startEnabled = true;
};

struct RouteSplitNode {
    std::string id;
    RouteSplitDef def;
};

struct FlowRiseDef {
    bool startEnabled = true;
};

struct FlowRiseNode {
    std::string id;
    FlowRiseDef def;
};

struct FlowFallDef {
    bool startEnabled = true;
};

struct FlowFallNode {
    std::string id;
    FlowFallDef def;
};

struct FlowDbncDef {
    bool startEnabled = true;
    /// Default debounce window when no Ms input has been seen yet.
    std::uint32_t defaultDebounceMs = 50;
};

struct FlowDbncNode {
    std::string id;
    FlowDbncDef def;
};

struct FlowOneshotDef {
    bool startEnabled = true;
    std::uint32_t defaultPulseMs = 100;
};

struct FlowOneshotNode {
    std::string id;
    FlowOneshotDef def;
};

struct TimeDelayDef {
    bool startEnabled = true;
    std::uint32_t defaultDelayMs = 100;
};

struct TimeDelayNode {
    std::string id;
    TimeDelayDef def;
};

struct TimeClockDef {
    bool startEnabled = true;
    /// Default tick rate before any Set_Hz input.
    double defaultHz = 1.0;
};

struct TimeClockNode {
    std::string id;
    TimeClockDef def;
};

struct TimeWatchDef {
    bool startEnabled = true;
};

struct TimeWatchNode {
    std::string id;
    TimeWatchDef def;
};

struct MemSampleDef {
    bool startEnabled = true;
};

struct MemSampleNode {
    std::string id;
    MemSampleDef def;
};

struct MemChatterDef {
    bool startEnabled = true;
    std::uint32_t defaultDebounceMs = 50;
};

struct MemChatterNode {
    std::string id;
    MemChatterDef def;
};

struct FlowDoOnceDef {
    bool startEnabled = true;
};

struct FlowDoOnceNode {
    std::string id;
    FlowDoOnceDef def;
};

struct FlowRelayDef {
    bool startEnabled = true;
};

struct FlowRelayNode {
    std::string id;
    FlowRelayDef def;
};

struct IoButtonDef {
    bool startEnabled = true;
};

struct IoButtonNode {
    std::string id;
    IoButtonDef def;
};

struct IoKeypadDef {
    bool startEnabled = true;
};

struct IoKeypadNode {
    std::string id;
    IoKeypadDef def;
};

struct IoDisplayDef {
    bool startEnabled = true;
};

struct IoDisplayNode {
    std::string id;
    IoDisplayDef def;
};

struct IoAudioDef {
    bool startEnabled = true;
};

struct IoAudioNode {
    std::string id;
    IoAudioDef def;
};

struct IoLoggerDef {
    bool startEnabled = true;
};

struct IoLoggerNode {
    std::string id;
    IoLoggerDef def;
};

struct IoTriggerDef {
    /// When true, the trigger accepts touch/untouch pulses without an explicit Arm.
    bool startArmed = false;
};

struct IoTriggerNode {
    std::string id;
    IoTriggerDef def;
};

using LogicNodeDefinition = std::variant<RelayNode,
                                         TimerNode,
                                         CounterNode,
                                         CompareNode,
                                         SequencerNode,
                                         PulseNode,
                                         LatchNode,
                                         ChannelNode,
                                         MergeNode,
                                         SplitNode,
                                         PredicateNode,
                                         InventoryGateNode,
                                         TriggerDetectorNode,
                                         GateAndNode,
                                         GateOrNode,
                                         GateNotNode,
                                         GateBufNode,
                                         GateXnorNode,
                                         GateXorNode,
                                         GateNandNode,
                                         GateNorNode,
                                         MathAbsNode,
                                         MathMinNode,
                                         MathMaxNode,
                                         MathClampNode,
                                         MathRoundNode,
                                         MathLerpNode,
                                         MathSignNode,
                                         RouteTeeNode,
                                         RoutePassNode,
                                         RouteMuxNode,
                                         RouteDemuxNode,
                                         MathAddNode,
                                         MathSubNode,
                                         MathMultNode,
                                         MathDivNode,
                                         MathModNode,
                                         MathCompareNode,
                                         RouteSelectNode,
                                         RouteMergeNode,
                                         RouteUnpackNode,
                                         RoutePackNode,
                                         MemEdgeNode,
                                         FlowRandomNode,
                                         RouteSplitNode,
                                         FlowRiseNode,
                                         FlowFallNode,
                                         FlowDbncNode,
                                         FlowOneshotNode,
                                         TimeDelayNode,
                                         TimeClockNode,
                                         TimeWatchNode,
                                         MemSampleNode,
                                         MemChatterNode,
                                         FlowDoOnceNode,
                                         FlowRelayNode,
                                         IoButtonNode,
                                         IoKeypadNode,
                                         IoDisplayNode,
                                         IoAudioNode,
                                         IoLoggerNode,
                                         IoTriggerNode>;

using InventoryQuery = std::function<bool(std::string_view instigatorId,
                                            const std::string& itemId,
                                            int quantity,
                                            bool consume)>;

using WorldValueQuery = std::function<std::optional<double>(std::string_view key)>;

struct LogicGraphSpec {
    std::vector<LogicNodeDefinition> nodes;
    std::vector<LogicRoute> routes;
    InventoryQuery inventoryQuery;
    WorldValueQuery worldValueQuery;
};

} // namespace ri::logic
