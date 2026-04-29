#include "RawIron/Logic/LogicGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>

namespace ri::logic {

namespace {

constexpr char kRouteSep = '\x1f';
constexpr std::size_t kIoKeypadMaxDigits = 32;

[[nodiscard]] std::optional<double> TryKeypadNumericValue(const std::string& buffer) {
    if (buffer.empty()) {
        return std::nullopt;
    }
    if (buffer.size() > 18) {
        return std::nullopt;
    }
    for (char c : buffer) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
    }
    std::uint64_t v = 0;
    for (char c : buffer) {
        v = v * 10ULL + static_cast<unsigned>(c - '0');
    }
    return static_cast<double>(v);
}

void FillIoKeypadContext(LogicContext& out, const std::string& buffer) {
    out.fields["value"] = buffer;
    if (const std::optional<double> num = TryKeypadNumericValue(buffer)) {
        out.parameter = *num;
    } else {
        out.parameter.reset();
    }
}

[[nodiscard]] std::optional<std::string> ResolveContextStringField(const LogicContext& ctx,
                                                                   std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const auto it = ctx.fields.find(key);
        if (it != ctx.fields.end()) {
            return it->second;
        }
    }
    return std::nullopt;
}

[[nodiscard]] double ClampUnitAudio(double x) {
    if (!std::isfinite(x)) {
        return 0.0;
    }
    if (x < 0.0) {
        return 0.0;
    }
    if (x > 1.0) {
        return 1.0;
    }
    return x;
}

[[nodiscard]] double ParseVolumeInput(const LogicContext& ctx, double current) {
    if (ctx.parameter.has_value()) {
        return ClampUnitAudio(*ctx.parameter);
    }
    if (const std::optional<std::string> s = ResolveContextStringField(ctx, {"volume", "value"})) {
        try {
            return ClampUnitAudio(std::stod(*s));
        } catch (...) {
            return current;
        }
    }
    return current;
}

void FillIoAudioDonePayload(LogicContext& out, bool playing, double volume, const std::string& clip) {
    out.fields["playing"] = playing ? "true" : "false";
    out.fields["clip"] = clip;
    out.fields["volume"] = std::to_string(volume);
    out.parameter = volume;
}

[[nodiscard]] bool PulseInputHigh(const LogicContext& ctx) {
    const double level = ctx.analogSignal.has_value() ? *ctx.analogSignal
                         : ctx.parameter.has_value()   ? *ctx.parameter
                                                       : 0.0;
    return level > 0.5;
}

[[nodiscard]] double ScalarPulseValue(const LogicContext& ctx) {
    if (ctx.analogSignal.has_value()) {
        return *ctx.analogSignal;
    }
    if (ctx.parameter.has_value()) {
        return *ctx.parameter;
    }
    return 0.0;
}

[[nodiscard]] double SanitizeScalar(double x) {
    if (!std::isfinite(x)) {
        return 0.0;
    }
    return x;
}

[[nodiscard]] std::optional<double> TryFieldDouble(const LogicContext& ctx, const char* key) {
    const auto it = ctx.fields.find(key);
    if (it == ctx.fields.end()) {
        return std::nullopt;
    }
    try {
        return SanitizeScalar(std::stod(it->second));
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] double NextUnit01(std::uint64_t& s) {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return static_cast<double>(s & 0xFFFFFFFFu) * (1.0 / 4294967296.0);
}

void ResolveFlowRandomRange(const LogicContext& ctx, double& minOut, double& maxOut) {
    minOut = 0.0;
    maxOut = 1.0;
    if (ctx.analogSignal.has_value() && ctx.parameter.has_value()) {
        minOut = SanitizeScalar(*ctx.analogSignal);
        maxOut = SanitizeScalar(*ctx.parameter);
    } else if (ctx.analogSignal.has_value()) {
        minOut = TryFieldDouble(ctx, "min").value_or(0.0);
        maxOut = SanitizeScalar(*ctx.analogSignal);
    } else if (ctx.parameter.has_value()) {
        minOut = TryFieldDouble(ctx, "min").value_or(0.0);
        maxOut = SanitizeScalar(*ctx.parameter);
    } else {
        const std::optional<double> fm = TryFieldDouble(ctx, "min");
        const std::optional<double> fx = TryFieldDouble(ctx, "max");
        if (fm.has_value()) {
            minOut = *fm;
        }
        if (fx.has_value()) {
            maxOut = *fx;
        } else if (fm.has_value() && !fx.has_value()) {
            maxOut = 1.0;
        }
    }
    if (minOut > maxOut) {
        std::swap(minOut, maxOut);
    }
    if (!std::isfinite(minOut) || !std::isfinite(maxOut)) {
        minOut = 0.0;
        maxOut = 1.0;
    }
}

[[nodiscard]] double ClampUnitInterval(double t) {
    if (!std::isfinite(t)) {
        return 0.0;
    }
    return std::max(0.0, std::min(1.0, t));
}

[[nodiscard]] int RouteSelectIndexFromScalar(double s) {
    s = SanitizeScalar(s);
    int idx = static_cast<int>(std::floor(s + 1e-9));
    if (idx < 0) {
        idx = 0;
    }
    if (idx > 2) {
        idx = 2;
    }
    return idx;
}

[[nodiscard]] double ProbeScalarHint(const std::optional<double>& v) {
    if (!v.has_value() || !std::isfinite(*v)) {
        return 0.0;
    }
    const double a = std::fabs(*v);
    return std::max(0.0, std::min(1.0, a / (1.0 + a)));
}

[[nodiscard]] std::string ResolveLogMessage(const LogicContext& ctx) {
    if (const std::optional<std::string> s = ResolveContextStringField(ctx, {"message", "text", "value"})) {
        return *s;
    }
    if (ctx.parameter.has_value()) {
        return std::to_string(*ctx.parameter);
    }
    return {};
}

double ClampStep(double step) {
    if (!std::isfinite(step)) {
        return 1.0;
    }
    if (step < 1.0) {
        return 1.0;
    }
    if (step > 1'000'000.0) {
        return 1'000'000.0;
    }
    return step;
}

void FixMinMax(std::optional<double>& minV, std::optional<double>& maxV) {
    if (minV && maxV && *minV > *maxV) {
        std::swap(*minV, *maxV);
    }
}

bool NearlyEqual(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}

std::string_view TrimAscii(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.remove_suffix(1);
    }
    return s;
}

} // namespace

std::string LogicGraph::NormalizeName(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c >= 'A' && c <= 'Z') {
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string LogicGraph::MakeRouteKey(std::string_view sourceId, std::string_view outputName) {
    std::string key;
    key.reserve(sourceId.size() + 1 + outputName.size());
    key.append(sourceId);
    key.push_back(kRouteSep);
    key.append(NormalizeName(outputName));
    return key;
}

std::uint32_t LogicGraph::ClampDelayMs(std::uint32_t delayMs) {
    if (delayMs > kMaxLogicDelayMs) {
        return static_cast<std::uint32_t>(kMaxLogicDelayMs);
    }
    return delayMs;
}

int LogicGraph::ParseMergeInIndex(std::string_view in) {
    if (in.size() < 3 || in[0] != 'i' || in[1] != 'n') {
        return -1;
    }
    int v = 0;
    for (std::size_t i = 2; i < in.size(); ++i) {
        const char c = in[i];
        if (c < '0' || c > '9') {
            return -1;
        }
        v = v * 10 + (c - '0');
        if (v > 31) {
            return -1;
        }
    }
    return v;
}

LogicGraph::LogicGraph(LogicGraphSpec spec) : spec_(std::move(spec)) {
    RebuildRoutes();
    SeedNodes();
    RebuildChannelIndex();
    RunSpawnEvaluations();
}

void LogicGraph::RebuildRoutes() {
    routes_.clear();
    for (const LogicRoute& route : spec_.routes) {
        std::string key = MakeRouteKey(route.sourceId, route.outputName);
        auto& bucket = routes_[key];
        bucket.insert(bucket.end(), route.targets.begin(), route.targets.end());
    }
}

void LogicGraph::RebuildChannelIndex() {
    channelSubscribers_.clear();
    for (const auto& def : spec_.nodes) {
        std::visit(
            [this](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, ChannelNode>) {
                    if (node.def.role == ChannelRole::Subscribe) {
                        channelSubscribers_[node.def.channelName].push_back(node.id);
                    }
                }
            },
            def);
    }
}

