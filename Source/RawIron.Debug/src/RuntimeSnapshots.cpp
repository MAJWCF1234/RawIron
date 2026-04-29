// Proximity snapshot rounding: `SnapshotFormatting.h` (kept in sync with `engine/snapshotRoundShim.js`).
#include "RawIron/Debug/RuntimeSnapshots.h"
#include "RawIron/Debug/SnapshotFormatting.h"

#include "RawIron/Math/Vec3.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ri::debug {
namespace {

std::string JoinStrings(const std::vector<std::string>& values) {
    if (values.empty()) {
        return "none";
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << ", ";
        }
        stream << values[index];
    }
    return stream.str();
}

std::string FormatDouble(double value, int precision = 2) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << (std::isfinite(value) ? value : 0.0);
    return stream.str();
}

std::string FormatAabb(const ri::spatial::Aabb& bounds) {
    std::ostringstream stream;
    stream << "min=" << ri::math::ToString(bounds.min) << " max=" << ri::math::ToString(bounds.max);
    return stream.str();
}

std::string JsonEscape(std::string_view in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const char c : in) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[12];
                std::snprintf(buf,
                              sizeof(buf),
                              "\\u%04x",
                              static_cast<unsigned int>(static_cast<unsigned char>(c)));
                out += buf;
            } else {
                out += c;
            }
        }
    }
    return out;
}

template<typename Row, typename Compare>
void SelectTopKNearest(std::vector<Row>& rows, std::size_t maxCount, Compare compare) {
    if (rows.empty() || maxCount == 0) {
        rows.clear();
        return;
    }
    if (rows.size() <= maxCount) {
        std::sort(rows.begin(), rows.end(), compare);
        return;
    }
    std::partial_sort(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(maxCount), rows.end(), compare);
    rows.resize(maxCount);
    std::sort(rows.begin(), rows.end(), compare);
}

void AppendEnvironmentLine(std::ostringstream& report,
                           std::string_view label,
                           const std::optional<RenderEnvironmentSnapshot>& environment) {
    if (!environment.has_value()) {
        report << label << ": none\n";
        return;
    }

    report << label << ": " << environment->label
           << " | volumes=" << JoinStrings(environment->activeVolumes)
           << " | mix=" << FormatDouble(environment->mix, 3)
           << " | delayMs=" << FormatDouble(environment->delayMs, 1);
    if (environment->tintStrength > 0.0 || environment->blurAmount > 0.0) {
        report << " | tint=" << FormatDouble(environment->tintStrength, 3)
               << " | blur=" << FormatDouble(environment->blurAmount, 4);
    }
    report << '\n';
}

} // namespace

HelperLibraryMetricsSnapshot BuildHelperLibraryMetricsSnapshot(
    std::string_view runtimeSession,
    const ri::runtime::RuntimeEventBusMetrics& eventBusMetrics,
    const ri::validation::SchemaValidationMetrics& schemaMetrics,
    const ri::audio::AudioManagerMetrics& audioMetrics,
    const DebugRecentActivity& recentActivity) {
    HelperLibraryMetricsSnapshot snapshot{};
    snapshot.schemaValidations = schemaMetrics.levelValidations;
    snapshot.schemaValidationFailures = schemaMetrics.levelValidationFailures;
    snapshot.tuningParses = schemaMetrics.tuningParses;
    snapshot.tuningParseFailures = schemaMetrics.tuningParseFailures;
    snapshot.eventBusEmits = eventBusMetrics.emitted;
    snapshot.eventBusListeners = eventBusMetrics.activeListeners;
    snapshot.eventBusListenersAdded = eventBusMetrics.listenersAdded;
    snapshot.eventBusListenersRemoved = eventBusMetrics.listenersRemoved;
    snapshot.audioManagedSounds = audioMetrics.managedSounds;
    snapshot.audioLoopsCreated = audioMetrics.loopsCreated;
    snapshot.audioOneShotsPlayed = audioMetrics.oneShotsPlayed;
    snapshot.audioVoicesPlayed = audioMetrics.voicesPlayed;
    snapshot.audioVoiceActive = audioMetrics.voiceActive;
    snapshot.audioEnvironment = audioMetrics.activeEnvironment;
    snapshot.audioEnvironmentChanges = audioMetrics.environmentChanges;
    snapshot.audioEnvironmentMix = audioMetrics.activeEnvironmentMix;
    snapshot.runtimeSession = std::string(runtimeSession);
    snapshot.lastLevelEvent = recentActivity.lastLevelEvent;
    snapshot.lastSchemaEvent = recentActivity.lastSchemaEvent;
    snapshot.lastMessage = recentActivity.lastMessage;
    snapshot.lastEntityIoEvent = recentActivity.lastEntityIoEvent;
    return snapshot;
}

