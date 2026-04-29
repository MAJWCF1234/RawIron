#include "RawIron/Logic/LogicAuthoring.h"
#include "RawIron/Logic/LogicPortSchema.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <type_traits>
#include <unordered_set>

namespace ri::logic {
namespace {

[[nodiscard]] std::string NodeDefinitionId(const LogicNodeDefinition& definition) {
    return std::visit([](const auto& node) { return node.id; }, definition);
}

[[nodiscard]] bool IsDefaultPlacement(const LogicNodePlacement& placement) {
    const bool defaultPosition = placement.position == std::array<float, 3>{0.0f, 0.0f, 0.0f};
    const bool defaultRotation = placement.rotationDegrees == std::array<float, 3>{0.0f, 0.0f, 0.0f};
    const bool defaultScale = placement.scale == std::array<float, 3>{1.0f, 1.0f, 1.0f};
    const bool defaultLayer = placement.layer == "logic";
    return defaultPosition && defaultRotation && defaultScale && defaultLayer;
}

[[nodiscard]] bool IsNodeInAutoCluster(const std::unordered_set<std::string>& autoNodeIds, std::string_view nodeId) {
    return autoNodeIds.contains(std::string(nodeId));
}

[[nodiscard]] std::array<float, 3> ComputeIoLaneAnchor(const LogicAutoLayoutOptions& options,
                                                       const std::array<float, 3>& endpoint) {
    const float roomMinX = options.roomCenter[0] - options.roomHalfExtents[0];
    const float laneX = roomMinX - std::abs(options.ioLaneOffset[0]);
    return std::array<float, 3>{laneX, endpoint[1], endpoint[2]};
}

[[nodiscard]] std::array<float, 3> ResolveEndpointPosition(
    std::string_view endpointId,
    const std::unordered_map<std::string, std::array<float, 3>>& nodePositions,
    const LogicAutoLayoutOptions& options) {
    const auto nodeIt = nodePositions.find(std::string(endpointId));
    if (nodeIt != nodePositions.end()) {
        return nodeIt->second;
    }
    const auto worldIt = options.worldEndpointPositions.find(std::string(endpointId));
    if (worldIt != options.worldEndpointPositions.end()) {
        return worldIt->second;
    }
    return options.roomCenter;
}

[[nodiscard]] std::string ToLower(std::string_view text) {
    std::string normalized(text);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return normalized;
}

struct PortResolution {
    bool recognized = false;
    bool normalized = false;
    std::string canonical;
};

[[nodiscard]] std::uint32_t ClampRouteDelayMs(const std::uint32_t authoredDelayMs) {
    const std::uint64_t clamped = std::min<std::uint64_t>(authoredDelayMs, kMaxLogicDelayMs);
    return static_cast<std::uint32_t>(clamped);
}

[[nodiscard]] std::uint64_t ClampTimerIntervalMs(const std::uint64_t intervalMs) {
    return std::min<std::uint64_t>(intervalMs, kMaxLogicDelayMs);
}

[[nodiscard]] double ClampCounterStep(const double step) {
    constexpr double kMinStep = 1.0;
    constexpr double kMaxStep = 1'000'000.0;
    const double magnitude = std::max(kMinStep, std::min(kMaxStep, std::abs(step)));
    return magnitude;
}

[[nodiscard]] std::uint64_t ClampDurationMs(const std::uint64_t durationMs) {
    return std::min<std::uint64_t>(durationMs, kMaxLogicDelayMs);
}

[[nodiscard]] PortResolution ResolvePortName(const LogicNodePortSchema& schema,
                                             std::string_view rawPortName,
                                             const bool inputPort) {
    PortResolution resolution{};
    resolution.canonical = std::string(rawPortName);
    if (rawPortName.empty()) {
        return resolution;
    }

    const std::vector<LogicPortDescriptor>& ports = inputPort ? schema.inputs : schema.outputs;
    const std::string lowered = ToLower(rawPortName);
    for (const LogicPortDescriptor& port : ports) {
        if (ToLower(port.name) == lowered) {
            resolution.recognized = true;
            resolution.canonical = port.name;
            resolution.normalized = (resolution.canonical != rawPortName);
            return resolution;
        }
    }

    static const std::unordered_map<std::string, std::string> kAliases{
        {"turnon", "Enable"},
        {"turnoff", "Disable"},
        {"power", "Trigger"},
        {"start", "Start"},
        {"stop", "Stop"},
        {"cancel", "Cancel"},
        {"canceltimer", "CancelTimer"},
        {"add", "Add"},
        {"subtract", "Subtract"},
        {"increment", "Increment"},
        {"decrement", "Decrement"},
        {"set", "Set"},
        {"setvalue", "SetValue"},
        {"evaluate", "Evaluate"},
        {"compare", "Compare"},
        {"advance", "Advance"},
        {"send", "Send"},
        {"onstepped", "OnStay"},
        {"onstarttouch", "OnStartTouch"},
        {"onendtouch", "OnEndTouch"},
    };

    const auto alias = kAliases.find(lowered);
    if (alias == kAliases.end()) {
        return resolution;
    }
    const std::string aliasLowered = ToLower(alias->second);
    for (const LogicPortDescriptor& port : ports) {
        if (ToLower(port.name) == aliasLowered) {
            resolution.recognized = true;
            resolution.canonical = port.name;
            resolution.normalized = true;
            return resolution;
        }
    }
    return resolution;
}

void AddIssue(std::vector<LogicAuthoringCompileIssue>& issues,
              const LogicAuthoringIssueSeverity severity,
              std::string code,
              std::string message,
              std::string subjectId = {}) {
    issues.push_back(LogicAuthoringCompileIssue{
        .severity = severity,
        .code = std::move(code),
        .message = std::move(message),
        .subjectId = std::move(subjectId),
    });
}

[[nodiscard]] bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
}

[[nodiscard]] LogicAuthoringCompileSummary BuildCompileSummary(
    const std::vector<LogicAuthoringCompileIssue>& issues) {
    LogicAuthoringCompileSummary summary{};
    for (const LogicAuthoringCompileIssue& issue : issues) {
        if (issue.severity == LogicAuthoringIssueSeverity::Error) {
            ++summary.errorCount;
        } else {
            ++summary.warningCount;
        }

        if (IsLogicAuthoringNormalizationIssue(issue.code)) {
            ++summary.normalizedPortCount;
        }
        if (GetLogicAuthoringIssueCategory(issue.code) == LogicAuthoringIssueCategory::Node &&
            IsLogicAuthoringNormalizationIssue(issue.code)) {
            ++summary.normalizedNodeCount;
        }
        if (EndsWith(issue.code, "_delay_clamped")) {
            ++summary.clampedDelayCount;
        }
        if (issue.code.find("world_actor") != std::string::npos && EndsWith(issue.code, "_assumed")) {
            ++summary.assumedWorldActorEndpointCount;
        }
        if (IsLogicAuthoringPortIssue(issue.code) && EndsWith(issue.code, "_unknown")) {
            ++summary.unknownPortCount;
        }
    }
    return summary;
}

[[nodiscard]] LogicNodeDefinition NormalizeNodeDefinition(const LogicNodeDefinition& definition,
                                                          std::vector<LogicAuthoringCompileIssue>& issues) {
    return std::visit(
        [&](const auto& node) -> LogicNodeDefinition {
            using NodeT = std::decay_t<decltype(node)>;
            NodeT normalized = node;

            if constexpr (std::is_same_v<NodeT, TimerNode>) {
                const std::uint64_t cappedInterval = ClampTimerIntervalMs(normalized.def.intervalMs);
                if (cappedInterval != normalized.def.intervalMs) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.timer_interval_clamped",
                             "Timer interval exceeded maximum and was clamped.",
                             normalized.id);
                    normalized.def.intervalMs = cappedInterval;
                }
                if (normalized.def.repeating && normalized.def.intervalMs == 0) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.timer_repeat_interval_adjusted",
                             "Repeating timer interval must be at least 1 ms; adjusted from 0 to 1.",
                             normalized.id);
                    normalized.def.intervalMs = 1;
                }
            } else if constexpr (std::is_same_v<NodeT, CounterNode>) {
                if (normalized.def.minValue.has_value() && normalized.def.maxValue.has_value() &&
                    *normalized.def.minValue > *normalized.def.maxValue) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.counter_minmax_swapped",
                             "Counter minValue was greater than maxValue; values were swapped.",
                             normalized.id);
                    std::swap(normalized.def.minValue, normalized.def.maxValue);
                }
                const double clampedStep = ClampCounterStep(normalized.def.step);
                if (clampedStep != normalized.def.step) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.counter_step_clamped",
                             "Counter step was out of range and was clamped to [1, 1000000].",
                             normalized.id);
                    normalized.def.step = clampedStep;
                }
            } else if constexpr (std::is_same_v<NodeT, SequencerNode>) {
                constexpr int kMinStepCount = 1;
                const int clampedStepCount = std::max(kMinStepCount, normalized.def.stepCount);
                if (clampedStepCount != normalized.def.stepCount) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.sequencer_step_count_clamped",
                             "Sequencer stepCount must be at least 1; value was clamped.",
                             normalized.id);
                    normalized.def.stepCount = clampedStepCount;
                }
            } else if constexpr (std::is_same_v<NodeT, CompareNode>) {
                if (normalized.def.minValue.has_value() && normalized.def.maxValue.has_value() &&
                    *normalized.def.minValue > *normalized.def.maxValue) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.compare_minmax_swapped",
                             "Compare minValue was greater than maxValue; values were swapped.",
                             normalized.id);
                    std::swap(normalized.def.minValue, normalized.def.maxValue);
                }
                if (normalized.def.equalsValue.has_value() &&
                    (normalized.def.minValue.has_value() || normalized.def.maxValue.has_value())) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.compare_equals_with_bounds",
                             "Compare node has equalsValue and range bounds; runtime evaluation may be ambiguous.",
                             normalized.id);
                }
                if (normalized.def.sourceLogicEntityId.has_value() && normalized.def.sourceWorldValueKey.has_value()) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.compare_multiple_sources",
                             "Compare node defines both sourceLogicEntityId and sourceWorldValue; source precedence should be reviewed.",
                             normalized.id);
                }
                if (!normalized.def.sourceLogicEntityId.has_value() && !normalized.def.sourceWorldValueKey.has_value() &&
                    !normalized.def.constantValue.has_value() && !normalized.def.constantText.has_value()) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.compare_no_observed_source",
                             "Compare node has no external source or constant observed value configured.",
                             normalized.id);
                }
            } else if constexpr (std::is_same_v<NodeT, PulseNode>) {
                const std::uint64_t clampedHoldMs = ClampDurationMs(normalized.def.holdMs);
                if (clampedHoldMs != normalized.def.holdMs) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.pulse_hold_clamped",
                             "Pulse holdMs exceeded maximum and was clamped.",
                             normalized.id);
                    normalized.def.holdMs = clampedHoldMs;
                }
            } else if constexpr (std::is_same_v<NodeT, ChannelNode>) {
                if (normalized.def.channelName.empty()) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.channel_name_defaulted",
                             "Channel node had empty channelName; defaulted to \"default\".",
                             normalized.id);
                    normalized.def.channelName = "default";
                }
            } else if constexpr (std::is_same_v<NodeT, MergeNode>) {
                constexpr int kMinExpectedInputs = 1;
                const int clampedExpected = std::max(kMinExpectedInputs, normalized.def.expectedInputs);
                if (clampedExpected != normalized.def.expectedInputs) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.merge_expected_inputs_clamped",
                             "Merge expectedInputs must be at least 1; value was clamped.",
                             normalized.id);
                    normalized.def.expectedInputs = clampedExpected;
                }
            } else if constexpr (std::is_same_v<NodeT, SplitNode>) {
                constexpr int kMinBranchCount = 1;
                const int clampedBranches = std::max(kMinBranchCount, normalized.def.branchCount);
                if (clampedBranches != normalized.def.branchCount) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.split_branch_count_clamped",
                             "Split branchCount must be at least 1; value was clamped.",
                             normalized.id);
                    normalized.def.branchCount = clampedBranches;
                }

                if (normalized.def.branchScales.size() > static_cast<std::size_t>(normalized.def.branchCount)) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.split_branch_scales_trimmed",
                             "Split branchScales had extra entries beyond branchCount; extras were trimmed.",
                             normalized.id);
                    normalized.def.branchScales.resize(static_cast<std::size_t>(normalized.def.branchCount));
                }
                if (normalized.def.branchIntrinsicDelayMs.size() > static_cast<std::size_t>(normalized.def.branchCount)) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.split_branch_delays_trimmed",
                             "Split branchIntrinsicDelayMs had extra entries beyond branchCount; extras were trimmed.",
                             normalized.id);
                    normalized.def.branchIntrinsicDelayMs.resize(static_cast<std::size_t>(normalized.def.branchCount));
                }
                for (std::uint32_t& delayMs : normalized.def.branchIntrinsicDelayMs) {
                    const std::uint32_t clampedDelay = ClampRouteDelayMs(delayMs);
                    if (clampedDelay != delayMs) {
                        AddIssue(issues,
                                 LogicAuthoringIssueSeverity::Warning,
                                 "node.split_branch_delay_clamped",
                                 "Split branch intrinsic delay exceeded maximum and was clamped.",
                                 normalized.id);
                        delayMs = clampedDelay;
                    }
                }
            } else if constexpr (std::is_same_v<NodeT, InventoryGateNode>) {
                constexpr int kMinQuantity = 1;
                const int clampedQuantity = std::max(kMinQuantity, normalized.def.quantity);
                if (clampedQuantity != normalized.def.quantity) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.inventory_quantity_clamped",
                             "Inventory gate quantity must be at least 1; value was clamped.",
                             normalized.id);
                    normalized.def.quantity = clampedQuantity;
                }
                if (normalized.def.itemId.empty()) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.inventory_item_missing",
                             "Inventory gate has empty itemId and will not match useful inventory keys.",
                             normalized.id);
                }
            } else if constexpr (std::is_same_v<NodeT, PredicateNode>) {
                if (normalized.def.rules.empty()) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.predicate_rules_empty",
                             "Predicate has no rules; it will behave as an unconditional pass-through.",
                             normalized.id);
                }
            } else if constexpr (std::is_same_v<NodeT, TriggerDetectorNode>) {
                const std::uint64_t clampedCooldown = ClampDurationMs(normalized.def.cooldownMs);
                if (clampedCooldown != normalized.def.cooldownMs) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.trigger_detector_cooldown_clamped",
                             "Trigger detector cooldown exceeded maximum and was clamped.",
                             normalized.id);
                    normalized.def.cooldownMs = clampedCooldown;
                }
                if (normalized.def.instigatorFilter == TriggerInstigatorFilter::Tag && normalized.def.instigatorTag.empty()) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.trigger_detector_tag_missing",
                             "Trigger detector uses tag filter but instigatorTag is empty; filter was reset to Any.",
                             normalized.id);
                    normalized.def.instigatorFilter = TriggerInstigatorFilter::Any;
                } else if (normalized.def.instigatorFilter != TriggerInstigatorFilter::Tag &&
                           !normalized.def.instigatorTag.empty()) {
                    AddIssue(issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "node.trigger_detector_tag_ignored",
                             "Trigger detector instigatorTag is set but filter is not Tag; tag value will be ignored.",
                             normalized.id);
                }
            }

            return normalized;
        },
        definition);
}

} // namespace

