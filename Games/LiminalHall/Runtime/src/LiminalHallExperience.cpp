#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"
#include "RawIron/Games/RuntimeDiagnosticsStandaloneDraw.h"

#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Audio/AudioBackendMiniaudio.h"
#include "RawIron/Audio/AudioManager.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/MovementController.h"
#include "RawIron/World/CheckpointPersistence.h"

#include "RawIron/Logic/LogicGraph.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cctype>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ri::games::liminal {

#if defined(_WIN32)
namespace {

namespace fs = std::filesystem;

std::optional<ri::content::GameManifest> ResolveStandaloneGameManifest(const StandaloneOptions& options) {
    if (!options.gameRoot.empty()) {
        return ri::content::LoadGameManifest(options.gameRoot / "manifest.json");
    }

    const fs::path workspaceRoot =
        options.workspaceRoot.empty() ? ri::content::DetectWorkspaceRoot(fs::current_path()) : options.workspaceRoot;
    return ri::content::ResolveGameManifest(workspaceRoot, options.gameId);
}

ri::spatial::Aabb BuildPlayerBounds(const ri::math::Vec3& feet) {
    return ri::spatial::Aabb{
        .min = ri::math::Vec3{feet.x - 0.25f, feet.y, feet.z - 0.25f},
        .max = ri::math::Vec3{feet.x + 0.25f, feet.y + 1.8f, feet.z + 0.25f},
    };
}

ri::math::Vec3 FeetFromBounds(const ri::spatial::Aabb& bounds) {
    return ri::math::Vec3{
        (bounds.min.x + bounds.max.x) * 0.5f,
        bounds.min.y,
        (bounds.min.z + bounds.max.z) * 0.5f,
    };
}

[[nodiscard]] ri::math::Vec3 CameraForwardWorld(const float yawDegrees, const float pitchDegrees) {
    const float yawRad = ri::math::DegreesToRadians(yawDegrees);
    const float pitchRad = ri::math::DegreesToRadians(pitchDegrees);
    const float cp = std::cos(pitchRad);
    return ri::math::Normalize(ri::math::Vec3{
        cp * std::sin(yawRad),
        std::sin(pitchRad),
        cp * std::cos(yawRad)});
}

std::optional<ri::math::Vec3> ResolveTeleportDestination(const ri::scene::Scene& scene,
                                                         const ri::world::TeleportRequest& request) {
    if (!request.targetId.empty()) {
        for (int nodeIndex = 0; nodeIndex < static_cast<int>(scene.NodeCount()); ++nodeIndex) {
            const ri::scene::Node& node = scene.GetNode(nodeIndex);
            if (node.name == request.targetId) {
                return scene.ComputeWorldPosition(nodeIndex) + request.offset;
            }
        }
    }
    return request.targetPosition + request.offset;
}

[[nodiscard]] std::string DescribeOptionalAssetState(const fs::path& path, const bool checkSqliteHeader = false) {
    std::error_code ec{};
    if (!fs::exists(path, ec)) {
        return "missing";
    }
    if (fs::is_directory(path, ec)) {
        return "invalid-dir";
    }
    const std::uintmax_t sizeBytes = fs::file_size(path, ec);
    if (ec || sizeBytes == 0U) {
        return "empty";
    }
    if (checkSqliteHeader) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            return "unreadable";
        }
        char header[16]{};
        stream.read(header, sizeof(header));
        const std::streamsize readCount = stream.gcount();
        static constexpr const char* kSqliteMagic = "SQLite format 3";
        if (readCount < 15 || std::string_view(header, 15) != kSqliteMagic) {
            return "invalid-header";
        }
    }
    return "ok";
}

[[nodiscard]] int CountDataRows(const fs::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return 0;
    }
    std::string line;
    int rows = 0;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        ++rows;
    }
    if (rows > 0) {
        --rows;
    }
    return std::max(0, rows);
}

[[nodiscard]] int CountNonCommentLines(const fs::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return 0;
    }
    std::string line;
    int lines = 0;
    while (std::getline(stream, line)) {
        std::size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }
        ++lines;
    }
    return lines;
}

struct LiminalDemoExtensions {
    int pluginCount = 0;
    int pluginHookCount = 0;
    int animationGraphNodeCount = 0;
    int vfxEntryCount = 0;
    int lightingRowCount = 0;
    int cinematicsRowCount = 0;
    int telemetryHeaderValid = 0;
    int entityRegistryRows = 0;
    int aiNodeRows = 0;
    int shaderRows = 0;
};

struct ShowcaseLightingRow {
    float intensity = 1.0f;
    ri::math::Vec3 color{0.7f, 0.7f, 0.7f};
};

struct ShowcaseCinematicRow {
    std::string id{};
    float durationSeconds = 0.0f;
    float fovPulse = 0.0f;
};

struct ShowcaseVfxEntry {
    std::string id{};
    float weight = 1.0f;
};

[[nodiscard]] std::vector<std::string> SplitCsvColumns(const std::string& line) {
    std::vector<std::string> columns{};
    std::stringstream stream(line);
    std::string part;
    while (std::getline(stream, part, ',')) {
        columns.push_back(part);
    }
    return columns;
}

[[nodiscard]] ri::math::Vec3 ParsePipeColor(const std::string& raw, const ri::math::Vec3& fallback) {
    std::stringstream stream(raw);
    std::string item;
    ri::math::Vec3 out = fallback;
    if (std::getline(stream, item, '|')) {
        out.x = std::stof(item);
    }
    if (std::getline(stream, item, '|')) {
        out.y = std::stof(item);
    }
    if (std::getline(stream, item, '|')) {
        out.z = std::stof(item);
    }
    return out;
}

[[nodiscard]] std::vector<ShowcaseLightingRow> LoadShowcaseLightingRows(const fs::path& path) {
    std::vector<ShowcaseLightingRow> rows{};
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return rows;
    }
    std::string line;
    bool headerSeen = false;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!headerSeen) {
            headerSeen = true;
            continue;
        }
        const std::vector<std::string> cols = SplitCsvColumns(line);
        if (cols.size() < 7) {
            continue;
        }
        ShowcaseLightingRow row{};
        row.intensity = std::clamp(std::stof(cols[5]), 0.2f, 4.0f);
        row.color = ParsePipeColor(cols[6], ri::math::Vec3{0.7f, 0.7f, 0.7f});
        rows.push_back(row);
    }
    return rows;
}

[[nodiscard]] std::vector<ShowcaseCinematicRow> LoadShowcaseCinematicRows(const fs::path& path) {
    std::vector<ShowcaseCinematicRow> rows{};
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return rows;
    }
    std::string line;
    bool headerSeen = false;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!headerSeen) {
            headerSeen = true;
            continue;
        }
        const std::vector<std::string> cols = SplitCsvColumns(line);
        if (cols.size() < 4) {
            continue;
        }
        ShowcaseCinematicRow row{};
        row.id = cols[0];
        row.durationSeconds = std::clamp(std::stof(cols[3]), 0.5f, 8.0f);
        row.fovPulse = 2.0f + std::clamp(row.durationSeconds * 0.8f, 0.0f, 6.0f);
        rows.push_back(row);
    }
    return rows;
}

[[nodiscard]] std::vector<ShowcaseVfxEntry> LoadShowcaseVfxEntries(const fs::path& path) {
    std::vector<ShowcaseVfxEntry> rows{};
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return rows;
    }
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> cols = SplitCsvColumns(line);
        if (cols.size() < 4) {
            continue;
        }
        ShowcaseVfxEntry entry{};
        entry.id = cols[0];
        entry.weight = std::clamp(static_cast<float>(std::stoi(cols[3])) / 100.0f, 0.2f, 2.0f);
        rows.push_back(entry);
    }
    return rows;
}

[[nodiscard]] int LoadAnimationGraphNodeCount(const fs::path& path) {
    return CountNonCommentLines(path);
}

LiminalDemoExtensions LoadLiminalDemoExtensions(const ri::content::GameManifest& manifest) {
    LiminalDemoExtensions out{};
    const fs::path root = manifest.rootPath;
    out.pluginCount = CountNonCommentLines(root / "plugins" / "manifest.plugins");
    out.pluginHookCount = CountNonCommentLines(root / "plugins" / "hooks.riplugin");
    out.animationGraphNodeCount = CountNonCommentLines(root / "assets" / "animation.graph");
    out.vfxEntryCount = CountNonCommentLines(root / "assets" / "vfx.manifest");
    out.lightingRowCount = CountDataRows(root / "levels" / "assembly.lighting.csv");
    out.cinematicsRowCount = CountDataRows(root / "levels" / "assembly.cinematics.csv");
    out.entityRegistryRows = CountDataRows(root / "data" / "entity.registry");
    out.aiNodeRows = CountDataRows(root / "levels" / "assembly.ai.nodes");
    out.shaderRows = CountNonCommentLines(root / "assets" / "shaders.manifest");
    out.telemetryHeaderValid = DescribeOptionalAssetState(root / "data" / "telemetry.db", true) == "ok" ? 1 : 0;
    return out;
}