EventEngineDebugState BuildEventEngineDebugState(const ri::events::EventEngine& engine) {
    EventEngineDebugState state{};
    state.eventCount = engine.GetEvents().size();
    state.activeRuntimeStates = engine.GetEventRuntimeStates().size();
    state.scheduledTimers = engine.ScheduledTimerCount();

    state.worldFlags.assign(engine.GetWorldFlags().begin(), engine.GetWorldFlags().end());
    std::sort(state.worldFlags.begin(), state.worldFlags.end());

    state.worldValues.reserve(engine.GetWorldValues().size());
    for (const auto& [key, value] : engine.GetWorldValues()) {
        state.worldValues.push_back(DebugNamedNumber{.key = key, .value = value});
    }
    std::sort(state.worldValues.begin(), state.worldValues.end(), [](const DebugNamedNumber& lhs, const DebugNamedNumber& rhs) {
        return lhs.key < rhs.key;
    });

    return state;
}

SpatialIndexDebugSnapshot BuildSpatialIndexDebugSnapshot(const ri::spatial::BspSpatialIndex& index) {
    SpatialIndexDebugSnapshot snapshot{};
    snapshot.entryCount = index.EntryCount();
    if (!index.Empty()) {
        const ri::spatial::Aabb bounds = index.Bounds();
        if (!ri::spatial::IsEmpty(bounds)) {
            snapshot.bounds = bounds;
        }
    }
    return snapshot;
}

SpatialDebugSnapshot BuildSpatialDebugSnapshot(const ri::trace::TraceScene& traceScene,
                                               const ri::spatial::BspSpatialIndex& collisionIndex,
                                               const ri::spatial::BspSpatialIndex& structuralIndex) {
    const ri::trace::TraceSceneMetrics metrics = traceScene.Metrics();
    return SpatialDebugSnapshot{
        .collisionIndex = BuildSpatialIndexDebugSnapshot(collisionIndex),
        .structuralIndex = BuildSpatialIndexDebugSnapshot(structuralIndex),
        .traceColliderCount = metrics.colliderCount,
        .staticTraceColliderCount = metrics.staticColliderCount,
        .structuralStaticTraceColliderCount = metrics.structuralStaticColliderCount,
        .dynamicTraceColliderCount = metrics.dynamicColliderCount,
        .boxQueries = metrics.boxQueries,
        .rayQueries = metrics.rayQueries,
        .traceBoxQueries = metrics.traceBoxQueries,
        .traceRayQueries = metrics.traceRayQueries,
        .sweptBoxQueries = metrics.sweptBoxQueries,
        .staticCandidates = metrics.staticCandidates,
        .dynamicCandidates = metrics.dynamicCandidates,
    };
}

std::vector<NearbyInteractiveSnapshot> SelectNearestInteractivesForSnapshot(
    const ri::math::Vec3& observer,
    std::span<const InteractiveProximityInput> candidates,
    std::size_t maxCount,
    int positionDecimalPlaces,
    int distanceDecimalPlaces) {
    struct Row {
        const InteractiveProximityInput* source = nullptr;
        float distance_squared = 0.0f;
    };

    std::vector<Row> rows;
    rows.reserve(candidates.size());
    for (const InteractiveProximityInput& candidate : candidates) {
        rows.push_back(Row{
            .source = &candidate,
            .distance_squared = ri::math::DistanceSquared(observer, candidate.position),
        });
    }

    SelectTopKNearest(rows, maxCount, [](const Row& lhs, const Row& rhs) {
        if (lhs.distance_squared != rhs.distance_squared) {
            return lhs.distance_squared < rhs.distance_squared;
        }
        return lhs.source->id < rhs.source->id;
    });

    std::vector<NearbyInteractiveSnapshot> result;
    result.reserve(rows.size());
    for (const Row& row : rows) {
        const double distance = std::sqrt(static_cast<double>(row.distance_squared));
        result.push_back(NearbyInteractiveSnapshot{
            .id = row.source->id,
            .label = row.source->label,
            .distance = RoundSnapshotScalar(distance, distanceDecimalPlaces),
            .position = RoundSnapshotVec3(row.source->position, positionDecimalPlaces),
        });
    }
    return result;
}