void LogicGraph::SeedNodes() {
    nodes_.clear();
    for (const auto& def : spec_.nodes) {
        std::visit(
            [this](const auto& node) {
                using T = std::decay_t<decltype(node)>;
                NodeSlot slot{};

                if constexpr (std::is_same_v<T, RelayNode>) {
                    slot.kind = Kind::Relay;
                    slot.relayDef = node.def;
                    slot.relay.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, TimerNode>) {
                    slot.kind = Kind::Timer;
                    slot.timerDef = node.def;
                    slot.timer.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, CounterNode>) {
                    slot.kind = Kind::Counter;
                    slot.counterDef = node.def;
                    FixMinMax(slot.counterDef.minValue, slot.counterDef.maxValue);
                    slot.counterDef.step = ClampStep(node.def.step);
                    slot.counter.enabled = node.def.startEnabled;
                    slot.counter.value = node.def.startValue;
                } else if constexpr (std::is_same_v<T, CompareNode>) {
                    slot.kind = Kind::Compare;
                    slot.compareDef = node.def;
                    slot.compare.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, SequencerNode>) {
                    slot.kind = Kind::Sequencer;
                    slot.sequencerDef = node.def;
                    slot.sequencer.enabled = node.def.startEnabled;
                    slot.sequencer.index = 0;
                } else if constexpr (std::is_same_v<T, PulseNode>) {
                    slot.kind = Kind::Pulse;
                    slot.pulseDef = node.def;
                    slot.pulse.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, LatchNode>) {
                    slot.kind = Kind::Latch;
                    slot.latchDef = node.def;
                    slot.latch.enabled = true;
                    slot.latch.value = node.def.startValue;
                } else if constexpr (std::is_same_v<T, ChannelNode>) {
                    slot.kind = Kind::Channel;
                    slot.channelDef = node.def;
                    slot.channel.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, MergeNode>) {
                    slot.kind = Kind::Merge;
                    slot.mergeDef = node.def;
                    slot.merge.enabled = node.def.startEnabled;
                    slot.merge.armedBits = 0;
                } else if constexpr (std::is_same_v<T, SplitNode>) {
                    slot.kind = Kind::Split;
                    slot.splitDef = node.def;
                    slot.split.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, PredicateNode>) {
                    slot.kind = Kind::Predicate;
                    slot.predicateDef = node.def;
                    slot.predicate.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, InventoryGateNode>) {
                    slot.kind = Kind::InventoryGate;
                    slot.inventoryGateDef = node.def;
                    slot.inventoryGate.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, TriggerDetectorNode>) {
                    slot.kind = Kind::TriggerDetector;
                    slot.triggerDetectorDef = node.def;
                    slot.triggerDetector.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, GateAndNode>) {
                    slot.kind = Kind::GateAnd;
                    slot.gateAndDef = node.def;
                    slot.gateAnd.enabled = node.def.startEnabled;
                    slot.gateAnd.armedA = false;
                    slot.gateAnd.armedB = false;
                } else if constexpr (std::is_same_v<T, GateOrNode>) {
                    slot.kind = Kind::GateOr;
                    slot.gateOrDef = node.def;
                    slot.gateOr.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, GateNotNode>) {
                    slot.kind = Kind::GateNot;
                    slot.gateNotDef = node.def;
                    slot.gateNot.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, GateBufNode>) {
                    slot.kind = Kind::GateBuf;
                    slot.gateBufDef = node.def;
                    slot.gateBuf.enabled = node.def.startEnabled;
                    slot.gateBuf.lastAnalog.reset();
                } else if constexpr (std::is_same_v<T, GateXnorNode>) {
                    slot.kind = Kind::GateXnor;
                    slot.gateXnorDef = node.def;
                    slot.gateXnor.enabled = node.def.startEnabled;
                    slot.gateXnor.armedA = false;
                    slot.gateXnor.armedB = false;
                    slot.gateXnor.hiA = false;
                    slot.gateXnor.hiB = false;
                } else if constexpr (std::is_same_v<T, GateXorNode>) {
                    slot.kind = Kind::GateXor;
                    slot.gateXorDef = node.def;
                    slot.gateXor.enabled = node.def.startEnabled;
                    slot.gateXor.armedA = false;
                    slot.gateXor.armedB = false;
                    slot.gateXor.hiA = false;
                    slot.gateXor.hiB = false;
                } else if constexpr (std::is_same_v<T, GateNandNode>) {
                    slot.kind = Kind::GateNand;
                    slot.gateNandDef = node.def;
                    slot.gateNand.enabled = node.def.startEnabled;
                    slot.gateNand.armedA = false;
                    slot.gateNand.armedB = false;
                    slot.gateNand.hiA = false;
                    slot.gateNand.hiB = false;
                } else if constexpr (std::is_same_v<T, GateNorNode>) {
                    slot.kind = Kind::GateNor;
                    slot.gateNorDef = node.def;
                    slot.gateNor.enabled = node.def.startEnabled;
                    slot.gateNor.armedA = false;
                    slot.gateNor.armedB = false;
                    slot.gateNor.hiA = false;
                    slot.gateNor.hiB = false;
                } else if constexpr (std::is_same_v<T, MathAbsNode>) {
                    slot.kind = Kind::MathAbs;
                    slot.mathAbsDef = node.def;
                    slot.mathAbs.enabled = node.def.startEnabled;
                    slot.mathAbs.lastOut.reset();
                } else if constexpr (std::is_same_v<T, MathMinNode>) {
                    slot.kind = Kind::MathMin;
                    slot.mathMinDef = node.def;
                    slot.mathMin.enabled = node.def.startEnabled;
                    slot.mathMin.armedA = false;
                    slot.mathMin.armedB = false;
                    slot.mathMin.lastOut.reset();
                } else if constexpr (std::is_same_v<T, MathMaxNode>) {
                    slot.kind = Kind::MathMax;
                    slot.mathMaxDef = node.def;
                    slot.mathMax.enabled = node.def.startEnabled;
                    slot.mathMax.armedA = false;
                    slot.mathMax.armedB = false;
                    slot.mathMax.lastOut.reset();
                } else if constexpr (std::is_same_v<T, MathClampNode>) {
                    slot.kind = Kind::MathClamp;
                    slot.mathClampDef = node.def;
                    slot.mathClamp.enabled = node.def.startEnabled;
                    slot.mathClamp.armedVal = false;
                    slot.mathClamp.armedLo = false;
                    slot.mathClamp.armedHi = false;
                    slot.mathClamp.lastOut.reset();
                } else if constexpr (std::is_same_v<T, MathRoundNode>) {
                    slot.kind = Kind::MathRound;
                    slot.mathRoundDef = node.def;
                    slot.mathRound.enabled = node.def.startEnabled;
                    slot.mathRound.lastOut.reset();
                } else if constexpr (std::is_same_v<T, RouteTeeNode>) {
                    slot.kind = Kind::RouteTee;
                    slot.routeTeeDef = node.def;
                    slot.routeTee.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, MathLerpNode>) {
                    slot.kind = Kind::MathLerp;
                    slot.mathLerpDef = node.def;
                    slot.mathLerp.enabled = node.def.startEnabled;
                    slot.mathLerp.armedA = false;
                    slot.mathLerp.armedB = false;
                    slot.mathLerp.armedT = false;
                    slot.mathLerp.lastOut.reset();
                } else if constexpr (std::is_same_v<T, MathSignNode>) {
                    slot.kind = Kind::MathSign;
                    slot.mathSignDef = node.def;
                    slot.mathSign.enabled = node.def.startEnabled;
                    slot.mathSign.lastSign.reset();
                    slot.mathSign.lastZeroPulse = false;
                } else if constexpr (std::is_same_v<T, RoutePassNode>) {
                    slot.kind = Kind::RoutePass;
                    slot.routePassDef = node.def;
                    slot.routePass.enabled = node.def.startEnabled;
                    slot.routePass.passOpen = true;
                } else if constexpr (std::is_same_v<T, RouteMuxNode>) {
                    slot.kind = Kind::RouteMux;
                    slot.routeMuxDef = node.def;
                    slot.routeMux.enabled = node.def.startEnabled;
                    slot.routeMux.selHigh = false;
                } else if constexpr (std::is_same_v<T, RouteDemuxNode>) {
                    slot.kind = Kind::RouteDemux;
                    slot.routeDemuxDef = node.def;
                    slot.routeDemux.enabled = node.def.startEnabled;
                    slot.routeDemux.selHigh = false;
                } else if constexpr (std::is_same_v<T, MathAddNode>) {
                    slot.kind = Kind::MathAdd;
                    slot.mathAddDef = node.def;
                    slot.mathAdd.enabled = node.def.startEnabled;
                    slot.mathAdd.armedA = false;
                    slot.mathAdd.armedB = false;
                    slot.mathAdd.lastSum.reset();
                    slot.mathAdd.lastCarry.reset();
                } else if constexpr (std::is_same_v<T, MathSubNode>) {
                    slot.kind = Kind::MathSub;
                    slot.mathSubDef = node.def;
                    slot.mathSub.enabled = node.def.startEnabled;
                    slot.mathSub.armedA = false;
                    slot.mathSub.armedB = false;
                    slot.mathSub.lastDiff.reset();
                    slot.mathSub.lastBorrow.reset();
                } else if constexpr (std::is_same_v<T, MathMultNode>) {
                    slot.kind = Kind::MathMult;
                    slot.mathMultDef = node.def;
                    slot.mathMult.enabled = node.def.startEnabled;
                    slot.mathMult.armedA = false;
                    slot.mathMult.armedB = false;
                    slot.mathMult.lastOut.reset();
                } else if constexpr (std::is_same_v<T, MathDivNode>) {
                    slot.kind = Kind::MathDiv;
                    slot.mathDivDef = node.def;
                    slot.mathDiv.enabled = node.def.startEnabled;
                    slot.mathDiv.armedA = false;
                    slot.mathDiv.armedB = false;
                    slot.mathDiv.lastQuot.reset();
                    slot.mathDiv.lastRem.reset();
                } else if constexpr (std::is_same_v<T, MathModNode>) {
                    slot.kind = Kind::MathMod;
                    slot.mathModDef = node.def;
                    slot.mathMod.enabled = node.def.startEnabled;
                    slot.mathMod.armedVal = false;
                    slot.mathMod.armedMod = false;
                    slot.mathMod.lastOut.reset();
                } else if constexpr (std::is_same_v<T, MathCompareNode>) {
                    slot.kind = Kind::MathCompare;
                    slot.mathCompareDef = node.def;
                    slot.mathCompare.enabled = node.def.startEnabled;
                    slot.mathCompare.armedA = false;
                    slot.mathCompare.armedB = false;
                    slot.mathCompare.lastOrder.reset();
                } else if constexpr (std::is_same_v<T, RouteSelectNode>) {
                    slot.kind = Kind::RouteSelect;
                    slot.routeSelectDef = node.def;
                    slot.routeSelect.enabled = node.def.startEnabled;
                    slot.routeSelect.selIdx = 0;
                    slot.routeSelect.lastSel = 0;
                } else if constexpr (std::is_same_v<T, RouteMergeNode>) {
                    slot.kind = Kind::RouteMerge;
                    slot.routeMergeDef = node.def;
                    slot.routeMerge.enabled = node.def.startEnabled;
                    slot.routeMerge.armed1 = false;
                    slot.routeMerge.armed2 = false;
                    slot.routeMerge.armed3 = false;
                } else if constexpr (std::is_same_v<T, RouteUnpackNode>) {
                    slot.kind = Kind::RouteUnpack;
                    slot.routeUnpackDef = node.def;
                    slot.routeUnpack.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, RoutePackNode>) {
                    slot.kind = Kind::RoutePack;
                    slot.routePackDef = node.def;
                    slot.routePack.enabled = node.def.startEnabled;
                    slot.routePack.armed0 = false;
                    slot.routePack.armed1 = false;
                    slot.routePack.armed2 = false;
                    slot.routePack.armed3 = false;
                } else if constexpr (std::is_same_v<T, MemEdgeNode>) {
                    slot.kind = Kind::MemEdge;
                    slot.memEdgeDef = node.def;
                    slot.memEdge.enabled = node.def.startEnabled;
                    slot.memEdge.hasLast = false;
                } else if constexpr (std::is_same_v<T, FlowRandomNode>) {
                    slot.kind = Kind::FlowRandom;
                    slot.flowRandomDef = node.def;
                    slot.flowRandom.enabled = node.def.startEnabled;
                    slot.flowRandom.lastVal.reset();
                    std::uint64_t h = 14695981039346656037ULL;
                    for (const char c : node.id) {
                        h ^= static_cast<std::uint8_t>(c);
                        h *= 1099511628211ULL;
                    }
                    slot.flowRandom.lcgState ^= h;
                } else if constexpr (std::is_same_v<T, RouteSplitNode>) {
                    slot.kind = Kind::RouteSplit;
                    slot.routeSplitDef = node.def;
                    slot.routeSplit.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, FlowRiseNode>) {
                    slot.kind = Kind::FlowRise;
                    slot.flowRiseDef = node.def;
                    slot.flowRise.enabled = node.def.startEnabled;
                    slot.flowRise.armed = false;
                    slot.flowRise.hasLastIn = false;
                    slot.flowRise.lastInHi = false;
                    slot.flowRise.reseedNextIn = false;
                } else if constexpr (std::is_same_v<T, FlowFallNode>) {
                    slot.kind = Kind::FlowFall;
                    slot.flowFallDef = node.def;
                    slot.flowFall.enabled = node.def.startEnabled;
                    slot.flowFall.armed = false;
                    slot.flowFall.hasLastIn = false;
                    slot.flowFall.lastInHi = false;
                    slot.flowFall.reseedNextIn = false;
                } else if constexpr (std::is_same_v<T, FlowDbncNode>) {
                    slot.kind = Kind::FlowDbnc;
                    slot.flowDbncDef = node.def;
                    slot.flowDbnc.enabled = node.def.startEnabled;
                    slot.flowDbnc.rawHi = false;
                    slot.flowDbnc.pendingHi = false;
                    slot.flowDbnc.stableHi = false;
                    slot.flowDbnc.settleAt = 0;
                    slot.flowDbnc.debounceMs = static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(static_cast<std::uint64_t>(node.def.defaultDebounceMs), kMaxLogicDelayMs));
                } else if constexpr (std::is_same_v<T, FlowOneshotNode>) {
                    slot.kind = Kind::FlowOneshot;
                    slot.flowOneshotDef = node.def;
                    slot.flowOneshot.enabled = node.def.startEnabled;
                    slot.flowOneshot.busy = false;
                    slot.flowOneshot.endAt = 0;
                    slot.flowOneshot.pulseMs =
                        std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::min<std::uint64_t>(
                                                      node.def.defaultPulseMs == 0 ? 1u : node.def.defaultPulseMs,
                                                      kMaxLogicDelayMs)));
                } else if constexpr (std::is_same_v<T, TimeDelayNode>) {
                    slot.kind = Kind::TimeDelay;
                    slot.timeDelayDef = node.def;
                    slot.timeDelay.enabled = node.def.startEnabled;
                    slot.timeDelay.delayMs = static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(static_cast<std::uint64_t>(node.def.defaultDelayMs), kMaxLogicDelayMs));
                    slot.timeDelay.hasPending = false;
                    slot.timeDelay.fireAt = 0;
                } else if constexpr (std::is_same_v<T, TimeClockNode>) {
                    slot.kind = Kind::TimeClock;
                    slot.timeClockDef = node.def;
                    slot.timeClock.enabled = node.def.startEnabled;
                    slot.timeClock.clockEnabled = false;
                    slot.timeClock.hz = std::max(0.001, node.def.defaultHz);
                    const double period = 1000.0 / slot.timeClock.hz;
                    slot.timeClock.periodMs =
                        std::max<std::uint64_t>(1, static_cast<std::uint64_t>(period + 0.5));
                    slot.timeClock.nextTickAt = 0;
                } else if constexpr (std::is_same_v<T, TimeWatchNode>) {
                    slot.kind = Kind::TimeWatch;
                    slot.timeWatchDef = node.def;
                    slot.timeWatch.enabled = node.def.startEnabled;
                    slot.timeWatch.running = false;
                    slot.timeWatch.lapStartMs = 0;
                    slot.timeWatch.accumulatedMs = 0;
                } else if constexpr (std::is_same_v<T, MemSampleNode>) {
                    slot.kind = Kind::MemSample;
                    slot.memSampleDef = node.def;
                    slot.memSample.enabled = node.def.startEnabled;
                    slot.memSample.holding = false;
                    slot.memSample.live = 0.0;
                    slot.memSample.latched = 0.0;
                    slot.memSample.lastEmitted.reset();
                } else if constexpr (std::is_same_v<T, MemChatterNode>) {
                    slot.kind = Kind::MemChatter;
                    slot.memChatterDef = node.def;
                    slot.memChatter.enabled = node.def.startEnabled;
                    slot.memChatter.rawHi = false;
                    slot.memChatter.pendingHi = false;
                    slot.memChatter.stableHi = false;
                    slot.memChatter.settleAt = 0;
                    slot.memChatter.debounceMs = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                        static_cast<std::uint64_t>(node.def.defaultDebounceMs), kMaxLogicDelayMs));
                } else if constexpr (std::is_same_v<T, FlowDoOnceNode>) {
                    slot.kind = Kind::FlowDoOnce;
                    slot.flowDoOnceDef = node.def;
                    slot.flowDoOnce.enabled = node.def.startEnabled;
                    slot.flowDoOnce.consumed = false;
                } else if constexpr (std::is_same_v<T, FlowRelayNode>) {
                    slot.kind = Kind::FlowRelay;
                    slot.flowRelayDef = node.def;
                    slot.flowRelay.enabled = node.def.startEnabled;
                } else if constexpr (std::is_same_v<T, IoButtonNode>) {
                    slot.kind = Kind::IoButton;
                    slot.ioButtonDef = node.def;
                    slot.ioButton.enabled = node.def.startEnabled;
                    slot.ioButton.held = false;
                } else if constexpr (std::is_same_v<T, IoKeypadNode>) {
                    slot.kind = Kind::IoKeypad;
                    slot.ioKeypadDef = node.def;
                    slot.ioKeypad.enabled = node.def.startEnabled;
                    slot.ioKeypad.buffer.clear();
                } else if constexpr (std::is_same_v<T, IoDisplayNode>) {
                    slot.kind = Kind::IoDisplay;
                    slot.ioDisplayDef = node.def;
                    slot.ioDisplay.enabled = node.def.startEnabled;
                    slot.ioDisplay.text.clear();
                    slot.ioDisplay.color.clear();
                } else if constexpr (std::is_same_v<T, IoAudioNode>) {
                    slot.kind = Kind::IoAudio;
                    slot.ioAudioDef = node.def;
                    slot.ioAudio.enabled = node.def.startEnabled;
                    slot.ioAudio.playing = false;
                    slot.ioAudio.volume = 1.0;
                    slot.ioAudio.lastClip.clear();
                } else if constexpr (std::is_same_v<T, IoLoggerNode>) {
                    slot.kind = Kind::IoLogger;
                    slot.ioLoggerDef = node.def;
                    slot.ioLogger.enabled = node.def.startEnabled;
                    slot.ioLogger.lastLevel.clear();
                    slot.ioLogger.lastMessage.clear();
                    slot.ioLogger.lineCount = 0;
                } else if constexpr (std::is_same_v<T, IoTriggerNode>) {
                    slot.kind = Kind::IoTrigger;
                    slot.ioTriggerDef = node.def;
                    slot.ioTrigger.armed = node.def.startArmed;
                    slot.ioTrigger.held = false;
                }

                auto [it, _] = nodes_.emplace(node.id, std::move(slot));
                if constexpr (std::is_same_v<T, TimerNode>) {
                    if (it->second.timer.enabled && node.def.autoStart) {
                        TimerStartFresh(node.id, it->second);
                    }
                }
            },
            def);
    }
}

void LogicGraph::Reset() {
    while (!pending_.empty()) {
        pending_.pop();
    }
    nowMs_ = 0;
    nextTieBreak_ = 0;
    dispatchDepth_ = 0;
    RebuildRoutes();
    SeedNodes();
    RebuildChannelIndex();
    RunSpawnEvaluations();
}

void LogicGraph::RunSpawnEvaluations() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind == Kind::Compare && slot.compareDef.evaluateOnSpawn && slot.compare.enabled) {
            LogicContext ctx;
            ctx.sourceId = id;
            HandleCompare(id, slot, "evaluate", ctx);
        }
    }
}

std::optional<double> LogicGraph::TryGetCounterValue(std::string_view nodeId) const {
    const std::string key(nodeId);
    auto it = nodes_.find(key);
    if (it == nodes_.end() || it->second.kind != Kind::Counter) {
        return std::nullopt;
    }
    return it->second.counter.value;
}

std::optional<bool> LogicGraph::TryGetLatchValue(std::string_view nodeId) const {
    const std::string key(nodeId);
    auto it = nodes_.find(key);
    if (it == nodes_.end() || it->second.kind != Kind::Latch) {
        return std::nullopt;
    }
    return it->second.latch.value;
}

std::optional<double> LogicGraph::TryGetLogicNumericProperty(std::string_view nodeId,
                                                             std::string_view propertyName) const {
    const std::string key(nodeId);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        return std::nullopt;
    }
    const std::string prop = NormalizeName(propertyName);
    if (it->second.kind == Kind::Counter) {
        if (prop == "value") {
            return it->second.counter.value;
        }
        return std::nullopt;
    }
    if (it->second.kind == Kind::Latch) {
        if (prop == "value" || prop == "latchedvalue") {
            return it->second.latch.value ? 1.0 : 0.0;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

void LogicGraph::ExpirePulseHolds() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind != Kind::Pulse || !slot.pulse.held) {
            continue;
        }
        if (slot.pulse.releaseAt > nowMs_) {
            continue;
        }
        slot.pulse.held = false;
        LogicContext ctx;
        ctx.sourceId = id;
        EmitOutput(id, "OnFall", ctx);
    }
}

void LogicGraph::ExpireFlowDebouncers() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind != Kind::FlowDbnc || !slot.flowDbnc.enabled) {
            continue;
        }
        FlowDbncRt& d = slot.flowDbnc;
        for (int iter = 0; iter < 64; ++iter) {
            if (d.settleAt == 0 || d.settleAt > nowMs_) {
                break;
            }
            if (d.rawHi == d.pendingHi) {
                if (d.stableHi != d.pendingHi) {
                    d.stableHi = d.pendingHi;
                    LogicContext out{};
                    out.sourceId = id;
                    out.analogSignal = d.stableHi ? 1.0 : 0.0;
                    out.parameter = out.analogSignal;
                    EmitOutput(id, "Out", out);
                }
                d.settleAt = 0;
                break;
            }
            d.pendingHi = d.rawHi;
            const std::uint32_t step = d.debounceMs == 0 ? 1u : std::max<std::uint32_t>(1u, d.debounceMs);
            const std::uint64_t next = nowMs_ + static_cast<std::uint64_t>(step);
            d.settleAt = next < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : next;
            break;
        }
    }
}

void LogicGraph::ExpireFlowOneshots() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind != Kind::FlowOneshot || !slot.flowOneshot.enabled) {
            continue;
        }
        FlowOneshotRt& o = slot.flowOneshot;
        if (!o.busy || o.endAt > nowMs_) {
            continue;
        }
        o.busy = false;
        o.endAt = 0;
        LogicContext lo{};
        lo.sourceId = id;
        lo.analogSignal = 0.0;
        lo.parameter = 0.0;
        EmitOutput(id, "Out", lo);
        EmitOutput(id, "Busy", lo);
    }
}