struct SpawnSetup {
    ri::math::Vec3 position{};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

SpawnSetup ResolveSpawnSetup(const StandaloneOptions& options,
                             const ri::content::GameManifest& manifest,
                             const ri::content::ScriptScalarMap& gameplay) {
    SpawnSetup spawn{};
    spawn.position = ri::math::Vec3{
        ri::content::ScriptScalarOr(gameplay, "spawn_x", 0.0f),
        ri::content::ScriptScalarOr(gameplay, "spawn_y", 0.0f),
        ri::content::ScriptScalarOr(gameplay, "spawn_z", 4.0f),
    };
    spawn.yaw = ri::content::ScriptScalarOrClamped(gameplay, "spawn_yaw", 0.0f, -180.0f, 180.0f);
    spawn.pitch = ri::content::ScriptScalarOrClamped(gameplay, "spawn_pitch", 0.0f, -40.0f, 30.0f);

    const fs::path defaultCheckpointRoot = options.workspaceRoot.empty()
        ? (manifest.rootPath / "Saved" / "Checkpoints" / manifest.id)
        : (options.workspaceRoot / "Saved" / "Checkpoints" / manifest.id);
    const fs::path checkpointRoot = options.checkpointStorageRoot.empty() ? defaultCheckpointRoot : options.checkpointStorageRoot;
    const ri::world::FileCheckpointStore checkpointStore(checkpointRoot);
    ri::world::CheckpointStartupOptions startupOptions{};
    startupOptions.startFromCheckpoint = options.startFromCheckpoint;
    startupOptions.slot = options.checkpointSlot;
    startupOptions.queryString = options.resumeQuery;
    std::string checkpointError;
    const ri::world::CheckpointStartupDecision startupDecision =
        ri::world::ResolveCheckpointStartupDecision(startupOptions, checkpointStore, &checkpointError);
    if (startupDecision.startFromCheckpoint && startupDecision.snapshot.has_value()) {
        const ri::world::RuntimeCheckpointSnapshot& snapshot = *startupDecision.snapshot;
        if (snapshot.playerPosition.has_value()) {
            spawn.position = *snapshot.playerPosition;
        }
        if (snapshot.playerRotation.has_value()) {
            spawn.yaw = std::clamp(snapshot.playerRotation->y, -180.0f, 180.0f);
            spawn.pitch = std::clamp(snapshot.playerRotation->x, -40.0f, 30.0f);
        }
        ri::core::LogInfo("Checkpoint resume: slot=" + startupDecision.slot +
                          " level=" + snapshot.state.level.value_or("<none>"));
    } else if (startupDecision.startFromCheckpoint) {
        ri::core::LogInfo("Checkpoint resume requested but no checkpoint found for slot=" + startupDecision.slot);
        if (!checkpointError.empty()) {
            ri::core::LogInfo("Checkpoint resume parse/load issue: " + checkpointError);
        }
    }
    return spawn;
}

float WrapDegrees(const float degrees) {
    float wrapped = std::fmod(degrees, 360.0f);
    if (wrapped > 180.0f) {
        wrapped -= 360.0f;
    } else if (wrapped < -180.0f) {
        wrapped += 360.0f;
    }
    return wrapped;
}

float ApproachDegrees(const float current, const float target, const float maxDelta) {
    const float delta = std::clamp(WrapDegrees(target - current), -maxDelta, maxDelta);
    return WrapDegrees(current + delta);
}

float YawFromDirection(const ri::math::Vec3& direction) {
    return ri::math::RadiansToDegrees(std::atan2(direction.x, direction.z));
}

float PitchFromDirection(const ri::math::Vec3& direction) {
    const float planarLength = std::max(0.001f, std::sqrt((direction.x * direction.x) + (direction.z * direction.z)));
    return std::clamp(
        ri::math::RadiansToDegrees(std::atan2(-direction.y, planarLength)),
        -40.0f,
        30.0f);
}

struct PreviewResolution {
    int width = 0;
    int height = 0;
};

[[nodiscard]] PreviewResolution ComputeSoftwarePreviewResolution(const StandaloneOptions& options) {
    PreviewResolution result{};
    const float scale = std::clamp(options.softwareRenderScale, 0.25f, 1.0f);
    result.width = std::max(64, static_cast<int>(std::lround(static_cast<float>(options.width) * scale)));
    result.height = std::max(64, static_cast<int>(std::lround(static_cast<float>(options.height) * scale)));
    return result;
}

const char* StandaloneRendererName(const StandaloneRenderer renderer) {
    switch (renderer) {
    case StandaloneRenderer::VulkanNative:
        return "vulkan-native";
    }
    return "unknown";
}

struct NativeRenderTuning {
    int qualityTier = 1;
    float exposure = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float fogDensity = 0.0095f;
};

const char* RenderQualityName(const RenderQuality quality) {
    switch (quality) {
    case RenderQuality::Competitive:
        return "competitive";
    case RenderQuality::Balanced:
        return "balanced";
    case RenderQuality::Cinematic:
        return "cinematic";
    }
    return "balanced";
}

NativeRenderTuning BaseNativeRenderTuning(const RenderQuality quality) {
    switch (quality) {
    case RenderQuality::Competitive:
        return NativeRenderTuning{0, 1.00f, 1.00f, 1.00f, 0.0085f};
    case RenderQuality::Cinematic:
        return NativeRenderTuning{2, 1.03f, 1.04f, 1.03f, 0.0105f};
    case RenderQuality::Balanced:
    default:
        return NativeRenderTuning{1, 1.00f, 1.00f, 1.00f, 0.0092f};
    }
}

ri::render::vulkan::VulkanPresentModePreference ToVulkanPresentModePreference(const StandalonePresentMode mode) {
    switch (mode) {
    case StandalonePresentMode::Mailbox:
        return ri::render::vulkan::VulkanPresentModePreference::Mailbox;
    case StandalonePresentMode::Immediate:
        return ri::render::vulkan::VulkanPresentModePreference::Immediate;
    case StandalonePresentMode::Fifo:
        return ri::render::vulkan::VulkanPresentModePreference::Fifo;
    case StandalonePresentMode::Auto:
    default:
        return ri::render::vulkan::VulkanPresentModePreference::Auto;
    }
}

void LogBenchmarkResults(const char* label,
                         const int frameCount,
                         const std::chrono::steady_clock::time_point startTime,
                         const std::chrono::steady_clock::time_point endTime) {
    if (frameCount <= 0) {
        return;
    }
    const double seconds = std::chrono::duration<double>(endTime - startTime).count();
    if (seconds <= 0.0) {
        return;
    }
    const double fps = static_cast<double>(frameCount) / seconds;
    const double milliseconds = (seconds * 1000.0) / static_cast<double>(frameCount);
    ri::core::LogSection("Standalone FPS");
    ri::core::LogInfo(std::string("Renderer: ") + label);
    ri::core::LogInfo("Frames: " + std::to_string(frameCount)
                      + " elapsed=" + std::to_string(seconds)
                      + "s avg=" + std::to_string(fps)
                      + " FPS (" + std::to_string(milliseconds) + " ms/frame)");
}

struct RuntimeState {
    HWND hwnd = nullptr;
    bool mouseCaptured = false;
    float rawMouseAccumX = 0.0f;
    float rawMouseAccumY = 0.0f;
    bool captureCursorHidden = false;
    bool captureMouse = true;
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float elapsedSeconds = 0.0f;
    World world{};
    ri::trace::TraceScene traceScene{};
    ri::trace::MovementControllerState movement{};
    ri::trace::MovementControllerOptions movementOptions{};
    ri::trace::MovementControllerOptions authoredMovementOptions{};
    ri::trace::KinematicVolumeModifiers activeVolumeModifiers{};
    ri::world::PhysicsConstraintState activeConstraintState{};
    ri::world::WaterSurfaceState activeWaterSurfaceState{};
    ri::world::KinematicMotionState activeKinematicMotionState{};
    ri::math::Vec3 previousKinematicTranslationDelta{};
    ri::render::software::ScenePreviewOptions previewOptions{};
    ri::render::software::ScenePreviewCache previewCache{};
    ri::render::software::SoftwareImage scenePreviewScratch{};
    float mouseSensitivityDegreesPerPixel = 0.12f;
    float cameraBaseHeight = 1.62f;
    float bobAmplitude = 0.014f;
    float bobFrequencyHz = 1.6f;
    float bobSprintScale = 1.75f;
    float fovBaseDegrees = 78.0f;
    float fovSprintAddDegrees = 4.0f;
    float fovLerpPerSecond = 9.0f;
    float currentFovDegrees = 78.0f;
    NativeRenderTuning nativeRenderTuning{};
    std::chrono::steady_clock::time_point lastTick = std::chrono::steady_clock::now();
    bool jumpHeldLastFrame = false;
    bool useHeldLastFrame = false;
    bool wasOnGroundLastFrame = true;
    int bhopChainCount = 0;
    double bhopFeedbackCooldownSeconds = 0.0;
    double environmentTelemetryAccumSeconds = 0.0;
    std::filesystem::path nativeSkyEquirectRelative{};
    bool diagnosticsVisible = false;
    int diagnosticsRoot = ri::scene::kInvalidHandle;
    std::vector<int> diagnosticsVolumeNodes{};
    std::vector<int> diagnosticsGizmoNodes{};
    ri::world::RuntimeEnvironmentService environmentService{};
    ri::world::RuntimeDiagnosticsLayer diagnosticsLayer{};
    std::filesystem::path gameRoot{};
    std::shared_ptr<ri::audio::AudioManager> audioManager{};
    std::unordered_map<std::string, std::shared_ptr<ri::audio::ManagedSound>> ambientLoopByContributionId{};
    bool logicPressurePlateTriggered = false;
    bool logicDoorOpen = false;
    bool logicPortalSpawned = false;
    bool logicPortalUsed = false;
    ri::math::Vec3 logicSpawnPosition{};
    float playerMaxHealth = 100.0f;
    float playerHealth = 100.0f;
    std::size_t previousActiveSpawnerCount = 0U;
    bool logicDemoPlateWasInside = false;
    std::unique_ptr<ri::logic::LogicGraph> logicDemoGraph{};
    bool showcaseEnabled = true;
    bool showcaseActive = false;
    bool showcaseDiagnosticsWasVisible = false;
    float showcaseDurationSeconds = 10.0f;
    float showcaseElapsedSeconds = 0.0f;
    float showcaseFovPulseDegrees = 0.0f;
    NativeRenderTuning showcaseBaseRenderTuning{};
    std::vector<ShowcaseLightingRow> showcaseLighting{};
    std::vector<ShowcaseCinematicRow> showcaseCinematics{};
    std::vector<ShowcaseVfxEntry> showcaseVfx{};
    int showcaseAnimationNodes = 0;
};

void ApplyEnvironmentAuthoringVolumes(RuntimeState& state, const float dt) {
    const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
    (void)state.environmentService.UpdateEnvironmentalVolumesAt(feet);
    state.movementOptions = state.authoredMovementOptions;
    state.activeVolumeModifiers = {};

    const ri::world::RuntimeEnvironmentState environmentState =
        state.environmentService.GetActiveEnvironmentStateAt(feet, static_cast<double>(state.elapsedSeconds) + dt);
    const ri::world::PhysicsVolumeModifiers& physicsState = environmentState.physics;
    state.activeConstraintState = environmentState.constraints;
    state.activeWaterSurfaceState = environmentState.waterSurface;
    state.activeKinematicMotionState = environmentState.kinematicMotion;
    state.activeVolumeModifiers.gravityScale = physicsState.gravityScale;
    state.activeVolumeModifiers.drag = physicsState.drag;
    state.activeVolumeModifiers.buoyancy = physicsState.buoyancy;
    state.activeVolumeModifiers.flow = physicsState.flow;
    state.activeVolumeModifiers.jumpScale = physicsState.jumpScale;
    if (state.activeWaterSurfaceState.inside && state.activeWaterSurfaceState.surface != nullptr) {
        const ri::world::WaterSurfacePrimitive& waterSurface = *state.activeWaterSurfaceState.surface;
        state.activeVolumeModifiers.drag += std::clamp(waterSurface.waveAmplitude * 0.45f, 0.0f, 2.0f);
        state.activeVolumeModifiers.buoyancy = std::max(
            state.activeVolumeModifiers.buoyancy,
            std::clamp(0.2f + (waterSurface.waveAmplitude * 0.5f), 0.0f, 2.0f));
        state.activeVolumeModifiers.flow.x += waterSurface.flowSpeed;
    }
    if (dt > 0.0001f && !state.activeKinematicMotionState.activeTranslationPrimitives.empty()) {
        const ri::math::Vec3 translationDelta =
            state.activeKinematicMotionState.translationDelta - state.previousKinematicTranslationDelta;
        state.activeVolumeModifiers.flow = state.activeVolumeModifiers.flow + (translationDelta * (1.0f / dt));
    }
    state.previousKinematicTranslationDelta = state.activeKinematicMotionState.translationDelta;

    const ri::world::NavmeshModifierAggregateState navmeshState =
        state.environmentService.GetNavmeshModifierAggregateAt(feet);
    if (!navmeshState.matches.empty()) {
        const float speedScale = std::clamp(1.0f / navmeshState.traversalCostMultiplier, 0.35f, 1.2f);
        state.movementOptions.maxGroundSpeed *= speedScale;
        state.movementOptions.maxSprintGroundSpeed *= speedScale;
        state.movementOptions.maxAirSpeed *= std::clamp(speedScale + 0.1f, 0.4f, 1.3f);
    }

    const ri::world::TraversalLinkSelectionState traversalState =
        state.environmentService.GetTraversalLinksAt(feet);
    const ri::world::HintPartitionState hintState = state.environmentService.GetHintPartitionStateAt(feet);
    const ri::world::LodOverrideSelectionState lodOverrideState =
        state.environmentService.ResolveLodOverrideAt(feet, "mesh_a");
    const ri::world::DoorWindowCutoutPrimitive* cutout = state.environmentService.GetDoorWindowCutoutAt(feet);
    if (traversalState.selected != nullptr) {
        if (traversalState.selected->kind == ri::world::TraversalLinkKind::Ladder) {
            state.movementOptions.maxGroundSpeed *= 0.6f;
            state.movementOptions.maxSprintGroundSpeed *= 0.65f;
            state.movementOptions.jumpSpeed *= 0.35f;
            state.movementOptions.airControl = std::max(state.movementOptions.airControl, 0.45f);
        } else if (traversalState.selected->kind == ri::world::TraversalLinkKind::Climb) {
            state.movementOptions.maxGroundSpeed *= 0.75f;
            state.movementOptions.maxSprintGroundSpeed *= 0.8f;
            state.movementOptions.jumpSpeed *= 0.6f;
            state.movementOptions.airControl = std::max(state.movementOptions.airControl, 0.35f);
        }
    }

    const ri::world::AmbientAudioMixState ambientMix =
        state.environmentService.GetAmbientAudioMixStateAt(feet);
    const ri::world::SpatialQueryMatchState spatialQueryState =
        state.environmentService.GetSpatialQueryStateAt(feet, 0x4U);
    const ri::world::PivotAnchorBindingState pivotBinding =
        state.environmentService.ResolvePivotAnchorBindingAt(feet);
    const ri::world::SymmetryMirrorResult symmetryMirror =
        state.environmentService.ResolveSymmetryMirrorAt(feet, {0.0f, 0.0f, 1.0f});
    const ri::world::AuthoringPlacementState authoringPlacement =
        state.environmentService.ResolveAuthoringPlacementAt(feet, {0.0f, 0.0f, 1.0f});
    const ri::world::TriggerUpdateResult triggerUpdate =
        state.environmentService.UpdateTriggerVolumesAt(feet, static_cast<double>(state.elapsedSeconds) + dt, nullptr, true);
    for (const ri::world::LaunchRequest& launch : triggerUpdate.launchRequests) {
        state.movement.body.velocity = state.movement.body.velocity + launch.impulse;
    }
    for (const ri::world::DamageRequest& damage : triggerUpdate.damageRequests) {
        const float applied = damage.killInstant
            ? state.playerHealth
            : std::max(0.0f, damage.damagePerSecond) * std::max(0.0f, dt);
        state.playerHealth = std::clamp(state.playerHealth - applied, 0.0f, state.playerMaxHealth);
    }
    if (state.playerHealth <= 0.0f) {
        state.movement.body.bounds = BuildPlayerBounds(state.logicSpawnPosition);
        state.movement.body.velocity = {};
        state.playerHealth = state.playerMaxHealth;
        ri::core::LogInfo("Runtime authoring: player defeated by trigger damage, respawned at spawn point.");
    }
    if (!triggerUpdate.teleportRequests.empty()) {
        if (const std::optional<ri::math::Vec3> destination =
                ResolveTeleportDestination(state.world.scene, triggerUpdate.teleportRequests.front());
            destination.has_value()) {
            state.movement.body.bounds = BuildPlayerBounds(*destination);
            state.movement.body.velocity = {};
        }
    }
    if (state.logicDemoGraph != nullptr) {
        for (const ri::world::TriggerTransition& transition : triggerUpdate.transitions) {
            if (transition.kind != ri::world::TriggerTransitionKind::Enter) {
                continue;
            }
            ri::logic::LogicContext ctx{};
            ctx.instigatorId = "liminal_player";
            if (transition.volumeId.rfind("event:", 0U) == 0U) {
                (void)state.environmentService.DispatchLevelEvent(
                    *state.logicDemoGraph,
                    std::string_view(transition.volumeId).substr(6U),
                    ctx);
            } else if (transition.volumeId.rfind("sequence:", 0U) == 0U) {
                (void)state.environmentService.DispatchLevelSequenceStep(
                    *state.logicDemoGraph,
                    std::string_view(transition.volumeId).substr(9U),
                    0U,
                    ctx);
            }
        }
    }
    const std::vector<ri::world::ActiveSpawnerState> activeSpawners = state.environmentService.GetActiveSpawnerStates();
    const std::size_t activeSpawnerCount = static_cast<std::size_t>(std::count_if(
        activeSpawners.begin(),
        activeSpawners.end(),
        [](const ri::world::ActiveSpawnerState& spawner) { return spawner.enabled && spawner.activeSpawn; }));
    if (activeSpawnerCount != state.previousActiveSpawnerCount) {
        state.previousActiveSpawnerCount = activeSpawnerCount;
        ri::core::LogInfo("Runtime authoring: active spawners=" + std::to_string(activeSpawnerCount));
    }
    const ri::world::SafeLightCoverageState safeLightCoverage = state.environmentService.GetSafeLightCoverageAt(feet);
    if (state.audioManager != nullptr) {
        const ri::world::AudioEnvironmentState authoredAudio =
            state.environmentService.GetActiveAudioEnvironmentStateAt(feet);
        ri::audio::AudioEnvironmentProfileInput profile{};
        profile.label = authoredAudio.label;
        profile.activeVolumes = authoredAudio.activeVolumes;
        profile.reverbMix = authoredAudio.reverbMix;
        profile.echoDelayMs = authoredAudio.echoDelayMs;
        profile.echoFeedback = authoredAudio.echoFeedback;
        profile.dampening = authoredAudio.dampening;
        profile.volumeScale = std::clamp(
            static_cast<double>(authoredAudio.volumeScale) * (0.8 + (ambientMix.combinedDesiredVolume * 0.2)),
            0.1,
            2.0);
        profile.playbackRate = std::clamp(static_cast<double>(authoredAudio.playbackRate), 0.5, 1.5);
        (void)state.audioManager->SetEnvironmentProfile(profile);

        std::unordered_map<std::string, bool> keepIds{};
        const std::size_t ambientVoiceLimit = 2U;
        for (std::size_t index = 0; index < ambientMix.contributions.size() && index < ambientVoiceLimit; ++index) {
            const ri::world::AmbientAudioContribution& contribution = ambientMix.contributions[index];
            if (contribution.audioPath.empty()) {
                continue;
            }
            keepIds[contribution.id] = true;
            auto found = state.ambientLoopByContributionId.find(contribution.id);
            if (found == state.ambientLoopByContributionId.end() || found->second == nullptr) {
                std::filesystem::path clipPath = state.gameRoot / contribution.audioPath;
                std::shared_ptr<ri::audio::ManagedSound> loop =
                    state.audioManager->CreateLoopingSound(clipPath.string(), contribution.desiredVolume);
                if (loop != nullptr) {
                    loop->Play();
                }
                state.ambientLoopByContributionId[contribution.id] = std::move(loop);
                found = state.ambientLoopByContributionId.find(contribution.id);
            }
            if (found != state.ambientLoopByContributionId.end() && found->second != nullptr) {
                found->second->SetVolume(contribution.desiredVolume);
                found->second->SetPlaybackRate(1.0 + (contribution.normalizedFalloff * 0.03));
            }
        }
        for (auto it = state.ambientLoopByContributionId.begin(); it != state.ambientLoopByContributionId.end();) {
            if (keepIds.contains(it->first)) {
                ++it;
                continue;
            }
            if (it->second != nullptr) {
                state.audioManager->StopManagedSound(it->second, true);
            }
            it = state.ambientLoopByContributionId.erase(it);
        }
        state.audioManager->Tick(static_cast<double>(dt) * 1000.0);
    }
    state.environmentTelemetryAccumSeconds += dt;
    if (state.environmentTelemetryAccumSeconds >= 1.0) {
        state.environmentTelemetryAccumSeconds = 0.0;
        ri::core::LogInfo(
            "Runtime authoring: ambient=" + std::to_string(ambientMix.contributions.size()) +
            " top=" + ambientMix.topContributionId +
            " flowVolumes=" + std::to_string(physicsState.activeSurfaceVelocity.size()) +
            " navmesh=" + std::to_string(navmeshState.matches.size()) +
            " navTag=" + navmeshState.dominantTag +
            " traversal=" + std::string(traversalState.selected != nullptr ? traversalState.selected->id : "none") +
            " queryMatches=" + std::to_string(spatialQueryState.matches.size()) +
            " queryMask=" + std::to_string(spatialQueryState.combinedFilterMask) +
            " pivot=" + std::string(pivotBinding.anchor != nullptr ? pivotBinding.anchor->anchorId : "none") +
            " mirror=" + std::string(symmetryMirror.mirrored ? "on" : "off") +
            " placementMirrored=" + std::string(authoringPlacement.mirrored ? "on" : "off") +
            " placementSnap=" + std::string(authoringPlacement.snappedToGrid ? "on" : "off") +
            " water=" + std::string(state.activeWaterSurfaceState.inside && state.activeWaterSurfaceState.surface != nullptr
                ? state.activeWaterSurfaceState.surface->id
                : "none") +
            " safeLight=" + std::string(safeLightCoverage.insideSafeLight ? "on" : "off") +
            " safeCoverage=" + std::to_string(safeLightCoverage.combinedCoverage) +
            " hp=" + std::to_string(state.playerHealth) +
            " kinTx=" + std::to_string(state.activeKinematicMotionState.activeTranslationPrimitives.size()) +
            " kinRot=" + std::to_string(state.activeKinematicMotionState.activeRotationPrimitives.size()) +
            " hintMode=" + std::string(hintState.inside && hintState.mode == ri::world::HintPartitionMode::Skip ? "skip" : "hint") +
            " forcedLod=" + std::string(
                lodOverrideState.selected != nullptr && lodOverrideState.forcedLod == ri::world::ForcedLod::Far ? "far" : "near") +
            " cutout=" + std::string(cutout != nullptr ? cutout->id : "none"));
    }
}

void ProcessPendingDoorTransitions(RuntimeState& state) {
    const std::vector<ri::world::DoorTransitionRequest> transitions = state.environmentService.ConsumePendingDoorTransitions();
    for (const ri::world::DoorTransitionRequest& transition : transitions) {
        state.environmentService.ApplyDoorTransitionMetadata(transition);
        if (!transition.accessFeedbackTag.empty()) {
            ri::core::LogInfo(
                "Door access feedback: door=" + transition.doorId + " tag=" + transition.accessFeedbackTag);
        }
        if (!transition.endingTrigger.empty()) {
            ri::core::LogInfo(
                "Door ending trigger: door=" + transition.doorId + " ending=" + transition.endingTrigger);
        }
        if (!transition.transitionLevel.empty()) {
            ri::core::LogInfo(
                "Door level transition requested: door=" + transition.doorId + " level=" + transition.transitionLevel);
        }
    }
}

[[nodiscard]] bool IsSkiesImageExtension(const std::string& extensionLowercase) {
    return extensionLowercase == ".png" || extensionLowercase == ".jpg" || extensionLowercase == ".jpeg"
        || extensionLowercase == ".hdr" || extensionLowercase == ".bmp" || extensionLowercase == ".tga";
}

[[nodiscard]] int SkyTexturePreferenceRank(const fs::path& filePath) {
    std::string name = filePath.filename().string();
    for (char& character : name) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (name.find("equirect") != std::string::npos) {
        return 0;
    }
    if (name.find("sky") != std::string::npos) {
        return 1;
    }
    return 2;
}

void CollectSkiesImageFiles(const fs::path& directory, const int maxDepth, const int depth, std::vector<fs::path>& out) {
    if (depth > maxDepth) {
        return;
    }
    std::error_code ec{};
    if (!fs::is_directory(directory, ec) || ec) {
        return;
    }
    for (const fs::directory_entry& entry : fs::directory_iterator(directory, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_directory()) {
            CollectSkiesImageFiles(entry.path(), maxDepth, depth + 1, out);
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string ext = entry.path().extension().string();
        for (char& character : ext) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (!IsSkiesImageExtension(ext)) {
            continue;
        }
        out.push_back(entry.path());
    }
}

[[nodiscard]] std::filesystem::path PickSkiesEquirectRelative(const std::filesystem::path& textureRoot) {
    if (textureRoot.empty()) {
        return {};
    }
    const fs::path skiesDir = textureRoot / "Skies";
    std::vector<fs::path> candidates{};
    CollectSkiesImageFiles(skiesDir, 4, 0, candidates);
    if (candidates.empty()) {
        return {};
    }
    std::sort(candidates.begin(), candidates.end(), [](const fs::path& left, const fs::path& right) {
        const int rankLeft = SkyTexturePreferenceRank(left);
        const int rankRight = SkyTexturePreferenceRank(right);
        if (rankLeft != rankRight) {
            return rankLeft < rankRight;
        }
        return left.filename().string() < right.filename().string();
    });

    const fs::path& pick = candidates.front();
    std::error_code relativeError{};
    const fs::path relative = fs::relative(pick, textureRoot, relativeError);
    if (!relativeError && !relative.empty()) {
        return relative.lexically_normal();
    }
    // Cross-volume or unusual layout: pass an absolute path; Vulkan resolves it without joining textureRoot.
    std::error_code canonicalError{};
    const fs::path canonicalPick = fs::weakly_canonical(pick, canonicalError);
    return canonicalError ? pick.lexically_normal() : canonicalPick;
}

void RegisterRawMouseForWindow(HWND hwnd) {
    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = 0;
    device.hwndTarget = hwnd;
    RegisterRawInputDevices(&device, 1, sizeof(device));
}

void LiminalStandaloneWin32Hook(void* user,
                                void* hwndVoid,
                                unsigned int message,
                                std::uint64_t wParam,
                                std::int64_t lParam) {
    auto* state = static_cast<RuntimeState*>(user);
    if (state == nullptr) {
        return;
    }

    HWND hwnd = static_cast<HWND>(hwndVoid);

    switch (message) {
    case WM_CREATE:
        RegisterRawMouseForWindow(hwnd);
        break;
    case WM_INPUT: {
        if (!state->captureMouse || GetForegroundWindow() != hwnd) {
            break;
        }
        HRAWINPUT handle = reinterpret_cast<HRAWINPUT>(lParam);
        UINT byteSize = 0;
        if (GetRawInputData(handle, RID_INPUT, nullptr, &byteSize, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
            break;
        }
        std::vector<std::uint8_t> buffer(byteSize);
        if (GetRawInputData(handle, RID_INPUT, buffer.data(), &byteSize, sizeof(RAWINPUTHEADER)) ==
            static_cast<UINT>(-1)) {
            break;
        }
        const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buffer.data());
        if (raw->header.dwType != RIM_TYPEMOUSE) {
            break;
        }
        const RAWMOUSE& mouse = raw->data.mouse;
        if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0) {
            break;
        }
        state->rawMouseAccumX += static_cast<float>(mouse.lLastX);
        state->rawMouseAccumY += static_cast<float>(mouse.lLastY);
        break;
    }
    case WM_ACTIVATE:
        if (LOWORD(static_cast<WPARAM>(wParam)) == WA_INACTIVE) {
            ClipCursor(nullptr);
            if (state->captureCursorHidden) {
                ShowCursor(TRUE);
                state->captureCursorHidden = false;
            }
        } else if (state->captureMouse) {
            RECT client{};
            if (GetClientRect(hwnd, &client)) {
                POINT upperLeft{client.left, client.top};
                POINT lowerRight{client.right, client.bottom};
                ClientToScreen(hwnd, &upperLeft);
                ClientToScreen(hwnd, &lowerRight);
                const RECT clip{upperLeft.x, upperLeft.y, lowerRight.x, lowerRight.y};
                ClipCursor(&clip);
            }
        }
        break;
    default:
        break;
    }
}

struct HeadlessAutoplayPlan {
    ri::math::Vec3 moveTarget{};
    ri::math::Vec3 lookTarget{};
    bool sprintHeld = true;
};

void UpdateMouseLook(RuntimeState& state) {
    if (!state.captureMouse) {
        if (state.captureCursorHidden) {
            ShowCursor(TRUE);
            state.captureCursorHidden = false;
        }
        state.mouseCaptured = false;
        state.rawMouseAccumX = 0.0f;
        state.rawMouseAccumY = 0.0f;
        return;
    }
    if (state.hwnd == nullptr || GetForegroundWindow() != state.hwnd) {
        state.mouseCaptured = false;
        state.rawMouseAccumX = 0.0f;
        state.rawMouseAccumY = 0.0f;
        ClipCursor(nullptr);
        if (state.captureCursorHidden) {
            ShowCursor(TRUE);
            state.captureCursorHidden = false;
        }
        return;
    }

    RECT client{};
    if (!GetClientRect(state.hwnd, &client)) {
        return;
    }
    POINT upperLeft{client.left, client.top};
    POINT lowerRight{client.right, client.bottom};
    ClientToScreen(state.hwnd, &upperLeft);
    ClientToScreen(state.hwnd, &lowerRight);
    const RECT screenClip{upperLeft.x, upperLeft.y, lowerRight.x, lowerRight.y};

    if (!state.mouseCaptured) {
        ClipCursor(&screenClip);
        state.rawMouseAccumX = 0.0f;
        state.rawMouseAccumY = 0.0f;
        state.mouseCaptured = true;
        if (!state.captureCursorHidden) {
            ShowCursor(FALSE);
            state.captureCursorHidden = true;
        }
        return;
    }

    const float dx = state.rawMouseAccumX;
    const float dy = state.rawMouseAccumY;
    state.rawMouseAccumX = 0.0f;
    state.rawMouseAccumY = 0.0f;

    state.yawDegrees += dx * state.mouseSensitivityDegreesPerPixel;
    state.pitchDegrees =
        std::clamp(state.pitchDegrees + (dy * state.mouseSensitivityDegreesPerPixel), -84.0f, 84.0f);
}

ri::trace::MovementInput ReadMovementInput(RuntimeState& state) {
    auto axis = [](int positiveKey, int negativeKey) -> float {
        const bool positive = (GetAsyncKeyState(positiveKey) & 0x8000) != 0;
        const bool negative = (GetAsyncKeyState(negativeKey) & 0x8000) != 0;
        if (positive == negative) {
            return 0.0f;
        }
        return positive ? 1.0f : -1.0f;
    };

    const float yawRadians = ri::math::DegreesToRadians(state.yawDegrees);
    const ri::math::Vec3 forward{
        std::sin(yawRadians),
        0.0f,
        std::cos(yawRadians),
    };
    const ri::math::Vec3 right = ri::math::Normalize(ri::math::Cross(ri::math::Vec3{0.0f, 1.0f, 0.0f}, forward));

    const bool jumpHeldNow = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    const bool jumpPressedEdge = jumpHeldNow && !state.jumpHeldLastFrame;
    state.jumpHeldLastFrame = jumpHeldNow;

    return ri::trace::MovementInput{
        .moveForward = axis('W', 'S'),
        .moveRight = axis('D', 'A'),
        .viewForwardWorld = forward,
        .viewRightWorld = right,
        .sprintHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0,
        .jumpPressed = jumpPressedEdge,
        .applyShortJumpGravity = !jumpHeldNow,
    };
}

void SimulateAndApplyView(RuntimeState& state,
                          const ri::trace::MovementInput& input,
                          const float dt,
                          const double animationSeconds) {
    state.elapsedSeconds += dt;
    state.previewOptions.animationTimeSeconds = animationSeconds;
    state.bhopFeedbackCooldownSeconds = std::max(0.0, state.bhopFeedbackCooldownSeconds - dt);
    const bool wasGrounded = state.movement.onGround;

    state.movement = ri::trace::SimulateMovementControllerStep(
                         state.traceScene, state.movement, input, dt, state.movementOptions, state.activeVolumeModifiers)
                         .state;
    for (const ri::world::ConstraintAxis axis : state.activeConstraintState.lockAxes) {
        switch (axis) {
        case ri::world::ConstraintAxis::X:
            state.movement.body.velocity.x = 0.0f;
            break;
        case ri::world::ConstraintAxis::Y:
            state.movement.body.velocity.y = 0.0f;
            break;
        case ri::world::ConstraintAxis::Z:
            state.movement.body.velocity.z = 0.0f;
            break;
        }
    }

    const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
    const ri::math::Vec3 planarVelocity{
        state.movement.body.velocity.x,
        0.0f,
        state.movement.body.velocity.z,
    };
    const float planarSpeed = ri::math::Length(planarVelocity);
    const float sprintSpeedRef = std::max(0.01f, state.movementOptions.maxSprintGroundSpeed);
    const float movementNorm = std::clamp(planarSpeed / sprintSpeedRef, 0.0f, 1.0f);
    const float bobScale = (input.sprintHeld ? state.bobSprintScale : 1.0f) * movementNorm;
    const float bobPhase = static_cast<float>((state.elapsedSeconds * state.bobFrequencyHz) * 6.283185307179586);
    const float bobVertical = std::sin(bobPhase) * state.bobAmplitude * bobScale;
    const float cameraHeight = state.cameraBaseHeight + bobVertical;
    const ri::math::Vec3 eye{feet.x, feet.y + cameraHeight, feet.z};

    const bool useHeldNow = (GetAsyncKeyState('E') & 0x8000) != 0;
    const bool usePressedEdge = useHeldNow && !state.useHeldLastFrame;
    state.useHeldLastFrame = useHeldNow;
    if (usePressedEdge) {
        const ri::math::Vec3 camForward = CameraForwardWorld(state.yawDegrees, state.pitchDegrees);
        ri::world::InteractionTargetOptions interactOpts{};
        const ri::world::InteractionTargetState target =
            state.environmentService.ResolveInteractionTarget(eye, camForward, interactOpts);
        if (target.kind == ri::world::InteractionTargetKind::Door) {
            std::string feedback;
            if (state.environmentService.TryInteractWithProceduralDoor(target.targetId, true, &feedback)) {
                ri::core::LogInfo(std::string("Interact (door): ") + target.targetId
                                  + (feedback.empty() ? "" : (" — " + feedback)));
            }
        } else if (target.kind == ri::world::InteractionTargetKind::InfoPanel) {
            if (!target.interactionHook.empty() && state.logicDemoGraph != nullptr) {
                ri::logic::LogicContext ctx{};
                ctx.instigatorId = "liminal_player";
                (void)state.environmentService.ApplyWorldActorLogicInput(
                    *state.logicDemoGraph, target.targetId, target.interactionHook, ctx);
                ri::core::LogInfo("Interact (panel): " + target.targetId + " hook=" + target.interactionHook);
            } else {
                ri::core::LogInfo("Interact (panel): " + target.targetId + " prompt=\"" + target.promptText + "\"");
            }
        }
    }

    ri::scene::Node& rig = state.world.scene.GetNode(state.world.playerRig);
    rig.localTransform.position = eye;
    rig.localTransform.rotationDegrees = ri::math::Vec3{0.0f, state.yawDegrees, 0.0f};

    ri::scene::Node& cameraNode = state.world.scene.GetNode(state.world.playerCameraNode);
    cameraNode.localTransform.position = ri::math::Vec3{};
    cameraNode.localTransform.rotationDegrees = ri::math::Vec3{state.pitchDegrees, 0.0f, 0.0f};
    if (cameraNode.camera != ri::scene::kInvalidHandle) {
        const float targetFov =
            state.fovBaseDegrees + (input.sprintHeld ? state.fovSprintAddDegrees : 0.0f) + state.showcaseFovPulseDegrees;
        const float blendAlpha = std::clamp(dt * state.fovLerpPerSecond, 0.0f, 1.0f);
        state.currentFovDegrees = state.currentFovDegrees + ((targetFov - state.currentFovDegrees) * blendAlpha);
        state.world.scene.GetCamera(cameraNode.camera).fieldOfViewDegrees = state.currentFovDegrees;
    }

    AnimateWorld(state.world, animationSeconds);

    // Lightweight runtime feedback so hop chaining is easy to verify while tuning.
    const bool startedJumpFromGround = wasGrounded && !state.movement.onGround && input.jumpPressed;
    if (startedJumpFromGround) {
        if (state.bhopFeedbackCooldownSeconds <= 0.0 && planarSpeed > (state.movementOptions.maxGroundSpeed * 0.9f)) {
            state.bhopChainCount += 1;
            state.bhopFeedbackCooldownSeconds = 0.14;
            ri::core::LogInfo(
                "BHop chain=" + std::to_string(state.bhopChainCount) +
                " speed=" + std::to_string(planarSpeed));
        } else if (planarSpeed <= (state.movementOptions.maxGroundSpeed * 0.9f)) {
            state.bhopChainCount = 1;
        }
    } else if (state.movement.onGround && !input.jumpPressed) {
        state.bhopChainCount = 0;
    }
    state.wasOnGroundLastFrame = state.movement.onGround;
}

ri::trace::MovementInput BuildIdleHeadlessInput(const RuntimeState& state) {
    const float yawRadians = ri::math::DegreesToRadians(state.yawDegrees);
    const ri::math::Vec3 forward{
        std::sin(yawRadians),
        0.0f,
        std::cos(yawRadians),
    };
    const ri::math::Vec3 right = ri::math::Normalize(ri::math::Cross(ri::math::Vec3{0.0f, 1.0f, 0.0f}, forward));
    return ri::trace::MovementInput{
        .viewForwardWorld = forward,
        .viewRightWorld = right,
    };
}

HeadlessAutoplayPlan BuildHeadlessAutoplayPlan(const RuntimeState& state) {
    const float t = state.elapsedSeconds;
    if (t < 1.25f) {
        return HeadlessAutoplayPlan{
            .moveTarget = ri::math::Vec3{-2.2f, 0.0f, 11.0f},
            .lookTarget = ri::math::Vec3{-12.0f, 5.5f, 25.0f},
            .sprintHeld = true,
        };
    }
    if (t < 3.3f) {
        return HeadlessAutoplayPlan{
            .moveTarget = ri::math::Vec3{6.5f, 0.0f, 23.5f},
            .lookTarget = ri::math::Vec3{13.5f, 5.2f, 40.0f},
            .sprintHeld = true,
        };
    }
    return HeadlessAutoplayPlan{
        .moveTarget = ri::math::Vec3{5.2f, 0.0f, 24.5f},
        .lookTarget = ri::math::Vec3{0.0f, 11.4f, 88.0f},
        .sprintHeld = false,
    };
}

ri::trace::MovementInput BuildHeadlessAutoplayInput(const RuntimeState& state, const HeadlessAutoplayPlan& plan) {
    const float yawRadians = ri::math::DegreesToRadians(state.yawDegrees);
    const ri::math::Vec3 forward{
        std::sin(yawRadians),
        0.0f,
        std::cos(yawRadians),
    };
    const ri::math::Vec3 right = ri::math::Normalize(ri::math::Cross(ri::math::Vec3{0.0f, 1.0f, 0.0f}, forward));
    const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
    ri::math::Vec3 moveVector = plan.moveTarget - feet;
    moveVector.y = 0.0f;
    const float moveDistance = ri::math::Length(moveVector);
    const ri::math::Vec3 moveDirection = moveDistance > 0.05f ? (moveVector / moveDistance) : ri::math::Vec3{};
    return ri::trace::MovementInput{
        .moveForward = std::clamp(ri::math::Dot(moveDirection, forward) * 1.2f, -1.0f, 1.0f),
        .moveRight = std::clamp(ri::math::Dot(moveDirection, right) * 1.2f, -1.0f, 1.0f),
        .viewForwardWorld = forward,
        .viewRightWorld = right,
        .sprintHeld = plan.sprintHeld,
    };
}

void EnsureRuntimeDiagnosticsGeometry(RuntimeState& state) {
    state.diagnosticsLayer.Rebuild(state.environmentService);
    const ri::world::RuntimeDiagnosticsSnapshot snapshot = state.diagnosticsLayer.Snapshot();
    ri::games::SyncStandaloneRuntimeDiagnosticsScene(state.world.scene,
                                                       state.world.handles.root,
                                                       state.diagnosticsRoot,
                                                       state.diagnosticsVolumeNodes,
                                                       state.diagnosticsGizmoNodes,
                                                       snapshot);
}

void SetRuntimeDiagnosticsVisible(RuntimeState& state, const bool visible) {
    state.diagnosticsLayer.SetVisible(visible);
    if (visible) {
        EnsureRuntimeDiagnosticsGeometry(state);
        return;
    }
    ri::games::HideStandaloneRuntimeDiagnosticsScene(
        state.world.scene, state.diagnosticsVolumeNodes, state.diagnosticsGizmoNodes);
}

void SetLogicLayerVisible(RuntimeState& state, const bool visible) {
    const std::size_t count = state.world.logicDemo.logicLayerNodes.size();
    for (std::size_t i = 0; i < count; ++i) {
        const int handle = state.world.logicDemo.logicLayerNodes[i];
        if (handle == ri::scene::kInvalidHandle || handle >= static_cast<int>(state.world.scene.NodeCount())) {
            continue;
        }
        ri::scene::Node& node = state.world.scene.GetNode(handle);
        if (!visible) {
            node.localTransform.scale = ri::math::Vec3{0.01f, 0.01f, 0.01f};
            continue;
        }
        if (i < state.world.logicDemo.logicLayerVisibleScales.size()) {
            node.localTransform.scale = state.world.logicDemo.logicLayerVisibleScales[i];
        } else {
            node.localTransform.scale = ri::math::Vec3{1.0f, 1.0f, 1.0f};
        }
    }
}

void InitializeLiminalHallLogicGraph(RuntimeState& state) {
    ri::logic::LogicGraphSpec spec;
    spec.nodes.push_back(ri::logic::TriggerDetectorNode{
        .id = "logic_demo_trigger",
        .def = {.oncePerInstigator = true,
                .cooldownMs = 0,
                .instigatorFilter = ri::logic::TriggerInstigatorFilter::Player,
                .requireExitBeforeRetrigger = false,
                .startEnabled = true},
    });
    spec.nodes.push_back(ri::logic::RelayNode{.id = "logic_demo_door", .def = {}});
    spec.nodes.push_back(ri::logic::PulseNode{
        .id = "logic_demo_portal",
        .def = {.holdMs = 1,
                .retrigger = ri::logic::PulseRetriggerMode::Ignore,
                .startEnabled = true},
    });
    spec.routes.push_back(ri::logic::LogicRoute{
        .sourceId = "logic_demo_trigger",
        .outputName = "OnPass",
        .targets =
            {
                {.targetId = "logic_demo_door", .inputName = "Trigger"},
                {.targetId = "logic_demo_portal", .inputName = "Trigger"},
            },
    });

    state.logicDemoGraph = std::make_unique<ri::logic::LogicGraph>(std::move(spec));
    state.logicDemoGraph->SetOutputHandler([&state](const ri::logic::LogicOutputEvent& ev) {
        if (ev.sourceId == "logic_demo_trigger" && ev.outputName == "onpass") {
            state.logicPressurePlateTriggered = true;
        } else if (ev.sourceId == "logic_demo_door" && ev.outputName == "ontrigger") {
            state.logicDoorOpen = true;
        } else if (ev.sourceId == "logic_demo_portal" && ev.outputName == "onrise") {
            state.logicPortalSpawned = true;
            ri::core::LogInfo("Logic demo: pressure plate triggered -> door opened -> portal spawned.");
        }
    });
}

void TickLogicDemo(RuntimeState& state, const float dt) {
    const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
    const ri::spatial::Aabb feetProbe{
        .min = feet - ri::math::Vec3{0.2f, 0.1f, 0.2f},
        .max = feet + ri::math::Vec3{0.2f, 1.8f, 0.2f},
    };

    const bool plateInside = ri::spatial::Intersects(feetProbe, state.world.logicDemo.pressurePlateBounds);
    if (state.logicDemoGraph != nullptr) {
        const std::uint64_t deltaMs = std::max<std::uint64_t>(
            1u, static_cast<std::uint64_t>(std::lround(static_cast<double>(dt) * 1000.0)));
        state.logicDemoGraph->AdvanceTime(deltaMs);
        if (plateInside && !state.logicDemoPlateWasInside) {
            ri::logic::LogicContext ctx;
            ctx.instigatorId = "liminal_player";
            ctx.fields["instigatorKind"] = "player";
            state.logicDemoGraph->DispatchInput("logic_demo_trigger", "Trigger", std::move(ctx));
        }
    }
    state.logicDemoPlateWasInside = plateInside;

    if (state.world.logicDemo.doorNode != ri::scene::kInvalidHandle) {
        ri::scene::Node& doorNode = state.world.scene.GetNode(state.world.logicDemo.doorNode);
        const ri::math::Vec3 target = state.logicDoorOpen
            ? state.world.logicDemo.doorOpenPosition
            : state.world.logicDemo.doorClosedPosition;
        const float alpha = std::clamp(dt * 2.5f, 0.0f, 1.0f);
        doorNode.localTransform.position = doorNode.localTransform.position +
                                           ((target - doorNode.localTransform.position) * alpha);
    }

    if (state.world.logicDemo.portalNode != ri::scene::kInvalidHandle) {
        ri::scene::Node& portalNode = state.world.scene.GetNode(state.world.logicDemo.portalNode);
        portalNode.localTransform.scale = state.logicPortalSpawned
            ? ri::math::Vec3{2.0f, 3.0f, 0.35f}
            : ri::math::Vec3{0.01f, 0.01f, 0.01f};
    }

    if (!state.logicPortalUsed && state.logicPortalSpawned &&
        ri::spatial::Intersects(feetProbe, state.world.logicDemo.portalBounds)) {
        state.logicPortalUsed = true;
        state.movement.body.bounds = BuildPlayerBounds(state.logicSpawnPosition);
        state.movement.body.velocity = ri::math::Vec3{};
        state.yawDegrees = 0.0f;
        state.pitchDegrees = 0.0f;
        ri::core::LogInfo("Logic demo: portal used -> teleported to spawn.");
    }

    auto setNodeColor = [&](const int handle, const ri::math::Vec3& color, const ri::math::Vec3& emissive) {
        if (handle == ri::scene::kInvalidHandle || handle >= static_cast<int>(state.world.scene.NodeCount())) {
            return;
        }
        ri::scene::Node& node = state.world.scene.GetNode(handle);
        if (node.material == ri::scene::kInvalidHandle) {
            return;
        }
        ri::scene::Material& material = state.world.scene.GetMaterial(node.material);
        material.baseColor = color;
        material.emissiveColor = emissive;
    };

    const float pulse = static_cast<float>((std::sin(state.elapsedSeconds * 8.0f) * 0.5) + 0.5);
    const ri::math::Vec3 inactive{0.22f, 0.22f, 0.25f};
    const ri::math::Vec3 inactiveEmit{0.02f, 0.02f, 0.03f};
    const ri::math::Vec3 activeNode{0.2f + 0.6f * pulse, 0.9f, 0.35f + 0.45f * pulse};
    const ri::math::Vec3 activeWire{0.9f, 0.8f + 0.2f * pulse, 0.2f + 0.4f * pulse};
    const ri::math::Vec3 activeEmit{0.1f + 0.3f * pulse, 0.2f + 0.2f * pulse, 0.1f + 0.3f * pulse};

    for (const int handle : state.world.logicDemo.logicPressureVisualNodes) {
        setNodeColor(handle,
                     state.logicPressurePlateTriggered ? activeNode : inactive,
                     state.logicPressurePlateTriggered ? activeEmit : inactiveEmit);
    }
    for (const int handle : state.world.logicDemo.logicDoorVisualNodes) {
        setNodeColor(handle, state.logicDoorOpen ? activeNode : inactive, state.logicDoorOpen ? activeEmit : inactiveEmit);
    }
    for (const int handle : state.world.logicDemo.logicPortalVisualNodes) {
        setNodeColor(handle,
                     state.logicPortalSpawned ? activeNode : inactive,
                     state.logicPortalSpawned ? activeEmit : inactiveEmit);
    }
    for (const int handle : state.world.logicDemo.logicWireVisualNodes) {
        const bool active = state.logicPortalSpawned || state.logicDoorOpen || state.logicPressurePlateTriggered;
        setNodeColor(handle, active ? activeWire : inactive, active ? activeEmit : inactiveEmit);
    }
}

void TickStandaloneFrame(RuntimeState& state) {
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x0001) != 0 && state.hwnd != nullptr) {
        PostMessageW(state.hwnd, WM_CLOSE, 0, 0);
        return;
    }
    const bool ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (((GetAsyncKeyState('L') & 0x0001) != 0) && ctrlHeld && shiftHeld) {
        state.diagnosticsVisible = !state.diagnosticsVisible;
        SetRuntimeDiagnosticsVisible(state, state.diagnosticsVisible);
        SetLogicLayerVisible(state, state.diagnosticsVisible);
        ri::core::LogInfo(std::string("Debug logic layer: ")
                          + (state.diagnosticsVisible
                                 ? "visible (Ctrl+Shift+L): helpers + logic nodes/wires ON"
                                 : "hidden (Ctrl+Shift+L): helpers + logic nodes/wires OFF"));
    }

    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - state.lastTick;
    state.lastTick = now;
    // Clamp to a tighter range so simulation remains stable under hitches.
    const float dt = std::clamp(static_cast<float>(elapsed.count()), 1.0f / 180.0f, 1.0f / 45.0f);

    UpdateMouseLook(state);
    ri::trace::MovementInput input = ReadMovementInput(state);
    if (state.showcaseActive) {
        state.showcaseElapsedSeconds = std::min(state.showcaseElapsedSeconds + dt, state.showcaseDurationSeconds);
        const float phase =
            std::clamp(state.showcaseElapsedSeconds / std::max(0.1f, state.showcaseDurationSeconds), 0.0f, 1.0f);
        const float sweepYaw = std::sin(phase * 7.5f) * 28.0f;
        const float sweepPitch = -6.0f + (std::cos(phase * 10.0f) * 5.0f);
        state.yawDegrees = WrapDegrees(state.yawDegrees + (sweepYaw * dt * 0.8f));
        state.pitchDegrees = std::clamp(state.pitchDegrees + (sweepPitch * dt * 0.65f), -40.0f, 30.0f);
        const float cinematicPulse = std::sin(phase * 16.0f) * 2.0f;
        float rowPulse = 3.0f;
        if (!state.showcaseCinematics.empty()) {
            const std::size_t rowIndex = std::min<std::size_t>(
                state.showcaseCinematics.size() - 1,
                static_cast<std::size_t>(phase * static_cast<float>(state.showcaseCinematics.size())));
            rowPulse = state.showcaseCinematics[rowIndex].fovPulse;
        }
        state.showcaseFovPulseDegrees = rowPulse + cinematicPulse;
        float lightingIntensity = 1.0f;
        ri::math::Vec3 lightingColor{0.72f, 0.72f, 0.74f};
        if (!state.showcaseLighting.empty()) {
            const std::size_t lightIndex = std::min<std::size_t>(
                state.showcaseLighting.size() - 1,
                static_cast<std::size_t>(phase * static_cast<float>(state.showcaseLighting.size())));
            lightingIntensity = state.showcaseLighting[lightIndex].intensity;
            lightingColor = state.showcaseLighting[lightIndex].color;
        }
        float vfxWeight = 1.0f;
        if (!state.showcaseVfx.empty()) {
            const std::size_t vfxIndex = std::min<std::size_t>(
                state.showcaseVfx.size() - 1,
                static_cast<std::size_t>(phase * static_cast<float>(state.showcaseVfx.size())));
            vfxWeight = state.showcaseVfx[vfxIndex].weight;
        }
        const float animationScale = std::clamp(1.0f + (static_cast<float>(state.showcaseAnimationNodes) * 0.03f), 1.0f, 1.35f);
        state.nativeRenderTuning.exposure = std::clamp(
            state.showcaseBaseRenderTuning.exposure * (0.9f + (lightingIntensity * 0.22f) + (phase * 0.25f)),
            0.5f,
            2.5f);
        state.nativeRenderTuning.contrast = std::clamp(
            state.showcaseBaseRenderTuning.contrast * (1.05f + (phase * 0.28f)),
            0.7f,
            1.6f);
        state.nativeRenderTuning.saturation = std::clamp(
            state.showcaseBaseRenderTuning.saturation * (0.88f + (phase * 0.5f) + (vfxWeight * 0.18f)),
            0.0f,
            1.8f);
        state.nativeRenderTuning.fogDensity = std::clamp(
            state.showcaseBaseRenderTuning.fogDensity * (1.2f + (vfxWeight * 0.55f) + (std::sin(phase * 18.0f) * 0.12f)),
            0.0f,
            0.05f);
        state.previewOptions.fogColor = ri::math::Vec3{
            std::clamp(lightingColor.x * (0.45f + phase * 0.65f), 0.0f, 1.0f),
            std::clamp(lightingColor.y * (0.40f + phase * 0.60f), 0.0f, 1.0f),
            std::clamp(lightingColor.z * (0.55f + phase * 0.75f), 0.0f, 1.0f),
        };
        state.bobAmplitude = std::clamp(0.014f * animationScale * (1.0f + phase * 0.6f), 0.0f, 0.25f);
        state.bobFrequencyHz = std::clamp(1.7f * animationScale + (phase * 0.7f), 0.1f, 6.0f);
        input.sprintHeld = true;
        if (state.showcaseElapsedSeconds >= state.showcaseDurationSeconds) {
            state.showcaseActive = false;
            state.showcaseFovPulseDegrees = 0.0f;
            state.nativeRenderTuning = state.showcaseBaseRenderTuning;
            SetRuntimeDiagnosticsVisible(state, state.showcaseDiagnosticsWasVisible);
            SetLogicLayerVisible(state, state.showcaseDiagnosticsWasVisible);
            ri::core::LogInfo("Liminal showcase sequence complete; gameplay camera restored.");
        }
    }
    ApplyEnvironmentAuthoringVolumes(state, dt);
    ProcessPendingDoorTransitions(state);
    SimulateAndApplyView(state, input, dt, static_cast<double>(GetTickCount64()) / 1000.0);
    TickLogicDemo(state, dt);
}