std::vector<ActorSnapshotEntry> SelectNearestActorsForSnapshot(
    const ri::math::Vec3& observer,
    std::span<const ActorProximityInput> candidates,
    std::size_t maxCount,
    int positionDecimalPlaces,
    int distanceDecimalPlaces) {
    struct Row {
        const ActorProximityInput* source = nullptr;
        float distance_squared = 0.0f;
    };

    std::vector<Row> rows;
    rows.reserve(candidates.size());
    for (const ActorProximityInput& candidate : candidates) {
        rows.push_back(Row{
            .source = &candidate,
            .distance_squared = ri::math::DistanceSquared(observer, candidate.position),
        });
    }

    SelectTopKNearest(rows, maxCount, [](const Row& lhs, const Row& rhs) {
        if (lhs.distance_squared != rhs.distance_squared) {
            return lhs.distance_squared < rhs.distance_squared;
        }
        if (lhs.source->id != rhs.source->id) {
            return lhs.source->id < rhs.source->id;
        }
        return lhs.source->kind < rhs.source->kind;
    });

    std::vector<ActorSnapshotEntry> result;
    result.reserve(rows.size());
    for (const Row& row : rows) {
        const double distance = std::sqrt(static_cast<double>(row.distance_squared));
        result.push_back(ActorSnapshotEntry{
            .kind = row.source->kind,
            .id = row.source->id,
            .state = row.source->state,
            .animation = row.source->animation,
            .distance = RoundSnapshotScalar(distance, distanceDecimalPlaces),
            .position = RoundSnapshotVec3(row.source->position, positionDecimalPlaces),
        });
    }
    return result;
}

std::string FormatRenderGameStateSnapshot(const RenderGameStateSnapshot& snapshot) {
    std::ostringstream report;
    report << "mode: " << (snapshot.mode.empty() ? "runtime" : snapshot.mode) << '\n';
    report << "level: " << (snapshot.level.empty() ? "none" : snapshot.level) << '\n';
    report << "paused: " << (snapshot.paused ? "true" : "false") << '\n';
    report << "collidables: " << snapshot.counts.collidables << '\n';
    report << "structural_collidables: " << snapshot.counts.structuralCollidables << '\n';
    report << "interactives: " << snapshot.counts.interactives << '\n';
    report << "physics_objects: " << snapshot.counts.physicsObjects << '\n';
    report << "trigger_volumes: " << snapshot.counts.triggerVolumes << '\n';
    report << "logic_entities: " << snapshot.counts.logicEntities << '\n';
    report << "audio_environment: " << (snapshot.audioEnvironment.has_value() ? snapshot.audioEnvironment->label : "none") << '\n';
    report << "runtime_session: " << (snapshot.helperLibraries.runtimeSession.empty() ? "unknown" : snapshot.helperLibraries.runtimeSession) << '\n';
    report << "schema_validations: " << snapshot.helperLibraries.schemaValidations << '\n';
    report << "event_bus_emits: " << snapshot.helperLibraries.eventBusEmits << '\n';
    report << "audio_managed: " << snapshot.helperLibraries.audioManagedSounds << '\n';
    report << "coordinate_system: "
           << (snapshot.coordinateSystem.empty() ? "rawiron native world coordinates" : snapshot.coordinateSystem) << '\n';

    if (!snapshot.nearbyInteractives.empty()) {
        report << "nearby_interactives:\n";
        for (const NearbyInteractiveSnapshot& entry : snapshot.nearbyInteractives) {
            report << "  - id=" << entry.id << " label=" << entry.label << " distance=" << FormatDouble(entry.distance, 3)
                   << " position=(" << FormatDouble(static_cast<double>(entry.position.x), 3) << ", "
                   << FormatDouble(static_cast<double>(entry.position.y), 3) << ", "
                   << FormatDouble(static_cast<double>(entry.position.z), 3) << ")\n";
        }
    }

    if (!snapshot.actors.empty()) {
        report << "actors:\n";
        for (const ActorSnapshotEntry& entry : snapshot.actors) {
            report << "  - kind=" << entry.kind << " id=" << entry.id << " state=" << entry.state
                   << " animation=" << (entry.animation.empty() ? "none" : entry.animation)
                   << " distance=" << FormatDouble(entry.distance, 3)
                   << " position=(" << FormatDouble(static_cast<double>(entry.position.x), 3) << ", "
                   << FormatDouble(static_cast<double>(entry.position.y), 3) << ", "
                   << FormatDouble(static_cast<double>(entry.position.z), 3) << ")\n";
        }
    }
    if (!snapshot.detailFields.empty()) {
        std::vector<std::pair<std::string, std::string>> ordered;
        ordered.reserve(snapshot.detailFields.size());
        for (const auto& [key, value] : snapshot.detailFields) {
            ordered.emplace_back(key, value);
        }
        std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });
        report << "details:\n";
        for (const auto& [key, value] : ordered) {
            report << "  - " << key << "=" << value << '\n';
        }
    }

    return report.str();
}

