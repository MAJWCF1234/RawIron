#include "RawIron/Events/EventEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ri::events {
namespace {

std::optional<std::size_t> NormalizeMaxRuns(const std::optional<std::size_t>& maxRuns) {
    if (!maxRuns.has_value() || *maxRuns == 0) {
        return std::nullopt;
    }
    return *maxRuns;
}

std::string ResolveGroupName(const EventAction& action) {
    if (!action.group.empty()) {
        return action.group;
    }
    if (!action.name.empty()) {
        return action.name;
    }
    return action.target;
}

std::string ResolveSequenceName(const EventAction& action) {
    if (!action.sequence.empty()) {
        return action.sequence;
    }
    if (!action.name.empty()) {
        return action.name;
    }
    return action.target;
}

bool NearlyEqualDouble(double lhs, double rhs) {
    if (lhs == rhs) {
        return true;
    }
    if (!std::isfinite(lhs) || !std::isfinite(rhs)) {
        return false;
    }
    const double absoluteDiff = std::fabs(lhs - rhs);
    constexpr double kAbsoluteEpsilon = 1e-9;
    constexpr double kRelativeEpsilon = 1e-9;
    if (absoluteDiff <= kAbsoluteEpsilon) {
        return true;
    }
    const double largestMagnitude = std::max(std::fabs(lhs), std::fabs(rhs));
    return absoluteDiff <= (largestMagnitude * kRelativeEpsilon);
}

} // namespace

std::string EventEngine::NormalizeTimerId(std::string_view timerId) {
    std::size_t begin = 0;
    while (begin < timerId.size() && std::isspace(static_cast<unsigned char>(timerId[begin])) != 0) {
        ++begin;
    }
    std::size_t end = timerId.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(timerId[end - 1])) != 0) {
        --end;
    }
    return std::string(timerId.substr(begin, end - begin));
}

EventDefinition NormalizeEventDefinition(const EventDefinition& event, std::size_t index, std::string_view sourceName) {
    EventDefinition normalized = event;
    if (normalized.cooldownMs > 86400000ULL) {
        normalized.cooldownMs = 86400000ULL;
    }
    normalized.maxRuns = NormalizeMaxRuns(normalized.maxRuns);
    normalized.resolvedId = !event.id.empty()
        ? event.id
        : (std::string(sourceName.empty() ? "level" : sourceName) + ":event:" + std::to_string(index));
    return normalized;
}

EventEngine::EventEngine(std::string sourceName)
    : sourceName_(std::move(sourceName)) {}

void EventEngine::SetSourceName(std::string_view sourceName) {
    sourceName_ = sourceName.empty() ? std::string("level") : std::string(sourceName);
}

void EventEngine::SetEvents(std::vector<EventDefinition> events) {
    events_.clear();
    events_.reserve(events.size());
    eventRuntimeStates_.clear();
    completedEventIds_.clear();
    timers_.clear();
    activeNamedTimerGenerations_.clear();
    nextTimerGeneration_ = 1;

    for (std::size_t index = 0; index < events.size(); ++index) {
        events_.push_back(NormalizeEventDefinition(events[index], index, sourceName_));
    }
}

void EventEngine::SetActionGroups(ActionGroupMap groups) {
    actionGroups_ = std::move(groups);
}

void EventEngine::SetTargetGroups(TargetGroupMap groups) {
    targetGroups_ = std::move(groups);
}

void EventEngine::SetSequences(SequenceMap sequences) {
    sequences_ = std::move(sequences);
}

const std::vector<EventDefinition>& EventEngine::GetEvents() const {
    return events_;
}

const std::unordered_map<std::string, EventRuntimeState>& EventEngine::GetEventRuntimeStates() const {
    return eventRuntimeStates_;
}

std::size_t EventEngine::ScheduledTimerCount() const {
    return timers_.size();
}

const std::set<std::string>& EventEngine::GetWorldFlags() const {
    return worldFlags_;
}

const std::unordered_map<std::string, double>& EventEngine::GetWorldValues() const {
    return worldValues_;
}

const std::set<std::string>& EventEngine::GetCompletedEventIds() const {
    return completedEventIds_;
}