void LogicGraph::ExpireTimeDelays() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind != Kind::TimeDelay || !slot.timeDelay.enabled) {
            continue;
        }
        TimeDelayRt& t = slot.timeDelay;
        if (!t.hasPending || t.fireAt == 0 || t.fireAt > nowMs_) {
            continue;
        }
        LogicContext out = t.pendingCtx;
        out.sourceId = id;
        t.hasPending = false;
        t.fireAt = 0;
        EmitOutput(id, "Out", std::move(out));
    }
}

void LogicGraph::ExpireTimeClocks() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind != Kind::TimeClock || !slot.timeClock.enabled || !slot.timeClock.clockEnabled) {
            continue;
        }
        TimeClockRt& c = slot.timeClock;
        for (int nTicks = 0; nTicks < 64 && c.nextTickAt != 0 && c.nextTickAt <= nowMs_; ++nTicks) {
            LogicContext tick{};
            tick.sourceId = id;
            tick.analogSignal = 1.0;
            tick.parameter = 1.0;
            EmitOutput(id, "Tick", tick);
            const std::uint64_t next = c.nextTickAt + c.periodMs;
            c.nextTickAt = next < c.nextTickAt ? std::numeric_limits<std::uint64_t>::max() : next;
        }
    }
}

void LogicGraph::ExpireMemChatters() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind != Kind::MemChatter || !slot.memChatter.enabled) {
            continue;
        }
        MemChatterRt& d = slot.memChatter;
        for (int iter = 0; iter < 64; ++iter) {
            if (d.settleAt == 0 || d.settleAt > nowMs_) {
                break;
            }
            if (d.rawHi == d.pendingHi) {
                if (d.stableHi != d.pendingHi) {
                    d.stableHi = d.pendingHi;
                    LogicContext out{};
                    out.sourceId = id;
                    out.analogSignal = d.stableHi ? 1.0 : 0.0;
                    out.parameter = out.analogSignal;
                    EmitOutput(id, "Stable", out);
                }
                d.settleAt = 0;
                break;
            }
            d.pendingHi = d.rawHi;
            const std::uint32_t step = d.debounceMs == 0 ? 1u : std::max<std::uint32_t>(1u, d.debounceMs);
            const std::uint64_t next = nowMs_ + static_cast<std::uint64_t>(step);
            d.settleAt = next < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : next;
            break;
        }
    }
}

void LogicGraph::PulseEmitActiveWhileHeld() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind != Kind::Pulse || !slot.pulse.held) {
            continue;
        }
        if (slot.pulse.releaseAt <= nowMs_) {
            continue;
        }
        LogicContext ctx;
        ctx.sourceId = id;
        EmitOutput(id, "OnActive", ctx);
    }
}

void LogicGraph::AdvanceTime(std::uint64_t deltaMs) {
    constexpr std::uint64_t kSpinLimit = 4096;
    std::uint64_t spins = 0;
    nowMs_ += deltaMs;
    PulseEmitActiveWhileHeld();
    ExpirePulseHolds();
    ExpireFlowDebouncers();
    ExpireFlowOneshots();
    ExpireTimeDelays();
    ExpireTimeClocks();
    ExpireMemChatters();

    while (spins++ < kSpinLimit) {
        while (!pending_.empty() && pending_.top().fireAt <= nowMs_) {
            PendingDelivery job = pending_.top();
            pending_.pop();
            DispatchInput(job.targetId, job.inputName, job.context);
        }

        bool firedTimer = false;
        for (auto& [id, slot] : nodes_) {
            if (slot.kind != Kind::Timer) {
                continue;
            }
            if (!slot.timer.enabled || !slot.timer.active) {
                continue;
            }
            if (slot.timer.fireAt > nowMs_) {
                continue;
            }
            if (slot.timer.scheduledWithStartCount != slot.timer.startCount) {
                continue;
            }
            TimerFire(id, slot);
            firedTimer = true;
        }

        if (!firedTimer) {
            break;
        }
    }
}

void LogicGraph::EmitWorldOutput(std::string_view sourceId, std::string_view outputName, LogicContext context) {
    EmitOutput(std::string(sourceId), std::string(outputName), std::move(context));
}

std::vector<LogicCircuitNodeProbe> LogicGraph::ProbeCircuitNodes() const {
    std::vector<LogicCircuitNodeProbe> out;
    out.reserve(nodes_.size());
    for (const auto& [id, n] : nodes_) {
        LogicCircuitNodeProbe p;
        p.id = id;
        switch (n.kind) {
        case Kind::Relay:
            p.kind = "relay";
            p.powered = n.relay.enabled;
            p.signalStrength = n.relay.enabled ? 1.0 : 0.0;
            p.detail = n.relay.enabled ? "conducts" : "open";
            break;
        case Kind::Timer:
            p.kind = "timer";
            p.powered = n.timer.active;
            p.signalStrength = n.timer.active ? 1.0 : 0.0;
            p.detail = n.timer.active ? "ticking" : (n.timer.enabled ? "idle" : "disabled");
            break;
        case Kind::Counter:
            p.kind = "counter";
            p.powered = n.counter.enabled && std::fabs(n.counter.value) > 1e-9;
            if (n.counterDef.maxValue && std::fabs(*n.counterDef.maxValue) > 1e-9) {
                const double t = n.counter.value / *n.counterDef.maxValue;
                p.signalStrength = std::max(0.0, std::min(1.0, t));
            } else {
                p.signalStrength = p.powered ? 1.0 : 0.0;
            }
            p.detail = "value=" + std::to_string(n.counter.value);
            break;
        case Kind::Compare:
            p.kind = "compare";
            if (n.compare.lastResult) {
                p.powered = *n.compare.lastResult;
                p.signalStrength = *n.compare.lastResult ? 1.0 : 0.0;
                p.detail = *n.compare.lastResult ? "true" : "false";
            } else {
                p.detail = "unset";
            }
            break;
        case Kind::Sequencer: {
            p.kind = "sequencer";
            p.powered = n.sequencer.enabled;
            const int maxI = std::max(1, n.sequencerDef.stepCount);
            const int span = std::max(1, maxI - 1);
            p.signalStrength = static_cast<double>(n.sequencer.index % maxI) / static_cast<double>(span);
            std::ostringstream os;
            os << "step " << n.sequencer.index << "/" << n.sequencerDef.stepCount;
            p.detail = os.str();
            break;
        }
        case Kind::Pulse:
            p.kind = "pulse";
            p.powered = n.pulse.held;
            p.signalStrength = n.pulse.held ? 1.0 : 0.0;
            p.detail = n.pulse.held ? "held" : "idle";
            break;
        case Kind::Latch:
            p.kind = "latch";
            p.powered = n.latch.value;
            p.signalStrength = n.latch.value ? 1.0 : 0.0;
            p.detail = n.latch.value ? "set" : "cleared";
            break;
        case Kind::Channel:
            p.kind = "channel";
            p.powered = n.channel.enabled;
            p.signalStrength = n.channel.enabled ? 1.0 : 0.0;
            p.detail = n.channelDef.channelName;
            break;
        case Kind::Merge:
            p.kind = "merge";
            p.powered = n.merge.enabled && n.merge.armedBits != 0;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.mergeDef.mode == MergeMode::All ? "AND arms" : "OR arms";
            break;
        case Kind::Split:
            p.kind = "split";
            p.powered = n.split.enabled;
            p.signalStrength = n.split.enabled ? 1.0 : 0.0;
            p.detail = "fan-out";
            break;
        case Kind::Predicate:
            p.kind = "predicate";
            p.powered = n.predicate.enabled;
            p.signalStrength = n.predicate.enabled ? 1.0 : 0.0;
            p.detail = "filter";
            break;
        case Kind::InventoryGate:
            p.kind = "inventory_gate";
            if (n.inventoryGate.lastResult) {
                p.powered = *n.inventoryGate.lastResult;
                p.signalStrength = *n.inventoryGate.lastResult ? 1.0 : 0.0;
                p.detail = *n.inventoryGate.lastResult ? "pass" : "fail";
            } else {
                p.detail = "unset";
            }
            break;
        case Kind::TriggerDetector:
            p.kind = "trigger_detector";
            p.powered = n.triggerDetector.enabled;
            p.signalStrength = n.triggerDetector.enabled ? 1.0 : 0.0;
            p.detail = "detector";
            break;
        case Kind::GateAnd:
            p.kind = "gate_and";
            p.powered = n.gateAnd.enabled && n.gateAnd.armedA && n.gateAnd.armedB;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = (n.gateAnd.armedA && n.gateAnd.armedB) ? "armed" : "waiting";
            break;
        case Kind::GateOr:
            p.kind = "gate_or";
            p.powered = n.gateOr.enabled;
            p.signalStrength = n.gateOr.enabled ? 1.0 : 0.0;
            p.detail = "or";
            break;
        case Kind::GateNot:
            p.kind = "gate_not";
            p.powered = n.gateNot.enabled;
            p.signalStrength = n.gateNot.enabled ? 1.0 : 0.0;
            p.detail = "invert";
            break;
        case Kind::GateBuf:
            p.kind = "gate_buf";
            p.powered = n.gateBuf.enabled && n.gateBuf.lastAnalog.has_value() && *n.gateBuf.lastAnalog > 0.5;
            p.signalStrength = n.gateBuf.lastAnalog.has_value()
                ? std::max(0.0, std::min(1.0, *n.gateBuf.lastAnalog))
                : 0.0;
            p.detail = "buffer";
            break;
        case Kind::GateXnor:
            p.kind = "gate_xnor";
            p.powered = n.gateXnor.enabled && n.gateXnor.armedA && n.gateXnor.armedB && (n.gateXnor.hiA == n.gateXnor.hiB);
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = (n.gateXnor.armedA && n.gateXnor.armedB) ? "armed" : "waiting";
            break;
        case Kind::GateXor:
            p.kind = "gate_xor";
            p.powered = n.gateXor.enabled && n.gateXor.armedA && n.gateXor.armedB && (n.gateXor.hiA != n.gateXor.hiB);
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = (n.gateXor.armedA && n.gateXor.armedB) ? "armed" : "waiting";
            break;
        case Kind::GateNand:
            p.kind = "gate_nand";
            p.powered = n.gateNand.enabled && n.gateNand.armedA && n.gateNand.armedB && !(n.gateNand.hiA && n.gateNand.hiB);
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = (n.gateNand.armedA && n.gateNand.armedB) ? "armed" : "waiting";
            break;
        case Kind::GateNor:
            p.kind = "gate_nor";
            p.powered = n.gateNor.enabled && n.gateNor.armedA && n.gateNor.armedB && !(n.gateNor.hiA || n.gateNor.hiB);
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = (n.gateNor.armedA && n.gateNor.armedB) ? "armed" : "waiting";
            break;
        case Kind::MathAbs:
            p.kind = "math_abs";
            p.powered = n.mathAbs.enabled && n.mathAbs.lastOut.has_value();
            p.signalStrength = ProbeScalarHint(n.mathAbs.lastOut);
            p.detail = p.powered ? "out" : "idle";
            break;
        case Kind::MathMin:
            p.kind = "math_min";
            p.powered = n.mathMin.enabled && n.mathMin.lastOut.has_value();
            p.signalStrength = ProbeScalarHint(n.mathMin.lastOut);
            p.detail = (n.mathMin.armedA && n.mathMin.armedB) ? "armed" : "waiting";
            break;
        case Kind::MathMax:
            p.kind = "math_max";
            p.powered = n.mathMax.enabled && n.mathMax.lastOut.has_value();
            p.signalStrength = ProbeScalarHint(n.mathMax.lastOut);
            p.detail = (n.mathMax.armedA && n.mathMax.armedB) ? "armed" : "waiting";
            break;
        case Kind::MathClamp:
            p.kind = "math_clamp";
            p.powered = n.mathClamp.enabled && n.mathClamp.lastOut.has_value();
            p.signalStrength = ProbeScalarHint(n.mathClamp.lastOut);
            p.detail = (n.mathClamp.armedVal && n.mathClamp.armedLo && n.mathClamp.armedHi) ? "armed" : "waiting";
            break;
        case Kind::MathRound:
            p.kind = "math_round";
            p.powered = n.mathRound.enabled && n.mathRound.lastOut.has_value();
            p.signalStrength = ProbeScalarHint(n.mathRound.lastOut);
            p.detail = p.powered ? "out" : "idle";
            break;
        case Kind::RouteTee:
            p.kind = "route_tee";
            p.powered = n.routeTee.enabled;
            p.signalStrength = n.routeTee.enabled ? 1.0 : 0.0;
            p.detail = "tee";
            break;
        case Kind::MathLerp:
            p.kind = "math_lerp";
            p.powered = n.mathLerp.enabled && n.mathLerp.lastOut.has_value();
            p.signalStrength = ProbeScalarHint(n.mathLerp.lastOut);
            p.detail = (n.mathLerp.armedA && n.mathLerp.armedB && n.mathLerp.armedT) ? "armed" : "waiting";
            break;
        case Kind::MathSign:
            p.kind = "math_sign";
            p.powered = n.mathSign.enabled && n.mathSign.lastSign.has_value();
            p.signalStrength = ProbeScalarHint(n.mathSign.lastSign);
            p.detail = n.mathSign.lastZeroPulse ? "zero" : "sign";
            break;
        case Kind::MathAdd:
            p.kind = "math_add";
            p.powered = n.mathAdd.enabled && n.mathAdd.lastSum.has_value();
            p.signalStrength = ProbeScalarHint(n.mathAdd.lastSum);
            p.detail = (n.mathAdd.armedA && n.mathAdd.armedB) ? "armed" : "waiting";
            break;
        case Kind::MathSub:
            p.kind = "math_sub";
            p.powered = n.mathSub.enabled && n.mathSub.lastDiff.has_value();
            p.signalStrength = ProbeScalarHint(n.mathSub.lastDiff);
            p.detail = (n.mathSub.armedA && n.mathSub.armedB) ? "armed" : "waiting";
            break;
        case Kind::MathMult:
            p.kind = "math_mult";
            p.powered = n.mathMult.enabled && n.mathMult.lastOut.has_value();
            p.signalStrength = ProbeScalarHint(n.mathMult.lastOut);
            p.detail = (n.mathMult.armedA && n.mathMult.armedB) ? "armed" : "waiting";
            break;
        case Kind::MathDiv:
            p.kind = "math_div";
            p.powered = n.mathDiv.enabled && n.mathDiv.lastQuot.has_value();
            p.signalStrength = ProbeScalarHint(n.mathDiv.lastQuot);
            p.detail = (n.mathDiv.armedA && n.mathDiv.armedB) ? "armed" : "waiting";
            break;
        case Kind::MathMod:
            p.kind = "math_mod";
            p.powered = n.mathMod.enabled && n.mathMod.lastOut.has_value();
            p.signalStrength = ProbeScalarHint(n.mathMod.lastOut);
            p.detail = (n.mathMod.armedVal && n.mathMod.armedMod) ? "armed" : "waiting";
            break;
        case Kind::MathCompare:
            p.kind = "math_compare";
            p.powered = n.mathCompare.enabled && n.mathCompare.lastOrder.has_value();
            p.signalStrength = p.powered ? 1.0 : 0.0;
            if (!n.mathCompare.lastOrder.has_value()) {
                p.detail = "waiting";
            } else if (*n.mathCompare.lastOrder == 1) {
                p.detail = "a>b";
            } else if (*n.mathCompare.lastOrder == 0) {
                p.detail = "a==b";
            } else {
                p.detail = "a<b";
            }
            break;
        case Kind::RoutePass:
            p.kind = "route_pass";
            p.powered = n.routePass.enabled && n.routePass.passOpen;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.routePass.passOpen ? "open" : "closed";
            break;
        case Kind::RouteMux:
            p.kind = "route_mux";
            p.powered = n.routeMux.enabled;
            p.signalStrength = n.routeMux.enabled ? 1.0 : 0.0;
            p.detail = n.routeMux.selHigh ? "sel=b" : "sel=a";
            break;
        case Kind::RouteDemux:
            p.kind = "route_demux";
            p.powered = n.routeDemux.enabled;
            p.signalStrength = n.routeDemux.enabled ? 1.0 : 0.0;
            p.detail = n.routeDemux.selHigh ? "to=b" : "to=a";
            break;
        case Kind::RouteSelect:
            p.kind = "route_select";
            p.powered = n.routeSelect.enabled;
            p.signalStrength = n.routeSelect.enabled ? 1.0 : 0.0;
            p.detail = "sel=" + std::to_string(n.routeSelect.selIdx);
            break;
        case Kind::RouteMerge:
            p.kind = "route_merge";
            p.powered = n.routeMerge.enabled;
            p.signalStrength = (n.routeMerge.armed1 && n.routeMerge.armed2 && n.routeMerge.armed3) ? 1.0 : 0.0;
            p.detail = (n.routeMerge.armed1 && n.routeMerge.armed2 && n.routeMerge.armed3) ? "armed" : "waiting";
            break;
        case Kind::RouteUnpack:
            p.kind = "route_unpack";
            p.powered = n.routeUnpack.enabled;
            p.signalStrength = n.routeUnpack.enabled ? 1.0 : 0.0;
            p.detail = "fan4";
            break;
        case Kind::RoutePack:
            p.kind = "route_pack";
            p.powered = n.routePack.enabled
                         && (n.routePack.armed0 || n.routePack.armed1 || n.routePack.armed2 || n.routePack.armed3);
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = (n.routePack.armed0 && n.routePack.armed1 && n.routePack.armed2 && n.routePack.armed3) ? "ready"
                                                                                                              : "wait";
            break;
        case Kind::MemEdge:
            p.kind = "mem_edge";
            p.powered = n.memEdge.enabled && n.memEdge.hasLast;
            p.signalStrength = p.powered && n.memEdge.lastHi ? 1.0 : 0.0;
            p.detail = n.memEdge.hasLast ? (n.memEdge.lastHi ? "hi" : "lo") : "unset";
            break;
        case Kind::FlowRandom:
            p.kind = "flow_random";
            p.powered = n.flowRandom.enabled && n.flowRandom.lastVal.has_value();
            p.signalStrength = ProbeScalarHint(n.flowRandom.lastVal);
            p.detail = n.flowRandom.lastVal.has_value() ? std::to_string(*n.flowRandom.lastVal) : "idle";
            break;
        case Kind::RouteSplit:
            p.kind = "route_split";
            p.powered = n.routeSplit.enabled;
            p.signalStrength = n.routeSplit.enabled ? 1.0 : 0.0;
            p.detail = "fan3";
            break;
        case Kind::FlowRise:
            p.kind = "flow_rise";
            p.powered = n.flowRise.enabled && n.flowRise.armed;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.flowRise.armed ? "armed" : "disarmed";
            break;
        case Kind::FlowFall:
            p.kind = "flow_fall";
            p.powered = n.flowFall.enabled && n.flowFall.armed;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.flowFall.armed ? "armed" : "disarmed";
            break;
        case Kind::FlowDbnc:
            p.kind = "flow_dbnc";
            p.powered = n.flowDbnc.enabled && n.flowDbnc.settleAt != 0;
            p.signalStrength = n.flowDbnc.stableHi ? 1.0 : 0.0;
            p.detail = n.flowDbnc.settleAt != 0 ? "pending" : (n.flowDbnc.stableHi ? "hi" : "lo");
            break;
        case Kind::FlowOneshot:
            p.kind = "flow_oneshot";
            p.powered = n.flowOneshot.enabled && n.flowOneshot.busy;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.flowOneshot.busy ? "busy" : "idle";
            break;
        case Kind::TimeDelay:
            p.kind = "time_delay";
            p.powered = n.timeDelay.enabled && n.timeDelay.hasPending;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.timeDelay.hasPending ? ("fire=" + std::to_string(n.timeDelay.fireAt)) : "idle";
            break;
        case Kind::TimeClock:
            p.kind = "time_clock";
            p.powered = n.timeClock.enabled && n.timeClock.clockEnabled;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.timeClock.clockEnabled ? ("hz=" + std::to_string(n.timeClock.hz)) : "off";
            break;
        case Kind::TimeWatch: {
            p.kind = "time_watch";
            p.powered = n.timeWatch.enabled && n.timeWatch.running;
            std::uint64_t totalMs = n.timeWatch.accumulatedMs;
            if (n.timeWatch.running && nowMs_ >= n.timeWatch.lapStartMs) {
                totalMs += (nowMs_ - n.timeWatch.lapStartMs);
            }
            p.signalStrength = std::min(1.0, static_cast<double>(totalMs) / 60000.0);
            p.detail = "ms=" + std::to_string(totalMs) + (n.timeWatch.running ? " run" : " stop");
            break;
        }
        case Kind::MemSample:
            p.kind = "mem_sample";
            p.powered = n.memSample.enabled;
            p.signalStrength = ProbeScalarHint(n.memSample.lastEmitted);
            p.detail = n.memSample.holding ? "hold" : "track";
            break;
        case Kind::MemChatter:
            p.kind = "mem_chatter";
            p.powered = n.memChatter.enabled && n.memChatter.settleAt != 0;
            p.signalStrength = n.memChatter.stableHi ? 1.0 : 0.0;
            p.detail = n.memChatter.settleAt != 0 ? "pending" : (n.memChatter.stableHi ? "hi" : "lo");
            break;
        case Kind::FlowDoOnce:
            p.kind = "flow_do_once";
            p.powered = n.flowDoOnce.enabled && n.flowDoOnce.consumed;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.flowDoOnce.consumed ? "done" : "ready";
            break;
        case Kind::FlowRelay:
            p.kind = "flow_relay";
            p.powered = n.flowRelay.enabled;
            p.signalStrength = n.flowRelay.enabled ? 1.0 : 0.0;
            p.detail = n.flowRelay.enabled ? "conducts" : "open";
            break;
        case Kind::IoButton:
            p.kind = "io_button";
            p.powered = n.ioButton.enabled && n.ioButton.held;
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.ioButton.held ? "down" : "up";
            break;
        case Kind::IoKeypad:
            p.kind = "io_keypad";
            p.powered = n.ioKeypad.enabled && !n.ioKeypad.buffer.empty();
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.ioKeypad.buffer.empty() ? "empty" : ("buf=" + n.ioKeypad.buffer);
            break;
        case Kind::IoDisplay:
            p.kind = "io_display";
            p.powered = n.ioDisplay.enabled && !n.ioDisplay.text.empty();
            p.signalStrength = p.powered ? 1.0 : 0.0;
            p.detail = n.ioDisplay.text.empty() ? "(no text)" : n.ioDisplay.text;
            break;
        case Kind::IoAudio:
            p.kind = "io_audio";
            p.powered = n.ioAudio.enabled && n.ioAudio.playing;
            p.signalStrength = n.ioAudio.enabled ? n.ioAudio.volume : 0.0;
            p.detail = n.ioAudio.playing ? ("vol=" + std::to_string(n.ioAudio.volume)) : "stopped";
            break;
        case Kind::IoLogger:
            p.kind = "io_logger";
            p.powered = n.ioLogger.enabled && !n.ioLogger.lastMessage.empty();
            if (n.ioLogger.lastLevel == "err") {
                p.signalStrength = p.powered ? 1.0 : 0.0;
            } else if (n.ioLogger.lastLevel == "warn") {
                p.signalStrength = p.powered ? 0.66 : 0.0;
            } else {
                p.signalStrength = p.powered ? 0.33 : 0.0;
            }
            p.detail = n.ioLogger.lastLevel.empty() ? "idle" : (n.ioLogger.lastLevel + ":" + n.ioLogger.lastMessage);
            break;
        case Kind::IoTrigger:
            p.kind = "io_trigger";
            p.powered = n.ioTrigger.armed && n.ioTrigger.held;
            p.signalStrength = n.ioTrigger.held ? 1.0 : 0.0;
            p.detail = n.ioTrigger.armed ? (n.ioTrigger.held ? "held" : "armed") : "disarmed";
            break;
        }
        out.push_back(std::move(p));
    }
    std::sort(out.begin(), out.end(), [](const LogicCircuitNodeProbe& a, const LogicCircuitNodeProbe& b) {
        return a.id < b.id;
    });
    return out;
}