RenderGameStateSnapshot BuildRenderGameStateSnapshot(const RenderGameStateBuildInput& input) {
    RenderGameStateSnapshot snapshot{};
    snapshot.mode = input.mode.empty() ? "runtime" : input.mode;
    snapshot.level = input.level;
    snapshot.paused = input.paused;
    snapshot.counts = input.counts;
    snapshot.helperLibraries = input.helperLibraries;
    snapshot.audioEnvironment = input.audioEnvironment;
    snapshot.postProcessEnvironment = input.postProcessEnvironment;
    snapshot.coordinateSystem = input.coordinateSystem;
    snapshot.nearbyInteractives = input.nearbyInteractives;
    snapshot.actors = input.actors;
    snapshot.detailFields = input.detailFields;

    snapshot.detailFields.emplace("eventBus.activeListeners",
                                  std::to_string(snapshot.helperLibraries.eventBusListeners));
    snapshot.detailFields.emplace("eventBus.emitted",
                                  std::to_string(snapshot.helperLibraries.eventBusEmits));
    snapshot.detailFields.emplace("schema.validations",
                                  std::to_string(snapshot.helperLibraries.schemaValidations));
    snapshot.detailFields.emplace("scene.collidables",
                                  std::to_string(snapshot.counts.collidables));
    return snapshot;
}