bool RunStandaloneNativeVulkanLoop(const StandaloneOptions& options, RuntimeState& state, std::string* error) {
    state.lastTick = std::chrono::steady_clock::now();
    state.hwnd = nullptr;
    const bool benchmarking = options.benchmarkFrames > 0;
    int benchmarkedFrames = 0;
    const auto benchmarkStart = std::chrono::steady_clock::now();

    const fs::path textureRootForVulkan = state.previewOptions.textureRoot.value_or(fs::path{});
    const ri::render::vulkan::VulkanPreviewWindowOptions windowOptions{
        .windowTitle = options.windowTitle,
        .presentModePreference = ToVulkanPresentModePreference(options.presentMode),
        .textureRoot = textureRootForVulkan,
        .messageUserData = &state,
        .onWin32Message = &LiminalStandaloneWin32Hook,
        .outClientHwnd = &state.hwnd,
    };

    const ri::render::vulkan::VulkanNativeSceneFrameCallback buildFrame =
        [&state, &options, &benchmarkedFrames, &textureRootForVulkan](ri::render::vulkan::VulkanNativeSceneFrame& frame,
                                                                     std::string*) {
            TickStandaloneFrame(state);
            frame.scene = &state.world.scene;
            frame.cameraNode = state.world.playerCameraNode;
            frame.textureRoot = textureRootForVulkan;
            frame.skyEquirectTextureRelative = state.nativeSkyEquirectRelative;
            frame.animationTimeSeconds = static_cast<double>(state.elapsedSeconds);
            frame.renderQualityTier = state.nativeRenderTuning.qualityTier;
            frame.renderExposure = state.nativeRenderTuning.exposure;
            frame.renderContrast = state.nativeRenderTuning.contrast;
            frame.renderSaturation = state.nativeRenderTuning.saturation;
            frame.renderFogDensity = state.nativeRenderTuning.fogDensity;
            if (options.benchmarkFrames > 0) {
                ++benchmarkedFrames;
                if (benchmarkedFrames >= options.benchmarkFrames && state.hwnd != nullptr) {
                    PostMessageW(state.hwnd, WM_CLOSE, 0, 0);
                }
            }
            return true;
        };

    const bool ok = ri::render::vulkan::RunVulkanNativeSceneLoop(
        options.width,
        options.height,
        buildFrame,
        windowOptions,
        error);
    if (benchmarking && ok) {
        LogBenchmarkResults("vulkan", benchmarkedFrames, benchmarkStart, std::chrono::steady_clock::now());
    }
    state.hwnd = nullptr;
    return ok;
}