bool EventEngine::SetWorldFlag(std::string_view flag, bool enabled) {
    if (flag.empty()) {
        return false;
    }
    if (enabled) {
        worldFlags_.insert(std::string(flag));
    } else {
        worldFlags_.erase(std::string(flag));
    }
    return true;
}

bool EventEngine::HasWorldFlag(std::string_view flag) const {
    return !flag.empty() && worldFlags_.contains(std::string(flag));
}

bool EventEngine::SetWorldValue(std::string_view key, double value) {
    if (key.empty() || !std::isfinite(value)) {
        return false;
    }
    worldValues_[std::string(key)] = value;
    return true;
}

bool EventEngine::AddWorldValue(std::string_view key, double amount) {
    if (key.empty() || !std::isfinite(amount)) {
        return false;
    }
    worldValues_[std::string(key)] = GetWorldValue(key) + amount;
    return true;
}

double EventEngine::GetWorldValue(std::string_view key) const {
    if (key.empty()) {
        return 0.0;
    }
    const auto found = worldValues_.find(std::string(key));
    return found == worldValues_.end() ? 0.0 : found->second;
}

EventCheckpointState EventEngine::CaptureCheckpointState() const {
    EventCheckpointState state{};
    state.worldFlags = worldFlags_;
    state.worldValues = worldValues_;
    state.completedEventIds = completedEventIds_;
    state.eventRuntimeStates = eventRuntimeStates_;
    return state;
}

void EventEngine::RestoreCheckpointState(const EventCheckpointState& state) {
    worldFlags_ = state.worldFlags;
    worldValues_.clear();
    for (const auto& [key, value] : state.worldValues) {
        if (!key.empty() && std::isfinite(value)) {
            worldValues_[key] = value;
        }
    }
    completedEventIds_ = state.completedEventIds;
    eventRuntimeStates_.clear();
    for (const auto& [eventId, runtime] : state.eventRuntimeStates) {
        if (eventId.empty()) {
            continue;
        }
        EventRuntimeState restored{};
        restored.runCount = runtime.runCount;
        restored.cooldownUntilMs = std::isfinite(runtime.cooldownUntilMs) ? runtime.cooldownUntilMs : 0.0;
        eventRuntimeStates_.emplace(eventId, restored);
    }
    timers_.clear();
    activeNamedTimerGenerations_.clear();
    nextTimerGeneration_ = 1;
}

void EventEngine::ResetWorldState() {
    worldFlags_.clear();
    worldValues_.clear();
    completedEventIds_.clear();
    eventRuntimeStates_.clear();
    timers_.clear();
    activeNamedTimerGenerations_.clear();
    nextTimerGeneration_ = 1;
}

bool EventEngine::EvaluateConditions(const EventConditions& conditions,
                                     const EventContext& context,
                                     ExtraConditionEvaluator extraEvaluator) const {
    for (const std::string& flag : conditions.requiredFlags) {
        if (!HasWorldFlag(flag)) {
            return false;
        }
    }
    for (const std::string& flag : conditions.missingFlags) {
        if (HasWorldFlag(flag)) {
            return false;
        }
    }
    for (const auto& [key, value] : conditions.valuesAtLeast) {
        if (GetWorldValue(key) < value) {
            return false;
        }
    }
    for (const auto& [key, value] : conditions.valuesAtMost) {
        if (GetWorldValue(key) > value) {
            return false;
        }
    }
    for (const auto& [key, value] : conditions.valuesEqual) {
        if (!NearlyEqualDouble(GetWorldValue(key), value)) {
            return false;
        }
    }
    if (extraEvaluator) {
        return extraEvaluator(conditions, context, *this);
    }
    return true;
}

std::vector<std::string> EventEngine::GetActionTargets(const EventAction& action) const {
    std::vector<std::string> targets;
    auto appendGroup = [&](std::string_view groupName) {
        if (groupName.empty()) {
            return;
        }
        const auto found = targetGroups_.find(std::string(groupName));
        if (found == targetGroups_.end()) {
            return;
        }
        targets.insert(targets.end(), found->second.begin(), found->second.end());
    };

    for (const std::string& groupName : action.targetGroups) {
        appendGroup(groupName);
    }
    appendGroup(action.targetGroup);
    targets.insert(targets.end(), action.targets.begin(), action.targets.end());
    if (!action.target.empty()) {
        targets.push_back(action.target);
    }

    std::vector<std::string> deduped;
    for (const std::string& target : targets) {
        if (target.empty()) {
            continue;
        }
        if (std::find(deduped.begin(), deduped.end(), target) == deduped.end()) {
            deduped.push_back(target);
        }
    }
    return deduped;
}