std::string FormatRuntimeDebugReport(const RuntimeDebugSnapshot& snapshot) {
    std::ostringstream report;
    report << "== RAWIRON DEBUG REPORT ==\n";
    report << "Timestamp: " << (snapshot.timestamp.empty() ? "unknown" : snapshot.timestamp) << '\n';
    report << "Session: " << (snapshot.sessionId.empty() ? "unknown" : snapshot.sessionId) << "\n\n";

    report << "== SCENE ==\n";
    report << "Scene Children: " << snapshot.scene.sceneChildren << '\n';
    report << "Collidables: " << snapshot.scene.collidables << '\n';
    report << "Structural Collidables: " << snapshot.scene.structuralCollidables << '\n';
    report << "BVH Meshes: " << snapshot.scene.bvhMeshes << '\n';
    report << "Interactives: " << snapshot.scene.interactives << '\n';
    report << "Physics Objects: " << snapshot.scene.physicsObjects << '\n';
    report << "Level Events: " << snapshot.scene.levelEvents << '\n';
    report << "Trigger Volumes: " << snapshot.scene.triggerVolumes << '\n';
    report << "Logic Entities: " << snapshot.scene.logicEntities << '\n';
    report << "Debug Helpers: " << (snapshot.scene.debugHelpers ? "true" : "false") << '\n';
    report << "Static Collidables: " << snapshot.scene.staticCollidables
           << " | Dynamic Collidables: " << snapshot.scene.dynamicCollidables << '\n';
    report << "Static Interactives: " << snapshot.scene.staticInteractives
           << " | Dynamic Interactives: " << snapshot.scene.dynamicInteractives << "\n\n";

    report << "== EVENT ENGINE ==\n";
    report << "Events: " << snapshot.eventEngine.eventCount
           << " | Runtime States: " << snapshot.eventEngine.activeRuntimeStates
           << " | Scheduled Timers: " << snapshot.eventEngine.scheduledTimers << '\n';
    report << "World Flags: " << JoinStrings(snapshot.eventEngine.worldFlags) << '\n';
    report << "World Values: ";
    if (snapshot.eventEngine.worldValues.empty()) {
        report << "none\n\n";
    } else {
        for (std::size_t index = 0; index < snapshot.eventEngine.worldValues.size(); ++index) {
            if (index > 0) {
                report << ", ";
            }
            report << snapshot.eventEngine.worldValues[index].key << '=' << FormatDouble(snapshot.eventEngine.worldValues[index].value, 2);
        }
        report << "\n\n";
    }

    report << "== HELPER LIBRARIES ==\n";
    report << "Schema validations: " << snapshot.helperLibraries.schemaValidations
           << " | Failures: " << snapshot.helperLibraries.schemaValidationFailures
           << " | Tuning parses: " << snapshot.helperLibraries.tuningParses
           << " | Tuning parse failures: " << snapshot.helperLibraries.tuningParseFailures << '\n';
    report << "Event bus emits: " << snapshot.helperLibraries.eventBusEmits
           << " | Active listeners: " << snapshot.helperLibraries.eventBusListeners
           << " | Added listeners: " << snapshot.helperLibraries.eventBusListenersAdded
           << " | Removed listeners: " << snapshot.helperLibraries.eventBusListenersRemoved << '\n';
    report << "Audio managed: " << snapshot.helperLibraries.audioManagedSounds
           << " | Loops: " << snapshot.helperLibraries.audioLoopsCreated
           << " | One-shots: " << snapshot.helperLibraries.audioOneShotsPlayed
           << " | Voices: " << snapshot.helperLibraries.audioVoicesPlayed
           << " | Voice active: " << (snapshot.helperLibraries.audioVoiceActive ? "true" : "false") << '\n';
    report << "Audio environment: " << snapshot.helperLibraries.audioEnvironment
           << " | Changes: " << snapshot.helperLibraries.audioEnvironmentChanges
           << " | Mix: " << FormatDouble(snapshot.helperLibraries.audioEnvironmentMix, 2) << '\n';
    report << "Session label: " << (snapshot.helperLibraries.runtimeSession.empty() ? "unknown" : snapshot.helperLibraries.runtimeSession) << '\n';
    report << "Recent activity: level=" << (snapshot.helperLibraries.lastLevelEvent.empty() ? "none" : snapshot.helperLibraries.lastLevelEvent)
           << " | schema=" << (snapshot.helperLibraries.lastSchemaEvent.empty() ? "none" : snapshot.helperLibraries.lastSchemaEvent)
           << " | message=" << (snapshot.helperLibraries.lastMessage.empty() ? "none" : snapshot.helperLibraries.lastMessage)
           << " | entity_io=" << (snapshot.helperLibraries.lastEntityIoEvent.empty() ? "none" : snapshot.helperLibraries.lastEntityIoEvent)
           << "\n\n";

    report << "== ENVIRONMENTS ==\n";
    AppendEnvironmentLine(report, "Audio", snapshot.audioEnvironment);
    AppendEnvironmentLine(report, "PostProcess", snapshot.postProcessEnvironment);
    report << '\n';

    report << "== SPATIAL ==\n";
    if (!snapshot.spatial.has_value()) {
        report << "Spatial snapshot unavailable\n\n";
    } else {
        report << "Collision Index Entries: " << snapshot.spatial->collisionIndex.entryCount << '\n';
        report << "Collision Index Bounds: "
               << (snapshot.spatial->collisionIndex.bounds.has_value() ? FormatAabb(*snapshot.spatial->collisionIndex.bounds) : std::string("empty"))
               << '\n';
        report << "Structural Index Entries: " << snapshot.spatial->structuralIndex.entryCount << '\n';
        report << "Structural Index Bounds: "
               << (snapshot.spatial->structuralIndex.bounds.has_value() ? FormatAabb(*snapshot.spatial->structuralIndex.bounds) : std::string("empty"))
               << '\n';
        report << "Trace Colliders: " << snapshot.spatial->traceColliderCount
               << " (static=" << snapshot.spatial->staticTraceColliderCount
               << ", structural_static=" << snapshot.spatial->structuralStaticTraceColliderCount
               << ", dynamic=" << snapshot.spatial->dynamicTraceColliderCount << ")\n";
        report << "Trace Queries: box=" << snapshot.spatial->boxQueries
               << " ray=" << snapshot.spatial->rayQueries
               << " trace_box=" << snapshot.spatial->traceBoxQueries
               << " trace_ray=" << snapshot.spatial->traceRayQueries
               << " swept_box=" << snapshot.spatial->sweptBoxQueries << '\n';
        report << "Trace Candidates: static=" << snapshot.spatial->staticCandidates
               << " dynamic=" << snapshot.spatial->dynamicCandidates << "\n\n";
    }

    report << "== STRUCTURAL GRAPH ==\n";
    if (!snapshot.structuralGraph.has_value()) {
        report << "Structural graph unavailable\n";
    } else {
        report << "Nodes: " << snapshot.structuralGraph->nodeCount
               << " | Edges: " << snapshot.structuralGraph->edgeCount
               << " | Cycles: " << snapshot.structuralGraph->cycleCount
               << " | Unresolved: " << snapshot.structuralGraph->unresolvedDependencyCount << '\n';
        report << "Phase Buckets: compile=" << snapshot.structuralGraph->phaseBuckets.compile
               << " runtime=" << snapshot.structuralGraph->phaseBuckets.runtime
               << " post_build=" << snapshot.structuralGraph->phaseBuckets.postBuild
               << " frame=" << snapshot.structuralGraph->phaseBuckets.frame << '\n';
    }

    return report.str();
}