void LogicGraph::DispatchInput(std::string_view nodeId, std::string_view inputName, LogicContext context) {
    struct DepthGuard {
        LogicGraph* graph = nullptr;
        explicit DepthGuard(LogicGraph* g) : graph(g) { ++graph->dispatchDepth_; }
        ~DepthGuard() {
            if (graph && --graph->dispatchDepth_ == 0) {
                graph->FlushAllPriorityLatches();
            }
        }
    };
    DepthGuard depthGuard(this);

    const std::string id(nodeId);
    const std::string normalizedIn = NormalizeName(inputName);
    if (inputDispatchHandler_) {
        inputDispatchHandler_(nodeId, normalizedIn, context);
    }

    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
        if (worldActorInputHandler_) {
            worldActorInputHandler_(nodeId, inputName, context);
        }
        return;
    }
    context.sourceId = id;
    NodeSlot& slot = it->second;

    switch (slot.kind) {
    case Kind::Relay:
        HandleRelay(id, slot, normalizedIn, context);
        break;
    case Kind::Timer:
        HandleTimer(id, slot, normalizedIn, context);
        break;
    case Kind::Counter:
        HandleCounter(id, slot, normalizedIn, context);
        break;
    case Kind::Compare:
        HandleCompare(id, slot, normalizedIn, context);
        break;
    case Kind::Sequencer:
        HandleSequencer(id, slot, normalizedIn, context);
        break;
    case Kind::Pulse:
        HandlePulse(id, slot, normalizedIn, context);
        break;
    case Kind::Latch:
        HandleLatch(id, slot, normalizedIn, context);
        break;
    case Kind::Channel:
        HandleChannel(id, slot, normalizedIn, context);
        break;
    case Kind::Merge:
        HandleMerge(id, slot, normalizedIn, context);
        break;
    case Kind::Split:
        HandleSplit(id, slot, normalizedIn, context);
        break;
    case Kind::Predicate:
        HandlePredicate(id, slot, normalizedIn, context);
        break;
    case Kind::InventoryGate:
        HandleInventoryGate(id, slot, normalizedIn, context);
        break;
    case Kind::TriggerDetector:
        HandleTriggerDetector(id, slot, normalizedIn, context);
        break;
    case Kind::GateAnd:
        HandleGateAnd(id, slot, normalizedIn, context);
        break;
    case Kind::GateOr:
        HandleGateOr(id, slot, normalizedIn, context);
        break;
    case Kind::GateNot:
        HandleGateNot(id, slot, normalizedIn, context);
        break;
    case Kind::GateBuf:
        HandleGateBuf(id, slot, normalizedIn, context);
        break;
    case Kind::GateXnor:
        HandleGateXnor(id, slot, normalizedIn, context);
        break;
    case Kind::GateXor:
        HandleGateXor(id, slot, normalizedIn, context);
        break;
    case Kind::GateNand:
        HandleGateNand(id, slot, normalizedIn, context);
        break;
    case Kind::GateNor:
        HandleGateNor(id, slot, normalizedIn, context);
        break;
    case Kind::MathAbs:
        HandleMathAbs(id, slot, normalizedIn, context);
        break;
    case Kind::MathMin:
        HandleMathMin(id, slot, normalizedIn, context);
        break;
    case Kind::MathMax:
        HandleMathMax(id, slot, normalizedIn, context);
        break;
    case Kind::MathClamp:
        HandleMathClamp(id, slot, normalizedIn, context);
        break;
    case Kind::MathRound:
        HandleMathRound(id, slot, normalizedIn, context);
        break;
    case Kind::RouteTee:
        HandleRouteTee(id, slot, normalizedIn, context);
        break;
    case Kind::MathLerp:
        HandleMathLerp(id, slot, normalizedIn, context);
        break;
    case Kind::MathSign:
        HandleMathSign(id, slot, normalizedIn, context);
        break;
    case Kind::MathAdd:
        HandleMathAdd(id, slot, normalizedIn, context);
        break;
    case Kind::MathSub:
        HandleMathSub(id, slot, normalizedIn, context);
        break;
    case Kind::MathMult:
        HandleMathMult(id, slot, normalizedIn, context);
        break;
    case Kind::MathDiv:
        HandleMathDiv(id, slot, normalizedIn, context);
        break;
    case Kind::MathMod:
        HandleMathMod(id, slot, normalizedIn, context);
        break;
    case Kind::MathCompare:
        HandleMathCompare(id, slot, normalizedIn, context);
        break;
    case Kind::RoutePass:
        HandleRoutePass(id, slot, normalizedIn, context);
        break;
    case Kind::RouteMux:
        HandleRouteMux(id, slot, normalizedIn, context);
        break;
    case Kind::RouteDemux:
        HandleRouteDemux(id, slot, normalizedIn, context);
        break;
    case Kind::RouteSelect:
        HandleRouteSelect(id, slot, normalizedIn, context);
        break;
    case Kind::RouteMerge:
        HandleRouteMerge(id, slot, normalizedIn, context);
        break;
    case Kind::RouteUnpack:
        HandleRouteUnpack(id, slot, normalizedIn, context);
        break;
    case Kind::RoutePack:
        HandleRoutePack(id, slot, normalizedIn, context);
        break;
    case Kind::MemEdge:
        HandleMemEdge(id, slot, normalizedIn, context);
        break;
    case Kind::FlowRandom:
        HandleFlowRandom(id, slot, normalizedIn, context);
        break;
    case Kind::RouteSplit:
        HandleRouteSplit(id, slot, normalizedIn, context);
        break;
    case Kind::FlowRise:
        HandleFlowRise(id, slot, normalizedIn, context);
        break;
    case Kind::FlowFall:
        HandleFlowFall(id, slot, normalizedIn, context);
        break;
    case Kind::FlowDbnc:
        HandleFlowDbnc(id, slot, normalizedIn, context);
        break;
    case Kind::FlowOneshot:
        HandleFlowOneshot(id, slot, normalizedIn, context);
        break;
    case Kind::TimeDelay:
        HandleTimeDelay(id, slot, normalizedIn, context);
        break;
    case Kind::TimeClock:
        HandleTimeClock(id, slot, normalizedIn, context);
        break;
    case Kind::TimeWatch:
        HandleTimeWatch(id, slot, normalizedIn, context);
        break;
    case Kind::MemSample:
        HandleMemSample(id, slot, normalizedIn, context);
        break;
    case Kind::MemChatter:
        HandleMemChatter(id, slot, normalizedIn, context);
        break;
    case Kind::FlowDoOnce:
        HandleFlowDoOnce(id, slot, normalizedIn, context);
        break;
    case Kind::FlowRelay:
        HandleFlowRelay(id, slot, normalizedIn, context);
        break;
    case Kind::IoButton:
        HandleIoButton(id, slot, normalizedIn, context);
        break;
    case Kind::IoKeypad:
        HandleIoKeypad(id, slot, normalizedIn, context);
        break;
    case Kind::IoDisplay:
        HandleIoDisplay(id, slot, normalizedIn, context);
        break;
    case Kind::IoAudio:
        HandleIoAudio(id, slot, normalizedIn, context);
        break;
    case Kind::IoLogger:
        HandleIoLogger(id, slot, normalizedIn, context);
        break;
    case Kind::IoTrigger:
        HandleIoTrigger(id, slot, normalizedIn, context);
        break;
    }
}

void LogicGraph::EmitOutputNormalized(const std::string& sourceId,
                                      std::string outputNameNormalized,
                                      LogicContext ctx,
                                      std::uint32_t intrinsicDelayMs) {
    ctx.sourceId = sourceId;
    if (outputHandler_) {
        LogicOutputEvent ev{sourceId, outputNameNormalized, ctx};
        outputHandler_(ev);
    }
    const std::string key = MakeRouteKey(sourceId, outputNameNormalized);
    auto it = routes_.find(key);
    if (it == routes_.end()) {
        return;
    }
    for (const LogicRouteTarget& target : it->second) {
        LogicContext next = ctx;
        if (target.edgeParameter) {
            next.parameter = target.edgeParameter;
        }
        const std::uint64_t sum = static_cast<std::uint64_t>(intrinsicDelayMs) + static_cast<std::uint64_t>(target.delayMs);
        const std::uint32_t d = ClampDelayMs(sum > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<std::uint32_t>(sum));
        if (d == 0) {
            DispatchInput(target.targetId, target.inputName, std::move(next));
        } else {
            pending_.push(
                PendingDelivery{nowMs_ + d, nextTieBreak_++, target.targetId, target.inputName, std::move(next)});
        }
    }
}