LogicAuthoringIssueCategory GetLogicAuthoringIssueCategory(std::string_view issueCode) {
    if (issueCode.rfind("node.", 0) == 0) {
        return LogicAuthoringIssueCategory::Node;
    }
    if (issueCode.rfind("wire.", 0) == 0) {
        if (issueCode.find("world_actor") != std::string::npos) {
            return LogicAuthoringIssueCategory::WorldActor;
        }
        if (issueCode.find("_input_") != std::string::npos || issueCode.find("_output_") != std::string::npos) {
            return LogicAuthoringIssueCategory::Port;
        }
        return LogicAuthoringIssueCategory::Wire;
    }
    return LogicAuthoringIssueCategory::General;
}

std::string_view GetLogicAuthoringIssueCategoryName(const LogicAuthoringIssueCategory category) {
    switch (category) {
    case LogicAuthoringIssueCategory::General:
        return "General";
    case LogicAuthoringIssueCategory::Node:
        return "Node";
    case LogicAuthoringIssueCategory::Wire:
        return "Wire";
    case LogicAuthoringIssueCategory::Port:
        return "Port";
    case LogicAuthoringIssueCategory::WorldActor:
        return "WorldActor";
    }
    return "General";
}

std::string_view GetLogicAuthoringIssueSeverityName(const LogicAuthoringIssueSeverity severity) {
    switch (severity) {
    case LogicAuthoringIssueSeverity::Warning:
        return "Warning";
    case LogicAuthoringIssueSeverity::Error:
        return "Error";
    }
    return "Warning";
}

