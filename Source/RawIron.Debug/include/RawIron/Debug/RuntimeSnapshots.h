#pragma once

#include "RawIron/Audio/AudioManager.h"
#include "RawIron/Events/EventEngine.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/Spatial/SpatialIndex.h"
#include "RawIron/Structural/StructuralGraph.h"
#include "RawIron/Trace/TraceScene.h"
#include "RawIron/Validation/Schemas.h"

#include <optional>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::debug {

struct DebugRecentActivity {
    std::string lastLevelEvent;
    std::string lastSchemaEvent;
    std::string lastMessage;
    std::string lastEntityIoEvent;
};

struct HelperLibraryMetricsSnapshot {
    std::size_t schemaValidations = 0;
    std::size_t schemaValidationFailures = 0;
    std::size_t tuningParses = 0;
    std::size_t tuningParseFailures = 0;
    std::size_t eventBusEmits = 0;
    std::size_t eventBusListeners = 0;
    std::size_t eventBusListenersAdded = 0;
    std::size_t eventBusListenersRemoved = 0;
    std::size_t audioManagedSounds = 0;
    std::size_t audioLoopsCreated = 0;
    std::size_t audioOneShotsPlayed = 0;
    std::size_t audioVoicesPlayed = 0;
    bool audioVoiceActive = false;
    std::string audioEnvironment = "none";
    std::size_t audioEnvironmentChanges = 0;
    double audioEnvironmentMix = 0.0;
    std::string runtimeSession;
    std::string lastLevelEvent;
    std::string lastSchemaEvent;
    std::string lastMessage;
    std::string lastEntityIoEvent;
};

struct DebugNamedNumber {
    std::string key;
    double value = 0.0;
};

struct EventEngineDebugState {
    std::size_t eventCount = 0;
    std::size_t activeRuntimeStates = 0;
    std::size_t scheduledTimers = 0;
    std::vector<std::string> worldFlags;
    std::vector<DebugNamedNumber> worldValues;
};

struct SpatialIndexDebugSnapshot {
    std::size_t entryCount = 0;
    std::optional<ri::spatial::Aabb> bounds;
};

struct SpatialDebugSnapshot {
    SpatialIndexDebugSnapshot collisionIndex;
    SpatialIndexDebugSnapshot structuralIndex;
    std::size_t traceColliderCount = 0;
    std::size_t staticTraceColliderCount = 0;
    std::size_t structuralStaticTraceColliderCount = 0;
    std::size_t dynamicTraceColliderCount = 0;
    std::size_t boxQueries = 0;
    std::size_t rayQueries = 0;
    std::size_t traceBoxQueries = 0;
    std::size_t traceRayQueries = 0;
    std::size_t sweptBoxQueries = 0;
    std::size_t staticCandidates = 0;
    std::size_t dynamicCandidates = 0;
};

struct RenderEnvironmentSnapshot {
    std::string label;
    std::vector<std::string> activeVolumes;
    double mix = 0.0;
    double delayMs = 0.0;
    double tintStrength = 0.0;
    double blurAmount = 0.0;
};

struct EngineCountSnapshot {
    std::size_t sceneChildren = 0;
    std::size_t collidables = 0;
    std::size_t structuralCollidables = 0;
    std::size_t bvhMeshes = 0;
    std::size_t interactives = 0;
    std::size_t physicsObjects = 0;
    std::size_t levelEvents = 0;
    std::size_t triggerVolumes = 0;
    std::size_t logicEntities = 0;
    std::size_t staticCollidables = 0;
    std::size_t dynamicCollidables = 0;
    std::size_t staticInteractives = 0;
    std::size_t dynamicInteractives = 0;
    bool debugHelpers = false;
};

struct InteractiveProximityInput {
    std::string id;
    std::string label;
    ri::math::Vec3 position{};
};

struct NearbyInteractiveSnapshot {
    std::string id;
    std::string label;
    double distance = 0.0;
    ri::math::Vec3 position{};
};

struct ActorProximityInput {
    std::string kind;
    std::string id;
    std::string state;
    std::string animation;
    ri::math::Vec3 position{};
};

struct ActorSnapshotEntry {
    std::string kind;
    std::string id;
    std::string state;
    std::string animation;
    double distance = 0.0;
    ri::math::Vec3 position{};
};

struct RenderGameStateSnapshot {
    std::string mode = "runtime";
    std::string level;
    bool paused = false;
    EngineCountSnapshot counts;
    HelperLibraryMetricsSnapshot helperLibraries;
    std::optional<RenderEnvironmentSnapshot> audioEnvironment;
    std::optional<RenderEnvironmentSnapshot> postProcessEnvironment;
    std::string coordinateSystem;
    std::vector<NearbyInteractiveSnapshot> nearbyInteractives;
    std::vector<ActorSnapshotEntry> actors;
    std::unordered_map<std::string, std::string> detailFields;
};