void LogicGraph::EmitOutput(const std::string& sourceId, const std::string& outputName, LogicContext ctx) {
    EmitOutputNormalized(sourceId, NormalizeName(outputName), std::move(ctx));
}

void LogicGraph::TimerStartFresh(const std::string& /*selfId*/, NodeSlot& n) {
    std::uint64_t interval = n.timerDef.intervalMs;
    if (interval > kMaxLogicDelayMs) {
        interval = kMaxLogicDelayMs;
    }
    if (n.timerDef.repeating && interval < 1) {
        interval = 1;
    }
    n.timer.startCount++;
    n.timer.scheduledWithStartCount = n.timer.startCount;
    n.timer.active = true;
    n.timer.fireAt = nowMs_ + interval;
}

void LogicGraph::TimerCancel(NodeSlot& n, bool /*fromDisable*/) {
    n.timer.active = false;
    n.timer.startCount++;
}

void LogicGraph::TimerFire(const std::string& selfId, NodeSlot& n) {
    if (!n.timer.enabled || !n.timer.active) {
        return;
    }
    if (n.timer.scheduledWithStartCount != n.timer.startCount) {
        return;
    }

    LogicContext ctx;
    ctx.sourceId = selfId;

    EmitOutput(selfId, "OnTimer", ctx);

    if (!n.timerDef.repeating) {
        EmitOutput(selfId, "OnFinished", ctx);
        TimerCancel(n, false);
    } else {
        std::uint64_t interval = n.timerDef.intervalMs;
        if (interval > kMaxLogicDelayMs) {
            interval = kMaxLogicDelayMs;
        }
        if (interval < 1) {
            interval = 1;
        }
        n.timer.fireAt = nowMs_ + interval;
    }
}

void LogicGraph::HandleRelay(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.relay.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.relay.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.relay.enabled = !n.relay.enabled;
        return;
    }
    if (in == "trigger" || in == "power" || in == "pulse") {
        if (!n.relay.enabled) {
            return;
        }
        if (!ctx.analogSignal.has_value()) {
            ctx.analogSignal = 1.0;
        }
        EmitOutput(selfId, "OnTrigger", ctx);
    }
}

void LogicGraph::HandleTimer(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    (void)ctx;
    if (in == "enable" || in == "turnon") {
        n.timer.enabled = true;
        if (n.timerDef.autoStart) {
            TimerStartFresh(selfId, n);
        }
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.timer.enabled = false;
        TimerCancel(n, true);
        return;
    }
    if (in == "toggle") {
        if (n.timer.active) {
            TimerCancel(n, false);
        } else {
            TimerStartFresh(selfId, n);
        }
        return;
    }
    if (in == "stop" || in == "cancel" || in == "canceltimer") {
        TimerCancel(n, false);
        return;
    }
    if (in == "trigger" || in == "start") {
        if (!n.timer.enabled) {
            return;
        }
        TimerStartFresh(selfId, n);
        return;
    }
    if (in == "reset") {
        if (!n.timer.enabled) {
            return;
        }
        TimerCancel(n, false);
        TimerStartFresh(selfId, n);
    }
}

void LogicGraph::AttachCounterPayload(LogicContext& ctx, double value, double delta) {
    ctx.fields["counterValue"] = std::to_string(value);
    ctx.fields["counterDelta"] = std::to_string(delta);
}

void LogicGraph::HandleCounter(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.counter.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.counter.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.counter.enabled = !n.counter.enabled;
        return;
    }

    auto clampValue = [&](double v) {
        if (n.counterDef.minValue) {
            v = std::max(v, *n.counterDef.minValue);
        }
        if (n.counterDef.maxValue) {
            v = std::min(v, *n.counterDef.maxValue);
        }
        return v;
    };

    auto emitDelta = [&](double oldV, double newV, bool forceChanged) {
        const double delta = newV - oldV;
        LogicContext out = ctx;
        AttachCounterPayload(out, newV, delta);
        const bool changed = !NearlyEqual(oldV, newV) || forceChanged;
        if (changed) {
            EmitOutput(selfId, "OnChanged", out);
        }
        if (delta > 0.0) {
            EmitOutput(selfId, "OnIncrement", out);
        }
        if (delta < 0.0) {
            EmitOutput(selfId, "OnDecrement", out);
        }
        if (!NearlyEqual(oldV, 0) && NearlyEqual(newV, 0)) {
            EmitOutput(selfId, "OnZero", out);
        }
        if (n.counterDef.minValue && NearlyEqual(newV, *n.counterDef.minValue)
            && (!NearlyEqual(oldV, *n.counterDef.minValue) || forceChanged)) {
            EmitOutput(selfId, "OnHitMin", out);
        }
        if (n.counterDef.maxValue && NearlyEqual(newV, *n.counterDef.maxValue)
            && (!NearlyEqual(oldV, *n.counterDef.maxValue) || forceChanged)) {
            EmitOutput(selfId, "OnHitMax", out);
        }
    };

    if (in == "reset") {
        const double oldV = n.counter.value;
        n.counter.value = clampValue(n.counterDef.startValue);
        emitDelta(oldV, n.counter.value, true);
        return;
    }

    if (!n.counter.enabled) {
        return;
    }

    if (in == "trigger" || in == "increment" || in == "add") {
        const double oldV = n.counter.value;
        const double add = ctx.parameter.value_or(n.counterDef.step);
        n.counter.value = clampValue(oldV + add);
        emitDelta(oldV, n.counter.value, false);
        return;
    }
    if (in == "decrement" || in == "subtract") {
        const double oldV = n.counter.value;
        const double sub = ctx.parameter.value_or(n.counterDef.step);
        n.counter.value = clampValue(oldV - sub);
        emitDelta(oldV, n.counter.value, false);
        return;
    }
    if (in == "setvalue" || in == "set") {
        const double oldV = n.counter.value;
        if (!ctx.parameter) {
            return;
        }
        n.counter.value = clampValue(*ctx.parameter);
        emitDelta(oldV, n.counter.value, false);
    }
}

LogicGraph::CompareObservation LogicGraph::ReadCompareObservation(const CompareDef& def) const {
    CompareObservation obs;
    if (def.sourceLogicEntityId) {
        obs.number = TryGetLogicNumericProperty(*def.sourceLogicEntityId, def.sourceProperty);
        return obs;
    }
    if (def.sourceWorldValueKey && spec_.worldValueQuery) {
        obs.number = spec_.worldValueQuery(*def.sourceWorldValueKey);
        return obs;
    }
    if (def.constantValue) {
        obs.number = def.constantValue;
        return obs;
    }
    if (def.constantText) {
        obs.hasText = true;
        obs.text = *def.constantText;
        char* end = nullptr;
        const double v = std::strtod(obs.text.c_str(), &end);
        if (end != obs.text.c_str() && std::isfinite(v)) {
            obs.number = v;
        }
        return obs;
    }
    return obs;
}

bool LogicGraph::EvaluateComparePredicate(const CompareDef& def, const CompareObservation& obs) {
    const bool numericTest = def.equalsValue || def.minValue || def.maxValue;
    if (numericTest) {
        if (!obs.number || !std::isfinite(*obs.number)) {
            return false;
        }
        const double v = *obs.number;
        if (def.equalsValue) {
            return NearlyEqual(v, *def.equalsValue);
        }
        if (def.minValue && def.maxValue) {
            return v >= *def.minValue && v <= *def.maxValue;
        }
        if (def.minValue) {
            return v >= *def.minValue;
        }
        if (def.maxValue) {
            return v <= *def.maxValue;
        }
    }
    if (obs.number && std::isfinite(*obs.number)) {
        return std::fabs(*obs.number) > 1e-12 || !NearlyEqual(*obs.number, 0);
    }
    if (obs.hasText) {
        return !TrimAscii(obs.text).empty();
    }
    return false;
}

void LogicGraph::AttachComparePayload(LogicContext& ctx, const CompareObservation& obs, bool result) {
    if (obs.number && std::isfinite(*obs.number)) {
        ctx.fields["compareValue"] = std::to_string(*obs.number);
    } else if (obs.hasText) {
        ctx.fields["compareValue"] = obs.text;
    } else {
        ctx.fields["compareValue"] = {};
    }
    ctx.fields["compareResult"] = result ? "true" : "false";
}

void LogicGraph::HandleCompare(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.compare.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.compare.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.compare.enabled = !n.compare.enabled;
        return;
    }
    if (in != "trigger" && in != "evaluate" && in != "compare") {
        return;
    }
    if (!n.compare.enabled) {
        return;
    }

    CompareObservation obs = ReadCompareObservation(n.compareDef);
    if (!obs.number && !obs.hasText && ctx.parameter) {
        obs.number = ctx.parameter;
    }

    const bool result = EvaluateComparePredicate(n.compareDef, obs);

    LogicContext out = ctx;
    AttachComparePayload(out, obs, result);

    EmitOutput(selfId, result ? "OnTrue" : "OnFalse", out);

    const std::optional<bool> prev = n.compare.lastResult;
    n.compare.lastResult = result;
    if (prev && *prev != result) {
        EmitOutput(selfId, result ? "OnBecomeTrue" : "OnBecomeFalse", out);
    }
}

void LogicGraph::HandleSequencer(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.sequencer.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.sequencer.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.sequencer.enabled = !n.sequencer.enabled;
        return;
    }
    if (in == "reset") {
        n.sequencer.index = 0;
        return;
    }
    if (in != "trigger" && in != "advance") {
        return;
    }
    if (!n.sequencer.enabled) {
        return;
    }

    int count = n.sequencerDef.stepCount;
    if (count < 1) {
        count = 1;
    }

    if (!n.sequencerDef.wrap && n.sequencer.index >= count) {
        return;
    }

    const int idx = n.sequencer.index % count;
    LogicContext out = ctx;
    out.fields["stepIndex"] = std::to_string(idx);
    EmitOutput(selfId, "OnStep", out);
    EmitOutput(selfId, "OnStep" + std::to_string(idx), out);

    const bool atEnd = idx + 1 >= count;
    if (atEnd) {
        if (n.sequencerDef.wrap) {
            n.sequencer.index = 0;
        } else {
            EmitOutput(selfId, "OnComplete", out);
            n.sequencer.index = count;
        }
    } else {
        n.sequencer.index = idx + 1;
    }
}

void LogicGraph::HandlePulse(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.pulse.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.pulse.enabled = false;
        if (n.pulse.held) {
            n.pulse.held = false;
            EmitOutput(selfId, "OnFall", ctx);
        }
        return;
    }
    if (in == "toggle") {
        n.pulse.enabled = !n.pulse.enabled;
        return;
    }
    if (in == "cancel") {
        if (n.pulse.held) {
            n.pulse.held = false;
            EmitOutput(selfId, "OnFall", ctx);
        }
        return;
    }
    if (in != "trigger" && in != "pulse") {
        return;
    }
    if (!n.pulse.enabled) {
        return;
    }

    std::uint64_t hold = n.pulseDef.holdMs;
    if (hold > kMaxLogicDelayMs) {
        hold = kMaxLogicDelayMs;
    }

    if (n.pulse.held) {
        if (n.pulseDef.retrigger == PulseRetriggerMode::Ignore) {
            return;
        }
        if (n.pulseDef.retrigger == PulseRetriggerMode::Extend) {
            n.pulse.releaseAt = nowMs_ + hold;
            return;
        }
        EmitOutput(selfId, "OnFall", ctx);
        n.pulse.held = false;
    }

    n.pulse.held = true;
    n.pulse.releaseAt = nowMs_ + hold;
    EmitOutput(selfId, "OnRise", ctx);
    EmitOutput(selfId, "OnActive", ctx);
}

void LogicGraph::HandleLatch(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.latch.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.latch.enabled = false;
        n.latch.pendingSet = false;
        n.latch.pendingReset = false;
        return;
    }
    if (!n.latch.enabled) {
        return;
    }

    if (n.latchDef.mode == LatchMode::PrioritySet) {
        if (in == "toggle") {
            const bool was = n.latch.value;
            n.latch.value = !n.latch.value;
            if (was != n.latch.value) {
                LogicContext out = ctx;
                out.fields["latchedValue"] = n.latch.value ? "true" : "false";
                EmitOutput(selfId, "OnChanged", out);
                EmitOutput(selfId, n.latch.value ? "OnTrue" : "OnFalse", out);
            }
            return;
        }
        if (in == "set") {
            n.latch.pendingSet = true;
            return;
        }
        if (in == "reset") {
            n.latch.pendingReset = true;
            return;
        }
        return;
    }

    const bool was = n.latch.value;
    if (n.latchDef.mode == LatchMode::Toggle) {
        if (in == "toggle" || in == "trigger") {
            n.latch.value = !n.latch.value;
        } else if (in == "set") {
            n.latch.value = true;
        } else if (in == "reset") {
            n.latch.value = false;
        } else {
            return;
        }
    } else {
        if (in == "set") {
            n.latch.value = true;
        } else if (in == "reset") {
            n.latch.value = false;
        } else if (in == "toggle") {
            n.latch.value = !n.latch.value;
        } else {
            return;
        }
    }

    if (was != n.latch.value) {
        LogicContext out = ctx;
        out.fields["latchedValue"] = n.latch.value ? "true" : "false";
        EmitOutput(selfId, "OnChanged", out);
        EmitOutput(selfId, n.latch.value ? "OnTrue" : "OnFalse", out);
    }
}

void LogicGraph::HandleChannel(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.channel.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.channel.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.channel.enabled = !n.channel.enabled;
        return;
    }
    if (!n.channel.enabled) {
        return;
    }

    if (n.channelDef.role != ChannelRole::Publish) {
        return;
    }
    if (in != "send" && in != "trigger") {
        return;
    }

    auto it = channelSubscribers_.find(n.channelDef.channelName);
    if (it == channelSubscribers_.end()) {
        return;
    }
    LogicContext msg = ctx;
    msg.sourceId = selfId;
    if (in == "trigger") {
        msg.parameter = std::nullopt;
    }
    for (const std::string& subId : it->second) {
        const auto subIt = nodes_.find(subId);
        if (subIt == nodes_.end() || subIt->second.kind != Kind::Channel || !subIt->second.channel.enabled) {
            continue;
        }
        EmitOutput(subId, "OnMessage", msg);
    }
}

void LogicGraph::HandleMerge(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.merge.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.merge.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.merge.enabled = !n.merge.enabled;
        return;
    }
    if (!n.merge.enabled) {
        return;
    }

    if (n.mergeDef.mode == MergeMode::Any) {
        if (in == "trigger") {
            EmitOutput(selfId, "OnTrigger", ctx);
        }
        return;
    }

    const int idx = ParseMergeInIndex(in);
    int need = n.mergeDef.expectedInputs;
    if (need < 1) {
        need = 1;
    }
    if (need > 31) {
        need = 31;
    }
    if (idx < 0 || idx >= need) {
        return;
    }
    n.merge.armedBits |= (1U << static_cast<unsigned>(idx));
    const std::uint32_t mask = need >= 32 ? 0xFFFFFFFFu : ((1U << static_cast<unsigned>(need)) - 1U);
    if ((n.merge.armedBits & mask) == mask) {
        n.merge.armedBits = 0;
        EmitOutput(selfId, "OnTrigger", ctx);
    }
}

void LogicGraph::HandleSplit(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.split.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.split.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.split.enabled = !n.split.enabled;
        return;
    }
    if (in != "trigger") {
        return;
    }
    if (!n.split.enabled) {
        return;
    }
    int branches = n.splitDef.branchCount;
    if (branches < 2) {
        branches = 2;
    }
    const double baseParam = ctx.parameter.value_or(1.0);
    for (int b = 0; b < branches; ++b) {
        LogicContext out = ctx;
        double scale = 1.0;
        if (b < static_cast<int>(n.splitDef.branchScales.size())) {
            scale = n.splitDef.branchScales[static_cast<std::size_t>(b)];
        }
        if (std::isfinite(scale)) {
            out.parameter = baseParam * scale;
        }
        std::uint32_t intrinsic = 0;
        if (b < static_cast<int>(n.splitDef.branchIntrinsicDelayMs.size())) {
            intrinsic = n.splitDef.branchIntrinsicDelayMs[static_cast<std::size_t>(b)];
        }
        EmitOutputNormalized(selfId, NormalizeName("Branch" + std::to_string(b)), out, intrinsic);
    }
}