bool IsLogicAuthoringNormalizationIssue(std::string_view issueCode) {
    return issueCode.find("_normalized") != std::string::npos || issueCode.find("_clamped") != std::string::npos ||
           issueCode.find("_swapped") != std::string::npos || issueCode.find("_adjusted") != std::string::npos ||
           issueCode.find("_trimmed") != std::string::npos || issueCode.find("_defaulted") != std::string::npos;
}

bool IsLogicAuthoringPortIssue(std::string_view issueCode) {
    const LogicAuthoringIssueCategory category = GetLogicAuthoringIssueCategory(issueCode);
    return category == LogicAuthoringIssueCategory::Port || category == LogicAuthoringIssueCategory::WorldActor;
}

LogicAuthoringIssuePresentation BuildLogicAuthoringIssuePresentation(const LogicAuthoringCompileIssue& issue) {
    const LogicAuthoringIssueCategory category = GetLogicAuthoringIssueCategory(issue.code);
    return LogicAuthoringIssuePresentation{
        .category = category,
        .categoryName = GetLogicAuthoringIssueCategoryName(category),
        .severityName = GetLogicAuthoringIssueSeverityName(issue.severity),
        .normalization = IsLogicAuthoringNormalizationIssue(issue.code),
        .portIssue = IsLogicAuthoringPortIssue(issue.code),
    };
}