bool InitializeRuntimeState(const StandaloneOptions& options,
                            const ri::content::GameManifest& manifest,
                            RuntimeState& state) {
    state.gameRoot = manifest.rootPath;
    std::string audioBackendError;
    std::shared_ptr<ri::audio::AudioBackend> audioBackend = ri::audio::CreateMiniaudioAudioBackend(&audioBackendError);
    if (audioBackend != nullptr) {
        state.audioManager = std::make_shared<ri::audio::AudioManager>(audioBackend);
    } else if (!audioBackendError.empty()) {
        ri::core::LogInfo("Audio backend unavailable: " + audioBackendError);
    }
    const PreviewResolution previewRes = ComputeSoftwarePreviewResolution(options);
    (void)previewRes;
    ri::core::LogInfo(
        "Vulkan swapchain " + std::to_string(options.width) + "x" + std::to_string(options.height)
        + " (native textured GPU path, mesh buffers cached on GPU)");

    const ri::content::ScriptScalarMap gameplay =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/gameplay.riscript"));
    const ri::content::ScriptScalarMap rendering =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/rendering.riscript"));
    const ri::content::ScriptScalarMap ui =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/ui.riscript"));
    const ri::content::ScriptScalarMap audio =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/audio.riscript"));
    const ri::content::ScriptScalarMap streaming =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/streaming.riscript"));
    const ri::content::ScriptScalarMap localization =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/localization.riscript"));
    const ri::content::ScriptScalarMap physics =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/physics.riscript"));
    const ri::content::ScriptScalarMap postprocess =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/postprocess.riscript"));
    const ri::content::ScriptScalarMap init =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/init.riscript"));
    const ri::content::ScriptScalarMap gameCfg =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "config/game.cfg"));
    const ri::content::ScriptScalarMap network =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/network.riscript"));
    const ri::content::ScriptScalarMap persistence =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/persistence.riscript"));
    const ri::content::ScriptScalarMap ai =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/ai.riscript"));
    const ri::content::ScriptScalarMap plugins =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/plugins.riscript"));
    const ri::content::ScriptScalarMap animation =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/animation.riscript"));
    const ri::content::ScriptScalarMap vfx =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "scripts/vfx.riscript"));
    const ri::content::ScriptScalarMap networkCfg =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "config/network.cfg"));
    const ri::content::ScriptScalarMap buildProfile =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "config/build.profile"));
    const ri::content::ScriptScalarMap securityPolicy =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "config/security.policy"));
    const ri::content::ScriptScalarMap pluginsPolicy =
        ri::content::LoadScriptScalars(ri::content::ResolveGameAssetPath(manifest.rootPath, "config/plugins.policy"));
    const LiminalDemoExtensions demoExtensions = LoadLiminalDemoExtensions(manifest);
    if (gameplay.empty()) {
        ri::core::LogInfo("Gameplay tuning script not found or empty; using defaults.");
    }
    if (rendering.empty()) {
        ri::core::LogInfo("Rendering tuning script not found or empty; using defaults.");
    }
    if (ui.empty()) {
        ri::core::LogInfo("UI tuning script not found or empty; using defaults.");
    }
    if (audio.empty()) {
        ri::core::LogInfo("Audio tuning script not found or empty; using defaults.");
    }
    if (streaming.empty()) {
        ri::core::LogInfo("Streaming tuning script not found or empty; using defaults.");
    }
    if (localization.empty()) {
        ri::core::LogInfo("Localization tuning script not found or empty; using defaults.");
    }
    if (physics.empty()) {
        ri::core::LogInfo("Physics tuning script not found or empty; using defaults.");
    }
    if (postprocess.empty()) {
        ri::core::LogInfo("Postprocess tuning script not found or empty; using defaults.");
    }
    if (init.empty()) {
        ri::core::LogInfo("Init tuning script not found or empty; using defaults.");
    }
    if (gameCfg.empty()) {
        ri::core::LogInfo("Game cfg not found or empty; using defaults.");
    }
    if (network.empty()) {
        ri::core::LogInfo("Network tuning script not found or empty; using defaults.");
    }
    if (persistence.empty()) {
        ri::core::LogInfo("Persistence tuning script not found or empty; using defaults.");
    }
    if (ai.empty()) {
        ri::core::LogInfo("AI tuning script not found or empty; using defaults.");
    }
    if (plugins.empty()) {
        ri::core::LogInfo("Plugins tuning script not found or empty; using defaults.");
    }
    if (animation.empty()) {
        ri::core::LogInfo("Animation tuning script not found or empty; using defaults.");
    }
    if (vfx.empty()) {
        ri::core::LogInfo("VFX tuning script not found or empty; using defaults.");
    }
    if (networkCfg.empty()) {
        ri::core::LogInfo("Network cfg not found or empty; using defaults.");
    }
    if (buildProfile.empty()) {
        ri::core::LogInfo("Build profile not found or empty; using defaults.");
    }
    if (securityPolicy.empty()) {
        ri::core::LogInfo("Security policy not found or empty; using defaults.");
    }
    if (pluginsPolicy.empty()) {
        ri::core::LogInfo("Plugins policy not found or empty; using defaults.");
    }

    state.world = BuildWorld(manifest.name.empty() ? "LiminalHall" : manifest.name, manifest.rootPath);
    InitializeLiminalHallLogicGraph(state);
    const ri::spatial::SpatialIndexOptions bspOptions{
        .maxLeafSize = static_cast<std::size_t>(
            ri::content::ScriptScalarOrIntClamped(gameplay, "bsp_max_leaf_size", 12, 2, 128)),
        .maxDepth = static_cast<std::size_t>(
            ri::content::ScriptScalarOrIntClamped(gameplay, "bsp_max_depth", 10, 1, 24)),
    };
    state.traceScene = ri::trace::TraceScene(state.world.colliders, bspOptions);
    ri::core::LogInfo("Trace collider count: " + std::to_string(state.traceScene.ColliderCount()));
    const ri::spatial::SpatialIndexMetrics staticMetrics = state.traceScene.StaticIndexMetrics();
    const ri::spatial::SpatialIndexMetrics structuralMetrics = state.traceScene.StructuralIndexMetrics();
    ri::core::LogInfo(
        "Trace BSP options: maxLeaf=" + std::to_string(bspOptions.maxLeafSize)
        + " maxDepth=" + std::to_string(bspOptions.maxDepth));
    ri::core::LogInfo(
        "Trace BSP static entries=" + std::to_string(staticMetrics.lastRebuildEntryCount)
        + " structural entries=" + std::to_string(structuralMetrics.lastRebuildEntryCount));
    {
        std::string colliderIds;
        for (std::size_t index = 0; index < state.world.colliders.size(); ++index) {
            if (index > 0U) {
                colliderIds += ",";
            }
            colliderIds += state.world.colliders[index].id;
        }
        ri::core::LogInfo("Trace colliders: " + colliderIds);
    }
    ri::games::SeedStandaloneDiagnosticsEnvironmentFromColliders(state.world.colliders, state.environmentService);
    state.diagnosticsLayer.Rebuild(state.environmentService);
    state.diagnosticsVisible = ri::content::ScriptScalarOrBool(ui, "show_runtime_diagnostics", false);
    SetRuntimeDiagnosticsVisible(state, state.diagnosticsVisible);
    SetLogicLayerVisible(state, state.diagnosticsVisible);
    ri::core::LogInfo(
        "UI tuning: diagnostics="
        + std::string(state.diagnosticsVisible ? "on" : "off")
        + " objectivePanel="
        + std::string(ri::content::ScriptScalarOrBool(ui, "show_objective_panel", true) ? "on" : "off")
        + " crosshairMode="
        + std::to_string(ri::content::ScriptScalarOrIntClamped(ui, "crosshair_mode", 1, 0, 4))
        + " crosshairScale="
        + std::to_string(ri::content::ScriptScalarOrClamped(ui, "crosshair_scale", 1.0f, 0.1f, 4.0f))
        + " hudVariant="
        + std::to_string(ri::content::ScriptScalarOrIntClamped(ui, "hud_style_variant", 1, 0, 8)));
    ri::core::LogInfo(
        "Game cfg: runtimeProfile="
        + std::to_string(ri::content::ScriptScalarOrIntClamped(gameCfg, "runtime_profile", 1, 0, 16))
        + " editorProfile="
        + std::to_string(ri::content::ScriptScalarOrIntClamped(gameCfg, "editor_profile", 1, 0, 16)));
    ri::core::LogInfo(
        "Audio tuning: masterGain="
        + std::to_string(ri::content::ScriptScalarOrClamped(audio, "audio_master_gain", 1.0f, 0.0f, 4.0f))
        + " envBlend="
        + std::to_string(ri::content::ScriptScalarOrClamped(audio, "audio_environment_blend", 1.0f, 0.0f, 2.0f)));
    ri::core::LogInfo(
        "Streaming tuning: budgetScale="
        + std::to_string(ri::content::ScriptScalarOrClamped(streaming, "streaming_budget_scale", 1.0f, 0.1f, 8.0f))
        + " checkpointAutosave="
        + std::string(ri::content::ScriptScalarOrBool(streaming, "checkpoint_autosave_enabled", true) ? "on" : "off"));
    ri::core::LogInfo(
        "Localization tuning: defaultLocale="
        + std::to_string(ri::content::ScriptScalarOrIntClamped(localization, "default_locale", 0, 0, 16))
        + " fallbackLocale="
        + std::to_string(ri::content::ScriptScalarOrIntClamped(localization, "fallback_locale", 0, 0, 16)));
    ri::core::LogInfo(
        "Physics tuning: gravityScale="
        + std::to_string(ri::content::ScriptScalarOrClamped(physics, "global_gravity_scale", 1.0f, 0.1f, 4.0f))
        + " dragScale="
        + std::to_string(ri::content::ScriptScalarOrClamped(physics, "global_drag_scale", 1.0f, 0.1f, 4.0f)));
    ri::core::LogInfo(
        "Postprocess tuning: quality="
        + std::to_string(ri::content::ScriptScalarOrIntClamped(postprocess, "postprocess_quality", 1, 0, 3))
        + " tintStrength="
        + std::to_string(
            ri::content::ScriptScalarOrClamped(postprocess, "postprocess_tint_strength", 0.0f, 0.0f, 1.0f)));
    ri::core::LogInfo(
        "Init tuning: warmupFrames="
        + std::to_string(ri::content::ScriptScalarOrIntClamped(init, "warmup_frames", 2, 0, 120))
        + " precache="
        + std::string(ri::content::ScriptScalarOrBool(init, "precache_enabled", true) ? "on" : "off"));
    ri::core::LogInfo(
        "Runtime ext tuning: networkKeys=" + std::to_string(network.size())
        + " persistenceKeys=" + std::to_string(persistence.size())
        + " aiKeys=" + std::to_string(ai.size())
        + " pluginsKeys=" + std::to_string(plugins.size())
        + " animationKeys=" + std::to_string(animation.size())
        + " vfxKeys=" + std::to_string(vfx.size())
        + " networkCfgKeys=" + std::to_string(networkCfg.size())
        + " buildProfileKeys=" + std::to_string(buildProfile.size())
        + " securityPolicyKeys=" + std::to_string(securityPolicy.size())
        + " pluginsPolicyKeys=" + std::to_string(pluginsPolicy.size()));
    ri::core::LogInfo(
        "Runtime ext files: navmesh="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "levels/assembly.navmesh"))
        + " zones="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "levels/assembly.zones.csv"))
        + " aiNodes="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "levels/assembly.ai.nodes"))
        + " dependencies="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "assets/dependencies.json"))
        + " streamingManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "assets/streaming.manifest"))
        + " shadersManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "assets/shaders.manifest"))
        + " schemaDb="
        + DescribeOptionalAssetState(
            ri::content::ResolveGameAssetPath(manifest.rootPath, "data/schema.db"),
            true)
        + " lookup="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "data/lookup.index"))
        + " entityRegistry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "data/entity.registry"))
        + " aiBehavior="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "ai/behavior.tree"))
        + " aiBlackboard="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "ai/blackboard.json"))
        + " aiFactions="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "ai/factions.cfg"))
        + " lighting="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "levels/assembly.lighting.csv"))
        + " cinematics="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "levels/assembly.cinematics.csv"))
        + " pluginsManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "plugins/manifest.plugins"))
        + " pluginsRegistry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "plugins/registry.json"))
        + " animationGraph="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "assets/animation.graph"))
        + " vfxManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "assets/vfx.manifest"))
        + " telemetry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest.rootPath, "data/telemetry.db"), true));
    ri::core::LogInfo(
        "Liminal showcase rows: lighting=" + std::to_string(demoExtensions.lightingRowCount)
        + " cinematics=" + std::to_string(demoExtensions.cinematicsRowCount)
        + " aiNodes=" + std::to_string(demoExtensions.aiNodeRows)
        + " entityRegistry=" + std::to_string(demoExtensions.entityRegistryRows)
        + " shaders=" + std::to_string(demoExtensions.shaderRows)
        + " pluginEntries=" + std::to_string(demoExtensions.pluginCount)
        + " hookEntries=" + std::to_string(demoExtensions.pluginHookCount)
        + " animationNodes=" + std::to_string(demoExtensions.animationGraphNodeCount)
        + " vfxEntries=" + std::to_string(demoExtensions.vfxEntryCount)
        + " telemetryHeaderOk=" + std::to_string(demoExtensions.telemetryHeaderValid));
    state.movementOptions.simulateStamina = false;
    state.movementOptions.maxGroundSpeed = ri::content::ScriptScalarOr(gameplay, "walk_speed", 4.8f);
    state.movementOptions.maxSprintGroundSpeed = ri::content::ScriptScalarOr(gameplay, "sprint_speed", 7.5f);
    state.movementOptions.maxAirSpeed = ri::content::ScriptScalarOr(gameplay, "air_speed", 7.5f);
    state.movementOptions.jumpSpeed = ri::content::ScriptScalarOr(gameplay, "jump_speed", 7.2f);
    state.movementOptions.gravity = ri::content::ScriptScalarOr(
        physics,
        "movement_gravity",
        ri::content::ScriptScalarOr(gameplay, "gravity", 26.0f));
    state.movementOptions.fallGravityMultiplier = ri::content::ScriptScalarOr(
        physics,
        "movement_fall_gravity_multiplier",
        ri::content::ScriptScalarOr(gameplay, "fall_gravity_multiplier", 1.3f));
    state.movementOptions.groundAcceleration =
        ri::content::ScriptScalarOr(gameplay, "ground_acceleration", state.movementOptions.groundAcceleration);
    state.movementOptions.airAcceleration =
        ri::content::ScriptScalarOr(gameplay, "air_acceleration", state.movementOptions.airAcceleration);
    state.movementOptions.groundFriction =
        ri::content::ScriptScalarOr(gameplay, "ground_friction", state.movementOptions.groundFriction);
    state.movementOptions.stopSpeed = ri::content::ScriptScalarOr(gameplay, "stop_speed", state.movementOptions.stopSpeed);
    state.movementOptions.airControl =
        ri::content::ScriptScalarOrClamped(gameplay, "air_control", state.movementOptions.airControl, 0.0f, 1.0f);
    state.movementOptions.coyoteTimeSeconds = ri::content::ScriptScalarOrClamped(
        gameplay, "coyote_time", state.movementOptions.coyoteTimeSeconds, 0.0f, 0.5f);
    state.movementOptions.jumpBufferTimeSeconds = ri::content::ScriptScalarOrClamped(
        gameplay, "jump_buffer_time", state.movementOptions.jumpBufferTimeSeconds, 0.0f, 0.5f);
    state.movementOptions.lowJumpGravityMultiplier = ri::content::ScriptScalarOrClamped(
        gameplay, "low_jump_gravity_multiplier", state.movementOptions.lowJumpGravityMultiplier, 1.0f, 4.0f);
    state.movementOptions.maxFallSpeed = ri::content::ScriptScalarOrClamped(
        gameplay, "max_fall_speed", state.movementOptions.maxFallSpeed, 4.0f, 120.0f);
    state.movementOptions.groundProbeJumpMaxDown = ri::content::ScriptScalarOrClamped(
        gameplay,
        "ground_probe_jump_max_down",
        state.movementOptions.groundProbeJumpMaxDown,
        0.0f,
        1.5f);
    state.movementOptions.airTurnResponsiveness = ri::content::ScriptScalarOrClamped(
        gameplay,
        "air_turn_responsiveness",
        state.movementOptions.airTurnResponsiveness,
        0.0f,
        4.0f);
    state.movementOptions.airStrafeAccelerationBoost = ri::content::ScriptScalarOrClamped(
        gameplay,
        "air_strafe_accel_boost",
        state.movementOptions.airStrafeAccelerationBoost,
        0.5f,
        3.5f);
    state.movementOptions.groundAdhesionSpeed = ri::content::ScriptScalarOrClamped(
        gameplay,
        "ground_adhesion_speed",
        state.movementOptions.groundAdhesionSpeed,
        0.0f,
        8.0f);
    state.movementOptions.enableWallJump =
        ri::content::ScriptScalarOrBool(gameplay, "wall_jump_enabled", state.movementOptions.enableWallJump);
    state.movementOptions.wallJumpProbeDistance = ri::content::ScriptScalarOrClamped(
        gameplay,
        "wall_jump_probe_distance",
        state.movementOptions.wallJumpProbeDistance,
        0.1f,
        1.8f);
    state.movementOptions.wallJumpVerticalSpeed = ri::content::ScriptScalarOrClamped(
        gameplay,
        "wall_jump_vertical_speed",
        state.movementOptions.wallJumpVerticalSpeed,
        2.0f,
        16.0f);
    state.movementOptions.wallJumpAwaySpeed = ri::content::ScriptScalarOrClamped(
        gameplay,
        "wall_jump_away_speed",
        state.movementOptions.wallJumpAwaySpeed,
        1.0f,
        16.0f);
    state.movementOptions.wallJumpCarry = ri::content::ScriptScalarOrClamped(
        gameplay,
        "wall_jump_carry",
        state.movementOptions.wallJumpCarry,
        0.0f,
        1.0f);
    const float physJumpScale =
        ri::content::ScriptScalarOrClamped(physics, "global_jump_scale", 1.0f, 0.65f, 1.35f);
    state.movementOptions.jumpVolumeScale =
        std::clamp(state.movementOptions.jumpVolumeScale * physJumpScale, 0.65f, 1.35f);
    const float physAirCtrlScale =
        ri::content::ScriptScalarOrClamped(physics, "global_air_control_scale", 1.0f, 0.75f, 1.35f);
    state.movementOptions.airControl =
        std::clamp(state.movementOptions.airControl * physAirCtrlScale, 0.0f, 1.0f);
    const float aiSprintScale = ri::content::ScriptScalarOrClamped(ai, "ai_alert_speed_scale", 1.0f, 0.6f, 1.6f);
    state.movementOptions.maxSprintGroundSpeed =
        std::clamp(state.movementOptions.maxSprintGroundSpeed * aiSprintScale, 3.0f, 14.0f);
    const float movementSpeedScale = ri::content::ScriptScalarOrClamped(
        gameplay,
        "movement_speed_scale",
        0.82f,
        0.45f,
        1.3f);
    state.movementOptions.maxGroundSpeed =
        std::clamp(state.movementOptions.maxGroundSpeed * movementSpeedScale, 2.0f, 12.0f);
    state.movementOptions.maxSprintGroundSpeed =
        std::clamp(state.movementOptions.maxSprintGroundSpeed * movementSpeedScale, 3.0f, 16.0f);
    state.movementOptions.maxAirSpeed =
        std::clamp(state.movementOptions.maxAirSpeed * movementSpeedScale, 2.0f, 16.0f);
    const float animationBobScale = ri::content::ScriptScalarOrClamped(animation, "animation_bob_scale", 1.0f, 0.5f, 2.0f);
    state.bobAmplitude = std::clamp(state.bobAmplitude * animationBobScale, 0.0f, 0.25f);
    const float vfxFogScale = ri::content::ScriptScalarOrClamped(vfx, "vfx_fog_scale", 1.0f, 0.5f, 1.8f);
    state.nativeRenderTuning.fogDensity = std::clamp(state.nativeRenderTuning.fogDensity * vfxFogScale, 0.0f, 0.05f);
    const bool pluginOrderBoost = ri::content::ScriptScalarOrBool(plugins, "plugin_render_priority_boost", false)
        && ri::content::ScriptScalarOrBool(pluginsPolicy, "allow_runtime_plugin_overrides", true)
        && demoExtensions.pluginCount > 0;
    if (pluginOrderBoost) {
        state.nativeRenderTuning.qualityTier = std::clamp(state.nativeRenderTuning.qualityTier + 1, 0, 2);
    }
    if (demoExtensions.cinematicsRowCount > 0) {
        state.fovLerpPerSecond = std::clamp(state.fovLerpPerSecond + 1.5f, 0.5f, 40.0f);
    }
    if (demoExtensions.lightingRowCount > 0) {
        state.nativeRenderTuning.exposure = std::clamp(state.nativeRenderTuning.exposure + 0.03f, 0.5f, 2.5f);
    }
    state.authoredMovementOptions = state.movementOptions;
    state.movement.onGround = true;
    const SpawnSetup spawn = ResolveSpawnSetup(options, manifest, gameplay);
    state.movement.body.bounds = BuildPlayerBounds(spawn.position);
    state.logicSpawnPosition = spawn.position;
    state.yawDegrees = spawn.yaw;
    state.pitchDegrees = spawn.pitch;
    {
        const ri::math::Vec3 center = ri::spatial::Center(state.movement.body.bounds);
        const std::optional<ri::trace::TraceHit> groundProbe = state.traceScene.FindGroundHit(
            center,
            ri::trace::GroundTraceOptions{
                .maxDistance = 2.0f,
                .structuralOnly = true,
                .minNormalY = 0.5f,
            });
        if (groundProbe.has_value()) {
            ri::core::LogInfo(
                "Spawn ground probe: hit=" + groundProbe->id +
                " y=" + std::to_string(groundProbe->point.y) +
                " distance=" + std::to_string(center.y - groundProbe->point.y));
        } else {
            ri::core::LogInfo("Spawn ground probe: no hit");
        }
        const std::vector<std::string> spawnCandidates = state.traceScene.QueryCollidablesForBox(
            ri::spatial::Aabb{
                .min = {center.x - 0.5f, center.y - 2.0f, center.z - 0.5f},
                .max = {center.x + 0.5f, center.y + 0.2f, center.z + 0.5f},
            },
            true);
        std::string colliderList;
        for (std::size_t index = 0; index < spawnCandidates.size(); ++index) {
            if (index > 0U) {
                colliderList += ",";
            }
            colliderList += spawnCandidates[index];
        }
        ri::core::LogInfo("Spawn collider candidates: " + (colliderList.empty() ? std::string("none") : colliderList));
    }

    state.previewOptions.width = previewRes.width;
    state.previewOptions.height = previewRes.height;
    state.previewOptions.pointSampleTextures = false;
    state.previewOptions.adaptiveTextureSampling = true;
    state.previewOptions.adaptivePointSampleStartDepth = 34.0f;
    state.previewOptions.enableFarHorizon = true;
    state.previewOptions.farHorizonStartDistance = 64.0f;
    state.previewOptions.farHorizonEndDistance = 190.0f;
    state.previewOptions.farHorizonMaxDistance = 340.0f;
    state.previewOptions.farHorizonMaxNodeStride = 3U;
    state.previewOptions.farHorizonMaxInstanceStride = 4U;
    state.previewOptions.clearTop = ri::math::Vec3{
        ri::content::ScriptScalarOr(rendering, "clear_top_r", 0.22f),
        ri::content::ScriptScalarOr(rendering, "clear_top_g", 0.22f),
        ri::content::ScriptScalarOr(rendering, "clear_top_b", 0.19f),
    };
    state.previewOptions.clearBottom = ri::math::Vec3{
        ri::content::ScriptScalarOr(rendering, "clear_bottom_r", 0.35f),
        ri::content::ScriptScalarOr(rendering, "clear_bottom_g", 0.34f),
        ri::content::ScriptScalarOr(rendering, "clear_bottom_b", 0.30f),
    };
    state.previewOptions.fogColor = ri::math::Vec3{
        ri::content::ScriptScalarOr(rendering, "fog_r", 0.62f),
        ri::content::ScriptScalarOr(rendering, "fog_g", 0.61f),
        ri::content::ScriptScalarOr(rendering, "fog_b", 0.55f),
    };
    state.previewOptions.ambientLight = ri::math::Vec3{
        ri::content::ScriptScalarOr(rendering, "ambient_r", 0.18f),
        ri::content::ScriptScalarOr(rendering, "ambient_g", 0.18f),
        ri::content::ScriptScalarOr(rendering, "ambient_b", 0.16f),
    };
    state.nativeRenderTuning = BaseNativeRenderTuning(options.renderQuality);
    state.nativeRenderTuning.exposure = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_exposure",
        ri::content::ScriptScalarOrClamped(
            rendering, "native_exposure", state.nativeRenderTuning.exposure, 0.5f, 2.5f),
        0.5f,
        2.5f);
    state.nativeRenderTuning.contrast = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_contrast",
        ri::content::ScriptScalarOrClamped(
            rendering, "native_contrast", state.nativeRenderTuning.contrast, 0.7f, 1.6f),
        0.7f,
        1.6f);
    state.nativeRenderTuning.saturation = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_saturation",
        ri::content::ScriptScalarOrClamped(
            rendering, "native_saturation", state.nativeRenderTuning.saturation, 0.0f, 1.8f),
        0.0f,
        1.8f);
    state.nativeRenderTuning.fogDensity = ri::content::ScriptScalarOrClamped(
        postprocess,
        "native_fog_density",
        ri::content::ScriptScalarOrClamped(
            rendering, "native_fog_density", state.nativeRenderTuning.fogDensity, 0.0f, 0.05f),
        0.0f,
        0.05f);
    state.cameraBaseHeight =
        ri::content::ScriptScalarOrClamped(gameplay, "camera_height", state.cameraBaseHeight, 0.8f, 2.2f);
    state.bobAmplitude =
        ri::content::ScriptScalarOrClamped(gameplay, "head_bob_amplitude", state.bobAmplitude, 0.0f, 0.2f);
    state.bobFrequencyHz =
        ri::content::ScriptScalarOrClamped(gameplay, "head_bob_frequency", state.bobFrequencyHz, 0.1f, 6.0f);
    state.bobSprintScale =
        ri::content::ScriptScalarOrClamped(gameplay, "head_bob_sprint_scale", state.bobSprintScale, 1.0f, 3.0f);
    state.fovBaseDegrees = ri::content::ScriptScalarOrClamped(
        postprocess,
        "fov_base",
        ri::content::ScriptScalarOrClamped(rendering, "fov_base", state.fovBaseDegrees, 45.0f, 120.0f),
        45.0f,
        120.0f);
    state.fovSprintAddDegrees = ri::content::ScriptScalarOrClamped(
        postprocess,
        "fov_sprint_add",
        ri::content::ScriptScalarOrClamped(rendering, "fov_sprint_add", state.fovSprintAddDegrees, 0.0f, 25.0f),
        0.0f,
        25.0f);
    state.fovLerpPerSecond = ri::content::ScriptScalarOrClamped(
        postprocess,
        "fov_lerp_per_second",
        ri::content::ScriptScalarOrClamped(rendering, "fov_lerp_per_second", state.fovLerpPerSecond, 0.5f, 40.0f),
        0.5f,
        40.0f);
    state.currentFovDegrees = state.fovBaseDegrees;
    const float scriptedSensitivity = ri::content::ScriptScalarOr(gameplay, "mouse_sensitivity", 0.12f);
    state.mouseSensitivityDegreesPerPixel = std::clamp(scriptedSensitivity, 0.01f, 2.0f);
    if (options.mouseSensitivityDegreesPerPixel.has_value()) {
        state.mouseSensitivityDegreesPerPixel =
            std::clamp(*options.mouseSensitivityDegreesPerPixel, 0.01f, 2.0f);
    }
    state.captureMouse = options.captureMouse;
    ri::core::LogInfo("Mouse sensitivity: " + std::to_string(state.mouseSensitivityDegreesPerPixel));
    ri::core::LogInfo(std::string("Mouse capture: ") + (state.captureMouse ? "enabled" : "disabled"));
    ri::core::LogInfo(
        "Movement tuning walk=" + std::to_string(state.movementOptions.maxGroundSpeed) +
        " sprint=" + std::to_string(state.movementOptions.maxSprintGroundSpeed) +
        " accel=" + std::to_string(state.movementOptions.groundAcceleration));
    ri::core::LogInfo(
        "Movement parkour profile airTurn=" + std::to_string(state.movementOptions.airTurnResponsiveness) +
        " strafeBoost=" + std::to_string(state.movementOptions.airStrafeAccelerationBoost) +
        " coyote=" + std::to_string(state.movementOptions.coyoteTimeSeconds) +
        " jumpBuffer=" + std::to_string(state.movementOptions.jumpBufferTimeSeconds) +
        " edgeProbe=" + std::to_string(state.movementOptions.groundProbeJumpMaxDown) +
        " groundAdhesion=" + std::to_string(state.movementOptions.groundAdhesionSpeed) +
        " wallJump=" + std::string(state.movementOptions.enableWallJump ? "on" : "off") +
        " wallProbe=" + std::to_string(state.movementOptions.wallJumpProbeDistance) +
        " wallUp=" + std::to_string(state.movementOptions.wallJumpVerticalSpeed) +
        " wallAway=" + std::to_string(state.movementOptions.wallJumpAwaySpeed));
    ri::core::LogInfo(
        "View tuning fovBase=" + std::to_string(state.fovBaseDegrees) +
        " fovSprintAdd=" + std::to_string(state.fovSprintAddDegrees) +
        " bobAmp=" + std::to_string(state.bobAmplitude));
    ri::core::LogInfo(
        "Showcase influences aiSprintScale=" + std::to_string(aiSprintScale) +
        " animationBobScale=" + std::to_string(animationBobScale) +
        " vfxFogScale=" + std::to_string(vfxFogScale) +
        " pluginRenderBoost=" + std::string(pluginOrderBoost ? "on" : "off"));
    ri::core::LogInfo(
        std::string("Native Vulkan quality: ") + RenderQualityName(options.renderQuality) +
        " tier=" + std::to_string(state.nativeRenderTuning.qualityTier) +
        " exposure=" + std::to_string(state.nativeRenderTuning.exposure) +
        " contrast=" + std::to_string(state.nativeRenderTuning.contrast) +
        " saturation=" + std::to_string(state.nativeRenderTuning.saturation) +
        " fogDensity=" + std::to_string(state.nativeRenderTuning.fogDensity));
    ri::core::LogInfo("Native Vulkan realtime lighting: directional shadow map=2048, local light=enabled");
    state.showcaseBaseRenderTuning = state.nativeRenderTuning;
    state.showcaseLighting = LoadShowcaseLightingRows(manifest.rootPath / "levels" / "assembly.lighting.csv");
    state.showcaseCinematics = LoadShowcaseCinematicRows(manifest.rootPath / "levels" / "assembly.cinematics.csv");
    state.showcaseVfx = LoadShowcaseVfxEntries(manifest.rootPath / "assets" / "vfx.manifest");
    state.showcaseAnimationNodes = LoadAnimationGraphNodeCount(manifest.rootPath / "assets" / "animation.graph");
    state.showcaseEnabled = ri::content::ScriptScalarOrBool(init, "liminal_showcase_enabled", true);
    state.showcaseDurationSeconds =
        ri::content::ScriptScalarOrClamped(init, "liminal_showcase_duration_s", 10.0f, 5.0f, 15.0f);
    state.showcaseActive = state.showcaseEnabled;
    if (state.showcaseActive) {
        state.showcaseDiagnosticsWasVisible = state.diagnosticsVisible;
        SetRuntimeDiagnosticsVisible(state, true);
        SetLogicLayerVisible(state, true);
        ri::core::LogSection("Liminal Showcase");
        ri::core::LogInfo("Startup showcase ON: cinematic sweep + exposure/fog/FOV pulses + debug overlay.");
        ri::core::LogInfo(
            "Showcase data hooks: lightingRows=" + std::to_string(state.showcaseLighting.size()) +
            " cinematicRows=" + std::to_string(state.showcaseCinematics.size()) +
            " vfxEntries=" + std::to_string(state.showcaseVfx.size()) +
            " animationNodes=" + std::to_string(state.showcaseAnimationNodes) +
            " durationS=" + std::to_string(state.showcaseDurationSeconds));
    } else {
        ri::core::LogInfo("Startup showcase disabled by scalar (liminal_showcase_enabled=0).");
    }

    const fs::path workspaceForTextures =
        !options.workspaceRoot.empty() ? options.workspaceRoot : ri::content::DetectWorkspaceRoot(manifest.rootPath);
    fs::path liminalExe{};
    wchar_t moduleWide[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, moduleWide, MAX_PATH) > 0) {
        liminalExe = fs::path(std::wstring(moduleWide));
    }
    const fs::path textureDir = ri::content::PickEngineTexturesDirectory(workspaceForTextures, liminalExe);
    if (!textureDir.empty()) {
        state.previewOptions.textureRoot = textureDir;
        state.nativeSkyEquirectRelative = PickSkiesEquirectRelative(textureDir);
        ri::core::LogInfo("Texture library: " + textureDir.string());
        if (!state.nativeSkyEquirectRelative.empty()) {
            ri::core::LogInfo("Native Vulkan sky texture: " + state.nativeSkyEquirectRelative.string());
        } else {
            ri::core::LogInfo(
                "Native Vulkan sky: no image found under Textures/Skies (add .png/.jpg/.hdr/etc.; subfolders ok).");
        }
    } else {
        ri::core::LogInfo("Texture library not found; preview will render without texture files.");
    }
    return true;
}

} // namespace
#endif

