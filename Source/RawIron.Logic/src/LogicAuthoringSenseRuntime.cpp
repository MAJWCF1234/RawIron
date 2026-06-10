#include "RawIron/Logic/LogicAuthoringSenseRuntime.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>

namespace ri::logic {
namespace {

[[nodiscard]] std::string NormalizeInputName(std::string_view inputName) {
    std::string out;
    out.reserve(inputName.size());
    for (char ch : inputName) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

[[nodiscard]] bool PulseContextHigh(const LogicContext& ctx) {
    const double level = ctx.analogSignal.has_value() ? *ctx.analogSignal
                         : ctx.parameter.has_value()   ? *ctx.parameter
                                                       : 1.0;
    return level > 0.5;
}

[[nodiscard]] bool UpdateEdgeBool(LogicAuthoringSenseRuntimeState& state,
                                  const std::string& key,
                                  const bool value) {
    bool& previous = state.edgeBoolByKey[key];
    if (previous == value) {
        return false;
    }
    previous = value;
    return true;
}

[[nodiscard]] float Distance3(const std::array<float, 3>& a, const std::array<float, 3>& b) {
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

[[nodiscard]] bool IsSenseKitEnabled(const LogicAuthoringSenseRuntimeState& state, const std::string& logicNodeId) {
    const auto it = state.enabledByLogicNodeId.find(logicNodeId);
    return it == state.enabledByLogicNodeId.end() || it->second;
}

[[nodiscard]] std::string NormalizeToken(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

[[nodiscard]] std::string ExtractContextString(const LogicContext& ctx, const std::string_view defaultValue) {
    auto pick = [&](const std::string_view key) -> std::optional<std::string> {
        const auto it = ctx.fields.find(std::string(key));
        if (it != ctx.fields.end() && !it->second.empty()) {
            return it->second;
        }
        return std::nullopt;
    };
    if (const std::optional<std::string> tag = pick("tag")) {
        return *tag;
    }
    if (const std::optional<std::string> key = pick("key")) {
        return *key;
    }
    if (const std::optional<std::string> value = pick("value")) {
        return *value;
    }
    return std::string(defaultValue);
}

[[nodiscard]] bool PollGateOpen(LogicAuthoringSenseRuntimeState& state, const std::string& logicNodeId) {
    const auto modeIt = state.pollModeByLogicNodeId.find(logicNodeId);
    if (modeIt == state.pollModeByLogicNodeId.end() || !modeIt->second) {
        return true;
    }
    const auto pendingIt = state.pollPendingByLogicNodeId.find(logicNodeId);
    if (pendingIt == state.pollPendingByLogicNodeId.end() || !pendingIt->second) {
        return false;
    }
    pendingIt->second = false;
    return true;
}

[[nodiscard]] double ResolveScalarValue(const std::string& key,
                                          const LogicAuthoringSenseProbeRecord& record,
                                          const std::array<float, 3>& probeWorldPosition,
                                          const float distance) {
    const std::string normalized = NormalizeToken(key.empty() ? "distance" : key);
    if (normalized == "distance" || normalized == "dist" || normalized == "prox" || normalized == "proximity") {
        return static_cast<double>(distance);
    }
    if (normalized == "x" || normalized == "dx") {
        return static_cast<double>(probeWorldPosition[0] - record.position[0]);
    }
    if (normalized == "y" || normalized == "dy") {
        return static_cast<double>(probeWorldPosition[1] - record.position[1]);
    }
    if (normalized == "z" || normalized == "dz") {
        return static_cast<double>(probeWorldPosition[2] - record.position[2]);
    }
    return 0.0;
}

[[nodiscard]] bool TagMatchesProbe(const LogicAuthoringSenseRuntimeState& state,
                                   const std::string& logicNodeId,
                                   const LogicAuthoringSenseRuntimeOptions* options) {
    std::string requiredTag = "player";
    if (const auto it = state.tagFilterByLogicNodeId.find(logicNodeId); it != state.tagFilterByLogicNodeId.end()) {
        requiredTag = it->second;
    }
    const std::string probeTag =
        options != nullptr && !options->probeInstigatorTag.empty() ? std::string(options->probeInstigatorTag)
                                                                   : std::string("player");
    return NormalizeToken(requiredTag) == NormalizeToken(probeTag);
}

[[nodiscard]] LogicContext MakeSenseContext(const std::string& logicNodeId) {
    LogicContext ctx{};
    ctx.instigatorId = "logic_sense_probe";
    ctx.sourceId = logicNodeId;
    ctx.fields["instigatorKind"] = "player";
    return ctx;
}

void EmitSenseOutput(LogicGraph& graph,
                     const std::string& logicNodeId,
                     const std::string_view outputName,
                     LogicContext ctx) {
    graph.EmitWorldOutput(logicNodeId, outputName, std::move(ctx));
}

[[nodiscard]] bool ShouldEmitInterval(LogicAuthoringSenseRuntimeState& state,
                                      const std::string& key,
                                      const std::uint64_t nowMs,
                                      const std::uint64_t intervalMs) {
    const auto lastIt = state.lastEmitMsByKey.find(key);
    if (lastIt != state.lastEmitMsByKey.end() && nowMs < lastIt->second + intervalMs) {
        return false;
    }
    state.lastEmitMsByKey[key] = nowMs;
    return true;
}

[[nodiscard]] float UnitNoise(const std::string& logicNodeId, const std::uint64_t nowMs) {
    std::uint64_t hash = nowMs ^ 0x9E3779B97F4A7C15ULL;
    for (const char ch : logicNodeId) {
        hash = (hash * 131ULL) ^ static_cast<std::uint64_t>(static_cast<unsigned char>(ch));
    }
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdULL;
    hash ^= hash >> 33;
    return static_cast<float>(hash % 10000ULL) / 10000.0f;
}

void EmitAnalogSenseOutput(LogicGraph& graph,
                           const std::string& logicNodeId,
                           const std::string_view outputName,
                           const double value) {
    LogicContext ctx = MakeSenseContext(logicNodeId);
    ctx.parameter = value;
    ctx.analogSignal = value;
    EmitSenseOutput(graph, logicNodeId, outputName, std::move(ctx));
}

[[nodiscard]] bool HasLineOfSightToProbe(const LogicAuthoringSenseProbeRecord& record,
                                           const std::array<float, 3>& probeWorldPosition,
                                           const float probeDistance,
                                           const LogicAuthoringSenseRuntimeOptions* options) {
    if (options == nullptr || !options->raycast) {
        return true;
    }
    constexpr float kSensorHeight = 1.2f;
    const std::array<float, 3> origin{
        record.position[0],
        record.position[1] + kSensorHeight,
        record.position[2],
    };
    const float dx = probeWorldPosition[0] - origin[0];
    const float dy = probeWorldPosition[1] - origin[1];
    const float dz = probeWorldPosition[2] - origin[2];
    const float span = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (span < 0.05f) {
        return true;
    }
    LogicSenseRaycastRequest request{};
    request.origin = origin;
    request.direction = {dx / span, dy / span, dz / span};
    request.maxDistance = span;
    const std::optional<LogicSenseRaycastHit> obstruction = options->raycast(request);
    if (!obstruction.has_value()) {
        return true;
    }
    return obstruction->distance >= probeDistance - options->lineOfSightClearance;
}

void ApplySenseKitInput(LogicAuthoringSenseRuntimeState& state,
                        const std::string& kitId,
                        const std::string& logicNodeId,
                        const std::string_view inputName,
                        const LogicContext& ctx) {
    const std::string normalized = NormalizeInputName(inputName);
    const bool high = PulseContextHigh(ctx);
    if (kitId == "sense_zone") {
        if (normalized == "arm" || normalized == "enable" || normalized == "en") {
            if (high) {
                state.enabledByLogicNodeId[logicNodeId] = true;
            }
        } else if (normalized == "clr" || normalized == "clear" || normalized == "disable") {
            if (high) {
                state.enabledByLogicNodeId[logicNodeId] = false;
            }
        }
        return;
    }
    if (kitId == "sense_overlap" || kitId == "sense_line" || kitId == "sense_ray") {
        if (normalized == "en" || normalized == "enable" || normalized == "arm") {
            if (high) {
                state.enabledByLogicNodeId[logicNodeId] = true;
            } else {
                state.enabledByLogicNodeId[logicNodeId] = false;
            }
        } else if (normalized == "rst" || normalized == "reset" || normalized == "clr" || normalized == "disable") {
            if (high) {
                state.enabledByLogicNodeId[logicNodeId] = false;
            }
        }
        return;
    }
    if (kitId == "sense_tag") {
        if (normalized == "tag") {
            state.tagFilterByLogicNodeId[logicNodeId] = ExtractContextString(ctx, "player");
        } else if (normalized == "poll") {
            state.pollModeByLogicNodeId[logicNodeId] = true;
            if (high) {
                state.pollPendingByLogicNodeId[logicNodeId] = true;
            }
        }
        return;
    }
    if (kitId == "sense_scalar") {
        if (normalized == "key") {
            state.scalarKeyByLogicNodeId[logicNodeId] = ExtractContextString(ctx, "distance");
        } else if (normalized == "poll") {
            state.pollModeByLogicNodeId[logicNodeId] = true;
            if (high) {
                state.pollPendingByLogicNodeId[logicNodeId] = true;
            }
        }
    }
}

} // namespace

void BindLogicSenseInputDispatchHandler(
    LogicGraph& graph,
    LogicAuthoringSenseRuntimeState& state,
    const std::unordered_map<std::string, std::string>& kitIdByLogicNodeId) {
    graph.SetInputDispatchHandler([&state, kitIdByLogicNodeId](const std::string_view targetId,
                                                               const std::string_view inputName,
                                                               const LogicContext& context) {
        const auto kitIt = kitIdByLogicNodeId.find(std::string(targetId));
        if (kitIt == kitIdByLogicNodeId.end()) {
            return;
        }
        if (kitIt->second.rfind("sense_", 0) != 0 || kitIt->second == "sense_tick") {
            return;
        }
        ApplySenseKitInput(state, kitIt->second, kitIt->first, inputName, context);
    });
}

void TickLogicAuthoringSenseNodes(LogicGraph& graph,
                                  const std::vector<LogicAuthoringSenseProbeRecord>& probes,
                                  const std::array<float, 3>& probeWorldPosition,
                                  LogicAuthoringSenseRuntimeState& state,
                                  const LogicAuthoringSenseRuntimeOptions* options) {
    const std::uint64_t nowMs = graph.NowMs();
    for (const LogicAuthoringSenseProbeRecord& record : probes) {
        if (!IsSenseKitEnabled(state, record.logicNodeId)) {
            continue;
        }
        const float distance = Distance3(record.position, probeWorldPosition);
        if (record.kitId == "sense_prox") {
            constexpr float kRadius = 8.0f;
            const bool inside = distance <= kRadius;
            if (!UpdateEdgeBool(state, record.logicNodeId + ":prox", inside)) {
                continue;
            }
            EmitSenseOutput(graph, record.logicNodeId, inside ? "Near" : "Far", MakeSenseContext(record.logicNodeId));
            continue;
        }
        if (record.kitId == "sense_zone") {
            constexpr float kRadius = 12.0f;
            const bool inside = distance <= kRadius;
            if (UpdateEdgeBool(state, record.logicNodeId + ":inside", inside) && inside) {
                EmitSenseOutput(graph, record.logicNodeId, "Inside", MakeSenseContext(record.logicNodeId));
                EmitAnalogSenseOutput(graph, record.logicNodeId, "Count", 1.0);
            }
            continue;
        }
        if (record.kitId == "sense_overlap") {
            constexpr float kRadius = 6.0f;
            const bool inside = distance <= kRadius;
            if (!UpdateEdgeBool(state, record.logicNodeId + ":overlap", inside)) {
                continue;
            }
            EmitSenseOutput(
                graph, record.logicNodeId, inside ? "Enter" : "Exit", MakeSenseContext(record.logicNodeId));
            continue;
        }
        if (record.kitId == "sense_line") {
            constexpr float kRadius = 10.0f;
            const bool hit = distance <= kRadius;
            if (UpdateEdgeBool(state, record.logicNodeId + ":line", hit) && hit) {
                EmitSenseOutput(graph, record.logicNodeId, "Hit", MakeSenseContext(record.logicNodeId));
            }
            continue;
        }
        if (record.kitId == "sense_ray") {
            constexpr float kLength = 18.0f;
            const bool inRange = distance <= kLength;
            const bool losClear = inRange && HasLineOfSightToProbe(record, probeWorldPosition, distance, options);
            const bool hit = inRange && losClear;
            if (!UpdateEdgeBool(state, record.logicNodeId + ":ray", hit)) {
                continue;
            }
            EmitSenseOutput(graph, record.logicNodeId, hit ? "Hit" : "Miss", MakeSenseContext(record.logicNodeId));
            continue;
        }
        if (record.kitId == "sense_scalar") {
            constexpr float kRadius = 10.0f;
            if (distance > kRadius) {
                continue;
            }
            if (!PollGateOpen(state, record.logicNodeId)) {
                continue;
            }
            const auto modeIt = state.pollModeByLogicNodeId.find(record.logicNodeId);
            const bool pollMode = modeIt != state.pollModeByLogicNodeId.end() && modeIt->second;
            if (!pollMode && !ShouldEmitInterval(state, record.logicNodeId + ":scalar", nowMs, 400U)) {
                continue;
            }
            std::string key = "distance";
            if (const auto keyIt = state.scalarKeyByLogicNodeId.find(record.logicNodeId);
                keyIt != state.scalarKeyByLogicNodeId.end()) {
                key = keyIt->second;
            }
            const double value = ResolveScalarValue(key, record, probeWorldPosition, distance);
            EmitSenseOutput(graph, record.logicNodeId, "Ok", MakeSenseContext(record.logicNodeId));
            EmitAnalogSenseOutput(graph, record.logicNodeId, "Val", value);
            continue;
        }
        if (record.kitId == "sense_axis") {
            constexpr float kRadius = 15.0f;
            if (distance > kRadius) {
                continue;
            }
            if (!ShouldEmitInterval(state, record.logicNodeId + ":axis", nowMs, 500U)) {
                continue;
            }
            const double dx = static_cast<double>(probeWorldPosition[0] - record.position[0]);
            const double dy = static_cast<double>(probeWorldPosition[1] - record.position[1]);
            const double dz = static_cast<double>(probeWorldPosition[2] - record.position[2]);
            EmitAnalogSenseOutput(graph, record.logicNodeId, "X", dx);
            EmitAnalogSenseOutput(graph, record.logicNodeId, "Y", dy);
            EmitAnalogSenseOutput(graph, record.logicNodeId, "Z", dz);
            continue;
        }
        if (record.kitId == "sense_tag") {
            constexpr float kRadius = 12.0f;
            const bool inRange = distance <= kRadius;
            const bool tagMatches = TagMatchesProbe(state, record.logicNodeId, options);
            const bool seen = inRange && tagMatches;
            if (!PollGateOpen(state, record.logicNodeId)) {
                continue;
            }
            const auto modeIt = state.pollModeByLogicNodeId.find(record.logicNodeId);
            const bool pollMode = modeIt != state.pollModeByLogicNodeId.end() && modeIt->second;
            if (pollMode) {
                if (!seen) {
                    continue;
                }
                LogicContext seenCtx = MakeSenseContext(record.logicNodeId);
                if (const auto tagIt = state.tagFilterByLogicNodeId.find(record.logicNodeId);
                    tagIt != state.tagFilterByLogicNodeId.end()) {
                    seenCtx.fields["tag"] = tagIt->second;
                } else {
                    seenCtx.fields["tag"] = "player";
                }
                EmitSenseOutput(graph, record.logicNodeId, "Seen", std::move(seenCtx));
                EmitAnalogSenseOutput(graph, record.logicNodeId, "Cnt", 1.0);
                continue;
            }
            if (UpdateEdgeBool(state, record.logicNodeId + ":tag", seen) && seen) {
                LogicContext seenCtx = MakeSenseContext(record.logicNodeId);
                if (const auto tagIt = state.tagFilterByLogicNodeId.find(record.logicNodeId);
                    tagIt != state.tagFilterByLogicNodeId.end()) {
                    seenCtx.fields["tag"] = tagIt->second;
                } else {
                    seenCtx.fields["tag"] = "player";
                }
                EmitSenseOutput(graph, record.logicNodeId, "Seen", std::move(seenCtx));
                EmitAnalogSenseOutput(graph, record.logicNodeId, "Cnt", 1.0);
            }
            continue;
        }
        if (record.kitId == "sense_noise") {
            constexpr float kRadius = 15.0f;
            if (distance > kRadius) {
                continue;
            }
            if (!ShouldEmitInterval(state, record.logicNodeId + ":noise", nowMs, 2000U)) {
                continue;
            }
            const float unit = UnitNoise(record.logicNodeId, nowMs);
            EmitAnalogSenseOutput(graph, record.logicNodeId, "Val", static_cast<double>(unit));
            EmitAnalogSenseOutput(graph, record.logicNodeId, "Norm", static_cast<double>(unit));
        }
    }
}

void TickLogicAuthoringSenseNodes(LogicGraph& graph,
                                  const LogicAuthoringEditorFile& file,
                                  const std::array<float, 3>& probeWorldPosition,
                                  LogicAuthoringSenseRuntimeState& state,
                                  const LogicAuthoringSenseRuntimeOptions* options) {
    std::vector<LogicAuthoringSenseProbeRecord> probes{};
    probes.reserve(file.nodes.size());
    for (const LogicAuthoringEditorNodeRecord& record : file.nodes) {
        if (record.kitId.rfind("sense_", 0) != 0 || record.kitId == "sense_tick") {
            continue;
        }
        LogicAuthoringSenseProbeRecord probe{};
        probe.logicNodeId = record.logicNodeId;
        probe.kitId = record.kitId;
        probe.position = record.position;
        probes.push_back(std::move(probe));
    }
    TickLogicAuthoringSenseNodes(graph, probes, probeWorldPosition, state, options);
}

} // namespace ri::logic