std::vector<LogicAuthoringIssuePresentation> BuildLogicAuthoringIssuePresentations(
    const std::vector<LogicAuthoringCompileIssue>& issues) {
    std::vector<LogicAuthoringIssuePresentation> presentations{};
    presentations.reserve(issues.size());
    for (const LogicAuthoringCompileIssue& issue : issues) {
        presentations.push_back(BuildLogicAuthoringIssuePresentation(issue));
    }
    return presentations;
}

bool LogicAuthoringCompileHasErrors(const LogicAuthoringCompileResult& result) {
    if (result.summary.errorCount > 0) {
        return true;
    }
    for (const LogicAuthoringCompileIssue& issue : result.issues) {
        if (issue.severity == LogicAuthoringIssueSeverity::Error) {
            return true;
        }
    }
    return false;
}

bool LogicAuthoringCompileSucceeded(const LogicAuthoringCompileResult& result) {
    return !LogicAuthoringCompileHasErrors(result);
}

LogicAuthoringGraph AutoLayoutLogicAuthoringGraph(const LogicAuthoringGraph& authoring,
                                                  const LogicAutoLayoutOptions& options) {
    LogicAuthoringGraph laidOut = authoring;
    const std::size_t columns = std::max<std::size_t>(1, options.columns);
    std::size_t autoIndex = 0;
    std::unordered_set<std::string> autoNodeIds{};
    autoNodeIds.reserve(laidOut.nodes.size());
    for (LogicNodeInstance& node : laidOut.nodes) {
        if (options.preserveExplicitPlacements && !IsDefaultPlacement(node.placement)) {
            continue;
        }
        const std::string nodeId = NodeDefinitionId(node.definition);
        const std::size_t row = autoIndex / columns;
        const std::size_t col = autoIndex % columns;
        node.placement.position = std::array<float, 3>{
            options.origin[0] + static_cast<float>(col) * options.spacing[0],
            options.origin[1] + static_cast<float>(row) * options.spacing[1],
            options.origin[2] + static_cast<float>(row) * options.spacing[2],
        };
        node.placement.rotationDegrees = std::array<float, 3>{0.0f, 0.0f, 0.0f};
        node.placement.scale = std::array<float, 3>{1.0f, 1.0f, 1.0f};
        node.placement.layer = options.layer;
        node.placement.debugVisible = options.debugVisible;
        if (!nodeId.empty()) {
            autoNodeIds.insert(nodeId);
        }
        ++autoIndex;
    }

    if (options.routeFallbackIoWires) {
        std::unordered_map<std::string, std::array<float, 3>> nodePositions{};
        nodePositions.reserve(laidOut.nodes.size());
        for (const LogicNodeInstance& node : laidOut.nodes) {
            const std::string nodeId = NodeDefinitionId(node.definition);
            if (!nodeId.empty()) {
                nodePositions[nodeId] = node.placement.position;
            }
        }

        for (LogicAuthoringWire& wire : laidOut.wires) {
            if (wire.muted) {
                continue;
            }
            if (options.preserveWireControlPoints && !wire.controlPoints.empty()) {
                continue;
            }
            if (wire.targets.empty()) {
                continue;
            }

            const bool sourceIsAuto = IsNodeInAutoCluster(autoNodeIds, wire.sourceId);
            const std::array<float, 3> sourcePos = ResolveEndpointPosition(wire.sourceId, nodePositions, options);

            // Route once per wire using the first target as representative trunk direction.
            const LogicRouteTarget& target = wire.targets.front();
            const bool targetIsAuto = IsNodeInAutoCluster(autoNodeIds, target.targetId);
            if (sourceIsAuto == targetIsAuto) {
                continue;
            }

            const std::array<float, 3> targetPos = ResolveEndpointPosition(target.targetId, nodePositions, options);
            const std::array<float, 3> inRoomPos = sourceIsAuto ? targetPos : sourcePos;
            const std::array<float, 3> laneA = ComputeIoLaneAnchor(options, sourcePos);
            const std::array<float, 3> laneB = ComputeIoLaneAnchor(options, inRoomPos);

            wire.controlPoints.clear();
            wire.controlPoints.push_back(laneA);
            wire.controlPoints.push_back(laneB);
        }
    }

    return laidOut;
}