std::string FormatRenderGameStateSnapshotJson(const RenderGameStateSnapshot& snapshot) {
    std::ostringstream out;
    out << "{";
    out << "\"mode\":\"" << JsonEscape(snapshot.mode.empty() ? "runtime" : snapshot.mode) << "\",";
    out << "\"level\":\"" << JsonEscape(snapshot.level) << "\",";
    out << "\"paused\":" << (snapshot.paused ? "true" : "false") << ",";
    out << "\"counts\":{";
    out << "\"sceneChildren\":" << snapshot.counts.sceneChildren << ",";
    out << "\"collidables\":" << snapshot.counts.collidables << ",";
    out << "\"structuralCollidables\":" << snapshot.counts.structuralCollidables << ",";
    out << "\"bvhMeshes\":" << snapshot.counts.bvhMeshes << ",";
    out << "\"interactives\":" << snapshot.counts.interactives << ",";
    out << "\"physicsObjects\":" << snapshot.counts.physicsObjects << ",";
    out << "\"levelEvents\":" << snapshot.counts.levelEvents << ",";
    out << "\"triggerVolumes\":" << snapshot.counts.triggerVolumes << ",";
    out << "\"logicEntities\":" << snapshot.counts.logicEntities << ",";
    out << "\"staticCollidables\":" << snapshot.counts.staticCollidables << ",";
    out << "\"dynamicCollidables\":" << snapshot.counts.dynamicCollidables << ",";
    out << "\"staticInteractives\":" << snapshot.counts.staticInteractives << ",";
    out << "\"dynamicInteractives\":" << snapshot.counts.dynamicInteractives;
    out << "},";
    out << "\"nearbyInteractives\":[";
    for (std::size_t i = 0; i < snapshot.nearbyInteractives.size(); ++i) {
        const NearbyInteractiveSnapshot& entry = snapshot.nearbyInteractives[i];
        if (i > 0) {
            out << ",";
        }
        out << "{";
        out << "\"id\":\"" << JsonEscape(entry.id) << "\",";
        out << "\"label\":\"" << JsonEscape(entry.label) << "\",";
        out << "\"distance\":" << FormatDouble(entry.distance, 3) << ",";
        out << "\"position\":{";
        out << "\"x\":" << FormatDouble(static_cast<double>(entry.position.x), 3) << ",";
        out << "\"y\":" << FormatDouble(static_cast<double>(entry.position.y), 3) << ",";
        out << "\"z\":" << FormatDouble(static_cast<double>(entry.position.z), 3);
        out << "}}";
    }
    out << "],";
    out << "\"actors\":[";
    for (std::size_t i = 0; i < snapshot.actors.size(); ++i) {
        const ActorSnapshotEntry& entry = snapshot.actors[i];
        if (i > 0) {
            out << ",";
        }
        out << "{";
        out << "\"kind\":\"" << JsonEscape(entry.kind) << "\",";
        out << "\"id\":\"" << JsonEscape(entry.id) << "\",";
        out << "\"state\":\"" << JsonEscape(entry.state) << "\",";
        out << "\"animation\":\"" << JsonEscape(entry.animation) << "\",";
        out << "\"distance\":" << FormatDouble(entry.distance, 3) << ",";
        out << "\"position\":{";
        out << "\"x\":" << FormatDouble(static_cast<double>(entry.position.x), 3) << ",";
        out << "\"y\":" << FormatDouble(static_cast<double>(entry.position.y), 3) << ",";
        out << "\"z\":" << FormatDouble(static_cast<double>(entry.position.z), 3);
        out << "}}";
    }
    out << "],";
    out << "\"coordinateSystem\":\""
        << JsonEscape(snapshot.coordinateSystem.empty() ? "rawiron native world coordinates" : snapshot.coordinateSystem)
        << "\"";
    if (!snapshot.detailFields.empty()) {
        std::vector<std::pair<std::string, std::string>> ordered;
        ordered.reserve(snapshot.detailFields.size());
        for (const auto& [key, value] : snapshot.detailFields) {
            ordered.emplace_back(key, value);
        }
        std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });
        out << ",\"details\":{";
        for (std::size_t i = 0; i < ordered.size(); ++i) {
            if (i > 0) {
                out << ",";
            }
            out << "\"" << JsonEscape(ordered[i].first) << "\":\"" << JsonEscape(ordered[i].second) << "\"";
        }
        out << "}";
    }
    out << "}";
    return out.str();
}