struct RenderGameStateBuildInput {
    std::string mode = "runtime";
    std::string level;
    bool paused = false;
    EngineCountSnapshot counts;
    HelperLibraryMetricsSnapshot helperLibraries;
    std::optional<RenderEnvironmentSnapshot> audioEnvironment;
    std::optional<RenderEnvironmentSnapshot> postProcessEnvironment;
    std::string coordinateSystem;
    std::vector<NearbyInteractiveSnapshot> nearbyInteractives;
    std::vector<ActorSnapshotEntry> actors;
    std::unordered_map<std::string, std::string> detailFields;
};

struct RuntimeDebugSnapshot {
    std::string timestamp;
    std::string sessionId;
    EngineCountSnapshot scene;
    EventEngineDebugState eventEngine;
    std::optional<SpatialDebugSnapshot> spatial;
    HelperLibraryMetricsSnapshot helperLibraries;
    std::optional<RenderEnvironmentSnapshot> audioEnvironment;
    std::optional<RenderEnvironmentSnapshot> postProcessEnvironment;
    std::optional<ri::structural::StructuralGraphSummary> structuralGraph;
};

struct ProofBoardPieceStatus {
    std::string pieceId;
    bool present = false;
    std::size_t count = 0;
};

struct ProofBoardSnapshot {
    bool towerStanding = false;
    std::vector<ProofBoardPieceStatus> pieces;
};

struct RenderGameStateDiffEntry {
    std::string field;
    std::string before;
    std::string after;
};

struct RenderGameStateDiff {
    std::vector<RenderGameStateDiffEntry> changes;
};

struct RenderGameStateTimelineEntry {
    std::string timestamp;
    RenderGameStateSnapshot snapshot;
};

[[nodiscard]] HelperLibraryMetricsSnapshot BuildHelperLibraryMetricsSnapshot(
    std::string_view runtimeSession,
    const ri::runtime::RuntimeEventBusMetrics& eventBusMetrics,
    const ri::validation::SchemaValidationMetrics& schemaMetrics,
    const ri::audio::AudioManagerMetrics& audioMetrics,
    const DebugRecentActivity& recentActivity = {});
[[nodiscard]] EventEngineDebugState BuildEventEngineDebugState(const ri::events::EventEngine& engine);
[[nodiscard]] SpatialIndexDebugSnapshot BuildSpatialIndexDebugSnapshot(const ri::spatial::BspSpatialIndex& index);
[[nodiscard]] SpatialDebugSnapshot BuildSpatialDebugSnapshot(const ri::trace::TraceScene& traceScene,
                                                             const ri::spatial::BspSpatialIndex& collisionIndex,
                                                             const ri::spatial::BspSpatialIndex& structuralIndex);
[[nodiscard]] std::string FormatRenderGameStateSnapshot(const RenderGameStateSnapshot& snapshot);
[[nodiscard]] RenderGameStateSnapshot BuildRenderGameStateSnapshot(const RenderGameStateBuildInput& input);
[[nodiscard]] std::string FormatRuntimeDebugReport(const RuntimeDebugSnapshot& snapshot);

[[nodiscard]] std::vector<NearbyInteractiveSnapshot> SelectNearestInteractivesForSnapshot(
    const ri::math::Vec3& observer,
    std::span<const InteractiveProximityInput> candidates,
    std::size_t maxCount,
    int positionDecimalPlaces = 3,
    int distanceDecimalPlaces = 3);

[[nodiscard]] std::vector<ActorSnapshotEntry> SelectNearestActorsForSnapshot(
    const ri::math::Vec3& observer,
    std::span<const ActorProximityInput> candidates,
    std::size_t maxCount,
    int positionDecimalPlaces = 3,
    int distanceDecimalPlaces = 3);

/// Stable machine-readable payload for tooling and automation snapshots.
[[nodiscard]] std::string FormatRenderGameStateSnapshotJson(const RenderGameStateSnapshot& snapshot);
[[nodiscard]] bool ExportRenderGameStateSnapshot(const RenderGameStateSnapshot& snapshot,
                                                 const std::filesystem::path& outputPath,
                                                 bool asJson,
                                                 std::string* error = nullptr);
[[nodiscard]] bool ExportRuntimeDebugSnapshot(const RuntimeDebugSnapshot& snapshot,
                                              const std::filesystem::path& outputPath,
                                              std::string* error = nullptr);
[[nodiscard]] ProofBoardSnapshot BuildProofBoardSnapshot(const EngineCountSnapshot& counts,
                                                         std::span<const std::string_view> requiredPieces);
[[nodiscard]] std::string FormatProofBoardSnapshot(const ProofBoardSnapshot& snapshot);
[[nodiscard]] RenderGameStateDiff BuildRenderGameStateDiff(const RenderGameStateSnapshot& before,
                                                           const RenderGameStateSnapshot& after);
[[nodiscard]] std::string FormatRenderGameStateDiff(const RenderGameStateDiff& diff);
void AppendRenderGameStateTimelineEntry(std::vector<RenderGameStateTimelineEntry>& timeline,
                                        RenderGameStateTimelineEntry entry,
                                        std::size_t maxEntries = 256);

} // namespace ri::debug