bool LogicGraph::PredicatePasses(const PredicateDef& def, const LogicContext& ctx) const {
    if (def.rules.empty()) {
        return true;
    }
    for (const auto& rule : def.rules) {
        const std::string nk = NormalizeName(rule.first);
        const std::string& v = rule.second;
        if (nk == "instigatormustbe") {
            const auto it = ctx.fields.find("instigatorKind");
            const std::string kind = it != ctx.fields.end() ? it->second : std::string{};
            if (NormalizeName(kind) != NormalizeName(v)) {
                return false;
            }
        } else if (nk == "hasflag") {
            const auto it = ctx.fields.find("flags");
            if (it == ctx.fields.end() || it->second.find(v) == std::string::npos) {
                return false;
            }
        }
    }
    return true;
}

void LogicGraph::HandlePredicate(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.predicate.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.predicate.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.predicate.enabled = !n.predicate.enabled;
        return;
    }
    if (in != "trigger") {
        return;
    }
    if (!n.predicate.enabled) {
        return;
    }
    if (PredicatePasses(n.predicateDef, ctx)) {
        EmitOutput(selfId, "OnPass", ctx);
    } else {
        EmitOutput(selfId, "OnFail", ctx);
    }
}

void LogicGraph::HandleInventoryGate(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.inventoryGate.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.inventoryGate.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.inventoryGate.enabled = !n.inventoryGate.enabled;
        return;
    }
    if (in != "trigger" && in != "evaluate") {
        return;
    }
    if (!n.inventoryGate.enabled) {
        return;
    }

    bool pass = false;
    if (spec_.inventoryQuery) {
        pass = spec_.inventoryQuery(ctx.instigatorId,
                                    n.inventoryGateDef.itemId,
                                    n.inventoryGateDef.quantity,
                                    n.inventoryGateDef.consumeOnPass);
    }

    LogicContext out = ctx;
    out.fields["inventoryItemId"] = n.inventoryGateDef.itemId;
    out.fields["inventoryPass"] = pass ? "true" : "false";

    EmitOutput(selfId, pass ? "OnPass" : "OnFail", out);

    const std::optional<bool> prev = n.inventoryGate.lastResult;
    n.inventoryGate.lastResult = pass;
    if (prev && *prev != pass) {
        EmitOutput(selfId, pass ? "OnBecomeTrue" : "OnBecomeFalse", out);
    }
}

bool LogicGraph::ContextHasTag(const LogicContext& ctx, std::string_view tag) {
    if (tag.empty()) {
        return false;
    }
    auto it = ctx.fields.find("tags");
    if (it == ctx.fields.end()) {
        return false;
    }
    const std::string needle = NormalizeName(std::string(tag));
    std::string_view rest = it->second;
    while (!rest.empty()) {
        const std::size_t comma = rest.find(',');
        std::string_view tok = rest.substr(0, comma);
        tok = TrimAscii(tok);
        if (!tok.empty() && NormalizeName(std::string(tok)) == needle) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(comma + 1);
    }
    return false;
}

bool LogicGraph::InstigatorMatchesDetector(const TriggerDetectorDef& def, const LogicContext& ctx) {
    switch (def.instigatorFilter) {
    case TriggerInstigatorFilter::Any:
        return true;
    case TriggerInstigatorFilter::Player: {
        auto it = ctx.fields.find("instigatorKind");
        const std::string kind = it != ctx.fields.end() ? it->second : std::string{};
        return NormalizeName(kind) == "player";
    }
    case TriggerInstigatorFilter::Npc: {
        auto it = ctx.fields.find("instigatorKind");
        const std::string kind = it != ctx.fields.end() ? it->second : std::string{};
        return NormalizeName(kind) == "npc";
    }
    case TriggerInstigatorFilter::Tag:
        return ContextHasTag(ctx, def.instigatorTag);
    default:
        return true;
    }
}

void LogicGraph::FlushAllPriorityLatches() {
    for (auto& [id, slot] : nodes_) {
        if (slot.kind != Kind::Latch || slot.latchDef.mode != LatchMode::PrioritySet) {
            continue;
        }
        if (!slot.latch.pendingSet && !slot.latch.pendingReset) {
            continue;
        }
        bool newV = slot.latch.value;
        if (slot.latch.pendingSet && slot.latch.pendingReset) {
            newV = true;
        } else if (slot.latch.pendingSet) {
            newV = true;
        } else if (slot.latch.pendingReset) {
            newV = false;
        }
        slot.latch.pendingSet = false;
        slot.latch.pendingReset = false;
        const bool was = slot.latch.value;
        if (newV == was) {
            continue;
        }
        slot.latch.value = newV;
        LogicContext out;
        out.sourceId = id;
        out.fields["latchedValue"] = newV ? "true" : "false";
        EmitOutput(id, "OnChanged", out);
        EmitOutput(id, newV ? "OnTrue" : "OnFalse", out);
    }
}

void LogicGraph::HandleTriggerDetector(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.triggerDetector.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.triggerDetector.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.triggerDetector.enabled = !n.triggerDetector.enabled;
        return;
    }
    if (in == "reset") {
        n.triggerDetector.onceSeen.clear();
        n.triggerDetector.waitingExit.clear();
        return;
    }
    if (in == "endtouch" || in == "onendtouch") {
        const std::string inst = ctx.instigatorId;
        if (!inst.empty()) {
            n.triggerDetector.waitingExit.erase(inst);
        }
        return;
    }

    if (in != "trigger") {
        return;
    }
    if (!n.triggerDetector.enabled) {
        return;
    }

    const std::string inst = ctx.instigatorId.empty() ? std::string{"_anonymous"} : ctx.instigatorId;

    if (!InstigatorMatchesDetector(n.triggerDetectorDef, ctx)) {
        EmitOutput(selfId, "OnReject", ctx);
        return;
    }

    if (n.triggerDetectorDef.requireExitBeforeRetrigger) {
        auto w = n.triggerDetector.waitingExit.find(inst);
        if (w != n.triggerDetector.waitingExit.end() && w->second) {
            EmitOutput(selfId, "OnReject", ctx);
            return;
        }
    }

    if (n.triggerDetectorDef.oncePerInstigator) {
        if (n.triggerDetector.onceSeen.contains(inst)) {
            EmitOutput(selfId, "OnReject", ctx);
            return;
        }
    }

    if (n.triggerDetectorDef.cooldownMs > 0 && n.triggerDetector.lastGlobalFireMs != 0) {
        if (nowMs_ < n.triggerDetector.lastGlobalFireMs + n.triggerDetectorDef.cooldownMs) {
            EmitOutput(selfId, "OnReject", ctx);
            return;
        }
    }

    if (n.triggerDetectorDef.oncePerInstigator) {
        n.triggerDetector.onceSeen[inst] = true;
    }
    if (n.triggerDetectorDef.requireExitBeforeRetrigger) {
        n.triggerDetector.waitingExit[inst] = true;
    }
    n.triggerDetector.lastGlobalFireMs = nowMs_;

    EmitOutput(selfId, "OnPass", ctx);
}

void LogicGraph::HandleGateAnd(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.gateAnd.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    if (in == "a") {
        n.gateAnd.armedA = true;
    } else {
        n.gateAnd.armedB = true;
    }
    if (n.gateAnd.armedA && n.gateAnd.armedB) {
        EmitOutput(selfId, "Out", ctx);
        n.gateAnd.armedA = false;
        n.gateAnd.armedB = false;
    }
}

void LogicGraph::HandleGateOr(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.gateOr.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    EmitOutput(selfId, "Out", ctx);
}

void LogicGraph::HandleGateNot(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.gateNot.enabled) {
        return;
    }
    if (in != "in") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    LogicContext out = ctx;
    out.analogSignal = hi ? 0.0 : 1.0;
    EmitOutput(selfId, "Out", out);
}

void LogicGraph::HandleGateBuf(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.gateBuf.enabled) {
        return;
    }
    if (in != "in") {
        return;
    }
    LogicContext out = ctx;
    if (!out.analogSignal.has_value()) {
        out.analogSignal = 1.0;
    }
    n.gateBuf.lastAnalog = out.analogSignal;
    EmitOutput(selfId, "Out", out);
}

void LogicGraph::HandleGateXnor(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.gateXnor.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    if (in == "a") {
        n.gateXnor.armedA = true;
        n.gateXnor.hiA = hi;
    } else {
        n.gateXnor.armedB = true;
        n.gateXnor.hiB = hi;
    }
    if (n.gateXnor.armedA && n.gateXnor.armedB) {
        LogicContext out = ctx;
        const bool xn = n.gateXnor.hiA == n.gateXnor.hiB;
        out.analogSignal = xn ? 1.0 : 0.0;
        EmitOutput(selfId, "Out", out);
        n.gateXnor.armedA = false;
        n.gateXnor.armedB = false;
    }
}

void LogicGraph::HandleGateXor(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.gateXor.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    if (in == "a") {
        n.gateXor.armedA = true;
        n.gateXor.hiA = hi;
    } else {
        n.gateXor.armedB = true;
        n.gateXor.hiB = hi;
    }
    if (n.gateXor.armedA && n.gateXor.armedB) {
        LogicContext out = ctx;
        const bool x = n.gateXor.hiA != n.gateXor.hiB;
        out.analogSignal = x ? 1.0 : 0.0;
        EmitOutput(selfId, "Out", out);
        n.gateXor.armedA = false;
        n.gateXor.armedB = false;
    }
}

void LogicGraph::HandleGateNand(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.gateNand.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    if (in == "a") {
        n.gateNand.armedA = true;
        n.gateNand.hiA = hi;
    } else {
        n.gateNand.armedB = true;
        n.gateNand.hiB = hi;
    }
    if (n.gateNand.armedA && n.gateNand.armedB) {
        LogicContext out = ctx;
        const bool nandOut = !(n.gateNand.hiA && n.gateNand.hiB);
        out.analogSignal = nandOut ? 1.0 : 0.0;
        EmitOutput(selfId, "Out", out);
        n.gateNand.armedA = false;
        n.gateNand.armedB = false;
    }
}

void LogicGraph::HandleGateNor(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.gateNor.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    if (in == "a") {
        n.gateNor.armedA = true;
        n.gateNor.hiA = hi;
    } else {
        n.gateNor.armedB = true;
        n.gateNor.hiB = hi;
    }
    if (n.gateNor.armedA && n.gateNor.armedB) {
        LogicContext out = ctx;
        const bool norOut = !(n.gateNor.hiA || n.gateNor.hiB);
        out.analogSignal = norOut ? 1.0 : 0.0;
        EmitOutput(selfId, "Out", out);
        n.gateNor.armedA = false;
        n.gateNor.armedB = false;
    }
}

void LogicGraph::HandleMathAbs(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathAbs.enabled) {
        return;
    }
    if (in != "in") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    const double y = std::fabs(v);
    LogicContext out = ctx;
    out.analogSignal = y;
    out.parameter = y;
    n.mathAbs.lastOut = y;
    EmitOutput(selfId, "Out", out);
}

void LogicGraph::HandleMathMin(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathMin.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "a") {
        n.mathMin.armedA = true;
        n.mathMin.lastA = v;
    } else {
        n.mathMin.armedB = true;
        n.mathMin.lastB = v;
    }
    if (n.mathMin.armedA && n.mathMin.armedB) {
        LogicContext out = ctx;
        const double y = std::min(n.mathMin.lastA, n.mathMin.lastB);
        out.analogSignal = y;
        out.parameter = y;
        n.mathMin.lastOut = y;
        EmitOutput(selfId, "Out", out);
        n.mathMin.armedA = false;
        n.mathMin.armedB = false;
    }
}

void LogicGraph::HandleMathMax(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathMax.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "a") {
        n.mathMax.armedA = true;
        n.mathMax.lastA = v;
    } else {
        n.mathMax.armedB = true;
        n.mathMax.lastB = v;
    }
    if (n.mathMax.armedA && n.mathMax.armedB) {
        LogicContext out = ctx;
        const double y = std::max(n.mathMax.lastA, n.mathMax.lastB);
        out.analogSignal = y;
        out.parameter = y;
        n.mathMax.lastOut = y;
        EmitOutput(selfId, "Out", out);
        n.mathMax.armedA = false;
        n.mathMax.armedB = false;
    }
}

void LogicGraph::HandleMathClamp(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathClamp.enabled) {
        return;
    }
    if (in != "val" && in != "lo" && in != "hi") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "val") {
        n.mathClamp.armedVal = true;
        n.mathClamp.lastVal = v;
    } else if (in == "lo") {
        n.mathClamp.armedLo = true;
        n.mathClamp.lastLo = v;
    } else {
        n.mathClamp.armedHi = true;
        n.mathClamp.lastHi = v;
    }
    if (n.mathClamp.armedVal && n.mathClamp.armedLo && n.mathClamp.armedHi) {
        LogicContext out = ctx;
        const double lo = std::min(n.mathClamp.lastLo, n.mathClamp.lastHi);
        const double hi = std::max(n.mathClamp.lastLo, n.mathClamp.lastHi);
        const double y = std::max(lo, std::min(n.mathClamp.lastVal, hi));
        out.analogSignal = y;
        out.parameter = y;
        n.mathClamp.lastOut = y;
        EmitOutput(selfId, "Out", out);
        n.mathClamp.armedVal = false;
        n.mathClamp.armedLo = false;
        n.mathClamp.armedHi = false;
    }
}

void LogicGraph::HandleMathRound(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathRound.enabled) {
        return;
    }
    if (in != "in") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    const double y = std::round(v);
    LogicContext out = ctx;
    out.analogSignal = y;
    out.parameter = y;
    n.mathRound.lastOut = y;
    EmitOutput(selfId, "Out", out);
}

void LogicGraph::HandleRouteTee(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routeTee.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routeTee.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routeTee.enabled = !n.routeTee.enabled;
        return;
    }
    if (in != "in") {
        return;
    }
    if (!n.routeTee.enabled) {
        return;
    }
    EmitOutput(selfId, "A", ctx);
    EmitOutput(selfId, "B", ctx);
}

void LogicGraph::HandleMathLerp(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathLerp.enabled) {
        return;
    }
    if (in != "a" && in != "b" && in != "t") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "a") {
        n.mathLerp.armedA = true;
        n.mathLerp.lastA = v;
    } else if (in == "b") {
        n.mathLerp.armedB = true;
        n.mathLerp.lastB = v;
    } else {
        n.mathLerp.armedT = true;
        n.mathLerp.lastT = v;
    }
    if (n.mathLerp.armedA && n.mathLerp.armedB && n.mathLerp.armedT) {
        const double t = ClampUnitInterval(n.mathLerp.lastT);
        const double y = n.mathLerp.lastA + (n.mathLerp.lastB - n.mathLerp.lastA) * t;
        LogicContext out = ctx;
        out.analogSignal = y;
        out.parameter = y;
        n.mathLerp.lastOut = y;
        EmitOutput(selfId, "Out", out);
        n.mathLerp.armedA = false;
        n.mathLerp.armedB = false;
        n.mathLerp.armedT = false;
    }
}

void LogicGraph::HandleMathSign(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathSign.enabled) {
        return;
    }
    if (in != "in") {
        return;
    }
    const double x = SanitizeScalar(ScalarPulseValue(ctx));
    constexpr double kEps = 1e-12;
    LogicContext signOut = ctx;
    n.mathSign.lastZeroPulse = false;
    if (std::fabs(x) < kEps) {
        signOut.analogSignal = 0.0;
        signOut.parameter = 0.0;
        n.mathSign.lastSign = 0.0;
        EmitOutput(selfId, "Sign", signOut);
        LogicContext zeroOut = ctx;
        zeroOut.analogSignal = 1.0;
        zeroOut.parameter = 1.0;
        n.mathSign.lastZeroPulse = true;
        EmitOutput(selfId, "Zero", zeroOut);
        return;
    }
    const double s = x > 0.0 ? 1.0 : -1.0;
    signOut.analogSignal = s;
    signOut.parameter = s;
    n.mathSign.lastSign = s;
    EmitOutput(selfId, "Sign", signOut);
}

