#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::events {

struct EventConditions {
    std::vector<std::string> requiredFlags;
    std::vector<std::string> missingFlags;
    std::unordered_map<std::string, double> valuesAtLeast;
    std::unordered_map<std::string, double> valuesAtMost;
    std::unordered_map<std::string, double> valuesEqual;
};

struct EventAction {
    std::string type;
    std::string text;
    std::string flag;
    std::string key;
    std::string name;
    std::string group;
    std::string sequence;
    std::string target;
    std::vector<std::string> targets;
    std::string targetGroup;
    std::vector<std::string> targetGroups;
    std::string timerId;
    bool enabled = true;
    bool replaceExisting = true;
    double durationMs = 0.0;
    double delayMs = 0.0;
    double value = 0.0;
    double amount = 0.0;
    EventConditions conditions;
    std::vector<EventAction> actions;
    std::vector<EventAction> thenActions;
    std::vector<EventAction> elseActions;
};

struct EventSequenceStep {
    double delayMs = 0.0;
    std::vector<EventAction> actions;
};

struct EventDefinition {
    std::string id;
    std::string hook;
    std::string sourceId;
    EventConditions conditions;
    std::vector<EventAction> actions;
    bool once = true;
    bool consumeInteraction = false;
    bool stopAfterMatch = false;
    std::uint64_t cooldownMs = 0;
    std::optional<std::size_t> maxRuns;
    std::string resolvedId;
};

struct EventRuntimeState {
    std::size_t runCount = 0;
    double cooldownUntilMs = 0.0;
};

struct EventContext {
    std::string sourceId;
    std::unordered_map<std::string, std::string> stringValues;
};

struct EventCheckpointState {
    std::set<std::string> worldFlags;
    std::unordered_map<std::string, double> worldValues;
    std::set<std::string> completedEventIds;
    std::unordered_map<std::string, EventRuntimeState> eventRuntimeStates;
};

using ActionGroupMap = std::unordered_map<std::string, std::vector<EventAction>>;
using TargetGroupMap = std::unordered_map<std::string, std::vector<std::string>>;
using SequenceMap = std::unordered_map<std::string, std::vector<EventSequenceStep>>;

class EventEngine;
using ActionExecutor = std::function<void(const EventAction& action, const EventContext& context, EventEngine& engine)>;
using ExtraConditionEvaluator = std::function<bool(const EventConditions& conditions, const EventContext& context, const EventEngine& engine)>;

[[nodiscard]] EventDefinition NormalizeEventDefinition(const EventDefinition& event,
                                                       std::size_t index,
                                                       std::string_view sourceName = "level");

class EventEngine {
public:
    explicit EventEngine(std::string sourceName = "level");

    void SetSourceName(std::string_view sourceName);
    void SetEvents(std::vector<EventDefinition> events);
    void SetActionGroups(ActionGroupMap groups);
    void SetTargetGroups(TargetGroupMap groups);
    void SetSequences(SequenceMap sequences);

    [[nodiscard]] const std::vector<EventDefinition>& GetEvents() const;
    [[nodiscard]] const std::unordered_map<std::string, EventRuntimeState>& GetEventRuntimeStates() const;
    [[nodiscard]] std::size_t ScheduledTimerCount() const;
    [[nodiscard]] const std::set<std::string>& GetWorldFlags() const;
    [[nodiscard]] const std::unordered_map<std::string, double>& GetWorldValues() const;
    [[nodiscard]] const std::set<std::string>& GetCompletedEventIds() const;

    bool SetWorldFlag(std::string_view flag, bool enabled = true);
    [[nodiscard]] bool HasWorldFlag(std::string_view flag) const;
    bool SetWorldValue(std::string_view key, double value);
    bool AddWorldValue(std::string_view key, double amount);
    [[nodiscard]] double GetWorldValue(std::string_view key) const;
    [[nodiscard]] EventCheckpointState CaptureCheckpointState() const;
    void RestoreCheckpointState(const EventCheckpointState& state);
    void ResetWorldState();

    [[nodiscard]] bool EvaluateConditions(const EventConditions& conditions,
                                          const EventContext& context = {},
                                          ExtraConditionEvaluator extraEvaluator = {}) const;
    [[nodiscard]] std::vector<std::string> GetActionTargets(const EventAction& action) const;

