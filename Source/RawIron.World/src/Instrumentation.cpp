#include "RawIron/World/Instrumentation.h"

#include "RawIron/Runtime/EntityIoTelemetry.h"
#include "RawIron/Runtime/RuntimeId.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ri::world {
namespace {

std::string GetFieldOrDefault(const ri::runtime::RuntimeEvent& event,
                              std::string_view field,
                              std::string_view fallback = {}) {
    const auto found = event.fields.find(std::string(field));
    if (found != event.fields.end() && !found->second.empty()) {
        return found->second;
    }
    return std::string(fallback);
}

bool ParseBooleanField(const ri::runtime::RuntimeEvent& event, std::string_view field) {
    const auto found = event.fields.find(std::string(field));
    if (found == event.fields.end()) {
        return false;
    }
    const std::string& value = found->second;
    return value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "on";
}

void RemoveListener(ri::runtime::RuntimeEventBus* bus,
                    std::string_view type,
                    std::optional<ri::runtime::RuntimeEventBus::ListenerId>& listenerId) {
    if (bus != nullptr && listenerId.has_value()) {
        bus->Off(type, *listenerId);
        listenerId.reset();
    }
}

std::string FormatNumber(double value) {
    std::string text = std::to_string(value);
    if (const std::size_t dot = text.find('.'); dot != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    return text.empty() ? std::string("0") : text;
}

std::string FormatFixedNumber(double value, int decimals) {
    if (!std::isfinite(value)) {
        return "0";
    }

    std::string text = std::to_string(value);
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos) {
        return text;
    }

    const std::size_t desiredLength = dot + 1U + static_cast<std::size_t>(std::max(decimals, 0));
    if (text.size() > desiredLength) {
        text.resize(desiredLength);
    }
    while (text.size() > (dot + 2U) && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.push_back('0');
    }
    return text;
}

InfoPanelValue GetRuntimeMetricValue(std::string_view metric,
                                     const InfoPanelBinding& binding,
                                     const InfoPanelBindingContext& context) {
    const RuntimeHelperMetricsSnapshot emptyMetrics{};
    const RuntimeHelperMetricsSnapshot& helperMetrics = context.helperMetrics != nullptr
        ? *context.helperMetrics
        : emptyMetrics;

    if (metric == "physicsObjects") return static_cast<double>(context.runtimeCounts.physicsObjects);
    if (metric == "enemies") return static_cast<double>(context.runtimeCounts.enemies);
    if (metric == "logicEntities") return static_cast<double>(context.runtimeCounts.logicEntities);
    if (metric == "structuralCollidables") return static_cast<double>(context.runtimeCounts.structuralCollidables);
    if (metric == "bvhMeshes") return static_cast<double>(context.runtimeCounts.bvhMeshes);
    if (metric == "triggerVolumes") return static_cast<double>(context.runtimeCounts.triggerVolumes);
    if (metric == "interactives") return static_cast<double>(context.runtimeCounts.interactives);
    if (metric == "collidables") return static_cast<double>(context.runtimeCounts.collidables);
    if (metric == "schemaValidations") return static_cast<double>(helperMetrics.schemaValidations);
    if (metric == "schemaValidationFailures") return static_cast<double>(helperMetrics.schemaValidationFailures);
    if (metric == "tuningParses") return static_cast<double>(helperMetrics.tuningParses);
    if (metric == "tuningParseFailures") return static_cast<double>(helperMetrics.tuningParseFailures);
    if (metric == "eventBusEmits") return static_cast<double>(helperMetrics.eventBusEmits);
    if (metric == "eventBusListeners") return static_cast<double>(helperMetrics.eventBusListeners);
    if (metric == "eventBusListenersAdded") return static_cast<double>(helperMetrics.eventBusListenersAdded);
    if (metric == "eventBusListenersRemoved") return static_cast<double>(helperMetrics.eventBusListenersRemoved);
    if (metric == "audioManagedSounds") return static_cast<double>(helperMetrics.audioManagedSounds);
    if (metric == "audioLoopsCreated") return static_cast<double>(helperMetrics.audioLoopsCreated);
    if (metric == "audioOneShotsPlayed") return static_cast<double>(helperMetrics.audioOneShotsPlayed);
    if (metric == "audioVoicesPlayed") return static_cast<double>(helperMetrics.audioVoicesPlayed);
    if (metric == "audioVoiceActive") return helperMetrics.audioVoiceActive;
    if (metric == "runtimeSession") return helperMetrics.runtimeSession;
    if (metric == "lastAudioEvent") return helperMetrics.lastAudioEvent;
    if (metric == "lastStateChange") return helperMetrics.lastStateChange;
    if (metric == "lastTriggerEvent") return helperMetrics.lastTriggerEvent;
    if (metric == "lastEntityIoEvent") return helperMetrics.lastEntityIoEvent;
    if (metric == "lastMessage") return helperMetrics.lastMessage;
    if (metric == "lastLevelEvent") return helperMetrics.lastLevelEvent;
    if (metric == "lastSchemaEvent") return helperMetrics.lastSchemaEvent;
    if (metric == "structuralDeferredOperations") return static_cast<double>(helperMetrics.structuralDeferredOperations);
    if (metric == "structuralDeferredUnsupportedOperations")
        return static_cast<double>(helperMetrics.structuralDeferredUnsupportedOperations);
    if (metric == "structuralDeferredHealth") return helperMetrics.structuralDeferredHealth;
    if (metric == "structuralDeferredStatusLine") return helperMetrics.structuralDeferredStatusLine;
    if (metric == "structuralDeferredSummary") return helperMetrics.structuralDeferredSummary;
    if (metric == "voxelGiBoundsVolumes") return static_cast<double>(helperMetrics.voxelGiBoundsVolumes);
    if (metric == "lightmapDensityVolumes") return static_cast<double>(helperMetrics.lightmapDensityVolumes);
    if (metric == "shadowExclusionVolumes") return static_cast<double>(helperMetrics.shadowExclusionVolumes);
    if (metric == "cullingDistanceVolumes") return static_cast<double>(helperMetrics.cullingDistanceVolumes);
    if (metric == "statsOverlayEnabled") return helperMetrics.statsOverlayEnabled;
    if (metric == "statsOverlayAttached") return helperMetrics.statsOverlayAttached;
    if (metric == "statsOverlayFrameTimeMs") return helperMetrics.statsOverlayFrameTimeMs;
    if (metric == "statsOverlayFramesPerSecond") return helperMetrics.statsOverlayFramesPerSecond;
    if (metric == "infoPanelSpawners") return static_cast<double>(helperMetrics.infoPanelSpawners);

    if (!std::holds_alternative<std::monostate>(binding.fallback)) {
        return binding.fallback;
    }
    return std::string("-");
}

} // namespace