LogicGraphSpec CompileLogicAuthoringGraph(const LogicAuthoringGraph& authoring) {
    return CompileLogicAuthoringGraphWithReport(authoring, {}).spec;
}

LogicGraphSpec CompileLogicAuthoringGraph(const LogicAuthoringGraph& authoring,
                                          const LogicAuthoringCompileOptions& options) {
    return CompileLogicAuthoringGraphWithReport(authoring, options).spec;
}

LogicAuthoringCompileResult CompileLogicAuthoringGraphWithReport(const LogicAuthoringGraph& authoring) {
    return CompileLogicAuthoringGraphWithReport(authoring, {});
}

LogicAuthoringCompileResult CompileLogicAuthoringGraphWithReport(const LogicAuthoringGraph& authoring,
                                                                 const LogicAuthoringCompileOptions& options) {
    LogicAuthoringCompileResult result{};
    LogicGraphSpec& spec = result.spec;
    std::unordered_set<std::string> seenNodeIds{};
    seenNodeIds.reserve(authoring.nodes.size());
    std::unordered_map<std::string, std::string> nodeKindsById{};
    nodeKindsById.reserve(authoring.nodes.size());

    for (const LogicNodeInstance& instance : authoring.nodes) {
        const std::string nodeId = NodeDefinitionId(instance.definition);
        if (nodeId.empty()) {
            AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Error,
                     "node.missing_id",
                     "Skipped logic node with empty id.");
            continue;
        }
        if (seenNodeIds.insert(nodeId).second) {
            const LogicNodeDefinition normalized = NormalizeNodeDefinition(instance.definition, result.issues);
            nodeKindsById[nodeId] = std::string(GetLogicNodeKindName(normalized));
            spec.nodes.push_back(normalized);
        } else {
            AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Warning,
                     "node.duplicate_id",
                     "Skipped duplicate logic node id.",
                     nodeId);
        }
    }

    std::unordered_set<std::string> seenWireIds{};
    seenWireIds.reserve(authoring.wires.size());
    for (const LogicAuthoringWire& wire : authoring.wires) {
        if (wire.muted) {
            continue;
        }
        if (!wire.id.empty()) {
            if (!seenWireIds.insert(wire.id).second) {
                AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.duplicate_id",
                         "Skipped duplicate wire id.",
                         wire.id);
                continue;
            }
        }
        if (wire.sourceId.empty()) {
            AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Error,
                     "wire.missing_source",
                     "Skipped wire with empty source id.",
                     wire.id);
            continue;
        }
        const bool sourceIsNode = seenNodeIds.contains(wire.sourceId);
        const bool sourceIsKnownWorldActor = options.knownWorldActorIds.contains(wire.sourceId);
        const bool sourceAcceptedAsWorldActor = !sourceIsNode && (sourceIsKnownWorldActor || options.allowUnknownWorldActorIds);
        if (!sourceIsNode && !sourceAcceptedAsWorldActor) {
            AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Error,
                     "wire.unknown_source",
                     "Skipped wire whose source node does not exist in authoring graph.",
                     wire.id.empty() ? wire.sourceId : wire.id);
            continue;
        }
        if (sourceAcceptedAsWorldActor && !sourceIsKnownWorldActor) {
            AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Warning,
                     "wire.world_actor_source_assumed",
                     "Wire source is not a logic node; treated as world actor endpoint.",
                     wire.sourceId);
        }
        if (wire.outputName.empty()) {
            AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Error,
                     "wire.missing_output",
                     "Skipped wire with empty output name.",
                     wire.id.empty() ? wire.sourceId : wire.id);
            continue;
        }
        if (sourceIsNode) {
            const auto kindIt = nodeKindsById.find(wire.sourceId);
            const LogicNodePortSchema schema =
                GetLogicNodePortSchema(kindIt == nodeKindsById.end() ? std::string_view{} : std::string_view(kindIt->second));
            const PortResolution port = ResolvePortName(schema, wire.outputName, false);
            if (!port.recognized) {
                AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.source_output_unknown",
                         "Source output port name is not defined for node kind.",
                         wire.id.empty() ? wire.sourceId : wire.id);
            } else if (port.normalized) {
                AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.source_output_normalized",
                         "Source output port normalized to canonical port name.",
                         wire.id.empty() ? wire.sourceId : wire.id);
            }
        } else if (sourceIsKnownWorldActor) {
            const auto kindIt = options.knownWorldActorKinds.find(wire.sourceId);
            if (kindIt != options.knownWorldActorKinds.end() && !kindIt->second.empty()) {
                const LogicNodePortSchema schema = GetWorldActorPortSchema(kindIt->second);
                const PortResolution port = ResolvePortName(schema, wire.outputName, false);
                if (!port.recognized) {
                    AddIssue(result.issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "wire.world_source_output_unknown",
                             "Source output port name is not defined for world actor kind.",
                             wire.id.empty() ? wire.sourceId : wire.id);
                } else if (port.normalized) {
                    AddIssue(result.issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "wire.world_source_output_normalized",
                             "World actor source output normalized to canonical port name.",
                             wire.id.empty() ? wire.sourceId : wire.id);
                }
            }
        }
        if (wire.targets.empty()) {
            AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Warning,
                     "wire.no_targets",
                     "Skipped wire with no route targets.",
                     wire.id.empty() ? wire.sourceId : wire.id);
            continue;
        }

        LogicRoute route{};
        route.sourceId = wire.sourceId;
        route.outputName = wire.outputName;
        if (sourceIsNode) {
            const auto kindIt = nodeKindsById.find(wire.sourceId);
            const LogicNodePortSchema schema =
                GetLogicNodePortSchema(kindIt == nodeKindsById.end() ? std::string_view{} : std::string_view(kindIt->second));
            const PortResolution port = ResolvePortName(schema, wire.outputName, false);
            if (port.recognized) {
                route.outputName = port.canonical;
            }
        } else if (sourceIsKnownWorldActor) {
            const auto kindIt = options.knownWorldActorKinds.find(wire.sourceId);
            if (kindIt != options.knownWorldActorKinds.end() && !kindIt->second.empty()) {
                const LogicNodePortSchema schema = GetWorldActorPortSchema(kindIt->second);
                const PortResolution port = ResolvePortName(schema, wire.outputName, false);
                if (port.recognized) {
                    route.outputName = port.canonical;
                }
            }
        }
        route.targets.reserve(wire.targets.size());
        for (const LogicRouteTarget& target : wire.targets) {
            if (target.targetId.empty() || target.inputName.empty()) {
                AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.target_incomplete",
                         "Skipped route target with missing targetId or inputName.",
                         wire.id.empty() ? wire.sourceId : wire.id);
                continue;
            }
            const bool targetIsNode = seenNodeIds.contains(target.targetId);
            const bool targetIsKnownWorldActor = options.knownWorldActorIds.contains(target.targetId);
            const bool targetAcceptedAsWorldActor =
                !targetIsNode && (targetIsKnownWorldActor || options.allowUnknownWorldActorIds);
            if (!targetIsNode && !targetAcceptedAsWorldActor) {
                AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.target_unknown",
                         "Skipped route target that does not match a node id in this authoring graph. "
                         "Use world-actor routes separately when targeting map actors.",
                         target.targetId);
                continue;
            }
            if (targetAcceptedAsWorldActor && !targetIsKnownWorldActor) {
                AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.world_actor_target_assumed",
                         "Route target is not a logic node; treated as world actor endpoint.",
                         target.targetId);
            }
            if (targetIsNode) {
                const auto kindIt = nodeKindsById.find(target.targetId);
                const LogicNodePortSchema schema = GetLogicNodePortSchema(
                    kindIt == nodeKindsById.end() ? std::string_view{} : std::string_view(kindIt->second));
                const PortResolution port = ResolvePortName(schema, target.inputName, true);
                if (!port.recognized) {
                    AddIssue(result.issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "wire.target_input_unknown",
                             "Target input port name is not defined for node kind.",
                             target.targetId);
                } else if (port.normalized) {
                    AddIssue(result.issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "wire.target_input_normalized",
                             "Target input port normalized to canonical port name.",
                             target.targetId);
                }
            } else if (targetIsKnownWorldActor) {
                const auto kindIt = options.knownWorldActorKinds.find(target.targetId);
                if (kindIt != options.knownWorldActorKinds.end() && !kindIt->second.empty()) {
                    const LogicNodePortSchema schema = GetWorldActorPortSchema(kindIt->second);
                    const PortResolution port = ResolvePortName(schema, target.inputName, true);
                    if (!port.recognized) {
                        AddIssue(result.issues,
                                 LogicAuthoringIssueSeverity::Warning,
                                 "wire.world_target_input_unknown",
                                 "Target input port name is not defined for world actor kind.",
                                 target.targetId);
                    } else if (port.normalized) {
                        AddIssue(result.issues,
                                 LogicAuthoringIssueSeverity::Warning,
                                 "wire.world_target_input_normalized",
                                 "World actor target input normalized to canonical port name.",
                                 target.targetId);
                    }
                }
            }
            LogicRouteTarget normalizedTarget = target;
            normalizedTarget.delayMs = ClampRouteDelayMs(target.delayMs);
            if (normalizedTarget.delayMs != target.delayMs) {
                AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.target_delay_clamped",
                         "Route target delay exceeded maximum and was clamped.",
                         target.targetId);
            }
            if (targetIsNode) {
                const auto kindIt = nodeKindsById.find(target.targetId);
                const LogicNodePortSchema schema = GetLogicNodePortSchema(
                    kindIt == nodeKindsById.end() ? std::string_view{} : std::string_view(kindIt->second));
                const PortResolution port = ResolvePortName(schema, target.inputName, true);
                if (port.recognized) {
                    normalizedTarget.inputName = port.canonical;
                }
            } else if (targetIsKnownWorldActor) {
                const auto kindIt = options.knownWorldActorKinds.find(target.targetId);
                if (kindIt != options.knownWorldActorKinds.end() && !kindIt->second.empty()) {
                    const LogicNodePortSchema schema = GetWorldActorPortSchema(kindIt->second);
                    const PortResolution port = ResolvePortName(schema, target.inputName, true);
                    if (port.recognized) {
                        normalizedTarget.inputName = port.canonical;
                    }
                }
            }
            route.targets.push_back(std::move(normalizedTarget));
        }
        if (route.targets.empty()) {
            continue;
        }
        spec.routes.push_back(std::move(route));
    }

    result.summary = BuildCompileSummary(result.issues);
    return result;
}

std::unordered_map<std::string, LogicNodePlacement> BuildLogicNodePlacementMap(const LogicAuthoringGraph& authoring) {
    std::unordered_map<std::string, LogicNodePlacement> placements{};
    placements.reserve(authoring.nodes.size());
    for (const LogicNodeInstance& instance : authoring.nodes) {
        const std::string nodeId = NodeDefinitionId(instance.definition);
        if (nodeId.empty()) {
            continue;
        }
        placements[nodeId] = instance.placement;
    }
    return placements;
}

} // namespace ri::logic