bool ExportRenderGameStateSnapshot(const RenderGameStateSnapshot& snapshot,
                                   const std::filesystem::path& outputPath,
                                   const bool asJson,
                                   std::string* error) {
    std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        if (error != nullptr) {
            *error = "Failed to open output path.";
        }
        return false;
    }
    stream << (asJson ? FormatRenderGameStateSnapshotJson(snapshot) : FormatRenderGameStateSnapshot(snapshot));
    if (!stream.good()) {
        if (error != nullptr) {
            *error = "Failed to write snapshot.";
        }
        return false;
    }
    return true;
}

bool ExportRuntimeDebugSnapshot(const RuntimeDebugSnapshot& snapshot,
                                const std::filesystem::path& outputPath,
                                std::string* error) {
    std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        if (error != nullptr) {
            *error = "Failed to open output path.";
        }
        return false;
    }
    stream << FormatRuntimeDebugReport(snapshot);
    if (!stream.good()) {
        if (error != nullptr) {
            *error = "Failed to write debug report.";
        }
        return false;
    }
    return true;
}

ProofBoardSnapshot BuildProofBoardSnapshot(const EngineCountSnapshot& counts,
                                           const std::span<const std::string_view> requiredPieces) {
    auto countByName = [&counts](std::string_view piece) -> std::size_t {
        if (piece == "collidables") return counts.collidables;
        if (piece == "structuralCollidables") return counts.structuralCollidables;
        if (piece == "interactives") return counts.interactives;
        if (piece == "physicsObjects") return counts.physicsObjects;
        if (piece == "triggerVolumes") return counts.triggerVolumes;
        if (piece == "logicEntities") return counts.logicEntities;
        if (piece == "levelEvents") return counts.levelEvents;
        return 0U;
    };

    ProofBoardSnapshot snapshot{};
    snapshot.towerStanding = counts.collidables > 0 && counts.structuralCollidables > 0;
    snapshot.pieces.reserve(requiredPieces.size());
    for (const std::string_view piece : requiredPieces) {
        const std::size_t count = countByName(piece);
        snapshot.pieces.push_back(ProofBoardPieceStatus{
            .pieceId = std::string(piece),
            .present = count > 0,
            .count = count,
        });
    }
    return snapshot;
}