std::string FormatInfoPanelValue(const InfoPanelValue& value) {
    if (const auto text = std::get_if<std::string>(&value)) {
        return *text;
    }
    if (const auto number = std::get_if<double>(&value)) {
        if (!std::isfinite(*number)) {
            return "-";
        }
        return FormatNumber(*number);
    }
    if (const auto flag = std::get_if<bool>(&value)) {
        return *flag ? "true" : "false";
    }
    return "-";
}

InfoPanelValue ResolveInfoPanelBindingValue(const InfoPanelBinding& binding,
                                            const InfoPanelBindingContext& context) {
    if (!binding.logicEntityId.empty()) {
        if (context.logicEntities == nullptr) {
            return !std::holds_alternative<std::monostate>(binding.fallback) ? binding.fallback : InfoPanelValue{"missing"};
        }

        const auto entityIt = context.logicEntities->find(binding.logicEntityId);
        if (entityIt == context.logicEntities->end()) {
            return !std::holds_alternative<std::monostate>(binding.fallback) ? binding.fallback : InfoPanelValue{"missing"};
        }

        const std::string property = binding.property.empty() ? std::string("value") : binding.property;
        const auto propIt = entityIt->second.find(property);
        if (propIt == entityIt->second.end()) {
            return !std::holds_alternative<std::monostate>(binding.fallback) ? binding.fallback : InfoPanelValue{"missing"};
        }

        if (const auto number = std::get_if<double>(&propIt->second); number != nullptr && !std::isfinite(*number)) {
            return !std::holds_alternative<std::monostate>(binding.fallback) ? binding.fallback : InfoPanelValue{"-"};
        }
        if (const auto flag = std::get_if<bool>(&propIt->second)) {
            return *flag ? InfoPanelValue{"ON"} : InfoPanelValue{"OFF"};
        }
        return propIt->second;
    }

    if (!binding.worldValue.empty()) {
        const auto* worldValues = context.worldValues;
        if (worldValues != nullptr) {
            const auto valueIt = worldValues->find(binding.worldValue);
            if (valueIt != worldValues->end()) {
                if (std::isfinite(valueIt->second)) {
                    return valueIt->second;
                }
                if (!std::holds_alternative<std::monostate>(binding.fallback)) {
                    return binding.fallback;
                }
                return 0.0;
            }
        }
        if (!std::holds_alternative<std::monostate>(binding.fallback)) {
            return binding.fallback;
        }
        return 0.0;
    }

    if (!binding.worldFlag.empty()) {
        const bool isSet = context.worldFlags != nullptr && context.worldFlags->contains(binding.worldFlag);
        return isSet ? InfoPanelValue{"SET"} : InfoPanelValue{"CLEAR"};
    }

    if (!binding.runtimeMetric.empty()) {
        return GetRuntimeMetricValue(binding.runtimeMetric, binding, context);
    }

    if (!std::holds_alternative<std::monostate>(binding.value)) {
        return binding.value;
    }
    if (!std::holds_alternative<std::monostate>(binding.fallback)) {
        return binding.fallback;
    }
    return std::string("-");
}