void LogicGraph::HandleMathAdd(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathAdd.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "a") {
        n.mathAdd.armedA = true;
        n.mathAdd.lastA = v;
    } else {
        n.mathAdd.armedB = true;
        n.mathAdd.lastB = v;
    }
    if (n.mathAdd.armedA && n.mathAdd.armedB) {
        const double sum = n.mathAdd.lastA + n.mathAdd.lastB;
        const bool hiA = n.mathAdd.lastA > 0.5;
        const bool hiB = n.mathAdd.lastB > 0.5;
        const double carry = (hiA && hiB) ? 1.0 : 0.0;
        LogicContext sumOut = ctx;
        sumOut.analogSignal = sum;
        sumOut.parameter = sum;
        n.mathAdd.lastSum = sum;
        EmitOutput(selfId, "Sum", sumOut);
        LogicContext carryOut = ctx;
        carryOut.analogSignal = carry;
        carryOut.parameter = carry;
        n.mathAdd.lastCarry = carry;
        EmitOutput(selfId, "Carry", carryOut);
        n.mathAdd.armedA = false;
        n.mathAdd.armedB = false;
    }
}

void LogicGraph::HandleMathSub(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathSub.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "a") {
        n.mathSub.armedA = true;
        n.mathSub.lastA = v;
    } else {
        n.mathSub.armedB = true;
        n.mathSub.lastB = v;
    }
    if (n.mathSub.armedA && n.mathSub.armedB) {
        const double diff = n.mathSub.lastA - n.mathSub.lastB;
        const double borrow = (n.mathSub.lastA < n.mathSub.lastB) ? 1.0 : 0.0;
        LogicContext diffOut = ctx;
        diffOut.analogSignal = diff;
        diffOut.parameter = diff;
        n.mathSub.lastDiff = diff;
        EmitOutput(selfId, "Diff", diffOut);
        LogicContext borrowOut = ctx;
        borrowOut.analogSignal = borrow;
        borrowOut.parameter = borrow;
        n.mathSub.lastBorrow = borrow;
        EmitOutput(selfId, "Borrow", borrowOut);
        n.mathSub.armedA = false;
        n.mathSub.armedB = false;
    }
}

void LogicGraph::HandleMathMult(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathMult.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "a") {
        n.mathMult.armedA = true;
        n.mathMult.lastA = v;
    } else {
        n.mathMult.armedB = true;
        n.mathMult.lastB = v;
    }
    if (n.mathMult.armedA && n.mathMult.armedB) {
        const double prod = n.mathMult.lastA * n.mathMult.lastB;
        const double p = SanitizeScalar(prod);
        LogicContext out = ctx;
        out.analogSignal = p;
        out.parameter = p;
        n.mathMult.lastOut = p;
        EmitOutput(selfId, "Prod", out);
        n.mathMult.armedA = false;
        n.mathMult.armedB = false;
    }
}

void LogicGraph::HandleMathDiv(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathDiv.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "a") {
        n.mathDiv.armedA = true;
        n.mathDiv.lastA = v;
    } else {
        n.mathDiv.armedB = true;
        n.mathDiv.lastB = v;
    }
    if (n.mathDiv.armedA && n.mathDiv.armedB) {
        constexpr double kTiny = 1e-15;
        double quot = 0;
        double rem = 0;
        if (std::fabs(n.mathDiv.lastB) >= kTiny) {
            quot = std::trunc(n.mathDiv.lastA / n.mathDiv.lastB);
            rem = n.mathDiv.lastA - quot * n.mathDiv.lastB;
        }
        quot = SanitizeScalar(quot);
        rem = SanitizeScalar(rem);
        LogicContext qOut = ctx;
        qOut.analogSignal = quot;
        qOut.parameter = quot;
        n.mathDiv.lastQuot = quot;
        EmitOutput(selfId, "Quot", qOut);
        LogicContext rOut = ctx;
        rOut.analogSignal = rem;
        rOut.parameter = rem;
        n.mathDiv.lastRem = rem;
        EmitOutput(selfId, "Rem", rOut);
        n.mathDiv.armedA = false;
        n.mathDiv.armedB = false;
    }
}

void LogicGraph::HandleMathMod(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathMod.enabled) {
        return;
    }
    if (in != "val" && in != "mod") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "val") {
        n.mathMod.armedVal = true;
        n.mathMod.lastVal = v;
    } else {
        n.mathMod.armedMod = true;
        n.mathMod.lastMod = v;
    }
    if (n.mathMod.armedVal && n.mathMod.armedMod) {
        constexpr double kTiny = 1e-15;
        double y = 0;
        if (std::fabs(n.mathMod.lastMod) >= kTiny) {
            y = std::fmod(n.mathMod.lastVal, n.mathMod.lastMod);
        }
        y = SanitizeScalar(y);
        LogicContext out = ctx;
        out.analogSignal = y;
        out.parameter = y;
        n.mathMod.lastOut = y;
        EmitOutput(selfId, "Rem", out);
        n.mathMod.armedVal = false;
        n.mathMod.armedMod = false;
    }
}

void LogicGraph::HandleMathCompare(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.mathCompare.enabled) {
        return;
    }
    if (in != "a" && in != "b") {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "a") {
        n.mathCompare.armedA = true;
        n.mathCompare.lastA = v;
    } else {
        n.mathCompare.armedB = true;
        n.mathCompare.lastB = v;
    }
    if (n.mathCompare.armedA && n.mathCompare.armedB) {
        const double a = n.mathCompare.lastA;
        const double b = n.mathCompare.lastB;
        LogicContext out = ctx;
        out.analogSignal = 1.0;
        out.parameter = 1.0;
        if (NearlyEqual(a, b)) {
            n.mathCompare.lastOrder = 0;
            EmitOutput(selfId, "A==B", out);
        } else if (a > b) {
            n.mathCompare.lastOrder = 1;
            EmitOutput(selfId, "A>B", out);
        } else {
            n.mathCompare.lastOrder = -1;
            EmitOutput(selfId, "A<B", out);
        }
        n.mathCompare.armedA = false;
        n.mathCompare.armedB = false;
    }
}

void LogicGraph::HandleRoutePass(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routePass.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routePass.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routePass.enabled = !n.routePass.enabled;
        return;
    }
    if (in == "en") {
        n.routePass.passOpen = PulseInputHigh(ctx);
        return;
    }
    if (in != "in") {
        return;
    }
    if (!n.routePass.enabled || !n.routePass.passOpen) {
        return;
    }
    EmitOutput(selfId, "Out", ctx);
}

void LogicGraph::HandleRouteMux(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routeMux.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routeMux.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routeMux.enabled = !n.routeMux.enabled;
        return;
    }
    if (in == "sel") {
        n.routeMux.selHigh = PulseInputHigh(ctx);
        return;
    }
    if (!n.routeMux.enabled) {
        return;
    }
    if (in == "a") {
        if (!n.routeMux.selHigh) {
            EmitOutput(selfId, "Out", ctx);
        }
        return;
    }
    if (in == "b") {
        if (n.routeMux.selHigh) {
            EmitOutput(selfId, "Out", ctx);
        }
    }
}

void LogicGraph::HandleRouteDemux(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routeDemux.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routeDemux.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routeDemux.enabled = !n.routeDemux.enabled;
        return;
    }
    if (in == "sel") {
        n.routeDemux.selHigh = PulseInputHigh(ctx);
        return;
    }
    if (in != "in") {
        return;
    }
    if (!n.routeDemux.enabled) {
        return;
    }
    if (n.routeDemux.selHigh) {
        EmitOutput(selfId, "B", ctx);
    } else {
        EmitOutput(selfId, "A", ctx);
    }
}

void LogicGraph::HandleRouteSelect(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routeSelect.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routeSelect.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routeSelect.enabled = !n.routeSelect.enabled;
        return;
    }
    if (in == "sel") {
        n.routeSelect.lastSel = SanitizeScalar(ScalarPulseValue(ctx));
        n.routeSelect.selIdx = RouteSelectIndexFromScalar(n.routeSelect.lastSel);
        return;
    }
    if (!n.routeSelect.enabled) {
        return;
    }
    if (in == "in0" && n.routeSelect.selIdx == 0) {
        EmitOutput(selfId, "Out", ctx);
    } else if (in == "in1" && n.routeSelect.selIdx == 1) {
        EmitOutput(selfId, "Out", ctx);
    } else if (in == "in2" && n.routeSelect.selIdx == 2) {
        EmitOutput(selfId, "Out", ctx);
    }
}

void LogicGraph::HandleRouteMerge(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routeMerge.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routeMerge.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routeMerge.enabled = !n.routeMerge.enabled;
        return;
    }
    if (!n.routeMerge.enabled) {
        return;
    }
    if (in == "in_1") {
        n.routeMerge.armed1 = true;
    } else if (in == "in_2") {
        n.routeMerge.armed2 = true;
    } else if (in == "in_3") {
        n.routeMerge.armed3 = true;
    } else {
        return;
    }
    if (n.routeMerge.armed1 && n.routeMerge.armed2 && n.routeMerge.armed3) {
        EmitOutput(selfId, "Out", ctx);
        n.routeMerge.armed1 = false;
        n.routeMerge.armed2 = false;
        n.routeMerge.armed3 = false;
    }
}

void LogicGraph::HandleRouteUnpack(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routeUnpack.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routeUnpack.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routeUnpack.enabled = !n.routeUnpack.enabled;
        return;
    }
    if (in != "busin") {
        return;
    }
    if (!n.routeUnpack.enabled) {
        return;
    }
    EmitOutput(selfId, "B0", ctx);
    EmitOutput(selfId, "B1", ctx);
    EmitOutput(selfId, "B2", ctx);
    EmitOutput(selfId, "B3", ctx);
}

void LogicGraph::HandleRoutePack(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routePack.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routePack.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routePack.enabled = !n.routePack.enabled;
        return;
    }
    if (!n.routePack.enabled) {
        return;
    }
    const double v = SanitizeScalar(ScalarPulseValue(ctx));
    if (in == "b0") {
        n.routePack.armed0 = true;
        n.routePack.v0 = v;
    } else if (in == "b1") {
        n.routePack.armed1 = true;
        n.routePack.v1 = v;
    } else if (in == "b2") {
        n.routePack.armed2 = true;
        n.routePack.v2 = v;
    } else if (in == "b3") {
        n.routePack.armed3 = true;
        n.routePack.v3 = v;
    } else {
        return;
    }
    if (n.routePack.armed0 && n.routePack.armed1 && n.routePack.armed2 && n.routePack.armed3) {
        LogicContext out = ctx;
        const double sum = n.routePack.v0 + n.routePack.v1 + n.routePack.v2 + n.routePack.v3;
        out.analogSignal = sum;
        out.parameter = sum;
        out.fields["b0"] = std::to_string(n.routePack.v0);
        out.fields["b1"] = std::to_string(n.routePack.v1);
        out.fields["b2"] = std::to_string(n.routePack.v2);
        out.fields["b3"] = std::to_string(n.routePack.v3);
        EmitOutput(selfId, "BusOut", out);
        n.routePack.armed0 = false;
        n.routePack.armed1 = false;
        n.routePack.armed2 = false;
        n.routePack.armed3 = false;
    }
}

void LogicGraph::HandleMemEdge(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.memEdge.enabled) {
        return;
    }
    if (in == "reset") {
        n.memEdge.hasLast = false;
        return;
    }
    if (in != "sig") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    if (!n.memEdge.hasLast) {
        n.memEdge.hasLast = true;
        n.memEdge.lastHi = hi;
        return;
    }
    if (hi && !n.memEdge.lastHi) {
        EmitOutput(selfId, "Rise", ctx);
    } else if (!hi && n.memEdge.lastHi) {
        EmitOutput(selfId, "Fall", ctx);
    }
    n.memEdge.lastHi = hi;
}

void LogicGraph::HandleFlowRandom(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.flowRandom.enabled) {
        return;
    }
    if (in != "trigger" && in != "power" && in != "pulse") {
        return;
    }
    n.flowRandom.lcgState ^= static_cast<std::uint64_t>(nowMs_) * 0xD6E8FEB866CF8519ULL;
    double minV = 0.0;
    double maxV = 1.0;
    ResolveFlowRandomRange(ctx, minV, maxV);
    const double span = maxV - minV;
    const double u = NextUnit01(n.flowRandom.lcgState);
    const double val = minV + u * span;
    n.flowRandom.lastVal = val;

    LogicContext outVal = ctx;
    outVal.analogSignal = val;
    outVal.parameter = val;
    outVal.fields["min"] = std::to_string(minV);
    outVal.fields["max"] = std::to_string(maxV);
    EmitOutput(selfId, "Val", outVal);

    LogicContext outMin = ctx;
    outMin.analogSignal = minV;
    outMin.parameter = minV;
    EmitOutput(selfId, "Min", outMin);

    LogicContext outMax = ctx;
    outMax.analogSignal = maxV;
    outMax.parameter = maxV;
    EmitOutput(selfId, "Max", outMax);
}

void LogicGraph::HandleRouteSplit(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.routeSplit.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.routeSplit.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.routeSplit.enabled = !n.routeSplit.enabled;
        return;
    }
    if (in != "in") {
        return;
    }
    if (!n.routeSplit.enabled) {
        return;
    }
    EmitOutput(selfId, "Out_1", ctx);
    EmitOutput(selfId, "Out_2", ctx);
    EmitOutput(selfId, "Out_3", ctx);
}

void LogicGraph::HandleFlowRise(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.flowRise.enabled) {
        return;
    }
    if (in == "arm") {
        const bool wasArmed = n.flowRise.armed;
        n.flowRise.armed = PulseInputHigh(ctx);
        if (n.flowRise.armed && !wasArmed) {
            n.flowRise.reseedNextIn = true;
        }
        return;
    }
    if (in != "in") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    if (!n.flowRise.armed) {
        n.flowRise.hasLastIn = true;
        n.flowRise.lastInHi = hi;
        return;
    }
    if (n.flowRise.reseedNextIn) {
        n.flowRise.lastInHi = hi;
        n.flowRise.hasLastIn = true;
        n.flowRise.reseedNextIn = false;
        return;
    }
    if (n.flowRise.hasLastIn && hi && !n.flowRise.lastInHi) {
        EmitOutput(selfId, "Pulse", ctx);
    }
    n.flowRise.lastInHi = hi;
    n.flowRise.hasLastIn = true;
}

void LogicGraph::HandleFlowFall(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.flowFall.enabled) {
        return;
    }
    if (in == "arm") {
        const bool wasArmed = n.flowFall.armed;
        n.flowFall.armed = PulseInputHigh(ctx);
        if (n.flowFall.armed && !wasArmed) {
            n.flowFall.reseedNextIn = true;
        }
        return;
    }
    if (in != "in") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    if (!n.flowFall.armed) {
        n.flowFall.hasLastIn = true;
        n.flowFall.lastInHi = hi;
        return;
    }
    if (n.flowFall.reseedNextIn) {
        n.flowFall.lastInHi = hi;
        n.flowFall.hasLastIn = true;
        n.flowFall.reseedNextIn = false;
        return;
    }
    if (n.flowFall.hasLastIn && !hi && n.flowFall.lastInHi) {
        EmitOutput(selfId, "Pulse", ctx);
    }
    n.flowFall.lastInHi = hi;
    n.flowFall.hasLastIn = true;
}