bool EventEngine::RunHook(std::string_view hookName,
                          const EventContext& context,
                          const ActionExecutor& executor,
                          double nowMs,
                          ExtraConditionEvaluator extraEvaluator) {
    if (hookName.empty() || events_.empty()) {
        return false;
    }

    bool consumed = false;
    for (const EventDefinition& event : events_) {
        if (event.hook != hookName) {
            continue;
        }
        if (!event.sourceId.empty() && event.sourceId != context.sourceId) {
            continue;
        }
        if (event.once && completedEventIds_.contains(event.resolvedId)) {
            continue;
        }

        EventRuntimeState& runtime = GetEventRuntimeState(event);
        if (runtime.cooldownUntilMs > nowMs) {
            continue;
        }
        if (event.maxRuns.has_value() && runtime.runCount >= *event.maxRuns) {
            continue;
        }
        if (!EvaluateConditions(event.conditions, context, extraEvaluator)) {
            continue;
        }

        if (event.once) {
            completedEventIds_.insert(event.resolvedId);
        }
        runtime.runCount += 1;
        runtime.cooldownUntilMs = event.cooldownMs > 0 ? nowMs + static_cast<double>(event.cooldownMs) : 0.0;

        RunActions(event.actions, context, executor, nowMs, nullptr, extraEvaluator);
        if (event.consumeInteraction) {
            consumed = true;
        }
        if (event.stopAfterMatch) {
            break;
        }
    }

    return consumed;
}

void EventEngine::RunActions(const std::vector<EventAction>& actions,
                             const EventContext& context,
                             const ActionExecutor& executor,
                             double nowMs,
                             std::set<std::string>* groupStack,
                             ExtraConditionEvaluator extraEvaluator) {
    for (const EventAction& action : actions) {
        RunAction(action, context, executor, nowMs, groupStack, extraEvaluator);
    }
}

void EventEngine::Tick(double nowMs,
                       const ActionExecutor& executor,
                       ExtraConditionEvaluator extraEvaluator) {
    std::stable_sort(timers_.begin(), timers_.end(), [](const TimerRecord& lhs, const TimerRecord& rhs) {
        return lhs.triggerTimeMs < rhs.triggerTimeMs;
    });

    std::vector<TimerRecord> pending;
    pending.swap(timers_);

    for (TimerRecord& timer : pending) {
        if (timer.triggerTimeMs > nowMs) {
            timers_.push_back(std::move(timer));
            continue;
        }

        if (!timer.timerId.empty()) {
            const auto found = activeNamedTimerGenerations_.find(timer.timerId);
            if (found == activeNamedTimerGenerations_.end() || found->second != timer.generation) {
                continue;
            }
            activeNamedTimerGenerations_.erase(found);
        }

        RunActions(timer.actions, timer.context, executor, nowMs, nullptr, extraEvaluator);
    }
}

bool EventEngine::CancelNamedTimer(std::string_view timerId) {
    const std::string normalizedTimerId = NormalizeTimerId(timerId);
    if (normalizedTimerId.empty()) {
        return false;
    }
    return activeNamedTimerGenerations_.erase(normalizedTimerId) > 0;
}

std::size_t EventEngine::CancelNamedTimersByPrefix(std::string_view prefix) {
    if (prefix.empty()) {
        return 0;
    }
    std::vector<std::string> toCancel;
    for (const auto& [timerId, generation] : activeNamedTimerGenerations_) {
        (void)generation;
        if (timerId.rfind(prefix, 0) == 0) {
            toCancel.push_back(timerId);
        }
    }
    for (const std::string& timerId : toCancel) {
        activeNamedTimerGenerations_.erase(timerId);
    }
    return toCancel.size();
}

EventRuntimeState& EventEngine::GetEventRuntimeState(const EventDefinition& event) {
    return eventRuntimeStates_[event.resolvedId];
}

