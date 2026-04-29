#pragma once

#include "RawIron/Logic/LogicCircuitSignal.h"
#include "RawIron/Logic/LogicTypes.h"

#include <cstdint>
#include <functional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::logic {

/// Directed event graph executor: nodes, routes with optional delay, monotonic clock.
class LogicGraph {
public:
    using OutputHandler = std::function<void(const LogicOutputEvent&)>;
    using WorldActorInputHandler =
        std::function<void(std::string_view actorId, std::string_view inputName, const LogicContext& context)>;
    /// Fires once per \ref DispatchInput after input names are normalized (includes world-actor targets with no logic node).
    using InputDispatchHandler =
        std::function<void(std::string_view targetId, std::string_view inputNameNormalized, const LogicContext& context)>;

    explicit LogicGraph(LogicGraphSpec spec);

    void SetOutputHandler(OutputHandler handler) { outputHandler_ = std::move(handler); }
    void SetWorldActorInputHandler(WorldActorInputHandler handler) { worldActorInputHandler_ = std::move(handler); }
    void SetInputDispatchHandler(InputDispatchHandler handler) { inputDispatchHandler_ = std::move(handler); }

    [[nodiscard]] std::uint64_t NowMs() const { return nowMs_; }

    void AdvanceTime(std::uint64_t deltaMs);

    void DispatchInput(std::string_view nodeId, std::string_view inputName, LogicContext context);

    void Reset();

    [[nodiscard]] std::optional<double> TryGetCounterValue(std::string_view nodeId) const;

    [[nodiscard]] std::optional<bool> TryGetLatchValue(std::string_view nodeId) const;

    /// Numeric property for `logic_compare` `sourceProperty` (e.g. counter `value`, latch `value`).
    [[nodiscard]] std::optional<double> TryGetLogicNumericProperty(std::string_view nodeId,
                                                                     std::string_view propertyName) const;

    /// Map/world actors emit outputs by stable id (routes use this `sourceId` without a logic node).
    void EmitWorldOutput(std::string_view sourceId, std::string_view outputName, LogicContext context);

    /// Debug / editor overlay: per-node “is it powered?” and a scalar hint (\ref LogicCircuitSignal.h).
    [[nodiscard]] std::vector<LogicCircuitNodeProbe> ProbeCircuitNodes() const;

private:
    int dispatchDepth_ = 0;
    struct PendingDelivery {
        std::uint64_t fireAt = 0;
        std::uint64_t tieBreak = 0;
        std::string targetId;
        std::string inputName;
        LogicContext context;
    };

    struct PendingGreater {
        bool operator()(const PendingDelivery& a, const PendingDelivery& b) const {
            if (a.fireAt != b.fireAt) {
                return a.fireAt > b.fireAt;
            }
            return a.tieBreak > b.tieBreak;
        }
    };

    LogicGraphSpec spec_;
    OutputHandler outputHandler_;
    WorldActorInputHandler worldActorInputHandler_;
    InputDispatchHandler inputDispatchHandler_;
    std::uint64_t nowMs_ = 0;
    std::uint64_t nextTieBreak_ = 0;

    using RouteMap = std::unordered_map<std::string, std::vector<LogicRouteTarget>>;
    RouteMap routes_;

    std::priority_queue<PendingDelivery, std::vector<PendingDelivery>, PendingGreater> pending_;