    bool RunHook(std::string_view hookName,
                 const EventContext& context,
                 const ActionExecutor& executor,
                 double nowMs,
                 ExtraConditionEvaluator extraEvaluator = {});
    void RunActions(const std::vector<EventAction>& actions,
                    const EventContext& context,
                    const ActionExecutor& executor,
                    double nowMs,
                    std::set<std::string>* groupStack = nullptr,
                    ExtraConditionEvaluator extraEvaluator = {});
    void Tick(double nowMs,
              const ActionExecutor& executor,
              ExtraConditionEvaluator extraEvaluator = {});

    bool CancelNamedTimer(std::string_view timerId);
    std::size_t CancelNamedTimersByPrefix(std::string_view prefix);

private:
    using BuiltinActionHandler = bool (EventEngine::*)(const EventAction& action,
                                                       const EventContext& context,
                                                       const ActionExecutor& executor,
                                                       double nowMs,
                                                       std::set<std::string>* groupStack,
                                                       ExtraConditionEvaluator extraEvaluator);

    struct TimerRecord {
        std::string timerId;
        double triggerTimeMs = 0.0;
        std::vector<EventAction> actions;
        EventContext context;
        std::uint64_t generation = 0;
    };

    EventRuntimeState& GetEventRuntimeState(const EventDefinition& event);
    [[nodiscard]] static std::string NormalizeTimerId(std::string_view timerId);
    void ScheduleNamedActions(std::string_view timerId,
                              const std::vector<EventAction>& actions,
                              const EventContext& context,
                              double triggerTimeMs,
                              bool replaceExisting);
    void RunAction(const EventAction& action,
                   const EventContext& context,
                   const ActionExecutor& executor,
                   double nowMs,
                   std::set<std::string>* groupStack,
                   ExtraConditionEvaluator extraEvaluator);
    [[nodiscard]] static const std::unordered_map<std::string, BuiltinActionHandler>& BuiltinActionHandlers();
    bool HandleSetFlagAction(const EventAction& action,
                             const EventContext& context,
                             const ActionExecutor& executor,
                             double nowMs,
                             std::set<std::string>* groupStack,
                             ExtraConditionEvaluator extraEvaluator);
    bool HandleSetValueAction(const EventAction& action,
                              const EventContext& context,
                              const ActionExecutor& executor,
                              double nowMs,
                              std::set<std::string>* groupStack,
                              ExtraConditionEvaluator extraEvaluator);
    bool HandleAddValueAction(const EventAction& action,
                              const EventContext& context,
                              const ActionExecutor& executor,
                              double nowMs,
                              std::set<std::string>* groupStack,
                              ExtraConditionEvaluator extraEvaluator);
    bool HandleRunGroupAction(const EventAction& action,
                              const EventContext& context,
                              const ActionExecutor& executor,
                              double nowMs,
                              std::set<std::string>* groupStack,
                              ExtraConditionEvaluator extraEvaluator);
    bool HandleRunSequenceAction(const EventAction& action,
                                 const EventContext& context,
                                 const ActionExecutor& executor,
                                 double nowMs,
                                 std::set<std::string>* groupStack,
                                 ExtraConditionEvaluator extraEvaluator);
    bool HandleIfAction(const EventAction& action,
                        const EventContext& context,
                        const ActionExecutor& executor,
                        double nowMs,
                        std::set<std::string>* groupStack,
                        ExtraConditionEvaluator extraEvaluator);
    bool HandleDelayAction(const EventAction& action,
                           const EventContext& context,
                           const ActionExecutor& executor,
                           double nowMs,
                           std::set<std::string>* groupStack,
                           ExtraConditionEvaluator extraEvaluator);
    bool HandleCancelSequenceAction(const EventAction& action,
                                    const EventContext& context,
                                    const ActionExecutor& executor,
                                    double nowMs,
                                    std::set<std::string>* groupStack,
                                    ExtraConditionEvaluator extraEvaluator);
    bool HandleCancelTimerAction(const EventAction& action,
                                 const EventContext& context,
                                 const ActionExecutor& executor,
                                 double nowMs,
                                 std::set<std::string>* groupStack,
                                 ExtraConditionEvaluator extraEvaluator);

    std::string sourceName_;
    std::vector<EventDefinition> events_;
    std::unordered_map<std::string, EventRuntimeState> eventRuntimeStates_;
    std::set<std::string> completedEventIds_;
    std::set<std::string> worldFlags_;
    std::unordered_map<std::string, double> worldValues_;
    ActionGroupMap actionGroups_;
    TargetGroupMap targetGroups_;
    SequenceMap sequences_;
    std::vector<TimerRecord> timers_;
    std::unordered_map<std::string, std::uint64_t> activeNamedTimerGenerations_;
    std::uint64_t nextTimerGeneration_ = 1;
};

} // namespace ri::events
