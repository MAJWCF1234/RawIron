#pragma once

#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/World/InfoPanel.h"
#include "RawIron/World/RuntimeState.h"

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ri::world {

struct EntityIoStats {
    std::size_t outputsFired = 0;
    std::size_t inputsDispatched = 0;
    std::size_t timersStarted = 0;
    std::size_t timersCancelled = 0;
    std::size_t countersChanged = 0;
};

struct EntityIoHistoryEntry {
    std::string id;
    double timeSeconds = 0.0;
    std::string kind;
    ri::runtime::RuntimeEventFields fields;
};

struct SpatialQueryStats {
    std::size_t collisionIndexBuilds = 0;
    std::size_t interactionIndexBuilds = 0;
    std::size_t triggerIndexBuilds = 0;
    std::size_t collidableBoxQueries = 0;
    std::size_t collidableRayQueries = 0;
    std::size_t interactiveRayQueries = 0;
    std::size_t triggerPointQueries = 0;
    std::size_t staticCandidates = 0;
    std::size_t dynamicCandidates = 0;
    std::size_t interactiveCandidates = 0;
    std::size_t triggerCandidates = 0;
    std::size_t triggerCandidatesScanned = 0;
    double lastCollisionRebuildMs = 0.0;
    double lastInteractionRebuildMs = 0.0;
    double lastTriggerRebuildMs = 0.0;
};

struct InfoPanelBindingContext {
    const LogicEntityStateMap* logicEntities = nullptr;
    const std::set<std::string>* worldFlags = nullptr;
    const std::unordered_map<std::string, double>* worldValues = nullptr;
    const RuntimeHelperMetricsSnapshot* helperMetrics = nullptr;
    RuntimeInfoPanelCounts runtimeCounts{};
};

struct RuntimeStatsOverlaySnapshot {
    RuntimeStatsOverlayMetrics metrics{};
    std::size_t sceneNodes = 0;
    std::size_t rootNodes = 0;
    std::size_t renderables = 0;
    std::size_t lights = 0;
    std::size_t cameras = 0;
    std::size_t selectedNode = 0;
    std::string modeLabel = "runtime";
    std::string sceneLabel = "workspace";
};

[[nodiscard]] std::string FormatInfoPanelValue(const InfoPanelValue& value);
[[nodiscard]] InfoPanelValue ResolveInfoPanelBindingValue(const InfoPanelBinding& binding,
                                                          const InfoPanelBindingContext& context);
[[nodiscard]] std::vector<std::string> ResolveInfoPanelLines(const InfoPanelDefinition& panel,
                                                             const InfoPanelBindingContext& context);
[[nodiscard]] std::vector<std::string> FormatRuntimeStatsOverlayLines(const RuntimeStatsOverlaySnapshot& snapshot,
                                                                      std::size_t maxLines = 5);

/// Tracks dynamic in-world info panel refresh/caching so creators can bind panel lines to live runtime state.
class DynamicInfoPanelSubsystem {
public:
    void SetSpawners(std::vector<DynamicInfoPanelSpawner> spawners);
    void Clear();
    [[nodiscard]] std::vector<DynamicInfoPanelRenderState> UpdateAndResolve(
        double nowSeconds,
        const InfoPanelBindingContext& context);

private:
    struct CacheEntry {
        DynamicInfoPanelRenderState state{};
        double nextRefreshSeconds = 0.0;
    };

    std::vector<DynamicInfoPanelSpawner> spawners_{};
    std::unordered_map<std::string, CacheEntry> cache_{};
};

/// Subscribes to `RuntimeEventBus` helper-facing channels and maintains `HelperActivityState`.
/// Umbrella concept: Documentation/02 Engine/Helper Telemetry Bus.md — discoverability header `HelperTelemetryBus.h`.
class HelperActivityTracker {
public:
    HelperActivityTracker() = default;
    ~HelperActivityTracker();

    void Attach(ri::runtime::RuntimeEventBus& eventBus);
    void Detach();
    [[nodiscard]] const HelperActivityState& GetState() const;
    void Reset();

private:
    ri::runtime::RuntimeEventBus* eventBus_ = nullptr;
    HelperActivityState state_{};
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> audioChangedListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> stateChangedListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> triggerChangedListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> entityIoListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> messageListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> levelLoadedListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> schemaValidatedListener_;
};

/// Ring buffer + counters for logic I/O tooling; field contract for bus emits is documented in `Documentation/02 Engine/Entity IO and Logic Graph.md`.
class EntityIoTracker {
public:
    void IncrementOutputsFired(std::size_t count = 1);
    void IncrementInputsDispatched(std::size_t count = 1);
    void IncrementTimersStarted(std::size_t count = 1);
    void IncrementTimersCancelled(std::size_t count = 1);
    void IncrementCountersChanged(std::size_t count = 1);

    void RecordEvent(std::string_view kind,
                     double timeSeconds,
                     ri::runtime::RuntimeEventFields fields = {});

    [[nodiscard]] const EntityIoStats& GetStats() const;
    [[nodiscard]] const std::vector<EntityIoHistoryEntry>& GetHistory() const;
    void Clear();

private:
    static constexpr std::size_t kMaxHistory = 14;

    EntityIoStats stats_{};
    std::vector<EntityIoHistoryEntry> history_;
};

class SpatialQueryTracker {
public:
    void RecordCollisionIndexBuild(double elapsedMs);
    void RecordInteractionIndexBuild(double elapsedMs);
    void RecordTriggerIndexBuild(double elapsedMs);
    void RecordCollidableBoxQuery(std::size_t staticCandidates, std::size_t dynamicCandidates = 0);
    void RecordCollidableRayQuery(std::size_t staticCandidates, std::size_t dynamicCandidates = 0);
    void RecordInteractiveRayQuery(std::size_t candidateCount);
    void RecordTriggerPointQuery(std::size_t candidateCount, std::size_t scannedCandidateCount = 0);

    [[nodiscard]] const SpatialQueryStats& GetStats() const;
    void Clear();

private:
    SpatialQueryStats stats_{};
};

class RuntimeStatsOverlayState {
public:
    explicit RuntimeStatsOverlayState(bool enabled = false);

    void SetEnabled(bool enabled);
    void SetAttached(bool attached);
    void SetVisible(bool visible);
    void RecordFrameDeltaSeconds(double deltaSeconds);
    [[nodiscard]] RuntimeStatsOverlayMetrics GetMetrics() const;

private:
    bool enabled_ = false;
    bool attached_ = false;
    bool visible_ = false;
    double frameTimeMs_ = 0.0;
    double framesPerSecond_ = 0.0;
    bool hasFrameSample_ = false;
};

} // namespace ri::world
