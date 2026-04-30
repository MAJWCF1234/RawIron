#include "RawIron/Audio/AudioManager.h"
#include "RawIron/Content/PrefabExpansion.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Debug/RuntimeSnapshots.h"
#include "RawIron/Events/EventEngine.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec2.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Render/VulkanCommandBufferRecorder.h"
#include "RawIron/Render/VulkanCommandList.h"
#include "RawIron/Render/VulkanCommandRecorder.h"
#include "RawIron/Render/VulkanFrameSubmission.h"
#include "RawIron/Render/VulkanIntentStaging.h"
#include "RawIron/Render/VulkanPipelineStateCache.h"
#include "RawIron/Runtime/LevelScopedSchedulers.h"
#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/Runtime/RuntimeId.h"
#include "RawIron/Runtime/RuntimeTuning.h"
#include "RawIron/Scene/AuthoringTransform.h"
#include "RawIron/Scene/LoftPrimitiveStack.h"
#include "RawIron/Render/PostProcessProfiles.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Spatial/SpatialIndex.h"
#include "RawIron/Trace/CameraFeetReconciliation.h"
#include "RawIron/Trace/LocomotionTuningSmoother.h"
#include "RawIron/Trace/ImpactDecalPlacement.h"
#include "RawIron/Trace/KinematicPhysics.h"
#include "RawIron/Trace/LocomotionRuntimeBridge.h"
#include "RawIron/Trace/MovementController.h"
#include "RawIron/Trace/ObjectPhysics.h"
#include "RawIron/Trace/SpatialQueryHelpers.h"
#include "RawIron/Trace/SweptVolumeContactSolver.h"
#include "RawIron/Trace/TraceScene.h"
#include "RawIron/Structural/ConvexClipper.h"
#include "RawIron/Structural/StructuralCompiler.h"
#include "RawIron/Structural/StructuralDeferredOperations.h"
#include "RawIron/Structural/StructuralGraph.h"
#include "RawIron/Structural/StructuralPrimitives.h"
#include "RawIron/Validation/Schemas.h"
#include "RawIron/World/Instrumentation.h"
#include "RawIron/World/InventoryState.h"
#include "RawIron/World/InteractionPromptState.h"
#include "RawIron/World/PresentationState.h"
#include "RawIron/World/RuntimeClipQuery.h"
#include "RawIron/World/RuntimeState.h"
#include "RawIron/World/SpatialDebugDraw.h"
#include "RawIron/World/VolumeDescriptors.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <tuple>
#include <string>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#include "RawIron/Test/TestHarness.h"

void TestContentEnvironmentVolumes();
void TestContentValue();
void TestValueSchema();
void TestGameManifest();
void TestPipelineArtifacts();
void TestScriptedCameraReview();
void TestContentPhysicsVolumes();
void TestContentPrefabExpansion();
void TestAudioEnvironment();
void TestAccessFeedbackState();
void TestDialogueCueState();
void TestDeveloperConsoleState();
void TestCheckpointPersistence();
void TestInventoryState();
void TestInteractionPromptState();
void TestLevelFlowPresentationState();
void TestHeadlessModuleVerifier();
void TestNpcAgentState();
void TestHostileCharacterAi();
void TestPickupFeedbackState();
void TestPlayerVitality();
void TestPresentationState();
void TestTextOverlayState();
void TestTextOverlayEventBridge();
void TestTextOverlayEvents();
void TestSignalBroadcastState();
void TestContentTriggerVolumes();
void TestContentWorldVolumes();
void TestRuntimeTriggerVolumes();
void TestRuntimeLocalGridSnap();
void TestStructuralDeferredOperations();
void TestWorldVolumeDescriptors();
void TestScalarClamp();
void TestFiniteComponents();
void TestSummarizeHelperActivity();
void TestRuntimeStatsOverlay();
void TestStructuralPhaseClassification();
void TestSnapshotFormatting();
void TestInputLabelFormat();
void TestVolumeContainment();
void TestDataSchema();
void TestLogicGraph();
void TestLogicEntityIoTelemetry();
void TestLogicWorldActors();
void TestWorldSpatialDebugAndProxyVolumes();
void TestRawIronGameplayInfrastructureStacks();

namespace {

constexpr float kIdentityMatrix4x4[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    Expect(NearlyEqual(actual.x, expected.x) && NearlyEqual(actual.y, expected.y) && NearlyEqual(actual.z, expected.z),
           label + " expected " + ri::math::ToString(expected) + " but got " + ri::math::ToString(actual));
}

ri::structural::StructuralNode MakeNode(std::string id,
                                        std::string name,
                                        std::string type,
                                        std::vector<std::string> targetIds = {}) {
    ri::structural::StructuralNode node{};
    node.id = std::move(id);
    node.name = std::move(name);
    node.type = std::move(type);
    node.targetIds = std::move(targetIds);
    return node;
}

ri::events::EventAction MakeAction(std::string type) {
    ri::events::EventAction action{};
    action.type = std::move(type);
    return action;
}

ri::events::EventAction MakeMessageAction(std::string text) {
    ri::events::EventAction action = MakeAction("message");
    action.text = std::move(text);
    return action;
}

ri::events::EventSequenceStep MakeSequenceStep(double delayMs, std::vector<ri::events::EventAction> actions) {
    ri::events::EventSequenceStep step{};
    step.delayMs = delayMs;
    step.actions = std::move(actions);
    return step;
}

ri::events::EventContext MakeContext(std::string sourceId) {
    ri::events::EventContext context{};
    context.sourceId = std::move(sourceId);
    return context;
}

ri::events::EventConditions MakeRequiredFlagCondition(std::vector<std::string> requiredFlags) {
    ri::events::EventConditions conditions{};
    conditions.requiredFlags = std::move(requiredFlags);
    return conditions;
}

class FakeAudioPlaybackHandle final : public ri::audio::AudioPlaybackHandle {
public:
    explicit FakeAudioPlaybackHandle(ri::audio::AudioClipRequest request)
        : request_(std::move(request)),
          volume_(request_.volume),
          playbackRate_(request_.playbackRate) {}

    void Play() override {
        playing_ = true;
        playCount_ += 1;
    }

    void Pause() override {
        playing_ = false;
        pauseCount_ += 1;
    }

    void Stop() override {
        playing_ = false;
        currentTime_ = 0.0;
        stopCount_ += 1;
    }

    void Unload() override {
        playing_ = false;
        unloaded_ = true;
        unloadCount_ += 1;
    }

    [[nodiscard]] double GetCurrentTime() const override {
        return currentTime_;
    }

    void SetCurrentTime(double value) override {
        currentTime_ = std::max(0.0, std::isfinite(value) ? value : 0.0);
    }

    [[nodiscard]] double GetVolume() const override {
        return volume_;
    }

    void SetVolume(double value) override {
        volume_ = value;
    }

    [[nodiscard]] double GetDuration() const override {
        return duration_;
    }

    [[nodiscard]] bool IsPlaying() const override {
        return playing_;
    }

    [[nodiscard]] double GetPlaybackRate() const override {
        return playbackRate_;
    }

    void SetPlaybackRate(double value) override {
        playbackRate_ = value;
    }

    void SetFinishedCallback(FinishedCallback callback) override {
        finishedCallback_ = std::move(callback);
    }

    void TriggerFinished() {
        playing_ = false;
        if (finishedCallback_) {
            finishedCallback_();
        }
    }

    [[nodiscard]] int StopCount() const {
        return stopCount_;
    }

    [[nodiscard]] int UnloadCount() const {
        return unloadCount_;
    }
    [[nodiscard]] const ri::audio::AudioClipRequest& Request() const {
        return request_;
    }

private:
    ri::audio::AudioClipRequest request_;
    FinishedCallback finishedCallback_;
    double currentTime_ = 0.0;
    double duration_ = 3.0;
    double volume_ = 1.0;
    double playbackRate_ = 1.0;
    int playCount_ = 0;
    int pauseCount_ = 0;
    int stopCount_ = 0;
    int unloadCount_ = 0;
    bool playing_ = false;
    bool unloaded_ = false;
};

class FakeAudioBackend final : public ri::audio::AudioBackend {
public:
    [[nodiscard]] std::shared_ptr<ri::audio::AudioPlaybackHandle> CreatePlayback(
        const ri::audio::AudioClipRequest& request) override {
        auto playback = std::make_shared<FakeAudioPlaybackHandle>(request);
        created_.push_back(playback);
        return playback;
    }

    void PumpHostThreadAudioWork() override {}

    [[nodiscard]] const std::vector<std::shared_ptr<FakeAudioPlaybackHandle>>& Created() const {
        return created_;
    }

private:
    std::vector<std::shared_ptr<FakeAudioPlaybackHandle>> created_;
};

void TestRuntimeIds() {
    Expect(ri::runtime::SanitizeRuntimeIdPrefix("  Raw Iron!!!  ") == "Raw-Iron",
           "Runtime ID prefix should trim whitespace and collapse invalid characters");

    const std::string runtimeId = ri::runtime::CreateRuntimeId("  Raw Iron!!!  ");
    Expect(runtimeId.rfind("Raw-Iron_", 0) == 0, "Runtime ID should preserve sanitized prefix");
    Expect(runtimeId.size() == std::string("Raw-Iron_").size() + 10U, "Runtime ID suffix should be ten characters long");

    constexpr std::string_view kAlphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string suffix = runtimeId.substr(std::string("Raw-Iron_").size());
    for (char character : suffix) {
        Expect(kAlphabet.find(character) != std::string_view::npos,
               "Runtime ID suffix should use the shared 62-char alphabet (proto runtimeIdShim parity)");
    }
}

void TestRuntimeIdsConcurrent() {
    constexpr int kThreadCount = 8;
    constexpr int kIdsPerThread = 2000;
    constexpr std::size_t kExpectedCount = static_cast<std::size_t>(kThreadCount * kIdsPerThread);

    std::mutex idsMutex;
    std::set<std::string> allIds;
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        workers.emplace_back([&]() {
            std::vector<std::string> localIds;
            localIds.reserve(kIdsPerThread);
            for (int idIndex = 0; idIndex < kIdsPerThread; ++idIndex) {
                localIds.push_back(ri::runtime::CreateRuntimeId("io"));
            }

            std::lock_guard<std::mutex> lock(idsMutex);
            for (const std::string& id : localIds) {
                Expect(id.rfind("io_", 0) == 0, "Concurrent runtime IDs should preserve the requested prefix");
                allIds.insert(id);
            }
        });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    Expect(allIds.size() == kExpectedCount,
           "Concurrent runtime ID generation should produce unique IDs across worker threads");
}

void TestRuntimeTuning() {
    Expect(!ri::runtime::SanitizeRuntimeTuningValue("unknown_key", 1.0).has_value(),
           "Runtime tuning sanitizer should ignore unknown keys");

    const std::optional<double> clamped = ri::runtime::SanitizeRuntimeTuningValue("walkSpeed", 99.0);
    Expect(clamped.has_value() && std::fabs(*clamped - 12.0) < 1e-6,
           "Walk speed should clamp to the schema maximum");

    const std::optional<double> clampedLow = ri::runtime::SanitizeRuntimeTuningValue("jumpForce", 0.01);
    Expect(clampedLow.has_value() && std::fabs(*clampedLow - 0.5) < 1e-6,
           "Jump force should clamp to the schema minimum (proto runtimeTuningShim parity)");

    const std::optional<double> defaulted = ri::runtime::SanitizeRuntimeTuningValue("walkSpeed", std::nullopt);
    Expect(defaulted.has_value() && std::fabs(*defaulted - 5.0) < 1e-6,
           "Missing tuning input should fall back to the authored default");

    const ri::runtime::RuntimeTuningLimits* limits = ri::runtime::FindRuntimeTuningLimits("sensitivity");
    Expect(limits != nullptr && std::fabs(limits->min - 0.2) < 1e-9 && std::fabs(limits->max - 3.0) < 1e-9,
           "Runtime tuning limits should expose sensitivity bounds for tooling");

    std::unordered_map<std::string, double> record{
        {"walkSpeed", 99.0},
        {"customKey", 3.0},
    };
    ri::runtime::SanitizeRuntimeTuningRecord(record);
    Expect(std::fabs(record.at("walkSpeed") - 12.0) < 1e-6 && std::fabs(record.at("customKey") - 3.0) < 1e-6,
           "Runtime tuning record sanitizer should clamp known keys and ignore unknown keys");

    const std::unordered_map<std::string, double> snapshot = ri::runtime::BuildRuntimeTuningSnapshot({
        {"walkSpeed", 99.0},
        {"gravity", 10.0},
        {"unknown", 1.0},
    });
    Expect(snapshot.contains("walkSpeed") && std::fabs(snapshot.at("walkSpeed") - 12.0) < 1e-6
               && snapshot.contains("gravity") && std::fabs(snapshot.at("gravity") - 10.0) < 1e-6
               && !snapshot.contains("unknown"),
           "Runtime tuning snapshot should merge overrides with defaults and ignore unknown keys");
    const std::string tuningReport = ri::runtime::FormatRuntimeTuningReport(snapshot);
    Expect(tuningReport.find("runtime_tuning") != std::string::npos
               && tuningReport.find("walkSpeed=") != std::string::npos
               && tuningReport.find("gravity=") != std::string::npos,
           "Runtime tuning report should serialize a readable snapshot");

    const std::optional<double> crouch = ri::runtime::SanitizeRuntimeTuningValue("crouchSpeed", 99.0);
    Expect(crouch.has_value() && std::fabs(*crouch - 10.0) < 1e-6,
           "Crouch speed should clamp to the schema maximum");

    const ri::trace::LocomotionTuning locomotion =
        ri::trace::LocomotionTuningFromRuntimeRecord({{"walkSpeed", 6.0}, {"crouchSpeed", 3.0}});
    Expect(std::fabs(locomotion.walkSpeed - 6.0f) < 1e-5f && std::fabs(locomotion.crouchSpeed - 3.0f) < 1e-5f,
           "Locomotion runtime bridge should merge sanitized tuning into locomotion scalars");

    const auto locomotionRecord = ri::trace::LocomotionTuningToRuntimeRecordSubset(locomotion);
    Expect(locomotionRecord.at("walkSpeed") == 6.0 && locomotionRecord.at("crouchSpeed") == 3.0,
           "Locomotion subset snapshot should round-trip numeric entries");

    ri::trace::TraceScene scene{};
    scene.SetColliders({
        {.id = "a", .bounds = ri::spatial::Aabb{{0, 0, 0}, {1, 1, 1}}},
        {.id = "b", .bounds = ri::spatial::Aabb{{2, 2, 2}, {3, 3, 3}}},
    });
    Expect(scene.EraseCollidersWithIds({"a"}) == 1U && scene.ColliderCount() == 1U,
           "TraceScene should erase colliders by id and rebuild broadphase entries");

    const auto splatter =
        ri::trace::BuildSplatterDecalPlacement({0, 0, 0}, {0, 1, 0}, 0.25f);
    Expect(splatter.has_value() && splatter->kind == ri::trace::ImpactDecalKind::Splatter
               && std::fabs(splatter->halfExtentU - 0.25f) < 1e-5f,
           "Splatter decal placement should face the surface normal with radius extents");

    const auto streak = ri::trace::BuildDragStreakDecalPlacement(
        {0, 0, 0}, {0, 1, 0}, {10.0f, 0.0f, 0.0f}, 0.05f, 0.5f, 5.0f, 0.1f, 2.0f);
    Expect(streak.has_value() && streak->kind == ri::trace::ImpactDecalKind::DragStreak,
           "Drag streak decal should align to projected velocity on the surface");

    const ri::math::Vec3 feet = ri::trace::CameraFeetPositionFromEye({1.0f, 4.0f, -2.0f},
                                                                     ri::trace::MovementStance::Standing);
    Expect(std::fabs(feet.y - (4.0f - 1.65f)) < 1e-4f,
           "Camera feet reconciliation should drop eye height along world up");

    const ri::math::Vec3 feetYaw = ri::trace::CameraFeetPositionFromEyeLocalOffset(
        {2.0f, 3.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.65f, 0.4f});
    Expect(std::fabs(feetYaw.y - (3.0f - 1.65f)) < 1e-4f && std::fabs(feetYaw.z - 0.6f) < 1e-4f,
           "Yaw-frame camera feet solve should separate vertical eye offset from forward camera lead");

    ri::trace::LocomotionTuningSmoother smoother{};
    smoother.value = ri::trace::DefaultLocomotionTuning();
    smoother.value.walkSpeed = 5.0f;
    ri::trace::LocomotionTuning target = smoother.value;
    target.walkSpeed = 10.0f;
    const ri::trace::LocomotionTuning blended =
        ri::trace::AdvanceLocomotionTuningSmoother(smoother, target, 0.1f);
    Expect(blended.walkSpeed > 5.2f && blended.walkSpeed < 9.9f,
           "Locomotion tuning smoother should ease toward targets without instantaneous pops");

    ri::scene::Transform import{};
    import.scale = {2.0f, 1.0f, 1.0f};
    const ri::scene::Transform scaled = ri::scene::ApplyAuthoringScaleMultiply(import, {0.5f, 2.0f, 1.0f});
    Expect(std::fabs(scaled.scale.x - 1.0f) < 1e-5f && std::fabs(scaled.scale.y - 2.0f) < 1e-5f,
           "Authoring scale multiply should apply component-wise after import scale");
}

void TestRuntimeEventBus() {
    ri::runtime::RuntimeEventBus bus = ri::runtime::CreateRuntimeEventBus();
    int handled = 0;
    std::string lastType;
    std::string lastValue;
    std::string lastId;
    std::string lastSequence;

    const auto first = bus.On("ping", [&](const ri::runtime::RuntimeEvent& event) {
        handled += 1;
        lastType = event.type;
        lastId = event.id;
        lastValue = event.fields.contains("mode") ? event.fields.at("mode") : std::string{};
        lastSequence = event.fields.contains("sequence") ? event.fields.at("sequence") : std::string{};
    });
    const auto second = bus.On("ping", [&](const ri::runtime::RuntimeEvent&) {
        handled += 1;
    });

    ri::runtime::RuntimeEvent event{};
    event.fields = {{"mode", "test"}};
    bus.Emit("ping", event);
    bus.EmitScoped("ping", "gameplay.door_a", "gameplay.door_b", ri::runtime::RuntimeEvent{
        .fields = {{"mode", "scoped"}},
    });

    Expect(handled == 4, "Runtime event bus should deliver events to all listeners for a type");
    Expect(lastType == "ping", "Runtime event bus should backfill the event type when omitted");
    Expect(lastValue == "scoped", "Runtime event bus should preserve event fields");
    Expect(!lastId.empty(), "Runtime event bus should assign deterministic event IDs when omitted by the caller");
    Expect(!lastSequence.empty(), "Runtime event bus should stamp event sequence metadata for telemetry consumers");

    ri::runtime::RuntimeEventBusMetrics metrics = bus.GetMetrics();
    Expect(metrics.emitted == 2, "Runtime event bus should count emitted events");
    Expect(metrics.listenersAdded == 2, "Runtime event bus should count listeners added");
    Expect(metrics.listenersRemoved == 0, "Runtime event bus should start with zero removals");
    Expect(metrics.activeListeners == 2, "Runtime event bus should count active listeners");
    Expect(metrics.emittedByType.contains("ping") && metrics.emittedByType.at("ping") == 2U,
           "Runtime event bus metrics should track emitted counts per event type");
    const std::vector<ri::runtime::RuntimeSignalRouteTrace> routes = bus.GetRecentSignalRoutes(4);
    Expect(!routes.empty() && routes.back().sourceScope == "gameplay.door_a" && routes.back().targetScope == "gameplay.door_b",
           "Runtime event bus should preserve recent scoped signal routes for deterministic event-chain debugging");

    Expect(bus.Off("ping", first), "Runtime event bus should remove existing listeners");
    Expect(!bus.Off("missing", 9999), "Runtime event bus should return false for missing listener buckets");
    metrics = bus.GetMetrics();
    Expect(metrics.listenersRemoved == 2, "Runtime event bus should mirror prototype removal metrics semantics");
    Expect(metrics.activeListeners == 1, "Runtime event bus should update active listener counts after removals");

    bus.Clear();
    Expect(bus.GetMetrics().activeListeners == 0, "Runtime event bus clear should drop all listeners");
    Expect(!bus.Off("ping", second), "Removed buckets should no longer report listeners");
}

void TestEngineAssetTextureAliases() {
    namespace fs = std::filesystem;
    const auto& aliases = ri::content::GetTextureAliasManifest();
    Expect(!aliases.empty() && aliases.contains("void_floor"),
           "Engine texture aliases should expose a compact compatibility manifest");
    Expect(aliases.contains("plaster") && aliases.contains("noise_perlin"),
           "Texture alias manifest should cover broad authoring vocab beyond prototype stubs");
    Expect(ri::content::ResolveTextureAlias("VOID_FLOOR") == "materials/liminal/void_floor",
           "Texture alias resolution should be case-insensitive for known aliases");
    Expect(ri::content::ResolveTextureAlias("materials/custom/path") == "materials/custom/path",
           "Texture alias resolution should pass through unknown authored names");

    ri::content::TextureAliasManifest overlay;
    overlay["plaster"] = "materials/custom/plaster_override";
    const ri::content::TextureAliasManifest merged =
        ri::content::MergeTextureAliasManifestsOverlayWins(ri::content::GetTextureAliasManifest(), overlay);
    Expect(merged.at("plaster") == "materials/custom/plaster_override",
           "Merged manifests should let overlay entries override built-in rows");
    Expect(ri::content::ResolveTextureAliasWithManifest("Plaster", merged) == "materials/custom/plaster_override",
           "ResolveTextureAliasWithManifest should use the supplied table");

    const ri::content::TextureAliasManifest hydrated = ri::content::BuildHydratedTextureAliasManifest(fs::path{});
    Expect(hydrated.size() >= ri::content::GetTextureAliasManifest().size(),
           "Hydrated resolver should start from the built-in manifest when no disk library is present");

    const fs::path tempRoot = fs::temp_directory_path() / "ri_texture_alias_test";
    std::error_code ec{};
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot / "materials" / "custom", ec);
    {
        std::ofstream fileA(tempRoot / "materials" / "custom" / "Panel_A.PNG");
        fileA << "png";
        std::ofstream fileB(tempRoot / "materials" / "custom" / "panel_b.png");
        fileB << "png";
    }
    const ri::content::TextureAliasManifest discovered = ri::content::DiscoverTextureAliasesUnderTexturesRoot(tempRoot, 8U);
    Expect(discovered.contains("materials/custom/panel_a"),
           "Discovered aliases should include lowercase relative path keys");
    Expect(discovered.contains("panel_a"),
           "Discovered aliases should include lowercase stem aliases when unique");
    const ri::content::TextureAliasManifest none = ri::content::DiscoverTextureAliasesUnderTexturesRoot(tempRoot, 0U);
    Expect(none.empty(), "Texture alias discovery should honor zero-entry caps");
    fs::remove_all(tempRoot, ec);

    ri::content::AssetVariantMap variantManifest{};
    variantManifest["materials/dev/dev_floor"] = {
        {"default", "materials/dev/dev_floor"},
        {"platform:mobile", "materials/dev/dev_floor_mobile"},
        {"quality:low", "materials/dev/dev_floor_low"},
    };
    std::string variantError;
    Expect(ri::content::ValidateAssetVariantManifest(variantManifest, &variantError) && variantError.empty(),
           "Asset variant manifest validation should accept well-formed entries");
    const std::string mobileResolved = ri::content::ResolveAssetVariantId(
        ri::content::AssetVariantResolveRequest{
            .logicalAssetId = "materials/dev/dev_floor",
            .platform = "mobile",
        },
        variantManifest,
        aliases);
    Expect(mobileResolved == "materials/dev/dev_floor_mobile",
           "Asset variant resolver should prefer explicit platform variants over defaults");
    const std::string fallbackResolved = ri::content::ResolveAssetVariantId(
        ri::content::AssetVariantResolveRequest{
            .logicalAssetId = "VOID_FLOOR",
            .variant = "missing",
        },
        variantManifest,
        aliases);
    Expect(fallbackResolved == "materials/liminal/void_floor",
           "Asset variant resolver should fall back to alias hydration for unmapped logical IDs");
}

void TestModelLoaderFallbackDiagnostics() {
    ri::scene::Scene scene{};
    std::string error;
    const int handle = ri::scene::AddModelNode(
        scene,
        ri::scene::ImportedModelOptions{
            .sourcePath = "missing_asset.mesh",
            .nodeName = "FallbackProbe",
            .backend = ri::scene::ModelImportBackend::Gltf,
            .fallbackBackends = {
                ri::scene::ModelImportBackend::Fbx,
                ri::scene::ModelImportBackend::WavefrontObj,
            },
        },
        &error);
    Expect(handle == ri::scene::kInvalidHandle,
           "Model loader should fail cleanly for missing multi-format assets");
    Expect(error.find("backend attempt") != std::string::npos
               && error.find("[gltf:") != std::string::npos
               && error.find("[fbx:") != std::string::npos
               && error.find("[obj:") != std::string::npos,
           "Model loader should include per-backend fallback diagnostics when all ingest attempts fail");

    std::string placeholderError;
    const int placeholderHandle = ri::scene::AddModelNode(
        scene,
        ri::scene::ImportedModelOptions{
            .sourcePath = "missing_asset.mesh",
            .nodeName = "FallbackPlaceholder",
            .backend = ri::scene::ModelImportBackend::Gltf,
            .fallbackBackends = {
                ri::scene::ModelImportBackend::Fbx,
                ri::scene::ModelImportBackend::WavefrontObj,
            },
            .createPlaceholderOnFailure = true,
        },
        &placeholderError);
    Expect(placeholderHandle != ri::scene::kInvalidHandle,
           "Model loader should optionally create a deterministic placeholder when import backends fail");
    Expect(placeholderError.find("Placeholder model created.") != std::string::npos,
           "Model loader placeholder path should preserve failure diagnostics");

    std::string candidateError;
    const std::vector<ri::scene::ModelImportBackend> fallbackCandidates =
        ri::scene::BuildExternalModelCandidateTypes(ri::scene::ImportedModelOptions{
            .sourcePath = "missing_asset.mesh",
            .backend = ri::scene::ModelImportBackend::Gltf,
            .fallbackBackends = {ri::scene::ModelImportBackend::Fbx, ri::scene::ModelImportBackend::WavefrontObj},
        }, &candidateError);
    Expect(candidateError.empty() && fallbackCandidates.size() == 3U,
           "Model candidate builder should return primary plus unique fallback backend order");
    const std::vector<ri::scene::ModelImportBackend> lockedCandidates =
        ri::scene::BuildExternalModelCandidateTypes(ri::scene::ImportedModelOptions{
            .sourcePath = "missing_asset.mesh",
            .backend = ri::scene::ModelImportBackend::Gltf,
            .fallbackBackends = {ri::scene::ModelImportBackend::Fbx},
            .lockToPrimaryBackend = true,
        }, &candidateError);
    Expect(candidateError.empty() && lockedCandidates.size() == 1U
               && lockedCandidates[0] == ri::scene::ModelImportBackend::Gltf,
           "Model candidate builder should support lock-to-primary mode without fallback probing");
}

void TestStructuralGraph() {
    using ri::structural::StructuralNode;
    using ri::structural::StructuralPhase;

    const std::vector<StructuralNode> nodes = {
        MakeNode("base", "Base", "box"),
        MakeNode("runtime", "Portal", "portal", {"base"}),
        MakeNode("frame", "Spinner", "kinematic_rotation_primitive", {"runtime"}),
        MakeNode("late", "LateCompile", "box", {"frame", "missing"}),
        MakeNode("cycle-a", "CycleA", "box", {"cycle-b"}),
        MakeNode("cycle-b", "CycleB", "box", {"cycle-a"}),
    };

    const ri::structural::StructuralDependencyGraph graph = ri::structural::BuildStructuralDependencyGraph(nodes);
    const ri::structural::StructuralDependencyGraph aliasGraph = ri::structural::buildStructuralDependencyGraph(nodes);
    Expect(aliasGraph.orderedNodes.size() == graph.orderedNodes.size(),
           "buildStructuralDependencyGraph should mirror BuildStructuralDependencyGraph outputs");
    Expect(graph.orderedNodes.size() == nodes.size(), "Structural graph should keep every node in the ordered list");
    Expect(graph.orderedNodes[0].id == "base", "Structural graph should start with dependency roots");
    Expect(graph.orderedNodes[1].id == "runtime", "Structural graph should order runtime nodes after their compile dependencies");
    Expect(graph.orderedNodes[2].id == "frame", "Structural graph should order frame nodes after runtime dependencies");
    Expect(graph.orderedNodes[3].id == "late", "Structural graph should keep dependent compile nodes after promoted frame dependencies");
    Expect(graph.orderedNodes[3].phase == StructuralPhase::Frame,
           "Structural graph should promote node phase when it depends on a later phase");

    Expect(graph.summary.edgeCount == 5, "Structural graph should count resolved dependency edges");
    Expect(graph.summary.cycleCount == 1, "Structural graph should report the presence of cycles");
    Expect(graph.summary.unresolvedDependencyCount == 1, "Structural graph should report unresolved dependencies");
    Expect(graph.summary.phaseBuckets.compile == 3, "Structural graph should count compile-phase nodes");
    Expect(graph.summary.phaseBuckets.runtime == 1, "Structural graph should count runtime-phase nodes");
    Expect(graph.summary.phaseBuckets.frame == 2, "Structural graph should count frame-phase nodes");

    StructuralNode depsNode{};
    depsNode.targetIds = {"a", "b"};
    depsNode.childNodeList = {"b", "c"};
    depsNode.pivotAnchorId = "d";
    depsNode.anchorId = "d";
    const std::vector<std::string> deps = ri::structural::GetExplicitDependencies(depsNode);
    Expect(deps.size() == 4, "Explicit dependency extraction should de-duplicate repeated IDs");
    const std::string graphSummary = ri::structural::FormatStructuralDependencyGraphSummary(graph, 4U);
    Expect(graphSummary.find("structural_graph_summary") != std::string::npos
               && graphSummary.find("ordered_nodes") != std::string::npos
               && graphSummary.find("missing") != std::string::npos,
           "Structural graph summary should include ordered output and unresolved dependency details");

    const std::vector<StructuralNode> multiCycleNodes = {
        MakeNode("cycle-1-a", "Cycle1A", "box", {"cycle-1-b"}),
        MakeNode("cycle-1-b", "Cycle1B", "box", {"cycle-1-a"}),
        MakeNode("cycle-2-a", "Cycle2A", "box", {"cycle-2-b"}),
        MakeNode("cycle-2-b", "Cycle2B", "box", {"cycle-2-a"}),
    };
    const ri::structural::StructuralDependencyGraph multiCycleGraph =
        ri::structural::BuildStructuralDependencyGraph(multiCycleNodes);
    Expect(multiCycleGraph.summary.cycleCount == 2,
           "Structural graph should count distinct strongly connected cycle components");

    using ri::structural::CreateAxisAlignedBoxSolid;
    using ri::structural::DedupeConvexPlanes;
    using ri::structural::EmitCompiledConvexFragments;
    using ri::structural::Plane;

    StructuralNode emitNode{};
    emitNode.id = "frag";
    emitNode.type = "box";
    const std::vector<ri::structural::CompiledGeometryNode> fragments =
        EmitCompiledConvexFragments(emitNode, {CreateAxisAlignedBoxSolid()}, "csg");
    Expect(!fragments.empty() && !fragments[0].compiledMesh.positions.empty(),
           "EmitCompiledConvexFragments should produce compiled mesh data for convex CSG output");

    const std::vector<Plane> planes = {
        Plane{.normal = {1.0f, 0.0f, 0.0f}, .constant = 1.0f},
        Plane{.normal = {1.0f, 0.0f, 0.0f}, .constant = 1.0f},
        Plane{.normal = {-1.0f, 0.0f, 0.0f}, .constant = -1.0f},
    };
    const std::vector<Plane> deduped = DedupeConvexPlanes(planes, 1e-3f);
    Expect(deduped.size() == 1U, "DedupeConvexPlanes should collapse redundant clipping planes");
}

void TestStructuralPrimitives() {
    using ri::structural::BuildPrimitiveMesh;
    using ri::structural::CreateConvexPrimitiveSolid;
    using ri::structural::IsNativeStructuralPrimitive;
    using ri::structural::StructuralPrimitiveOptions;
    const auto MakePrimitiveOptions = [](auto configure) {
        StructuralPrimitiveOptions options{};
        configure(options);
        return options;
    };

    Expect(IsNativeStructuralPrimitive("box")
               && IsNativeStructuralPrimitive("plane")
               && IsNativeStructuralPrimitive("arch")
               && IsNativeStructuralPrimitive("hollow_box")
               && IsNativeStructuralPrimitive("ramp")
               && IsNativeStructuralPrimitive("wedge")
               && IsNativeStructuralPrimitive("cylinder")
               && IsNativeStructuralPrimitive("cone")
               && IsNativeStructuralPrimitive("pyramid")
               && IsNativeStructuralPrimitive("stairs")
               && IsNativeStructuralPrimitive("spiral_stairs")
               && IsNativeStructuralPrimitive("tube")
               && IsNativeStructuralPrimitive("torus")
               && IsNativeStructuralPrimitive("corner")
               && IsNativeStructuralPrimitive("capsule")
               && IsNativeStructuralPrimitive("frustum")
               && IsNativeStructuralPrimitive("geodesic_sphere")
               && IsNativeStructuralPrimitive("superellipsoid")
               && IsNativeStructuralPrimitive("extrude_along_normal_primitive")
               && IsNativeStructuralPrimitive("lattice_volume")
               && IsNativeStructuralPrimitive("hexahedron")
               && IsNativeStructuralPrimitive("convex_hull")
               && IsNativeStructuralPrimitive("roof_gable")
               && IsNativeStructuralPrimitive("hipped_roof")
               && !IsNativeStructuralPrimitive("portal"),
           "Structural primitive registry should recognize the expanded native primitive subset");

    const auto boxSolid = CreateConvexPrimitiveSolid("box");
    Expect(boxSolid.has_value() && boxSolid->polygons.size() == 6U,
           "Structural primitives should create a convex box solid");
    const ri::structural::CompiledMesh boxMesh = BuildPrimitiveMesh("box");
    Expect(boxMesh.triangleCount == 12U
               && boxMesh.hasBounds
               && NearlyEqual(boxMesh.boundsMin.x, -0.5f)
               && NearlyEqual(boxMesh.boundsMax.z, 0.5f),
           "Structural primitives should compile a native box mesh");

    const auto rampSolid = CreateConvexPrimitiveSolid("ramp");
    Expect(rampSolid.has_value() && rampSolid->polygons.size() == 5U,
           "Structural primitives should create a convex ramp solid");
    const ri::structural::CompiledMesh rampMesh = BuildPrimitiveMesh("ramp");
    Expect(rampMesh.triangleCount == 8U && rampMesh.hasBounds,
           "Structural primitives should compile a native ramp mesh");

    const auto wedgeSolid = CreateConvexPrimitiveSolid("wedge");
    Expect(wedgeSolid.has_value() && wedgeSolid->polygons.size() == 5U,
           "Structural primitives should create a convex wedge solid");
    const ri::structural::CompiledMesh wedgeMesh = BuildPrimitiveMesh("wedge");
    Expect(wedgeMesh.triangleCount == 8U && wedgeMesh.hasBounds,
           "Structural primitives should compile a native wedge mesh");

    const auto cylinderSolid = CreateConvexPrimitiveSolid("cylinder", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
        options.radialSegments = 8;
    }));
    Expect(cylinderSolid.has_value() && cylinderSolid->polygons.size() == 10U,
           "Structural primitives should create an n-gon cylinder solid");
    const ri::structural::CompiledMesh cylinderMesh =
        BuildPrimitiveMesh("cylinder", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 8;
        }));
    Expect(cylinderMesh.triangleCount == 28U && cylinderMesh.hasBounds,
           "Structural primitives should compile a native cylinder mesh");

    const auto coneSolid = CreateConvexPrimitiveSolid("cone", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
        options.sides = 6;
    }));
    Expect(coneSolid.has_value() && coneSolid->polygons.size() == 7U,
           "Structural primitives should create a convex cone solid");
    const ri::structural::CompiledMesh coneMesh =
        BuildPrimitiveMesh("cone", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.sides = 6;
        }));
    Expect(coneMesh.triangleCount == 10U && coneMesh.hasBounds,
           "Structural primitives should compile a native cone mesh");

    const auto pyramidSolid = CreateConvexPrimitiveSolid("pyramid");
    Expect(pyramidSolid.has_value() && pyramidSolid->polygons.size() == 5U,
           "Structural primitives should create a convex pyramid solid");
    const ri::structural::CompiledMesh pyramidMesh = BuildPrimitiveMesh("pyramid");
    Expect(pyramidMesh.triangleCount == 6U && pyramidMesh.hasBounds,
           "Structural primitives should compile a native pyramid mesh");

    const ri::structural::CompiledMesh planeMesh = BuildPrimitiveMesh("plane");
    Expect(planeMesh.triangleCount == 2U
               && planeMesh.hasBounds
               && NearlyEqual(planeMesh.boundsMin.z, 0.0f)
               && NearlyEqual(planeMesh.boundsMax.z, 0.0f),
           "Structural primitives should compile a native plane mesh");

    const ri::structural::CompiledMesh hollowBoxMesh = BuildPrimitiveMesh("hollow_box");
    Expect(hollowBoxMesh.triangleCount == 72U
               && hollowBoxMesh.hasBounds
               && NearlyEqual(hollowBoxMesh.boundsMin.x, -0.5f)
               && NearlyEqual(hollowBoxMesh.boundsMax.y, 0.5f),
           "Structural primitives should compile a native hollow box shell mesh");

    const auto frustumSolid = CreateConvexPrimitiveSolid("frustum", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
        options.radialSegments = 8;
        options.topRadius = 0.2f;
        options.bottomRadius = 0.5f;
    }));
    Expect(frustumSolid.has_value() && frustumSolid->polygons.size() == 10U,
           "Structural primitives should create a convex frustum solid");
    const ri::structural::CompiledMesh frustumMesh =
        BuildPrimitiveMesh("frustum", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 8;
            options.topRadius = 0.2f;
            options.bottomRadius = 0.5f;
        }));
    Expect(frustumMesh.triangleCount == 28U && frustumMesh.hasBounds,
           "Structural primitives should compile a native frustum mesh");

    const auto geodesicSphereSolid = CreateConvexPrimitiveSolid("geodesic_sphere");
    Expect(geodesicSphereSolid.has_value() && geodesicSphereSolid->polygons.size() == 20U,
           "Structural primitives should create a convex geodesic sphere solid");
    const ri::structural::CompiledMesh geodesicSphereMesh = BuildPrimitiveMesh("geodesic_sphere");
    Expect(geodesicSphereMesh.triangleCount == 20U
               && geodesicSphereMesh.hasBounds
               && geodesicSphereMesh.boundsMax.y > 0.4f,
           "Structural primitives should compile a native geodesic sphere mesh");

    const ri::structural::CompiledMesh detailedGeodesicSphereMesh =
        BuildPrimitiveMesh("geodesic_sphere", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.detail = 2;
        }));
    Expect(detailedGeodesicSphereMesh.triangleCount == 320U
               && detailedGeodesicSphereMesh.hasBounds
               && detailedGeodesicSphereMesh.boundsMax.x > 0.45f,
           "Structural primitives should compile detail-controlled geodesic sphere meshes");

    const ri::structural::CompiledMesh superellipsoidMesh =
        BuildPrimitiveMesh("superellipsoid", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 10;
            options.sides = 16;
            options.exponentX = 0.35f;
            options.exponentY = 0.5f;
            options.exponentZ = 0.75f;
        }));
    Expect(superellipsoidMesh.triangleCount == 320U
               && superellipsoidMesh.hasBounds
               && superellipsoidMesh.boundsMax.y > 0.45f,
           "Structural primitives should compile signed-power superellipsoid meshes");

    const std::vector<ri::math::Vec2> sanitizedProfile =
        ri::scene::SanitizePrimitiveProfile2dLoop({
            {std::numeric_limits<float>::quiet_NaN(), 0.0f},
            {-10.0f, -0.25f},
            {0.25f, -0.25f},
            {0.25f, 0.25f},
        }, "diamond", 16U, 0.5f);
    Expect(sanitizedProfile.size() >= 3U
               && sanitizedProfile.front().x >= -0.5f
               && sanitizedProfile.front().x <= 0.5f,
           "Profile sanitizer should normalize arbitrary authored points into bounded 2D loops");

    const ri::structural::CompiledMesh extrudeMesh =
        BuildPrimitiveMesh("extrude_along_normal_primitive", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.depth = 0.35f;
            options.points = {{-0.5f, -0.25f, 0.0f}, {0.5f, -0.25f, 0.0f}, {0.0f, 0.5f, 0.0f}};
        }));
    Expect(extrudeMesh.triangleCount == 12U
               && extrudeMesh.hasBounds
               && extrudeMesh.boundsMax.z > 0.15f,
           "Structural primitives should compile author-profile extrude-along-normal meshes");

    const ri::structural::CompiledMesh latticeMesh =
        BuildPrimitiveMesh("lattice_volume", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.cellsX = 2;
            options.cellsY = 1;
            options.cellsZ = 2;
            options.latticeStyle = "octet_truss";
            options.strutRadius = 0.025f;
        }));
    Expect(latticeMesh.triangleCount > 200U
               && latticeMesh.hasBounds
               && latticeMesh.boundsMin.x < -0.5f
               && latticeMesh.boundsMax.z > 0.5f,
           "Structural primitives should compile deduplicated cell lattice volume meshes");

    const ri::structural::CompiledMesh stairsMesh =
        BuildPrimitiveMesh("stairs", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.steps = 5;
        }));
    Expect(stairsMesh.triangleCount == 60U
               && stairsMesh.hasBounds
               && stairsMesh.boundsMax.y > 0.45f,
           "Structural primitives should compile native stepped stair meshes");

    const ri::structural::CompiledMesh spiralStairsMesh =
        BuildPrimitiveMesh("spiral_stairs", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.steps = 6;
            options.radialSegments = 12;
            options.sweepDegrees = 270.0f;
            options.startDegrees = -120.0f;
            options.topRadius = 0.14f;
            options.bottomRadius = 0.48f;
            options.centerColumn = true;
        }));
    Expect(spiralStairsMesh.triangleCount == 120U
               && spiralStairsMesh.hasBounds
               && spiralStairsMesh.boundsMax.y > 0.45f,
           "Structural primitives should compile native spiral stair meshes with a center column");

    const ri::structural::CompiledMesh tubeMesh =
        BuildPrimitiveMesh("tube", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.topRadius = 0.24f;
        }));
    Expect(tubeMesh.triangleCount == 96U
               && tubeMesh.hasBounds
               && tubeMesh.boundsMax.z > 0.45f,
           "Structural primitives should compile native hollow tube meshes");

    const ri::structural::CompiledMesh torusMesh =
        BuildPrimitiveMesh("torus", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.sides = 8;
            options.thickness = 0.12f;
        }));
    Expect(torusMesh.triangleCount == 192U
               && torusMesh.hasBounds
               && torusMesh.boundsMax.x > 0.45f,
           "Structural primitives should compile native full torus meshes");

    const ri::structural::CompiledMesh cornerMesh =
        BuildPrimitiveMesh("corner", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.thickness = 0.18f;
            options.radialSegments = 4;
            options.archStyle = "rounded";
        }));
    Expect(cornerMesh.triangleCount == 32U
               && cornerMesh.hasBounds
               && cornerMesh.boundsMax.x > 0.45f,
           "Structural primitives should compile native mitered/rounded corner meshes");

    const ri::structural::CompiledMesh halfPipeMesh =
        BuildPrimitiveMesh("half_pipe", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.thickness = 0.12f;
        }));
    Expect(halfPipeMesh.triangleCount > 40U
               && halfPipeMesh.hasBounds
               && halfPipeMesh.boundsMax.y > 0.4f,
           "Structural primitives should compile native half-pipe channel meshes");

    const ri::structural::CompiledMesh quarterPipeMesh =
        BuildPrimitiveMesh("quarter_pipe", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 10;
            options.thickness = 0.1f;
        }));
    Expect(quarterPipeMesh.triangleCount > 30U
               && quarterPipeMesh.hasBounds
               && quarterPipeMesh.boundsMax.x > 0.3f,
           "Structural primitives should compile native quarter-pipe channel meshes");

    const ri::structural::CompiledMesh pipeElbowMesh =
        BuildPrimitiveMesh("pipe_elbow", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.sides = 8;
            options.sweepDegrees = 90.0f;
            options.length = 0.32f;
            options.thickness = 0.08f;
        }));
    Expect(pipeElbowMesh.triangleCount == 192U
               && pipeElbowMesh.hasBounds
               && pipeElbowMesh.boundsMax.z > 0.25f,
           "Structural primitives should compile native pipe elbow arc meshes");

    const ri::structural::CompiledMesh torusSliceMesh =
        BuildPrimitiveMesh("torus_slice", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 14;
            options.sides = 8;
            options.sweepDegrees = 135.0f;
            options.length = 0.34f;
            options.thickness = 0.06f;
        }));
    Expect(torusSliceMesh.triangleCount == 224U
               && torusSliceMesh.hasBounds
               && torusSliceMesh.boundsMax.x > 0.3f,
           "Structural primitives should compile native torus-slice meshes");

    const ri::structural::CompiledMesh splineSweepMesh =
        BuildPrimitiveMesh("spline_sweep", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.thickness = 0.04f;
            options.points = {{-0.45f, 0.0f, -0.4f}, {0.0f, 0.25f, 0.0f}, {0.45f, 0.0f, 0.4f}};
        }));
    Expect(splineSweepMesh.triangleCount == 24U
               && splineSweepMesh.hasBounds
               && splineSweepMesh.boundsMax.y > 0.25f,
           "Structural primitives should compile native spline-sweep beam meshes");

    const ri::structural::CompiledMesh revolveMesh =
        BuildPrimitiveMesh("revolve", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.sweepDegrees = 270.0f;
            options.points = {{0.18f, -0.5f, 0.0f}, {0.34f, -0.1f, 0.0f}, {0.2f, 0.45f, 0.0f}};
        }));
    Expect(revolveMesh.triangleCount == 72U
               && revolveMesh.hasBounds
               && revolveMesh.boundsMax.y > 0.4f,
           "Structural primitives should compile native profile-revolve meshes");

    const ri::structural::CompiledMesh domeVaultMesh =
        BuildPrimitiveMesh("dome_vault", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.sides = 6;
            options.ridgeRatio = 0.8f;
        }));
    Expect(domeVaultMesh.triangleCount == 144U
               && domeVaultMesh.hasBounds
               && domeVaultMesh.boundsMax.y > 0.35f,
           "Structural primitives should compile native dome-vault meshes");

    const ri::structural::CompiledMesh loftMesh =
        BuildPrimitiveMesh("loft_primitive", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.points = {{-0.4f, -0.25f, 0.0f}, {0.4f, -0.25f, 0.0f}, {0.4f, 0.25f, 0.0f}, {-0.4f, 0.25f, 0.0f}};
            options.vertices = {{0.0f, 0.0f, -0.5f}, {0.15f, 0.1f, 0.0f}, {0.0f, 0.0f, 0.5f}};
        }));
    Expect(loftMesh.triangleCount == 16U
               && loftMesh.hasBounds
               && loftMesh.boundsMax.z > 0.45f,
           "Structural primitives should compile native profile loft meshes");

    const ri::structural::CompiledMesh splineRibbonMesh =
        BuildPrimitiveMesh("spline_ribbon", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.thickness = 0.08f;
            options.points = {{-0.5f, 0.0f, -0.25f}, {0.0f, 0.12f, 0.0f}, {0.5f, 0.0f, 0.25f}};
        }));
    Expect(splineRibbonMesh.triangleCount == 4U
               && splineRibbonMesh.hasBounds
               && splineRibbonMesh.boundsMax.x > 0.45f,
           "Structural primitives should compile native spline ribbon surface meshes");

    const ri::structural::CompiledMesh catenaryMesh =
        BuildPrimitiveMesh("catenary_primitive", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.depth = 0.25f;
            options.thickness = 0.025f;
        }));
    Expect(catenaryMesh.triangleCount == 144U
               && catenaryMesh.hasBounds
               && catenaryMesh.boundsMin.y < -0.2f,
           "Structural primitives should compile native catenary cable meshes");

    const ri::structural::CompiledMesh cableMesh =
        BuildPrimitiveMesh("cable_primitive", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 10;
            options.depth = 0.18f;
            options.thickness = 0.02f;
        }));
    Expect(cableMesh.triangleCount == 120U
               && cableMesh.hasBounds
               && cableMesh.boundsMin.y < -0.15f,
           "Structural primitives should compile native suspended cable meshes");

    const ri::structural::CompiledMesh thickPolygonMesh =
        BuildPrimitiveMesh("thick_polygon_primitive", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.depth = 0.4f;
            options.points = {{-0.4f, -0.3f, 0.0f}, {0.35f, -0.25f, 0.0f}, {0.45f, 0.2f, 0.0f}, {-0.25f, 0.35f, 0.0f}};
        }));
    Expect(thickPolygonMesh.triangleCount == 16U
               && thickPolygonMesh.hasBounds
               && thickPolygonMesh.boundsMax.y > 0.15f,
           "Structural primitives should compile native thick polygon blockout meshes");

    const ri::structural::CompiledMesh trimSheetSweepMesh =
        BuildPrimitiveMesh("trim_sheet_sweep", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.thickness = 0.035f;
            options.points = {{-0.45f, 0.0f, -0.3f}, {0.0f, 0.1f, 0.0f}, {0.45f, 0.0f, 0.3f}};
        }));
    Expect(trimSheetSweepMesh.triangleCount == 24U
               && trimSheetSweepMesh.hasBounds
               && trimSheetSweepMesh.boundsMax.z > 0.3f,
           "Structural primitives should compile native trim-sheet sweep meshes");

    const ri::structural::CompiledMesh waterSurfaceMesh =
        BuildPrimitiveMesh("water_surface_primitive", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 4;
            options.thickness = 0.04f;
        }));
    Expect(waterSurfaceMesh.triangleCount == 32U
               && waterSurfaceMesh.hasBounds
               && waterSurfaceMesh.boundsMax.y > 0.01f,
           "Structural primitives should compile native water surface tessellation meshes");

    const ri::structural::CompiledMesh terrainQuadMesh =
        BuildPrimitiveMesh("terrain_quad", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.cellsX = 4;
            options.cellsZ = 3;
            options.depth = 0.18f;
        }));
    Expect(terrainQuadMesh.triangleCount == 24U
               && terrainQuadMesh.hasBounds
               && terrainQuadMesh.boundsMax.y > 0.05f,
           "Structural primitives should compile native terrain quad meshes");

    const ri::structural::CompiledMesh heightmapPatchMesh =
        BuildPrimitiveMesh("heightmap_patch", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.cellsX = 3;
            options.cellsZ = 3;
            options.depth = 0.2f;
        }));
    Expect(heightmapPatchMesh.triangleCount == 18U
               && heightmapPatchMesh.hasBounds
               && heightmapPatchMesh.boundsMin.y < -0.05f,
           "Structural primitives should compile native heightmap patch meshes");

    const ri::structural::CompiledMesh capsuleMesh =
        BuildPrimitiveMesh("capsule", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.hemisphereSegments = 6;
            options.length = 0.5f;
        }));
    Expect(capsuleMesh.triangleCount > 100U
               && capsuleMesh.hasBounds
               && NearlyEqual(capsuleMesh.boundsMin.y, -0.5f, 0.02f)
               && NearlyEqual(capsuleMesh.boundsMax.y, 0.5f, 0.02f),
           "Structural primitives should compile a native capsule mesh");

    const ri::structural::CompiledMesh archMesh =
        BuildPrimitiveMesh("arch", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.radialSegments = 12;
            options.thickness = 0.16f;
            options.spanDegrees = 180.0f;
        }));
    Expect(archMesh.triangleCount > 40U
               && archMesh.hasBounds
               && archMesh.boundsMax.y > 0.45f,
           "Structural primitives should compile a native round arch mesh");

    const ri::structural::CompiledMesh gothicArchMesh =
        BuildPrimitiveMesh("arch", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.thickness = 0.16f;
            options.archStyle = "gothic";
        }));
    Expect(gothicArchMesh.triangleCount > 20U
               && gothicArchMesh.hasBounds
               && gothicArchMesh.boundsMax.y > 0.3f,
           "Structural primitives should compile a native gothic arch mesh");

    const auto hexahedronSolid = CreateConvexPrimitiveSolid("hexahedron", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
        options.vertices = {
            {-0.5f, -0.5f, -0.5f},
            {0.5f, -0.5f, -0.5f},
            {0.5f, -0.5f, 0.5f},
            {-0.5f, -0.5f, 0.5f},
            {-0.4f, 0.5f, -0.5f},
            {0.5f, 0.4f, -0.4f},
            {0.4f, 0.5f, 0.5f},
            {-0.5f, 0.45f, 0.4f},
        };
    }));
    Expect(hexahedronSolid.has_value() && hexahedronSolid->polygons.size() == 6U,
           "Structural primitives should create a native hexahedron solid");
    const ri::structural::CompiledMesh hexahedronMesh =
        BuildPrimitiveMesh("hexahedron", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.vertices = {
                {-0.5f, -0.5f, -0.5f},
                {0.5f, -0.5f, -0.5f},
                {0.5f, -0.5f, 0.5f},
                {-0.5f, -0.5f, 0.5f},
                {-0.4f, 0.5f, -0.5f},
                {0.5f, 0.4f, -0.4f},
                {0.4f, 0.5f, 0.5f},
                {-0.5f, 0.45f, 0.4f},
            };
        }));
    Expect(hexahedronMesh.triangleCount == 12U && hexahedronMesh.hasBounds,
           "Structural primitives should compile a native hexahedron mesh");

    const auto convexHullSolid = CreateConvexPrimitiveSolid("convex_hull", MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
        options.points = {
            {-0.5f, -0.5f, -0.5f},
            {0.5f, -0.5f, -0.5f},
            {0.5f, -0.5f, 0.5f},
            {-0.5f, -0.5f, 0.5f},
            {0.0f, 0.5f, 0.0f},
        };
    }));
    Expect(convexHullSolid.has_value() && convexHullSolid->polygons.size() == 5U,
           "Structural primitives should create a native convex hull solid");
    const ri::structural::CompiledMesh convexHullMesh = BuildPrimitiveMesh(
        "convex_hull",
        MakePrimitiveOptions([](StructuralPrimitiveOptions& options) {
            options.points = {
                {-0.5f, -0.5f, -0.5f},
                {0.5f, -0.5f, -0.5f},
                {0.5f, -0.5f, 0.5f},
                {-0.5f, -0.5f, 0.5f},
                {0.0f, 0.5f, 0.0f},
            };
        }));
    Expect(convexHullMesh.triangleCount == 6U && convexHullMesh.hasBounds,
           "Structural primitives should compile a native convex hull mesh");

    const auto gableRoofSolid = CreateConvexPrimitiveSolid("roof_gable");
    Expect(gableRoofSolid.has_value() && gableRoofSolid->polygons.size() == 7U,
           "Structural primitives should create a native gable roof solid");
    const ri::structural::CompiledMesh gableRoofMesh = BuildPrimitiveMesh("roof_gable");
    Expect(gableRoofMesh.triangleCount == 16U && gableRoofMesh.hasBounds,
           "Structural primitives should compile a native gable roof mesh");

    const auto hippedRoofSolid = CreateConvexPrimitiveSolid("hipped_roof");
    Expect(hippedRoofSolid.has_value() && hippedRoofSolid->polygons.size() >= 6U,
           "Structural primitives should create a native hipped roof solid");
    const ri::structural::CompiledMesh hippedRoofMesh = BuildPrimitiveMesh("hipped_roof");
    Expect(hippedRoofMesh.triangleCount >= 8U && hippedRoofMesh.hasBounds,
           "Structural primitives should compile a native hipped roof mesh");
}

void TestConvexClipperAndCompiler() {
    const ri::structural::ConvexSolid box = ri::structural::CreateAxisAlignedBoxSolid();
    Expect(box.polygons.size() == 6U, "Axis-aligned box should generate six faces");
    Expect(ri::structural::ToString(ri::structural::ClassifyPointToPlane({1.0f, 0.0f, 0.0f}, ri::structural::CreatePlaneFromPointNormal({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}))) == "front",
           "Point classification should detect front-side points");

    const ri::structural::Plane splitPlane = ri::structural::CreatePlaneFromPointNormal({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    const ri::structural::ConvexSolidClipResult split = ri::structural::ClipConvexSolidByPlane(box, splitPlane);
    Expect(split.split, "Convex solid clipper should report a split when slicing through the box center");
    Expect(split.front.has_value() && split.back.has_value(), "Convex solid clipper should return both halves of a center split");
    Expect(split.capPoints.size() == 4U, "Convex solid clipper should create a quad cap for a center box split");

    const auto boxBounds = ri::structural::ComputeSolidBounds(box);
    Expect(boxBounds.has_value(), "Structural compiler should compute bounds for non-empty solids");
    ExpectVec3(boxBounds->min, {-0.5f, -0.5f, -0.5f}, "Box bounds min");
    ExpectVec3(boxBounds->max, {0.5f, 0.5f, 0.5f}, "Box bounds max");

    const ri::math::Mat4 translated = ri::math::TranslationMatrix({2.0f, 3.0f, 4.0f});
    const ri::structural::ConvexSolid worldBox = ri::structural::CreateWorldSpaceBoxSolid(translated);
    const auto worldBounds = ri::structural::ComputeSolidBounds(worldBox);
    Expect(worldBounds.has_value(), "World-space box should still have bounds");
    ExpectVec3(worldBounds->min, {1.5f, 2.5f, 3.5f}, "World-space box bounds min");
    ExpectVec3(worldBounds->max, {2.5f, 3.5f, 4.5f}, "World-space box bounds max");

    const ri::structural::CompiledMesh mesh = ri::structural::BuildCompiledMeshFromConvexSolid(box);
    Expect(mesh.triangleCount == 12U, "Compiled mesh should triangulate a box into twelve triangles");
    Expect(mesh.positions.size() == 36U, "Compiled mesh should emit three vertices per triangle");
    Expect(mesh.normals.size() == mesh.positions.size(), "Compiled mesh should emit a normal for every vertex");

    std::vector<ri::structural::Triangle> triangles;
    for (std::size_t index = 0; index < mesh.positions.size(); index += 3) {
        triangles.push_back(ri::structural::Triangle{
            .a = mesh.positions[index],
            .b = mesh.positions[index + 1],
            .c = mesh.positions[index + 2],
        });
    }
    const std::vector<ri::structural::Plane> planes = ri::structural::ExtractConvexPlanesFromTriangles(triangles);
    Expect(planes.size() == 6U, "Plane extraction should collapse box triangles into six unique planes");

    const std::vector<ri::structural::ConvexSolid> insidePieces = ri::structural::IntersectSolidWithConvexPlanes(box, {splitPlane});
    const std::vector<ri::structural::ConvexSolid> outsidePieces = ri::structural::SubtractConvexPlanesFromSolid(box, {splitPlane});
    Expect(insidePieces.size() == 1U, "Convex intersection should keep one inside piece for a single clipping plane");
    Expect(outsidePieces.size() == 1U, "Convex subtraction should return one outside piece for a single clipping plane");

    const std::vector<ri::structural::CompiledGeometryNode> nodes = ri::structural::BuildCompiledGeometryNodesFromSolids(
        MakeNode("wall", "Wall", "box"),
        outsidePieces,
        "raw");
    Expect(nodes.size() == 1U, "Compiled geometry node builder should emit one node per solid");
    Expect(nodes[0].node.id == "raw_fragment_1", "Compiled geometry node builder should suffix fragment IDs");
    Expect(nodes[0].node.name == "Wall fragment 1", "Compiled geometry node builder should suffix fragment names");
    Expect(nodes[0].compiledMesh.triangleCount == 12U, "Compiled geometry node builder should carry compiled triangle counts");
    const std::vector<ri::structural::CompiledGeometryNode> anonymousNodes = ri::structural::BuildCompiledGeometryNodesFromSolids(
        MakeNode("", "Anonymous", "box"),
        outsidePieces,
        "anonymous");
    Expect(anonymousNodes.size() == 1U && anonymousNodes[0].node.id == "anonymous_fragment_1",
           "Compiled geometry node builder should honor the supplied fragment prefix even when the source node has no ID");

    ri::structural::StructuralNode unionNode = MakeNode("union_op", "Union", "boolean_union");
    unionNode.targetIds = {"box_a", "box_b"};
    ri::structural::StructuralNode intersectionNode = MakeNode("intersect_op", "Intersection", "boolean_intersection");
    intersectionNode.childNodeList = {"box_a", "box_b"};
    ri::structural::StructuralNode differenceNode = MakeNode("diff_op", "Difference", "boolean_difference");
    differenceNode.targetIds = {"box_a", "box_b"};

    Expect(ri::structural::GetBooleanOperatorTargetIds(unionNode).size() == 2U,
           "Structural compiler should read boolean operator target IDs from targetIds");
    Expect(ri::structural::GetBooleanOperatorTargetIds(intersectionNode).size() == 2U,
           "Structural compiler should fall back to childNodeList for boolean operator target IDs");

    const ri::structural::StructuralBooleanTarget targetA{
        .node = MakeNode("box_a", "Box A", "box"),
        .solids = {ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({0.0f, 0.0f, 0.0f}))},
    };
    const ri::structural::StructuralBooleanTarget targetB{
        .node = MakeNode("box_b", "Box B", "box"),
        .solids = {ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({0.5f, 0.0f, 0.0f}))},
    };
    const std::vector<ri::structural::StructuralBooleanTarget> targets = {targetA, targetB};
    const auto AggregateCompiledBounds = [](const std::vector<ri::structural::CompiledGeometryNode>& compiledNodes) {
        ri::structural::Bounds bounds{};
        bool hasBounds = false;
        for (const ri::structural::CompiledGeometryNode& compiledNode : compiledNodes) {
            if (!compiledNode.compiledMesh.hasBounds) {
                continue;
            }
            if (!hasBounds) {
                bounds.min = compiledNode.compiledMesh.boundsMin;
                bounds.max = compiledNode.compiledMesh.boundsMax;
                hasBounds = true;
                continue;
            }
            bounds.min.x = std::min(bounds.min.x, compiledNode.compiledMesh.boundsMin.x);
            bounds.min.y = std::min(bounds.min.y, compiledNode.compiledMesh.boundsMin.y);
            bounds.min.z = std::min(bounds.min.z, compiledNode.compiledMesh.boundsMin.z);
            bounds.max.x = std::max(bounds.max.x, compiledNode.compiledMesh.boundsMax.x);
            bounds.max.y = std::max(bounds.max.y, compiledNode.compiledMesh.boundsMax.y);
            bounds.max.z = std::max(bounds.max.z, compiledNode.compiledMesh.boundsMax.z);
        }
        Expect(hasBounds, "Boolean compiler results should keep compiled bounds");
        return bounds;
    };

    Expect(ri::structural::SupportsBooleanAdditiveTarget(targetA),
           "Structural compiler should treat non-empty solid targets as boolean-additive inputs");

    const ri::structural::StructuralBooleanCompileResult unionResult =
        ri::structural::CompileBooleanUnionNode(unionNode, targets);
    Expect(!unionResult.compiledNodes.empty(),
           "Structural compiler should emit union fragments for overlapping additive targets");
    Expect(unionResult.targetIds.size() == 2U,
           "Structural compiler should report the boolean union target IDs it consumed");
    Expect(std::all_of(unionResult.compiledNodes.begin(), unionResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.compiledMesh.triangleCount > 0U && node.compiledMesh.hasBounds;
           }),
           "Structural compiler should keep convex fragment meshes valid through boolean union compilation");
    Expect(unionResult.compiledNodes[0].node.id == "union_op_box_a_fragment_1",
           "Structural compiler should prefix boolean union fragment IDs with the operator ID");
    const ri::structural::Bounds unionBounds = AggregateCompiledBounds(unionResult.compiledNodes);
    Expect(NearlyEqual(unionBounds.min.x, -0.5f) && NearlyEqual(unionBounds.max.x, 1.0f),
           "Structural compiler should preserve the full additive coverage of a two-box boolean union");

    const ri::structural::StructuralBooleanCompileResult intersectionResult =
        ri::structural::CompileBooleanIntersectionNode(intersectionNode, targets);
    Expect(!intersectionResult.compiledNodes.empty(),
           "Structural compiler should emit an overlapping fragment for a simple two-box intersection");
    Expect(std::all_of(intersectionResult.compiledNodes.begin(), intersectionResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.compiledMesh.triangleCount > 0U && node.compiledMesh.hasBounds;
           }),
           "Structural compiler should compile convex intersection fragments into valid meshes");
    const ri::structural::Bounds intersectionBounds = AggregateCompiledBounds(intersectionResult.compiledNodes);
    Expect(NearlyEqual(intersectionBounds.min.x, 0.0f)
           && NearlyEqual(intersectionBounds.max.x, 0.5f),
           "Structural compiler should preserve the correct overlap slab for boolean intersections");

    const ri::structural::StructuralBooleanCompileResult differenceResult =
        ri::structural::CompileBooleanDifferenceNode(differenceNode, targets);
    Expect(!differenceResult.compiledNodes.empty(),
           "Structural compiler should emit surviving convex fragments for boolean differences");
    Expect(std::all_of(differenceResult.compiledNodes.begin(), differenceResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.compiledMesh.triangleCount > 0U && node.compiledMesh.hasBounds;
           }),
           "Structural compiler should preserve bounds on boolean difference output");
    const ri::structural::Bounds differenceBounds = AggregateCompiledBounds(differenceResult.compiledNodes);
    Expect(NearlyEqual(differenceBounds.min.x, -0.5f)
           && NearlyEqual(differenceBounds.max.x, 1.0f),
           "Structural compiler should keep the expected left and right slabs when compiling boolean differences");
    Expect(std::any_of(differenceResult.compiledNodes.begin(), differenceResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return NearlyEqual(node.compiledMesh.boundsMax.x, 0.0f);
           }) && std::any_of(differenceResult.compiledNodes.begin(), differenceResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return NearlyEqual(node.compiledMesh.boundsMin.x, 0.5f);
           }),
           "Structural compiler should preserve the expected left and right slabs for simple boolean differences");

    ri::structural::StructuralNode aggregateNode = MakeNode("aggregate_op", "Aggregate", "convex_hull_aggregate");
    aggregateNode.targetIds = {"box_a", "box_b"};
    const std::vector<ri::structural::CompiledGeometryNode> aggregateResult =
        ri::structural::CompileConvexHullAggregateNode(aggregateNode, targets);
    Expect(aggregateResult.size() == 1U,
           "Structural compiler should emit one compiled node for a convex hull aggregate");
    Expect(aggregateResult[0].node.id == "aggregate_op_fragment_1",
           "Structural compiler should name convex hull aggregate output from the aggregate node ID");
    Expect(aggregateResult[0].compiledMesh.hasBounds
           && NearlyEqual(aggregateResult[0].compiledMesh.boundsMin.x, -0.5f)
           && NearlyEqual(aggregateResult[0].compiledMesh.boundsMax.x, 1.0f),
           "Structural compiler should wrap the full authored target span in the aggregate convex hull bounds");
    Expect(aggregateResult[0].compiledMesh.triangleCount > 0U,
           "Structural compiler should compile a usable convex hull aggregate mesh");

    ri::structural::StructuralNode arrayNode = MakeNode("array_a", "Array", "array_primitive");
    arrayNode.position = {10.0f, 0.0f, 0.0f};
    arrayNode.count = 3;
    arrayNode.primitiveType = "box";
    arrayNode.offsetStepPosition = {2.0f, 0.0f, 0.0f};
    arrayNode.basePrimitive = std::make_shared<ri::structural::StructuralNode>(MakeNode("base_box", "Base Box", "box"));
    arrayNode.basePrimitive->position = {1.0f, 0.0f, 0.0f};

    const std::vector<ri::structural::StructuralNode> expandedArray =
        ri::structural::ExpandArrayPrimitiveNodes({arrayNode});
    Expect(expandedArray.size() == 3U,
           "Structural compiler should expand array primitives into repeated authored nodes");
    Expect(expandedArray[0].id == "base_box_array_1"
           && expandedArray[1].id == "base_box_array_2"
           && expandedArray[2].id == "base_box_array_3",
           "Structural compiler should generate stable IDs for expanded array primitives");
    ExpectVec3(expandedArray[0].position, {11.0f, 0.0f, 0.0f},
               "Structural compiler should place the first expanded array primitive at the composed root-plus-base transform");
    ExpectVec3(expandedArray[1].position, {13.0f, 0.0f, 0.0f},
               "Structural compiler should advance expanded array primitives by the authored offset step");
    Expect(expandedArray[2].type == "box",
           "Structural compiler should resolve array primitive outputs to the underlying primitive type");

    ri::structural::StructuralNode mirroredBase = MakeNode("pillar_a", "Pillar", "box");
    mirroredBase.position = {2.0f, 3.0f, 4.0f};
    mirroredBase.rotation = {10.0f, 20.0f, 30.0f};
    ri::structural::StructuralNode mirrorNode = MakeNode("mirror_a", "Mirror", "symmetry_mirror_plane", {"pillar_a"});
    mirrorNode.mirrorAxis = "x";
    mirrorNode.position = {1.0f, 0.0f, 0.0f};

    const std::vector<ri::structural::StructuralNode> mirroredNodes =
        ri::structural::ExpandSymmetryMirrorNodes({mirroredBase, mirrorNode});
    Expect(mirroredNodes.size() == 3U,
           "Structural compiler should append mirrored structural nodes for symmetry planes");
    const ri::structural::StructuralNode& mirroredPillar = mirroredNodes.back();
    Expect(mirroredPillar.id == "pillar_a_mirrored_1"
           && mirroredPillar.mirroredFrom == "pillar_a",
           "Structural compiler should preserve source identity when expanding symmetry mirror nodes");
    ExpectVec3(mirroredPillar.position, {0.0f, 3.0f, 4.0f},
               "Structural compiler should mirror authored positions across the requested axis plane");
    ExpectVec3(mirroredPillar.rotation, {10.0f, -20.0f, -30.0f},
               "Structural compiler should mirror authored Euler rotations using the prototype axis rules");

    ri::structural::StructuralNode beveledTarget = MakeNode("bevel_target", "Bevel Target", "box");
    ri::structural::StructuralNode bevelModifier = MakeNode("bevel_modifier", "Bevel Modifier", "bevel_modifier_primitive");
    bevelModifier.position = {};
    bevelModifier.scale = {2.0f, 2.0f, 2.0f};
    bevelModifier.bevelRadius = 0.18f;
    bevelModifier.bevelSegments = 6;

    const ri::structural::StructuralNode beveledNode =
        ri::structural::ApplyBevelModifiersToNode(beveledTarget, {bevelModifier});
    Expect(NearlyEqual(beveledNode.bevelRadius, 0.18f) && beveledNode.bevelSegments == 6,
           "Structural compiler should carry bevel metadata onto overlapping authored box nodes");

    ri::structural::StructuralNode detailTarget = MakeNode("detail_target", "Detail Target", "box");
    detailTarget.position = {4.0f, 0.0f, 0.0f};
    ri::structural::StructuralNode detailModifier = MakeNode("detail_modifier", "Detail Modifier", "structural_detail_modifier");
    detailModifier.position = {4.0f, 0.0f, 0.0f};
    detailModifier.scale = {2.0f, 2.0f, 2.0f};

    const ri::structural::StructuralNode detailedNode =
        ri::structural::ApplyStructuralDetailModifiersToNode(detailTarget, {detailModifier});
    Expect(!detailedNode.isStructural
           && detailedNode.detailOnly
           && detailedNode.excludeFromVisibility
           && detailedNode.excludeFromNavigation,
           "Structural compiler should mark intersecting authored nodes as structural detail when a detail modifier overlaps them");

    ri::structural::StructuralNode hexahedronNode = MakeNode("hexa_a", "Hexahedron", "hexahedron");
    hexahedronNode.vertices = {
        {-0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f},
        {-0.4f, 0.5f, -0.5f},
        {0.5f, 0.5f, -0.4f},
        {0.4f, 0.5f, 0.5f},
        {-0.5f, 0.5f, 0.4f},
    };
    hexahedronNode.position = {2.0f, 0.0f, 0.0f};

    ri::structural::StructuralNode reconciler = MakeNode("reconcile_a", "Reconciler", "non_manifold_reconciler");
    reconciler.position = {2.0f, 0.0f, 0.0f};
    reconciler.scale = {3.0f, 3.0f, 3.0f};
    reconciler.targetIds = {"hexa_a"};

    const ri::structural::StructuralNode reconciledNode =
        ri::structural::ApplyNonManifoldReconcilersToNode(hexahedronNode, {reconciler});
    Expect(reconciledNode.type == "convex_hull"
           && reconciledNode.reconciledNonManifold
           && reconciledNode.position.x == 0.0f
           && reconciledNode.scale.x == 1.0f,
           "Structural compiler should hullize targeted risky geometry into a world-space convex hull when reconcilers apply");
    Expect(!reconciledNode.points.empty(),
           "Structural compiler should preserve world-space hull points on reconciled nodes");
    const std::optional<ri::structural::Bounds> reconciledBounds =
        ri::structural::CreateGeometryBoundsForNode(reconciledNode);
    Expect(reconciledBounds.has_value()
           && reconciledBounds->min.x > 1.0f
           && reconciledBounds->max.x < 3.0f,
           "Structural compiler should keep reconciled convex hull geometry near the transformed authored source bounds");

    ri::structural::StructuralNode pipelineArray = MakeNode("array_seed", "Array Seed", "array_primitive");
    pipelineArray.count = 2;
    pipelineArray.offsetStepPosition = {0.5f, 0.0f, 0.0f};
    pipelineArray.basePrimitive = std::make_shared<ri::structural::StructuralNode>(MakeNode("array_box", "Array Box", "box"));

    ri::structural::StructuralNode pipelineMirrorBase = MakeNode("mirror_base", "Mirror Base", "box");
    pipelineMirrorBase.position = {2.0f, 0.0f, 0.0f};
    ri::structural::StructuralNode pipelineMirror = MakeNode("mirror_plane", "Mirror Plane", "symmetry_mirror_plane", {"mirror_base"});
    pipelineMirror.position = {1.0f, 0.0f, 0.0f};
    pipelineMirror.mirrorAxis = "x";

    ri::structural::StructuralNode pipelineUnion = MakeNode("union_pipeline", "Union Pipeline", "boolean_union");
    pipelineUnion.targetIds = {"array_box_array_1", "array_box_array_2"};

    ri::structural::StructuralNode pipelineAggregate = MakeNode("aggregate_pipeline", "Aggregate Pipeline", "convex_hull_aggregate");
    pipelineAggregate.targetIds = {"array_box_array_1", "array_box_array_2"};

    ri::structural::StructuralNode pipelineBevelTarget = MakeNode("box_bevel", "Box Bevel", "box");
    ri::structural::StructuralNode pipelineBevelModifier = MakeNode("bevel_pipeline", "Pipeline Bevel", "bevel_modifier_primitive");
    pipelineBevelModifier.scale = {2.0f, 2.0f, 2.0f};
    pipelineBevelModifier.bevelRadius = 0.2f;
    pipelineBevelModifier.bevelSegments = 5;
    pipelineBevelModifier.targetIds = {"box_bevel"};

    ri::structural::StructuralNode pipelineDetailTarget = MakeNode("box_detail", "Box Detail", "box");
    pipelineDetailTarget.position = {4.0f, 0.0f, 0.0f};
    ri::structural::StructuralNode pipelineDetailModifier = MakeNode("detail_pipeline", "Pipeline Detail", "structural_detail_modifier");
    pipelineDetailModifier.position = {4.0f, 0.0f, 0.0f};
    pipelineDetailModifier.scale = {2.0f, 2.0f, 2.0f};

    ri::structural::StructuralNode pipelineHexa = MakeNode("hexa_pipeline", "Hexa Pipeline", "hexahedron");
    pipelineHexa.position = {6.0f, 0.0f, 0.0f};
    pipelineHexa.vertices = {
        {-0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, 0.5f},
        {-0.5f, -0.5f, 0.5f},
        {-0.4f, 0.5f, -0.5f},
        {0.5f, 0.5f, -0.4f},
        {0.4f, 0.5f, 0.5f},
        {-0.5f, 0.5f, 0.4f},
    };
    ri::structural::StructuralNode pipelineReconciler = MakeNode("reconcile_pipeline", "Pipeline Reconciler", "non_manifold_reconciler");
    pipelineReconciler.position = {6.0f, 0.0f, 0.0f};
    pipelineReconciler.scale = {3.0f, 3.0f, 3.0f};
    pipelineReconciler.targetIds = {"hexa_pipeline"};

    const ri::structural::StructuralGeometryCompileResult pipelineResult =
        ri::structural::CompileStructuralGeometryNodes({
            pipelineArray,
            pipelineMirrorBase,
            pipelineMirror,
            pipelineUnion,
            pipelineAggregate,
            pipelineBevelTarget,
            pipelineBevelModifier,
            pipelineDetailTarget,
            pipelineDetailModifier,
            pipelineHexa,
            pipelineReconciler,
        });

    Expect(std::any_of(pipelineResult.expandedNodes.begin(), pipelineResult.expandedNodes.end(), [](const ri::structural::StructuralNode& node) {
               return node.id == "array_box_array_1";
           }) && std::any_of(pipelineResult.expandedNodes.begin(), pipelineResult.expandedNodes.end(), [](const ri::structural::StructuralNode& node) {
               return node.id == "mirror_base_mirrored_1";
           }),
           "Structural compiler orchestration should run array and symmetry expansion before later compile stages");
    Expect(std::any_of(pipelineResult.compiledNodes.begin(), pipelineResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id == "aggregate_pipeline_fragment_1";
           }),
           "Structural compiler orchestration should compile convex hull aggregate outputs from authored target IDs");
    Expect(std::any_of(pipelineResult.compiledNodes.begin(), pipelineResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id == "union_pipeline_array_box_array_1_fragment_1";
           }),
           "Structural compiler orchestration should compile native boolean union outputs from expanded authored targets");
    Expect(std::find(pipelineResult.suppressedTargetIds.begin(), pipelineResult.suppressedTargetIds.end(), "array_box_array_1")
               != pipelineResult.suppressedTargetIds.end(),
           "Structural compiler orchestration should record suppressed boolean target IDs after union compilation");
    const auto FindPassthroughById = [&](const std::string& id) -> const ri::structural::StructuralNode* {
        auto found = std::find_if(pipelineResult.passthroughNodes.begin(), pipelineResult.passthroughNodes.end(), [&](const ri::structural::StructuralNode& node) {
            return node.id == id;
        });
        return found != pipelineResult.passthroughNodes.end() ? &(*found) : nullptr;
    };
    const ri::structural::StructuralNode* pipelineBeveled = FindPassthroughById("box_bevel");
    Expect(pipelineBeveled != nullptr && NearlyEqual(pipelineBeveled->bevelRadius, 0.2f) && pipelineBeveled->bevelSegments == 5,
           "Structural compiler orchestration should carry bevel metadata through the passthrough node set");
    const ri::structural::StructuralNode* pipelineDetailed = FindPassthroughById("box_detail");
    Expect(pipelineDetailed != nullptr && pipelineDetailed->detailOnly && !pipelineDetailed->isStructural,
           "Structural compiler orchestration should preserve detail-only structural tagging in passthrough output");
    Expect(pipelineResult.bevelModifiersApplied >= 1U && pipelineResult.detailModifiersApplied >= 1U,
           "Structural compiler orchestration should report how many authored nodes were affected by bevel/detail modifier passes");
    const ri::structural::StructuralNode* pipelineReconciled = FindPassthroughById("hexa_pipeline");
    Expect(pipelineReconciled != nullptr && pipelineReconciled->type == "convex_hull" && pipelineReconciled->reconciledNonManifold,
           "Structural compiler orchestration should carry reconciled authored geometry through the passthrough node set");
    Expect(FindPassthroughById("array_box_array_1") == nullptr,
           "Structural compiler orchestration should suppress boolean-consumed targets from passthrough output");
    const std::uint64_t signatureA = ri::structural::BuildStructuralCompileSignature(
        {pipelineBevelTarget, pipelineBevelModifier},
        {});
    const ri::structural::StructuralCompileIncrementalResult incrementalA =
        ri::structural::CompileStructuralGeometryNodesIncremental(
            {pipelineBevelTarget, pipelineBevelModifier},
            {},
            0U,
            nullptr);
    const ri::structural::StructuralCompileIncrementalResult incrementalReuse =
        ri::structural::CompileStructuralGeometryNodesIncremental(
            {pipelineBevelTarget, pipelineBevelModifier},
            {},
            incrementalA.signature,
            &incrementalA.result);
    Expect(signatureA == incrementalA.signature && !incrementalA.reusedPrevious && incrementalReuse.reusedPrevious,
           "Structural compiler incremental path should reuse prior compile results when signatures match");

    ri::structural::StructuralNode subtractiveTarget = MakeNode("subtract_target", "Subtract Target", "box");
    ri::structural::StructuralNode subtractiveCutter = MakeNode("subtract_cutter", "Subtract Cutter", "boolean_subtractor");
    subtractiveCutter.primitiveType = "box";
    subtractiveCutter.position = {0.25f, 0.0f, 0.0f};
    subtractiveCutter.scale = {0.5f, 1.0f, 1.0f};
    subtractiveCutter.targetIds = {"subtract_target"};

    const ri::structural::StructuralGeometryCompileResult subtractiveResult =
        ri::structural::CompileStructuralGeometryNodes({subtractiveTarget, subtractiveCutter});
    Expect(std::any_of(subtractiveResult.compiledNodes.begin(), subtractiveResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id.rfind("subtract_target_fragment_", 0) == 0
                   && node.compiledMesh.hasBounds
                   && NearlyEqual(node.compiledMesh.boundsMax.x, 0.0f);
           }),
           "Structural compiler orchestration should compile subtractive cutter volumes against targeted additive structural nodes");
    Expect(std::none_of(subtractiveResult.passthroughNodes.begin(), subtractiveResult.passthroughNodes.end(), [](const ri::structural::StructuralNode& node) {
               return node.id == "subtract_target";
           }),
           "Structural compiler orchestration should replace touched subtractive targets with compiled geometry output");

    ri::structural::StructuralNode intersectTarget = MakeNode("intersect_target", "Intersect Target", "box");
    ri::structural::StructuralNode intersectCutter = MakeNode("intersect_cutter", "Intersect Cutter", "box");
    intersectCutter.opType = "intersect";
    intersectCutter.position = {0.25f, 0.0f, 0.0f};
    intersectCutter.scale = {0.5f, 1.0f, 1.0f};
    intersectCutter.targetIds = {"intersect_target"};

    const ri::structural::StructuralGeometryCompileResult intersectResult =
        ri::structural::CompileStructuralGeometryNodes({intersectTarget, intersectCutter});
    Expect(std::any_of(intersectResult.compiledNodes.begin(), intersectResult.compiledNodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id.rfind("intersect_target_fragment_", 0) == 0
                   && node.compiledMesh.hasBounds
                   && NearlyEqual(node.compiledMesh.boundsMin.x, 0.0f)
                   && NearlyEqual(node.compiledMesh.boundsMax.x, 0.5f);
           }),
           "Structural compiler orchestration should support intersect-style cutter volumes driven by authored opType");

    const ri::structural::StructuralGeometryCompileResult lowCostResult =
        ri::structural::CompileStructuralGeometryNodes(
            {subtractiveTarget, subtractiveCutter},
            ri::structural::StructuralCompileOptions{
                .enableHighCostBooleanPasses = false,
                .enableNonManifoldReconcile = false,
            });
    Expect(lowCostResult.compiledNodes.empty(),
           "Structural compiler should support low-cost mode that bypasses expensive boolean clipping passes");
    Expect(std::any_of(lowCostResult.passthroughNodes.begin(), lowCostResult.passthroughNodes.end(), [](const ri::structural::StructuralNode& node) {
               return node.id == "subtract_target";
           }),
           "Structural compiler low-cost mode should keep additive structural targets on the passthrough path");

    ri::structural::StructuralNode terrainCutout = MakeNode("terrain_cutout", "Terrain Cutout", "terrain_hole_cutout");
    terrainCutout.targetIds = {"terrain_a"};
    ri::structural::StructuralNode splineDeformer = MakeNode("spline_deformer", "Spline Deformer", "spline_mesh_deformer");
    splineDeformer.targetIds = {"rail_a"};
    ri::structural::StructuralNode scatterAlias = MakeNode("scatter_alias", "Scatter Alias", "scatter_surface_primitive");
    scatterAlias.targetIds = {"wall_a"};
    ri::structural::StructuralNode shrinkwrapModifier = MakeNode("shrinkwrap_mod", "Shrinkwrap", "shrinkwrap_modifier_primitive");
    shrinkwrapModifier.targetIds = {"prop_a"};
    ri::structural::StructuralNode filletAlias = MakeNode("fillet_alias", "Fillet Alias", "auto_fillet_boolean_primitive");
    filletAlias.targetIds = {"prop_b"};
    ri::structural::StructuralNode sdfBlendAlias = MakeNode("sdf_blend_alias", "SDF Blend Alias", "sdf_organic_blend_primitive");
    sdfBlendAlias.targetIds = {"prop_c"};
    ri::structural::StructuralNode convexSubdivision = MakeNode(
        "convex_subdivision",
        "Convex Subdivision",
        "automatic_convex_subdivision_modifier");
    convexSubdivision.targetIds = {"prop_d"};

    const ri::structural::StructuralGeometryCompileResult deferredResult =
        ri::structural::CompileStructuralGeometryNodes(
            {terrainCutout, splineDeformer, scatterAlias, shrinkwrapModifier, filletAlias, sdfBlendAlias, convexSubdivision});
    Expect(deferredResult.deferredOperations.size() == 7U,
           "Structural compiler orchestration should collect deferred target operations into native compile output");
    const auto FindDeferred = [&](const std::string& id) -> const ri::structural::StructuralDeferredTargetOperation* {
        auto found = std::find_if(deferredResult.deferredOperations.begin(), deferredResult.deferredOperations.end(), [&](const ri::structural::StructuralDeferredTargetOperation& operation) {
            return operation.node.id == id;
        });
        return found != deferredResult.deferredOperations.end() ? &(*found) : nullptr;
    };
    const ri::structural::StructuralDeferredTargetOperation* deferredTerrain = FindDeferred("terrain_cutout");
    Expect(deferredTerrain != nullptr
           && deferredTerrain->normalizedType == "terrain_hole_cutout"
           && deferredTerrain->targetIds.size() == 1U
           && deferredTerrain->targetIds[0] == "terrain_a",
           "Structural compiler orchestration should preserve terrain-hole target IDs in deferred native operation output");
    const ri::structural::StructuralDeferredTargetOperation* deferredScatter = FindDeferred("scatter_alias");
    Expect(deferredScatter != nullptr
           && deferredScatter->normalizedType == "surface_scatter_volume",
           "Structural compiler orchestration should normalize scatter-surface aliases into the surface-scatter deferred operation family");
    const ri::structural::StructuralDeferredTargetOperation* deferredFillet = FindDeferred("fillet_alias");
    Expect(deferredFillet != nullptr
           && deferredFillet->normalizedType == "fillet_boolean_modifier",
           "Structural compiler orchestration should normalize fillet-boolean aliases into deferred fillet operations");
    const ri::structural::StructuralDeferredTargetOperation* deferredSdfBlend = FindDeferred("sdf_blend_alias");
    Expect(deferredSdfBlend != nullptr
           && deferredSdfBlend->normalizedType == "sdf_blend_node",
           "Structural compiler orchestration should normalize SDF organic blend aliases into deferred SDF blend operations");
    Expect(std::none_of(deferredResult.passthroughNodes.begin(), deferredResult.passthroughNodes.end(), [](const ri::structural::StructuralNode& node) {
               return node.id == "terrain_cutout"
                   || node.id == "spline_deformer"
                   || node.id == "scatter_alias"
                   || node.id == "shrinkwrap_mod"
                   || node.id == "fillet_alias"
                   || node.id == "sdf_blend_alias"
                   || node.id == "convex_subdivision";
           }),
           "Structural compiler orchestration should keep deferred target operations out of normal passthrough structural geometry output");

    ri::structural::CompiledGeometryNode terrainMeshNode{};
    terrainMeshNode.node = MakeNode("terrain_a", "Terrain A", "terrain_quad");
    terrainMeshNode.compiledMesh.positions = {
        {-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f},
        {4.0f, 0.0f, -1.0f}, {6.0f, 0.0f, -1.0f}, {5.0f, 0.0f, 1.0f},
    };
    terrainMeshNode.compiledMesh.normals = {
        {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    };
    terrainMeshNode.compiledMesh.triangleCount = 2U;
    terrainMeshNode.compiledMesh.hasBounds = true;
    terrainMeshNode.compiledMesh.boundsMin = {-1.0f, 0.0f, -1.0f};
    terrainMeshNode.compiledMesh.boundsMax = {6.0f, 0.0f, 1.0f};

    ri::structural::StructuralNode terrainCutoutExecution = MakeNode("terrain_cut_exec", "Terrain Cut Exec", "terrain_hole_cutout");
    terrainCutoutExecution.position = {0.0f, 0.0f, 0.0f};
    terrainCutoutExecution.scale = {3.0f, 1.0f, 3.0f};
    const ri::structural::CompiledMesh cutTerrainMesh =
        ri::structural::ApplyTerrainHoleCutoutToMesh(terrainMeshNode.compiledMesh, terrainCutoutExecution);
    Expect(cutTerrainMesh.triangleCount == 1U
           && cutTerrainMesh.hasBounds
           && NearlyEqual(cutTerrainMesh.boundsMin.x, 4.0f)
           && NearlyEqual(cutTerrainMesh.boundsMax.x, 6.0f),
           "Structural deferred terrain cutout execution should remove triangles whose centroids fall inside the authored cutout volume");

    ri::structural::CompiledGeometryNode shrinkTargetA{};
    shrinkTargetA.node = MakeNode("wrap_a", "Wrap A", "box");
    shrinkTargetA.compiledMesh = ri::structural::BuildCompiledMeshFromConvexSolid(
        ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({0.0f, 0.0f, 0.0f})));
    ri::structural::CompiledGeometryNode shrinkTargetB{};
    shrinkTargetB.node = MakeNode("wrap_b", "Wrap B", "box");
    shrinkTargetB.compiledMesh = ri::structural::BuildCompiledMeshFromConvexSolid(
        ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({2.0f, 0.0f, 0.0f})));
    ri::structural::CompiledGeometryNode untouchedTarget{};
    untouchedTarget.node = MakeNode("wrap_c", "Wrap C", "box");
    untouchedTarget.compiledMesh = ri::structural::BuildCompiledMeshFromConvexSolid(
        ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({6.0f, 0.0f, 0.0f})));

    ri::structural::StructuralDeferredTargetOperation shrinkwrapOperation{};
    shrinkwrapOperation.node = MakeNode("shrink_exec", "Shrink Exec", "shrinkwrap_modifier_primitive");
    shrinkwrapOperation.normalizedType = "shrinkwrap_modifier_primitive";
    shrinkwrapOperation.targetIds = {"wrap_a", "wrap_b"};

    const ri::structural::StructuralDeferredExecutionResult deferredExecution =
        ri::structural::ExecuteStructuralDeferredTargetOperations({shrinkwrapOperation}, {shrinkTargetA, shrinkTargetB, untouchedTarget});
    Expect(deferredExecution.operationStats.size() == 1U
               && deferredExecution.operationStats.front().succeeded
               && deferredExecution.operationStats.front().targetCount == 2U,
           "Structural deferred execution should report per-operation stats for successful shrinkwrap passes");
    Expect(std::find(deferredExecution.replacedTargetIds.begin(), deferredExecution.replacedTargetIds.end(), "wrap_a")
               != deferredExecution.replacedTargetIds.end()
           && std::find(deferredExecution.replacedTargetIds.begin(), deferredExecution.replacedTargetIds.end(), "wrap_b")
               != deferredExecution.replacedTargetIds.end(),
           "Structural deferred shrinkwrap execution should mark replaced target colliders when the authored modifier requests replacement");
    Expect(std::any_of(deferredExecution.nodes.begin(), deferredExecution.nodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id == "shrink_exec"
                   && node.compiledMesh.hasBounds
                   && NearlyEqual(node.compiledMesh.boundsMin.x, -0.5f)
                   && NearlyEqual(node.compiledMesh.boundsMax.x, 2.5f);
           }),
           "Structural deferred shrinkwrap execution should generate a convex hull collider covering the targeted source geometry span");
    Expect(std::any_of(deferredExecution.nodes.begin(), deferredExecution.nodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id == "wrap_c";
           }) && std::none_of(deferredExecution.nodes.begin(), deferredExecution.nodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id == "wrap_a" || node.node.id == "wrap_b";
           }),
           "Structural deferred execution should preserve untouched target meshes while removing replaced shrinkwrap sources");

    ri::structural::StructuralDeferredTargetOperation convexSubdivisionOperation{};
    convexSubdivisionOperation.node = MakeNode(
        "convex_subdivision_exec",
        "Convex Subdivision Exec",
        "automatic_convex_subdivision_modifier");
    convexSubdivisionOperation.normalizedType = "automatic_convex_subdivision_modifier";
    convexSubdivisionOperation.targetIds = {"wrap_a", "wrap_b"};
    convexSubdivisionOperation.node.replaceChildColliders = false;
    const ri::structural::StructuralDeferredExecutionResult convexSubdivisionExecution =
        ri::structural::ExecuteStructuralDeferredTargetOperations(
            {convexSubdivisionOperation},
            {shrinkTargetA, shrinkTargetB, untouchedTarget});
    Expect(std::count_if(convexSubdivisionExecution.nodes.begin(), convexSubdivisionExecution.nodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id.rfind("convex_subdivision_exec_convex_", 0) == 0;
           }) == 2,
           "Structural deferred convex-subdivision execution should emit one convex hull per targeted source node");
    Expect(convexSubdivisionExecution.operationStats.size() == 1U
               && convexSubdivisionExecution.operationStats.front().generatedNodeCount == 2U,
           "Structural deferred execution stats should report generated-node counts for convex subdivision operations");

    ri::structural::StructuralDeferredTargetOperation filletExecutionOperation{};
    filletExecutionOperation.node = MakeNode("fillet_exec", "Fillet Exec", "auto_fillet_boolean_primitive");
    filletExecutionOperation.normalizedType = "fillet_boolean_modifier";
    filletExecutionOperation.targetIds = {"wrap_a", "wrap_b"};
    const ri::structural::StructuralDeferredExecutionResult filletExecution =
        ri::structural::ExecuteStructuralDeferredTargetOperations(
            {filletExecutionOperation},
            {shrinkTargetA, shrinkTargetB, untouchedTarget});
    Expect(std::any_of(filletExecution.nodes.begin(), filletExecution.nodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
               return node.node.id == "fillet_exec" && node.compiledMesh.hasBounds;
           }),
           "Structural deferred fillet execution should produce stable hull output across targeted authored geometry");

    ri::structural::StructuralDeferredTargetOperation unsupportedOperation{};
    unsupportedOperation.node = MakeNode("unsupported_exec", "Unsupported Exec", "unknown_modifier");
    unsupportedOperation.normalizedType = "unknown_modifier";
    unsupportedOperation.targetIds = {"wrap_a"};
    const ri::structural::StructuralDeferredExecutionResult unsupportedExecution =
        ri::structural::ExecuteStructuralDeferredTargetOperations({unsupportedOperation}, {shrinkTargetA});
    Expect(unsupportedExecution.operationStats.size() == 1U
               && unsupportedExecution.operationStats.front().skippedUnsupportedType
               && !unsupportedExecution.operationStats.front().succeeded,
           "Structural deferred execution should surface unsupported operation types in operation-level stats");

    ri::structural::StructuralGeometryCompileResult deferredPipelineCompile{};
    deferredPipelineCompile.compiledNodes = {shrinkTargetA, shrinkTargetB};
    deferredPipelineCompile.deferredOperations = {shrinkwrapOperation, unsupportedOperation};
    const ri::structural::StructuralDeferredPipelineResult deferredPipelineResult =
        ri::structural::ExecuteStructuralDeferredPipeline(deferredPipelineCompile);
    Expect(!deferredPipelineResult.nodes.empty()
               && deferredPipelineResult.deferredExecution.operationStats.size() == 2U
               && std::find(
                      deferredPipelineResult.unsupportedOperationIds.begin(),
                      deferredPipelineResult.unsupportedOperationIds.end(),
                      "unsupported_exec") != deferredPipelineResult.unsupportedOperationIds.end(),
           "Structural deferred pipeline execution should aggregate unsupported operation IDs for runtime/editor diagnostics");

    ri::structural::StructuralNode compileAndRunTarget = MakeNode("compile_run_target", "Compile+Run Target", "box");
    ri::structural::StructuralNode compileAndRunModifier =
        MakeNode("compile_run_mod", "Compile+Run Modifier", "automatic_convex_subdivision_modifier");
    compileAndRunModifier.targetIds = {"compile_run_target"};
    const ri::structural::StructuralDeferredPipelineResult compileAndRunResult =
        ri::structural::CompileAndExecuteStructuralDeferredPipeline({compileAndRunTarget, compileAndRunModifier});
    Expect(!compileAndRunResult.compileResult.deferredOperations.empty()
               && compileAndRunResult.deferredExecution.operationStats.size() == 1U
               && compileAndRunResult.deferredExecution.operationStats.front().succeeded
               && std::any_of(compileAndRunResult.nodes.begin(), compileAndRunResult.nodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
                      return node.node.id.rfind("compile_run_mod_convex_", 0) == 0;
                  }),
           "Structural deferred compile+execute pipeline should compile authored deferred modifiers and emit executable geometry in one pass");
    const std::string deferredReport = ri::structural::BuildStructuralDeferredPipelineReport(compileAndRunResult);
    Expect(deferredReport.find("Structural deferred pipeline report") != std::string::npos
               && deferredReport.find("operations:") != std::string::npos
               && deferredReport.find("compile_run_mod") != std::string::npos,
           "Structural deferred pipeline report formatting should include summary and per-operation lines");
}

void TestEventEngine() {
    ri::events::EventDefinition normalizedInput{};
    normalizedInput.id = "";
    normalizedInput.hook = "trigger_enter";
    normalizedInput.cooldownMs = 999999999ULL;
    normalizedInput.maxRuns = 0;
    const ri::events::EventDefinition normalized = ri::events::NormalizeEventDefinition(normalizedInput, 3, "facility");
    Expect(normalized.cooldownMs == 86400000ULL, "Event normalization should clamp cooldown to one day");
    Expect(!normalized.maxRuns.has_value(), "Event normalization should drop non-positive maxRuns");
    Expect(normalized.resolvedId == "facility:event:3", "Event normalization should synthesize stable runtime IDs");

    ri::events::EventEngine engine("facility");
    engine.SetTargetGroups({
        {"lights", {"light-a", "light-b"}},
        {"doors", {"light-b", "door-c"}},
    });
    engine.SetActionGroups({
        {"intro", {MakeMessageAction("intro")}},
    });
    engine.SetSequences({
        {"alarm", {
            MakeSequenceStep(100.0, {MakeMessageAction("seq-1")}),
            MakeSequenceStep(50.0, {MakeMessageAction("seq-2")}),
        }},
    });

    ri::events::EventDefinition eventA{};
    eventA.id = "event-a";
    eventA.hook = "panel_use";
    eventA.sourceId = "panel-1";
    ri::events::EventAction setFlag = MakeAction("set_flag");
    setFlag.flag = "power_on";
    ri::events::EventAction addValue = MakeAction("add_value");
    addValue.key = "progress";
    addValue.amount = 2.0;
    eventA.actions = {setFlag, addValue};

    ri::events::EventDefinition eventB{};
    eventB.id = "event-b";
    eventB.hook = "panel_use";
    eventB.sourceId = "panel-1";
    eventB.consumeInteraction = true;
    eventB.conditions.requiredFlags = {"power_on"};
    eventB.actions = {MakeMessageAction("power restored")};

    ri::events::EventDefinition eventC{};
    eventC.id = "event-c";
    eventC.hook = "panel_use";
    eventC.sourceId = "panel-1";
    eventC.once = false;
    eventC.cooldownMs = 1000;
    eventC.maxRuns = 2;
    eventC.actions = {MakeMessageAction("repeatable")};

    engine.SetEvents({eventA, eventB, eventC});

    std::vector<std::string> executedActions;
    const ri::events::ActionExecutor executor = [&](const ri::events::EventAction& action,
                                                    const ri::events::EventContext&,
                                                    ri::events::EventEngine&) {
        if (action.type == "message") {
            executedActions.push_back(action.text);
        } else {
            executedActions.push_back(action.type);
        }
    };

    const bool consumed = engine.RunHook("panel_use", MakeContext("panel-1"), executor, 100.0);
    Expect(consumed, "Event engine should report consumed interactions when matching events request it");
    Expect(engine.HasWorldFlag("power_on"), "Event engine should execute built-in flag actions");
    Expect(NearlyEqual(static_cast<float>(engine.GetWorldValue("progress")), 2.0f),
           "Event engine should execute built-in numeric value actions");
    Expect(executedActions.size() == 2U, "Event engine should execute custom actions after built-in state changes");
    Expect(executedActions[0] == "power restored",
           "Event engine should let later events in the same hook observe world-state changes from earlier events");
    Expect(executedActions[1] == "repeatable", "Event engine should execute repeatable events on the first matching hook");

    engine.RunHook("panel_use", MakeContext("panel-1"), executor, 150.0);
    Expect(executedActions.size() == 2U, "Event engine should respect cooldown windows");
    engine.RunHook("panel_use", MakeContext("panel-1"), executor, 1205.0);
    Expect(executedActions.size() == 3U && executedActions.back() == "repeatable",
           "Event engine should re-fire repeatable events after cooldown");
    engine.RunHook("panel_use", MakeContext("panel-1"), executor, 2500.0);
    Expect(executedActions.size() == 3U, "Event engine should respect maxRuns for repeatable events");

    ri::events::EventAction targetAction{};
    targetAction.target = "door-d";
    targetAction.targets = {"door-c", "door-d"};
    targetAction.targetGroups = {"lights", "doors"};
    const std::vector<std::string> targets = engine.GetActionTargets(targetAction);
    Expect(targets.size() == 4U, "Event engine should collapse target groups and direct targets into a deduplicated list");

    engine.SetWorldValue("float_value", 0.1 + 0.2);
    ri::events::EventConditions floatEqualConditions{};
    floatEqualConditions.valuesEqual["float_value"] = 0.3;
    Expect(engine.EvaluateConditions(floatEqualConditions),
           "Event engine equality conditions should tolerate floating-point precision error");

    ri::events::EventAction runGroup = MakeAction("run_group");
    runGroup.group = "intro";
    ri::events::EventAction delayedAction = MakeAction("delay");
    delayedAction.timerId = "delayed-note";
    delayedAction.delayMs = 100.0;
    delayedAction.actions = {MakeMessageAction("delayed")};
    ri::events::EventAction runSequence = MakeAction("run_sequence");
    runSequence.sequence = "alarm";
    engine.RunActions({runGroup, delayedAction, runSequence}, {}, executor, 0.0);

    Expect(engine.ScheduledTimerCount() == 3U, "Event engine should schedule delayed actions and sequence steps");
    Expect(executedActions.back() == "intro", "Action groups should execute immediately");

    engine.Tick(99.0, executor);
    Expect(executedActions.back() == "intro", "Timers should not fire before their trigger time");
    engine.Tick(100.0, executor);
    Expect(executedActions[4] == "delayed", "Named delayed actions should fire when their trigger time is reached");
    Expect(executedActions[5] == "seq-1", "Sequences should execute the first timed step");

    ri::events::EventAction cancelSequence = MakeAction("cancel_sequence");
    cancelSequence.sequence = "alarm";
    engine.RunActions({cancelSequence}, {}, executor, 110.0);
    engine.Tick(200.0, executor);
    Expect(std::find(executedActions.begin(), executedActions.end(), "seq-2") == executedActions.end(),
           "Canceling a sequence should prevent later scheduled steps from firing");

    ri::events::EventAction cancelDelay = MakeAction("delay");
    cancelDelay.timerId = "cancel-me";
    cancelDelay.delayMs = 50.0;
    cancelDelay.actions = {MakeMessageAction("should-not-run")};
    ri::events::EventAction cancelTimer = MakeAction("cancel_timer");
    cancelTimer.timerId = "cancel-me";
    engine.RunActions({cancelDelay, cancelTimer}, {}, executor, 300.0);
    engine.Tick(400.0, executor);
    Expect(std::find(executedActions.begin(), executedActions.end(), "should-not-run") == executedActions.end(),
           "Canceling named timers should suppress their delayed actions");

    ri::events::EventAction paddedDelay = MakeAction("delay");
    paddedDelay.timerId = "  padded-id  ";
    paddedDelay.delayMs = 20.0;
    paddedDelay.actions = {MakeMessageAction("trimmed-id-fired")};
    ri::events::EventAction paddedCancel = MakeAction("cancel_timer");
    paddedCancel.timerId = "padded-id";
    engine.RunActions({paddedDelay, paddedCancel}, {}, executor, 450.0);
    engine.Tick(500.0, executor);
    Expect(std::find(executedActions.begin(), executedActions.end(), "trimmed-id-fired") == executedActions.end(),
           "Named timer cancellation should normalize whitespace-only timer-id differences");

    ri::events::EventAction nonFiniteDelay = MakeAction("delay");
    nonFiniteDelay.timerId = "non-finite-delay";
    nonFiniteDelay.delayMs = std::numeric_limits<double>::quiet_NaN();
    nonFiniteDelay.actions = {MakeMessageAction("non-finite-delay-fired")};
    engine.RunActions({nonFiniteDelay}, {}, executor, 600.0);
    engine.Tick(600.0, executor);
    Expect(std::find(executedActions.begin(), executedActions.end(), "non-finite-delay-fired") != executedActions.end(),
           "Named timer scheduling should sanitize non-finite trigger times deterministically");

    ri::events::EventAction ifAction = MakeAction("if");
    ifAction.conditions = MakeRequiredFlagCondition({"power_on"});
    ifAction.thenActions = {MakeMessageAction("branch-then")};
    ifAction.elseActions = {MakeMessageAction("branch-else")};
    engine.RunActions({ifAction}, {}, executor, 500.0);
    Expect(executedActions.back() == "branch-then", "Event engine should evaluate conditional action branches against live world state");

    const ri::events::EventCheckpointState captured = engine.CaptureCheckpointState();
    Expect(captured.worldFlags.contains("power_on")
           && captured.worldValues.contains("progress")
           && captured.completedEventIds.contains("event-a"),
           "Event engine checkpoints should capture flags, values, and completed event IDs");
    Expect(captured.eventRuntimeStates.contains("event-c")
           && captured.eventRuntimeStates.at("event-c").runCount == 2U,
           "Event engine checkpoints should capture per-event runtime counters for repeatable events");

    engine.ResetWorldState();
    Expect(!engine.HasWorldFlag("power_on")
           && NearlyEqual(static_cast<float>(engine.GetWorldValue("progress")), 0.0f)
           && engine.GetCompletedEventIds().empty(),
           "Event engine world-state reset should clear flags, values, and completed-event markers");

    ri::events::EventCheckpointState restore{};
    restore.worldFlags = {"power_on", "checkpoint_active"};
    restore.worldValues = {{"progress", 5.0}, {"danger", 2.0}};
    restore.completedEventIds = {"event-a", "event-b"};
    engine.RestoreCheckpointState(restore);
    Expect(engine.HasWorldFlag("power_on")
           && engine.HasWorldFlag("checkpoint_active")
           && NearlyEqual(static_cast<float>(engine.GetWorldValue("progress")), 5.0f)
           && engine.GetCompletedEventIds().contains("event-b"),
           "Event engine should restore checkpoint world state and completed-event tracking");

    ri::events::EventCheckpointState playableRestore{};
    playableRestore.worldFlags = {"power_on", "checkpoint_active"};
    playableRestore.worldValues = {{"progress", 5.0}, {"danger", 2.0}};
    playableRestore.completedEventIds = {"event-a"};
    playableRestore.eventRuntimeStates["event-c"] = captured.eventRuntimeStates.at("event-c");
    engine.RestoreCheckpointState(playableRestore);

    executedActions.clear();
    const bool consumedAfterRestore = engine.RunHook("panel_use", MakeContext("panel-1"), executor, 5000.0);
    Expect(consumedAfterRestore,
           "Restored checkpoint state should preserve event-engine behavior that depends on restored world state");
    Expect(std::find(executedActions.begin(), executedActions.end(), "power restored") != executedActions.end(),
           "Restored world flags should satisfy event conditions immediately after checkpoint restore");
    Expect(std::find(executedActions.begin(), executedActions.end(), "repeatable") == executedActions.end(),
           "Restored checkpoints should preserve repeatable event maxRuns state");

    ri::events::EventAction staleDelay = MakeAction("delay");
    staleDelay.delayMs = 100.0;
    staleDelay.actions = {MakeMessageAction("stale-delay")};
    engine.RunActions({staleDelay}, {}, executor, 6000.0);
    Expect(engine.ScheduledTimerCount() == 1U, "Event engine should schedule delayed timers before event replacement");

    engine.SetEvents({eventA});
    Expect(engine.ScheduledTimerCount() == 0U, "Event engine should clear pending timers when replacing event definitions");
    executedActions.clear();
    engine.Tick(7000.0, executor);
    Expect(std::find(executedActions.begin(), executedActions.end(), "stale-delay") == executedActions.end(),
           "Event replacement should prevent stale delayed actions from firing");

    const ri::world::PostProcessState disabledState = ri::world::SetPostProcessingEnabled(ri::world::PostProcessState{}, false);
    const ri::render::PostProcessParameters disabledParameters =
        ri::world::BuildPostProcessParameters(disabledState, 10.0, 0.25f);
    Expect(NearlyEqual(disabledParameters.noiseAmount, 0.0f) && NearlyEqual(disabledParameters.staticFadeAmount, 0.25f),
           "Post-process toggle helper should preserve raw render static fade while disabling effects");
    const ri::world::PostProcessState gameplayState = ri::world::ApplyPostProcessPhase(
        ri::world::SetPostProcessingEnabled(ri::world::PostProcessState{}, true),
        "gameplay");
    const ri::render::PostProcessParameters gameplayParameters =
        ri::world::BuildPostProcessParameters(gameplayState, 10.0, 0.0f);
    Expect(gameplayParameters.noiseAmount >= 0.001f,
           "Post-process phase helper should apply deterministic gameplay baseline shaping");
}

void TestSpatialIndex() {
    using ri::spatial::Aabb;
    using ri::spatial::BspSpatialIndex;
    using ri::spatial::SpatialEntry;

    const std::vector<SpatialEntry> entries = {
        SpatialEntry{.id = "wall-a", .bounds = Aabb{.min = {-4.0f, 0.0f, -1.0f}, .max = {-2.0f, 2.0f, 1.0f}}},
        SpatialEntry{.id = "wall-b", .bounds = Aabb{.min = {-1.0f, 0.0f, -1.0f}, .max = {1.0f, 2.0f, 1.0f}}},
        SpatialEntry{.id = "wall-c", .bounds = Aabb{.min = {2.0f, 0.0f, -1.0f}, .max = {4.0f, 2.0f, 1.0f}}},
        SpatialEntry{.id = "tower", .bounds = Aabb{.min = {-0.5f, 0.0f, 3.0f}, .max = {0.5f, 6.0f, 5.0f}}},
        SpatialEntry{.id = "skip-empty", .bounds = ri::spatial::MakeEmptyAabb()},
        SpatialEntry{.id = "", .bounds = Aabb{.min = {8.0f, 0.0f, 8.0f}, .max = {9.0f, 1.0f, 9.0f}}},
    };

    BspSpatialIndex index(entries, {.maxLeafSize = 1, .maxDepth = 8});
    Expect(!index.Empty(), "Spatial index should build when valid entries exist");
    Expect(index.EntryCount() == 4U, "Spatial index should skip empty and unnamed entries");
    ExpectVec3(ri::spatial::Center(index.Bounds()), {0.0f, 3.0f, 2.0f}, "Spatial index bounds center");

    const std::vector<std::string> middleHits = index.QueryBox(Aabb{.min = {-1.25f, -1.0f, -1.5f}, .max = {1.25f, 3.0f, 1.5f}});
    Expect(middleHits.size() == 1U && middleHits[0] == "wall-b", "Spatial index box query should find overlapping central entries");

    const std::vector<std::string> wideHits = index.QueryBox(Aabb{.min = {-10.0f, -1.0f, -2.0f}, .max = {10.0f, 10.0f, 2.0f}});
    Expect(wideHits.size() == 3U, "Spatial index box query should gather entries across both BSP branches");

    const std::vector<std::string> missHits = index.QueryBox(ri::spatial::MakeEmptyAabb());
    Expect(missHits.empty(), "Spatial index box query should reject empty bounds");

    const std::vector<std::string> forwardRayHits = index.QueryRay({-10.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 20.0f);
    Expect(forwardRayHits.size() == 3U, "Spatial index ray query should report all boxes intersected by a long ray");

    const std::vector<std::string> shortRayHits = index.QueryRay({-10.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 6.5f);
    Expect(shortRayHits.size() == 1U && shortRayHits[0] == "wall-a", "Spatial index ray query should respect far distance limits");

    const std::vector<std::string> verticalRayHits = index.QueryRay({0.0f, 10.0f, 4.0f}, {0.0f, -1.0f, 0.0f}, 20.0f);
    Expect(verticalRayHits.size() == 1U && verticalRayHits[0] == "tower", "Spatial index ray query should hit tall vertical volumes");

    const std::vector<std::string> invalidRayHits = index.QueryRay({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 10.0f);
    Expect(invalidRayHits.empty(), "Spatial index ray query should reject zero-length directions");

    BspSpatialIndex rebuilt;
    rebuilt.Rebuild({
        SpatialEntry{.id = "single", .bounds = Aabb{.min = {1.0f, 1.0f, 1.0f}, .max = {2.0f, 2.0f, 2.0f}}},
    });
    Expect(rebuilt.EntryCount() == 1U, "Spatial index should rebuild from fresh entry sets");
    const std::vector<std::string> rebuildHits = rebuilt.QueryRay({0.0f, 1.5f, 1.5f}, {1.0f, 0.0f, 0.0f}, 5.0f);
    Expect(rebuildHits.size() == 1U && rebuildHits[0] == "single", "Spatial index should query correctly after rebuild");
}

void TestTraceScene() {
    using ri::spatial::Aabb;
    using ri::trace::TraceCollider;
    using ri::trace::TraceScene;

    TraceScene scene({
        TraceCollider{.id = "floor", .bounds = Aabb{.min = {-10.0f, -1.0f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}}, .structural = true, .dynamic = false},
        TraceCollider{.id = "wall", .bounds = Aabb{.min = {2.0f, 0.0f, -1.0f}, .max = {3.0f, 3.0f, 1.0f}}, .structural = true, .dynamic = false},
        TraceCollider{.id = "crate", .bounds = Aabb{.min = {5.0f, 0.0f, -1.0f}, .max = {6.0f, 1.0f, 1.0f}}, .structural = false, .dynamic = true},
    });

    Expect(scene.ColliderCount() == 3U, "Trace scene should keep valid colliders");

    const std::vector<std::string> crateCandidates = scene.QueryCollidablesForBox(
        Aabb{.min = {4.8f, 0.2f, -0.2f}, .max = {5.2f, 0.8f, 0.2f}});
    Expect(crateCandidates.size() == 1U && crateCandidates[0] == "crate",
           "Trace scene box candidate query should include dynamic colliders");
    Expect(scene.QueryCollidablesForBox(Aabb{.min = {4.8f, 0.2f, -0.2f}, .max = {5.2f, 0.8f, 0.2f}}, true).empty(),
           "Structural-only box candidate queries should ignore non-structural dynamic colliders");
    const std::vector<std::string> deterministicIds = ri::trace::QueryDeterministicBoxIds(
        scene,
        Aabb{.min = {-10.0f, -2.0f, -2.0f}, .max = {10.0f, 4.0f, 2.0f}},
        ri::trace::SpatialQueryFilter{
            .structuralOnly = false,
            .idPrefix = "c",
        });
    Expect(deterministicIds.size() == 1U && deterministicIds[0] == "crate",
           "Spatial query service should support deterministic filtered broad+narrow box query IDs");

    const auto overlapHit = scene.TraceBox(
        Aabb{.min = {1.5f, 1.0f, -0.5f}, .max = {2.5f, 2.0f, 0.5f}});
    Expect(overlapHit.has_value(), "TraceBox should return an overlap hit for intersecting colliders");
    Expect(overlapHit->id == "wall", "TraceBox should report the wall as the blocking collider");
    ExpectVec3(overlapHit->normal, {-1.0f, 0.0f, 0.0f}, "TraceBox hit normal");
    Expect(NearlyEqual(overlapHit->penetration, 0.5f), "TraceBox should compute penetration depth");

    const auto rayHit = scene.TraceRay({0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 10.0f);
    Expect(rayHit.has_value(), "TraceRay should return the nearest hit");
    Expect(rayHit->id == "wall", "TraceRay should resolve the nearest structural hit before farther colliders");
    ExpectVec3(rayHit->point, {2.0f, 1.0f, 0.0f}, "TraceRay hit point");
    ExpectVec3(rayHit->normal, {-1.0f, 0.0f, 0.0f}, "TraceRay hit normal");

    ri::trace::TraceOptions structuralOnlyOptions{};
    structuralOnlyOptions.structuralOnly = true;
    Expect(!scene.TraceRay({4.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, 2.0f, structuralOnlyOptions).has_value(),
           "Structural-only TraceRay should ignore non-structural dynamic colliders");
    const auto crateRayHit = scene.TraceRay({4.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, 2.0f);
    Expect(crateRayHit.has_value() && crateRayHit->id == "crate",
           "TraceRay should include dynamic colliders when not restricted to structural-only queries");

    const auto sweptHit = scene.TraceSweptBox(
        Aabb{.min = {-0.5f, 0.5f, -0.5f}, .max = {0.5f, 1.5f, 0.5f}},
        {3.0f, 0.0f, 0.0f});
    Expect(sweptHit.has_value(), "TraceSweptBox should detect collisions along the swept path");
    Expect(sweptHit->id == "wall", "TraceSweptBox should report the wall as the earliest blocking collider");
    Expect(NearlyEqual(sweptHit->time, 0.5f), "TraceSweptBox should compute normalized hit time");
    ExpectVec3(sweptHit->normal, {-1.0f, 0.0f, 0.0f}, "TraceSweptBox hit normal");

    const ri::trace::SlideMoveResult slide = scene.SlideMoveBox(
        Aabb{.min = {-0.5f, 0.5f, -0.5f}, .max = {0.5f, 1.5f, 0.5f}},
        {3.0f, 1.0f, 0.0f});
    Expect(slide.blocked, "SlideMoveBox should report blocked movement when a wall is encountered");
    Expect(slide.hits.size() == 1U && slide.hits[0].id == "wall", "SlideMoveBox should retain hit history");
    Expect(std::fabs(slide.remainingDelta.x) <= 0.01f, "SlideMoveBox should not leave motion pushing into the hit surface");
    Expect(slide.positionDelta.y > 0.9f, "SlideMoveBox should preserve motion parallel to the hit surface in the resolved movement");

    ri::trace::GroundTraceOptions groundOptions{};
    groundOptions.maxDistance = 5.0f;
    const auto groundHit = scene.FindGroundHit({0.0f, 2.0f, 0.0f}, groundOptions);
    Expect(groundHit.has_value(), "FindGroundHit should detect walkable floor below the origin");
    Expect(groundHit->id == "floor", "FindGroundHit should resolve the floor collider");
    ExpectVec3(groundHit->normal, {0.0f, 1.0f, 0.0f}, "FindGroundHit normal");

    Expect(!scene.TraceBox(Aabb{.min = {1.5f, 1.0f, -0.5f}, .max = {2.5f, 2.0f, 0.5f}}, {.ignoreId = "wall"}).has_value(),
           "TraceBox ignoreId should suppress hits against the ignored collider");

    TraceScene duplicateIdScene({
        TraceCollider{.id = "floor", .bounds = Aabb{.min = {-10.0f, -1.0f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}}, .structural = true, .dynamic = false},
        TraceCollider{.id = "wall_dup", .bounds = Aabb{.min = {2.0f, 0.0f, -1.0f}, .max = {3.0f, 3.0f, 1.0f}}, .structural = true, .dynamic = false},
        TraceCollider{.id = "wall_dup", .bounds = Aabb{.min = {5.0f, 0.0f, -1.0f}, .max = {6.0f, 3.0f, 1.0f}}, .structural = true, .dynamic = false},
    });
    Expect(duplicateIdScene.ColliderCount() == 2U,
           "Trace scene should reject duplicate collider IDs during collider ingestion");
    const auto duplicateIdHit = duplicateIdScene.TraceRay({0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 10.0f);
    Expect(duplicateIdHit.has_value() && duplicateIdHit->id == "wall_dup",
           "Trace scene with duplicate IDs should keep one stable collider target");
    Expect(duplicateIdHit.has_value() && NearlyEqual(duplicateIdHit->point.x, 2.0f),
           "Trace scene duplicate-ID handling should preserve first-ingested collider geometry");
}

void TestRuntimeClipQuerySweepAndLevelSchedulers() {
    using ri::spatial::Aabb;
    using ri::trace::TraceCollider;
    using ri::trace::TraceScene;

    ri::world::RuntimeEnvironmentService env;
    ri::world::ClippingRuntimeVolume clip{};
    clip.id = "clip_a";
    clip.position = {0.0f, 0.0f, 0.0f};
    clip.size = {2.0f, 2.0f, 2.0f};
    clip.modes = {"visibility"};
    clip.enabled = true;

    ri::world::FilteredCollisionRuntimeVolume filt{};
    filt.id = "filt_b";
    filt.position = {5.0f, 0.0f, 0.0f};
    filt.size = {2.0f, 2.0f, 2.0f};
    filt.channels = {"player"};

    env.SetClippingVolumes({clip});
    env.SetFilteredCollisionVolumes({filt});

    ri::world::RuntimeClipQueryBroadphase broadphase(env);
    ri::world::RuntimeClipBoundsCache cache;

    const std::vector<std::string> boxHits = broadphase.QueryBox(
        Aabb{.min = {-0.5f, -0.5f, -0.5f}, .max = {0.5f, 0.5f, 0.5f}}, {}, &cache);
    Expect(boxHits.size() == 1U && boxHits[0] == "clip_a",
           "Runtime clip box broadphase should return sorted overlapping clip volumes");

    ri::world::RuntimeClipBoxQueryOptions physicsOnly{};
    physicsOnly.clipModeMask = ri::world::ClipVolumeModeBit(ri::world::ClipVolumeMode::Physics);
    const std::vector<std::string> noPhysicsMatch = broadphase.QueryBox(
        Aabb{.min = {-0.5f, -0.5f, -0.5f}, .max = {0.5f, 0.5f, 0.5f}}, physicsOnly, &cache);
    Expect(noPhysicsMatch.empty(),
           "Clip mode mask should exclude volumes that only carry other clip modes");

    const std::vector<std::string> rayIds = broadphase.QueryRaySweptBounds(
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 10.0f, {.sweptThickness = 0.05f}, &cache);
    Expect(rayIds.size() >= 2U,
           "Swept-ray AABB should gather clip and collision candidates along the segment");
    Expect(std::find(rayIds.begin(), rayIds.end(), "clip_a") != rayIds.end(),
           "Swept-ray broadphase should include clipping volumes near the ray origin");
    Expect(std::find(rayIds.begin(), rayIds.end(), "filt_b") != rayIds.end(),
           "Swept-ray broadphase should include filtered collision volumes along the segment");

    TraceScene traceScene({
        TraceCollider{.id = "obs", .bounds = Aabb{.min = {2.0f, 0.0f, -1.0f}, .max = {3.0f, 2.0f, 1.0f}}, .structural = true},
    });
    const ri::trace::SweptVolumeContactSolver solver(traceScene);
    const std::optional<ri::trace::TraceHit> swept = solver.ResolveFirstContact(
        Aabb{.min = {-0.5f, 0.5f, -0.5f}, .max = {0.5f, 1.5f, 0.5f}}, {4.0f, 0.0f, 0.0f});
    Expect(swept.has_value() && swept->id == "obs",
           "SweptVolumeContactSolver should resolve earliest swept contact via TraceScene");

    const std::optional<ri::trace::TraceHit> staticHit = ri::trace::SweptVolumeContactSolver::ResolveVsStaticAabb(
        Aabb{.min = {0.0f, 0.0f, 0.0f}, .max = {1.0f, 1.0f, 1.0f}},
        {2.0f, 0.0f, 0.0f},
        Aabb{.min = {2.5f, -1.0f, -1.0f}, .max = {4.0f, 2.0f, 2.0f}},
        "wall");
    Expect(staticHit.has_value() && NearlyEqual(staticHit->time, 0.75f),
           "Static swept AABB narrow-phase should report normalized impact time");

    ri::runtime::LevelScopedTimeoutScheduler timeouts;
    int once = 0;
    (void)timeouts.ScheduleAfter(0.5, [&] { ++once; }, 0.0);
    timeouts.Tick(0.6);
    Expect(once == 1, "Level-scoped timeout scheduler should invoke one-shot callbacks");
    timeouts.Clear();

    ri::runtime::LevelScopedIntervalScheduler intervals;
    int rep = 0;
    (void)intervals.ScheduleEvery(1.0, [&] { ++rep; }, 0.0);
    intervals.Tick(1.0, 1.0);
    Expect(rep == 1, "Level-scoped interval scheduler should invoke repeating callbacks");
}

void TestKinematicPhysics() {
    using ri::spatial::Aabb;
    using ri::trace::KinematicBodyState;
    using ri::trace::KinematicConstraintState;
    using ri::trace::KinematicPhysicsOptions;
    using ri::trace::KinematicVolumeModifiers;
    using ri::trace::TraceCollider;
    using ri::trace::TraceScene;

    TraceScene scene({
        TraceCollider{.id = "floor", .bounds = Aabb{.min = {-10.0f, -1.0f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}}, .structural = true, .dynamic = false},
        TraceCollider{.id = "wall", .bounds = Aabb{.min = {1.0f, 0.0f, -1.0f}, .max = {2.0f, 2.0f, 1.0f}}, .structural = true, .dynamic = false},
    });

    KinematicBodyState body{};
    body.bounds = Aabb{.min = {-0.25f, 0.4f, -0.25f}, .max = {0.25f, 1.4f, 0.25f}};
    body.velocity = ri::math::Vec3{3.8f, 0.0f, 0.0f};
    const ri::trace::KinematicStepResult hitStep = ri::trace::SimulateKinematicBodyStep(
        scene,
        body,
        0.2f,
        KinematicPhysicsOptions{.ignoreColliderId = "player"},
        KinematicVolumeModifiers{},
        KinematicConstraintState{});
    Expect(!hitStep.hits.empty(), "Kinematic simulation should report hits when sweeping into structural colliders");
    Expect(hitStep.state.bounds.max.x <= 1.01f, "Kinematic simulation should stop at the wall rather than tunneling");

    KinematicBodyState falling{};
    falling.bounds = Aabb{.min = {-0.25f, 0.02f, -0.25f}, .max = {0.25f, 1.02f, 0.25f}};
    falling.velocity = ri::math::Vec3{0.0f, -3.0f, 0.0f};
    const ri::trace::KinematicStepResult grounded = ri::trace::SimulateKinematicBodyStep(scene, falling, 0.1f);
    Expect(grounded.onGround, "Kinematic simulation should detect and settle against walkable ground");
    Expect(std::fabs(grounded.state.velocity.y) <= 0.05f, "Ground settling should zero-out downward velocity");

    KinematicBodyState constrained{};
    constrained.bounds = Aabb{.min = {-0.25f, 1.0f, -0.25f}, .max = {0.25f, 2.0f, 0.25f}};
    constrained.velocity = ri::math::Vec3{1.0f, 0.0f, 0.0f};
    const ri::trace::KinematicStepResult locked = ri::trace::SimulateKinematicBodyStep(
        scene,
        constrained,
        0.1f,
        KinematicPhysicsOptions{},
        KinematicVolumeModifiers{},
        KinematicConstraintState{.lockX = true});
    Expect(NearlyEqual(ri::spatial::Center(locked.state.bounds).x, ri::spatial::Center(constrained.bounds).x),
           "Axis locking should preserve constrained center axes");
    Expect(std::fabs(locked.state.velocity.x) <= 0.0001f, "Axis locking should clear constrained velocity components");

    {
        KinematicBodyState slide{};
        slide.bounds = Aabb{.min = {-0.25f, 0.4f, -0.25f}, .max = {0.25f, 1.4f, 0.25f}};
        slide.velocity = ri::math::Vec3{3.8f, 0.0f, 0.0f};
        const ri::trace::KinematicPhysicsOptions cool{
            .impactNotifyCooldownSeconds = 0.5f,
            .ignoreColliderId = "player",
        };
        const ri::trace::KinematicStepResult firstHit =
            ri::trace::SimulateKinematicBodyStep(scene, slide, 0.2f, cool, {}, {});
        Expect(firstHit.impact.has_value(),
               "Kinematic impact notify cooldown should still allow the first impact event");
        const ri::trace::KinematicStepResult secondHit =
            ri::trace::SimulateKinematicBodyStep(scene, firstHit.state, 0.05f, cool, {}, {});
        Expect(!secondHit.impact.has_value(),
               "Impact notify cooldown should throttle repeat notifications like the prototype physics object");
    }

    {
        TraceScene tallWallScene({
            TraceCollider{.id = "floor", .bounds = Aabb{.min = {-10.0f, -1.0f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}}, .structural = true, .dynamic = false},
            TraceCollider{.id = "tall_wall", .bounds = Aabb{.min = {0.6f, 0.0f, -1.0f}, .max = {0.9f, 2.0f, 1.0f}}, .structural = true, .dynamic = false},
        });
        KinematicBodyState blocked{};
        blocked.bounds = Aabb{.min = {-0.25f, 0.02f, -0.25f}, .max = {0.25f, 1.02f, 0.25f}};
        blocked.velocity = ri::math::Vec3{4.0f, 0.0f, 0.0f};
        const ri::trace::KinematicStepResult stepAttempt = ri::trace::SimulateKinematicBodyStep(
            tallWallScene,
            blocked,
            0.2f,
            KinematicPhysicsOptions{.maxStepUpHeight = 1.0f, .ignoreColliderId = "player"},
            KinematicVolumeModifiers{},
            KinematicConstraintState{});
        Expect(stepAttempt.state.bounds.max.x <= 0.62f,
               "Step-up should not bypass a tall wall without a valid landing surface");
        Expect(stepAttempt.state.bounds.min.y <= 0.08f,
               "Rejected step-up should keep the body near its grounded height instead of hovering upward");
    }
}

void TestOrientedKinematicPhysics() {
    using ri::spatial::Aabb;
    using ri::trace::KinematicPhysicsOptions;
    using ri::trace::OrientedKinematicBodyState;
    using ri::trace::TraceCollider;
    using ri::trace::TraceScene;

    const ri::spatial::Aabb axisBounds = ri::trace::ComputeOrientedBoxWorldBounds(
        {0.0f, 1.0f, 0.0f}, {0.25f, 0.5f, 0.15f}, {0.0f, 0.0f, 0.0f});
    const ri::spatial::Aabb yawBounds = ri::trace::ComputeOrientedBoxWorldBounds(
        {0.0f, 1.0f, 0.0f}, {0.25f, 0.5f, 0.15f}, {0.0f, 45.0f, 0.0f});
    const ri::math::Vec3 axisSize = ri::spatial::Size(axisBounds);
    const ri::math::Vec3 yawSize = ri::spatial::Size(yawBounds);
    Expect(yawSize.x > axisSize.x + 0.02f && yawSize.z > axisSize.z + 0.02f,
           "Yaw should widen world AABB extents for an asymmetric horizontal footprint");

    TraceScene scene({
        TraceCollider{.id = "floor", .bounds = Aabb{.min = {-10.0f, -1.0f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}}, .structural = true, .dynamic = false},
        TraceCollider{.id = "wall", .bounds = Aabb{.min = {1.0f, 0.0f, -1.0f}, .max = {2.0f, 2.0f, 1.0f}}, .structural = true, .dynamic = false},
    });

    OrientedKinematicBodyState slide{};
    slide.center = {0.0f, 0.75f, 0.0f};
    slide.halfExtents = {0.25f, 0.25f, 0.25f};
    slide.orientationDegrees = {0.0f, 35.0f, 0.0f};
    slide.velocity = {4.0f, 0.0f, 0.0f};
    const auto wallStop = ri::trace::SimulateOrientedKinematicBodyStep(
        scene,
        slide,
        0.18f,
        KinematicPhysicsOptions{.ignoreColliderId = "player"});
    Expect(!wallStop.hits.empty(), "Oriented kinematic sweeps should record structural hits");
    Expect(wallStop.worldBounds.max.x <= 1.02f,
           "Oriented kinematic simulation should respect slide stops at structural solids");

    constexpr float kPi = 3.14159265f;
    OrientedKinematicBodyState spin{};
    spin.center = {0.0f, 4.0f, 0.0f};
    spin.halfExtents = {0.2f, 0.2f, 0.2f};
    spin.angularVelocity = {0.0f, kPi * 0.5f, 0.0f};
    const auto spun = ri::trace::SimulateOrientedKinematicBodyStep(
        scene,
        spin,
        1.0f,
        KinematicPhysicsOptions{.gravity = 0.0f, .ignoreColliderId = {}});
    const float expectedYawDegrees =
        ri::math::RadiansToDegrees(spin.angularVelocity.y * 0.25f); // same 0.25s cap as axis-aligned kinematic
    Expect(std::fabs(spun.state.orientationDegrees.y - expectedYawDegrees) < 0.6f,
           "Oriented kinematic steps should integrate angular velocity over the clamped simulation window");

    OrientedKinematicBodyState drop{};
    drop.center = {0.0f, 1.2f, 0.0f};
    drop.halfExtents = {0.2f, 0.2f, 0.2f};
    drop.orientationDegrees = {15.0f, 0.0f, -20.0f};
    drop.velocity = {0.0f, -2.0f, 0.0f};
    const auto landed = ri::trace::SimulateOrientedKinematicBodyStep(scene, drop, 0.15f, KinematicPhysicsOptions{});
    Expect(landed.onGround, "Oriented bodies should settle onto walkable structural floors");
}

void TestKinematicAdvanceDuration() {
    using ri::spatial::Aabb;
    using ri::trace::KinematicAdvanceStats;
    using ri::trace::KinematicPhysicsOptions;
    using ri::trace::OrientedKinematicBodyState;
    using ri::trace::TraceCollider;
    using ri::trace::TraceScene;

    TraceScene scene({
        TraceCollider{.id = "floor", .bounds = Aabb{.min = {-10.0f, -1.0f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}}, .structural = true, .dynamic = false},
    });

    ri::trace::KinematicAdvanceStats stats{};
    constexpr float kPi = 3.14159265f;
    ri::trace::OrientedKinematicBodyState spin{};
    spin.center = {0.0f, 4.0f, 0.0f};
    spin.halfExtents = {0.2f, 0.2f, 0.2f};
    spin.angularVelocity = {0.0f, kPi * 0.5f, 0.0f};
    const auto fullTurn = ri::trace::SimulateOrientedKinematicBodyForDuration(
        scene,
        spin,
        1.0f,
        KinematicPhysicsOptions{
            .gravity = 0.0f,
            .angularDamping = 1.0f,
            .ignoreColliderId = {},
        },
        {},
        {},
        &stats);
    Expect(stats.sliceCount == 4U,
           "One second of simulation should run four 0.25s kinematic slices by default");
    Expect(std::fabs(stats.consumedSeconds - 1.0f) < 0.0001f,
           "Kinematic duration helper should consume the full requested horizon when under the slice budget");
    Expect(std::fabs(fullTurn.state.orientationDegrees.y - 90.0f) < 1.5f,
           "Stacked kinematic slices should accumulate angular integration across the full duration");

    ri::trace::KinematicBodyState body{};
    body.bounds = Aabb{.min = {-0.2f, 0.5f, -0.2f}, .max = {0.2f, 1.1f, 0.2f}};
    body.velocity = {0.0f, 0.0f, 0.0f};
    stats = {};
    (void)ri::trace::SimulateKinematicBodyForDuration(
        scene, body, 0.08f, KinematicPhysicsOptions{}, {}, {}, &stats);
    Expect(stats.sliceCount == 1U,
           "Durations shorter than one slice should still advance exactly one integration step");
}

void TestObjectPhysicsBatch() {
    using ri::spatial::Aabb;
    using ri::trace::KinematicBodyState;
    using ri::trace::KinematicObjectSlot;
    using ri::trace::KinematicPhysicsOptions;
    using ri::trace::ObjectPhysicsBatchOptions;
    using ri::trace::StepKinematicObjectBatch;
    using ri::trace::TraceCollider;
    using ri::trace::TraceScene;
    using ri::trace::WakeKinematicObject;

    TraceScene scene({
        TraceCollider{.id = "floor",
                      .bounds = Aabb{.min = {-10.0f, -1.0f, -10.0f}, .max = {10.0f, 0.0f, 10.0f}},
                      .structural = true,
                      .dynamic = false},
        TraceCollider{.id = "crate_a",
                      .bounds = Aabb{.min = {-0.3f, 0.0f, -0.3f}, .max = {0.3f, 0.6f, 0.3f}},
                      .structural = true,
                      .dynamic = true},
        TraceCollider{.id = "crate_b",
                      .bounds = Aabb{.min = {2.0f, 0.0f, -0.3f}, .max = {2.6f, 0.6f, 0.3f}},
                      .structural = true,
                      .dynamic = true},
    });

    std::vector<KinematicObjectSlot> objects{};
    objects.push_back(KinematicObjectSlot{
        .objectColliderId = "crate_a",
        .state =
            KinematicBodyState{.bounds = Aabb{.min = {-0.3f, 0.0f, -0.3f}, .max = {0.3f, 0.6f, 0.3f}},
                               .velocity = ri::math::Vec3{2.5f, 0.0f, 0.0f}},
    });
    objects.push_back(KinematicObjectSlot{
        .objectColliderId = "crate_b",
        .state =
            KinematicBodyState{.bounds = Aabb{.min = {2.0f, 0.0f, -0.3f}, .max = {2.6f, 0.6f, 0.3f}},
                               .velocity = ri::math::Vec3{-0.5f, 0.0f, 0.0f}},
    });

    ObjectPhysicsBatchOptions batchOpts{.enableSleep = false};
    const auto first =
        StepKinematicObjectBatch(scene, objects, 1.0f / 60.0f, KinematicPhysicsOptions{}, {}, {}, batchOpts);
    Expect(first.simulatedCount == 2U && first.skippedSleeping == 0U,
           "Batch step should simulate every awake slot");
    Expect(first.steps.size() == 2U, "Batch step should return per-slot results");
    Expect(ri::math::Length(objects[0].state.velocity) > 0.1f,
           "Moving kinematic object should retain motion after a batch integration step");

    objects[0].sleeping = true;
    objects[1].sleeping = true;
    const auto nap = StepKinematicObjectBatch(
        scene, objects, 1.0f / 60.0f, KinematicPhysicsOptions{}, {}, {}, ObjectPhysicsBatchOptions{.enableSleep = true});
    Expect(nap.skippedSleeping == 2U && nap.simulatedCount == 0U,
           "Sleeping slots should be skipped when batch sleep is enabled");
    WakeKinematicObject(objects[0]);
    const auto wake =
        StepKinematicObjectBatch(scene, objects, 1.0f / 60.0f, KinematicPhysicsOptions{}, {}, {}, batchOpts);
    Expect(wake.simulatedCount >= 1U,
           "Woken slots should participate in batch integration again");
}

void TestPostProcessProfiles() {
    const ri::render::PostProcessParameters analog = ri::render::MakePostProcessPreset(ri::render::PostProcessPreset::AnalogHorror);
    Expect(NearlyEqual(analog.noiseAmount, 0.03f) && NearlyEqual(analog.barrelDistortion, 0.03f),
           "Analog-horror preset should expose expected proto-style baseline values");

    const ri::render::PostProcessParameters vhs = ri::render::MakePostProcessPreset(ri::render::PostProcessPreset::Vhs);
    const ri::render::PostProcessParameters mixed = ri::render::BlendPostProcessParameters(vhs, analog, 0.5f);
    Expect(mixed.noiseAmount > vhs.noiseAmount && mixed.noiseAmount < analog.noiseAmount,
           "Post-process blending should interpolate scalar channels");

    const std::span<const ri::render::PostProcessPresetDefinition> definitions =
        ri::render::GetPostProcessPresetDefinitions();
    Expect(definitions.size() >= 10U,
           "Post-process catalog should expose a broad preset registry for editor/tool browsing");
    const std::optional<ri::render::PostProcessPreset> parsedFocus =
        ri::render::TryParsePostProcessPreset("combat_focus");
    Expect(parsedFocus.has_value() && *parsedFocus == ri::render::PostProcessPreset::CombatFocus,
           "Post-process preset parsing should resolve registered preset slugs");
    const std::optional<ri::render::PostProcessPreset> parsedAlias =
        ri::render::TryParsePostProcessPreset("  Combat-Focus  ");
    Expect(parsedAlias.has_value() && *parsedAlias == ri::render::PostProcessPreset::CombatFocus,
           "Post-process preset parsing should accept case and separator aliasing");
    Expect(ri::render::ToString(ri::render::PostProcessPreset::ColdFacility) == "cold_facility",
           "Post-process preset string conversion should return stable tool-facing slugs");

    constexpr std::array<ri::render::PostProcessPresetLayer, 2> stack{{
        {ri::render::PostProcessPreset::CrispGameplay, 1.0f},
        {ri::render::PostProcessPreset::ColdFacility, 0.5f},
    }};
    const ri::render::PostProcessParameters stacked =
        ri::render::ComposePostProcessPresetStack(stack);
    Expect(stacked.tintStrength > 0.0f && stacked.noiseAmount < analog.noiseAmount,
           "Stacked post-process presets should combine cleaner gameplay shaping with layered mood");

    ri::world::PostProcessState state{};
    state.tintColor = ri::math::Vec3{1.2f, -0.4f, 0.5f};
    state.tintStrength = 1.2f;
    state.noiseAmount = 0.5f;
    state.scanlineAmount = 0.15f;
    state.blurAmount = 0.08f;
    state.barrelDistortion = 0.3f;
    state.chromaticAberration = 0.2f;
    const ri::render::PostProcessParameters mapped = ri::world::BuildPostProcessParameters(state, 42.0, 2.0f);
    Expect(NearlyEqual(mapped.timeSeconds, 42.0f), "Post-process mapping should preserve finite time");
    Expect(NearlyEqual(mapped.tintColor.x, 1.0f) && NearlyEqual(mapped.tintColor.y, 0.0f),
           "Post-process mapping should clamp tint color to unit range");
    Expect(NearlyEqual(mapped.tintStrength, 1.0f) && NearlyEqual(mapped.staticFadeAmount, 1.0f),
           "Post-process mapping should clamp strength/fade channels");
    Expect(mapped.barrelDistortion <= 0.2f && mapped.chromaticAberration <= 0.05f,
           "Post-process mapping should clamp distortion channels to stable renderer limits");

    ri::render::PostProcessParameters unstable{};
    unstable.timeSeconds = 20000.0f;
    unstable.tintColor = ri::math::Vec3{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
    };
    const ri::render::PostProcessParameters sanitized = ri::render::SanitizePostProcessParameters(unstable);
    Expect(sanitized.timeSeconds >= -4096.0f && sanitized.timeSeconds <= 4096.0f,
           "Post-process sanitization should wrap very large time values into a stable range");
    Expect(NearlyEqual(sanitized.tintColor.x, 0.0f) && NearlyEqual(sanitized.tintColor.y, 0.0f)
               && NearlyEqual(sanitized.tintColor.z, 0.0f),
           "Post-process sanitization should coerce non-finite tint channels to zero");

    state.enabled = false;
    const ri::render::PostProcessParameters disabled = ri::world::BuildPostProcessParameters(state, 12.0, 0.25f);
    Expect(NearlyEqual(disabled.timeSeconds, 12.0f)
               && NearlyEqual(disabled.noiseAmount, 0.0f)
               && NearlyEqual(disabled.staticFadeAmount, 0.25f),
           "Post-process mapping should support runtime-disabled pass output while preserving fade timing");
}

void TestValidationSchemas() {
    const ri::validation::SchemaValidationMetrics before = ri::validation::GetSchemaValidationMetrics();

    ri::validation::RuntimeTuning validTuning{};
    validTuning.walkSpeed = 4.0;
    validTuning.gravity = 25.0;
    const ri::validation::RuntimeTuning parsedValid = ri::validation::ParseStoredRuntimeTuning(validTuning);
    Expect(parsedValid.walkSpeed.has_value() && NearlyEqual(static_cast<float>(*parsedValid.walkSpeed), 4.0f),
           "Runtime tuning validation should preserve valid finite values");
    Expect(parsedValid.gravity.has_value() && NearlyEqual(static_cast<float>(*parsedValid.gravity), 25.0f),
           "Runtime tuning validation should preserve additional valid finite values");

    ri::validation::RuntimeTuning invalidTuning{};
    invalidTuning.jumpForce = std::numeric_limits<double>::infinity();
    invalidTuning.maxFallSpeed = std::numeric_limits<double>::quiet_NaN();
    const ri::validation::RuntimeTuning parsedInvalid = ri::validation::ParseStoredRuntimeTuning(invalidTuning);
    Expect(!parsedInvalid.jumpForce.has_value() && !parsedInvalid.maxFallSpeed.has_value(),
           "Runtime tuning validation should drop invalid non-finite values");

    ri::validation::LevelPayload emptyLevel{};
    const std::optional<std::string> emptyLevelError = ri::validation::ValidateLevelPayload(emptyLevel, "empty_level");
    Expect(emptyLevelError.has_value() && *emptyLevelError == "Level must have levelName, geometry, or lights.",
           "Level validation should reject empty payloads without authored identity or content");

    ri::validation::LevelPayload invalidEventLevel{};
    invalidEventLevel.levelName = "Facility";
    ri::events::EventDefinition invalidEvent{};
    invalidEvent.actions = {MakeMessageAction("hi")};
    invalidEventLevel.events = {invalidEvent};
    const std::optional<std::string> invalidEventError = ri::validation::ValidateLevelPayload(invalidEventLevel, "event_level");
    Expect(invalidEventError.has_value() && invalidEventError->find("events[0].hook must not be empty.") != std::string::npos,
           "Level validation should reject events without hook names");

    ri::validation::LevelPayload invalidWorldspawnLevel{};
    invalidWorldspawnLevel.levelName = "Facility";
    ri::validation::LevelConnection invalidConnection{};
    invalidConnection.target = "";
    invalidWorldspawnLevel.worldspawn.outputs["OnStart"] = {invalidConnection};
    const std::optional<std::string> invalidWorldspawnError = ri::validation::ValidateLevelPayload(invalidWorldspawnLevel, "worldspawn_level");
    Expect(invalidWorldspawnError.has_value() && invalidWorldspawnError->find("worldspawn.outputs.OnStart[0].target must not be empty.") != std::string::npos,
           "Level validation should reject worldspawn output connections without targets");

    ri::validation::LevelPayload validLevel{};
    validLevel.levelName = "Facility";
    validLevel.geometry.push_back(MakeNode("room", "Room", "box"));

    ri::events::EventDefinition validEvent{};
    validEvent.id = "boot";
    validEvent.hook = "level_load";
    validEvent.actions = {MakeMessageAction("boot")};
    validLevel.events = {validEvent};

    validLevel.worldspawn.inputs["OnStart"] = {MakeMessageAction("boot")};
    validLevel.worldspawn.outputs["OnFinished"] = {
        ri::validation::LevelConnection{
            .target = "relay_a",
            .input = std::optional<std::string>("Trigger"),
            .delayMs = std::optional<double>(150.0),
            .once = std::optional<bool>(true),
        },
    };

    validLevel.actionGroups["intro"] = {MakeMessageAction("intro")};
    validLevel.targetGroups["lights"] = {"light-a", "light-b"};
    validLevel.sequences["alarm"] = {MakeSequenceStep(100.0, {MakeMessageAction("seq")})};

    const std::optional<std::string> validLevelError = ri::validation::ValidateLevelPayload(validLevel, "valid_level");
    Expect(!validLevelError.has_value(), "Level validation should accept well-formed engine-native payloads");

    ri::validation::LevelPayload dirtyLevel = validLevel;
    dirtyLevel.levelName = "  Facility  ";
    dirtyLevel.objective = "   \t  ";
    dirtyLevel.worldspawn.outputs["  OnStart  "] = {
        ri::validation::LevelConnection{
            .target = " relay_b ",
            .input = std::optional<std::string>(" Trigger "),
        },
    };
    dirtyLevel.worldspawn.inputs["  Use  "] = {MakeMessageAction("used")};
    ri::validation::SanitizeLevelPayload(dirtyLevel);
    Expect(dirtyLevel.levelName.has_value() && *dirtyLevel.levelName == "Facility",
           "Level sanitization should trim optional identity strings");
    Expect(!dirtyLevel.objective.has_value(), "Level sanitization should drop whitespace-only objectives");
    Expect(dirtyLevel.worldspawn.outputs.contains("OnStart")
               && dirtyLevel.worldspawn.outputs.at("OnStart").size() == 1U
               && dirtyLevel.worldspawn.outputs.at("OnStart")[0].target == "relay_b"
               && dirtyLevel.worldspawn.outputs.at("OnStart")[0].input.has_value()
               && *dirtyLevel.worldspawn.outputs.at("OnStart")[0].input == "Trigger",
           "Level sanitization should normalize worldspawn output labels and endpoint strings");
    Expect(dirtyLevel.worldspawn.inputs.contains("Use"),
           "Level sanitization should normalize worldspawn input labels");

    ri::validation::RuntimeCheckpointState invalidCheckpoint{};
    invalidCheckpoint.level = "";
    invalidCheckpoint.flags = {"", "power_on"};
    invalidCheckpoint.eventIds = {"boot", ""};
    invalidCheckpoint.values = {{"progress", std::numeric_limits<double>::quiet_NaN()}};
    const std::optional<std::string> invalidCheckpointError =
        ri::validation::ValidateCheckpointState(invalidCheckpoint, "checkpoint");
    Expect(invalidCheckpointError.has_value(),
           "Checkpoint validation should reject empty level labels and invalid payload fields");

    ri::validation::RuntimeCheckpointState checkpointRaw{};
    checkpointRaw.level = "level1.json";
    checkpointRaw.checkpointId = "";
    checkpointRaw.flags = {"power_on", "", "power_on", "hazard"};
    checkpointRaw.eventIds = {"event_a", "", "event_a", "event_b"};
    checkpointRaw.values = {
        {"progress", 2.0},
        {"", 1.0},
        {"danger", std::numeric_limits<double>::infinity()},
    };
    const ri::validation::RuntimeCheckpointState parsedCheckpoint =
        ri::validation::ParseStoredCheckpointState(checkpointRaw);
    Expect(parsedCheckpoint.level.has_value() && *parsedCheckpoint.level == "level1.json"
           && !parsedCheckpoint.checkpointId.has_value(),
           "Checkpoint parsing should keep non-empty level names and clear empty checkpoint IDs");
    Expect(parsedCheckpoint.flags.size() == 2U
           && parsedCheckpoint.flags[0] == "power_on"
           && parsedCheckpoint.flags[1] == "hazard",
           "Checkpoint parsing should drop empty flags and deduplicate while preserving order");
    Expect(parsedCheckpoint.eventIds.size() == 2U
           && parsedCheckpoint.eventIds[0] == "event_a"
           && parsedCheckpoint.eventIds[1] == "event_b",
           "Checkpoint parsing should drop empty event IDs and deduplicate while preserving order");
    Expect(parsedCheckpoint.values.size() == 1U
           && parsedCheckpoint.values.contains("progress")
           && NearlyEqual(static_cast<float>(parsedCheckpoint.values.at("progress")), 2.0f),
           "Checkpoint parsing should keep only finite named world values");
    Expect(!ri::validation::ValidateCheckpointState(parsedCheckpoint, "checkpoint").has_value(),
           "Checkpoint validation should accept sanitized checkpoint payloads");

    const ri::validation::SchemaValidationMetrics after = ri::validation::GetSchemaValidationMetrics();
    Expect(after.tuningParses >= before.tuningParses + 2, "Schema metrics should count runtime tuning parses");
    Expect(after.tuningParseFailures >= before.tuningParseFailures + 1, "Schema metrics should count failed runtime tuning parses");
    Expect(after.levelValidations >= before.levelValidations + 4, "Schema metrics should count level validation attempts");
    Expect(after.levelValidationFailures >= before.levelValidationFailures + 3, "Schema metrics should count rejected level payloads");
    Expect(after.levelPayloadSanitizations >= before.levelPayloadSanitizations + 1,
           "Schema metrics should record level payload sanitization passes");
    Expect(after.levelPayloadSanitizationRepairs >= before.levelPayloadSanitizationRepairs + 2,
           "Schema metrics should count individual sanitization repairs");
}

void TestAudioManager() {
    auto backend = std::make_shared<FakeAudioBackend>();
    ri::audio::AudioManager manager(backend);

    ri::audio::AudioEnvironmentProfileInput environment{};
    environment.activeVolumes = {" hall_b ", "hall_a", "hall_b", ""};
    environment.reverbMix = 0.75;
    environment.echoDelayMs = 250.0;
    environment.echoFeedback = 0.8;
    environment.dampening = 0.5;
    environment.volumeScale = 1.5;
    environment.playbackRate = 1.25;

    const std::optional<ri::audio::AudioEnvironmentProfile> normalized = manager.NormalizeEnvironmentProfile(environment);
    Expect(normalized.has_value(), "Audio environment normalization should produce a profile for valid input");
    Expect(normalized->label == "hall_a,hall_b",
           "Audio environment normalization should trim, dedupe, and order active-volume labels deterministically");
    Expect(NearlyEqual(static_cast<float>(normalized->reverbMix), 0.75f),
           "Audio environment normalization should preserve finite reverb mix values");

    Expect(manager.SetEnvironmentProfile(environment),
           "Audio manager should report environment changes when a new profile is applied");
    Expect(!manager.SetEnvironmentProfile(environment),
           "Audio manager should ignore environment writes that resolve to the same normalized signature");

    const ri::audio::AudioResolvedPlayback resolved = manager.ApplyEnvironmentToPlayback({
        .volume = 0.6,
        .playbackRate = 1.0,
        .pan = 0.5,
        .occlusion = 0.2,
        .distanceMeters = 6.0,
    });
    Expect(NearlyEqual(static_cast<float>(resolved.volume), 0.39343f),
           "Audio manager should fold environment, occlusion, and distance attenuation into playback volume");
    Expect(NearlyEqual(static_cast<float>(resolved.playbackRate), 1.25f),
           "Audio manager should scale playback rate through the active environment profile");
    Expect(NearlyEqual(static_cast<float>(resolved.pan), 0.5f),
           "Audio manager should preserve normalized spatial pan in resolved playback");

    const std::shared_ptr<ri::audio::ManagedSound> loop = manager.CreateLoopingSound("audio/ambient_loop.ogg", 0.5);
    Expect(loop != nullptr, "Audio manager should create looping sounds when a backend is available");
    Expect(loop->IsLooping(), "Looping audio creation should mark sounds as looping");
    Expect(!loop->IsPlaying(), "Managed loop creation should not auto-play before the caller starts playback");
    Expect(loop->GetPath() == "audio/ambient_loop.ogg", "Managed sounds should preserve their source path");
    Expect(NearlyEqual(static_cast<float>(loop->GetVolume()), 0.675f),
           "Managed sounds should inherit environment-adjusted volume at creation time");
    Expect(NearlyEqual(static_cast<float>(backend->Created().front()->Request().pan), 0.0f),
           "Loop playback requests should default to centered pan");
    loop->Play();
    Expect(loop->IsPlaying(), "Managed loops should play once explicitly started");

    loop->SetCurrentTime(2.5);
    Expect(NearlyEqual(static_cast<float>(loop->GetCurrentTime()), 2.5f),
           "Managed sounds should preserve assigned playback time");
    loop->SetVolume(2.0);
    Expect(NearlyEqual(static_cast<float>(loop->GetVolume()), 1.0f),
           "Managed sounds should clamp volume to the valid range");
    loop->SetPlaybackRate(2.0);
    Expect(NearlyEqual(static_cast<float>(loop->GetPlaybackRate()), 1.5f),
           "Managed sounds should clamp playback rate to the valid range");

    const std::shared_ptr<ri::audio::ManagedSound> oneShot = manager.PlayOneShot("audio/hit.wav", 0.4);
    Expect(oneShot != nullptr, "Audio manager should create one-shot sounds when unmuted");
    Expect(manager.PendingEchoCount() == 1U,
           "Audio manager should schedule echo playback when the active environment requests it");
    const std::shared_ptr<ri::audio::ManagedSound> safeOneShot = manager.PlayOneShotSafe(
        ri::audio::OneShotAudioEventRequest{
            .eventId = "pickup_ammo",
            .path = "audio/pickup.wav",
            .channel = "sfx",
            .priority = 2,
            .concurrencyCap = 1U,
            .antiSpamWindowMs = 400.0,
            .volume = 0.5,
            .distanceMeters = 2.0,
            .occlusion = 0.1,
        },
        ri::audio::SafeAudioPlaybackContext{
            .source = "pickup",
            .deviceReady = true,
            .worldReady = true,
        });
    Expect(safeOneShot != nullptr,
           "Audio manager safe gateway should emit valid one-shot events when context/device are ready");
    const std::shared_ptr<ri::audio::ManagedSound> safeSpamBlocked = manager.PlayOneShotSafe(
        ri::audio::OneShotAudioEventRequest{
            .eventId = "pickup_ammo",
            .path = "audio/pickup.wav",
            .channel = "sfx",
            .concurrencyCap = 1U,
            .antiSpamWindowMs = 400.0,
            .volume = 0.5,
        });
    Expect(safeSpamBlocked == nullptr,
           "Audio manager one-shot dispatcher should enforce anti-spam/concurrency safety caps deterministically");

    manager.Tick(200.0);
    Expect(manager.PendingEchoCount() == 1U, "Audio manager should keep pending echoes until their delay expires");
    manager.Tick(60.0);
    Expect(manager.PendingEchoCount() == 0U, "Audio manager should release pending echoes after enough time passes");
    Expect(backend->Created().size() >= 3U,
           "Audio manager should have created loop, one-shot, and echo playback handles");

    backend->Created()[2]->TriggerFinished();
    backend->Created()[1]->TriggerFinished();
    Expect(manager.GetMetrics().managedSounds <= 2U,
           "Audio manager should release finished transient sounds from managed tracking");

    const std::shared_ptr<ri::audio::ManagedSound> voiceA = manager.PlayVoice("audio/voice_a.ogg", 0.9);
    Expect(voiceA != nullptr, "Audio manager should create voice playback when unmuted");
    const auto voiceAHandle = backend->Created().back();
    const std::shared_ptr<ri::audio::ManagedSound> voiceB = manager.PlayVoice("audio/voice_b.ogg", 0.9);
    Expect(voiceB != nullptr, "Audio manager should replace active voice playback with a new request");
    Expect(voiceAHandle->StopCount() >= 1 && voiceAHandle->UnloadCount() >= 1,
           "Audio manager should stop and unload the previous active voice when a new one starts");
    Expect(manager.GetMetrics().voiceActive,
           "Audio manager metrics should report an active voice after voice playback begins");

    backend->Created().back()->TriggerFinished();
    Expect(!manager.GetMetrics().voiceActive,
           "Audio manager should clear the active voice when the current voice playback finishes");

    manager.SyncLoopedAudioBuses("explore", {
        {.channel = "music", .phase = "explore", .loopPath = "audio/music_explore.ogg", .volume = 0.7, .fadeMs = 120.0, .priority = 2},
        {.channel = "ambience", .phase = "explore", .loopPath = "audio/ambience_hall.ogg", .volume = 0.4, .fadeMs = 90.0, .priority = 1},
    });
    manager.Tick(150.0);
    auto loopStates = manager.GetLoopedAudioBusStates();
    Expect(loopStates.size() >= 2U
               && loopStates[0].channel == "ambience"
               && loopStates[1].channel == "music",
           "Audio manager should keep deterministic channel ordering for looped-bus state sync");
    manager.SetLoopedAudioBusDuck("music", 0.3);
    manager.SyncLoopedAudioBuses("combat", {
        {.channel = "music", .phase = "combat", .loopPath = "audio/music_combat.ogg", .volume = 1.0, .fadeMs = 100.0, .priority = 4},
        {.channel = "music", .phase = "combat", .loopPath = "audio/music_lowprio.ogg", .volume = 0.2, .fadeMs = 100.0, .priority = 1},
    });
    manager.Tick(120.0);
    loopStates = manager.GetLoopedAudioBusStates();
    const auto musicIt = std::find_if(loopStates.begin(), loopStates.end(), [](const ri::audio::LoopedAudioBusChannelState& state) {
        return state.channel == "music";
    });
    Expect(musicIt != loopStates.end()
               && musicIt->loopPath == "audio/music_combat.ogg"
               && NearlyEqual(static_cast<float>(musicIt->targetVolume), 0.3f),
           "Audio manager loop bus should arbitrate by priority and apply channel ducking deterministically");

    const std::size_t managedBeforeStop = manager.GetMetrics().managedSounds;
    manager.StopManagedSound(loop, true);
    Expect(manager.GetMetrics().managedSounds + 1U == managedBeforeStop,
           "Audio manager should unregister explicitly unloaded managed sounds without disturbing other managed loop-bus channels");

    manager.SetMuted(true);
    Expect(manager.PlayOneShot("audio/muted_hit.wav", 0.4) == nullptr,
           "Audio manager should suppress one-shot playback when muted");
    Expect(manager.PlayVoice("audio/muted_voice.wav", 1.0) == nullptr,
           "Audio manager should suppress voice playback when muted");

    const ri::audio::AudioManagerMetrics metrics = manager.GetMetrics();
    Expect(metrics.loopsCreated >= 1U, "Audio manager should count created looping sounds");
    Expect(metrics.oneShotsPlayed >= 1U, "Audio manager should count played one-shots");
    Expect(metrics.voicesPlayed == 2U, "Audio manager should count played voice lines");
    Expect(metrics.environmentChanges == 1U, "Audio manager should count distinct environment changes");
    Expect(metrics.activeEnvironment == "hall_a,hall_b",
           "Audio manager metrics should report the normalized active environment label");
    Expect(NearlyEqual(static_cast<float>(metrics.activeEnvironmentMix), 0.75f),
           "Audio manager metrics should report the active environment reverb mix");
    Expect(metrics.droppedEchoes == 0U, "Audio manager should report dropped-echo metrics");
    Expect(metrics.oneShotDroppedBySafety > 0U,
           "Audio manager should report safety-gated one-shot drops for invalid/duplicate dispatch");

    manager.SetMuted(false);
    for (int i = 0; i < 280; ++i) {
        (void)manager.PlayOneShot("audio/echo_stress.wav", 0.8);
    }
    const ri::audio::AudioManagerMetrics stressedMetrics = manager.GetMetrics();
    Expect(stressedMetrics.pendingEchoes <= 256U,
           "Audio manager should cap pending echoes to keep dynamic audio scheduling bounded");
    Expect(stressedMetrics.droppedEchoes > 0U,
           "Audio manager should count dropped echoes when the pending queue exceeds its cap");

    auto backendMaster = std::make_shared<FakeAudioBackend>();
    ri::audio::AudioManager managerMaster(backendMaster);
    ri::audio::AudioEnvironmentProfileInput flatEnv{};
    flatEnv.activeVolumes = {"room"};
    flatEnv.volumeScale = 1.0;
    flatEnv.dampening = 0.0;
    managerMaster.SetEnvironmentProfile(flatEnv);
    managerMaster.SetMasterLinearGain(0.5);
    const std::shared_ptr<ri::audio::ManagedSound> attenuated = managerMaster.CreateLoopingSound("audio/master_test.ogg", 0.8);
    Expect(attenuated != nullptr, "Audio manager should honor master gain when creating loops");
    Expect(NearlyEqual(static_cast<float>(attenuated->GetVolume()), 0.4f),
           "Master linear gain should scale resolved clip volume for new playbacks");
    Expect(NearlyEqual(static_cast<float>(managerMaster.GetMasterLinearGain()), 0.5f),
           "Master linear gain getter should return the clamped value");
    managerMaster.SetMasterLinearGain(1.0);
    const std::shared_ptr<ri::audio::ManagedSound> unity = managerMaster.CreateLoopingSound("audio/master_full.ogg", 0.25);
    Expect(NearlyEqual(static_cast<float>(unity->GetVolume()), 0.25f),
           "Unity master gain should pass environment-resolved volume through unchanged");
}

void TestRuntimeSnapshots() {
    ri::runtime::RuntimeEventBusMetrics eventBusMetrics{};
    eventBusMetrics.emitted = 4;
    eventBusMetrics.listenersAdded = 3;
    eventBusMetrics.listenersRemoved = 1;
    eventBusMetrics.activeListeners = 2;

    ri::validation::SchemaValidationMetrics schemaMetrics{};
    schemaMetrics.tuningParses = 6;
    schemaMetrics.tuningParseFailures = 1;
    schemaMetrics.checkpointParses = 3;
    schemaMetrics.checkpointParseFailures = 1;
    schemaMetrics.checkpointValidations = 5;
    schemaMetrics.checkpointValidationFailures = 2;
    schemaMetrics.levelValidations = 5;
    schemaMetrics.levelValidationFailures = 2;

    ri::audio::AudioManagerMetrics audioMetrics{};
    audioMetrics.managedSounds = 3;
    audioMetrics.loopsCreated = 1;
    audioMetrics.oneShotsPlayed = 2;
    audioMetrics.voicesPlayed = 1;
    audioMetrics.voiceActive = true;
    audioMetrics.environmentChanges = 2;
    audioMetrics.activeEnvironment = "hall_a";
    audioMetrics.activeEnvironmentMix = 0.75;

    const ri::debug::HelperLibraryMetricsSnapshot helperMetrics = ri::debug::BuildHelperLibraryMetricsSnapshot(
        "session-42",
        eventBusMetrics,
        schemaMetrics,
        audioMetrics,
        ri::debug::DebugRecentActivity{
            .lastLevelEvent = "level_load",
            .lastSchemaEvent = "validated facility",
            .lastMessage = "engine ready",
            .lastEntityIoEvent = "relay_a->door_b",
        });

    Expect(helperMetrics.schemaValidations == 5U, "Debug snapshot builder should mirror schema validation totals");
    Expect(helperMetrics.eventBusListeners == 2U, "Debug snapshot builder should mirror active event listeners");
    Expect(helperMetrics.audioEnvironment == "hall_a", "Debug snapshot builder should mirror active audio environment labels");
    Expect(helperMetrics.runtimeSession == "session-42", "Debug snapshot builder should preserve runtime session IDs");

    ri::events::EventEngine engine("facility");
    ri::events::EventDefinition bootEvent{};
    bootEvent.id = "boot";
    bootEvent.hook = "level_load";
    bootEvent.actions = {MakeMessageAction("boot")};
    engine.SetEvents({bootEvent});
    engine.SetWorldFlag("power_on");
    engine.SetWorldValue("progress", 2.0);
    engine.SetWorldValue("danger", 1.0);

    const ri::debug::EventEngineDebugState eventState = ri::debug::BuildEventEngineDebugState(engine);
    Expect(eventState.eventCount == 1U, "Event debug snapshots should count authored events");
    Expect(eventState.worldFlags.size() == 1U && eventState.worldFlags[0] == "power_on",
           "Event debug snapshots should include sorted world flags");
    Expect(eventState.worldValues.size() == 2U && eventState.worldValues[0].key == "danger" && eventState.worldValues[1].key == "progress",
           "Event debug snapshots should sort world values by key");

    const ri::spatial::BspSpatialIndex collisionIndex({
        ri::spatial::SpatialEntry{
            .id = "wall",
            .bounds = ri::spatial::Aabb{.min = {-1.0f, 0.0f, -1.0f}, .max = {1.0f, 2.0f, 1.0f}},
        },
    });
    const ri::spatial::BspSpatialIndex structuralIndex({
        ri::spatial::SpatialEntry{
            .id = "floor",
            .bounds = ri::spatial::Aabb{.min = {-5.0f, -1.0f, -5.0f}, .max = {5.0f, 0.0f, 5.0f}},
        },
    });

    const ri::debug::SpatialIndexDebugSnapshot collisionSnapshot = ri::debug::BuildSpatialIndexDebugSnapshot(collisionIndex);
    Expect(collisionSnapshot.entryCount == 1U && collisionSnapshot.bounds.has_value(),
           "Spatial debug snapshots should preserve entry counts and non-empty bounds");

    ri::trace::TraceScene debugTraceScene({
        ri::trace::TraceCollider{
            .id = "wall",
            .bounds = ri::spatial::Aabb{.min = {-1.0f, 0.0f, -1.0f}, .max = {1.0f, 2.0f, 1.0f}},
            .structural = true,
            .dynamic = false,
        },
        ri::trace::TraceCollider{
            .id = "crate",
            .bounds = ri::spatial::Aabb{.min = {2.0f, 0.0f, -1.0f}, .max = {3.0f, 1.0f, 1.0f}},
            .structural = false,
            .dynamic = true,
        },
    });
    (void)debugTraceScene.QueryCollidablesForBox(
        ri::spatial::Aabb{.min = {-2.0f, -1.0f, -2.0f}, .max = {4.0f, 3.0f, 2.0f}});
    (void)debugTraceScene.TraceRay(
        ri::math::Vec3{-3.0f, 1.0f, 0.0f},
        ri::math::Vec3{1.0f, 0.0f, 0.0f},
        8.0f);

    ri::debug::RenderGameStateSnapshot gameState{};
    gameState.mode = "devlaunch";
    gameState.level = "facility.ri_scene";
    gameState.paused = false;
    gameState.counts.collidables = 8;
    gameState.counts.structuralCollidables = 6;
    gameState.counts.interactives = 2;
    gameState.counts.physicsObjects = 1;
    gameState.counts.triggerVolumes = 3;
    gameState.counts.logicEntities = 4;
    gameState.helperLibraries = helperMetrics;
    gameState.detailFields = {
        {"build", "dev"},
        {"renderer", "vulkan"},
    };
    gameState.audioEnvironment = ri::debug::RenderEnvironmentSnapshot{
        .label = "hall_a",
        .activeVolumes = {"hall_a"},
        .mix = 0.75,
        .delayMs = 250.0,
    };
    gameState.coordinateSystem = "rawiron test coordinates";

    const ri::debug::RenderGameStateSnapshot builtSnapshot = ri::debug::BuildRenderGameStateSnapshot(
        ri::debug::RenderGameStateBuildInput{
            .mode = "devlaunch",
            .level = "facility.ri_scene",
            .paused = false,
            .counts = gameState.counts,
            .helperLibraries = helperMetrics,
            .audioEnvironment = gameState.audioEnvironment,
            .postProcessEnvironment = std::nullopt,
            .coordinateSystem = gameState.coordinateSystem,
            .nearbyInteractives = {},
            .actors = {},
            .detailFields = {{"build", "dev"}},
        });
    Expect(builtSnapshot.detailFields.contains("eventBus.emitted")
               && builtSnapshot.detailFields.at("eventBus.emitted") == "4"
               && builtSnapshot.detailFields.contains("schema.validations"),
           "Render-game snapshot builder should stamp core debug/reporting detail fields");

    const std::string stateText = ri::debug::FormatRenderGameStateSnapshot(gameState);
    Expect(stateText.find("mode: devlaunch") != std::string::npos,
           "Render-game snapshot formatting should include the runtime mode");
    Expect(stateText.find("audio_environment: hall_a") != std::string::npos,
           "Render-game snapshot formatting should include the active audio environment");
    Expect(stateText.find("runtime_session: session-42") != std::string::npos,
           "Render-game snapshot formatting should include the runtime session label");

    const ri::math::Vec3 proximityObserver{0.0f, 0.0f, 0.0f};
    const ri::debug::InteractiveProximityInput interactiveCandidates[] = {
        {.id = "door", .label = "Door", .position = {0.0f, 0.0f, 5.0f}},
        {.id = "crate", .label = "Crate", .position = {1.0f, 0.0f, 0.0f}},
        {.id = "note", .label = "Note", .position = {0.0f, 0.0f, 2.0f}},
    };
    const std::vector<ri::debug::NearbyInteractiveSnapshot> nearestInteractives =
        ri::debug::SelectNearestInteractivesForSnapshot(proximityObserver, interactiveCandidates, 2);
    Expect(nearestInteractives.size() == 2U, "Interactive proximity selection should honor the max count");
    Expect(nearestInteractives[0].id == "crate" && nearestInteractives[1].id == "note",
           "Interactive proximity selection should prefer closer candidates");

    const ri::debug::InteractiveProximityInput tiedInteractives[] = {
        {.id = "b", .label = "B", .position = {2.0f, 0.0f, 0.0f}},
        {.id = "a", .label = "A", .position = {2.0f, 0.0f, 0.0f}},
    };
    const std::vector<ri::debug::NearbyInteractiveSnapshot> tieBroken =
        ri::debug::SelectNearestInteractivesForSnapshot(proximityObserver, tiedInteractives, 2);
    Expect(tieBroken.size() == 2U && tieBroken[0].id == "a" && tieBroken[1].id == "b",
           "Interactive proximity selection should break distance ties using stable id ordering");

    const ri::debug::ActorProximityInput actorCandidates[] = {
        {.kind = "enemy", .id = "e1", .state = "patrol", .animation = "walk", .position = {10.0f, 0.0f, 0.0f}},
        {.kind = "enemy", .id = "e2", .state = "alert", .animation = "run", .position = {3.0f, 0.0f, 4.0f}},
        {.kind = "friendly_npc", .id = "npc_shop", .state = "PATROL", .animation = "idle", .position = {0.0f, 0.0f, 1.0f}},
    };
    const std::vector<ri::debug::ActorSnapshotEntry> nearestActorRows =
        ri::debug::SelectNearestActorsForSnapshot(proximityObserver, actorCandidates, 4);
    Expect(nearestActorRows.size() == 3U && nearestActorRows[0].id == "npc_shop" && nearestActorRows[1].id == "e2",
           "Actor proximity selection should rank mixed actors by distance");

    gameState.nearbyInteractives = nearestInteractives;
    gameState.actors = ri::debug::SelectNearestActorsForSnapshot(proximityObserver, actorCandidates, 2);
    const std::string proximityText = ri::debug::FormatRenderGameStateSnapshot(gameState);
    Expect(proximityText.find("nearby_interactives:") != std::string::npos
               && proximityText.find("actors:") != std::string::npos,
           "Render-game snapshot formatting should list proximity selections when present");
    const std::string proximityJson = ri::debug::FormatRenderGameStateSnapshotJson(gameState);
    Expect(proximityJson.find("\"nearbyInteractives\":[") != std::string::npos
               && proximityJson.find("\"actors\":[") != std::string::npos
               && proximityJson.find("\"id\":\"crate\"") != std::string::npos
               && proximityJson.find("\"details\":{") != std::string::npos
               && proximityJson.find("\"build\":\"dev\"") != std::string::npos,
           "Render-game snapshot JSON should include nearby interactives and actor rows for tooling");
    ri::debug::RenderGameStateSnapshot changedState = gameState;
    changedState.paused = true;
    changedState.counts.collidables = 9;
    changedState.detailFields["build"] = "prod";
    const ri::debug::RenderGameStateDiff stateDiff = ri::debug::BuildRenderGameStateDiff(gameState, changedState);
    const std::string stateDiffText = ri::debug::FormatRenderGameStateDiff(stateDiff);
    Expect(!stateDiff.changes.empty()
               && stateDiffText.find("render_state_diff") != std::string::npos
               && stateDiffText.find("details.build") != std::string::npos,
           "Runtime snapshot diagnostics should support state diff formatting for time-travel debugging");
    std::vector<ri::debug::RenderGameStateTimelineEntry> timeline;
    ri::debug::AppendRenderGameStateTimelineEntry(timeline, {.timestamp = "t0", .snapshot = gameState}, 2U);
    ri::debug::AppendRenderGameStateTimelineEntry(timeline, {.timestamp = "t1", .snapshot = changedState}, 2U);
    ri::debug::AppendRenderGameStateTimelineEntry(timeline, {.timestamp = "t2", .snapshot = changedState}, 2U);
    Expect(timeline.size() == 2U && timeline.front().timestamp == "t1",
           "Runtime snapshot timeline should retain bounded recent history for replay diagnostics");

    ri::debug::RuntimeDebugSnapshot reportSnapshot{};
    reportSnapshot.timestamp = "2026-04-06T12:00:00Z";
    reportSnapshot.sessionId = "session-42";
    reportSnapshot.scene.sceneChildren = 12;
    reportSnapshot.scene.collidables = 8;
    reportSnapshot.scene.structuralCollidables = 6;
    reportSnapshot.scene.bvhMeshes = 1;
    reportSnapshot.scene.interactives = 2;
    reportSnapshot.scene.physicsObjects = 1;
    reportSnapshot.scene.levelEvents = 1;
    reportSnapshot.scene.triggerVolumes = 3;
    reportSnapshot.scene.logicEntities = 4;
    reportSnapshot.scene.debugHelpers = true;
    reportSnapshot.scene.staticCollidables = 6;
    reportSnapshot.scene.dynamicCollidables = 2;
    reportSnapshot.scene.staticInteractives = 1;
    reportSnapshot.scene.dynamicInteractives = 1;
    reportSnapshot.eventEngine = eventState;
    reportSnapshot.spatial = ri::debug::BuildSpatialDebugSnapshot(debugTraceScene, collisionIndex, structuralIndex);
    reportSnapshot.helperLibraries = helperMetrics;
    reportSnapshot.audioEnvironment = ri::debug::RenderEnvironmentSnapshot{
        .label = "hall_a",
        .activeVolumes = {"hall_a"},
        .mix = 0.75,
        .delayMs = 250.0,
    };
    reportSnapshot.postProcessEnvironment = ri::debug::RenderEnvironmentSnapshot{
        .label = "fog_bay",
        .activeVolumes = {"fog_bay"},
        .mix = 0.25,
        .delayMs = 0.0,
        .tintStrength = 0.2,
        .blurAmount = 0.015,
    };
    reportSnapshot.structuralGraph = ri::structural::StructuralGraphSummary{
        .nodeCount = 4,
        .edgeCount = 3,
        .cycleCount = 0,
        .unresolvedDependencyCount = 1,
        .unresolvedDependencies = {},
        .phaseBuckets = ri::structural::StructuralPhaseBuckets{
            .compile = 2,
            .runtime = 1,
            .postBuild = 0,
            .frame = 1,
        },
    };

    const std::string report = ri::debug::FormatRuntimeDebugReport(reportSnapshot);
    Expect(report.find("== RAWIRON DEBUG REPORT ==") != std::string::npos,
           "Runtime debug report formatting should include a clear title");
    Expect(report.find("Session: session-42") != std::string::npos,
           "Runtime debug report formatting should include the runtime session");
    Expect(report.find("World Flags: power_on") != std::string::npos,
           "Runtime debug report formatting should include world flags");
    Expect(report.find("danger=1.00, progress=2.00") != std::string::npos,
           "Runtime debug report formatting should include sorted world values");
    Expect(report.find("Audio environment: hall_a | Changes: 2 | Mix: 0.75") != std::string::npos,
           "Runtime debug report formatting should include helper audio metrics");
    Expect(report.find("Nodes: 4 | Edges: 3 | Cycles: 0 | Unresolved: 1") != std::string::npos,
           "Runtime debug report formatting should include structural graph summary");
    Expect(report.find("Trace Queries: box=") != std::string::npos,
           "Runtime debug report formatting should include trace query telemetry");
    Expect(report.find("Trace Candidates: static=") != std::string::npos,
           "Runtime debug report formatting should include trace candidate telemetry");

    const std::array<std::string_view, 4> requiredPieces{
        "collidables",
        "interactives",
        "triggerVolumes",
        "logicEntities",
    };
    const ri::debug::ProofBoardSnapshot proof = ri::debug::BuildProofBoardSnapshot(reportSnapshot.scene, requiredPieces);
    Expect(proof.towerStanding, "Proof board should report tower standing when structural scene pieces are present");
    const std::string proofText = ri::debug::FormatProofBoardSnapshot(proof);
    Expect(proofText.find("proof_board") != std::string::npos
               && proofText.find("tower_standing=true") != std::string::npos
               && proofText.find("collidables") != std::string::npos,
           "Proof board formatter should serialize piece inventory and standing checks");
}

void TestVulkanCommandListSink() {
    ri::core::RenderCommandStream stream;
    stream.EmitSorted(
        ri::core::RenderCommandType::ClearColor,
        ri::core::ClearColorCommand{.r = 0.12f, .g = 0.2f, .b = 0.32f, .a = 1.0f},
        ri::core::PackRenderSortKey(0U, 1U, 0U, 0U));
    stream.EmitSorted(
        ri::core::RenderCommandType::SetViewProjection,
        ri::core::SetViewProjectionCommand{},
        ri::core::PackRenderSortKey(0U, 1U, 0U, 1U));
    stream.EmitSorted(
        ri::core::RenderCommandType::DrawMesh,
        ri::core::DrawMeshCommand{.meshHandle = 4, .materialHandle = 7, .firstIndex = 0U, .indexCount = 36U, .instanceCount = 2U},
        ri::core::PackRenderSortKey(1U, 3U, 2U, 20U));
    stream.EmitSorted(
        ri::core::RenderCommandType::DrawMesh,
        ri::core::DrawMeshCommand{.meshHandle = 2, .materialHandle = 3, .firstIndex = 0U, .indexCount = 24U, .instanceCount = 1U},
        ri::core::PackRenderSortKey(1U, 3U, 1U, 10U));

    const ri::core::RenderSubmissionPlan plan = ri::core::BuildRenderSubmissionPlan(stream);
    ri::render::vulkan::VulkanCommandListSink sink;
    ri::core::RenderRecorderStats stats{};
    Expect(ri::core::ExecuteRenderSubmissionPlan(stream, plan, sink, &stats),
           "Vulkan command-list sink should execute valid submission plans");
    Expect(stats.commandsVisited == 4U && stats.drawCommands == 2U,
           "Vulkan command-list sink execution should count command kinds");

    const auto& operations = sink.Operations();
    Expect(operations.size() == 8U,
           "Vulkan command-list sink should emit begin/end batch markers and per-command operations");
    Expect(operations[0].type == ri::render::vulkan::VulkanRenderOpType::BeginBatch
               && operations[1].type == ri::render::vulkan::VulkanRenderOpType::ClearColor
               && operations[2].type == ri::render::vulkan::VulkanRenderOpType::SetViewProjection
               && operations[3].type == ri::render::vulkan::VulkanRenderOpType::EndBatch,
           "Vulkan command-list sink should preserve first batch command ordering");
    Expect(operations[4].type == ri::render::vulkan::VulkanRenderOpType::BeginBatch
               && operations[5].type == ri::render::vulkan::VulkanRenderOpType::DrawMesh
               && operations[6].type == ri::render::vulkan::VulkanRenderOpType::DrawMesh
               && operations[7].type == ri::render::vulkan::VulkanRenderOpType::EndBatch,
           "Vulkan command-list sink should preserve second batch draw ordering");
    Expect(operations[5].meshHandle == 2 && operations[6].meshHandle == 4,
           "Vulkan command-list sink should preserve draw sort-key ordering");
}

void TestVulkanCommandRecorder() {
    class CapturingRecorder final : public ri::render::vulkan::VulkanBackendRecorder {
    public:
        bool BeginBatch(std::uint8_t passIndex, std::uint16_t pipelineBucket) override {
            log.push_back("begin:" + std::to_string(passIndex) + ":" + std::to_string(pipelineBucket));
            return true;
        }

        bool EndBatch(std::uint8_t passIndex, std::uint16_t pipelineBucket) override {
            log.push_back("end:" + std::to_string(passIndex) + ":" + std::to_string(pipelineBucket));
            return true;
        }

        bool ClearColor(const float rgba[4]) override {
            lastClearR = rgba[0];
            lastClearA = rgba[3];
            log.push_back("clear");
            return true;
        }

        bool SetViewProjection(const float viewProjection[16]) override {
            (void)viewProjection;
            log.push_back("set_view_projection");
            return true;
        }

        bool DrawMesh(std::int32_t meshHandle,
                      std::int32_t materialHandle,
                      std::uint32_t firstIndex,
                      std::uint32_t indexCount,
                      std::uint32_t instanceCount,
                      const float model[16]) override {
            (void)model;
            log.push_back("draw:" + std::to_string(meshHandle));
            lastDrawMesh = meshHandle;
            lastDrawMaterial = materialHandle;
            lastDrawFirstIndex = firstIndex;
            lastDrawIndexCount = indexCount;
            lastDrawInstanceCount = instanceCount;
            return true;
        }

        std::vector<std::string> log;
        float lastClearR = 0.0f;
        float lastClearA = 0.0f;
        int lastDrawMesh = -1;
        int lastDrawMaterial = -1;
        std::uint32_t lastDrawFirstIndex = 0;
        std::uint32_t lastDrawIndexCount = 0;
        std::uint32_t lastDrawInstanceCount = 0;
    };

    std::vector<ri::render::vulkan::VulkanRenderOp> operations = {
        ri::render::vulkan::VulkanRenderOp{
            .type = ri::render::vulkan::VulkanRenderOpType::BeginBatch,
            .passIndex = 0,
            .pipelineBucket = 1,
        },
        ri::render::vulkan::VulkanRenderOp{
            .type = ri::render::vulkan::VulkanRenderOpType::ClearColor,
            .passIndex = 0,
            .pipelineBucket = 1,
            .clearColor = {0.25f, 0.5f, 0.75f, 1.0f},
        },
        ri::render::vulkan::VulkanRenderOp{
            .type = ri::render::vulkan::VulkanRenderOpType::SetViewProjection,
            .passIndex = 0,
            .pipelineBucket = 1,
        },
        ri::render::vulkan::VulkanRenderOp{
            .type = ri::render::vulkan::VulkanRenderOpType::DrawMesh,
            .passIndex = 0,
            .pipelineBucket = 1,
            .meshHandle = 9,
            .materialHandle = 3,
            .firstIndex = 12,
            .indexCount = 36,
            .instanceCount = 2,
        },
        ri::render::vulkan::VulkanRenderOp{
            .type = ri::render::vulkan::VulkanRenderOpType::EndBatch,
            .passIndex = 0,
            .pipelineBucket = 1,
        },
    };

    CapturingRecorder recorder;
    Expect(ri::render::vulkan::RecordVulkanCommandList(operations, recorder),
           "Vulkan command recorder should consume valid command lists");
    Expect(recorder.log.size() == 5U
               && recorder.log[0] == "begin:0:1"
               && recorder.log[1] == "clear"
               && recorder.log[2] == "set_view_projection"
               && recorder.log[3] == "draw:9"
               && recorder.log[4] == "end:0:1",
           "Vulkan command recorder should preserve operation order");
    Expect(NearlyEqual(recorder.lastClearR, 0.25f) && NearlyEqual(recorder.lastClearA, 1.0f),
           "Vulkan command recorder should pass clear color values");
    Expect(recorder.lastDrawMesh == 9 && recorder.lastDrawMaterial == 3
               && recorder.lastDrawFirstIndex == 12
               && recorder.lastDrawIndexCount == 36
               && recorder.lastDrawInstanceCount == 2,
           "Vulkan command recorder should pass draw arguments");

    operations.push_back(ri::render::vulkan::VulkanRenderOp{
        .type = ri::render::vulkan::VulkanRenderOpType::Unknown,
    });
    Expect(!ri::render::vulkan::RecordVulkanCommandList(operations, recorder),
           "Vulkan command recorder should reject unknown operations");
}

void TestVulkanCommandBufferRecorder() {
    ri::render::vulkan::VulkanCommandBufferRecorder recorder;

    Expect(!recorder.InBatch(), "Vulkan command-buffer recorder should start outside a batch");
    Expect(!recorder.EndBatch(0U, 1U),
           "Vulkan command-buffer recorder should reject batch end before batch start");
    Expect(!recorder.DrawMesh(1, 2, 0U, 3U, 1U, kIdentityMatrix4x4),
           "Vulkan command-buffer recorder should reject draw calls outside a batch");

    Expect(recorder.BeginBatch(2U, 7U),
           "Vulkan command-buffer recorder should start a batch");
    Expect(recorder.InBatch(), "Vulkan command-buffer recorder should track active batch state");
    const float clearColor[4] = {0.2f, 0.3f, 0.4f, 1.0f};
    Expect(recorder.ClearColor(clearColor),
           "Vulkan command-buffer recorder should record clear commands in batch");
    Expect(recorder.SetViewProjection(kIdentityMatrix4x4),
           "Vulkan command-buffer recorder should record camera setup commands in batch");
    Expect(recorder.DrawMesh(11, 5, 6U, 36U, 2U, kIdentityMatrix4x4),
           "Vulkan command-buffer recorder should record draw commands in batch");
    Expect(!recorder.EndBatch(2U, 8U),
           "Vulkan command-buffer recorder should reject mismatched batch end metadata");
    Expect(recorder.EndBatch(2U, 7U),
           "Vulkan command-buffer recorder should end the matching batch");
    Expect(!recorder.InBatch(), "Vulkan command-buffer recorder should clear active batch state on end");

    const auto& intents = recorder.Intents();
    Expect(intents.size() == 5U,
           "Vulkan command-buffer recorder should emit begin/commands/end intents");
    Expect(intents[0].type == ri::render::vulkan::VulkanCommandIntentType::BeginBatch
               && intents[1].type == ri::render::vulkan::VulkanCommandIntentType::ClearColor
               && intents[2].type == ri::render::vulkan::VulkanCommandIntentType::SetViewProjection
               && intents[3].type == ri::render::vulkan::VulkanCommandIntentType::DrawMesh
               && intents[4].type == ri::render::vulkan::VulkanCommandIntentType::EndBatch,
           "Vulkan command-buffer recorder should preserve batch-local operation ordering");
    Expect(NearlyEqual(intents[1].clearColor[0], 0.2f) && NearlyEqual(intents[1].clearColor[3], 1.0f),
           "Vulkan command-buffer recorder should preserve clear color arguments");
    Expect(intents[3].meshHandle == 11 && intents[3].materialHandle == 5
               && intents[3].firstIndex == 6U
               && intents[3].indexCount == 36U
               && intents[3].instanceCount == 2U,
           "Vulkan command-buffer recorder should preserve draw arguments");

    recorder.Reset();
    Expect(!recorder.InBatch() && recorder.Intents().empty(),
           "Vulkan command-buffer recorder reset should clear state and intents");
}

void TestVulkanIntentStagingPlan() {
    ri::render::vulkan::VulkanCommandBufferRecorder recorder;
    Expect(recorder.BeginBatch(0U, 1U), "Intent staging setup should begin first batch");
    const float clearA[4] = {0.1f, 0.2f, 0.3f, 1.0f};
    Expect(recorder.ClearColor(clearA), "Intent staging setup should record clear");
    Expect(recorder.SetViewProjection(kIdentityMatrix4x4), "Intent staging setup should record view projection");
    Expect(recorder.EndBatch(0U, 1U), "Intent staging setup should end first batch");

    Expect(recorder.BeginBatch(1U, 4U), "Intent staging setup should begin second batch");
    Expect(recorder.DrawMesh(3, 2, 0U, 36U, 1U, kIdentityMatrix4x4), "Intent staging setup should record draw");
    Expect(recorder.DrawMesh(5, 2, 36U, 36U, 2U, kIdentityMatrix4x4), "Intent staging setup should record second draw");
    Expect(recorder.EndBatch(1U, 4U), "Intent staging setup should end second batch");

    const auto plan = ri::render::vulkan::BuildVulkanIntentStagingPlan(recorder.Intents());
    Expect(plan.status == ri::render::vulkan::VulkanIntentStagingStatus::Ok,
           "Intent staging should accept valid recorder output");
    Expect(plan.totalIntents == 8U && plan.stagedIntents == 4U,
           "Intent staging should track total and staged command counts");
    Expect(plan.ranges.size() == 2U,
           "Intent staging should produce one range per batch");
    Expect(plan.ranges[0].passIndex == 0U && plan.ranges[0].pipelineBucket == 1U
               && plan.ranges[0].intentCount == 2U
               && plan.ranges[0].clearCount == 1U
               && plan.ranges[0].setViewProjectionCount == 1U
               && plan.ranges[0].drawCount == 0U,
           "Intent staging should classify first batch command counts");
    Expect(plan.ranges[1].passIndex == 1U && plan.ranges[1].pipelineBucket == 4U
               && plan.ranges[1].intentCount == 2U
               && plan.ranges[1].clearCount == 0U
               && plan.ranges[1].setViewProjectionCount == 0U
               && plan.ranges[1].drawCount == 2U,
           "Intent staging should classify second batch command counts");

    std::vector<ri::render::vulkan::VulkanCommandIntent> invalid = {
        ri::render::vulkan::VulkanCommandIntent{
            .type = ri::render::vulkan::VulkanCommandIntentType::BeginBatch,
            .passIndex = 0U,
            .pipelineBucket = 1U,
        },
        ri::render::vulkan::VulkanCommandIntent{
            .type = ri::render::vulkan::VulkanCommandIntentType::DrawMesh,
            .passIndex = 0U,
            .pipelineBucket = 1U,
            .meshHandle = 1,
            .materialHandle = 1,
            .firstIndex = 0U,
            .indexCount = 6U,
            .instanceCount = 1U,
        },
        ri::render::vulkan::VulkanCommandIntent{
            .type = ri::render::vulkan::VulkanCommandIntentType::BeginBatch,
            .passIndex = 1U,
            .pipelineBucket = 2U,
        },
    };
    const auto invalidPlan = ri::render::vulkan::BuildVulkanIntentStagingPlan(invalid);
    Expect(invalidPlan.status == ri::render::vulkan::VulkanIntentStagingStatus::UnexpectedBeginBatch,
           "Intent staging should reject nested batch starts");
}

void TestVulkanFrameSubmission() {
    ri::render::vulkan::VulkanCommandBufferRecorder source;
    Expect(source.BeginBatch(0U, 2U), "Frame submission setup should begin pass-0 batch");
    const float clearA[4] = {0.05f, 0.1f, 0.2f, 1.0f};
    Expect(source.ClearColor(clearA), "Frame submission setup should record clear in first batch");
    Expect(source.SetViewProjection(kIdentityMatrix4x4), "Frame submission setup should record camera setup in first batch");
    Expect(source.EndBatch(0U, 2U), "Frame submission setup should end pass-0 batch");

    Expect(source.BeginBatch(1U, 3U), "Frame submission setup should begin pass-1 batch");
    Expect(source.DrawMesh(7, 4, 0U, 36U, 1U, kIdentityMatrix4x4), "Frame submission setup should record draw in second batch");
    Expect(source.EndBatch(1U, 3U), "Frame submission setup should end pass-1 batch");

    const auto intents = source.Intents();
    const auto plan = ri::render::vulkan::BuildVulkanIntentStagingPlan(intents);
    Expect(plan.status == ri::render::vulkan::VulkanIntentStagingStatus::Ok,
           "Frame submission should require valid staging plan input");

    ri::render::vulkan::VulkanCommandBufferRecorder destination;
    ri::render::vulkan::VulkanFrameSubmissionStats stats{};
    Expect(ri::render::vulkan::ExecuteVulkanFrameSubmission(intents, plan, destination, {}, &stats),
           "Frame submission should replay staged intents into backend recorder");
    Expect(stats.rangesVisited == 2U && stats.rangesSubmitted == 2U && stats.commandsSubmitted == 3U,
           "Frame submission stats should reflect all staged ranges and commands");

    const auto& replayed = destination.Intents();
    Expect(replayed.size() == 7U,
           "Frame submission replay should emit begin/end markers plus staged commands");
    Expect(replayed[0].type == ri::render::vulkan::VulkanCommandIntentType::BeginBatch
               && replayed[1].type == ri::render::vulkan::VulkanCommandIntentType::ClearColor
               && replayed[2].type == ri::render::vulkan::VulkanCommandIntentType::SetViewProjection
               && replayed[3].type == ri::render::vulkan::VulkanCommandIntentType::EndBatch
               && replayed[4].type == ri::render::vulkan::VulkanCommandIntentType::BeginBatch
               && replayed[5].type == ri::render::vulkan::VulkanCommandIntentType::DrawMesh
               && replayed[6].type == ri::render::vulkan::VulkanCommandIntentType::EndBatch,
           "Frame submission replay should preserve range ordering and command ordering");

    ri::render::vulkan::VulkanCommandBufferRecorder filteredDestination;
    Expect(ri::render::vulkan::ExecuteVulkanFrameSubmission(
               intents,
               plan,
               filteredDestination,
               ri::render::vulkan::VulkanSubmissionPassFilter{.minPassIndex = 1U, .maxPassIndex = 1U},
               &stats),
           "Frame submission should support pass-range filtering");
    Expect(stats.rangesVisited == 2U && stats.rangesSubmitted == 1U && stats.commandsSubmitted == 1U,
           "Frame submission filtering should only submit matching pass ranges");

    auto invalidPlan = plan;
    invalidPlan.status = ri::render::vulkan::VulkanIntentStagingStatus::CommandOutsideBatch;
    Expect(!ri::render::vulkan::ExecuteVulkanFrameSubmission(intents, invalidPlan, destination),
           "Frame submission should reject invalid staging plan statuses");

    ri::render::vulkan::VulkanPipelineStateCache cache;
    int resolveCalls = 0;
    const auto resolver = [&](const ri::render::vulkan::VulkanPipelineStateKey& key)
        -> std::optional<ri::render::vulkan::VulkanPipelineStateRecord> {
        resolveCalls += 1;
        return ri::render::vulkan::VulkanPipelineStateRecord{
            .pipelineHandle = static_cast<std::uint64_t>(100U + key.pipelineBucket),
            .layoutHandle = static_cast<std::uint64_t>(200U + key.materialBucket),
        };
    };

    ri::render::vulkan::VulkanCommandBufferRecorder cachedDestination;
    Expect(ri::render::vulkan::ExecuteVulkanFrameSubmissionWithPipelineCache(
               intents,
               plan,
               cachedDestination,
               cache,
               resolver,
               {},
               &stats),
           "Cached frame submission should execute valid staged intents");
    Expect(stats.commandsSubmitted == 3U && stats.pipelineResolves == 1U && stats.pipelineResolveFailures == 0U,
           "Cached frame submission stats should report resolve attempts and success");
    Expect(resolveCalls == 1, "Cached frame submission should resolve one unique draw pipeline state");

    ri::render::vulkan::VulkanCommandBufferRecorder cachedDestinationRepeat;
    Expect(ri::render::vulkan::ExecuteVulkanFrameSubmissionWithPipelineCache(
               intents,
               plan,
               cachedDestinationRepeat,
               cache,
               resolver,
               {},
               &stats),
           "Cached frame submission should support repeated execution with warm cache");
    Expect(resolveCalls == 1, "Warm pipeline cache should avoid duplicate resolver calls");

    const auto failingResolver = [](const ri::render::vulkan::VulkanPipelineStateKey&)
        -> std::optional<ri::render::vulkan::VulkanPipelineStateRecord> {
        return std::nullopt;
    };
    ri::render::vulkan::VulkanPipelineStateCache emptyCache;
    Expect(!ri::render::vulkan::ExecuteVulkanFrameSubmissionWithPipelineCache(
               intents,
               plan,
               cachedDestination,
               emptyCache,
               failingResolver,
               {},
               &stats),
           "Cached frame submission should fail when pipeline state resolution fails");
    Expect(stats.pipelineResolveFailures > 0U,
           "Cached frame submission should track resolve failures");
}

void TestVulkanPipelineStateCache() {
    ri::render::vulkan::VulkanPipelineStateCache cache;
    const ri::render::vulkan::VulkanPipelineStateKey keyA{
        .passIndex = 1U,
        .pipelineBucket = 7U,
        .materialBucket = 2U,
    };
    const ri::render::vulkan::VulkanPipelineStateKey keyB{
        .passIndex = 1U,
        .pipelineBucket = 7U,
        .materialBucket = 5U,
    };

    int resolveCalls = 0;
    const auto resolver = [&](const ri::render::vulkan::VulkanPipelineStateKey& key)
        -> std::optional<ri::render::vulkan::VulkanPipelineStateRecord> {
        resolveCalls += 1;
        return ri::render::vulkan::VulkanPipelineStateRecord{
            .pipelineHandle = static_cast<std::uint64_t>(1000 + key.materialBucket),
            .layoutHandle = static_cast<std::uint64_t>(2000 + key.pipelineBucket),
        };
    };

    const auto firstA = cache.Resolve(keyA, resolver);
    Expect(firstA.has_value(), "Pipeline cache should resolve first miss using provided resolver");
    Expect(firstA->pipelineHandle == 1002U && firstA->layoutHandle == 2007U,
           "Pipeline cache should preserve resolved backend handles");

    const auto secondA = cache.Resolve(keyA, resolver);
    Expect(secondA.has_value() && secondA->pipelineHandle == firstA->pipelineHandle,
           "Pipeline cache should return cached value on repeated key");
    Expect(resolveCalls == 1, "Pipeline cache should not call resolver on cache hits");

    const auto firstB = cache.Resolve(keyB, resolver);
    Expect(firstB.has_value() && firstB->pipelineHandle == 1005U,
           "Pipeline cache should resolve distinct keys independently");
    Expect(resolveCalls == 2, "Pipeline cache should call resolver for each distinct miss");

    const auto lookedUpA = cache.Lookup(keyA);
    Expect(lookedUpA.has_value() && lookedUpA->layoutHandle == 2007U,
           "Pipeline cache lookup should return cached entries without resolver");

    const auto missing = cache.Lookup(ri::render::vulkan::VulkanPipelineStateKey{
        .passIndex = 9U,
        .pipelineBucket = 9U,
        .materialBucket = 9U,
    });
    Expect(!missing.has_value(), "Pipeline cache lookup should return nullopt for missing keys");

    const auto stats = cache.Stats();
    Expect(stats.lookups == 3U && stats.hits == 1U && stats.misses == 2U && stats.stored == 2U,
           "Pipeline cache stats should track native cache lifecycle accurately");

    cache.Clear();
    Expect(!cache.Lookup(keyA).has_value(), "Pipeline cache clear should drop all stored states");
    Expect(cache.Stats().stored == 0U, "Pipeline cache clear should reset stored-count stat");
}

void TestWorldRuntimeState() {
    ri::world::RuntimeVolume box{};
    box.shape = ri::world::VolumeShape::Box;
    box.position = {0.0f, 0.0f, 0.0f};
    box.size = {4.0f, 4.0f, 4.0f};
    Expect(ri::world::IsPointInsideVolume({1.0f, 1.0f, 1.0f}, box),
           "World runtime volume checks should accept points inside box volumes");
    Expect(!ri::world::IsPointInsideVolume({3.0f, 0.0f, 0.0f}, box),
           "World runtime volume checks should reject points outside box volumes");

    ri::world::RuntimeVolume cylinder{};
    cylinder.shape = ri::world::VolumeShape::Cylinder;
    cylinder.position = {0.0f, 0.0f, 0.0f};
    cylinder.radius = 2.0f;
    cylinder.height = 4.0f;
    Expect(ri::world::IsPointInsideVolume({1.0f, 1.0f, 1.0f}, cylinder),
           "World runtime volume checks should accept points inside cylinder volumes");
    Expect(!ri::world::IsPointInsideVolume({2.1f, 0.0f, 0.0f}, cylinder),
           "World runtime volume checks should reject points outside cylinder radius");

    ri::world::RuntimeVolume sphere{};
    sphere.shape = ri::world::VolumeShape::Sphere;
    sphere.position = {0.0f, 0.0f, 0.0f};
    sphere.radius = 2.0f;
    Expect(ri::world::IsPointInsideVolume({1.0f, 1.0f, 0.0f}, sphere),
           "World runtime volume checks should accept points inside sphere volumes");
    Expect(!ri::world::IsPointInsideVolume({3.0f, 0.0f, 0.0f}, sphere),
           "World runtime volume checks should reject points outside sphere volumes");

    ri::world::RuntimeEnvironmentService environmentService;

    ri::world::PostProcessVolume postA{};
    postA.id = "pp_a";
    postA.shape = ri::world::VolumeShape::Box;
    postA.position = {0.0f, 0.0f, 0.0f};
    postA.size = {4.0f, 4.0f, 4.0f};
    postA.tintColor = {1.0f, 0.0f, 0.0f};
    postA.tintStrength = 0.2f;
    postA.blurAmount = 0.003f;
    postA.noiseAmount = 0.05f;

    ri::world::PostProcessVolume postB{};
    postB.id = "pp_b";
    postB.shape = ri::world::VolumeShape::Box;
    postB.position = {0.0f, 0.0f, 0.0f};
    postB.size = {4.0f, 4.0f, 4.0f};
    postB.tintColor = {0.0f, 0.0f, 1.0f};
    postB.tintStrength = 0.4f;
    postB.blurAmount = 0.001f;
    postB.noiseAmount = 0.10f;
    postB.scanlineAmount = 0.02f;
    postB.barrelDistortion = 0.01f;
    postB.chromaticAberration = 0.005f;
    environmentService.SetPostProcessVolumes({postA, postB});


    ri::world::LocalizedFogVolume fog{};
    fog.id = "fog_a";
    fog.shape = ri::world::VolumeShape::Box;
    fog.position = {0.0f, 0.0f, 0.0f};
    fog.size = {4.0f, 4.0f, 4.0f};
    fog.tintColor = {0.2f, 0.8f, 0.4f};
    fog.tintStrength = 0.5f;
    fog.blurAmount = 0.004f;
    environmentService.SetLocalizedFogVolumes({fog});

    ri::world::FluidSimulationVolume fluid{};
    fluid.id = "fluid_a";
    fluid.shape = ri::world::VolumeShape::Box;
    fluid.position = {0.0f, 0.0f, 0.0f};
    fluid.size = {4.0f, 4.0f, 4.0f};
    fluid.gravityScale = 0.5f;
    fluid.jumpScale = 0.8f;
    fluid.drag = 2.0f;
    fluid.buoyancy = 1.2f;
    fluid.flow = {1.0f, 0.0f, -0.5f};
    fluid.tintColor = {0.42f, 0.74f, 1.0f};
    fluid.tintStrength = 0.7f;
    fluid.reverbMix = 0.25f;
    fluid.echoDelayMs = 90.0f;
    environmentService.SetFluidSimulationVolumes({fluid});

    ri::world::PhysicsModifierVolume customGravity{};
    customGravity.id = "gravity_a";
    customGravity.shape = ri::world::VolumeShape::Box;
    customGravity.position = {0.0f, 0.0f, 0.0f};
    customGravity.size = {4.0f, 4.0f, 4.0f};
    customGravity.gravityScale = 0.8f;
    customGravity.jumpScale = 0.9f;
    customGravity.drag = 0.6f;
    customGravity.buoyancy = 0.3f;
    customGravity.flow = {0.2f, 0.0f, 0.1f};

    ri::world::PhysicsModifierVolume directionalWind{};
    directionalWind.id = "wind_a";
    directionalWind.shape = ri::world::VolumeShape::Box;
    directionalWind.position = {0.0f, 0.0f, 0.0f};
    directionalWind.size = {4.0f, 4.0f, 4.0f};
    directionalWind.gravityScale = 1.0f;
    directionalWind.jumpScale = 1.0f;
    directionalWind.drag = 0.4f;
    directionalWind.buoyancy = 0.0f;
    directionalWind.flow = {2.0f, 0.0f, 0.0f};
    environmentService.SetPhysicsModifierVolumes({customGravity, directionalWind});

    ri::world::SurfaceVelocityPrimitive conveyor{};
    conveyor.id = "surface_a";
    conveyor.shape = ri::world::VolumeShape::Box;
    conveyor.position = {0.0f, 0.0f, 0.0f};
    conveyor.size = {4.0f, 4.0f, 4.0f};
    conveyor.flow = {0.0f, 0.0f, 1.5f};
    environmentService.SetSurfaceVelocityPrimitives({conveyor});

    ri::world::RadialForceVolume radial{};
    radial.id = "radial_a";
    radial.shape = ri::world::VolumeShape::Sphere;
    radial.position = {0.0f, 0.0f, 0.0f};
    radial.radius = 4.0f;
    radial.strength = 4.0f;
    radial.falloff = 1.0f;
    environmentService.SetRadialForceVolumes({radial});

    ri::world::PhysicsConstraintVolume constraint{};
    constraint.id = "constraint_a";
    constraint.shape = ri::world::VolumeShape::Box;
    constraint.position = {0.0f, 0.0f, 0.0f};
    constraint.size = {4.0f, 4.0f, 4.0f};
    constraint.lockAxes = {ri::world::ConstraintAxis::X, ri::world::ConstraintAxis::Z};
    environmentService.SetPhysicsConstraintVolumes({constraint});

    ri::world::WaterSurfacePrimitive waterSurface{};
    waterSurface.id = "water_surface_a";
    waterSurface.type = "water_surface_primitive";
    waterSurface.shape = ri::world::VolumeShape::Box;
    waterSurface.position = {0.0f, 0.0f, 0.0f};
    waterSurface.size = {8.0f, 2.0f, 8.0f};
    waterSurface.waveAmplitude = 0.15f;
    waterSurface.waveFrequency = 0.5f;
    waterSurface.flowSpeed = 1.2f;
    waterSurface.blocksUnderwaterFog = true;
    environmentService.SetWaterSurfacePrimitives({waterSurface});

    ri::world::KinematicTranslationPrimitive kinematicTranslation{};
    kinematicTranslation.id = "kin_tx_a";
    kinematicTranslation.type = "kinematic_translation_primitive";
    kinematicTranslation.shape = ri::world::VolumeShape::Box;
    kinematicTranslation.position = {0.0f, 0.0f, 0.0f};
    kinematicTranslation.size = {5.0f, 3.0f, 5.0f};
    kinematicTranslation.axis = {1.0f, 0.0f, 0.0f};
    kinematicTranslation.distance = 4.0f;
    kinematicTranslation.cycleSeconds = 4.0f;
    kinematicTranslation.pingPong = true;
    environmentService.SetKinematicTranslationPrimitives({kinematicTranslation});

    ri::world::KinematicRotationPrimitive kinematicRotation{};
    kinematicRotation.id = "kin_rot_a";
    kinematicRotation.type = "kinematic_rotation_primitive";
    kinematicRotation.shape = ri::world::VolumeShape::Box;
    kinematicRotation.position = {0.0f, 0.0f, 0.0f};
    kinematicRotation.size = {5.0f, 3.0f, 5.0f};
    kinematicRotation.axis = {0.0f, 1.0f, 0.0f};
    kinematicRotation.angularSpeedDegreesPerSecond = 90.0f;
    kinematicRotation.maxAngleDegrees = 120.0f;
    kinematicRotation.pingPong = true;
    environmentService.SetKinematicRotationPrimitives({kinematicRotation});

    ri::world::SplinePathFollowerPrimitive splineFollower{};
    splineFollower.id = "spline_follow_a";
    splineFollower.type = "spline_path_follower_primitive";
    splineFollower.shape = ri::world::VolumeShape::Box;
    splineFollower.position = {0.0f, 0.0f, 0.0f};
    splineFollower.size = {5.0f, 3.0f, 5.0f};
    splineFollower.splinePoints = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 2.0f}};
    splineFollower.speedUnitsPerSecond = 3.5f;
    splineFollower.loop = true;
    environmentService.SetSplinePathFollowerPrimitives({splineFollower});

    ri::world::CablePrimitive cable{};
    cable.id = "cable_a";
    cable.type = "cable_primitive";
    cable.shape = ri::world::VolumeShape::Box;
    cable.position = {0.0f, 0.0f, 0.0f};
    cable.size = {5.0f, 3.0f, 5.0f};
    cable.start = {0.0f, 1.0f, 0.0f};
    cable.end = {0.0f, -2.0f, 0.0f};
    cable.swayAmplitude = 0.25f;
    cable.swayFrequency = 0.9f;
    environmentService.SetCablePrimitives({cable});

    ri::world::ClippingRuntimeVolume clipping{};
    clipping.id = "clip_a";
    clipping.type = "clipping_volume";
    clipping.shape = ri::world::VolumeShape::Box;
    clipping.position = {0.0f, 0.0f, 0.0f};
    clipping.size = {5.0f, 3.0f, 5.0f};
    clipping.modes = {"visibility", "collision"};
    clipping.enabled = true;
    environmentService.SetClippingVolumes({clipping});

    ri::world::FilteredCollisionRuntimeVolume filteredCollision{};
    filteredCollision.id = "filtered_collision_runtime_a";
    filteredCollision.type = "filtered_collision_volume";
    filteredCollision.shape = ri::world::VolumeShape::Box;
    filteredCollision.position = {0.0f, 0.0f, 0.0f};
    filteredCollision.size = {5.0f, 3.0f, 5.0f};
    filteredCollision.channels = {"player", "camera"};
    environmentService.SetFilteredCollisionVolumes({filteredCollision});

    ri::world::CameraBlockingVolume cameraBlocker{};
    cameraBlocker.id = "camera_blocker_a";
    cameraBlocker.type = "camera_blocking_volume";
    cameraBlocker.shape = ri::world::VolumeShape::Box;
    cameraBlocker.position = {0.0f, 0.0f, 0.0f};
    cameraBlocker.size = {4.0f, 4.0f, 4.0f};
    cameraBlocker.channels = {"camera"};
    environmentService.SetCameraBlockingVolumes({cameraBlocker});

    ri::world::AiPerceptionBlockerVolume aiBlocker{};
    aiBlocker.id = "ai_blocker_a";
    aiBlocker.type = "ai_perception_blocker_volume";
    aiBlocker.shape = ri::world::VolumeShape::Box;
    aiBlocker.position = {0.0f, 0.0f, 0.0f};
    aiBlocker.size = {4.0f, 4.0f, 4.0f};
    aiBlocker.modes = {"ai"};
    aiBlocker.enabled = true;
    environmentService.SetAiPerceptionBlockerVolumes({aiBlocker});

    ri::world::SafeZoneRuntimeVolume safeZone{};
    safeZone.id = "safe_zone_a";
    safeZone.type = "safe_zone_volume";
    safeZone.shape = ri::world::VolumeShape::Box;
    safeZone.position = {0.0f, 0.0f, 0.0f};
    safeZone.size = {6.0f, 4.0f, 6.0f};
    safeZone.dropAggro = true;
    environmentService.SetSafeZoneVolumes({safeZone});

    ri::world::TraversalLinkVolume ladder{};
    ladder.id = "ladder_a";
    ladder.type = "ladder_volume";
    ladder.kind = ri::world::TraversalLinkKind::Ladder;
    ladder.shape = ri::world::VolumeShape::Box;
    ladder.position = {0.0f, 0.0f, 0.0f};
    ladder.size = {2.0f, 6.0f, 2.0f};
    ladder.climbSpeed = 4.8f;
    environmentService.SetTraversalLinkVolumes({ladder});

    ri::world::PivotAnchorPrimitive pivotAnchor{};
    pivotAnchor.id = "pivot_anchor_a";
    pivotAnchor.type = "pivot_anchor_primitive";
    pivotAnchor.shape = ri::world::VolumeShape::Box;
    pivotAnchor.position = {0.0f, 0.0f, 0.0f};
    pivotAnchor.size = {3.0f, 3.0f, 3.0f};
    pivotAnchor.anchorId = "door_hinge_anchor";
    pivotAnchor.forwardAxis = {0.0f, 1.0f, 0.0f};
    pivotAnchor.alignToSurfaceNormal = true;
    environmentService.SetPivotAnchorPrimitives({pivotAnchor});

    ri::world::SymmetryMirrorPlane symmetryPlane{};
    symmetryPlane.id = "symmetry_plane_a";
    symmetryPlane.type = "symmetry_mirror_plane";
    symmetryPlane.shape = ri::world::VolumeShape::Box;
    symmetryPlane.position = {0.0f, 0.0f, 0.0f};
    symmetryPlane.size = {10.0f, 10.0f, 4.0f};
    symmetryPlane.planeNormal = {0.0f, 0.0f, 1.0f};
    symmetryPlane.planeOffset = 0.5f;
    symmetryPlane.keepOriginal = false;
    symmetryPlane.snapToGrid = true;
    environmentService.SetSymmetryMirrorPlanes({symmetryPlane});

    ri::world::SpatialQueryVolume spatialQuery{};
    spatialQuery.id = "query_a";
    spatialQuery.type = "spatial_query_volume";
    spatialQuery.shape = ri::world::VolumeShape::Box;
    spatialQuery.position = {0.0f, 0.0f, 0.0f};
    spatialQuery.size = {4.0f, 4.0f, 4.0f};
    spatialQuery.filterMask = 0x4U;
    spatialQuery.broadcastFrequency = 0.25;
    environmentService.SetSpatialQueryVolumes({spatialQuery});

    ri::world::LocalGridSnapVolume snap{};
    snap.id = "snap_a";
    snap.shape = ri::world::VolumeShape::Box;
    snap.position = {0.0f, 0.0f, 0.0f};
    snap.size = {6.0f, 4.0f, 6.0f};
    snap.snapSize = 0.5f;
    environmentService.SetLocalGridSnapVolumes({snap});

    ri::world::HintPartitionVolume hintPartition{};
    hintPartition.id = "hint_a";
    hintPartition.shape = ri::world::VolumeShape::Box;
    hintPartition.position = {0.0f, 0.0f, 0.0f};
    hintPartition.size = {6.0f, 4.0f, 6.0f};
    hintPartition.mode = ri::world::HintPartitionMode::Skip;
    environmentService.SetHintPartitionVolumes({hintPartition});
    ri::world::DoorWindowCutoutPrimitive cutout{};
    cutout.id = "cutout_a";
    cutout.type = "door_window_cutout";
    cutout.shape = ri::world::VolumeShape::Box;
    cutout.position = {0.0f, 0.0f, 0.0f};
    cutout.size = {4.0f, 4.0f, 1.0f};
    cutout.openingWidth = 1.6f;
    cutout.openingHeight = 2.3f;
    cutout.sillHeight = 0.4f;
    cutout.lintelHeight = 2.5f;
    cutout.carveCollision = true;
    cutout.carveVisual = true;
    environmentService.SetDoorWindowCutoutPrimitives({cutout});

    ri::world::CameraConfinementVolume cameraBox{};
    cameraBox.id = "camera_box_a";
    cameraBox.shape = ri::world::VolumeShape::Box;
    cameraBox.position = {0.0f, 0.0f, 0.0f};
    cameraBox.size = {6.0f, 4.0f, 6.0f};
    environmentService.SetCameraConfinementVolumes({cameraBox});

    ri::world::LodOverrideVolume lodOverride{};
    lodOverride.id = "lod_box_a";
    lodOverride.shape = ri::world::VolumeShape::Box;
    lodOverride.position = {0.0f, 0.0f, 0.0f};
    lodOverride.size = {8.0f, 6.0f, 8.0f};
    lodOverride.targetIds = {"mesh_a", "mesh_b"};
    lodOverride.forcedLod = ri::world::ForcedLod::Far;
    environmentService.SetLodOverrideVolumes({lodOverride});

    ri::world::LodSwitchPrimitive lodSwitch{};
    lodSwitch.id = "lod_switch_a";
    lodSwitch.type = "lod_switch_primitive";
    lodSwitch.shape = ri::world::VolumeShape::Box;
    lodSwitch.position = {0.0f, 0.0f, 0.0f};
    lodSwitch.size = {8.0f, 6.0f, 8.0f};
    lodSwitch.levels = {
        ri::world::LodSwitchLevel{
            .name = "near",
            .representation = {.kind = ri::world::LodSwitchRepresentationKind::Primitive, .payloadId = "mesh_near"},
            .collisionProfile = ri::world::LodSwitchCollisionProfile::Full,
            .distanceEnter = 0.0f,
            .distanceExit = 28.0f,
        },
        ri::world::LodSwitchLevel{
            .name = "far",
            .representation = {.kind = ri::world::LodSwitchRepresentationKind::Cluster, .payloadId = "cluster_far"},
            .collisionProfile = ri::world::LodSwitchCollisionProfile::Simplified,
            .distanceEnter = 24.0f,
            .distanceExit = 100000.0f,
        },
    };
    lodSwitch.policy.metric = ri::world::LodSwitchMetric::CameraDistance;
    lodSwitch.policy.hysteresisEnabled = true;
    lodSwitch.policy.transitionMode = ri::world::LodSwitchTransitionMode::Crossfade;
    lodSwitch.policy.crossfadeSeconds = 0.5f;
    environmentService.SetLodSwitchPrimitives({lodSwitch});
    environmentService.UpdateLodSwitchPrimitives({40.0f, 0.0f, 0.0f}, 1.0, 0.0f);

    ri::world::SurfaceScatterVolume surfaceScatter{};
    surfaceScatter.id = "surface_scatter_a";
    surfaceScatter.type = "surface_scatter_volume";
    surfaceScatter.shape = ri::world::VolumeShape::Box;
    surfaceScatter.position = {0.0f, 0.0f, 0.0f};
    surfaceScatter.size = {12.0f, 2.0f, 12.0f};
    surfaceScatter.targetIds = {"receiver_floor_a", "receiver_floor_b"};
    surfaceScatter.sourceRepresentation.kind = ri::world::SurfaceScatterRepresentationKind::Mesh;
    surfaceScatter.sourceRepresentation.payloadId = "mesh_pebble_a";
    surfaceScatter.density.count = 80U;
    surfaceScatter.density.maxPoints = 120U;
    surfaceScatter.distribution.seed = 9001U;
    surfaceScatter.distribution.minSeparation = 0.3f;
    surfaceScatter.collisionPolicy = ri::world::SurfaceScatterCollisionPolicy::Proxy;
    surfaceScatter.culling.maxActiveDistance = 30.0f;
    environmentService.SetSurfaceScatterVolumes({surfaceScatter});

    ri::world::SplineMeshDeformerPrimitive splineDeformer{};
    splineDeformer.id = "spline_deformer_a";
    splineDeformer.type = "spline_mesh_deformer";
    splineDeformer.shape = ri::world::VolumeShape::Box;
    splineDeformer.position = {0.0f, 0.0f, 0.0f};
    splineDeformer.size = {12.0f, 4.0f, 12.0f};
    splineDeformer.targetIds = {"receiver_floor_a"};
    splineDeformer.splinePoints = {
        {-4.0f, 0.0f, 0.0f},
        {0.0f, 0.3f, 3.0f},
        {4.0f, 0.0f, 0.0f},
    };
    splineDeformer.sampleCount = 18U;
    splineDeformer.maxSamples = 128U;
    splineDeformer.seed = 5150U;
    splineDeformer.collisionEnabled = true;
    splineDeformer.maxActiveDistance = 40.0f;
    environmentService.SetSplineMeshDeformerPrimitives({splineDeformer});

    ri::world::SplineDecalRibbonPrimitive splineRibbon{};
    splineRibbon.id = "spline_ribbon_a";
    splineRibbon.type = "spline_decal_ribbon";
    splineRibbon.shape = ri::world::VolumeShape::Box;
    splineRibbon.position = {0.0f, 0.0f, 0.0f};
    splineRibbon.size = {12.0f, 2.0f, 12.0f};
    splineRibbon.splinePoints = {
        {-3.0f, 0.0f, -2.0f},
        {0.0f, 0.0f, 2.0f},
        {3.0f, 0.0f, -1.0f},
    };
    splineRibbon.width = 2.0f;
    splineRibbon.tessellation = 20U;
    splineRibbon.maxSamples = 128U;
    splineRibbon.seed = 6160U;
    splineRibbon.transparentBlend = true;
    splineRibbon.depthWrite = false;
    splineRibbon.maxActiveDistance = 35.0f;
    environmentService.SetSplineDecalRibbonPrimitives({splineRibbon});

    ri::world::TopologicalUvRemapperVolume uvRemapperRuntime{};
    uvRemapperRuntime.id = "uv_remapper_runtime";
    uvRemapperRuntime.type = "topological_uv_remapper";
    uvRemapperRuntime.shape = ri::world::VolumeShape::Box;
    uvRemapperRuntime.position = {0.0f, 0.0f, 0.0f};
    uvRemapperRuntime.size = {12.0f, 4.0f, 12.0f};
    uvRemapperRuntime.targetIds = {"mesh_a", "mesh_b"};
    uvRemapperRuntime.sharedTextureId = "atlas_main";
    uvRemapperRuntime.maxMaterialPatches = 4U;
    uvRemapperRuntime.maxActiveDistance = 200.0f;
    environmentService.SetTopologicalUvRemapperVolumes({uvRemapperRuntime});

    ri::world::TriPlanarNode triPlanarRuntime{};
    triPlanarRuntime.id = "tri_planar_runtime";
    triPlanarRuntime.type = "tri_planar_node";
    triPlanarRuntime.shape = ri::world::VolumeShape::Box;
    triPlanarRuntime.position = {0.0f, 0.0f, 0.0f};
    triPlanarRuntime.size = {10.0f, 4.0f, 10.0f};
    triPlanarRuntime.targetIds = {"cliff_single"};
    triPlanarRuntime.textureX = "rock_x";
    triPlanarRuntime.textureY = "rock_y";
    triPlanarRuntime.textureZ = "rock_z";
    triPlanarRuntime.maxMaterialPatches = 8U;
    triPlanarRuntime.maxActiveDistance = 200.0f;
    environmentService.SetTriPlanarNodes({triPlanarRuntime});

    ri::world::InstanceCloudPrimitive instanceCloud{};
    instanceCloud.id = "instance_cloud_a";
    instanceCloud.type = "instance_cloud_primitive";
    instanceCloud.shape = ri::world::VolumeShape::Box;
    instanceCloud.position = {0.0f, 0.0f, 0.0f};
    instanceCloud.size = {10.0f, 4.0f, 10.0f};
    instanceCloud.sourceRepresentation.kind = ri::world::InstanceCloudRepresentationKind::Mesh;
    instanceCloud.sourceRepresentation.payloadId = "mesh_crate_a";
    instanceCloud.count = 64U;
    instanceCloud.seed = 4242U;
    instanceCloud.collisionPolicy = ri::world::InstanceCloudCollisionPolicy::Simplified;
    instanceCloud.culling.maxActiveDistance = 25.0f;
    instanceCloud.culling.frustumCulling = true;
    environmentService.SetInstanceCloudPrimitives({instanceCloud});

    ri::world::VoronoiFracturePrimitive voronoiFracture{};
    voronoiFracture.id = "voronoi_fracture_a";
    voronoiFracture.type = "voronoi_fracture_primitive";
    voronoiFracture.shape = ri::world::VolumeShape::Box;
    voronoiFracture.position = {0.0f, 0.0f, 0.0f};
    voronoiFracture.size = {10.0f, 4.0f, 10.0f};
    voronoiFracture.targetIds = {"fracture_target_a"};
    voronoiFracture.cellCount = 48U;
    environmentService.SetVoronoiFracturePrimitives({voronoiFracture});

    ri::world::MetaballPrimitive metaball{};
    metaball.id = "metaball_a";
    metaball.type = "metaball_primitive";
    metaball.shape = ri::world::VolumeShape::Sphere;
    metaball.position = {0.0f, 0.0f, 0.0f};
    metaball.size = {8.0f, 8.0f, 8.0f};
    metaball.controlPoints = {{-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    metaball.resolution = 32U;
    environmentService.SetMetaballPrimitives({metaball});

    ri::world::LatticeVolume lattice{};
    lattice.id = "lattice_a";
    lattice.type = "lattice_volume";
    lattice.shape = ri::world::VolumeShape::Box;
    lattice.position = {0.0f, 0.0f, 0.0f};
    lattice.size = {12.0f, 6.0f, 12.0f};
    lattice.targetIds = {"lattice_target_a"};
    lattice.maxCells = 2048U;
    environmentService.SetLatticeVolumes({lattice});

    ri::world::ManifoldSweepPrimitive manifoldSweep{};
    manifoldSweep.id = "manifold_sweep_a";
    manifoldSweep.type = "manifold_sweep";
    manifoldSweep.shape = ri::world::VolumeShape::Box;
    manifoldSweep.position = {0.0f, 0.0f, 0.0f};
    manifoldSweep.size = {10.0f, 4.0f, 10.0f};
    manifoldSweep.targetIds = {"pipe_target_a"};
    manifoldSweep.splinePoints = {{-3.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}};
    manifoldSweep.sampleCount = 40U;
    environmentService.SetManifoldSweepPrimitives({manifoldSweep});

    ri::world::TrimSheetSweepPrimitive trimSheetSweep{};
    trimSheetSweep.id = "trim_sheet_sweep_a";
    trimSheetSweep.type = "trim_sheet_sweep";
    trimSheetSweep.shape = ri::world::VolumeShape::Box;
    trimSheetSweep.position = {0.0f, 0.0f, 0.0f};
    trimSheetSweep.size = {10.0f, 2.0f, 10.0f};
    trimSheetSweep.targetIds = {"trim_target_a"};
    trimSheetSweep.splinePoints = {{-2.0f, 0.0f, -1.0f}, {2.0f, 0.0f, 1.0f}};
    trimSheetSweep.trimSheetId = "sheet_city";
    environmentService.SetTrimSheetSweepPrimitives({trimSheetSweep});

    ri::world::LSystemBranchPrimitive lSystemBranch{};
    lSystemBranch.id = "l_system_branch_a";
    lSystemBranch.type = "l_system_branch_primitive";
    lSystemBranch.shape = ri::world::VolumeShape::Box;
    lSystemBranch.position = {0.0f, 0.0f, 0.0f};
    lSystemBranch.size = {10.0f, 8.0f, 10.0f};
    lSystemBranch.targetIds = {"branch_target_a"};
    lSystemBranch.iterations = 5U;
    environmentService.SetLSystemBranchPrimitives({lSystemBranch});

    ri::world::GeodesicSpherePrimitive geodesicSphere{};
    geodesicSphere.id = "geodesic_sphere_a";
    geodesicSphere.type = "geodesic_sphere";
    geodesicSphere.shape = ri::world::VolumeShape::Sphere;
    geodesicSphere.position = {0.0f, 0.0f, 0.0f};
    geodesicSphere.size = {6.0f, 6.0f, 6.0f};
    geodesicSphere.subdivisionLevel = 3U;
    environmentService.SetGeodesicSpherePrimitives({geodesicSphere});

    ri::world::ExtrudeAlongNormalPrimitive extrudeAlongNormal{};
    extrudeAlongNormal.id = "extrude_normal_a";
    extrudeAlongNormal.type = "extrude_along_normal_primitive";
    extrudeAlongNormal.shape = ri::world::VolumeShape::Box;
    extrudeAlongNormal.position = {0.0f, 0.0f, 0.0f};
    extrudeAlongNormal.size = {8.0f, 4.0f, 8.0f};
    extrudeAlongNormal.targetIds = {"extrude_target_a"};
    extrudeAlongNormal.distance = 0.4f;
    extrudeAlongNormal.shellCount = 2U;
    environmentService.SetExtrudeAlongNormalPrimitives({extrudeAlongNormal});

    ri::world::SuperellipsoidPrimitive superellipsoid{};
    superellipsoid.id = "superellipsoid_a";
    superellipsoid.type = "superellipsoid";
    superellipsoid.shape = ri::world::VolumeShape::Sphere;
    superellipsoid.position = {0.0f, 0.0f, 0.0f};
    superellipsoid.size = {8.0f, 8.0f, 8.0f};
    superellipsoid.exponentX = 2.5f;
    superellipsoid.exponentY = 3.0f;
    superellipsoid.exponentZ = 2.0f;
    environmentService.SetSuperellipsoidPrimitives({superellipsoid});

    ri::world::PrimitiveDemoLattice primitiveDemoLattice{};
    primitiveDemoLattice.id = "primitive_demo_lattice_a";
    primitiveDemoLattice.type = "primitive_demo_lattice";
    primitiveDemoLattice.shape = ri::world::VolumeShape::Box;
    primitiveDemoLattice.position = {0.0f, 0.0f, 0.0f};
    primitiveDemoLattice.size = {8.0f, 4.0f, 8.0f};
    primitiveDemoLattice.targetIds = {"demo_lattice_target"};
    environmentService.SetPrimitiveDemoLatticePrimitives({primitiveDemoLattice});

    ri::world::PrimitiveDemoVoronoi primitiveDemoVoronoi{};
    primitiveDemoVoronoi.id = "primitive_demo_voronoi_a";
    primitiveDemoVoronoi.type = "primitive_demo_voronoi";
    primitiveDemoVoronoi.shape = ri::world::VolumeShape::Box;
    primitiveDemoVoronoi.position = {0.0f, 0.0f, 0.0f};
    primitiveDemoVoronoi.size = {8.0f, 4.0f, 8.0f};
    primitiveDemoVoronoi.targetIds = {"demo_voronoi_target"};
    environmentService.SetPrimitiveDemoVoronoiPrimitives({primitiveDemoVoronoi});

    ri::world::ThickPolygonPrimitive thickPolygon{};
    thickPolygon.id = "thick_polygon_a";
    thickPolygon.type = "thick_polygon_primitive";
    thickPolygon.shape = ri::world::VolumeShape::Box;
    thickPolygon.position = {0.0f, 0.0f, 0.0f};
    thickPolygon.size = {6.0f, 2.0f, 6.0f};
    thickPolygon.points = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    environmentService.SetThickPolygonPrimitives({thickPolygon});

    ri::world::StructuralProfilePrimitive structuralProfile{};
    structuralProfile.id = "structural_profile_a";
    structuralProfile.type = "structural_profile";
    structuralProfile.shape = ri::world::VolumeShape::Box;
    structuralProfile.position = {0.0f, 0.0f, 0.0f};
    structuralProfile.size = {6.0f, 4.0f, 6.0f};
    structuralProfile.profileId = "profile_arcade_wall";
    environmentService.SetStructuralProfilePrimitives({structuralProfile});

    ri::world::HalfPipePrimitive halfPipe{};
    halfPipe.id = "half_pipe_a";
    halfPipe.type = "half_pipe";
    halfPipe.shape = ri::world::VolumeShape::Box;
    halfPipe.position = {0.0f, 0.0f, 0.0f};
    halfPipe.size = {8.0f, 4.0f, 8.0f};
    environmentService.SetHalfPipePrimitives({halfPipe});

    ri::world::QuarterPipePrimitive quarterPipe{};
    quarterPipe.id = "quarter_pipe_a";
    quarterPipe.type = "quarter_pipe";
    quarterPipe.shape = ri::world::VolumeShape::Box;
    quarterPipe.position = {0.0f, 0.0f, 0.0f};
    quarterPipe.size = {8.0f, 4.0f, 8.0f};
    environmentService.SetQuarterPipePrimitives({quarterPipe});

    ri::world::PipeElbowPrimitive pipeElbow{};
    pipeElbow.id = "pipe_elbow_a";
    pipeElbow.type = "pipe_elbow";
    pipeElbow.shape = ri::world::VolumeShape::Box;
    pipeElbow.position = {0.0f, 0.0f, 0.0f};
    pipeElbow.size = {6.0f, 4.0f, 6.0f};
    environmentService.SetPipeElbowPrimitives({pipeElbow});

    ri::world::TorusSlicePrimitive torusSlice{};
    torusSlice.id = "torus_slice_a";
    torusSlice.type = "torus_slice";
    torusSlice.shape = ri::world::VolumeShape::Box;
    torusSlice.position = {0.0f, 0.0f, 0.0f};
    torusSlice.size = {8.0f, 4.0f, 8.0f};
    environmentService.SetTorusSlicePrimitives({torusSlice});

    ri::world::SplineSweepPrimitive splineSweep{};
    splineSweep.id = "spline_sweep_a";
    splineSweep.type = "spline_sweep";
    splineSweep.shape = ri::world::VolumeShape::Box;
    splineSweep.position = {0.0f, 0.0f, 0.0f};
    splineSweep.size = {8.0f, 4.0f, 8.0f};
    splineSweep.targetIds = {"spline_sweep_target"};
    splineSweep.splinePoints = {{-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}};
    environmentService.SetSplineSweepPrimitives({splineSweep});

    ri::world::RevolvePrimitive revolve{};
    revolve.id = "revolve_a";
    revolve.type = "revolve";
    revolve.shape = ri::world::VolumeShape::Box;
    revolve.position = {0.0f, 0.0f, 0.0f};
    revolve.size = {6.0f, 4.0f, 6.0f};
    revolve.profilePoints = {{1.0f, 0.0f, 0.0f}, {0.5f, 1.0f, 0.0f}};
    environmentService.SetRevolvePrimitives({revolve});

    ri::world::DomeVaultPrimitive domeVault{};
    domeVault.id = "dome_vault_a";
    domeVault.type = "dome_vault";
    domeVault.shape = ri::world::VolumeShape::Box;
    domeVault.position = {0.0f, 0.0f, 0.0f};
    domeVault.size = {10.0f, 5.0f, 10.0f};
    environmentService.SetDomeVaultPrimitives({domeVault});

    ri::world::LoftPrimitive loft{};
    loft.id = "loft_a";
    loft.type = "loft_primitive";
    loft.shape = ri::world::VolumeShape::Box;
    loft.position = {0.0f, 0.0f, 0.0f};
    loft.size = {8.0f, 4.0f, 8.0f};
    loft.pathPoints = {{-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}};
    loft.profilePoints = {{0.5f, 0.0f, 0.0f}, {0.0f, 0.5f, 0.0f}};
    environmentService.SetLoftPrimitives({loft});

    ri::world::NavmeshModifierVolume navmeshModifier{};
    navmeshModifier.id = "nav_box_a";
    navmeshModifier.shape = ri::world::VolumeShape::Box;
    navmeshModifier.position = {0.0f, 0.0f, 0.0f};
    navmeshModifier.size = {6.0f, 4.0f, 6.0f};
    navmeshModifier.traversalCost = 2.5f;
    navmeshModifier.tag = "slow";
    environmentService.SetNavmeshModifierVolumes({navmeshModifier});

    ri::world::AmbientAudioVolume ambientBox{};
    ambientBox.id = "ambient_box_a";
    ambientBox.type = "ambient_audio_volume";
    ambientBox.shape = ri::world::VolumeShape::Box;
    ambientBox.position = {0.0f, 0.0f, 0.0f};
    ambientBox.size = {8.0f, 4.0f, 8.0f};
    ambientBox.audioPath = "Assets/Audio/drone.ogg";
    ambientBox.baseVolume = 0.5f;
    ambientBox.maxDistance = 16.0f;
    ambientBox.label = "drone";

    ri::world::AmbientAudioVolume ambientSpline{};
    ambientSpline.id = "ambient_spline_a";
    ambientSpline.type = "ambient_audio_spline";
    ambientSpline.shape = ri::world::VolumeShape::Box;
    ambientSpline.position = {0.0f, 0.0f, 0.0f};
    ambientSpline.size = {8.0f, 4.0f, 8.0f};
    ambientSpline.audioPath = "Assets/Audio/wind.ogg";
    ambientSpline.baseVolume = 0.6f;
    ambientSpline.maxDistance = 20.0f;
    ambientSpline.label = "wind";
    ambientSpline.splinePoints = {
        {0.0f, 0.0f, 0.0f},
        {10.0f, 0.0f, 0.0f},
        {20.0f, 0.0f, 10.0f},
    };
    environmentService.SetAmbientAudioVolumes({ambientBox, ambientSpline});

    ri::world::ReflectionProbeVolume reflectionProbe{};
    reflectionProbe.id = "reflection_probe_a";
    reflectionProbe.shape = ri::world::VolumeShape::Box;
    reflectionProbe.position = {0.0f, 0.0f, 0.0f};
    reflectionProbe.size = {6.0f, 6.0f, 6.0f};
    environmentService.SetReflectionProbeVolumes({reflectionProbe});

    ri::world::LightImportanceVolume lightImportance{};
    lightImportance.id = "light_importance_a";
    lightImportance.shape = ri::world::VolumeShape::Box;
    lightImportance.position = {0.0f, 0.0f, 0.0f};
    lightImportance.size = {8.0f, 6.0f, 8.0f};
    lightImportance.probeGridBounds = true;
    environmentService.SetLightImportanceVolumes({lightImportance});

    ri::world::LightPortalVolume lightPortal{};
    lightPortal.id = "light_portal_a";
    lightPortal.shape = ri::world::VolumeShape::Box;
    lightPortal.position = {0.0f, 0.0f, 0.0f};
    lightPortal.size = {5.0f, 5.0f, 1.0f};
    environmentService.SetLightPortalVolumes({lightPortal});

    ri::world::VoxelGiBoundsVolume voxelGiBounds{};
    voxelGiBounds.id = "voxel_gi_a";
    voxelGiBounds.type = "voxel_gi_bounds";
    voxelGiBounds.shape = ri::world::VolumeShape::Box;
    voxelGiBounds.position = {0.0f, 0.0f, 0.0f};
    voxelGiBounds.size = {12.0f, 10.0f, 12.0f};
    voxelGiBounds.voxelSize = 0.5f;
    voxelGiBounds.cascadeCount = 3U;
    environmentService.SetVoxelGiBoundsVolumes({voxelGiBounds});

    ri::world::LightmapDensityVolume lightmapDensity{};
    lightmapDensity.id = "lightmap_density_a";
    lightmapDensity.type = "lightmap_density_volume";
    lightmapDensity.shape = ri::world::VolumeShape::Box;
    lightmapDensity.position = {0.0f, 0.0f, 0.0f};
    lightmapDensity.size = {10.0f, 6.0f, 10.0f};
    lightmapDensity.texelsPerMeter = 512.0f;
    lightmapDensity.minimumTexelsPerMeter = 128.0f;
    lightmapDensity.maximumTexelsPerMeter = 1024.0f;
    environmentService.SetLightmapDensityVolumes({lightmapDensity});

    ri::world::ShadowExclusionVolume shadowExclusion{};
    shadowExclusion.id = "shadow_exclusion_a";
    shadowExclusion.type = "shadow_exclusion_volume";
    shadowExclusion.shape = ri::world::VolumeShape::Box;
    shadowExclusion.position = {0.0f, 0.0f, 0.0f};
    shadowExclusion.size = {8.0f, 6.0f, 8.0f};
    shadowExclusion.affectVolumetricShadows = true;
    shadowExclusion.fadeDistance = 2.5f;
    environmentService.SetShadowExclusionVolumes({shadowExclusion});

    ri::world::CullingDistanceVolume cullingDistance{};
    cullingDistance.id = "culling_distance_a";
    cullingDistance.type = "culling_distance_volume";
    cullingDistance.shape = ri::world::VolumeShape::Box;
    cullingDistance.position = {0.0f, 0.0f, 0.0f};
    cullingDistance.size = {16.0f, 8.0f, 16.0f};
    cullingDistance.nearDistance = 5.0f;
    cullingDistance.farDistance = 240.0f;
    cullingDistance.allowHlod = false;
    environmentService.SetCullingDistanceVolumes({cullingDistance});

    ri::world::ReferenceImagePlane referencePlane{};
    referencePlane.id = "reference_plane_a";
    referencePlane.type = "reference_image_plane";
    referencePlane.shape = ri::world::VolumeShape::Box;
    referencePlane.position = {0.0f, 2.0f, 0.0f};
    referencePlane.size = {8.0f, 5.0f, 1.0f};
    referencePlane.textureId = "caution_stripes_refined.png";
    referencePlane.opacity = 0.78f;
    referencePlane.renderOrder = 60;

    ri::world::ReferenceImagePlane referencePlaneTop = referencePlane;
    referencePlaneTop.id = "reference_plane_top";
    referencePlaneTop.opacity = 0.55f;
    referencePlaneTop.renderOrder = 120;
    environmentService.SetReferenceImagePlanes({referencePlane, referencePlaneTop});

    ri::world::Text3dPrimitive text3dLabel{};
    text3dLabel.id = "text3d_label_a";
    text3dLabel.type = "text_3d_primitive";
    text3dLabel.shape = ri::world::VolumeShape::Box;
    text3dLabel.position = {0.0f, 2.0f, 0.0f};
    text3dLabel.size = {4.0f, 3.0f, 2.0f};
    text3dLabel.text = "MOVE";
    text3dLabel.textScale = 1.4f;

    ri::world::Text3dPrimitive text3dHeader = text3dLabel;
    text3dHeader.id = "text3d_header_a";
    text3dHeader.text = "PARKOUR START";
    text3dHeader.textScale = 2.1f;
    text3dHeader.alwaysFaceCamera = true;
    environmentService.SetText3dPrimitives({text3dLabel, text3dHeader});

    ri::world::AnnotationCommentPrimitive annotationComment{};
    annotationComment.id = "comment_a";
    annotationComment.type = "annotation_comment_primitive";
    annotationComment.shape = ri::world::VolumeShape::Box;
    annotationComment.position = {0.0f, 2.0f, 0.0f};
    annotationComment.size = {4.0f, 3.0f, 4.0f};
    annotationComment.text = "ALIGN THIS HALLWAY";
    annotationComment.textScale = 2.0f;

    ri::world::AnnotationCommentPrimitive annotationCommentPriority = annotationComment;
    annotationCommentPriority.id = "comment_b";
    annotationCommentPriority.text = "CHECK CEILING TRIM";
    annotationCommentPriority.textScale = 3.6f;
    environmentService.SetAnnotationCommentPrimitives({annotationComment, annotationCommentPriority});

    ri::world::MeasureToolPrimitive measureBox{};
    measureBox.id = "measure_box_a";
    measureBox.type = "measure_tool_primitive";
    measureBox.shape = ri::world::VolumeShape::Box;
    measureBox.position = {0.0f, 2.0f, 0.0f};
    measureBox.size = {4.0f, 2.0f, 6.0f};
    measureBox.mode = ri::world::MeasureToolMode::Box;
    measureBox.unitSuffix = "u";
    measureBox.textScale = 2.6f;
    measureBox.labelOffset = {0.0f, 0.9f, 0.0f};

    ri::world::MeasureToolPrimitive measureLine = measureBox;
    measureLine.id = "measure_line_a";
    measureLine.mode = ri::world::MeasureToolMode::Line;
    measureLine.lineStart = {-1.0f, 2.0f, 0.0f};
    measureLine.lineEnd = {2.0f, 6.0f, 0.0f};
    measureLine.textScale = 3.8f;
    measureLine.showFill = false;
    environmentService.SetMeasureToolPrimitives({measureBox, measureLine});

    ri::world::RenderTargetSurface renderTargetSurface{};
    renderTargetSurface.id = "render_target_surface_a";
    renderTargetSurface.type = "render_target_surface";
    renderTargetSurface.shape = ri::world::VolumeShape::Box;
    renderTargetSurface.position = {0.0f, 2.0f, 0.0f};
    renderTargetSurface.size = {10.0f, 6.0f, 1.0f};
    renderTargetSurface.renderResolution = 512;
    renderTargetSurface.resolutionCap = 256;
    renderTargetSurface.maxActiveDistance = 14.0f;
    renderTargetSurface.updateEveryFrames = 2U;

    ri::world::PlanarReflectionSurface planarReflectionSurface{};
    planarReflectionSurface.id = "planar_reflection_surface_a";
    planarReflectionSurface.type = "planar_reflection_surface";
    planarReflectionSurface.shape = ri::world::VolumeShape::Box;
    planarReflectionSurface.position = {0.0f, 2.0f, 0.0f};
    planarReflectionSurface.size = {10.0f, 6.0f, 1.0f};
    planarReflectionSurface.renderResolution = 384;
    planarReflectionSurface.resolutionCap = 512;
    planarReflectionSurface.maxActiveDistance = 8.0f;
    planarReflectionSurface.updateEveryFrames = 3U;
    environmentService.SetRenderTargetSurfaces({renderTargetSurface});
    environmentService.SetPlanarReflectionSurfaces({planarReflectionSurface});

    ri::world::PassThroughPrimitive passThroughGhost{};
    passThroughGhost.id = "pass_through_ghost_a";
    passThroughGhost.type = "pass_through_primitive";
    passThroughGhost.shape = ri::world::VolumeShape::Box;
    passThroughGhost.position = {0.0f, 2.0f, 0.0f};
    passThroughGhost.size = {8.0f, 5.0f, 0.8f};
    passThroughGhost.primitiveShape = ri::world::PassThroughPrimitiveShape::Plane;
    passThroughGhost.material.opacity = 0.35f;
    passThroughGhost.visualBehavior.pulseEnabled = true;
    passThroughGhost.visualBehavior.pulseSpeed = 1.0f;
    passThroughGhost.visualBehavior.pulseMinOpacity = 0.20f;
    passThroughGhost.visualBehavior.pulseMaxOpacity = 0.40f;
    passThroughGhost.visualBehavior.distanceFadeEnabled = true;
    passThroughGhost.visualBehavior.fadeNear = 1.0f;
    passThroughGhost.visualBehavior.fadeFar = 20.0f;

    ri::world::PassThroughPrimitive passThroughBlocking = passThroughGhost;
    passThroughBlocking.id = "pass_through_blocking_warning_a";
    passThroughBlocking.interactionProfile.blocksPlayer = true;
    passThroughBlocking.material.opacity = 0.08f;
    passThroughBlocking.events.onUse = "inspect_warning";
    passThroughBlocking.passThrough = false;
    environmentService.SetPassThroughPrimitives({passThroughGhost, passThroughBlocking});

    ri::world::SkyProjectionSurface skyProjectionSurface{};
    skyProjectionSurface.id = "sky_projection_surface_a";
    skyProjectionSurface.type = "sky_projection_surface";
    skyProjectionSurface.primitiveType = "plane";
    skyProjectionSurface.shape = ri::world::VolumeShape::Box;
    skyProjectionSurface.position = {0.0f, 12.0f, -32.0f};
    skyProjectionSurface.size = {32.0f, 16.0f, 1.0f};
    skyProjectionSurface.visual.mode = ri::world::SkyProjectionVisualMode::Gradient;
    skyProjectionSurface.visual.opacity = 0.92f;
    skyProjectionSurface.behavior.followCameraYaw = true;
    skyProjectionSurface.behavior.parallaxFactor = 0.25f;
    skyProjectionSurface.behavior.renderLayer = ri::world::SkyProjectionRenderLayer::Background;
    environmentService.SetSkyProjectionSurfaces({skyProjectionSurface});

    ri::world::VolumetricEmitterBounds volumetricEmitter{};
    volumetricEmitter.id = "volumetric_emitter_a";
    volumetricEmitter.type = "volumetric_emitter_bounds";
    volumetricEmitter.shape = ri::world::VolumeShape::Box;
    volumetricEmitter.position = {0.0f, 2.0f, 0.0f};
    volumetricEmitter.size = {8.0f, 5.0f, 8.0f};
    volumetricEmitter.emission.particleCount = 128U;
    volumetricEmitter.emission.spawnMode = ri::world::VolumetricEmitterSpawnMode::Uniform;
    volumetricEmitter.emission.spawnRatePerSecond = 12.0f;
    volumetricEmitter.culling.maxActiveDistance = 40.0f;
    volumetricEmitter.culling.pauseWhenOffscreen = false;
    environmentService.SetVolumetricEmitterBounds({volumetricEmitter});

    ri::world::VisibilityPrimitive portalPrimitive{};
    portalPrimitive.id = "portal_a";
    portalPrimitive.type = "portal";
    portalPrimitive.kind = ri::world::VisibilityPrimitiveKind::Portal;
    portalPrimitive.position = {0.0f, 0.0f, 0.0f};
    portalPrimitive.size = {8.0f, 10.0f, 0.25f};

    ri::world::VisibilityPrimitive antiPortalPrimitive{};
    antiPortalPrimitive.id = "anti_portal_a";
    antiPortalPrimitive.type = "anti_portal";
    antiPortalPrimitive.kind = ri::world::VisibilityPrimitiveKind::AntiPortal;
    antiPortalPrimitive.position = {0.0f, 0.0f, 0.0f};
    antiPortalPrimitive.size = {6.0f, 8.0f, 0.25f};

    ri::world::VisibilityPrimitive occlusionPrimitive{};
    occlusionPrimitive.id = "occ_portal_a";
    occlusionPrimitive.type = "occlusion_portal";
    occlusionPrimitive.kind = ri::world::VisibilityPrimitiveKind::OcclusionPortal;
    occlusionPrimitive.position = {0.0f, 0.0f, 0.0f};
    occlusionPrimitive.size = {4.0f, 4.0f, 0.18f};
    occlusionPrimitive.closed = true;
    environmentService.SetVisibilityPrimitives({portalPrimitive, antiPortalPrimitive, occlusionPrimitive});

    ri::world::OcclusionPortalVolume occlusionPortal{};
    occlusionPortal.id = "occ_portal_a";
    occlusionPortal.shape = ri::world::VolumeShape::Box;
    occlusionPortal.position = {0.0f, 0.0f, 0.0f};
    occlusionPortal.size = {4.0f, 4.0f, 0.18f};
    occlusionPortal.closed = true;
    environmentService.SetOcclusionPortalVolumes({occlusionPortal});
    Expect(environmentService.CountVisibilityPrimitives(ri::world::VisibilityPrimitiveKind::Portal) == 1U &&
               environmentService.CountVisibilityPrimitives(ri::world::VisibilityPrimitiveKind::AntiPortal) == 1U,
           "World environment service should track typed visibility primitive counts");
    Expect(environmentService.CountClosedOcclusionPortals() == 1U,
           "World environment service should count closed occlusion portals");
    Expect(environmentService.SetOcclusionPortalClosed("occ_portal_a", false),
           "World environment service should toggle occlusion portals by ID");
    Expect(environmentService.CountClosedOcclusionPortals() == 0U
               && !environmentService.GetOcclusionPortalVolumes().front().closed
               && !environmentService.GetVisibilityPrimitives().back().closed,
           "World environment service should mirror occlusion portal state onto visibility primitives");
    {
        ri::world::RuntimeEnvironmentService occlusionAutoEnvironment{};
        ri::world::OcclusionPortalVolume autoPortal{};
        autoPortal.id = "occ_only_a";
        autoPortal.shape = ri::world::VolumeShape::Box;
        autoPortal.position = {1.0f, 0.0f, 0.0f};
        autoPortal.size = {2.0f, 3.0f, 0.5f};
        autoPortal.closed = true;
        occlusionAutoEnvironment.SetOcclusionPortalVolumes({autoPortal});
        Expect(occlusionAutoEnvironment.CountVisibilityPrimitives(ri::world::VisibilityPrimitiveKind::OcclusionPortal) == 1U,
               "World environment service should auto-register occlusion portals into visibility primitive storage");
        const auto autoMatches = occlusionAutoEnvironment.GetVisibilityPrimitivesAt({1.0f, 0.0f, 0.0f});
        Expect(!autoMatches.empty() && autoMatches.front()->id == "occ_only_a",
               "World environment service should expose auto-registered occlusion portals through visibility point queries");
    }
    const auto visibilityAtOrigin = environmentService.GetVisibilityPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(visibilityAtOrigin.size() >= 2U,
           "World environment service should query portal/anti-portal visibility primitives by point");

    ri::world::DamageTriggerVolume damageVolume{};
    damageVolume.id = "damage_a";
    damageVolume.type = "damage_volume";
    damageVolume.shape = ri::world::VolumeShape::Box;
    damageVolume.position = {0.0f, 0.0f, 0.0f};
    damageVolume.size = {6.0f, 4.0f, 6.0f};
    damageVolume.damagePerSecond = 30.0f;
    damageVolume.broadcastFrequency = 0.1;
    damageVolume.label = "acid_pool";

    ri::world::DamageTriggerVolume killVolume{};
    killVolume.id = "kill_a";
    killVolume.type = "kill_volume";
    killVolume.shape = ri::world::VolumeShape::Box;
    killVolume.position = {0.0f, 0.0f, 0.0f};
    killVolume.size = {4.0f, 4.0f, 4.0f};
    killVolume.killInstant = true;
    killVolume.label = "void";
    environmentService.SetDamageVolumes({damageVolume, killVolume});

    const ri::world::TriggerUpdateResult damageUpdate =
        environmentService.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 1.0, nullptr, true, nullptr, nullptr);
    const bool hasPeriodicDamage = std::any_of(
        damageUpdate.damageRequests.begin(),
        damageUpdate.damageRequests.end(),
        [](const ri::world::DamageRequest& req) {
            return req.volumeId == "damage_a" && req.damagePerSecond > 0.0f && !req.killInstant;
        });
    const bool hasKillDamage = std::any_of(
        damageUpdate.damageRequests.begin(),
        damageUpdate.damageRequests.end(),
        [](const ri::world::DamageRequest& req) {
            return req.volumeId == "kill_a" && req.killInstant;
        });
    Expect(hasPeriodicDamage && hasKillDamage,
           "World environment service should emit trigger-driven damage and kill requests");

    ri::world::LevelSpawnerDefinition levelSpawner{};
    levelSpawner.id = "spawn_a";
    levelSpawner.entityId = "enemy_patrol";
    levelSpawner.enabledByDefault = false;
    environmentService.SetLevelSpawnerDefinitions({levelSpawner});
    auto spawnerStates = environmentService.GetActiveSpawnerStates();
    const bool hasSpawner = std::any_of(
        spawnerStates.begin(),
        spawnerStates.end(),
        [](const ri::world::ActiveSpawnerState& state) {
            return state.id == "spawn_a" && state.entityId == "enemy_patrol" && !state.enabled;
        });
    Expect(hasSpawner,
           "World environment service should index level spawner definitions into active spawner state");

    ri::world::SafeLightRuntimeState safeLight{};
    safeLight.id = "safe_light_a";
    safeLight.groupId = "safe_group_a";
    safeLight.position = {0.0f, 0.0f, 0.0f};
    safeLight.radius = 10.0f;
    safeLight.intensity = 1.0f;
    safeLight.enabled = true;
    safeLight.safeZone = true;
    environmentService.SetSafeLights({safeLight});
    const auto safeCoverage = environmentService.GetSafeLightCoverageAt({1.0f, 0.0f, 0.0f});
    Expect(safeCoverage.insideSafeLight && safeCoverage.combinedCoverage > 0.0f,
           "World environment service should compute safe-light coverage for player position");
    Expect(environmentService.SetLevelLightEnabled("safe_light_a", false),
           "World environment service should support direct light-control hooks by id");
    Expect(environmentService.SetLevelLightGroupEnabled("safe_group_a", true) == 1U,
           "World environment service should support grouped light-control hooks");

    ri::world::AudioReverbVolume reverb{};
    reverb.id = "reverb_a";
    reverb.shape = ri::world::VolumeShape::Box;
    reverb.position = {0.0f, 0.0f, 0.0f};
    reverb.size = {4.0f, 4.0f, 4.0f};
    reverb.reverbMix = 0.5f;
    reverb.echoDelayMs = 150.0f;
    reverb.echoFeedback = 0.4f;
    reverb.dampening = 0.1f;
    reverb.volumeScale = 1.5f;
    reverb.playbackRate = 1.1f;
    environmentService.SetAudioReverbVolumes({reverb});

    ri::world::AudioOcclusionVolume occlusion{};
    occlusion.id = "occ_a";
    occlusion.shape = ri::world::VolumeShape::Box;
    occlusion.position = {0.0f, 0.0f, 0.0f};
    occlusion.size = {4.0f, 4.0f, 4.0f};
    occlusion.occlusionStrength = 0.6f;
    occlusion.volumeScale = 0.75f;
    environmentService.SetAudioOcclusionVolumes({occlusion});

    const ri::world::PostProcessState postState = environmentService.GetActivePostProcessStateAt({0.0f, 0.0f, 0.0f});
    Expect(postState.label == "pp_a,pp_b,fog_a,fluid_a",
           "World environment service should merge active post-process, fog, and fluid volume labels");
    Expect(NearlyEqual(postState.tintStrength, 0.7f),
           "World environment service should use the strongest post-process tint contribution");
    Expect(NearlyEqual(postState.blurAmount, 0.004f),
           "World environment service should keep the strongest blur contribution");
    Expect(NearlyEqual(postState.noiseAmount, 0.15f),
           "World environment service should accumulate post-process noise amounts");
    Expect(NearlyEqual(postState.scanlineAmount, 0.02f),
           "World environment service should accumulate scanline amounts");
    ExpectVec3(postState.tintColor, {0.42f, 0.74f, 1.0f},
               "World environment service should preserve layered tint-color state with fluid override semantics");

    const ri::world::AudioEnvironmentState audioState = environmentService.GetActiveAudioEnvironmentStateAt({0.0f, 0.0f, 0.0f});
    Expect(audioState.label == "reverb_a,occ_a,fluid_a",
           "World environment service should merge reverb, occlusion, and fluid audio volume labels");
    Expect(NearlyEqual(audioState.reverbMix, 0.5f),
           "World environment service should keep the strongest reverb mix");
    Expect(NearlyEqual(audioState.echoDelayMs, 150.0f),
           "World environment service should keep the strongest echo delay");
    Expect(NearlyEqual(audioState.echoFeedback, 0.4f),
           "World environment service should preserve reverb echo feedback");
    Expect(NearlyEqual(audioState.dampening, 0.6f),
           "World environment service should merge occlusion dampening into audio state");
    Expect(NearlyEqual(audioState.volumeScale, 1.125f),
           "World environment service should combine reverb and occlusion volume scaling");
    Expect(NearlyEqual(audioState.playbackRate, 1.1f),
           "World environment service should preserve playback-rate shaping from reverb volumes");

    const ri::world::PhysicsVolumeModifiers physicsState = environmentService.GetPhysicsVolumeModifiersAt({0.0f, 0.0f, 0.0f});
    Expect(NearlyEqual(physicsState.gravityScale, 0.4f) &&
               NearlyEqual(physicsState.jumpScale, 0.72f) &&
               NearlyEqual(physicsState.drag, 3.0f) &&
               NearlyEqual(physicsState.buoyancy, 1.2f),
           "World environment service should merge physics, fluid, and wind gameplay modifiers through native physics state");
    ExpectVec3(physicsState.flow, {3.2f, 0.0f, 1.1f},
               "World environment service should merge custom gravity, fluid, surface, and radial force flow vectors");
    Expect(physicsState.activeVolumes.size() == 2U &&
               physicsState.activeVolumes[0] == "gravity_a" &&
               physicsState.activeVolumes[1] == "wind_a",
           "World environment service should report active authored physics volume IDs");
    Expect(physicsState.activeFluids.size() == 1U && physicsState.activeFluids.front() == "fluid_a",
           "World environment service should report active fluid volume IDs");
    Expect(physicsState.activeSurfaceVelocity.size() == 1U && physicsState.activeSurfaceVelocity.front() == "surface_a",
           "World environment service should report active surface velocity primitives");
    Expect(physicsState.activeRadialForces.empty(),
           "World environment service should skip radial force contribution when sampled at the force origin");

    const ri::world::PhysicsVolumeModifiers offsetPhysicsState =
        environmentService.GetPhysicsVolumeModifiersAt({2.0f, 0.0f, 0.0f});
    ExpectVec3(offsetPhysicsState.flow, {5.2f, 0.0f, 1.1f},
               "World environment service should add radial-force falloff to the aggregated flow field away from the origin");
    Expect(offsetPhysicsState.activeRadialForces.size() == 1U && offsetPhysicsState.activeRadialForces.front() == "radial_a",
           "World environment service should report active radial force volumes");

    {
        const ri::trace::TraceScene modifierScene({
            ri::trace::TraceCollider{
                .id = "floor",
                .bounds = ri::spatial::Aabb{.min = {-16.0f, -1.0f, -16.0f}, .max = {16.0f, 0.0f, 16.0f}},
                .structural = true,
                .dynamic = false},
        });
        ri::trace::MovementControllerState movement{};
        movement.onGround = true;
        movement.body.bounds = ri::spatial::Aabb{.min = {-0.2f, 0.01f, -0.2f}, .max = {0.2f, 1.81f, 0.2f}};
        movement.body.velocity = {};
        ri::trace::MovementControllerOptions options{};
        options.maxGroundSpeed = 0.0f;
        options.maxSprintGroundSpeed = 0.0f;
        options.maxAirSpeed = 0.0f;
        options.groundAcceleration = 0.0f;
        options.airAcceleration = 0.0f;
        options.groundFriction = 0.0f;
        options.gravity = 25.0f;
        options.fallGravityMultiplier = 1.0f;
        options.simulateStamina = false;

        const ri::world::PhysicsVolumeModifiers bridgeState = environmentService.GetPhysicsVolumeModifiersAt({0.0f, 0.0f, 0.0f});
        ri::trace::KinematicVolumeModifiers modifiers{};
        modifiers.gravityScale = bridgeState.gravityScale;
        modifiers.drag = bridgeState.drag;
        modifiers.buoyancy = bridgeState.buoyancy;
        modifiers.flow = bridgeState.flow;
        modifiers.jumpScale = bridgeState.jumpScale;

        const ri::trace::MovementControllerResult bridgedStep = ri::trace::SimulateMovementControllerStep(
            modifierScene,
            movement,
            ri::trace::MovementInput{},
            0.1f,
            options,
            modifiers);
        Expect(bridgedStep.state.body.velocity.x > 0.05f,
               "World surface velocity primitives should produce usable runtime flow through movement simulation");
    }

    const ri::world::PhysicsConstraintState constraintState =
        environmentService.GetPhysicsConstraintStateAt({0.0f, 0.0f, 0.0f});
    Expect(constraintState.lockAxes.size() == 2U &&
               constraintState.lockAxes[0] == ri::world::ConstraintAxis::X &&
               constraintState.lockAxes[1] == ri::world::ConstraintAxis::Z,
           "World environment service should report active typed physics-constraint axes");
    const ri::world::WaterSurfaceState waterState =
        environmentService.GetWaterSurfaceStateAt({0.0f, 0.0f, 0.0f}, 1.0);
    Expect(waterState.inside &&
               waterState.surface != nullptr &&
               waterState.surface->id == "water_surface_a" &&
               waterState.surfaceY > 0.8f,
           "World environment service should resolve water-surface primitives with animated surface height");
    const ri::world::KinematicMotionState kinematicState =
        environmentService.ResolveKinematicMotionAt({0.0f, 0.0f, 0.0f}, 1.0);
    Expect(!kinematicState.activeTranslationPrimitives.empty() &&
               !kinematicState.activeRotationPrimitives.empty() &&
               kinematicState.translationDelta.x > 0.8f &&
               kinematicState.rotationDeltaDegrees.y > 20.0f,
           "World environment service should resolve kinematic translation and rotation primitive motion");
    const std::vector<const ri::world::SplinePathFollowerPrimitive*> splineFollowers =
        environmentService.GetSplinePathFollowerPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(splineFollowers.size() == 1U && splineFollowers.front()->id == "spline_follow_a",
           "World environment service should expose spline-path follower primitives at a position");
    const std::vector<ri::world::SplinePathFollowerRuntimeState> splineFollowerStates =
        environmentService.GetSplinePathFollowerRuntimeStates({0.0f, 0.0f, 0.0f}, 0.5);
    Expect(splineFollowerStates.size() == 1U &&
               splineFollowerStates.front().active &&
               splineFollowerStates.front().splineValid &&
               splineFollowerStates.front().normalizedProgress > 0.0f &&
               splineFollowerStates.front().sampleForward.x >= 0.0f,
           "World environment service should evaluate spline-path follower runtime states over time");
    const std::vector<const ri::world::CablePrimitive*> activeCables =
        environmentService.GetCablePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(activeCables.size() == 1U && activeCables.front()->id == "cable_a",
           "World environment service should expose cable primitives at a position");
    const std::vector<ri::world::CableRuntimeState> cableStates =
        environmentService.GetCableRuntimeStates({0.0f, 0.0f, 0.0f}, 0.5);
    Expect(cableStates.size() == 1U &&
               cableStates.front().active &&
               std::fabs(cableStates.front().swayOffset) > 0.0001f &&
               cableStates.front().resolvedEnd.y < cableStates.front().resolvedStart.y,
           "World environment service should evaluate cable runtime sway states over time");
    const std::vector<const ri::world::ClippingRuntimeVolume*> clippingVolumes =
        environmentService.GetClippingVolumesAt({0.0f, 0.0f, 0.0f});
    Expect(clippingVolumes.size() == 1U && clippingVolumes.front()->id == "clip_a",
           "World environment service should expose clipping volumes at a position");
    const std::vector<const ri::world::FilteredCollisionRuntimeVolume*> filteredVolumes =
        environmentService.GetFilteredCollisionVolumesAt({0.0f, 0.0f, 0.0f});
    Expect(filteredVolumes.size() == 1U && filteredVolumes.front()->id == "filtered_collision_runtime_a",
           "World environment service should expose filtered-collision volumes at a position");
    Expect(environmentService.IsCameraBlockedAt({0.0f, 0.0f, 0.0f}),
           "World environment service should resolve camera-blocking helper volumes by collision channel");
    const ri::world::AiPerceptionBlockerState aiBlockerState =
        environmentService.GetAiPerceptionBlockerStateAt({0.0f, 0.0f, 0.0f});
    Expect(aiBlockerState.blocked &&
               aiBlockerState.anyEnabled &&
               aiBlockerState.matches.size() == 1U &&
               aiBlockerState.matches.front()->id == "ai_blocker_a",
           "World environment service should resolve AI perception blocker states at a position");
    const ri::world::SafeZoneState safeZoneState = environmentService.GetSafeZoneStateAt({0.0f, 0.0f, 0.0f});
    Expect(safeZoneState.inside &&
               safeZoneState.dropAggro &&
               safeZoneState.matches.size() == 1U &&
               safeZoneState.matches.front()->id == "safe_zone_a",
           "World environment service should resolve safe-zone runtime state at a position");

    const ri::world::TraversalLinkVolume* activeTraversal = environmentService.GetTraversalLinkAt({0.0f, 0.0f, 0.0f});
    Expect(activeTraversal != nullptr &&
               activeTraversal->id == "ladder_a" &&
               activeTraversal->kind == ri::world::TraversalLinkKind::Ladder &&
               NearlyEqual(activeTraversal->climbSpeed, 4.8f),
           "World environment service should report the active traversal link at a position");
    const ri::world::TraversalLinkSelectionState traversalSelection =
        environmentService.GetTraversalLinksAt({0.0f, 0.0f, 0.0f});
    Expect(traversalSelection.selected != nullptr &&
               traversalSelection.matches.size() >= 1U &&
               traversalSelection.selected->id == "ladder_a",
           "World environment service should return traversal-link selection state with deterministic priority");
    const ri::world::PivotAnchorPrimitive* activePivotAnchor = environmentService.GetPivotAnchorAt({0.0f, 0.0f, 0.0f});
    Expect(activePivotAnchor != nullptr &&
               activePivotAnchor->id == "pivot_anchor_a" &&
               activePivotAnchor->anchorId == "door_hinge_anchor" &&
               activePivotAnchor->alignToSurfaceNormal,
           "World environment service should resolve pivot-anchor helper primitives at a position");
    const ri::world::PivotAnchorBindingState pivotBinding =
        environmentService.ResolvePivotAnchorBindingAt({0.2f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    Expect(pivotBinding.anchor != nullptr &&
               pivotBinding.anchor->id == "pivot_anchor_a",
           "World environment service should expose resolved pivot-anchor binding state");
    ExpectVec3(pivotBinding.resolvedPosition, {0.0f, 0.0f, 0.0f},
               "World environment service should resolve pivot anchor position from helper metadata");
    ExpectVec3(pivotBinding.resolvedForwardAxis, {0.0f, 1.0f, 0.0f},
               "World environment service should resolve pivot anchor axis from helper metadata");
    const ri::world::SymmetryMirrorPlane* activeMirrorPlane =
        environmentService.GetSymmetryMirrorPlaneAt({0.0f, 0.0f, 0.0f});
    Expect(activeMirrorPlane != nullptr &&
               activeMirrorPlane->id == "symmetry_plane_a" &&
               NearlyEqual(activeMirrorPlane->planeOffset, 0.5f) &&
               !activeMirrorPlane->keepOriginal &&
               activeMirrorPlane->snapToGrid,
           "World environment service should resolve symmetry-mirror helper volumes at a position");
    const ri::world::SymmetryMirrorResult mirrorResult =
        environmentService.ResolveSymmetryMirrorAt({1.0f, 0.0f, 1.5f}, {0.0f, 0.0f, 1.0f});
    Expect(mirrorResult.plane != nullptr &&
               mirrorResult.mirrored,
           "World environment service should expose mirrored transform results for symmetry helpers");
    ExpectVec3(mirrorResult.mirroredPosition, {1.0f, 0.0f, -0.5f},
               "World environment service should mirror positions against symmetry plane authoring metadata");
    ExpectVec3(mirrorResult.mirroredForward, {0.0f, 0.0f, -1.0f},
               "World environment service should mirror forward direction against symmetry plane normal");
    const ri::world::AuthoringPlacementState placement =
        environmentService.ResolveAuthoringPlacementAt({1.12f, 0.0f, 1.40f}, {0.0f, 0.0f, 1.0f});
    Expect(placement.pivotAnchor != nullptr &&
               placement.mirrorPlane != nullptr &&
               placement.mirrored &&
               placement.snappedToGrid,
           "World environment service should resolve combined pivot/symmetry authoring placement state");
    ExpectVec3(placement.resolvedPosition, {0.0f, 0.0f, 1.0f},
               "World environment service should apply symmetry mirroring plus local grid snapping in placement resolution");
    ExpectVec3(placement.resolvedForward, {0.0f, 1.0f, 0.0f},
               "World environment service should preserve authored pivot axis through combined placement resolution");
    const ri::world::SpatialQueryMatchState spatialQueryState =
        environmentService.GetSpatialQueryStateAt({0.0f, 0.0f, 0.0f}, 0x4U);
    Expect(spatialQueryState.matches.size() == 1U &&
               spatialQueryState.matches.front()->id == "query_a" &&
               spatialQueryState.combinedFilterMask == 0x4U &&
               !spatialQueryState.hasUnfilteredVolume,
           "World environment service should expose filtered spatial-query authoring metadata at a position");
    const ri::world::LocalGridSnapVolume* activeSnap = environmentService.GetLocalGridSnapAt({0.0f, 0.0f, 0.0f});
    Expect(activeSnap != nullptr &&
               activeSnap->id == "snap_a" &&
               NearlyEqual(activeSnap->snapSize, 0.5f),
           "World environment service should report the active local grid snap volume at a position");
    const ri::world::HintPartitionVolume* activeHint = environmentService.GetHintPartitionVolumeAt({0.0f, 0.0f, 0.0f});
    Expect(activeHint != nullptr &&
               activeHint->id == "hint_a" &&
               activeHint->mode == ri::world::HintPartitionMode::Skip,
           "World environment service should report the active hint partition volume at a position");
    const ri::world::HintPartitionState hintState = environmentService.GetHintPartitionStateAt({0.0f, 0.0f, 0.0f});
    Expect(hintState.inside &&
               hintState.volume != nullptr &&
               hintState.mode == ri::world::HintPartitionMode::Skip,
           "World environment service should expose hint-skip state for authoring partition metadata");
    const ri::world::DoorWindowCutoutPrimitive* activeCutout = environmentService.GetDoorWindowCutoutAt({0.0f, 0.0f, 0.0f});
    Expect(activeCutout != nullptr &&
               activeCutout->id == "cutout_a" &&
               NearlyEqual(activeCutout->openingWidth, 1.6f),
           "World environment service should resolve door-window cutout helper primitives at a position");
    const ri::world::CameraConfinementVolume* activeConfinement =
        environmentService.GetCameraConfinementVolumeAt({0.0f, 0.0f, 0.0f});
    Expect(activeConfinement != nullptr && activeConfinement->id == "camera_box_a",
           "World environment service should report the active camera confinement volume at a position");
    const std::vector<const ri::world::LodOverrideVolume*> activeLodOverrides =
        environmentService.GetLodOverridesAt({0.0f, 0.0f, 0.0f});
    Expect(activeLodOverrides.size() == 1U &&
               activeLodOverrides.front()->id == "lod_box_a" &&
               activeLodOverrides.front()->forcedLod == ri::world::ForcedLod::Far,
           "World environment service should report active lod override volumes at a position");
    const ri::world::LodOverrideSelectionState lodOverrideState =
        environmentService.ResolveLodOverrideAt({0.0f, 0.0f, 0.0f}, "mesh_a");
    Expect(lodOverrideState.selected != nullptr &&
               lodOverrideState.selected->id == "lod_box_a" &&
               lodOverrideState.hasTargetMatch &&
               lodOverrideState.forcedLod == ri::world::ForcedLod::Far,
           "World environment service should resolve forced lod overrides for targeted meshes");
    ri::world::LodOverrideVolume lodOverrideWildcard{};
    lodOverrideWildcard.id = "lod_wildcard_b";
    lodOverrideWildcard.shape = ri::world::VolumeShape::Box;
    lodOverrideWildcard.position = {0.0f, 0.0f, 0.0f};
    lodOverrideWildcard.size = {8.0f, 6.0f, 8.0f};
    lodOverrideWildcard.forcedLod = ri::world::ForcedLod::Near;
    ri::world::LodOverrideVolume lodOverrideTargeted{};
    lodOverrideTargeted.id = "lod_targeted_a";
    lodOverrideTargeted.shape = ri::world::VolumeShape::Box;
    lodOverrideTargeted.position = {0.0f, 0.0f, 0.0f};
    lodOverrideTargeted.size = {8.0f, 6.0f, 8.0f};
    lodOverrideTargeted.targetIds = {"mesh_b"};
    lodOverrideTargeted.forcedLod = ri::world::ForcedLod::Far;
    environmentService.SetLodOverrideVolumes({lodOverrideWildcard, lodOverrideTargeted});
    const ri::world::LodOverrideSelectionState lodOverrideTargetedState =
        environmentService.ResolveLodOverrideAt({0.0f, 0.0f, 0.0f}, "mesh_b");
    Expect(lodOverrideTargetedState.selected != nullptr
               && lodOverrideTargetedState.selected->id == "lod_targeted_a"
               && lodOverrideTargetedState.hasTargetMatch
               && lodOverrideTargetedState.forcedLod == ri::world::ForcedLod::Far,
           "World environment service should deterministically prefer explicit target LOD overrides over wildcard fallbacks");
    const ri::world::LodOverrideSelectionState lodOverrideWildcardState =
        environmentService.ResolveLodOverrideAt({0.0f, 0.0f, 0.0f}, "mesh_unknown");
    Expect(lodOverrideWildcardState.selected != nullptr
               && lodOverrideWildcardState.selected->id == "lod_wildcard_b"
               && !lodOverrideWildcardState.hasTargetMatch
               && lodOverrideWildcardState.forcedLod == ri::world::ForcedLod::Near,
           "World environment service should deterministically fall back to wildcard LOD overrides when no explicit target exists");
    const std::vector<ri::world::LodSwitchSelectionState> lodSwitchStates =
        environmentService.GetLodSwitchSelectionStates();
    Expect(lodSwitchStates.size() == 1U &&
               lodSwitchStates.front().id == "lod_switch_a" &&
               lodSwitchStates.front().activeLevel == "far" &&
               lodSwitchStates.front().collisionProfile == ri::world::LodSwitchCollisionProfile::Simplified,
           "World environment service should deterministically select active lod-switch levels from viewer distance");
    environmentService.UpdateLodSwitchPrimitives({10.0f, 0.0f, 0.0f}, 1.25, 0.0f);
    const std::vector<ri::world::LodSwitchSelectionState> lodSwitchStatesNear =
        environmentService.GetLodSwitchSelectionStates();
    Expect(lodSwitchStatesNear.size() == 1U &&
               lodSwitchStatesNear.front().activeLevel == "near",
           "World environment service should leave the far band when the camera moves inside the near-only distance range");
    environmentService.UpdateLodSwitchPrimitives({0.0f, 0.0f, 0.0f}, 1.9, 0.0f);
    const std::vector<ri::world::LodSwitchSelectionState> lodSwitchStatesResolved =
        environmentService.GetLodSwitchSelectionStates();
    Expect(lodSwitchStatesResolved.size() == 1U &&
               lodSwitchStatesResolved.front().activeLevel == "near" &&
               lodSwitchStatesResolved.front().switchCount >= 2U &&
               !lodSwitchStatesResolved.front().crossfadeActive &&
               lodSwitchStatesResolved.front().crossfadeAlpha == 1.0f,
           "World environment service should resolve lod-switch transitions with deterministic crossfade completion");
    const std::vector<const ri::world::SurfaceScatterVolume*> activeSurfaceScatterVolumes =
        environmentService.GetSurfaceScatterVolumesAt({0.0f, 0.0f, 0.0f});
    Expect(activeSurfaceScatterVolumes.size() == 1U &&
               activeSurfaceScatterVolumes.front()->id == "surface_scatter_a" &&
               activeSurfaceScatterVolumes.front()->targetIds.size() == 2U,
           "World environment service should report active surface-scatter volumes at a position");
    const ri::world::SurfaceScatterVolume* activeSurfaceScatterVolume =
        environmentService.GetSurfaceScatterVolumeAt({0.0f, 0.0f, 0.0f});
    Expect(activeSurfaceScatterVolume != nullptr &&
               activeSurfaceScatterVolume->id == "surface_scatter_a" &&
               activeSurfaceScatterVolume->sourceRepresentation.payloadId == "mesh_pebble_a",
           "World environment service should resolve the top surface-scatter volume at a position");
    const std::vector<ri::world::SurfaceScatterRuntimeState> surfaceScatterStates =
        environmentService.GetSurfaceScatterRuntimeStates({0.0f, 0.0f, 0.0f});
    Expect(surfaceScatterStates.size() == 1U &&
               surfaceScatterStates.front().id == "surface_scatter_a" &&
               surfaceScatterStates.front().active &&
               surfaceScatterStates.front().withinDistance &&
               surfaceScatterStates.front().targetsResolved &&
               surfaceScatterStates.front().requestedCount >= 80U &&
               surfaceScatterStates.front().generatedCount >= 1U &&
               surfaceScatterStates.front().generatedCount <= 120U &&
               surfaceScatterStates.front().layoutSignature != 0U &&
               surfaceScatterStates.front().collisionPolicy == ri::world::SurfaceScatterCollisionPolicy::Proxy,
           "World environment service should compute deterministic surface-scatter runtime state with explicit budget constraints");
    const std::vector<const ri::world::SplineMeshDeformerPrimitive*> activeSplineDeformers =
        environmentService.GetSplineMeshDeformerPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(activeSplineDeformers.size() == 1U &&
               activeSplineDeformers.front()->id == "spline_deformer_a" &&
               activeSplineDeformers.front()->targetIds.size() == 1U,
           "World environment service should report active spline-mesh deformers at a position");
    const ri::world::SplineMeshDeformerPrimitive* activeSplineDeformer =
        environmentService.GetSplineMeshDeformerPrimitiveAt({0.0f, 0.0f, 0.0f});
    Expect(activeSplineDeformer != nullptr &&
               activeSplineDeformer->id == "spline_deformer_a" &&
               activeSplineDeformer->collisionEnabled,
           "World environment service should resolve the top spline-mesh deformer at a position");
    const std::vector<ri::world::SplineMeshDeformerRuntimeState> splineDeformerStates =
        environmentService.GetSplineMeshDeformerRuntimeStates({0.0f, 0.0f, 0.0f});
    Expect(splineDeformerStates.size() == 1U &&
               splineDeformerStates.front().id == "spline_deformer_a" &&
               splineDeformerStates.front().active &&
               splineDeformerStates.front().withinDistance &&
               splineDeformerStates.front().splineValid &&
               splineDeformerStates.front().targetsResolved &&
               splineDeformerStates.front().requestedSamples == 18U &&
               splineDeformerStates.front().generatedSegments == 17U &&
               splineDeformerStates.front().topologySignature != 0U,
           "World environment service should compute deterministic spline-mesh deformer topology state");
    const std::vector<const ri::world::SplineDecalRibbonPrimitive*> activeSplineRibbons =
        environmentService.GetSplineDecalRibbonPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(activeSplineRibbons.size() == 1U &&
               activeSplineRibbons.front()->id == "spline_ribbon_a" &&
               activeSplineRibbons.front()->transparentBlend,
           "World environment service should report active spline-decal ribbons at a position");
    const ri::world::SplineDecalRibbonPrimitive* activeSplineRibbon =
        environmentService.GetSplineDecalRibbonPrimitiveAt({0.0f, 0.0f, 0.0f});
    Expect(activeSplineRibbon != nullptr &&
               activeSplineRibbon->id == "spline_ribbon_a" &&
               !activeSplineRibbon->depthWrite,
           "World environment service should resolve the top spline-decal ribbon at a position");
    const std::vector<ri::world::SplineDecalRibbonRuntimeState> splineRibbonStates =
        environmentService.GetSplineDecalRibbonRuntimeStates({0.0f, 0.0f, 0.0f});
    Expect(splineRibbonStates.size() == 1U &&
               splineRibbonStates.front().id == "spline_ribbon_a" &&
               splineRibbonStates.front().active &&
               splineRibbonStates.front().withinDistance &&
               splineRibbonStates.front().splineValid &&
               splineRibbonStates.front().requestedSamples == 20U &&
               splineRibbonStates.front().generatedSegments == 19U &&
               splineRibbonStates.front().generatedTriangles == 38U &&
               splineRibbonStates.front().topologySignature != 0U,
           "World environment service should compute deterministic spline-decal ribbon topology state");
    const std::vector<const ri::world::TopologicalUvRemapperVolume*> activeUvRemappers =
        environmentService.GetTopologicalUvRemapperVolumesAt({0.0f, 0.0f, 0.0f});
    Expect(activeUvRemappers.size() == 1U &&
               activeUvRemappers.front()->id == "uv_remapper_runtime" &&
               activeUvRemappers.front()->targetIds.size() == 2U,
           "World environment service should report active topological UV remapper volumes at a position");
    const ri::world::TopologicalUvRemapperVolume* topUvRemapper =
        environmentService.GetTopologicalUvRemapperVolumeAt({0.0f, 0.0f, 0.0f});
    Expect(topUvRemapper != nullptr && topUvRemapper->sharedTextureId == "atlas_main",
           "World environment service should resolve the prioritized topological UV remapper at a position");
    const std::vector<const ri::world::TriPlanarNode*> activeTriPlanarNodes =
        environmentService.GetTriPlanarNodesAt({0.0f, 0.0f, 0.0f});
    Expect(activeTriPlanarNodes.size() == 1U && activeTriPlanarNodes.front()->id == "tri_planar_runtime",
           "World environment service should report active tri-planar nodes at a position");
    const ri::world::TriPlanarNode* topTriPlanar = environmentService.GetTriPlanarNodeAt({0.0f, 0.0f, 0.0f});
    Expect(topTriPlanar != nullptr && topTriPlanar->textureY == "rock_y",
           "World environment service should resolve the prioritized tri-planar node at a position");
    const std::vector<ri::world::ProceduralUvProjectionRuntimeState> proceduralUvStates =
        environmentService.GetProceduralUvProjectionRuntimeStates({0.0f, 0.0f, 0.0f});
    Expect(proceduralUvStates.size() == 2U && proceduralUvStates.front().estimatedMaterialPatches == 2U &&
               proceduralUvStates.front().kind == ri::world::ProceduralUvProjectionKind::TopologicalRemapper &&
               proceduralUvStates.front().configSignature != 0U && proceduralUvStates[1U].estimatedMaterialPatches == 1U &&
               proceduralUvStates[1U].kind == ri::world::ProceduralUvProjectionKind::TriPlanarNode &&
               proceduralUvStates[1U].textureSetValid && proceduralUvStates[1U].targetSetValid,
           "World environment service should merge procedural UV projection runtime state with bounded patch estimates");
    const std::vector<const ri::world::InstanceCloudPrimitive*> activeInstanceClouds =
        environmentService.GetInstanceCloudPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(activeInstanceClouds.size() == 1U &&
               activeInstanceClouds.front()->id == "instance_cloud_a" &&
               activeInstanceClouds.front()->sourceRepresentation.kind == ri::world::InstanceCloudRepresentationKind::Mesh,
           "World environment service should report active instance-cloud primitives at a position");
    const ri::world::InstanceCloudPrimitive* activeInstanceCloud =
        environmentService.GetInstanceCloudPrimitiveAt({0.0f, 0.0f, 0.0f});
    Expect(activeInstanceCloud != nullptr &&
               activeInstanceCloud->id == "instance_cloud_a" &&
               activeInstanceCloud->sourceRepresentation.payloadId == "mesh_crate_a",
           "World environment service should resolve the top instance-cloud primitive at a position");
    const std::vector<ri::world::InstanceCloudRuntimeState> instanceCloudStates =
        environmentService.GetInstanceCloudRuntimeStates({0.0f, 0.0f, 0.0f});
    Expect(instanceCloudStates.size() == 1U &&
               instanceCloudStates.front().id == "instance_cloud_a" &&
               instanceCloudStates.front().active &&
               instanceCloudStates.front().withinDistance &&
               instanceCloudStates.front().instanceCount == 64U &&
               instanceCloudStates.front().activeInstanceCount == 64U &&
               instanceCloudStates.front().collisionPolicy == ri::world::InstanceCloudCollisionPolicy::Simplified,
           "World environment service should compute deterministic instance-cloud runtime state and active counts");
    const auto voronoiAtOrigin = environmentService.GetVoronoiFracturePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(voronoiAtOrigin.size() == 1U && voronoiAtOrigin.front()->id == "voronoi_fracture_a",
           "World environment service should expose voronoi fracture primitives through runtime point queries");
    const auto metaballAtOrigin = environmentService.GetMetaballPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(metaballAtOrigin.size() == 1U && metaballAtOrigin.front()->id == "metaball_a",
           "World environment service should expose metaball primitives through runtime point queries");
    const auto latticeAtOrigin = environmentService.GetLatticeVolumesAt({0.0f, 0.0f, 0.0f});
    Expect(latticeAtOrigin.size() == 1U && latticeAtOrigin.front()->id == "lattice_a",
           "World environment service should expose lattice volumes through runtime point queries");
    const auto manifoldAtOrigin = environmentService.GetManifoldSweepPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(manifoldAtOrigin.size() == 1U && manifoldAtOrigin.front()->id == "manifold_sweep_a",
           "World environment service should expose manifold sweep primitives through runtime point queries");
    const auto trimSheetAtOrigin = environmentService.GetTrimSheetSweepPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(trimSheetAtOrigin.size() == 1U && trimSheetAtOrigin.front()->id == "trim_sheet_sweep_a",
           "World environment service should expose trim-sheet sweep primitives through runtime point queries");
    const auto lSystemAtOrigin = environmentService.GetLSystemBranchPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(lSystemAtOrigin.size() == 1U && lSystemAtOrigin.front()->id == "l_system_branch_a",
           "World environment service should expose l-system branch primitives through runtime point queries");
    const auto geodesicAtOrigin = environmentService.GetGeodesicSpherePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(geodesicAtOrigin.size() == 1U && geodesicAtOrigin.front()->id == "geodesic_sphere_a",
           "World environment service should expose geodesic sphere primitives through runtime point queries");
    const auto extrudeAtOrigin = environmentService.GetExtrudeAlongNormalPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(extrudeAtOrigin.size() == 1U && extrudeAtOrigin.front()->id == "extrude_normal_a",
           "World environment service should expose extrude-along-normal primitives through runtime point queries");
    const auto superellipsoidAtOrigin = environmentService.GetSuperellipsoidPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(superellipsoidAtOrigin.size() == 1U && superellipsoidAtOrigin.front()->id == "superellipsoid_a",
           "World environment service should expose superellipsoid primitives through runtime point queries");
    const auto primitiveDemoLatticeAtOrigin = environmentService.GetPrimitiveDemoLatticePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(primitiveDemoLatticeAtOrigin.size() == 1U && primitiveDemoLatticeAtOrigin.front()->id == "primitive_demo_lattice_a",
           "World environment service should expose primitive-demo lattice helpers through runtime point queries");
    const auto primitiveDemoVoronoiAtOrigin = environmentService.GetPrimitiveDemoVoronoiPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(primitiveDemoVoronoiAtOrigin.size() == 1U && primitiveDemoVoronoiAtOrigin.front()->id == "primitive_demo_voronoi_a",
           "World environment service should expose primitive-demo voronoi helpers through runtime point queries");
    const auto thickPolygonAtOrigin = environmentService.GetThickPolygonPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(thickPolygonAtOrigin.size() == 1U && thickPolygonAtOrigin.front()->id == "thick_polygon_a",
           "World environment service should expose thick-polygon primitives through runtime point queries");
    const auto structuralProfileAtOrigin = environmentService.GetStructuralProfilePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(structuralProfileAtOrigin.size() == 1U && structuralProfileAtOrigin.front()->id == "structural_profile_a",
           "World environment service should expose structural-profile primitives through runtime point queries");
    const auto halfPipeAtOrigin = environmentService.GetHalfPipePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(halfPipeAtOrigin.size() == 1U && halfPipeAtOrigin.front()->id == "half_pipe_a",
           "World environment service should expose half-pipe primitives through runtime point queries");
    const auto quarterPipeAtOrigin = environmentService.GetQuarterPipePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(quarterPipeAtOrigin.size() == 1U && quarterPipeAtOrigin.front()->id == "quarter_pipe_a",
           "World environment service should expose quarter-pipe primitives through runtime point queries");
    const auto pipeElbowAtOrigin = environmentService.GetPipeElbowPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(pipeElbowAtOrigin.size() == 1U && pipeElbowAtOrigin.front()->id == "pipe_elbow_a",
           "World environment service should expose pipe-elbow primitives through runtime point queries");
    const auto torusSliceAtOrigin = environmentService.GetTorusSlicePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(torusSliceAtOrigin.size() == 1U && torusSliceAtOrigin.front()->id == "torus_slice_a",
           "World environment service should expose torus-slice primitives through runtime point queries");
    const auto splineSweepAtOrigin = environmentService.GetSplineSweepPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(splineSweepAtOrigin.size() == 1U && splineSweepAtOrigin.front()->id == "spline_sweep_a",
           "World environment service should expose spline-sweep primitives through runtime point queries");
    const auto revolveAtOrigin = environmentService.GetRevolvePrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(revolveAtOrigin.size() == 1U && revolveAtOrigin.front()->id == "revolve_a",
           "World environment service should expose revolve primitives through runtime point queries");
    const auto domeVaultAtOrigin = environmentService.GetDomeVaultPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(domeVaultAtOrigin.size() == 1U && domeVaultAtOrigin.front()->id == "dome_vault_a",
           "World environment service should expose dome-vault primitives through runtime point queries");
    const auto loftAtOrigin = environmentService.GetLoftPrimitivesAt({0.0f, 0.0f, 0.0f});
    Expect(loftAtOrigin.size() == 1U && loftAtOrigin.front()->id == "loft_a",
           "World environment service should expose loft primitives through runtime point queries");
    const std::vector<const ri::world::NavmeshModifierVolume*> activeNavmeshModifiers =
        environmentService.GetNavmeshModifiersAt({0.0f, 0.0f, 0.0f});
    Expect(activeNavmeshModifiers.size() == 1U &&
               activeNavmeshModifiers.front()->id == "nav_box_a" &&
               NearlyEqual(activeNavmeshModifiers.front()->traversalCost, 2.5f),
           "World environment service should report active navmesh modifier volumes at a position");
    const ri::world::NavmeshModifierAggregateState navmeshAggregate =
        environmentService.GetNavmeshModifierAggregateAt({0.0f, 0.0f, 0.0f});
    Expect(navmeshAggregate.matches.size() == 1U &&
               NearlyEqual(navmeshAggregate.traversalCostMultiplier, 2.5f) &&
               NearlyEqual(navmeshAggregate.maxTraversalCost, 2.5f) &&
               navmeshAggregate.dominantTag == "slow",
           "World environment service should expose aggregate navmesh traversal metadata");
    const std::vector<const ri::world::ReferenceImagePlane*> activeReferencePlanes =
        environmentService.GetReferenceImagePlanesAt({0.0f, 2.0f, 0.0f});
    Expect(activeReferencePlanes.size() == 2U &&
               activeReferencePlanes.front()->id == "reference_plane_top" &&
               activeReferencePlanes.back()->id == "reference_plane_a",
           "World environment service should sort active reference image planes by render precedence");
    const ri::world::ReferenceImagePlane* activeReferencePlane =
        environmentService.GetReferenceImagePlaneAt({0.0f, 2.0f, 0.0f});
    Expect(activeReferencePlane != nullptr && activeReferencePlane->id == "reference_plane_top",
           "World environment service should resolve the top-most active reference image plane at a position");
    const std::vector<const ri::world::Text3dPrimitive*> activeText3dPrimitives =
        environmentService.GetText3dPrimitivesAt({0.0f, 2.0f, 0.0f});
    Expect(activeText3dPrimitives.size() == 2U &&
               activeText3dPrimitives.front()->id == "text3d_header_a" &&
               activeText3dPrimitives.back()->id == "text3d_label_a",
           "World environment service should sort active 3D text primitives by strongest presentation first");
    const ri::world::Text3dPrimitive* activeText3d =
        environmentService.GetText3dPrimitiveAt({0.0f, 2.0f, 0.0f});
    Expect(activeText3d != nullptr &&
               activeText3d->id == "text3d_header_a" &&
               activeText3d->alwaysFaceCamera,
           "World environment service should resolve the top 3D text primitive at a position");
    const std::vector<const ri::world::AnnotationCommentPrimitive*> activeComments =
        environmentService.GetAnnotationCommentPrimitivesAt({0.0f, 2.0f, 0.0f});
    Expect(activeComments.size() == 2U &&
               activeComments.front()->id == "comment_b" &&
               activeComments.back()->id == "comment_a",
           "World environment service should sort active annotation comments by strongest presentation first");
    const ri::world::AnnotationCommentPrimitive* activeComment =
        environmentService.GetAnnotationCommentPrimitiveAt({0.0f, 2.0f, 0.0f});
    Expect(activeComment != nullptr &&
               activeComment->id == "comment_b" &&
               activeComment->text == "CHECK CEILING TRIM",
           "World environment service should resolve the top annotation comment at a position");
    const std::vector<const ri::world::MeasureToolPrimitive*> activeMeasureTools =
        environmentService.GetMeasureToolPrimitivesAt({0.0f, 2.0f, 0.0f});
    Expect(activeMeasureTools.size() == 2U &&
               activeMeasureTools.front()->id == "measure_line_a" &&
               activeMeasureTools.back()->id == "measure_box_a",
           "World environment service should prioritize active measure tools by presentation strength");
    const ri::world::MeasureToolPrimitive* activeMeasureTool =
        environmentService.GetMeasureToolPrimitiveAt({0.0f, 2.0f, 0.0f});
    Expect(activeMeasureTool != nullptr && activeMeasureTool->id == "measure_line_a",
           "World environment service should resolve the highest-priority active measure tool");
    const std::vector<ri::world::MeasureToolReadout> measureReadouts =
        environmentService.GetMeasureToolReadoutsAt({0.0f, 2.0f, 0.0f});
    Expect(measureReadouts.size() == 2U &&
               measureReadouts.front().id == "measure_line_a" &&
               measureReadouts.front().label == "5.00u" &&
               measureReadouts.front().showWireframe &&
               !measureReadouts.front().showFill,
           "World environment service should compute line-measure readouts from live endpoints");
    Expect(measureReadouts.back().id == "measure_box_a" &&
               measureReadouts.back().label == "4.00 x 2.00 x 6.00u",
           "World environment service should compute box-measure readouts from authored dimensions");
    const std::vector<const ri::world::RenderTargetSurface*> activeRenderTargets =
        environmentService.GetRenderTargetSurfacesAt({0.0f, 2.0f, 0.0f});
    Expect(activeRenderTargets.size() == 1U &&
               activeRenderTargets.front()->id == "render_target_surface_a",
           "World environment service should report active render-target surfaces at a position");
    const ri::world::RenderTargetSurface* activeRenderTargetSurface =
        environmentService.GetRenderTargetSurfaceAt({0.0f, 2.0f, 0.0f});
    Expect(activeRenderTargetSurface != nullptr &&
               activeRenderTargetSurface->id == "render_target_surface_a",
           "World environment service should resolve the top render-target surface at a position");
    const std::vector<const ri::world::PlanarReflectionSurface*> activePlanarReflections =
        environmentService.GetPlanarReflectionSurfacesAt({0.0f, 2.0f, 0.0f});
    Expect(activePlanarReflections.size() == 1U &&
               activePlanarReflections.front()->id == "planar_reflection_surface_a",
           "World environment service should report active planar-reflection surfaces at a position");
    const ri::world::PlanarReflectionSurface* activePlanarReflectionSurface =
        environmentService.GetPlanarReflectionSurfaceAt({0.0f, 2.0f, 0.0f});
    Expect(activePlanarReflectionSurface != nullptr &&
               activePlanarReflectionSurface->id == "planar_reflection_surface_a",
           "World environment service should resolve the top planar-reflection surface at a position");
    const std::vector<ri::world::DynamicSurfaceRenderState> dynamicSurfaceStates =
        environmentService.GetDynamicSurfaceRenderStates({0.0f, 2.0f, 0.0f}, 6ULL);
    Expect(dynamicSurfaceStates.size() == 2U &&
               dynamicSurfaceStates.front().kind == ri::world::DynamicSurfaceKind::PlanarReflection &&
               dynamicSurfaceStates.front().effectiveResolution == 384 &&
               dynamicSurfaceStates.front().shouldUpdate &&
               dynamicSurfaceStates.back().kind == ri::world::DynamicSurfaceKind::RenderTarget &&
               dynamicSurfaceStates.back().shouldUpdate &&
               dynamicSurfaceStates.back().effectiveResolution == 256,
           "World environment service should compute dynamic-surface update states with resolution caps and frame cadence");
    const std::vector<const ri::world::PassThroughPrimitive*> activePassThroughPrimitives =
        environmentService.GetPassThroughPrimitivesAt({0.0f, 2.0f, 0.0f});
    Expect(activePassThroughPrimitives.size() == 2U &&
               activePassThroughPrimitives.front()->id == "pass_through_ghost_a" &&
               activePassThroughPrimitives.back()->id == "pass_through_blocking_warning_a",
           "World environment service should resolve pass-through primitives by visible precedence");
    const ri::world::PassThroughPrimitive* activePassThroughPrimitive =
        environmentService.GetPassThroughPrimitiveAt({0.0f, 2.0f, 0.0f});
    Expect(activePassThroughPrimitive != nullptr &&
               activePassThroughPrimitive->id == "pass_through_ghost_a",
           "World environment service should return the top pass-through primitive at a position");
    const std::vector<ri::world::PassThroughVisualState> passThroughStates =
        environmentService.GetPassThroughVisualStates({0.0f, 2.0f, 0.0f}, 0.75);
    Expect(passThroughStates.size() == 2U &&
               passThroughStates.front().id == "pass_through_ghost_a" &&
               passThroughStates.front().effectiveOpacity >= 0.20f &&
               passThroughStates.front().effectiveOpacity <= 0.40f &&
               passThroughStates.front().hasPulse &&
               passThroughStates.front().hasDistanceFade &&
               !passThroughStates.front().blocksAnyChannel,
           "World environment service should animate pass-through opacity using pulse and distance-fade behavior");
    Expect(passThroughStates.back().id == "pass_through_blocking_warning_a" &&
               passThroughStates.back().hasConfiguredEvents &&
               passThroughStates.back().blocksAnyChannel &&
               passThroughStates.back().invisibleWallRisk,
           "World environment service should flag invisible-wall risk when blocking pass-through visuals are too transparent");
    const std::vector<const ri::world::SkyProjectionSurface*> activeSkyProjectionSurfaces =
        environmentService.GetSkyProjectionSurfacesAt({0.0f, 12.0f, -32.0f});
    Expect(activeSkyProjectionSurfaces.size() == 1U &&
               activeSkyProjectionSurfaces.front()->id == "sky_projection_surface_a",
           "World environment service should report active sky-projection surfaces at a position");
    const ri::world::SkyProjectionSurface* activeSkyProjectionSurface =
        environmentService.GetSkyProjectionSurfaceAt({0.0f, 12.0f, -32.0f});
    Expect(activeSkyProjectionSurface != nullptr &&
               activeSkyProjectionSurface->id == "sky_projection_surface_a",
           "World environment service should resolve the top sky-projection surface");
    const std::vector<ri::world::SkyProjectionSurfaceState> skyProjectionStates =
        environmentService.GetSkyProjectionSurfaceStates({4.0f, 2.0f, 8.0f}, 0.6f);
    Expect(skyProjectionStates.size() == 1U &&
               skyProjectionStates.front().id == "sky_projection_surface_a" &&
               skyProjectionStates.front().backgroundLayer &&
               skyProjectionStates.front().requiresPerFrameUpdate &&
               skyProjectionStates.front().projectedYawRadians == 0.6f &&
               NearlyEqual(skyProjectionStates.front().effectiveOpacity, 0.92f),
           "World environment service should compute sky-projection camera-follow and parallax state");
    const std::vector<const ri::world::VolumetricEmitterBounds*> activeEmitters =
        environmentService.GetVolumetricEmitterBoundsAt({0.0f, 2.0f, 0.0f});
    Expect(activeEmitters.size() == 1U &&
               activeEmitters.front()->id == "volumetric_emitter_a",
           "World environment service should report active volumetric emitter bounds at a position");
    const ri::world::VolumetricEmitterBounds* activeEmitter =
        environmentService.GetVolumetricEmitterBoundsAtPoint({0.0f, 2.0f, 0.0f});
    Expect(activeEmitter != nullptr &&
               activeEmitter->id == "volumetric_emitter_a",
           "World environment service should resolve the top volumetric emitter at a position");
    const std::vector<ri::world::VolumetricEmitterRuntimeState> emitterStates =
        environmentService.GetVolumetricEmitterRuntimeStates({0.0f, 2.0f, 0.0f});
    Expect(emitterStates.size() == 1U &&
               emitterStates.front().id == "volumetric_emitter_a" &&
               emitterStates.front().shouldSimulate &&
               emitterStates.front().withinDistanceGate &&
               emitterStates.front().insideVolume &&
               emitterStates.front().effectiveParticleCount == 128U &&
               NearlyEqual(emitterStates.front().spawnRatePerSecond, 12.0f),
           "World environment service should compute volumetric-emitter runtime simulation state");
    ri::world::VolumetricEmitterBounds emitterTieB = volumetricEmitter;
    emitterTieB.id = "volumetric_emitter_b";
    emitterTieB.emission.particleCount = volumetricEmitter.emission.particleCount;
    ri::world::VolumetricEmitterBounds emitterTieA = volumetricEmitter;
    emitterTieA.id = "volumetric_emitter_a2";
    emitterTieA.emission.particleCount = volumetricEmitter.emission.particleCount;
    emitterTieA.emission.loop = false;
    emitterTieA.emission.spawnRatePerSecond = 0.0f;
    emitterTieA.particleSpawn = ri::world::ParticleSpawnAuthoring{};
    emitterTieA.particleSpawn->activation.strictInnerVolumeOnly = true;
    emitterTieA.particleSpawn->emissionPolicy.burstCountOnEnter = 6U;
    environmentService.SetVolumetricEmitterBounds({emitterTieB, emitterTieA});
    const std::vector<const ri::world::VolumetricEmitterBounds*> deterministicEmitterOrder =
        environmentService.GetVolumetricEmitterBoundsAt({0.0f, 2.0f, 0.0f});
    Expect(deterministicEmitterOrder.size() == 2U
               && deterministicEmitterOrder.front()->id == "volumetric_emitter_a2"
               && deterministicEmitterOrder.back()->id == "volumetric_emitter_b",
           "World environment service should deterministically sort overlapping volumetric emitters when distance and particle budgets tie");
    const std::vector<ri::world::VolumetricEmitterRuntimeState> emitterOneShotStates =
        environmentService.GetVolumetricEmitterRuntimeStates({0.0f, 2.0f, 0.0f});
    const auto oneShotIt = std::find_if(
        emitterOneShotStates.begin(),
        emitterOneShotStates.end(),
        [](const ri::world::VolumetricEmitterRuntimeState& state) {
            return state.id == "volumetric_emitter_a2";
        });
    Expect(oneShotIt != emitterOneShotStates.end()
               && oneShotIt->shouldSimulate
               && oneShotIt->strictInnerVolumeOnly
               && oneShotIt->burstOnEnterCount == 6U,
           "World environment service should keep one-shot particle-spawn emitters simulation-eligible when burst-on-enter authoring is present");
    const std::vector<ri::world::AmbientAudioContribution> ambientContributions =
        environmentService.GetAmbientAudioContributionsAt({0.0f, 0.0f, 0.0f});
    Expect(ambientContributions.size() == 2U,
           "World environment service should report ambient audio contributions for overlapping authored helpers");
    const auto ambientBoxIt = std::find_if(
        ambientContributions.begin(),
        ambientContributions.end(),
        [](const ri::world::AmbientAudioContribution& contribution) {
            return contribution.id == "ambient_box_a";
        });
    const auto ambientSplineIt = std::find_if(
        ambientContributions.begin(),
        ambientContributions.end(),
        [](const ri::world::AmbientAudioContribution& contribution) {
            return contribution.id == "ambient_spline_a";
        });
    Expect(ambientBoxIt != ambientContributions.end() &&
               NearlyEqual(ambientBoxIt->desiredVolume, 0.5f) &&
               NearlyEqual(ambientBoxIt->distance, 0.0f),
           "World environment service should treat points inside ambient audio volumes as full-strength contributions");
    Expect(ambientSplineIt != ambientContributions.end() &&
               ambientSplineIt->desiredVolume > 0.0f &&
               ambientSplineIt->distance >= 0.0f,
           "World environment service should compute spline-based ambient audio contributions");
    const ri::world::AmbientAudioMixState ambientMixState =
        environmentService.GetAmbientAudioMixStateAt({0.0f, 0.0f, 0.0f});
    Expect(ambientMixState.contributions.size() == 2U &&
               ambientMixState.topContributionId == ambientMixState.contributions.front().id &&
               NearlyEqual(ambientMixState.topDesiredVolume, ambientMixState.contributions.front().desiredVolume) &&
               ambientMixState.combinedDesiredVolume >= ambientMixState.topDesiredVolume,
           "World environment service should expose combined ambient-audio mix state");

    const ri::world::RuntimeEnvironmentState environmentState =
        environmentService.GetActiveEnvironmentStateAt({0.0f, 0.0f, 0.0f});
    Expect(environmentState.constraints.lockAxes.size() == 2U &&
               environmentState.waterSurface.inside &&
               environmentState.waterSurface.surface != nullptr &&
               environmentState.waterSurface.surface->id == "water_surface_a" &&
               !environmentState.kinematicMotion.activeTranslationPrimitives.empty() &&
               !environmentState.kinematicMotion.activeRotationPrimitives.empty(),
           "World environment service should surface physics/water/kinematic state through the combined environment query");

    ri::world::FogBlockerVolume fogBlocker{};
    fogBlocker.id = "fog_blocker_a";
    fogBlocker.shape = ri::world::VolumeShape::Box;
    fogBlocker.position = {0.0f, 0.0f, 0.0f};
    fogBlocker.size = {4.0f, 4.0f, 4.0f};
    environmentService.SetFogBlockerVolumes({fogBlocker});

    const ri::world::PostProcessState blockedPostState = environmentService.GetActivePostProcessStateAt({0.0f, 0.0f, 0.0f});
    Expect(blockedPostState.label == "pp_a,pp_b,fluid_a",
           "World environment service should suppress localized fog when a fog blocker is active");
    Expect(NearlyEqual(blockedPostState.blurAmount, 0.003f),
           "World environment service should fall back to non-fog blur sources when fog is blocked");
    ExpectVec3(blockedPostState.tintColor, {0.42f, 0.74f, 1.0f},
               "World environment service should keep fluid tint-color overrides when fog is blocked");

    ri::runtime::RuntimeEventBusMetrics eventBusMetrics{};
    eventBusMetrics.emitted = 6;
    eventBusMetrics.activeListeners = 2;
    eventBusMetrics.listenersAdded = 4;
    eventBusMetrics.listenersRemoved = 1;

    ri::validation::SchemaValidationMetrics schemaMetrics{};
    schemaMetrics.levelValidations = 7;
    schemaMetrics.levelValidationFailures = 2;
    schemaMetrics.tuningParses = 5;
    schemaMetrics.tuningParseFailures = 1;

    ri::audio::AudioManagerMetrics audioMetrics{};
    audioMetrics.managedSounds = 3;
    audioMetrics.loopsCreated = 1;
    audioMetrics.oneShotsPlayed = 2;
    audioMetrics.voicesPlayed = 1;
    audioMetrics.voiceActive = true;
    audioMetrics.environmentChanges = 2;
    audioMetrics.activeEnvironment = "reverb_a";
    audioMetrics.activeEnvironmentMix = 0.5;

    ri::world::RuntimeStatsOverlayMetrics statsMetrics{};
    statsMetrics.enabled = true;
    statsMetrics.attached = true;
    statsMetrics.visible = true;

    ri::world::HelperActivityState activity{};
    activity.lastAudioEvent = "environment changed";
    activity.lastStateChange = "flashlight=on";
    activity.lastTriggerEvent = "trigger_a:entered";
    activity.lastEntityIoEvent = "relay_a:trigger";
    activity.lastMessage = "  engine   ready  ";
    activity.lastLevelEvent = "facility_start";
    activity.lastSchemaEvent = "facility:ok";

    const ri::world::RuntimeHelperMetricsSnapshot helperSnapshot = ri::world::BuildRuntimeHelperMetricsSnapshot(
        "session_123456789012345678901234567890",
        eventBusMetrics,
        schemaMetrics,
        audioMetrics,
        statsMetrics,
        activity,
        environmentService,
        ri::structural::StructuralGraphSummary{
            .nodeCount = 4,
            .edgeCount = 3,
            .cycleCount = 1,
            .unresolvedDependencyCount = 2,
            .unresolvedDependencies = {},
            .phaseBuckets = ri::structural::StructuralPhaseBuckets{
                .compile = 1,
                .runtime = 1,
                .postBuild = 1,
                .frame = 1,
            },
        },
        blockedPostState);

    Expect(helperSnapshot.schemaValidations == 7U && helperSnapshot.audioManagedSounds == 3U,
           "World helper metrics should mirror validation and audio counts");
    Expect(helperSnapshot.runtimeSession == "session:1234567890123456789~",
           "World helper metrics should normalize and trim runtime session labels");
    Expect(helperSnapshot.lastMessage == "engine ready",
           "World helper metrics should summarize helper activity strings");
    Expect(helperSnapshot.structuralDeferredOperations == 0U
               && helperSnapshot.structuralDeferredUnsupportedOperations == 0U
               && helperSnapshot.structuralDeferredHealth == "none"
               && helperSnapshot.structuralDeferredStatusLine == "none"
               && helperSnapshot.structuralDeferredSummary == "none",
           "World helper metrics should default deferred structural diagnostics when no pipeline result is supplied");

    ri::structural::StructuralDeferredPipelineResult deferredMetrics{};
    deferredMetrics.unsupportedOperationIds = {"unsupported_exec"};
    deferredMetrics.deferredExecution.operationStats.push_back(ri::structural::StructuralDeferredOperationStats{
        .operationId = "supported_exec",
        .normalizedType = "shrinkwrap_modifier_primitive",
        .targetCount = 2U,
        .generatedNodeCount = 1U,
        .succeeded = true,
    });
    deferredMetrics.deferredExecution.operationStats.push_back(ri::structural::StructuralDeferredOperationStats{
        .operationId = "unsupported_exec",
        .normalizedType = "unknown_modifier",
        .targetCount = 1U,
        .skippedUnsupportedType = true,
    });
    const ri::world::RuntimeHelperMetricsSnapshot deferredHelperSnapshot = ri::world::BuildRuntimeHelperMetricsSnapshot(
        "session_123",
        eventBusMetrics,
        schemaMetrics,
        audioMetrics,
        statsMetrics,
        activity,
        environmentService,
        std::nullopt,
        blockedPostState,
        deferredMetrics);
    Expect(deferredHelperSnapshot.structuralDeferredOperations == 2U
               && deferredHelperSnapshot.structuralDeferredUnsupportedOperations == 1U
               && deferredHelperSnapshot.structuralDeferredHealth == "unsupported"
               && deferredHelperSnapshot.structuralDeferredStatusLine == "unsupported (1/2)"
               && deferredHelperSnapshot.structuralDeferredSummary != "none",
           "World helper metrics should expose deferred structural operation diagnostics and unsupported-operation counts");

    ri::structural::StructuralDeferredPipelineResult warningDeferredMetrics{};
    warningDeferredMetrics.deferredExecution.operationStats.push_back(ri::structural::StructuralDeferredOperationStats{
        .operationId = "warning_exec",
        .normalizedType = "sdf_intersection_node",
        .targetCount = 1U,
        .succeeded = false,
    });
    const ri::world::RuntimeHelperMetricsSnapshot warningDeferredHelperSnapshot = ri::world::BuildRuntimeHelperMetricsSnapshot(
        "session_warn",
        eventBusMetrics,
        schemaMetrics,
        audioMetrics,
        statsMetrics,
        activity,
        environmentService,
        std::nullopt,
        blockedPostState,
        warningDeferredMetrics);
    Expect(warningDeferredHelperSnapshot.structuralDeferredHealth == "warnings"
               && warningDeferredHelperSnapshot.structuralDeferredStatusLine == "warnings (0/1)"
               && warningDeferredHelperSnapshot.structuralDeferredUnsupportedOperations == 0U,
           "World helper metrics should report warning deferred health when operations fail without unsupported types");

    ri::structural::StructuralDeferredPipelineResult okDeferredMetrics{};
    okDeferredMetrics.deferredExecution.operationStats.push_back(ri::structural::StructuralDeferredOperationStats{
        .operationId = "ok_exec",
        .normalizedType = "shrinkwrap_modifier_primitive",
        .targetCount = 1U,
        .succeeded = true,
    });
    const ri::world::RuntimeHelperMetricsSnapshot okDeferredHelperSnapshot = ri::world::BuildRuntimeHelperMetricsSnapshot(
        "session_ok",
        eventBusMetrics,
        schemaMetrics,
        audioMetrics,
        statsMetrics,
        activity,
        environmentService,
        std::nullopt,
        blockedPostState,
        okDeferredMetrics);
    Expect(okDeferredHelperSnapshot.structuralDeferredHealth == "ok"
               && okDeferredHelperSnapshot.structuralDeferredStatusLine == "ok (0/1)"
               && okDeferredHelperSnapshot.structuralDeferredOperations == 1U,
           "World helper metrics should report ok deferred health when all deferred operations succeed");
    Expect(helperSnapshot.postProcessVolumes >= 1U
           && helperSnapshot.audioReverbVolumes >= 1U
           && helperSnapshot.audioOcclusionVolumes >= 1U
           && helperSnapshot.ambientAudioVolumes >= 1U
           && helperSnapshot.localizedFogVolumes >= 1U
           && helperSnapshot.volumetricFogBlockers >= 1U
           && helperSnapshot.fluidSimulationVolumes >= 1U
           && helperSnapshot.physicsModifierVolumes >= 1U
           && helperSnapshot.surfaceVelocityPrimitives >= 1U
           && helperSnapshot.waterSurfacePrimitives >= 1U
           && helperSnapshot.radialForceVolumes >= 1U
           && helperSnapshot.physicsConstraintVolumes >= 1U
           && helperSnapshot.kinematicTranslationPrimitives >= 1U
           && helperSnapshot.kinematicRotationPrimitives >= 1U
           && helperSnapshot.splinePathFollowerPrimitives >= 1U
           && helperSnapshot.cablePrimitives >= 1U
           && helperSnapshot.clippingVolumes >= 1U
           && helperSnapshot.filteredCollisionVolumes >= 1U
           && helperSnapshot.traversalLinkVolumes >= 1U
           && helperSnapshot.localGridSnapVolumes >= 1U
           && helperSnapshot.hintPartitionVolumes >= 1U
           && helperSnapshot.cameraConfinementVolumes >= 1U
           && helperSnapshot.lodOverrideVolumes >= 1U
           && helperSnapshot.lodSwitchPrimitives >= 1U
           && helperSnapshot.lodSwitchSwitches >= 2U
           && helperSnapshot.lodSwitchThrashWarnings == 0U,
           "World helper metrics should report core environment and lod-switch counts");
    Expect(helperSnapshot.surfaceScatterVolumes == 1U
           && helperSnapshot.surfaceScatterActiveInstances >= 1U
           && helperSnapshot.splineMeshDeformers == 1U
           && helperSnapshot.splineMeshDeformerSegments == 17U
           && helperSnapshot.splineDecalRibbons == 1U
           && helperSnapshot.splineDecalRibbonTriangles == 38U
           && helperSnapshot.topologicalUvRemappers == 1U
           && helperSnapshot.triPlanarNodes == 1U
           && helperSnapshot.proceduralUvProjectionEstimatedPatches == 3U
           && helperSnapshot.instanceCloudPrimitives == 1U
           && helperSnapshot.instanceCloudActiveInstances == 64U
           && helperSnapshot.voronoiFracturePrimitives == 1U
           && helperSnapshot.metaballPrimitives == 1U
           && helperSnapshot.latticeVolumes == 1U
           && helperSnapshot.manifoldSweepPrimitives == 1U
           && helperSnapshot.trimSheetSweepPrimitives == 1U
           && helperSnapshot.lSystemBranchPrimitives == 1U
           && helperSnapshot.geodesicSpherePrimitives == 1U
           && helperSnapshot.extrudeAlongNormalPrimitives == 1U
           && helperSnapshot.superellipsoidPrimitives == 1U
           && helperSnapshot.primitiveDemoLatticePrimitives == 1U
           && helperSnapshot.primitiveDemoVoronoiPrimitives == 1U
           && helperSnapshot.thickPolygonPrimitives == 1U
           && helperSnapshot.structuralProfilePrimitives == 1U
           && helperSnapshot.halfPipePrimitives == 1U
           && helperSnapshot.quarterPipePrimitives == 1U
           && helperSnapshot.pipeElbowPrimitives == 1U
           && helperSnapshot.torusSlicePrimitives == 1U
           && helperSnapshot.splineSweepPrimitives == 1U
           && helperSnapshot.revolvePrimitives == 1U
           && helperSnapshot.domeVaultPrimitives == 1U
           && helperSnapshot.loftPrimitives == 1U,
           "World helper metrics should report procedural authoring primitive counts across all migration batches");
    Expect(helperSnapshot.navmeshModifierVolumes >= 1U
           && helperSnapshot.reflectionProbeVolumes >= 1U
           && helperSnapshot.lightImportanceVolumes >= 1U
           && helperSnapshot.lightPortalVolumes >= 1U
           && helperSnapshot.voxelGiBoundsVolumes >= 1U
           && helperSnapshot.lightmapDensityVolumes >= 1U
           && helperSnapshot.shadowExclusionVolumes >= 1U
           && helperSnapshot.cullingDistanceVolumes >= 1U
           && helperSnapshot.referenceImagePlanes >= 1U
           && helperSnapshot.text3dPrimitives >= 1U
           && helperSnapshot.annotationCommentPrimitives >= 1U
           && helperSnapshot.measureToolPrimitives >= 1U
           && helperSnapshot.renderTargetSurfaces >= 1U
           && helperSnapshot.planarReflectionSurfaces >= 1U
           && helperSnapshot.passThroughPrimitives >= 1U
           && helperSnapshot.skyProjectionSurfaces >= 1U
           && helperSnapshot.volumetricEmitterBounds >= 1U
           && helperSnapshot.portalPrimitives >= 1U
           && helperSnapshot.antiPortalPrimitives >= 1U
           && helperSnapshot.occlusionPortalVolumes >= 1U
           && helperSnapshot.closedOcclusionPortals == 0U,
           "World helper metrics should report environment-volume counts from the runtime service");
    Expect(helperSnapshot.structuralGraphNodes == 4U
           && helperSnapshot.structuralGraphPostBuildPhase == 1U
           && helperSnapshot.structuralGraphFramePhase == 1U,
           "World helper metrics should mirror structural graph summary counts");
    Expect(helperSnapshot.activePostProcess == "pp_a,pp_b,fluid_a"
           && NearlyEqual(static_cast<float>(helperSnapshot.activePostProcessTint), 0.7f),
           "World helper metrics should report the active post-process label and tint strength");
    Expect(helperSnapshot.statsOverlayEnabled && helperSnapshot.statsOverlayAttached,
           "World helper metrics should report stats overlay visibility and attachment state");
}

void TestWorldInstrumentation() {
    ri::runtime::RuntimeEventBus bus;
    ri::world::HelperActivityTracker tracker;
    tracker.Attach(bus);

    bus.Emit("audioChanged", ri::runtime::RuntimeEvent{
        .id = "",
        .type = "",
        .fields = {
            {"type", "oneShot"},
            {"channel", "sfx"},
        },
    });
    bus.Emit("stateChanged", ri::runtime::RuntimeEvent{
        .id = "",
        .type = "",
        .fields = {
            {"key", "flashlightOn"},
            {"value", "true"},
        },
    });
    bus.Emit("triggerChanged", ri::runtime::RuntimeEvent{
        .id = "",
        .type = "",
        .fields = {
            {"triggerId", "bay_trigger"},
            {"state", "enter"},
        },
    });
    bus.Emit("entityIo", ri::runtime::RuntimeEvent{
        .id = "",
        .type = "",
        .fields = {
            {"sourceId", "relay_a"},
            {"inputName", "Trigger"},
            {"kind", "signal"},
        },
    });
    bus.Emit("message", ri::runtime::RuntimeEvent{
        .id = "",
        .type = "",
        .fields = {
            {"text", "  hello   from   rawiron  "},
        },
    });
    bus.Emit("levelLoaded", ri::runtime::RuntimeEvent{
        .id = "",
        .type = "",
        .fields = {
            {"levelFilename", "facility.ri_scene"},
        },
    });
    bus.Emit("schemaValidated", ri::runtime::RuntimeEvent{
        .id = "",
        .type = "",
        .fields = {
            {"target", "facility"},
            {"ok", "true"},
        },
    });

    const ri::world::HelperActivityState& activity = tracker.GetState();
    Expect(activity.lastAudioEvent == "oneShot:sfx",
           "World helper activity tracking should summarize audio change events");
    Expect(activity.lastStateChange == "flashlightOn=true",
           "World helper activity tracking should summarize state-change events");
    Expect(activity.lastTriggerEvent == "bay_trigger:enter",
           "World helper activity tracking should summarize trigger events");
    Expect(activity.lastEntityIoEvent == "relay_a:Trigger",
           "World helper activity tracking should summarize entity-I/O events");
    Expect(activity.lastMessage == "hello from rawiron",
           "World helper activity tracking should collapse whitespace in message events");
    Expect(activity.lastLevelEvent == "facility.ri_scene",
           "World helper activity tracking should preserve loaded-level labels");
    Expect(activity.lastSchemaEvent == "facility:ok",
           "World helper activity tracking should summarize schema validation results");

    tracker.Detach();
    bus.Emit("message", ri::runtime::RuntimeEvent{
        .id = "",
        .type = "",
        .fields = {
            {"text", "this should not land"},
        },
    });
    Expect(tracker.GetState().lastMessage == "hello from rawiron",
           "World helper activity tracking should stop observing once detached");
    tracker.Reset();
    Expect(tracker.GetState().lastMessage == "none" && tracker.GetState().lastAudioEvent == "none",
           "World helper activity tracking should reset to the prototype default state");

    ri::world::EntityIoTracker ioTracker;
    ioTracker.IncrementOutputsFired();
    ioTracker.IncrementInputsDispatched(2);
    ioTracker.IncrementTimersStarted();
    ioTracker.IncrementTimersCancelled();
    ioTracker.IncrementCountersChanged(3);
    for (int index = 0; index < 16; ++index) {
        ioTracker.RecordEvent("signal", static_cast<double>(index) * 0.5, {
            {"sourceId", "relay_" + std::to_string(index)},
        });
    }

    const ri::world::EntityIoStats& ioStats = ioTracker.GetStats();
    Expect(ioStats.outputsFired == 1U
           && ioStats.inputsDispatched == 2U
           && ioStats.timersStarted == 1U
           && ioStats.timersCancelled == 1U
           && ioStats.countersChanged == 3U,
           "Entity-I/O tracker should accumulate the prototype counter set");
    Expect(ioTracker.GetHistory().size() == 14U,
           "Entity-I/O tracker should cap history to the prototype helper limit");
    Expect(ioTracker.GetHistory().front().fields.at("sourceId") == "relay_15",
           "Entity-I/O tracker should keep the newest history entry at the front");
    Expect(ioTracker.GetHistory().back().fields.at("sourceId") == "relay_2",
           "Entity-I/O tracker should discard the oldest entries past the history limit");
    ioTracker.Clear();
    Expect(ioTracker.GetHistory().empty() && ioTracker.GetStats().outputsFired == 0U,
           "Entity-I/O tracker clear should reset history and stats");

    ri::world::SpatialQueryTracker spatialTracker;
    spatialTracker.RecordCollisionIndexBuild(2.5);
    spatialTracker.RecordInteractionIndexBuild(1.25);
    spatialTracker.RecordTriggerIndexBuild(0.75);
    spatialTracker.RecordCollidableBoxQuery(4, 2);
    spatialTracker.RecordCollidableRayQuery(3, 1);
    spatialTracker.RecordInteractiveRayQuery(5);
    spatialTracker.RecordTriggerPointQuery(6);

    const ri::world::SpatialQueryStats& spatialStats = spatialTracker.GetStats();
    Expect(spatialStats.collisionIndexBuilds == 1U
           && spatialStats.interactionIndexBuilds == 1U
           && spatialStats.triggerIndexBuilds == 1U,
           "Spatial query tracker should count index rebuilds");
    Expect(NearlyEqual(static_cast<float>(spatialStats.lastCollisionRebuildMs), 2.5f)
           && NearlyEqual(static_cast<float>(spatialStats.lastInteractionRebuildMs), 1.25f)
           && NearlyEqual(static_cast<float>(spatialStats.lastTriggerRebuildMs), 0.75f),
           "Spatial query tracker should preserve last rebuild timings");
    Expect(spatialStats.collidableBoxQueries == 1U
           && spatialStats.collidableRayQueries == 1U
           && spatialStats.interactiveRayQueries == 1U
           && spatialStats.triggerPointQueries == 1U,
           "Spatial query tracker should count each query type");
    Expect(spatialStats.staticCandidates == 7U
           && spatialStats.dynamicCandidates == 3U
           && spatialStats.interactiveCandidates == 5U
           && spatialStats.triggerCandidates == 6U,
           "Spatial query tracker should accumulate candidate counts like the prototype runtime");
    spatialTracker.Clear();
    Expect(spatialTracker.GetStats().collidableBoxQueries == 0U && spatialTracker.GetStats().staticCandidates == 0U,
           "Spatial query tracker clear should reset all counters");

    ri::world::RuntimeStatsOverlayState overlayState(true);
    overlayState.SetAttached(true);
    overlayState.SetVisible(true);
    const ri::world::RuntimeStatsOverlayMetrics overlayMetrics = overlayState.GetMetrics();
    Expect(overlayMetrics.enabled && overlayMetrics.attached && overlayMetrics.visible,
           "Runtime stats overlay state should mirror the prototype visible/attached flags");

    ri::world::LogicEntityStateMap logicStates;
    logicStates["door_a"]["open"] = true;
    logicStates["door_a"]["progress"] = 0.5;

    std::set<std::string> worldFlags{"power_on"};
    std::unordered_map<std::string, double> worldValues{{"danger", 2.0}};

    ri::world::RuntimeHelperMetricsSnapshot helperMetrics{};
    helperMetrics.schemaValidations = 9;
    helperMetrics.eventBusEmits = 12;
    helperMetrics.runtimeSession = "session:abc";
    helperMetrics.statsOverlayEnabled = true;

    ri::world::InfoPanelBindingContext panelContext{};
    panelContext.logicEntities = &logicStates;
    panelContext.worldFlags = &worldFlags;
    panelContext.worldValues = &worldValues;
    panelContext.helperMetrics = &helperMetrics;
    panelContext.runtimeCounts.physicsObjects = 3;
    panelContext.runtimeCounts.triggerVolumes = 7;

    ri::world::InfoPanelBinding logicBinding{};
    logicBinding.logicEntityId = "door_a";
    logicBinding.property = "open";
    Expect(ri::world::FormatInfoPanelValue(ri::world::ResolveInfoPanelBindingValue(
               logicBinding,
               panelContext))
               == "ON",
           "Info-panel binding resolution should map logic-entity booleans to ON/OFF strings");

    ri::world::InfoPanelBinding worldFlagBinding{};
    worldFlagBinding.worldFlag = "power_on";
    Expect(ri::world::FormatInfoPanelValue(ri::world::ResolveInfoPanelBindingValue(
               worldFlagBinding,
               panelContext))
               == "SET",
           "Info-panel binding resolution should report world-flag set/clear state");

    ri::world::InfoPanelBinding worldValueBinding{};
    worldValueBinding.worldValue = "danger";
    Expect(ri::world::FormatInfoPanelValue(ri::world::ResolveInfoPanelBindingValue(
               worldValueBinding,
               panelContext))
               == "2",
           "Info-panel binding resolution should return finite world values");

    ri::world::InfoPanelBinding schemaMetricBinding{};
    schemaMetricBinding.runtimeMetric = "schemaValidations";
    Expect(ri::world::FormatInfoPanelValue(ri::world::ResolveInfoPanelBindingValue(
               schemaMetricBinding,
               panelContext))
               == "9",
           "Info-panel binding resolution should expose helper metrics");

    ri::world::InfoPanelBinding runtimeCountMetricBinding{};
    runtimeCountMetricBinding.runtimeMetric = "physicsObjects";
    Expect(ri::world::FormatInfoPanelValue(ri::world::ResolveInfoPanelBindingValue(
               runtimeCountMetricBinding,
               panelContext))
               == "3",
           "Info-panel binding resolution should expose runtime-count metrics");

    ri::world::InfoPanelBinding schemaLineBinding{};
    schemaLineBinding.label = "schema validations";
    schemaLineBinding.runtimeMetric = "schemaValidations";
    ri::world::InfoPanelBinding sessionLineBinding{};
    sessionLineBinding.label = "runtime session";
    sessionLineBinding.runtimeMetric = "runtimeSession";
    ri::world::InfoPanelDefinition panelDefinition{};
    panelDefinition.lines = {"status"};
    panelDefinition.bindings = {schemaLineBinding, sessionLineBinding};
    panelDefinition.replaceBindings = false;

    const std::vector<std::string> lines = ri::world::ResolveInfoPanelLines(
        panelDefinition,
        panelContext);
    Expect(lines.size() == 3U
           && lines[0] == "status"
           && lines[1] == "SCHEMA VALIDATIONS: 9"
           && lines[2] == "RUNTIME SESSION: session:abc",
           "Info-panel line resolution should append uppercased labeled bindings");
}

void TestWorldSpatialDebugAndProxyVolumes() {
    ri::world::RuntimeVolumeSeed seed{};
    seed.id = "proxy_a";
    seed.type = "invisible_structural_proxy_volume";
    seed.size = ri::math::Vec3{2.0f, 4.0f, 6.0f};
    seed.position = ri::math::Vec3{1.0f, 2.0f, 3.0f};
    const ri::world::InvisibleStructuralProxyVolume proxy = ri::world::CreateInvisibleStructuralProxyVolume(
        seed,
        "brush_01",
        0.25f,
        true,
        true,
        false);
    Expect(!proxy.debugVisible && proxy.sourceId == "brush_01" && proxy.logicEnabled == false,
           "Invisible structural proxy volumes should preserve authored source links and remain non-rendered at runtime");
    const std::vector<ri::spatial::SpatialEntry> proxyEntries =
        ri::world::BuildInvisibleStructuralProxySpatialEntries({proxy});
    Expect(proxyEntries.size() == 1U
               && proxyEntries[0].id == "proxy_a"
               && proxyEntries[0].bounds.max.x > proxy.position.x
               && proxyEntries[0].bounds.min.x < proxy.position.x,
           "Invisible structural proxy volumes should emit deterministic proxy spatial geometry for collision/query routing");

    const ri::world::CollisionChannelResolveResult resolvedChannels =
        ri::world::ResolveCollisionChannelAuthoring({"Pawn", "projectile", "bad_channel"});
    Expect(resolvedChannels.channels.size() == 2U
               && resolvedChannels.unknownTokens.size() == 1U
               && !resolvedChannels.usedDefault
               && resolvedChannels.mask != 0U,
           "Collision channel authoring resolver should normalize aliases, report unknown labels, and build runtime masks");

    ri::world::FilteredCollisionVolume volume{};
    volume.channels = resolvedChannels.channels;
    volume.channelMask = resolvedChannels.mask;
    volume.includeTags = {"combat", "loud"};
    volume.excludeTags = {"ghost"};
    volume.requireAllIncludeTags = false;
    volume.allowUntaggedTrace = false;
    Expect(ri::world::TraceAndVolumeTagsMatch("bullet", {"combat", "fast"}, volume),
           "Trace/volume tag matching should allow compatible channels and included routing tags");
    Expect(!ri::world::TraceAndVolumeTagsMatch("bullet", {"ghost", "combat"}, volume)
               && !ri::world::TraceAndVolumeTagsMatch("camera", {"combat"}, volume),
           "Trace/volume tag matching should block excluded tags and incompatible channels deterministically");

    std::vector<ri::world::RuntimeVolume> debugVolumes;
    ri::world::RuntimeVolume trigger{};
    trigger.id = "trigger_box";
    trigger.type = "generic_trigger_volume";
    trigger.debugVisible = true;
    trigger.shape = ri::world::VolumeShape::Box;
    trigger.size = {4.0f, 4.0f, 4.0f};
    debugVolumes.push_back(trigger);
    ri::world::RuntimeVolume hiddenProxy = static_cast<const ri::world::RuntimeVolume&>(proxy);
    debugVolumes.push_back(hiddenProxy);
    const std::vector<ri::world::DebugVolumeDrawItem> visualizerItems =
        ri::world::BuildDebugVolumeVisualizerItems(debugVolumes, ri::world::DebugVolumeVisualizationRule{.includeHidden = true});
    Expect(!visualizerItems.empty()
               && visualizerItems[0].id <= visualizerItems.back().id,
           "Debug volume visualizer should emit stable layered draw items with semantic coloring");

    ri::world::SpatialRelationDebugTracer tracer;
    tracer.Push(ri::world::SpatialRelationDebugSegment{
        .channel = "trace.ray",
        .sourceId = "player",
        .targetId = "door_a",
        .start = {0.0f, 1.0f, 0.0f},
        .end = {4.0f, 1.0f, 0.0f},
        .ttlSeconds = 0.5,
    });
    tracer.Push(ri::world::SpatialRelationDebugSegment{
        .channel = "logic.link",
        .sourceId = "switch_a",
        .targetId = "door_a",
        .start = {1.0f, 1.0f, 0.0f},
        .end = {3.0f, 1.0f, 0.0f},
        .ttlSeconds = 1.0,
    });
    tracer.Tick(0.6);
    const std::vector<ri::world::SpatialRelationDebugSegment> logicSegments = tracer.Query("logic.link");
    const std::vector<ri::world::SpatialRelationDebugSegment> allSegments = tracer.Query();
    Expect(logicSegments.size() == 1U && allSegments.size() == 1U && allSegments[0].channel == "logic.link",
           "Spatial relation debug tracers should persist per-channel diagnostics with deterministic TTL pruning");

    ri::world::DebugHelperRegistry registry;
    registry.Register(ri::world::DebugArtifactRecord{
        .id = "physics:ray:1",
        .category = "physics",
        .source = "trace",
        .ttlSeconds = 0.2,
    });
    registry.Register(ri::world::DebugArtifactRecord{
        .id = "render:overlay:1",
        .category = "render",
        .source = "overlay",
        .ttlSeconds = 1.0,
    });
    registry.Tick(0.3);
    const std::vector<ri::world::DebugArtifactRecord> physicsRecords = registry.QueryByCategory("physics");
    const std::vector<ri::world::DebugArtifactRecord> allRecordsBeforeCleanup = registry.QueryByCategory();
    Expect(physicsRecords.empty() && allRecordsBeforeCleanup.size() == 1U,
           "Debug helper registry should isolate artifacts by category and prune expired entries by lifetime");
    registry.CleanupForHotReload();
    Expect(registry.QueryByCategory().empty(),
           "Debug helper registry should support hot-reload-safe cleanup of all debug artifacts");

    ri::world::RuntimeDebugVisibilityController visibility(ri::world::RuntimeDebugVisibilityPolicy{
        .role = "dev",
        .enablePersistence = true,
        .maxArtifactsPerTick = 4U,
        .enabledDomains = {ri::world::DebugDomain::Physics, ri::world::DebugDomain::Render},
    });
    const std::vector<ri::world::ComposableDebugVisualizationSource> composableSources{
        {
            .sourceId = "p1",
            .volume = trigger,
            .annotation = "trace_source",
            .domain = ri::world::DebugDomain::Physics,
        },
        {
            .sourceId = "a1",
            .volume = hiddenProxy,
            .annotation = "ai_sense",
            .domain = ri::world::DebugDomain::AI,
        },
    };
    const std::vector<ri::world::DebugVolumeDrawItem> composableItems = ri::world::BuildComposableDebugVisualizations(
        composableSources,
        visibility,
        ri::world::DebugVolumeVisualizationRule{
            .includeHidden = true,
            .lodStride = 1U,
        });
    Expect(composableItems.size() == 1U && composableItems[0].type.find("trace_source") != std::string::npos,
           "Composable debug visualization builder should honor visibility domains and attach source-linked annotations");
}

} // namespace

namespace {

const ri::test::TestCase kEngineImportCases[] = {
    {"TestRuntimeIds", TestRuntimeIds},
    {"TestRuntimeIdsConcurrent", TestRuntimeIdsConcurrent},
    {"TestRuntimeTuning", TestRuntimeTuning},
    {"TestRuntimeEventBus", TestRuntimeEventBus},
    {"TestEngineAssetTextureAliases", TestEngineAssetTextureAliases},
    {"TestModelLoaderFallbackDiagnostics", TestModelLoaderFallbackDiagnostics},
    {"TestStructuralGraph", TestStructuralGraph},
    {"TestConvexClipperAndCompiler", TestConvexClipperAndCompiler},
    {"TestStructuralPrimitives", TestStructuralPrimitives},
    {"TestEventEngine", TestEventEngine},
    {"TestSpatialIndex", TestSpatialIndex},
    {"TestTraceScene", TestTraceScene},
    {"TestRuntimeClipQuerySweepAndLevelSchedulers", TestRuntimeClipQuerySweepAndLevelSchedulers},
    {"TestKinematicPhysics", TestKinematicPhysics},
    {"TestOrientedKinematicPhysics", TestOrientedKinematicPhysics},
    {"TestKinematicAdvanceDuration", TestKinematicAdvanceDuration},
    {"TestObjectPhysicsBatch", TestObjectPhysicsBatch},
    {"TestPostProcessProfiles", TestPostProcessProfiles},
    {"TestValidationSchemas", TestValidationSchemas},
    {"TestDataSchema", TestDataSchema},
    {"TestLogicGraph", TestLogicGraph},
    {"TestLogicEntityIoTelemetry", TestLogicEntityIoTelemetry},
    {"TestLogicWorldActors", TestLogicWorldActors},
    {"TestAudioEnvironment", TestAudioEnvironment},
    {"TestAudioManager", TestAudioManager},
    {"TestRuntimeSnapshots", TestRuntimeSnapshots},
    {"TestVulkanCommandListSink", TestVulkanCommandListSink},
    {"TestVulkanCommandRecorder", TestVulkanCommandRecorder},
    {"TestVulkanCommandBufferRecorder", TestVulkanCommandBufferRecorder},
    {"TestVulkanIntentStagingPlan", TestVulkanIntentStagingPlan},
    {"TestVulkanFrameSubmission", TestVulkanFrameSubmission},
    {"TestVulkanPipelineStateCache", TestVulkanPipelineStateCache},
    {"TestContentEnvironmentVolumes", TestContentEnvironmentVolumes},
    {"TestContentValue", TestContentValue},
    {"TestValueSchema", TestValueSchema},
    {"TestGameManifest", TestGameManifest},
    {"TestPipelineArtifacts", TestPipelineArtifacts},
    {"TestScriptedCameraReview", TestScriptedCameraReview},
    {"TestContentPhysicsVolumes", TestContentPhysicsVolumes},
    {"TestContentPrefabExpansion", TestContentPrefabExpansion},
    {"TestAccessFeedbackState", TestAccessFeedbackState},
    {"TestDialogueCueState", TestDialogueCueState},
    {"TestDeveloperConsoleState", TestDeveloperConsoleState},
    {"TestCheckpointPersistence", TestCheckpointPersistence},
    {"TestInventoryState", TestInventoryState},
    {"TestInteractionPromptState", TestInteractionPromptState},
    {"TestLevelFlowPresentationState", TestLevelFlowPresentationState},
    {"TestHeadlessModuleVerifier", TestHeadlessModuleVerifier},
    {"TestNpcAgentState", TestNpcAgentState},
    {"TestHostileCharacterAi", TestHostileCharacterAi},
    {"TestPickupFeedbackState", TestPickupFeedbackState},
    {"TestPlayerVitality", TestPlayerVitality},
    {"TestPresentationState", TestPresentationState},
    {"TestTextOverlayState", TestTextOverlayState},
    {"TestTextOverlayEventBridge", TestTextOverlayEventBridge},
    {"TestTextOverlayEvents", TestTextOverlayEvents},
    {"TestSignalBroadcastState", TestSignalBroadcastState},
    {"TestContentTriggerVolumes", TestContentTriggerVolumes},
    {"TestContentWorldVolumes", TestContentWorldVolumes},
    {"TestRuntimeTriggerVolumes", TestRuntimeTriggerVolumes},
    {"TestRuntimeLocalGridSnap", TestRuntimeLocalGridSnap},
    {"TestStructuralDeferredOperations", TestStructuralDeferredOperations},
    {"TestWorldVolumeDescriptors", TestWorldVolumeDescriptors},
    {"TestScalarClamp", TestScalarClamp},
    {"TestFiniteComponents", TestFiniteComponents},
    {"TestSummarizeHelperActivity", TestSummarizeHelperActivity},
    {"TestRuntimeStatsOverlay", TestRuntimeStatsOverlay},
    {"TestStructuralPhaseClassification", TestStructuralPhaseClassification},
    {"TestSnapshotFormatting", TestSnapshotFormatting},
    {"TestInputLabelFormat", TestInputLabelFormat},
    {"TestVolumeContainment", TestVolumeContainment},
    {"TestWorldRuntimeState", TestWorldRuntimeState},
    {"TestWorldInstrumentation", TestWorldInstrumentation},
    {"TestWorldSpatialDebugAndProxyVolumes", TestWorldSpatialDebugAndProxyVolumes},
    {"TestRawIronGameplayInfrastructureStacks", TestRawIronGameplayInfrastructureStacks},
};

} // namespace

int main(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index] != nullptr ? argv[index] : "");
        if (argument == "--stacktrace-smoke") {
            ri::core::InitializeCrashDiagnostics(ri::core::CrashDiagnosticsConfig{
                .installTerminateHandler = false,
                .installUnhandledExceptionFilter = false,
                .maxStackFrames = 12,
            });
            std::cout << "Stacktrace diagnostics smoke hook\n";
            std::cout << ri::core::CaptureStackTraceText(12) << '\n';
            return 0;
        }
    }

    return ri::test::RunTestHarness(
        "RawIron.EngineImport.Tests",
        std::span<const ri::test::TestCase>(kEngineImportCases),
        argc,
        argv);
}