void EventEngine::ScheduleNamedActions(std::string_view timerId,
                                       const std::vector<EventAction>& actions,
                                       const EventContext& context,
                                       double triggerTimeMs,
                                       bool replaceExisting) {
    const double sanitizedTriggerTime = std::isfinite(triggerTimeMs) ? triggerTimeMs : 0.0;
    const std::string normalizedTimerId = NormalizeTimerId(timerId);
    if (normalizedTimerId.empty()) {
        timers_.push_back(TimerRecord{
            .timerId = {},
            .triggerTimeMs = sanitizedTriggerTime,
            .actions = actions,
            .context = context,
            .generation = 0,
        });
        return;
    }

    const std::string timerKey = normalizedTimerId;
    if (activeNamedTimerGenerations_.contains(timerKey) && !replaceExisting) {
        return;
    }
    activeNamedTimerGenerations_.erase(timerKey);

    const std::uint64_t generation = nextTimerGeneration_++;
    activeNamedTimerGenerations_[timerKey] = generation;
    timers_.push_back(TimerRecord{
        .timerId = timerKey,
        .triggerTimeMs = sanitizedTriggerTime,
        .actions = actions,
        .context = context,
        .generation = generation,
    });
}

void EventEngine::RunAction(const EventAction& action,
                            const EventContext& context,
                            const ActionExecutor& executor,
                            double nowMs,
                            std::set<std::string>* groupStack,
                            ExtraConditionEvaluator extraEvaluator) {
    const auto handlerIt = BuiltinActionHandlers().find(action.type);
    if (handlerIt != BuiltinActionHandlers().end()) {
        (this->*(handlerIt->second))(action, context, executor, nowMs, groupStack, extraEvaluator);
        return;
    }

    if (executor) {
        executor(action, context, *this);
    }
}

const std::unordered_map<std::string, EventEngine::BuiltinActionHandler>& EventEngine::BuiltinActionHandlers() {
    static const std::unordered_map<std::string, BuiltinActionHandler> handlers{
        {"set_flag", &EventEngine::HandleSetFlagAction},
        {"set_value", &EventEngine::HandleSetValueAction},
        {"add_value", &EventEngine::HandleAddValueAction},
        {"run_group", &EventEngine::HandleRunGroupAction},
        {"run_sequence", &EventEngine::HandleRunSequenceAction},
        {"if", &EventEngine::HandleIfAction},
        {"delay", &EventEngine::HandleDelayAction},
        {"cancel_sequence", &EventEngine::HandleCancelSequenceAction},
        {"cancel_timer", &EventEngine::HandleCancelTimerAction},
    };
    return handlers;
}

bool EventEngine::HandleSetFlagAction(const EventAction& action,
                                      const EventContext& context,
                                      const ActionExecutor& executor,
                                      const double nowMs,
                                      std::set<std::string>* groupStack,
                                      ExtraConditionEvaluator extraEvaluator) {
    (void)context;
    (void)executor;
    (void)nowMs;
    (void)groupStack;
    (void)extraEvaluator;
    SetWorldFlag(action.flag, action.enabled);
    return true;
}

bool EventEngine::HandleSetValueAction(const EventAction& action,
                                       const EventContext& context,
                                       const ActionExecutor& executor,
                                       const double nowMs,
                                       std::set<std::string>* groupStack,
                                       ExtraConditionEvaluator extraEvaluator) {
    (void)context;
    (void)executor;
    (void)nowMs;
    (void)groupStack;
    (void)extraEvaluator;
    const std::string key = !action.key.empty() ? action.key : action.name;
    SetWorldValue(key, action.value);
    return true;
}

bool EventEngine::HandleAddValueAction(const EventAction& action,
                                       const EventContext& context,
                                       const ActionExecutor& executor,
                                       const double nowMs,
                                       std::set<std::string>* groupStack,
                                       ExtraConditionEvaluator extraEvaluator) {
    (void)context;
    (void)executor;
    (void)nowMs;
    (void)groupStack;
    (void)extraEvaluator;
    const std::string key = !action.key.empty() ? action.key : action.name;
    AddWorldValue(key, action.amount != 0.0 ? action.amount : action.value);
    return true;
}