void LogicGraph::HandleFlowDbnc(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.flowDbnc.enabled) {
        return;
    }
    if (in == "ms") {
        const double v = ScalarPulseValue(ctx);
        if (v >= 0.0 && std::isfinite(v)) {
            n.flowDbnc.debounceMs =
                static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(v), kMaxLogicDelayMs));
        }
        return;
    }
    if (in == "rst" || in == "reset") {
        n.flowDbnc.settleAt = 0;
        n.flowDbnc.rawHi = false;
        n.flowDbnc.pendingHi = false;
        if (n.flowDbnc.stableHi) {
            n.flowDbnc.stableHi = false;
            LogicContext out = ctx;
            out.analogSignal = 0.0;
            out.parameter = 0.0;
            EmitOutput(selfId, "Out", out);
        }
        return;
    }
    if (in != "in") {
        return;
    }
    const bool hi = PulseInputHigh(ctx);
    n.flowDbnc.rawHi = hi;
    n.flowDbnc.pendingHi = hi;
    if (n.flowDbnc.debounceMs == 0) {
        n.flowDbnc.settleAt = 0;
        if (n.flowDbnc.stableHi != hi) {
            n.flowDbnc.stableHi = hi;
            LogicContext out = ctx;
            out.analogSignal = hi ? 1.0 : 0.0;
            out.parameter = out.analogSignal;
            EmitOutput(selfId, "Out", out);
        }
        return;
    }
    const std::uint32_t ms = std::max<std::uint32_t>(1u, n.flowDbnc.debounceMs);
    const std::uint64_t next = nowMs_ + static_cast<std::uint64_t>(ms);
    n.flowDbnc.settleAt = next < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : next;
}

void LogicGraph::HandleFlowOneshot(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.flowOneshot.enabled) {
        return;
    }
    if (in == "ms") {
        const double v = ScalarPulseValue(ctx);
        if (v >= 0.0 && std::isfinite(v)) {
            const std::uint32_t pm =
                static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(v), kMaxLogicDelayMs));
            n.flowOneshot.pulseMs = std::max<std::uint32_t>(1u, pm);
        }
        return;
    }
    if (in != "trig" && in != "trigger" && in != "power" && in != "pulse") {
        return;
    }
    if (n.flowOneshot.busy) {
        return;
    }
    n.flowOneshot.busy = true;
    const std::uint64_t end = nowMs_ + static_cast<std::uint64_t>(n.flowOneshot.pulseMs);
    n.flowOneshot.endAt = end < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : end;
    LogicContext hi = ctx;
    hi.analogSignal = 1.0;
    hi.parameter = 1.0;
    EmitOutput(selfId, "Out", hi);
    EmitOutput(selfId, "Busy", hi);
}

void LogicGraph::HandleTimeDelay(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.timeDelay.enabled) {
        return;
    }
    if (in == "set_ms") {
        const double v = ScalarPulseValue(ctx);
        if (v >= 0.0 && std::isfinite(v)) {
            n.timeDelay.delayMs =
                static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(v), kMaxLogicDelayMs));
        }
        return;
    }
    if (in != "in") {
        return;
    }
    if (n.timeDelay.delayMs == 0) {
        EmitOutput(selfId, "Out", ctx);
        return;
    }
    n.timeDelay.pendingCtx = ctx;
    n.timeDelay.hasPending = true;
    const std::uint64_t end = nowMs_ + static_cast<std::uint64_t>(n.timeDelay.delayMs);
    n.timeDelay.fireAt = end < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : end;
}

void LogicGraph::HandleTimeClock(const std::string& /*selfId*/, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.timeClock.enabled) {
        return;
    }
    if (in == "enable" || in == "turnon") {
        n.timeClock.clockEnabled = true;
        if (n.timeClock.nextTickAt == 0) {
            const std::uint64_t next = nowMs_ + n.timeClock.periodMs;
            n.timeClock.nextTickAt = next < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : next;
        }
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.timeClock.clockEnabled = false;
        n.timeClock.nextTickAt = 0;
        return;
    }
    if (in == "toggle") {
        n.timeClock.clockEnabled = !n.timeClock.clockEnabled;
        if (n.timeClock.clockEnabled && n.timeClock.nextTickAt == 0) {
            const std::uint64_t next = nowMs_ + n.timeClock.periodMs;
            n.timeClock.nextTickAt = next < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : next;
        }
        if (!n.timeClock.clockEnabled) {
            n.timeClock.nextTickAt = 0;
        }
        return;
    }
    if (in == "set_hz") {
        const double hz = std::max(0.001, SanitizeScalar(ScalarPulseValue(ctx)));
        n.timeClock.hz = hz;
        const double period = 1000.0 / hz;
        n.timeClock.periodMs = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(period + 0.5));
        if (n.timeClock.clockEnabled && n.timeClock.nextTickAt == 0) {
            const std::uint64_t next = nowMs_ + n.timeClock.periodMs;
            n.timeClock.nextTickAt = next < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : next;
        }
        return;
    }
}

void LogicGraph::HandleTimeWatch(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.timeWatch.enabled) {
        return;
    }
    auto emitRunMs = [&](bool run, std::uint64_t totalMs) {
        LogicContext r = ctx;
        r.sourceId = selfId;
        r.analogSignal = run ? 1.0 : 0.0;
        r.parameter = r.analogSignal;
        EmitOutput(selfId, "Run", r);
        LogicContext m = ctx;
        m.sourceId = selfId;
        m.analogSignal = static_cast<double>(totalMs);
        m.parameter = m.analogSignal;
        EmitOutput(selfId, "Ms", m);
    };
    if (in == "start") {
        if (!n.timeWatch.running) {
            n.timeWatch.running = true;
            n.timeWatch.lapStartMs = nowMs_;
        }
        std::uint64_t totalMs = n.timeWatch.accumulatedMs;
        if (n.timeWatch.running && nowMs_ >= n.timeWatch.lapStartMs) {
            totalMs += (nowMs_ - n.timeWatch.lapStartMs);
        }
        emitRunMs(true, totalMs);
        return;
    }
    if (in == "stop") {
        if (n.timeWatch.running && nowMs_ >= n.timeWatch.lapStartMs) {
            n.timeWatch.accumulatedMs += (nowMs_ - n.timeWatch.lapStartMs);
        }
        n.timeWatch.running = false;
        n.timeWatch.lapStartMs = 0;
        emitRunMs(false, n.timeWatch.accumulatedMs);
        return;
    }
    if (in == "rst" || in == "reset") {
        n.timeWatch.running = false;
        n.timeWatch.lapStartMs = 0;
        n.timeWatch.accumulatedMs = 0;
        emitRunMs(false, 0);
    }
}

void LogicGraph::HandleMemSample(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.memSample.enabled) {
        return;
    }
    if (in == "hold") {
        n.memSample.holding = PulseInputHigh(ctx);
        return;
    }
    if (in == "sig") {
        n.memSample.live = SanitizeScalar(ScalarPulseValue(ctx));
        if (!n.memSample.holding) {
            n.memSample.latched = n.memSample.live;
            if (!n.memSample.lastEmitted.has_value()
                || !NearlyEqual(*n.memSample.lastEmitted, n.memSample.latched)) {
                n.memSample.lastEmitted = n.memSample.latched;
                LogicContext out = ctx;
                out.analogSignal = n.memSample.latched;
                out.parameter = n.memSample.latched;
                EmitOutput(selfId, "Out", out);
            }
        }
        return;
    }
    if (in == "cap") {
        n.memSample.latched = n.memSample.live;
        n.memSample.lastEmitted = n.memSample.latched;
        LogicContext out = ctx;
        out.analogSignal = n.memSample.latched;
        out.parameter = n.memSample.latched;
        EmitOutput(selfId, "Out", out);
    }
}

void LogicGraph::HandleMemChatter(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.memChatter.enabled) {
        return;
    }
    if (in == "ms") {
        const double v = ScalarPulseValue(ctx);
        if (v >= 0.0 && std::isfinite(v)) {
            n.memChatter.debounceMs =
                static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(v), kMaxLogicDelayMs));
        }
        return;
    }
    if (in == "rst" || in == "reset") {
        n.memChatter.settleAt = 0;
        n.memChatter.rawHi = false;
        n.memChatter.pendingHi = false;
        if (n.memChatter.stableHi) {
            n.memChatter.stableHi = false;
            LogicContext out = ctx;
            out.analogSignal = 0.0;
            out.parameter = 0.0;
            EmitOutput(selfId, "Stable", out);
        }
        return;
    }
    if (in != "sig") {
        return;
    }
    EmitOutput(selfId, "Raw", ctx);
    const bool hi = PulseInputHigh(ctx);
    n.memChatter.rawHi = hi;
    n.memChatter.pendingHi = hi;
    if (n.memChatter.debounceMs == 0) {
        n.memChatter.settleAt = 0;
        if (n.memChatter.stableHi != hi) {
            n.memChatter.stableHi = hi;
            LogicContext out = ctx;
            out.analogSignal = hi ? 1.0 : 0.0;
            out.parameter = out.analogSignal;
            EmitOutput(selfId, "Stable", out);
        }
        return;
    }
    const std::uint32_t ms = std::max<std::uint32_t>(1u, n.memChatter.debounceMs);
    const std::uint64_t next = nowMs_ + static_cast<std::uint64_t>(ms);
    n.memChatter.settleAt = next < nowMs_ ? std::numeric_limits<std::uint64_t>::max() : next;
}

void LogicGraph::HandleFlowDoOnce(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.flowDoOnce.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.flowDoOnce.enabled = false;
        return;
    }
    if (in == "toggle") {
        n.flowDoOnce.enabled = !n.flowDoOnce.enabled;
        return;
    }
    if (in == "reset") {
        n.flowDoOnce.consumed = false;
        return;
    }
    if (in != "trigger") {
        return;
    }
    if (!n.flowDoOnce.enabled || n.flowDoOnce.consumed) {
        return;
    }
    n.flowDoOnce.consumed = true;
    EmitOutput(selfId, "Fired", ctx);
}

void LogicGraph::HandleFlowRelay(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "en" || in == "enable" || in == "turnon") {
        n.flowRelay.enabled = true;
        return;
    }
    if (in == "dis" || in == "disable" || in == "turnoff") {
        n.flowRelay.enabled = false;
        return;
    }
    if (in != "trig" && in != "trigger" && in != "power" && in != "pulse") {
        return;
    }
    if (!n.flowRelay.enabled) {
        return;
    }
    if (!ctx.analogSignal.has_value()) {
        ctx.analogSignal = 1.0;
    }
    EmitOutput(selfId, "Out", ctx);
}

void LogicGraph::HandleIoButton(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.ioButton.enabled = true;
        return;
    }
    if (in == "disable" || in == "turnoff") {
        n.ioButton.enabled = false;
        if (n.ioButton.held) {
            n.ioButton.held = false;
            EmitOutput(selfId, "Release", ctx);
        }
        return;
    }
    if (in == "press") {
        if (!n.ioButton.enabled || n.ioButton.held) {
            return;
        }
        n.ioButton.held = true;
        if (!ctx.analogSignal.has_value()) {
            ctx.analogSignal = 1.0;
        }
        EmitOutput(selfId, "Press", ctx);
        return;
    }
    if (in == "release") {
        if (!n.ioButton.held) {
            return;
        }
        n.ioButton.held = false;
        EmitOutput(selfId, "Release", ctx);
    }
}

void LogicGraph::HandleIoKeypad(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "enable" || in == "turnon") {
        n.ioKeypad.enabled = true;
        return;
    }
    if (in == "reset") {
        n.ioKeypad.buffer.clear();
        {
            LogicContext out = ctx;
            FillIoKeypadContext(out, n.ioKeypad.buffer);
            EmitOutput(selfId, "Val", out);
        }
        return;
    }
    if (in == "enter") {
        if (!n.ioKeypad.enabled) {
            return;
        }
        LogicContext out = ctx;
        FillIoKeypadContext(out, n.ioKeypad.buffer);
        EmitOutput(selfId, "Enter", out);
        return;
    }
    if (in == "digit") {
        if (!n.ioKeypad.enabled) {
            return;
        }
        int digit = -1;
        if (ctx.parameter.has_value()) {
            digit = static_cast<int>(*ctx.parameter);
        } else {
            const auto it = ctx.fields.find("digit");
            if (it != ctx.fields.end() && it->second.size() == 1) {
                const char c = it->second[0];
                if (c >= '0' && c <= '9') {
                    digit = c - '0';
                }
            }
        }
        if (digit < 0 || digit > 9) {
            return;
        }
        if (n.ioKeypad.buffer.size() >= kIoKeypadMaxDigits) {
            return;
        }
        n.ioKeypad.buffer.push_back(static_cast<char>('0' + digit));
        LogicContext out = ctx;
        FillIoKeypadContext(out, n.ioKeypad.buffer);
        EmitOutput(selfId, "Val", out);
        return;
    }
    if (in.size() == 1 && in[0] >= '0' && in[0] <= '9') {
        if (!n.ioKeypad.enabled) {
            return;
        }
        if (n.ioKeypad.buffer.size() >= kIoKeypadMaxDigits) {
            return;
        }
        n.ioKeypad.buffer.push_back(in[0]);
        LogicContext out = ctx;
        FillIoKeypadContext(out, n.ioKeypad.buffer);
        EmitOutput(selfId, "Val", out);
    }
}

void LogicGraph::HandleIoDisplay(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.ioDisplay.enabled) {
        return;
    }
    if (in == "settext") {
        std::string next;
        if (const std::optional<std::string> fromFields = ResolveContextStringField(ctx, {"text", "value"})) {
            next = *fromFields;
        } else if (ctx.parameter.has_value()) {
            next = std::to_string(*ctx.parameter);
        } else {
            next.clear();
        }
        n.ioDisplay.text = std::move(next);
    } else if (in == "setcolor") {
        if (const std::optional<std::string> c = ResolveContextStringField(ctx, {"color", "value"})) {
            n.ioDisplay.color = *c;
        } else {
            n.ioDisplay.color.clear();
        }
    } else {
        return;
    }
    LogicContext out = ctx;
    out.fields["text"] = n.ioDisplay.text;
    out.fields["color"] = n.ioDisplay.color;
    EmitOutput(selfId, "Done", out);
}

void LogicGraph::HandleIoAudio(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.ioAudio.enabled) {
        return;
    }
    if (in == "play") {
        n.ioAudio.playing = true;
        if (const std::optional<std::string> c = ResolveContextStringField(ctx, {"clip", "cue", "asset", "path", "value"})) {
            n.ioAudio.lastClip = *c;
        } else {
            n.ioAudio.lastClip.clear();
        }
    } else if (in == "stop") {
        n.ioAudio.playing = false;
    } else if (in == "setvol") {
        n.ioAudio.volume = ParseVolumeInput(ctx, n.ioAudio.volume);
    } else {
        return;
    }
    LogicContext out = ctx;
    FillIoAudioDonePayload(out, n.ioAudio.playing, n.ioAudio.volume, n.ioAudio.lastClip);
    EmitOutput(selfId, "Done", out);
}

void LogicGraph::HandleIoLogger(const std::string& /*selfId*/, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (!n.ioLogger.enabled) {
        return;
    }
    if (in != "log" && in != "warn" && in != "err") {
        return;
    }
    n.ioLogger.lastLevel = std::string(in);
    n.ioLogger.lastMessage = ResolveLogMessage(ctx);
    ++n.ioLogger.lineCount;
}

void LogicGraph::HandleIoTrigger(const std::string& selfId, NodeSlot& n, std::string_view in, LogicContext& ctx) {
    if (in == "arm") {
        n.ioTrigger.armed = true;
        return;
    }
    if (in == "disarm") {
        n.ioTrigger.armed = false;
        if (n.ioTrigger.held) {
            n.ioTrigger.held = false;
            EmitOutput(selfId, "Untouch", ctx);
        }
        return;
    }
    if (in == "touch") {
        if (!n.ioTrigger.armed || n.ioTrigger.held) {
            return;
        }
        n.ioTrigger.held = true;
        if (!ctx.analogSignal.has_value()) {
            ctx.analogSignal = 1.0;
        }
        EmitOutput(selfId, "Touch", ctx);
        return;
    }
    if (in == "untouch") {
        if (!n.ioTrigger.held) {
            return;
        }
        n.ioTrigger.held = false;
        EmitOutput(selfId, "Untouch", ctx);
    }
}

} // namespace ri::logic
