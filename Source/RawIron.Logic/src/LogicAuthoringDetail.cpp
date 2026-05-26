#include "LogicAuthoringDetail.h"

#include "RawIron/Logic/LogicTypes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <type_traits>
#include <unordered_map>

namespace ri::logic::detail {

[[nodiscard]] std::string NodeDefinitionId(const LogicNodeDefinition& definition) {
    return std::visit([](const auto& node) { return node.id; }, definition);
}

[[nodiscard]] std::string ToLower(std::string_view text) {
    std::string normalized(text);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return normalized;
}

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

[[nodiscard]] PortResolution ResolvePortName(
    const LogicNodePortSchema& schema, std::string_view rawPortName, const bool inputPort) {
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
              std::string subjectId) {
    issues.push_back(LogicAuthoringCompileIssue{
        .severity = severity,
        .code = std::move(code),
        .message = std::move(message),
        .subjectId = std::move(subjectId),
    });
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

} // namespace ri::logic::detail