bool EventEngine::HandleRunGroupAction(const EventAction& action,
                                       const EventContext& context,
                                       const ActionExecutor& executor,
                                       const double nowMs,
                                       std::set<std::string>* groupStack,
                                       ExtraConditionEvaluator extraEvaluator) {
    const std::string groupName = ResolveGroupName(action);
    if (groupName.empty()) {
        return true;
    }
    std::set<std::string> localGroupStack = groupStack != nullptr ? *groupStack : std::set<std::string>{};
    if (localGroupStack.contains(groupName)) {
        return true;
    }
    const auto found = actionGroups_.find(groupName);
    if (found == actionGroups_.end()) {
        return true;
    }
    localGroupStack.insert(groupName);
    RunActions(found->second, context, executor, nowMs, &localGroupStack, extraEvaluator);
    return true;
}

bool EventEngine::HandleRunSequenceAction(const EventAction& action,
                                          const EventContext& context,
                                          const ActionExecutor& executor,
                                          const double nowMs,
                                          std::set<std::string>* groupStack,
                                          ExtraConditionEvaluator extraEvaluator) {
    (void)executor;
    (void)groupStack;
    (void)extraEvaluator;
    const std::string sequenceName = ResolveSequenceName(action);
    const auto found = sequences_.find(sequenceName);
    if (sequenceName.empty() || found == sequences_.end()) {
        return true;
    }
    const std::string timerPrefix = "sequence:" + sequenceName + ':';
    if (action.replaceExisting) {
        CancelNamedTimersByPrefix(timerPrefix);
    }
    double elapsedMs = 0.0;
    for (std::size_t index = 0; index < found->second.size(); ++index) {
        const EventSequenceStep& step = found->second[index];
        elapsedMs += std::max(0.0, step.delayMs);
        ScheduleNamedActions(timerPrefix + std::to_string(index), step.actions, context, nowMs + elapsedMs, true);
    }
    return true;
}

bool EventEngine::HandleIfAction(const EventAction& action,
                                 const EventContext& context,
                                 const ActionExecutor& executor,
                                 const double nowMs,
                                 std::set<std::string>* groupStack,
                                 ExtraConditionEvaluator extraEvaluator) {
    const bool passed = EvaluateConditions(action.conditions, context, extraEvaluator);
    const std::vector<EventAction>& branch = passed
        ? (!action.thenActions.empty() ? action.thenActions : action.actions)
        : action.elseActions;
    RunActions(branch, context, executor, nowMs, groupStack, extraEvaluator);
    return true;
}

bool EventEngine::HandleDelayAction(const EventAction& action,
                                    const EventContext& context,
                                    const ActionExecutor& executor,
                                    const double nowMs,
                                    std::set<std::string>* groupStack,
                                    ExtraConditionEvaluator extraEvaluator) {
    (void)executor;
    (void)groupStack;
    (void)extraEvaluator;
    ScheduleNamedActions(action.timerId,
                         action.actions,
                         context,
                         nowMs + std::max(0.0, action.delayMs),
                         action.replaceExisting);
    return true;
}

bool EventEngine::HandleCancelSequenceAction(const EventAction& action,
                                             const EventContext& context,
                                             const ActionExecutor& executor,
                                             const double nowMs,
                                             std::set<std::string>* groupStack,
                                             ExtraConditionEvaluator extraEvaluator) {
    (void)context;
    (void)executor;
    (void)nowMs;
    (void)groupStack;
    (void)extraEvaluator;
    const std::string sequenceName = ResolveSequenceName(action);
    if (!sequenceName.empty()) {
        CancelNamedTimersByPrefix("sequence:" + sequenceName + ':');
    }
    return true;
}

bool EventEngine::HandleCancelTimerAction(const EventAction& action,
                                          const EventContext& context,
                                          const ActionExecutor& executor,
                                          const double nowMs,
                                          std::set<std::string>* groupStack,
                                          ExtraConditionEvaluator extraEvaluator) {
    (void)context;
    (void)executor;
    (void)nowMs;
    (void)groupStack;
    (void)extraEvaluator;
    for (const std::string& timerId : GetActionTargets(action)) {
        CancelNamedTimer(timerId);
    }
    if (!action.timerId.empty()) {
        CancelNamedTimer(action.timerId);
    }
    return true;
}

} // namespace ri::events