    struct RelayRt {
        bool enabled = true;
    };
    struct TimerRt {
        bool enabled = true;
        bool active = false;
        std::uint64_t fireAt = 0;
        std::uint64_t startCount = 0;
        std::uint64_t scheduledWithStartCount = 0;
    };
    struct CounterRt {
        bool enabled = true;
        double value = 0;
    };
    struct CompareRt {
        bool enabled = true;
        std::optional<bool> lastResult;
    };
    struct SequencerRt {
        bool enabled = true;
        int index = 0;
    };
    struct PulseRt {
        bool enabled = true;
        bool held = false;
        std::uint64_t releaseAt = 0;
    };
    struct LatchRt {
        bool enabled = true;
        bool value = false;
        bool pendingSet = false;
        bool pendingReset = false;
    };
    struct ChannelRt {
        bool enabled = true;
    };
    struct MergeRt {
        bool enabled = true;
        std::uint32_t armedBits = 0;
    };
    struct SplitRt {
        bool enabled = true;
    };
    struct PredicateRt {
        bool enabled = true;
    };
    struct InventoryGateRt {
        bool enabled = true;
        std::optional<bool> lastResult;
    };
    struct TriggerDetectorRt {
        bool enabled = true;
        std::unordered_map<std::string, bool> onceSeen;
        std::unordered_map<std::string, bool> waitingExit;
        std::uint64_t lastGlobalFireMs = 0;
    };
    struct GateAndRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
    };
    struct GateOrRt {
        bool enabled = true;
    };
    struct GateNotRt {
        bool enabled = true;
    };
    struct GateBufRt {
        bool enabled = true;
        std::optional<double> lastAnalog;
    };
    struct GateXnorRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        bool hiA = false;
        bool hiB = false;
    };
    struct GateXorRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        bool hiA = false;
        bool hiB = false;
    };
    struct GateNandRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        bool hiA = false;
        bool hiB = false;
    };
    struct GateNorRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        bool hiA = false;
        bool hiB = false;
    };
    struct MathAbsRt {
        bool enabled = true;
        std::optional<double> lastOut;
    };
    struct MathMinRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        double lastA = 0;
        double lastB = 0;
        std::optional<double> lastOut;
    };
    struct MathMaxRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        double lastA = 0;
        double lastB = 0;
        std::optional<double> lastOut;
    };
    struct MathClampRt {
        bool enabled = true;
        bool armedVal = false;
        bool armedLo = false;
        bool armedHi = false;
        double lastVal = 0;
        double lastLo = 0;
        double lastHi = 0;
        std::optional<double> lastOut;
    };
    struct MathRoundRt {
        bool enabled = true;
        std::optional<double> lastOut;
    };
    struct MathLerpRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        bool armedT = false;
        double lastA = 0;
        double lastB = 0;
        double lastT = 0;
        std::optional<double> lastOut;
    };
    struct MathSignRt {
        bool enabled = true;
        std::optional<double> lastSign;
        bool lastZeroPulse = false;
    };
    struct MathAddRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        double lastA = 0;
        double lastB = 0;
        std::optional<double> lastSum;
        std::optional<double> lastCarry;
    };
    struct MathSubRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        double lastA = 0;
        double lastB = 0;
        std::optional<double> lastDiff;
        std::optional<double> lastBorrow;
    };
    struct MathMultRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        double lastA = 0;
        double lastB = 0;
        std::optional<double> lastOut;
    };
    struct MathDivRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        double lastA = 0;
        double lastB = 0;
        std::optional<double> lastQuot;
        std::optional<double> lastRem;
    };
    struct MathModRt {
        bool enabled = true;
        bool armedVal = false;
        bool armedMod = false;
        double lastVal = 0;
        double lastMod = 0;
        std::optional<double> lastOut;
    };
    struct MathCompareRt {
        bool enabled = true;
        bool armedA = false;
        bool armedB = false;
        double lastA = 0;
        double lastB = 0;
        /// -1 = A<B, 0 = A==B, 1 = A>B after last coincident compare; unset before first fire.
        std::optional<int> lastOrder;
    };
    struct RouteTeeRt {
        bool enabled = true;
    };
    struct RoutePassRt {
        bool enabled = true;
        bool passOpen = true;
    };
    struct RouteMuxRt {
        bool enabled = true;
        bool selHigh = false;
    };
    struct RouteDemuxRt {
        bool enabled = true;
        bool selHigh = false;
    };
    struct RouteSelectRt {
        bool enabled = true;
        int selIdx = 0;
        double lastSel = 0;
    };
    struct RouteMergeRt {
        bool enabled = true;
        bool armed1 = false;
        bool armed2 = false;
        bool armed3 = false;
    };
    struct RouteUnpackRt {
        bool enabled = true;
    };
    struct RoutePackRt {
        bool enabled = true;
        bool armed0 = false;
        bool armed1 = false;
        bool armed2 = false;
        bool armed3 = false;
        double v0 = 0;
        double v1 = 0;
        double v2 = 0;
        double v3 = 0;
    };
    struct MemEdgeRt {
        bool enabled = true;
        bool hasLast = false;
        bool lastHi = false;
    };
    struct FlowRandomRt {
        bool enabled = true;
        std::uint64_t lcgState = 0x9E3779B97F4A7C15ULL;
        std::optional<double> lastVal;
    };
    struct RouteSplitRt {
        bool enabled = true;
    };
    struct FlowRiseRt {
        bool enabled = true;
        bool armed = false;
        bool hasLastIn = false;
        bool lastInHi = false;
        bool reseedNextIn = false;
    };
    struct FlowFallRt {
        bool enabled = true;
        bool armed = false;
        bool hasLastIn = false;
        bool lastInHi = false;
        bool reseedNextIn = false;
    };
    struct FlowDbncRt {
        bool enabled = true;
        bool rawHi = false;
        bool pendingHi = false;
        bool stableHi = false;
        std::uint64_t settleAt = 0;
        std::uint32_t debounceMs = 50;
    };
    struct FlowOneshotRt {
        bool enabled = true;
        bool busy = false;
        std::uint64_t endAt = 0;
        std::uint32_t pulseMs = 100;
    };
    struct TimeDelayRt {
        bool enabled = true;
        std::uint32_t delayMs = 100;
        bool hasPending = false;
        std::uint64_t fireAt = 0;
        LogicContext pendingCtx{};
    };
    struct TimeClockRt {
        bool enabled = true;
        bool clockEnabled = false;
        double hz = 1.0;
        std::uint64_t periodMs = 1000;
        std::uint64_t nextTickAt = 0;
    };
    struct TimeWatchRt {
        bool enabled = true;
        bool running = false;
        std::uint64_t lapStartMs = 0;
        std::uint64_t accumulatedMs = 0;
    };
    struct MemSampleRt {
        bool enabled = true;
        bool holding = false;
        double live = 0.0;
        double latched = 0.0;
        std::optional<double> lastEmitted;
    };
    struct MemChatterRt {
        bool enabled = true;
        bool rawHi = false;
        bool pendingHi = false;
        bool stableHi = false;
        std::uint64_t settleAt = 0;
        std::uint32_t debounceMs = 50;
    };
    struct FlowDoOnceRt {
        bool enabled = true;
        bool consumed = false;
    };
    struct FlowRelayRt {
        bool enabled = true;
    };
    struct IoButtonRt {
        bool enabled = true;
        bool held = false;
    };
    struct IoKeypadRt {
        bool enabled = true;
        std::string buffer;
    };
    struct IoDisplayRt {
        bool enabled = true;
        std::string text;
        std::string color;
    };
    struct IoAudioRt {
        bool enabled = true;
        bool playing = false;
        double volume = 1.0;
        std::string lastClip;
    };
    struct IoLoggerRt {
        bool enabled = true;
        std::string lastLevel;
        std::string lastMessage;
        std::uint64_t lineCount = 0;
    };
    struct IoTriggerRt {
        bool armed = false;
        bool held = false;
    };

    enum class Kind {
        Relay,
        Timer,
        Counter,
        Compare,
        Sequencer,
        Pulse,
        Latch,
        Channel,
        Merge,
        Split,
        Predicate,
        InventoryGate,
        TriggerDetector,
        GateAnd,
        GateOr,
        GateNot,
        GateBuf,
        GateXnor,
        GateXor,
        GateNand,
        GateNor,
        MathAbs,
        MathMin,
        MathMax,
        MathClamp,
        MathRound,
        MathLerp,
        MathSign,
        MathAdd,
        MathSub,
        MathMult,
        MathDiv,
        MathMod,
        MathCompare,
        RouteTee,
        RoutePass,
        RouteMux,
        RouteDemux,
        RouteSelect,
        RouteMerge,
        RouteUnpack,
        RoutePack,
        MemEdge,
        FlowRandom,
        RouteSplit,
        FlowRise,
        FlowFall,
        FlowDbnc,
        FlowOneshot,
        TimeDelay,
        TimeClock,
        TimeWatch,
        MemSample,
        MemChatter,
        FlowDoOnce,
        FlowRelay,
        IoButton,
        IoKeypad,
        IoDisplay,
        IoAudio,
        IoLogger,
        IoTrigger
    };

    struct NodeSlot {
        Kind kind = Kind::Relay;
        RelayDef relayDef;
        TimerDef timerDef;
        CounterDef counterDef;
        CompareDef compareDef;
        SequencerDef sequencerDef;
        PulseDef pulseDef;
        LatchDef latchDef;
        ChannelDef channelDef;
        MergeDef mergeDef;
        SplitDef splitDef;
        PredicateDef predicateDef;
        InventoryGateDef inventoryGateDef;
        TriggerDetectorDef triggerDetectorDef;
        GateAndDef gateAndDef;
        GateOrDef gateOrDef;
        GateNotDef gateNotDef;
        GateBufDef gateBufDef;
        GateXnorDef gateXnorDef;
        GateXorDef gateXorDef;
        GateNandDef gateNandDef;
        GateNorDef gateNorDef;
        MathAbsDef mathAbsDef;
        MathMinDef mathMinDef;
        MathMaxDef mathMaxDef;
        MathClampDef mathClampDef;
        MathRoundDef mathRoundDef;
        MathLerpDef mathLerpDef;
        MathSignDef mathSignDef;
        MathAddDef mathAddDef;
        MathSubDef mathSubDef;
        MathMultDef mathMultDef;
        MathDivDef mathDivDef;
        MathModDef mathModDef;
        MathCompareDef mathCompareDef;
        RouteTeeDef routeTeeDef;
        RoutePassDef routePassDef;
        RouteMuxDef routeMuxDef;
        RouteDemuxDef routeDemuxDef;
        RouteSelectDef routeSelectDef;
        RouteMergeDef routeMergeDef;
        RouteUnpackDef routeUnpackDef;
        RoutePackDef routePackDef;
        MemEdgeDef memEdgeDef;
        FlowRandomDef flowRandomDef;
        RouteSplitDef routeSplitDef;
        FlowRiseDef flowRiseDef;
        FlowFallDef flowFallDef;
        FlowDbncDef flowDbncDef;
        FlowOneshotDef flowOneshotDef;
        TimeDelayDef timeDelayDef;
        TimeClockDef timeClockDef;
        TimeWatchDef timeWatchDef;
        MemSampleDef memSampleDef;
        MemChatterDef memChatterDef;
        FlowDoOnceDef flowDoOnceDef;
        FlowRelayDef flowRelayDef;
        IoButtonDef ioButtonDef;
        IoKeypadDef ioKeypadDef;
        IoDisplayDef ioDisplayDef;
        IoAudioDef ioAudioDef;
        IoLoggerDef ioLoggerDef;
        IoTriggerDef ioTriggerDef;

        RelayRt relay;
        TimerRt timer;
        CounterRt counter;
        CompareRt compare;
        SequencerRt sequencer;
        PulseRt pulse;
        LatchRt latch;
        ChannelRt channel;
        MergeRt merge;
        SplitRt split;
        PredicateRt predicate;
        InventoryGateRt inventoryGate;
        TriggerDetectorRt triggerDetector;
        GateAndRt gateAnd;
        GateOrRt gateOr;
        GateNotRt gateNot;
        GateBufRt gateBuf;
        GateXnorRt gateXnor;
        GateXorRt gateXor;
        GateNandRt gateNand;
        GateNorRt gateNor;
        MathAbsRt mathAbs;
        MathMinRt mathMin;
        MathMaxRt mathMax;
        MathClampRt mathClamp;
        MathRoundRt mathRound;
        MathLerpRt mathLerp;
        MathSignRt mathSign;
        MathAddRt mathAdd;
        MathSubRt mathSub;
        MathMultRt mathMult;
        MathDivRt mathDiv;
        MathModRt mathMod;
        MathCompareRt mathCompare;
        RouteTeeRt routeTee;
        RoutePassRt routePass;
        RouteMuxRt routeMux;
        RouteDemuxRt routeDemux;
        RouteSelectRt routeSelect;
        RouteMergeRt routeMerge;
        RouteUnpackRt routeUnpack;
        RoutePackRt routePack;
        MemEdgeRt memEdge;
        FlowRandomRt flowRandom;
        RouteSplitRt routeSplit;
        FlowRiseRt flowRise;
        FlowFallRt flowFall;
        FlowDbncRt flowDbnc;
        FlowOneshotRt flowOneshot;
        TimeDelayRt timeDelay;
        TimeClockRt timeClock;
        TimeWatchRt timeWatch;
        MemSampleRt memSample;
        MemChatterRt memChatter;
        FlowDoOnceRt flowDoOnce;
        FlowRelayRt flowRelay;
        IoButtonRt ioButton;
        IoKeypadRt ioKeypad;
        IoDisplayRt ioDisplay;
        IoAudioRt ioAudio;
        IoLoggerRt ioLogger;
        IoTriggerRt ioTrigger;
    };

    std::unordered_map<std::string, NodeSlot> nodes_;
    std::unordered_map<std::string, std::vector<std::string>> channelSubscribers_;

    void RebuildRoutes();
    void RebuildChannelIndex();
    void SeedNodes();
    void RunSpawnEvaluations();

    static std::string NormalizeName(std::string_view name);
    static std::string MakeRouteKey(std::string_view sourceId, std::string_view outputName);
    static std::uint32_t ClampDelayMs(std::uint32_t delayMs);

    void EmitOutput(const std::string& sourceId, const std::string& outputName, LogicContext ctx);
    void EmitOutputNormalized(const std::string& sourceId,
                              std::string outputNameNormalized,
                              LogicContext ctx,
                              std::uint32_t intrinsicDelayMs = 0);

    void HandleRelay(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleTimer(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleCounter(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleCompare(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleSequencer(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandlePulse(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleLatch(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleChannel(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMerge(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleSplit(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandlePredicate(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleInventoryGate(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleTriggerDetector(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleGateAnd(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleGateOr(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleGateNot(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleGateBuf(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleGateXnor(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleGateXor(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleGateNand(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleGateNor(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathAbs(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathMin(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathMax(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathClamp(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathRound(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRouteTee(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathLerp(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathSign(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathAdd(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathSub(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathMult(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathDiv(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathMod(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRoutePass(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRouteMux(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRouteDemux(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMathCompare(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRouteSelect(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRouteMerge(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRouteUnpack(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRoutePack(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMemEdge(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleFlowRandom(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleRouteSplit(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleFlowRise(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleFlowFall(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleFlowDbnc(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleFlowOneshot(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleTimeDelay(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleTimeClock(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleTimeWatch(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMemSample(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleMemChatter(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleFlowDoOnce(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleFlowRelay(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleIoButton(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleIoKeypad(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleIoDisplay(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleIoAudio(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleIoLogger(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);
    void HandleIoTrigger(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx);

    void TimerStartFresh(const std::string& selfId, NodeSlot& n);
    void TimerCancel(NodeSlot& n, bool fromDisable);
    void TimerFire(const std::string& selfId, NodeSlot& n);

    void ExpirePulseHolds();
    void ExpireFlowDebouncers();
    void ExpireFlowOneshots();
    void ExpireTimeDelays();
    void ExpireTimeClocks();
    void ExpireMemChatters();

    struct CompareObservation {
        std::optional<double> number;
        bool hasText = false;
        std::string text;
    };

    CompareObservation ReadCompareObservation(const CompareDef& def) const;
    static bool EvaluateComparePredicate(const CompareDef& def, const CompareObservation& obs);
    static void AttachComparePayload(LogicContext& ctx, const CompareObservation& obs, bool result);

    static void AttachCounterPayload(LogicContext& ctx, double value, double delta);

    bool PredicatePasses(const PredicateDef& def, const LogicContext& ctx) const;
    static bool InstigatorMatchesDetector(const TriggerDetectorDef& def, const LogicContext& ctx);
    static int ParseMergeInIndex(std::string_view in);

    void FlushAllPriorityLatches();
    void PulseEmitActiveWhileHeld();
    static bool ContextHasTag(const LogicContext& ctx, std::string_view tag);
};

} // namespace ri::logic