std::vector<std::string> ResolveInfoPanelLines(const InfoPanelDefinition& panel,
                                               const InfoPanelBindingContext& context) {
    std::vector<std::string> staticLines = panel.lines;
    if (staticLines.empty() && !panel.text.empty()) {
        staticLines.push_back(panel.text);
    }
    if (panel.bindings.empty()) {
        return staticLines;
    }

    std::vector<std::string> boundLines;
    boundLines.reserve(panel.bindings.size());
    for (const InfoPanelBinding& binding : panel.bindings) {
        std::string upperLabel = binding.label;
        std::transform(upperLabel.begin(), upperLabel.end(), upperLabel.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        const std::string label = upperLabel.empty() ? std::string{} : (upperLabel + ": ");
        boundLines.push_back(label + FormatInfoPanelValue(ResolveInfoPanelBindingValue(binding, context)));
    }

    if (panel.replaceBindings) {
        return boundLines;
    }

    staticLines.insert(staticLines.end(), boundLines.begin(), boundLines.end());
    return staticLines;
}

std::vector<std::string> FormatRuntimeStatsOverlayLines(const RuntimeStatsOverlaySnapshot& snapshot,
                                                        std::size_t maxLines) {
    std::vector<std::string> lines;
    lines.reserve(6);
    lines.push_back("RAW IRON STATS [" + snapshot.modeLabel + "]");
    lines.push_back("fps " + FormatFixedNumber(snapshot.metrics.framesPerSecond, 1) +
                    " | frame " + FormatFixedNumber(snapshot.metrics.frameTimeMs, 2) + " ms");
    lines.push_back("nodes " + std::to_string(snapshot.sceneNodes) +
                    " | roots " + std::to_string(snapshot.rootNodes) +
                    " | drawables " + std::to_string(snapshot.renderables));
    lines.push_back("lights " + std::to_string(snapshot.lights) +
                    " | cameras " + std::to_string(snapshot.cameras) +
                    " | selected " + std::to_string(snapshot.selectedNode));
    lines.push_back("overlay " + std::string(snapshot.metrics.visible ? "visible" : "hidden") +
                    " | attached " + std::string(snapshot.metrics.attached ? "yes" : "no"));
    lines.push_back("scene " + snapshot.sceneLabel);
    if (maxLines > 0U && lines.size() > maxLines) {
        lines.resize(maxLines);
    }
    return lines;
}

void DynamicInfoPanelSubsystem::SetSpawners(std::vector<DynamicInfoPanelSpawner> spawners) {
    spawners_ = std::move(spawners);
    cache_.clear();
}

void DynamicInfoPanelSubsystem::Clear() {
    spawners_.clear();
    cache_.clear();
}

std::vector<DynamicInfoPanelRenderState> DynamicInfoPanelSubsystem::UpdateAndResolve(
    const double nowSeconds,
    const InfoPanelBindingContext& context) {
    std::vector<DynamicInfoPanelRenderState> states{};
    states.reserve(spawners_.size());
    std::unordered_map<std::string, bool> seenIds{};
    seenIds.reserve(spawners_.size());

    for (const DynamicInfoPanelSpawner& spawner : spawners_) {
        const std::string cacheId = spawner.id.empty() ? std::string("info_panel") : spawner.id;
        seenIds[cacheId] = true;
        CacheEntry& cached = cache_[cacheId];

        if (cached.state.id.empty()) {
            cached.state.id = cacheId;
            cached.state.revision = 0;
        }
        cached.state.title = spawner.title;
        cached.state.position = spawner.position;
        cached.state.size = spawner.size;

        const bool forceRefresh = cached.nextRefreshSeconds <= nowSeconds;
        if (forceRefresh) {
            const std::vector<std::string> nextLines = ResolveInfoPanelLines(spawner.panel, context);
            const bool changed = nextLines != cached.state.lines;
            cached.state.lines = nextLines;
            cached.state.dirty = changed;
            if (changed) {
                cached.state.revision += 1;
            }
            const double refreshInterval = std::max(0.01, spawner.refreshIntervalSeconds);
            cached.nextRefreshSeconds = nowSeconds + refreshInterval;
        } else {
            cached.state.dirty = false;
        }

        states.push_back(cached.state);
    }

    for (auto it = cache_.begin(); it != cache_.end();) {
        if (!seenIds.contains(it->first)) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }

    return states;
}

HelperActivityTracker::~HelperActivityTracker() {
    Detach();
}

void HelperActivityTracker::Attach(ri::runtime::RuntimeEventBus& eventBus) {
    if (eventBus_ == &eventBus) {
        return;
    }

    Detach();
    eventBus_ = &eventBus;

    audioChangedListener_ = eventBus.On("audioChanged", [this](const ri::runtime::RuntimeEvent& event) {
        const std::string activityType = GetFieldOrDefault(event, "type", event.type);
        const std::string channel = GetFieldOrDefault(event, "channel");
        state_.lastAudioEvent = SummarizeHelperActivity(activityType + (!channel.empty() ? ":" + channel : std::string{}));
    });

    stateChangedListener_ = eventBus.On("stateChanged", [this](const ri::runtime::RuntimeEvent& event) {
        const std::string key = GetFieldOrDefault(event, "key", "state");
        const std::string value = GetFieldOrDefault(event, "value");
        state_.lastStateChange = SummarizeHelperActivity(key + "=" + value);
    });

    triggerChangedListener_ = eventBus.On("triggerChanged", [this](const ri::runtime::RuntimeEvent& event) {
        const std::string triggerId = GetFieldOrDefault(event, "triggerId", "trigger");
        const std::string state = GetFieldOrDefault(event, "state", "changed");
        state_.lastTriggerEvent = SummarizeHelperActivity(triggerId + ":" + state);
    });

    entityIoListener_ =
        eventBus.On(ri::runtime::entity_io::kEventType, [this](const ri::runtime::RuntimeEvent& event) {
            using ri::runtime::entity_io::kFieldInputName;
            using ri::runtime::entity_io::kFieldKind;
            using ri::runtime::entity_io::kFieldOutputName;
            using ri::runtime::entity_io::kFieldSourceId;
            using ri::runtime::entity_io::kFieldTargetId;
            using ri::runtime::entity_io::kFieldTimerId;

            std::string source = GetFieldOrDefault(event, kFieldSourceId);
            if (source.empty()) {
                source = GetFieldOrDefault(event, kFieldTargetId);
            }
            if (source.empty()) {
                source = GetFieldOrDefault(event, kFieldTimerId, "entity");
            }

            std::string detail = GetFieldOrDefault(event, kFieldInputName);
            if (detail.empty()) {
                detail = GetFieldOrDefault(event, kFieldOutputName);
            }
            if (detail.empty()) {
                detail = GetFieldOrDefault(event, kFieldKind, "signal");
            }

            state_.lastEntityIoEvent = SummarizeHelperActivity(source + ":" + detail);
        });

    messageListener_ = eventBus.On("message", [this](const ri::runtime::RuntimeEvent& event) {
        state_.lastMessage = SummarizeHelperActivity(GetFieldOrDefault(event, "text", "message"), 30);
    });

    levelLoadedListener_ = eventBus.On("levelLoaded", [this](const ri::runtime::RuntimeEvent& event) {
        std::string label = GetFieldOrDefault(event, "levelName");
        if (label.empty()) {
            label = GetFieldOrDefault(event, "levelFilename", "level");
        }
        state_.lastLevelEvent = SummarizeHelperActivity(label, 30);
    });

    schemaValidatedListener_ = eventBus.On("schemaValidated", [this](const ri::runtime::RuntimeEvent& event) {
        const std::string target = GetFieldOrDefault(event, "target", "level");
        const std::string result = ParseBooleanField(event, "ok") ? "ok" : "fail";
        state_.lastSchemaEvent = SummarizeHelperActivity(target + ":" + result, 30);
    });
}

void HelperActivityTracker::Detach() {
    RemoveListener(eventBus_, "audioChanged", audioChangedListener_);
    RemoveListener(eventBus_, "stateChanged", stateChangedListener_);
    RemoveListener(eventBus_, "triggerChanged", triggerChangedListener_);
    RemoveListener(eventBus_, ri::runtime::entity_io::kEventType, entityIoListener_);
    RemoveListener(eventBus_, "message", messageListener_);
    RemoveListener(eventBus_, "levelLoaded", levelLoadedListener_);
    RemoveListener(eventBus_, "schemaValidated", schemaValidatedListener_);
    eventBus_ = nullptr;
}

const HelperActivityState& HelperActivityTracker::GetState() const {
    return state_;
}

void HelperActivityTracker::Reset() {
    state_ = HelperActivityState{};
}

void EntityIoTracker::IncrementOutputsFired(std::size_t count) {
    stats_.outputsFired += count;
}

void EntityIoTracker::IncrementInputsDispatched(std::size_t count) {
    stats_.inputsDispatched += count;
}

void EntityIoTracker::IncrementTimersStarted(std::size_t count) {
    stats_.timersStarted += count;
}

void EntityIoTracker::IncrementTimersCancelled(std::size_t count) {
    stats_.timersCancelled += count;
}

void EntityIoTracker::IncrementCountersChanged(std::size_t count) {
    stats_.countersChanged += count;
}

void EntityIoTracker::RecordEvent(std::string_view kind,
                                  double timeSeconds,
                                  ri::runtime::RuntimeEventFields fields) {
    EntityIoHistoryEntry entry{};
    entry.id = ri::runtime::CreateRuntimeId("io");
    entry.timeSeconds = timeSeconds;
    entry.kind = std::string(kind);
    entry.fields = std::move(fields);

    history_.insert(history_.begin(), std::move(entry));
    if (history_.size() > kMaxHistory) {
        history_.resize(kMaxHistory);
    }
}

const EntityIoStats& EntityIoTracker::GetStats() const {
    return stats_;
}

const std::vector<EntityIoHistoryEntry>& EntityIoTracker::GetHistory() const {
    return history_;
}

void EntityIoTracker::Clear() {
    stats_ = EntityIoStats{};
    history_.clear();
}

void SpatialQueryTracker::RecordCollisionIndexBuild(double elapsedMs) {
    stats_.collisionIndexBuilds += 1;
    stats_.lastCollisionRebuildMs = elapsedMs;
}

void SpatialQueryTracker::RecordInteractionIndexBuild(double elapsedMs) {
    stats_.interactionIndexBuilds += 1;
    stats_.lastInteractionRebuildMs = elapsedMs;
}

void SpatialQueryTracker::RecordTriggerIndexBuild(double elapsedMs) {
    stats_.triggerIndexBuilds += 1;
    stats_.lastTriggerRebuildMs = elapsedMs;
}

void SpatialQueryTracker::RecordCollidableBoxQuery(std::size_t staticCandidates, std::size_t dynamicCandidates) {
    stats_.collidableBoxQueries += 1;
    stats_.staticCandidates += staticCandidates;
    stats_.dynamicCandidates += dynamicCandidates;
}

void SpatialQueryTracker::RecordCollidableRayQuery(std::size_t staticCandidates, std::size_t dynamicCandidates) {
    stats_.collidableRayQueries += 1;
    stats_.staticCandidates += staticCandidates;
    stats_.dynamicCandidates += dynamicCandidates;
}

void SpatialQueryTracker::RecordInteractiveRayQuery(std::size_t candidateCount) {
    stats_.interactiveRayQueries += 1;
    stats_.interactiveCandidates += candidateCount;
}

void SpatialQueryTracker::RecordTriggerPointQuery(std::size_t candidateCount, std::size_t scannedCandidateCount) {
    stats_.triggerPointQueries += 1;
    stats_.triggerCandidates += candidateCount;
    stats_.triggerCandidatesScanned += scannedCandidateCount;
}

const SpatialQueryStats& SpatialQueryTracker::GetStats() const {
    return stats_;
}

void SpatialQueryTracker::Clear() {
    stats_ = SpatialQueryStats{};
}

RuntimeStatsOverlayState::RuntimeStatsOverlayState(bool enabled)
    : enabled_(enabled) {}

void RuntimeStatsOverlayState::SetEnabled(bool enabled) {
    enabled_ = enabled;
}

void RuntimeStatsOverlayState::SetAttached(bool attached) {
    attached_ = attached;
}

void RuntimeStatsOverlayState::SetVisible(bool visible) {
    visible_ = visible;
}

void RuntimeStatsOverlayState::RecordFrameDeltaSeconds(double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) {
        return;
    }

    const double sampleMs = deltaSeconds * 1000.0;
    if (!hasFrameSample_) {
        frameTimeMs_ = sampleMs;
        hasFrameSample_ = true;
    } else {
        frameTimeMs_ = (frameTimeMs_ * 0.85) + (sampleMs * 0.15);
    }
    framesPerSecond_ = frameTimeMs_ > 0.0 ? (1000.0 / frameTimeMs_) : 0.0;
}

RuntimeStatsOverlayMetrics RuntimeStatsOverlayState::GetMetrics() const {
    return RuntimeStatsOverlayMetrics{
        .enabled = enabled_,
        .attached = attached_,
        .visible = visible_,
        .frameTimeMs = frameTimeMs_,
        .framesPerSecond = framesPerSecond_,
    };
}

} // namespace ri::world