std::string FormatProofBoardSnapshot(const ProofBoardSnapshot& snapshot) {
    std::ostringstream out;
    out << "proof_board\n";
    out << "tower_standing=" << (snapshot.towerStanding ? "true" : "false") << '\n';
    out << "pieces\n";
    for (const ProofBoardPieceStatus& piece : snapshot.pieces) {
        out << "  - " << piece.pieceId
            << " present=" << (piece.present ? "true" : "false")
            << " count=" << piece.count << '\n';
    }
    return out.str();
}

RenderGameStateDiff BuildRenderGameStateDiff(const RenderGameStateSnapshot& before,
                                             const RenderGameStateSnapshot& after) {
    RenderGameStateDiff diff{};
    auto appendIfChanged = [&](std::string field, std::string lhs, std::string rhs) {
        if (lhs != rhs) {
            diff.changes.push_back(RenderGameStateDiffEntry{
                .field = std::move(field),
                .before = std::move(lhs),
                .after = std::move(rhs),
            });
        }
    };

    appendIfChanged("mode", before.mode, after.mode);
    appendIfChanged("level", before.level, after.level);
    appendIfChanged("paused", before.paused ? "true" : "false", after.paused ? "true" : "false");
    appendIfChanged("counts.collidables",
                    std::to_string(before.counts.collidables),
                    std::to_string(after.counts.collidables));
    appendIfChanged("counts.interactives",
                    std::to_string(before.counts.interactives),
                    std::to_string(after.counts.interactives));
    appendIfChanged("eventBus.emits",
                    std::to_string(before.helperLibraries.eventBusEmits),
                    std::to_string(after.helperLibraries.eventBusEmits));
    appendIfChanged("eventBus.listeners",
                    std::to_string(before.helperLibraries.eventBusListeners),
                    std::to_string(after.helperLibraries.eventBusListeners));

    std::vector<std::pair<std::string, std::string>> keys;
    keys.reserve(before.detailFields.size() + after.detailFields.size());
    for (const auto& [key, value] : before.detailFields) {
        (void)value;
        keys.emplace_back(key, key);
    }
    for (const auto& [key, value] : after.detailFields) {
        (void)value;
        keys.emplace_back(key, key);
    }
    std::sort(keys.begin(), keys.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    keys.erase(std::unique(keys.begin(), keys.end(), [](const auto& lhs, const auto& rhs) { return lhs.first == rhs.first; }),
               keys.end());
    for (const auto& [key, _] : keys) {
        const std::string beforeValue = before.detailFields.contains(key) ? before.detailFields.at(key) : "<missing>";
        const std::string afterValue = after.detailFields.contains(key) ? after.detailFields.at(key) : "<missing>";
        appendIfChanged("details." + key, beforeValue, afterValue);
    }
    return diff;
}

std::string FormatRenderGameStateDiff(const RenderGameStateDiff& diff) {
    std::ostringstream out;
    out << "render_state_diff\n";
    out << "changes=" << diff.changes.size() << '\n';
    for (const RenderGameStateDiffEntry& entry : diff.changes) {
        out << "  - " << entry.field << ": " << entry.before << " -> " << entry.after << '\n';
    }
    return out.str();
}

void AppendRenderGameStateTimelineEntry(std::vector<RenderGameStateTimelineEntry>& timeline,
                                        RenderGameStateTimelineEntry entry,
                                        const std::size_t maxEntries) {
    timeline.push_back(std::move(entry));
    if (timeline.size() > maxEntries) {
        timeline.erase(timeline.begin(), timeline.begin() + static_cast<std::ptrdiff_t>(timeline.size() - maxEntries));
    }
}

} // namespace ri::debug