bool RunStandalone(const StandaloneOptions& options, std::string* error) {
    try {
#if defined(_WIN32)
        const std::optional<ri::content::GameManifest> manifest = ResolveStandaloneGameManifest(options);
        if (!manifest.has_value()) {
            if (error != nullptr) {
                *error = "Unable to resolve game manifest for '" + options.gameId + "'.";
            }
            return false;
        }
        const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(*manifest);
        if (!formatIssues.empty()) {
            if (error != nullptr) {
                *error = "Game format validation failed:";
                for (const std::string& issue : formatIssues) {
                    *error += " " + issue;
                }
            }
            return false;
        }
        const ri::content::GameRuntimeSupportData standaloneSupport =
            ri::content::LoadGameRuntimeSupportData(manifest->rootPath);
        const ri::content::AudioZoneRow* audioAtOrigin =
            ri::content::FindAudioZoneAtPoint(0.0f, 0.0f, 0.0f, standaloneSupport);
        const float lodScaleSample = standaloneSupport.lodRanges.empty()
            ? 1.0f
            : ri::content::ComputeLodScaleForDistance(
                  standaloneSupport.lodRanges.front().id,
                  24.0f,
                  standaloneSupport);
        ri::core::LogInfo(std::string("Runtime support data: triggers=")
            + std::to_string(standaloneSupport.levelTriggers.size()) + ", occlusion="
            + std::to_string(standaloneSupport.occlusionVolumes.size()) + ", materials="
            + std::to_string(standaloneSupport.materialsById.size()) + ", achievements="
            + std::to_string(standaloneSupport.achievementIdsByPlatform.size()) + ", audioZones="
            + std::to_string(standaloneSupport.audioZones.size()) + ", lodRanges="
            + std::to_string(standaloneSupport.lodRanges.size()) + ", audioAtOrigin="
            + std::string(audioAtOrigin != nullptr ? audioAtOrigin->id : "none") + ", lodScale@24m="
            + std::to_string(lodScaleSample));
        ri::core::LogSection("RawIron Standalone");
        ri::core::LogInfo("Game: " + manifest->name + " (" + manifest->id + ")");
        ri::core::LogInfo("Game root: " + manifest->rootPath.string());
        ri::core::LogInfo("Presenter: " + std::string(StandaloneRendererName(options.renderer)));

        RuntimeState state{};
        InitializeRuntimeState(options, *manifest, state);

        std::string runtimeError;
        const bool ok = RunStandaloneNativeVulkanLoop(options, state, &runtimeError);
        if (!ok) {
            if (error != nullptr) {
                *error = runtimeError;
            }
            return false;
        }
        return true;
#else
        if (error != nullptr) {
            *error = "Standalone currently requires Windows.";
        }
        return false;
#endif
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

bool RunHeadlessCapture(const HeadlessCaptureOptions& options, std::string* error) {
    try {
#if defined(_WIN32)
        const std::optional<ri::content::GameManifest> manifest = ResolveStandaloneGameManifest(options.standalone);
        if (!manifest.has_value()) {
            if (error != nullptr) {
                *error = "Unable to resolve game manifest for '" + options.standalone.gameId + "'.";
            }
            return false;
        }
        const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(*manifest);
        if (!formatIssues.empty()) {
            if (error != nullptr) {
                *error = "Game format validation failed:";
                for (const std::string& issue : formatIssues) {
                    *error += " " + issue;
                }
            }
            return false;
        }
        const ri::content::GameRuntimeSupportData headlessSupport =
            ri::content::LoadGameRuntimeSupportData(manifest->rootPath);
        const ri::content::AudioZoneRow* headlessAudioAtOrigin =
            ri::content::FindAudioZoneAtPoint(0.0f, 0.0f, 0.0f, headlessSupport);
        const float headlessLodScaleSample = headlessSupport.lodRanges.empty()
            ? 1.0f
            : ri::content::ComputeLodScaleForDistance(
                  headlessSupport.lodRanges.front().id,
                  24.0f,
                  headlessSupport);
        ri::core::LogInfo(std::string("Runtime support data: triggers=")
            + std::to_string(headlessSupport.levelTriggers.size()) + ", occlusion="
            + std::to_string(headlessSupport.occlusionVolumes.size()) + ", materials="
            + std::to_string(headlessSupport.materialsById.size()) + ", achievements="
            + std::to_string(headlessSupport.achievementIdsByPlatform.size()) + ", audioZones="
            + std::to_string(headlessSupport.audioZones.size()) + ", lodRanges="
            + std::to_string(headlessSupport.lodRanges.size()) + ", audioAtOrigin="
            + std::string(headlessAudioAtOrigin != nullptr ? headlessAudioAtOrigin->id : "none")
            + ", lodScale@24m=" + std::to_string(headlessLodScaleSample));

        RuntimeState state{};
        StandaloneOptions runOptions = options.standalone;
        runOptions.captureMouse = false;
        runOptions.renderer = StandaloneRenderer::VulkanNative;
        InitializeRuntimeState(runOptions, *manifest, state);
        ri::core::LogInfo("Mouse capture forced off for headless mode.");

        const int frames = std::max(1, options.frames);
        const float dt = std::clamp(options.deltaSeconds, 1.0f / 240.0f, 1.0f / 15.0f);

        ri::core::LogSection("Liminal Headless");
        ri::core::LogInfo("Frames: " + std::to_string(frames) + " dt=" + std::to_string(dt));
        ri::core::LogInfo(std::string("Autoplay: ") + (options.autoplay ? "enabled" : "disabled"));

        for (int frameIndex = 0; frameIndex < frames; ++frameIndex) {
            ri::trace::MovementInput input = BuildIdleHeadlessInput(state);
            if (options.autoplay) {
                const HeadlessAutoplayPlan plan = BuildHeadlessAutoplayPlan(state);
                const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
                const ri::math::Vec3 eye{feet.x, feet.y + state.cameraBaseHeight, feet.z};
                const ri::math::Vec3 lookVector = plan.lookTarget - eye;
                state.yawDegrees = ApproachDegrees(state.yawDegrees, YawFromDirection(lookVector), dt * 84.0f);
                state.pitchDegrees = std::clamp(
                    state.pitchDegrees +
                        std::clamp(PitchFromDirection(lookVector) - state.pitchDegrees, -(dt * 48.0f), dt * 48.0f),
                    -40.0f,
                    30.0f);
                input = BuildHeadlessAutoplayInput(state, plan);
            }
            const double animationSeconds = static_cast<double>(frameIndex) * static_cast<double>(dt);
            ApplyEnvironmentAuthoringVolumes(state, dt);
            ProcessPendingDoorTransitions(state);
            SimulateAndApplyView(state, input, dt, animationSeconds);
            TickLogicDemo(state, dt);
            ri::render::software::RenderScenePreviewInto(
                state.world.scene,
                state.world.playerCameraNode,
                state.previewOptions,
                state.scenePreviewScratch,
                &state.previewCache);
        }

        ri::render::software::SoftwareImage lastImage = std::move(state.scenePreviewScratch);

        const ri::math::Vec3 feet = FeetFromBounds(state.movement.body.bounds);
        ri::core::LogInfo(
            "Final headless feet=" + std::to_string(feet.x) + "," + std::to_string(feet.y) + "," + std::to_string(feet.z) +
            " velocity=" + std::to_string(state.movement.body.velocity.x) + "," +
            std::to_string(state.movement.body.velocity.y) + "," +
            std::to_string(state.movement.body.velocity.z) +
            " onGround=" + std::string(state.movement.onGround ? "true" : "false"));

        if (options.outputPath.empty()) {
            ri::core::LogInfo("Headless run complete (no --output specified, image not saved).");
            return true;
        }

        fs::create_directories(options.outputPath.parent_path());
        if (!ri::render::software::SaveBmp(lastImage, options.outputPath.string())) {
            if (error != nullptr) {
                *error = "Failed to save headless capture BMP to " + options.outputPath.string();
            }
            return false;
        }
        ri::core::LogInfo("Headless capture saved: " + options.outputPath.string());
        ri::core::LogInfo("Image size: " + std::to_string(lastImage.width) + "x" + std::to_string(lastImage.height));
        return true;
#else
        (void)options;
        if (error != nullptr) {
            *error = "Liminal headless capture currently requires Windows build path parity with standalone setup.";
        }
        return false;
#endif
    } catch (const std::exception& exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

} // namespace ri::games::liminal
