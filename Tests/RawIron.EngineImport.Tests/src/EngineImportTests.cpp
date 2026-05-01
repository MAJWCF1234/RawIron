// Merged from former Test*.cpp sources; each section uses `detail_*` for private helpers.

#include "RawIron/World/AccessFeedbackState.h"
#include <stdexcept>
#include <string>
#include "RawIron/Audio/AudioEnvironment.h"
#include "RawIron/Math/Angles.h"
#include <cmath>
#include "RawIron/Content/WorldVolumeContent.h"
#include "RawIron/Content/PrefabExpansion.h"
#include "RawIron/Math/Vec3.h"
#include <array>
#include <limits>
#include <string_view>
#include "RawIron/Content/Value.h"
#include <optional>
#include "RawIron/DataSchema/DataSchema.h"
#include <unordered_set>
#include <vector>
#include "RawIron/World/DialogueCueState.h"
#include "RawIron/World/DeveloperConsoleState.h"
#include "RawIron/World/CheckpointPersistence.h"
#include "RawIron/Math/FiniteComponents.h"
#include <span>
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include <filesystem>
#include <fstream>
#include "RawIron/World/HeadlessModuleVerifier.h"
#include "RawIron/World/NpcAgentState.h"
#include <cstdint>
#include "RawIron/World/HostileCharacterAi.h"
#include "RawIron/Core/ActionBindings.h"
#include "RawIron/Core/InputLabelFormat.h"
#include "RawIron/World/InteractionPromptState.h"
#include "RawIron/World/InventoryState.h"
#include <climits>
#include <unordered_map>
#include "RawIron/World/LevelFlowPresentationState.h"
#include "RawIron/Logic/LogicGraph.h"
#include "RawIron/Logic/LogicTypes.h"
#include "RawIron/Runtime/EntityIoTelemetry.h"
#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/Trace/SpatialQueryHelpers.h"
#include "RawIron/Trace/TraceScene.h"
#include "RawIron/World/Instrumentation.h"
#include "RawIron/World/LogicEntityIoTelemetry.h"
#include <cstddef>
#include "RawIron/Logic/LogicCircuitSignal.h"
#include "RawIron/Logic/WorldActorPorts.h"
#include "RawIron/World/RuntimeState.h"
#include "RawIron/World/WorldLogicBridge.h"
#include "RawIron/World/HeadlessVerification.h"
#include "RawIron/World/PickupFeedbackState.h"
#include "RawIron/Content/DeclarativeModelDefinition.h"
#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/AssetPackageManifest.h"
#include "RawIron/Content/Pipeline/AssetExtractionInventory.h"
#include "RawIron/World/PlayerVitality.h"
#include "RawIron/World/PresentationState.h"
#include "RawIron/Audio/AudioManager.h"
#include "RawIron/Validation/Schemas.h"
#include "RawIron/World/VolumeDescriptors.h"
#include <algorithm>
#include "RawIron/Math/ScalarClamp.h"
#include "RawIron/Scene/ScriptedCameraReview.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/World/SignalBroadcastState.h"
#include "RawIron/World/TextOverlayState.h"
#include "RawIron/World/TextOverlayEventBridge.h"
#include "RawIron/World/TextOverlayEvents.h"
#include "RawIron/Debug/RuntimeSnapshots.h"
#include "RawIron/Debug/SnapshotFormatting.h"
#include "RawIron/Render/RgbaImagePathCache.h"
#include "RawIron/Scene/AnimationLibraryHydration.h"
#include "RawIron/Scene/LoftPrimitiveStack.h"
#include "RawIron/Trace/BvhLifetimeRegistry.h"
#include "RawIron/World/HudChannelTtlScheduler.h"
#include "RawIron/World/MissionCompletionFlow.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Structural/ConvexClipper.h"
#include "RawIron/Structural/StructuralCompiler.h"
#include "RawIron/Structural/StructuralDeferredOperations.h"
#include "RawIron/Structural/StructuralGraph.h"
#include <memory>
#include "RawIron/World/HelperActivitySummary.h"
#include "RawIron/Content/ValueSchema.h"
#include "RawIron/DataSchema/IdFormat.h"
#include "RawIron/DataSchema/ValidationReport.h"
#include "RawIron/Spatial/VolumeContainment.h"

// --- merged from TestAccessFeedbackState.cpp ---
namespace detail_AccessFeedbackState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestAccessFeedbackState() {
    using namespace detail_AccessFeedbackState;
    ri::world::AccessFeedbackState state({
        .mode = ri::world::AccessFeedbackMode::Verbose,
        .allowObjectiveUpdates = true,
        .allowHints = true,
        .historyLimit = 3U,
    });

    state.RecordGranted({
        .requiredItemLabel = "Level 2 Keycard",
        .grantedMessage = "ACCESS GRANTED",
        .deniedMessage = "",
        .unlockObjective = "Reach the lift control room.",
        .unlockHint = "Proceed through the service door.",
        .lockedHint = "",
        .grantedAudioCue = "ui/access_granted_terminal",
        .uiPulseCue = "ui_pulse_small",
        .hapticCue = "haptic_soft",
        .context = ri::world::AccessFeedbackContext::Terminal,
    });
    Expect(state.ActiveMessage().has_value() && state.ActiveMessage()->text == "ACCESS GRANTED",
           "Access feedback should preserve verbose granted messaging");
    Expect(state.ActiveHint().has_value() && state.ActiveHint()->text == "Proceed through the service door.",
           "Access feedback should surface unlock hints when enabled");
    const std::optional<std::string> objective = state.ConsumePendingObjective();
    Expect(objective.has_value() && *objective == "Reach the lift control room.",
           "Access feedback should expose pending unlock objectives");
    Expect(state.ConsumePendingUiPulse() && state.ConsumePendingHapticPulse(),
           "Access feedback should expose mix-safe positive cue pulses for UI/haptic channels");

    state.RecordDenied({
        .requiredItemLabel = "Level 3 Keycard",
        .grantedMessage = "",
        .deniedMessage = "",
        .unlockObjective = "",
        .unlockHint = "",
        .lockedHint = "Search the upper offices.",
        .deniedAudioCue = "ui/access_denied_hard",
        .context = ri::world::AccessFeedbackContext::Door,
        .deniedSeverity = ri::world::AccessDeniedSeverity::High,
    });
    Expect(state.ActiveMessage().has_value()
               && state.ActiveMessage()->text == "Door locked. Level 3 Keycard required."
               && state.ActiveMessage()->severity == ri::world::PresentationSeverity::Critical,
           "Access feedback should build a default denied message when verbose text is absent");
    Expect(state.ActiveHint().has_value() && state.ActiveHint()->text == "Search the upper offices.",
           "Access feedback should surface locked hints when enabled");
    state.RecordDenied({
        .requiredItemLabel = "Level 3 Keycard",
        .lockedHint = "Search the upper offices.",
        .context = ri::world::AccessFeedbackContext::Door,
        .deniedSeverity = ri::world::AccessDeniedSeverity::High,
    });
    Expect(!state.History().empty() && state.History().back().suppressedByCooldown,
           "Access feedback should apply denied-cue cooldown to prevent negative-state spam");

    state.SetPolicy({
        .mode = ri::world::AccessFeedbackMode::Disabled,
        .allowObjectiveUpdates = true,
        .allowHints = true,
        .historyLimit = 2U,
    });
    state.RecordDenied({
        .requiredItemLabel = "Master Key",
        .grantedMessage = "",
        .deniedMessage = "Should not appear.",
        .unlockObjective = "",
        .unlockHint = "",
        .lockedHint = "",
    });
    Expect(!state.ActiveMessage().has_value()
               && !state.ActiveHint().has_value()
               && !state.ConsumePendingObjective().has_value()
               && state.History().empty(),
           "Access feedback should be fully optional when disabled");
}

// --- merged from TestAudioEnvironment.cpp ---
namespace detail_AudioEnvironment {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool NearlyEqual(double lhs, double rhs, double eps = 0.0001) {
    return std::fabs(lhs - rhs) <= eps;
}

}

 // namespace

void TestAudioEnvironment() {
    using namespace detail_AudioEnvironment;
    using namespace ri::audio;

    ri::audio::AudioEnvironmentProfileInput in{};
    in.activeVolumes = {"a", "b"};
    in.reverbMix = 0.75;
    in.echoDelayMs = 250.0;
    in.echoFeedback = 0.8;
    in.dampening = 0.5;
    in.volumeScale = 1.5;
    in.playbackRate = 1.25;

    const AudioEnvironmentProfile norm = NormalizeAudioEnvironmentProfile(in);
    Expect(norm.label == "a,b", "Joined active volumes should become the label when label is empty");
    Expect(NearlyEqual(norm.reverbMix, 0.75), "Finite reverb mix should be preserved");

    ri::audio::AudioEnvironmentProfileInput labeled{};
    labeled.label = "  custom  ";
    labeled.reverbMix = 2.0;
    const AudioEnvironmentProfile t = NormalizeAudioEnvironmentProfile(labeled);
    Expect(t.label == "custom", "Label should trim whitespace");
    Expect(NearlyEqual(t.reverbMix, 1.0), "Reverb mix should clamp to 1");

    const AudioResolvedPlayback dry = MixPlaybackWithEnvironment({.volume = 0.6, .playbackRate = 1.0}, std::nullopt);
    Expect(NearlyEqual(dry.volume, 0.6) && NearlyEqual(dry.playbackRate, 1.0), "Without profile, request should clamp only");

    const AudioResolvedPlayback wet =
        MixPlaybackWithEnvironment({.volume = 0.6, .playbackRate = 1.0}, norm);
    Expect(NearlyEqual(wet.volume, 0.81),
           "Wet mix should apply volumeScale and dampening attenuation (0.6 * 1.5 * 0.9)");
    Expect(NearlyEqual(wet.playbackRate, 1.25), "Wet mix should scale playback rate");

    const auto echoOk = TryResolveAudioEchoLayer(0.5, 1.0, norm);
    Expect(echoOk.has_value(), "Echo layer should spawn when reverb/feedback/delay are positive");
    Expect(echoOk.has_value() && NearlyEqual(echoOk->volume, 0.3),
           "Echo volume should be dry * reverbMix * feedback (0.5 * 0.75 * 0.8)");
    Expect(echoOk.has_value() && NearlyEqual(echoOk->playbackRate, kAudioEchoPlaybackRateScale),
           "Echo rate should use kAudioEchoPlaybackRateScale");

    ri::audio::AudioEnvironmentProfile noEcho = norm;
    noEcho.reverbMix = 0.0;
    Expect(!TryResolveAudioEchoLayer(1.0, 1.0, noEcho).has_value(), "Zero reverb mix should suppress echo");

    const std::string sigA = BuildAudioEnvironmentSignature(norm);
    const std::string sigB = BuildAudioEnvironmentSignature(norm);
    Expect(sigA == sigB, "Signature should be stable for identical profiles");
    Expect(BuildAudioEnvironmentSignature(std::nullopt) == "none", "Missing profile should signature to none");

    const float threePi = ri::math::Pi() * 3.0f;
    const float native = std::remainderf(threePi, ri::math::TwoPi());
    Expect(NearlyEqual(static_cast<double>(ri::math::NormalizeYawRadians(threePi)), static_cast<double>(native)),
           "NormalizeYawRadians should stay aligned with std::remainderf for the proto yaw shim");
}

// --- merged from TestContentEnvironmentVolumes.cpp ---
namespace detail_ContentEnvironmentVolumes {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    const bool matches = NearlyEqual(actual.x, expected.x) &&
                         NearlyEqual(actual.y, expected.y) &&
                         NearlyEqual(actual.z, expected.z);
    Expect(matches,
           label + " expected (" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " +
               std::to_string(expected.z) + ") but got (" + std::to_string(actual.x) + ", " +
               std::to_string(actual.y) + ", " + std::to_string(actual.z) + ")");
}

ri::content::Value MakeVec3(double x, double y, double z) {
    using Value = ri::content::Value;
    return Value::Array{Value(x), Value(y), Value(z)};
}

}

 // namespace

void TestContentEnvironmentVolumes() {
    using namespace detail_ContentEnvironmentVolumes;
    using ri::content::Value;

    {
        Value::Object data{};
        data["position"] = MakeVec3(0.0, 1.0, 2.0);
        data["size"] = MakeVec3(0.0, 4.0, 6.0);
        data["tintColor"] = "#9fff9d";
        data["tintStrength"] = 2.0;
        data["blurAmount"] = 0.03;
        data["noiseAmount"] = 0.8;
        data["scanlineAmount"] = 0.2;
        data["barrelDistortion"] = 0.4;
        data["chromaticAberration"] = 0.1;

        const ri::world::PostProcessVolume volume = ri::content::BuildPostProcessVolume(data);
        Expect(volume.type == "post_process_volume",
               "Content environment bridge should classify post-process volumes");
        Expect(volume.shape == ri::world::VolumeShape::Sphere,
               "Content environment bridge should default post-process volumes to sphere shape");
        ExpectVec3(volume.position, {0.0f, 1.0f, 2.0f},
                   "Content environment bridge should preserve post-process positions");
        ExpectVec3(volume.size, {0.001f, 4.0f, 6.0f},
                   "Content environment bridge should clamp post-process extents to a non-zero minimum");
        ExpectVec3(volume.tintColor, {0.624f, 1.000f, 0.616f},
                   "Content environment bridge should preserve post-process tint colors");
        Expect(NearlyEqual(volume.tintStrength, 1.0f) &&
                   NearlyEqual(volume.blurAmount, 0.02f) &&
                   NearlyEqual(volume.noiseAmount, 0.2f) &&
                   NearlyEqual(volume.scanlineAmount, 0.08f) &&
                   NearlyEqual(volume.barrelDistortion, 0.1f) &&
                   NearlyEqual(volume.chromaticAberration, 0.02f),
               "Content environment bridge should clamp post-process tuning like the prototype");
    }

    {
        Value::Object data{};
        data["shape"] = "box";
        data["size"] = MakeVec3(0.0, 0.0, 4.0);
        data["reverbMix"] = 2.0;
        data["echoDelayMs"] = 9000.0;
        data["echoFeedback"] = -2.0;
        data["dampening"] = 4.0;
        data["volumeScale"] = 0.05;
        data["playbackRate"] = 9.0;

        const ri::world::AudioReverbVolume volume = ri::content::BuildAudioReverbVolume(data);
        Expect(volume.type == "audio_reverb_volume",
               "Content environment bridge should classify audio reverb volumes");
        Expect(volume.shape == ri::world::VolumeShape::Box,
               "Content environment bridge should preserve explicit reverb volume shapes");
        ExpectVec3(volume.size, {0.001f, 0.001f, 4.0f},
                   "Content environment bridge should clamp reverb extents to a non-zero minimum");
        Expect(NearlyEqual(volume.reverbMix, 1.0f) &&
                   NearlyEqual(volume.echoDelayMs, 2000.0f) &&
                   NearlyEqual(volume.echoFeedback, 0.0f) &&
                   NearlyEqual(volume.dampening, 1.0f) &&
                   NearlyEqual(volume.volumeScale, 0.2f) &&
                   NearlyEqual(volume.playbackRate, 1.5f),
               "Content environment bridge should clamp authored reverb tuning");
    }

    {
        Value::Object data{};
        data["size"] = MakeVec3(0.0, 0.0, 0.0);
        data["dampening"] = 2.0;
        data["volumeScale"] = -1.0;

        const ri::world::AudioOcclusionVolume volume = ri::content::BuildAudioOcclusionVolume(data);
        Expect(volume.type == "audio_occlusion_volume",
               "Content environment bridge should classify audio occlusion volumes");
        Expect(NearlyEqual(volume.occlusionStrength, 1.0f) &&
                   NearlyEqual(volume.volumeScale, 0.1f),
               "Content environment bridge should clamp authored occlusion tuning");
        ExpectVec3(volume.size, {0.001f, 0.001f, 0.001f},
                   "Content environment bridge should clamp occlusion extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["shape"] = "sphere";
        data["size"] = MakeVec3(0.0, 0.0, 0.0);
        data["gravityScale"] = -5.0;
        data["jumpScale"] = 6.0;
        data["drag"] = 20.0;
        data["buoyancy"] = 9.0;
        data["flowDirection"] = MakeVec3(1.0, 0.0, -0.5);
        data["flowStrength"] = 5.0;
        data["underwaterTint"] = "#6bbcff";
        data["tintStrength"] = 3.0;
        data["reverbMix"] = 2.0;
        data["echoDelayMs"] = 9000.0;

        const ri::world::FluidSimulationVolume volume = ri::content::BuildFluidSimulationVolume(data);
        Expect(volume.type == "fluid_simulation_volume",
               "Content environment bridge should classify fluid simulation volumes");
        Expect(volume.shape == ri::world::VolumeShape::Box,
               "Content environment bridge should keep fluid volumes on supported prototype shapes");
        Expect(NearlyEqual(volume.gravityScale, -2.0f) &&
                   NearlyEqual(volume.jumpScale, 4.0f) &&
                   NearlyEqual(volume.drag, 8.0f) &&
                   NearlyEqual(volume.buoyancy, 3.0f),
               "Content environment bridge should clamp authored fluid tuning");
        ExpectVec3(volume.flow, {5.0f, 0.0f, -2.5f},
                   "Content environment bridge should translate flow direction and strength into runtime flow");
        ExpectVec3(volume.tintColor, {0.420f, 0.737f, 1.000f},
                   "Content environment bridge should preserve fluid tint colors");
        Expect(NearlyEqual(volume.tintStrength, 1.0f) &&
                   NearlyEqual(volume.reverbMix, 1.0f) &&
                   NearlyEqual(volume.echoDelayMs, 2000.0f),
               "Content environment bridge should clamp fluid environment shaping");
        ExpectVec3(volume.size, {0.001f, 0.001f, 0.001f},
                   "Content environment bridge should clamp fluid extents to a non-zero minimum");
    }
}

// --- merged from TestContentPhysicsVolumes.cpp ---
namespace detail_ContentPhysicsVolumes {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    const bool matches = NearlyEqual(actual.x, expected.x) &&
                         NearlyEqual(actual.y, expected.y) &&
                         NearlyEqual(actual.z, expected.z);
    Expect(matches,
           label + " expected (" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " +
               std::to_string(expected.z) + ") but got (" + std::to_string(actual.x) + ", " +
               std::to_string(actual.y) + ", " + std::to_string(actual.z) + ")");
}

ri::content::Value MakeVec3(double x, double y, double z) {
    using Value = ri::content::Value;
    return Value::Array{Value(x), Value(y), Value(z)};
}

}

 // namespace

void TestContentPhysicsVolumes() {
    using namespace detail_ContentPhysicsVolumes;
    using ri::content::Value;

    {
        Value::Object data{};
        data["gravityDirection"] = MakeVec3(0.0, -1.0, 0.0);
        data["gravityStrength"] = 2.5;
        data["size"] = MakeVec3(0.0, 4.0, 6.0);
        data["gravityScale"] = -9.0;
        data["jumpScale"] = 5.0;
        data["drag"] = 10.0;
        data["buoyancy"] = 5.0;
        data["flowDirection"] = MakeVec3(1.0, -0.5, 0.25);
        data["force"] = 3.0;

        const ri::world::PhysicsModifierVolume volume = ri::content::BuildCustomGravityVolume(data);
        Expect(volume.type == "custom_gravity_volume",
               "Content physics bridge should classify custom gravity volumes");
        ExpectVec3(volume.size, {0.001f, 4.0f, 6.0f},
                   "Content physics bridge should clamp custom gravity extents to a non-zero minimum");
        Expect(NearlyEqual(volume.gravityScale, -2.0f) &&
                   NearlyEqual(volume.jumpScale, 4.0f) &&
                   NearlyEqual(volume.drag, 8.0f) &&
                   NearlyEqual(volume.buoyancy, 3.0f),
               "Content physics bridge should clamp custom gravity tuning");
        ExpectVec3(volume.flow, {3.0f, -1.5f, 0.75f},
                   "Content physics bridge should preserve non-normalized custom gravity flow");
    }

    {
        Value::Object data{};
        data["size"] = MakeVec3(6.0, 4.0, 0.0);
        data["windDirection"] = MakeVec3(3.0, 0.0, 4.0);
        data["windStrength"] = 10.0;
        data["drag"] = -2.0;

        const ri::world::PhysicsModifierVolume volume = ri::content::BuildDirectionalWindVolume(data);
        Expect(volume.type == "directional_wind_volume",
               "Content physics bridge should classify directional wind volumes");
        ExpectVec3(volume.size, {6.0f, 4.0f, 0.001f},
                   "Content physics bridge should clamp directional wind extents to a non-zero minimum");
        Expect(NearlyEqual(volume.gravityScale, 1.0f) &&
                   NearlyEqual(volume.jumpScale, 1.0f) &&
                   NearlyEqual(volume.drag, 0.0f) &&
                   NearlyEqual(volume.buoyancy, 0.0f),
               "Content physics bridge should preserve directional wind baseline modifiers");
        ExpectVec3(volume.flow, {6.0f, 0.0f, 8.0f},
                   "Content physics bridge should normalize wind direction before applying strength");
    }

    {
        Value::Object data{};
        data["currentDirection"] = MakeVec3(0.0, 0.0, -2.0);
        data["currentStrength"] = -1.5;
        data["fluidDrag"] = 0.7;
        data["lift"] = 1.4;

        const ri::world::PhysicsModifierVolume volume = ri::content::BuildBuoyancyVolume(data);
        Expect(volume.type == "buoyancy_volume",
               "Content physics bridge should classify buoyancy volumes");
        Expect(NearlyEqual(volume.gravityScale, 0.8f) &&
                   NearlyEqual(volume.jumpScale, 0.9f) &&
                   NearlyEqual(volume.drag, 0.7f) &&
                   NearlyEqual(volume.buoyancy, 1.4f),
               "Content physics bridge should accept buoyancy aliases for drag/lift tuning");
        ExpectVec3(volume.flow, {0.0f, 0.0f, 3.0f},
                   "Content physics bridge should preserve buoyancy flow direction and signed strength");
    }

    {
        Value::Object data{};
        data["pointB"] = MakeVec3(0.0, 0.0, 5.0);
        data["speed"] = 7.0;

        const ri::world::SurfaceVelocityPrimitive volume = ri::content::BuildSurfaceVelocityPrimitive(data);
        Expect(volume.type == "surface_velocity_primitive",
               "Content physics bridge should classify surface velocity primitives");
        Expect(NearlyEqual(volume.size.x, 4.0f) &&
                   NearlyEqual(volume.size.y, 0.6f) &&
                   NearlyEqual(volume.size.z, 4.0f),
               "Content physics bridge should preserve surface velocity defaults");
        ExpectVec3(volume.flow, {0.0f, 0.0f, 7.0f},
                   "Content physics bridge should normalize surface velocity direction before applying speed");
    }

    {
        Value::Object data{};
        data["gravityScale"] = 0.6;
        data["jumpScale"] = 1.3;
        data["drag"] = 0.4;
        data["buoyancy"] = 0.5;
        data["flowDirection"] = MakeVec3(0.0, 0.0, 1.0);
        data["flowStrength"] = 2.0;
        const ri::world::PhysicsModifierVolume volume = ri::content::BuildPhysicsVolume(data);
        Expect(volume.type == "physics_volume" &&
                   NearlyEqual(volume.gravityScale, 0.6f) &&
                   NearlyEqual(volume.jumpScale, 1.3f) &&
                   NearlyEqual(volume.drag, 0.4f) &&
                   NearlyEqual(volume.buoyancy, 0.5f),
               "Content physics bridge should classify generic physics volumes and parse local modifier tuning");
        ExpectVec3(volume.flow, {0.0f, 0.0f, 2.0f},
                   "Content physics bridge should support local flow shaping in generic physics volumes");
    }

    {
        Value::Object data{};
        data["waveHeight"] = 2.0;
        data["waveSpeed"] = 20.0;
        data["currentSpeed"] = -45.0;
        data["blocksUnderwaterFog"] = true;
        const ri::world::WaterSurfacePrimitive volume = ri::content::BuildWaterSurfacePrimitive(data);
        Expect(volume.type == "water_surface_primitive" &&
                   NearlyEqual(volume.waveAmplitude, 2.0f) &&
                   NearlyEqual(volume.waveFrequency, 12.0f) &&
                   NearlyEqual(volume.flowSpeed, -30.0f) &&
                   volume.blocksUnderwaterFog,
               "Content physics bridge should classify water-surface primitives and clamp wave/current settings");
    }

    {
        Value::Object data{};
        data["size"] = MakeVec3(6.0, 0.0, 6.0);
        data["strength"] = 80.0;
        data["falloff"] = -2.0;
        data["mode"] = "inward";
        data["deadzone"] = 2.0;

        const ri::world::RadialForceVolume volume = ri::content::BuildRadialForceVolume(data);
        Expect(volume.type == "radial_force_volume",
               "Content physics bridge should classify radial force volumes");
        ExpectVec3(volume.size, {6.0f, 0.001f, 6.0f},
                   "Content physics bridge should clamp radial force extents to a non-zero minimum");
        Expect(NearlyEqual(volume.strength, -40.0f) &&
                   NearlyEqual(volume.falloff, 0.0f) &&
                   NearlyEqual(volume.innerRadius, 2.0f),
               "Content physics bridge should support inward radial mode and deadzone radius");
    }
}

// --- merged from TestContentPrefabExpansion.cpp ---
namespace detail_ContentPrefabExpansion {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    const bool matches = NearlyEqual(actual.x, expected.x) &&
                         NearlyEqual(actual.y, expected.y) &&
                         NearlyEqual(actual.z, expected.z);
    Expect(matches,
           label + " expected (" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " +
               std::to_string(expected.z) + ") but got (" + std::to_string(actual.x) + ", " +
               std::to_string(actual.y) + ", " + std::to_string(actual.z) + ")");
}

const ri::content::Value::Object& ExpectObject(const ri::content::Value& value, std::string_view label) {
    const auto* object = value.TryGetObject();
    Expect(object != nullptr, std::string(label) + " should be an object");
    return *object;
}

const ri::content::Value::Array& ExpectArray(const ri::content::Value& value, std::string_view label) {
    const auto* array = value.TryGetArray();
    Expect(array != nullptr, std::string(label) + " should be an array");
    return *array;
}

std::string ExpectString(const ri::content::Value& value, const std::string& label) {
    const auto* text = value.TryGetString();
    Expect(text != nullptr, label + " should be a string");
    return *text;
}

ri::math::Vec3 ExpectVec3Value(const ri::content::Value& value) {
    return ri::content::SanitizeVec3(value, {std::numeric_limits<float>::quiet_NaN(),
                                             std::numeric_limits<float>::quiet_NaN(),
                                             std::numeric_limits<float>::quiet_NaN()});
}

ri::content::Value MakeVec3(double x, double y, double z) {
    using Value = ri::content::Value;
    return Value::Array{Value(x), Value(y), Value(z)};
}

ri::content::Value BuildLevelData() {
    using Value = ri::content::Value;

    Value::Object basePanelMaterial{};
    basePanelMaterial["color"] = "#ffeeaa";
    basePanelMaterial["layers"] = Value::Array{Value("template")};

    Value::Object basePanel{};
    basePanel["material"] = Value(std::move(basePanelMaterial));
    basePanel["rotation"] = MakeVec3(0.0, 0.0, 0.0);

    Value::Object accentPanelMaterial{};
    accentPanelMaterial["intensity"] = 3.0;

    Value::Object accentPanel{};
    accentPanel["template"] = "base_panel";
    accentPanel["material"] = Value(std::move(accentPanelMaterial));

    Value::Object entityTemplates{};
    entityTemplates["base_panel"] = Value(std::move(basePanel));
    entityTemplates["accent_panel"] = Value(std::move(accentPanel));

    Value::Object rootPanelMaterial{};
    rootPanelMaterial["layers"] = Value::Array{Value("override")};

    Value::Object rootPanel{};
    rootPanel["id"] = "panel_root";
    rootPanel["template"] = "accent_panel";
    rootPanel["position"] = MakeVec3(1.0, 2.0, 3.0);
    rootPanel["material"] = Value(std::move(rootPanelMaterial));

    Value::Object prefabCrate{};
    prefabCrate["id"] = "crate";
    prefabCrate["template"] = "base_panel";
    prefabCrate["position"] = MakeVec3(1.0, 0.0, 0.0);
    prefabCrate["scale"] = MakeVec3(1.0, 2.0, 1.0);
    prefabCrate["lookAt"] = MakeVec3(0.0, 0.0, 1.0);
    prefabCrate["path"] = Value::Array{
        MakeVec3(0.0, 0.0, 0.0),
        MakeVec3(1.0, 0.0, 0.0),
    };

    Value::Object nestedLampInstance{};
    nestedLampInstance["prefab"] = "lamp";
    nestedLampInstance["position"] = MakeVec3(0.0, 0.0, 2.0);

    Value::Object roomPrefab{};
    roomPrefab["geometry"] = Value::Array{Value(std::move(prefabCrate))};
    roomPrefab["prefabInstances"] = Value::Array{Value(std::move(nestedLampInstance))};
    roomPrefab["doors"] = Value::Array{Value::Object{
        {"id", Value("door_proto")},
        {"type", Value("procedural_door")},
        {"transitionLevel", Value("levels/basement.json")},
    }};
    roomPrefab["infoPanels"] = Value::Array{Value::Object{
        {"id", Value("panel_proto")},
        {"text", Value("STATUS: ONLINE")},
    }};

    Value::Object lampLight{};
    lampLight["id"] = "bulb";
    lampLight["position"] = MakeVec3(0.0, 1.0, 0.0);

    Value::Object lampPrefab{};
    lampPrefab["lights"] = Value::Array{Value(std::move(lampLight))};

    Value::Object prefabs{};
    prefabs["room"] = Value(std::move(roomPrefab));
    prefabs["lamp"] = Value(std::move(lampPrefab));

    Value::Object rootPrefabInstance{};
    rootPrefabInstance["prefab"] = "room";
    rootPrefabInstance["position"] = MakeVec3(10.0, 0.0, 0.0);
    rootPrefabInstance["rotation"] = MakeVec3(0.0, 1.57079632679, 0.0);
    rootPrefabInstance["scale"] = MakeVec3(2.0, 1.0, 1.0);

    Value::Object levelData{};
    levelData["entityTemplates"] = Value(std::move(entityTemplates));
    levelData["geometry"] = Value::Array{Value(std::move(rootPanel))};
    levelData["prefabs"] = Value(std::move(prefabs));
    levelData["prefabInstances"] = Value::Array{Value(std::move(rootPrefabInstance))};
    return Value(std::move(levelData));
}

ri::content::Value BuildRecursiveTemplates() {
    using Value = ri::content::Value;

    Value::Object templateA{};
    templateA["template"] = "b";

    Value::Object templateB{};
    templateB["template"] = "a";

    Value::Object templates{};
    templates["a"] = Value(std::move(templateA));
    templates["b"] = Value(std::move(templateB));

    Value::Object geometryNode{};
    geometryNode["template"] = "a";

    Value::Object levelData{};
    levelData["entityTemplates"] = Value(std::move(templates));
    levelData["geometry"] = Value::Array{Value(std::move(geometryNode))};
    return Value(std::move(levelData));
}

ri::content::Value BuildRecursivePrefabs() {
    using Value = ri::content::Value;

    Value::Object prefabAChild{};
    prefabAChild["prefab"] = "b";

    Value::Object prefabA{};
    prefabA["prefabInstances"] = Value::Array{Value(std::move(prefabAChild))};

    Value::Object prefabBChild{};
    prefabBChild["prefab"] = "a";

    Value::Object prefabB{};
    prefabB["prefabInstances"] = Value::Array{Value(std::move(prefabBChild))};

    Value::Object prefabs{};
    prefabs["a"] = Value(std::move(prefabA));
    prefabs["b"] = Value(std::move(prefabB));

    Value::Object rootInstance{};
    rootInstance["prefab"] = "a";

    Value::Object levelData{};
    levelData["prefabs"] = Value(std::move(prefabs));
    levelData["prefabInstances"] = Value::Array{Value(std::move(rootInstance))};
    return Value(std::move(levelData));
}

}

 // namespace

void TestContentPrefabExpansion() {
    using namespace detail_ContentPrefabExpansion;
    using ri::content::Value;

    ExpectVec3(ri::content::SanitizeVec3(
                   Value(Value::Array{Value(1.0), Value("bad"), Value(3.0)}),
                   {0.0f, 2.0f, 0.0f}),
               {1.0f, 2.0f, 3.0f},
               "Content sanitizers should preserve finite vec3 components and fall back invalid ones");

    const std::array<double, 2> uv = ri::content::SanitizeVec2(
        Value(Value::Array{Value(0.5), Value("bad")}),
        {1.0, 2.0});
    Expect(NearlyEqual(static_cast<float>(uv[0]), 0.5f) && NearlyEqual(static_cast<float>(uv[1]), 2.0f),
           "Content sanitizers should preserve finite vec2 components and fall back invalid ones");

    const std::array<double, 4> quaternion = ri::content::SanitizeQuaternion(
        Value(Value::Array{Value(0.0), Value(0.0), Value(0.0), Value(0.0)}));
    Expect(NearlyEqual(static_cast<float>(quaternion[0]), 0.0f) &&
               NearlyEqual(static_cast<float>(quaternion[1]), 0.0f) &&
               NearlyEqual(static_cast<float>(quaternion[2]), 0.0f) &&
               NearlyEqual(static_cast<float>(quaternion[3]), 1.0f),
           "Content sanitizers should fall back invalid quaternions to identity");

    ExpectVec3(ri::content::SanitizeScale(
                   Value(Value::Array{Value(0.0), Value(2048.0), Value(-0.00001)}),
                   {1.0f, 1.0f, 1.0f}),
               {1.0f, 512.0f, 1.0f},
               "Content sanitizers should clamp near-zero and extreme scale values");
    Expect(ri::content::ClampFiniteInteger(Value(5.7), 1, 0, 10) == 6,
           "Content sanitizers should round finite integers like the prototype");
    Expect(NearlyEqual(static_cast<float>(ri::content::ClampPickupMotion(Value("bad"), 0.85, 0.0, 24.0)), 0.85f),
           "Content sanitizers should preserve pickup-motion fallbacks on invalid values");

    const Value expandedLevel = ri::content::ExpandLevelAuthoringData(BuildLevelData());
    const auto& expandedLevelObject = ExpectObject(expandedLevel, "Expanded level");
    const auto& geometry = ExpectArray(expandedLevelObject.at("geometry"), "Expanded geometry");
    const auto& lights = ExpectArray(expandedLevelObject.at("lights"), "Expanded lights");
    const auto& doors = ExpectArray(expandedLevelObject.at("doors"), "Expanded doors");
    const auto& infoPanels = ExpectArray(expandedLevelObject.at("infoPanels"), "Expanded info panels");
    Expect(geometry.size() == 2U, "Prefab expansion should append prefab geometry to root geometry");
    Expect(lights.size() == 1U, "Prefab expansion should append nested prefab lights to root lights");
    Expect(doors.size() == 1U && infoPanels.size() == 1U,
           "Prefab expansion should append door/info-panel authored collections");

    const auto& rootPanel = ExpectObject(geometry[0], "Root panel");
    Expect(rootPanel.find("template") == rootPanel.end(),
           "Template application should remove template keys from expanded nodes");
    const auto& rootMaterial = ExpectObject(rootPanel.at("material"), "Root panel material");
    Expect(ExpectString(rootMaterial.at("color"), "Root panel material color") == "#ffeeaa",
           "Template application should inherit nested object properties");
    const auto& rootLayers = ExpectArray(rootMaterial.at("layers"), "Root panel material layers");
    Expect(rootLayers.size() == 1U && ExpectString(rootLayers.front(), "Root panel layer") == "override",
           "Template application should let overrides replace array values");
    Expect(NearlyEqual(static_cast<float>(*rootMaterial.at("intensity").TryGetNumber()), 3.0f),
           "Template application should merge derived template fields");

    const auto& prefabCrate = ExpectObject(geometry[1], "Prefab crate");
    Expect(ExpectString(prefabCrate.at("id"), "Prefab crate id") == "room_0_crate",
           "Prefab expansion should prefix authored IDs with instance prefixes");
    ExpectVec3(ExpectVec3Value(prefabCrate.at("position")), {10.0f, 0.0f, -2.0f},
               "Prefab expansion should transform prefab positions through instance transforms");
    ExpectVec3(ExpectVec3Value(prefabCrate.at("scale")), {2.0f, 2.0f, 1.0f},
               "Prefab expansion should multiply authored scale by instance scale");
    ExpectVec3(ExpectVec3Value(prefabCrate.at("lookAt")), {11.0f, 0.0f, 0.0f},
               "Prefab expansion should transform look-at vectors");

    const auto& cratePath = ExpectArray(prefabCrate.at("path"), "Prefab crate path");
    ExpectVec3(ExpectVec3Value(cratePath.front()), {10.0f, 0.0f, 0.0f},
               "Prefab expansion should transform path points");
    ExpectVec3(ExpectVec3Value(cratePath.back()), {10.0f, 0.0f, -2.0f},
               "Prefab expansion should transform every path point");

    const auto& bulb = ExpectObject(lights.front(), "Nested prefab light");
    Expect(ExpectString(bulb.at("id"), "Nested prefab light id") == "room_0_room_0_bulb",
           "Nested prefabs should inherit recursively generated ID prefixes");
    ExpectVec3(ExpectVec3Value(bulb.at("position")), {12.0f, 1.0f, 0.0f},
               "Nested prefabs should inherit parent instance transforms");
    const auto& door = ExpectObject(doors.front(), "Expanded door");
    Expect(ExpectString(door.at("id"), "Expanded door id") == "room_0_door_proto",
           "Prefab expansion should prefix expanded door IDs with instance prefixes");
    const auto& panel = ExpectObject(infoPanels.front(), "Expanded info panel");
    Expect(ExpectString(panel.at("id"), "Expanded info panel id") == "room_0_panel_proto",
           "Prefab expansion should prefix expanded info-panel IDs with instance prefixes");

    bool caughtRecursiveTemplate = false;
    try {
        (void)ri::content::ApplyLevelEntityTemplates(BuildRecursiveTemplates());
    } catch (const std::runtime_error&) {
        caughtRecursiveTemplate = true;
    }
    Expect(caughtRecursiveTemplate,
           "Template expansion should reject recursive entity template references");

    bool caughtRecursivePrefab = false;
    try {
        (void)ri::content::ExpandLevelPrefabs(BuildRecursivePrefabs());
    } catch (const std::runtime_error&) {
        caughtRecursivePrefab = true;
    }
    Expect(caughtRecursivePrefab,
           "Prefab expansion should reject recursive prefab references");

    bool caughtMalformedRootPrefabInstance = false;
    try {
        using Value = ri::content::Value;
        Value::Object malformedLevel{};
        malformedLevel["prefabs"] = Value::Object{{"room", Value::Object{}}};
        malformedLevel["prefabInstances"] = Value::Array{Value("bad-entry")};
        (void)ri::content::ExpandLevelPrefabs(Value(malformedLevel));
    } catch (const std::runtime_error&) {
        caughtMalformedRootPrefabInstance = true;
    }
    Expect(caughtMalformedRootPrefabInstance,
           "Prefab expansion should fail fast on malformed root prefabInstances entries");

    bool caughtMalformedNestedPrefabInstance = false;
    try {
        using Value = ri::content::Value;
        Value::Object level{};
        Value::Object prefab{};
        prefab["prefabInstances"] = Value::Array{Value::Object{}};
        level["prefabs"] = Value::Object{{"room", Value(prefab)}};
        level["prefabInstances"] = Value::Array{Value::Object{{"prefab", Value("room")}}};
        (void)ri::content::ExpandLevelPrefabs(Value(level));
    } catch (const std::runtime_error&) {
        caughtMalformedNestedPrefabInstance = true;
    }
    Expect(caughtMalformedNestedPrefabInstance,
           "Prefab expansion should fail fast on malformed nested prefabInstances entries");
}

// --- merged from TestContentTriggerVolumes.cpp ---
namespace detail_ContentTriggerVolumes {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    const bool matches = NearlyEqual(actual.x, expected.x) &&
                         NearlyEqual(actual.y, expected.y) &&
                         NearlyEqual(actual.z, expected.z);
    Expect(matches,
           label + " expected (" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " +
               std::to_string(expected.z) + ") but got (" + std::to_string(actual.x) + ", " +
               std::to_string(actual.y) + ", " + std::to_string(actual.z) + ")");
}

ri::content::Value MakeVec3(double x, double y, double z) {
    using Value = ri::content::Value;
    return Value::Array{Value(x), Value(y), Value(z)};
}

}

 // namespace

void TestContentTriggerVolumes() {
    using namespace detail_ContentTriggerVolumes;
    using ri::content::Value;

    {
        Value::Object data{};
        data["type"] = "generic_trigger_volume";
        data["broadcastFrequency"] = 2.5;
        data["shape"] = "sphere";
        data["onEnterEvent"] = "evt_trigger_enter";

        const ri::world::GenericTriggerVolume volume = ri::content::BuildGenericTriggerVolume(data);
        Expect(volume.type == "generic_trigger_volume",
               "Content trigger bridge should preserve generic trigger volume typing");
        Expect(NearlyEqual(static_cast<float>(volume.broadcastFrequency), 2.5f),
               "Content trigger bridge should preserve generic trigger broadcast frequency");
        Expect(NearlyEqual(volume.radius, 1.5f),
               "Content trigger bridge should preserve prototype default generic trigger radius");
        Expect(volume.onEnterEvent == "evt_trigger_enter",
               "Content trigger bridge should preserve authored trigger enter-event dispatch ids");
    }

    {
        Value::Object data{};
        data["type"] = "generic_trigger_volume";
        data["startArmed"] = false;

        const ri::world::GenericTriggerVolume volume = ri::content::BuildGenericTriggerVolume(data);
        Expect(!volume.armed, "Content should map startArmed false to disarmed generic triggers");
    }

    {
        Value::Object data{};
        data["type"] = "generic_trigger_volume";
        data["armed"] = true;

        const ri::world::GenericTriggerVolume volume = ri::content::BuildGenericTriggerVolume(data);
        Expect(volume.armed, "Content should map armed true for generic triggers");
    }

    {
        Value::Object data{};
        data["shape"] = "cylinder";
        data["radius"] = 10.0;
        data["height"] = 5.0;
        data["broadcastFrequency"] = 3.0;
        data["filterMask"] = 7.0;
        data["onExitEvent"] = "evt_query_exit";

        const ri::world::SpatialQueryVolume volume = ri::content::BuildSpatialQueryVolume(data);
        Expect(volume.type == "spatial_query_volume",
               "Content trigger bridge should classify spatial query volumes");
        Expect(volume.shape == ri::world::VolumeShape::Cylinder,
               "Content trigger bridge should preserve authored spatial query volume shapes");
        Expect(NearlyEqual(static_cast<float>(volume.broadcastFrequency), 3.0f) &&
                   volume.filterMask == 7U,
               "Content trigger bridge should preserve spatial query broadcast frequency and filter mask");
        Expect(volume.onExitEvent == "evt_query_exit",
               "Content trigger bridge should preserve authored spatial-query exit-event ids");
    }

    {
        Value::Object data{};
        data["pivotId"] = "helper_anchor_a";
        data["axis"] = MakeVec3(0.0, 1.0, 0.0);
        data["alignNormal"] = true;
        const ri::world::PivotAnchorPrimitive primitive = ri::content::BuildPivotAnchorPrimitive(data);
        Expect(primitive.type == "pivot_anchor_primitive" &&
                   primitive.anchorId == "helper_anchor_a" &&
                   primitive.alignToSurfaceNormal,
               "Content helper bridge should classify pivot anchor primitives");
        ExpectVec3(primitive.forwardAxis, {0.0f, 1.0f, 0.0f},
                   "Content helper bridge should preserve authored pivot forward-axis direction");
    }

    {
        Value::Object data{};
        data["mirrorNormal"] = MakeVec3(0.0, 0.0, 1.0);
        data["offset"] = 64.0;
        data["keepOriginal"] = false;
        data["snapToGrid"] = true;
        const ri::world::SymmetryMirrorPlane helper = ri::content::BuildSymmetryMirrorPlane(data);
        Expect(helper.type == "symmetry_mirror_plane" &&
                   !helper.keepOriginal &&
                   helper.snapToGrid &&
                   NearlyEqual(helper.planeOffset, 64.0f),
               "Content helper bridge should classify symmetry mirror-plane helpers");
        ExpectVec3(helper.planeNormal, {0.0f, 0.0f, 1.0f},
                   "Content helper bridge should preserve authored mirror-plane normal");
    }

    {
        Value::Object data{};
        data["doorWidth"] = 1.8;
        data["doorHeight"] = 2.2;
        data["sillHeight"] = 0.3;
        data["headerHeight"] = 2.5;
        data["carveCollision"] = false;
        data["carveVisual"] = true;
        const ri::world::DoorWindowCutoutPrimitive primitive = ri::content::BuildDoorWindowCutoutPrimitive(data);
        Expect(primitive.type == "door_window_cutout"
                   && NearlyEqual(primitive.openingWidth, 1.8f)
                   && NearlyEqual(primitive.openingHeight, 2.2f)
                   && NearlyEqual(primitive.sillHeight, 0.3f)
                   && NearlyEqual(primitive.lintelHeight, 2.5f)
                   && !primitive.carveCollision
                   && primitive.carveVisual,
               "Content helper bridge should classify and map door-window cutout helper primitives");
    }

    {
        Value::Object data{};
        data["id"] = "door_exit_a";
        data["position"] = MakeVec3(2.0, 0.0, -4.0);
        data["scale"] = MakeVec3(1.4, 2.4, 0.3);
        data["interactionPrompt"] = "Exit Facility";
        data["deniedPrompt"] = "Access denied";
        data["transitionLevel"] = "levels/finale.json";
        data["endingTrigger"] = "ending_escape";
        data["accessFeedbackTag"] = "door_locked";
        data["startsLocked"] = true;
        const ri::world::ProceduralDoorEntity door = ri::content::BuildProceduralDoorEntity(data);
        Expect(door.type == "procedural_door"
                   && door.startsLocked
                   && door.transitionLevel == "levels/finale.json"
                   && door.endingTrigger == "ending_escape"
                   && door.accessFeedbackTag == "door_locked"
                   && door.interactionPrompt == "Exit Facility"
                   && door.deniedPrompt == "Access denied",
               "Content world-volume bridge should parse procedural door entities with transition/access metadata");
    }

    {
        Value::Object binding{};
        binding["label"] = "ALERT";
        binding["runtimeMetric"] = "eventBusEmits";
        Value::Object data{};
        data["id"] = "panel_security_a";
        data["position"] = MakeVec3(0.0, 1.5, 0.0);
        data["scale"] = MakeVec3(2.5, 1.4, 0.1);
        data["focusable"] = true;
        data["interactionPrompt"] = "Inspect Panel";
        data["interactionHook"] = "security_panel_interact";
        data["bindingsMode"] = "replace";
        data["bindings"] = Value::Array{binding};
        const ri::world::DynamicInfoPanelSpawner panel = ri::content::BuildDynamicInfoPanelSpawner(data);
        Expect(panel.focusable
                   && panel.interactionPrompt == "Inspect Panel"
                   && panel.interactionHook == "security_panel_interact"
                   && panel.panel.replaceBindings
                   && panel.panel.bindings.size() == 1U
                   && panel.panel.bindings.front().runtimeMetric == "eventBusEmits",
               "Content world-volume bridge should parse interactive info panels with live-runtime bindings");
    }

    {
        Value::Object data{};
        data["level"] = "facility_b.ri_scene";
        data["size"] = MakeVec3(0.0, 4.0, 6.0);

        const ri::world::StreamingLevelVolume volume = ri::content::BuildStreamingLevelVolume(data);
        Expect(volume.type == "streaming_level_volume",
               "Content trigger bridge should classify streaming level volumes");
        Expect(volume.targetLevel == "facility_b.ri_scene",
               "Content trigger bridge should preserve target-level aliases for streaming volumes");
        ExpectVec3(volume.size, {0.001f, 4.0f, 6.0f},
                   "Content trigger bridge should clamp streaming volume extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["targetLevel"] = "hub.ri_scene";
        data["pointA"] = MakeVec3(4.0, 1.0, -2.0);
        data["rotation"] = MakeVec3(0.0, 1.57, 0.0);

        const ri::world::CheckpointSpawnVolume volume = ri::content::BuildCheckpointSpawnVolume(data);
        Expect(volume.type == "checkpoint_spawn_volume",
               "Content trigger bridge should classify checkpoint spawn volumes");
        Expect(volume.targetLevel == "hub.ri_scene",
               "Content trigger bridge should preserve checkpoint target levels");
        ExpectVec3(volume.respawn, {4.0f, 1.0f, -2.0f},
                   "Content trigger bridge should preserve checkpoint respawn positions");
        ExpectVec3(volume.respawnRotation, {0.0f, 1.57f, 0.0f},
                   "Content trigger bridge should preserve checkpoint respawn rotation");
    }

    {
        Value::Object data{};
        data["targetIds"] = Value::Array{Value("door_exit"), Value("door_backup")};
        data["destination"] = MakeVec3(30.0, 2.0, -6.0);
        data["respawnRotation"] = MakeVec3(0.0, 3.14, 0.0);
        data["offset"] = MakeVec3(0.0, 1.0, 0.0);

        const ri::world::TeleportVolume volume = ri::content::BuildTeleportVolume(data);
        Expect(volume.type == "teleport_volume",
               "Content trigger bridge should classify teleport volumes");
        Expect(volume.targetId == "door_exit",
               "Content trigger bridge should fall back to the first authored teleport target ID");
        ExpectVec3(volume.targetPosition, {30.0f, 2.0f, -6.0f},
                   "Content trigger bridge should preserve authored teleport destinations");
        ExpectVec3(volume.targetRotation, {0.0f, 3.14f, 0.0f},
                   "Content trigger bridge should preserve authored teleport rotation");
        ExpectVec3(volume.offset, {0.0f, 1.0f, 0.0f},
                   "Content trigger bridge should preserve authored teleport offsets");
    }

    {
        Value::Object data{};
        data["targetIds"] = Value::Array{Value(""), Value(""), Value("door_backup")};

        const ri::world::TeleportVolume volume = ri::content::BuildTeleportVolume(data);
        Expect(volume.targetId == "door_backup",
               "Content trigger bridge should skip empty teleport target aliases when selecting fallback destination ids");
    }

    {
        Value::Object data{};
        data["impulse"] = MakeVec3(3.0, 0.0, 4.0);
        data["strength"] = 20.0;
        data["affectPhysics"] = false;

        const ri::world::LaunchVolume volume = ri::content::BuildLaunchVolume(data);
        Expect(volume.type == "launch_volume",
               "Content trigger bridge should classify launch volumes");
        ExpectVec3(volume.impulse, {12.0f, 0.0f, 16.0f},
                   "Content trigger bridge should normalize launch direction before applying strength");
        Expect(!volume.affectPhysics,
               "Content trigger bridge should preserve launch-volume affect-physics tuning");
    }

    {
        Value::Object data{};
        data["strength"] = 18.0;
        data["impulse"] = MakeVec3(0.0, 0.0, 0.0);

        const ri::world::LaunchVolume volume = ri::content::BuildLaunchVolume(data);
        ExpectVec3(volume.impulse, {0.0f, 18.0f, 0.0f},
                   "Content trigger bridge should fall back to vertical impulse when no launch direction is authored");
    }

    {
        Value::Object data{};
        data["scale"] = MakeVec3(6.0, 0.0, 6.0);

        const ri::world::AnalyticsHeatmapVolume volume = ri::content::BuildAnalyticsHeatmapVolume(data);
        Expect(volume.type == "analytics_heatmap_volume",
               "Content trigger bridge should classify analytics heatmap volumes");
        Expect(volume.entryCount == 0U && NearlyEqual(static_cast<float>(volume.dwellSeconds), 0.0f),
               "Content trigger bridge should initialize analytics heatmap runtime counters");
        ExpectVec3(volume.size, {6.0f, 0.001f, 6.0f},
                   "Content trigger bridge should clamp analytics heatmap extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["scale"] = MakeVec3(6.0, 4.0, 6.0);
        data["stayIntervalSeconds"] = 0.5;
        data["sampleSubjectMask"] = 3.0;
        data["debugDraw"] = true;

        const ri::world::AnalyticsHeatmapVolume volume = ri::content::BuildAnalyticsHeatmapVolume(data);
        Expect(NearlyEqual(static_cast<float>(volume.broadcastFrequency), 0.5f)
                   && volume.sampleSubjectMask == 3U && volume.debugDraw,
               "Content trigger bridge should map analytics heatmap authoring fields");
    }

    {
        Value::Object data{};
        data["debugVisible"] = false;
        const ri::world::GenericTriggerVolume hidden = ri::content::BuildGenericTriggerVolume(data);
        Expect(!hidden.debugVisible,
               "Content trigger bridge should map debugVisible metadata onto runtime volumes");
    }

    {
        Value::Object data{};
        const ri::world::GenericTriggerVolume visibleByDefault = ri::content::BuildGenericTriggerVolume(data);
        Expect(visibleByDefault.debugVisible,
               "Runtime volumes should remain debug-visible by default when metadata is omitted");
    }
}

// --- merged from TestContentValue.cpp ---
namespace detail_ContentValue {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool NearlyEqual(double lhs, double rhs, double epsilon = 0.00001) {
    return std::fabs(lhs - rhs) <= epsilon;
}

}

 // namespace

void TestContentValue() {
    using namespace detail_ContentValue;
    using ri::content::Value;

    Value root = Value::Object{
        {"player",
         Value::Object{
             {"tuning", Value::Object{{"walkSpeed", "5.5"}, {"sprintEnabled", "true"}}},
         }},
    };
    const Value* walkSpeed = root.FindPath("player.tuning.walkSpeed");
    Expect(walkSpeed != nullptr, "Value::FindPath should resolve nested dotted object paths");
    const std::optional<double> walkSpeedNumber = walkSpeed->CoerceNumber();
    Expect(walkSpeedNumber.has_value() && NearlyEqual(*walkSpeedNumber, 5.5),
           "Value::CoerceNumber should parse numeric strings");

    const Value* sprintEnabled = root.FindPath("player.tuning.sprintEnabled");
    Expect(sprintEnabled != nullptr, "Value::FindPath should resolve sibling properties");
    const std::optional<bool> sprintEnabledBool = sprintEnabled->CoerceBoolean();
    Expect(sprintEnabledBool.has_value() && *sprintEnabledBool,
           "Value::CoerceBoolean should parse canonical true/false strings");

    const Value number = Value::ParseLooseScalar("  12.25 ");
    const std::optional<double> numberCoerced = number.CoerceNumber();
    Expect(numberCoerced.has_value() && NearlyEqual(*numberCoerced, 12.25),
           "Value::ParseLooseScalar should parse finite numeric tokens");

    const Value boolean = Value::ParseLooseScalar(" FALSE ");
    const std::optional<bool> booleanCoerced = boolean.CoerceBoolean();
    Expect(booleanCoerced.has_value() && !*booleanCoerced,
           "Value::ParseLooseScalar should parse case-insensitive booleans");

    const Value text = Value::ParseLooseScalar("not-a-number");
    const std::optional<double> missingNumber = text.CoerceNumber();
    Expect(!missingNumber.has_value(), "Value::CoerceNumber should not invent numbers from text");
}

// --- merged from TestContentWorldVolumes.cpp ---
namespace detail_ContentWorldVolumes {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    const bool matches = NearlyEqual(actual.x, expected.x) &&
                         NearlyEqual(actual.y, expected.y) &&
                         NearlyEqual(actual.z, expected.z);
    Expect(matches,
           label + " expected (" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " +
               std::to_string(expected.z) + ") but got (" + std::to_string(actual.x) + ", " +
               std::to_string(actual.y) + ", " + std::to_string(actual.z) + ")");
}

ri::content::Value MakeVec3(double x, double y, double z) {
    using Value = ri::content::Value;
    return Value::Array{Value(x), Value(y), Value(z)};
}

}

 // namespace

void TestContentWorldVolumes() {
    using namespace detail_ContentWorldVolumes;
    using ri::content::Value;

    {
        Value::Object data{};
        data["id"] = "fog_01";
        data["shape"] = "sphere";
        data["position"] = MakeVec3(1.0, 2.0, 3.0);
        data["rotation"] = MakeVec3(0.1, 0.2, 0.3);
        data["scale"] = MakeVec3(-8.0, 4.0, 6.0);
        data["radius"] = "9.5";
        data["height"] = 7.0;

        const ri::world::RuntimeVolumeSeed seed = ri::content::BuildRuntimeVolumeSeed(
            data,
            ri::world::VolumeDefaults{"volume", "volume", ri::world::VolumeShape::Box, {4.0f, 4.0f, 4.0f}});
        Expect(seed.id == "fog_01", "Content world-volume bridge should preserve authored IDs");
        Expect(seed.shape.has_value() && seed.shape.value() == ri::world::VolumeShape::Sphere,
               "Content world-volume bridge should parse sphere volume shapes");
        ExpectVec3(seed.position.value_or(ri::math::Vec3{}), {1.0f, 2.0f, 3.0f},
                   "Content world-volume bridge should parse authored positions");
        ExpectVec3(seed.rotationRadians.value_or(ri::math::Vec3{}), {0.1f, 0.2f, 0.3f},
                   "Content world-volume bridge should preserve authored rotations for future runtime helpers");
        ExpectVec3(seed.size.value_or(ri::math::Vec3{}), {8.0f, 4.0f, 6.0f},
                   "Content world-volume bridge should resolve scale as positive runtime size");
        Expect(NearlyEqual(seed.radius.value_or(0.0f), 9.5f),
               "Content world-volume bridge should parse finite radius overrides");
        Expect(NearlyEqual(seed.height.value_or(0.0f), 7.0f),
               "Content world-volume bridge should parse finite height overrides");
    }

    {
        Value::Object data{};
        data["blocks"] = Value::Array{Value("player"), Value("camera"), Value("player"), Value("bogus")};
        data["size"] = MakeVec3(2.0, 3.0, 4.0);

        const ri::world::FilteredCollisionVolume volume = ri::content::BuildFilteredCollisionVolume(data);
        Expect(volume.type == "filtered_collision_volume",
               "Content world-volume bridge should default filtered-collision type");
        Expect(volume.channels.size() == 2U,
               "Content world-volume bridge should dedupe and filter authored collision channels");
        Expect(volume.channels[0] == ri::world::CollisionChannel::Player &&
                   volume.channels[1] == ri::world::CollisionChannel::Camera,
               "Content world-volume bridge should preserve collision-channel order");
    }

    {
        Value::Object data{};
        data["blocks"] = Value::Array{Value("camera"), Value("player"), Value("camera")};
        const ri::world::FilteredCollisionVolume volume = ri::content::BuildCameraBlockingVolume(data);
        Expect(volume.type == "camera_blocking_volume",
               "Content world-volume bridge should give camera blockers their own runtime type");
        Expect(volume.channels.size() == 2U &&
                   volume.channels[0] == ri::world::CollisionChannel::Camera &&
                   volume.channels[1] == ri::world::CollisionChannel::Player,
               "Content world-volume bridge should parse authored camera-blocking channels with filtering");
        ExpectVec3(volume.size, {2.0f, 2.0f, 2.0f},
                   "Content world-volume bridge should preserve camera-blocking fallback size");
    }

    {
        Value::Object data{};
        data["id"] = "listen_blocker";
        data["scale"] = MakeVec3(3.0, 4.0, 5.0);
        data["modes"] = Value::Array{Value("ai"), Value("visibility"), Value("ai")};
        data["enabled"] = false;
        const ri::world::ClipRuntimeVolume volume = ri::content::BuildAiPerceptionBlockerVolume(data);
        Expect(volume.type == "ai_perception_blocker_volume",
               "Content world-volume bridge should classify AI blockers distinctly");
        Expect(volume.modes.size() == 2U &&
                   volume.modes[0] == ri::world::ClipVolumeMode::AI &&
                   volume.modes[1] == ri::world::ClipVolumeMode::Visibility,
               "Content world-volume bridge should parse authored AI-blocker clip modes with filtering");
        Expect(!volume.enabled, "Content world-volume bridge should preserve authored AI-blocker enable state");
        ExpectVec3(volume.size, {3.0f, 4.0f, 5.0f},
                   "Content world-volume bridge should read AI blocker size from authored scale");
    }

    {
        Value::Object data{};
        data["damage"] = 320.0;
        data["label"] = "lava";
        const ri::world::DamageVolume volume = ri::content::BuildDamageVolume(data, false);
        Expect(volume.type == "damage_volume", "Content world-volume bridge should classify standard damage volumes");
        Expect(NearlyEqual(volume.damagePerSecond, 320.0f),
               "Content world-volume bridge should preserve authored damage-per-second values");
        Expect(!volume.killInstant, "Content world-volume bridge should not force standard damage volumes lethal");
        Expect(volume.label == "lava", "Content world-volume bridge should preserve authored damage labels");

        const ri::world::DamageVolume kill = ri::content::BuildDamageVolume(data, true);
        Expect(kill.type == "kill_volume", "Content world-volume bridge should classify kill volumes distinctly");
        Expect(kill.killInstant && NearlyEqual(kill.damagePerSecond, 9999.0f),
               "Content world-volume bridge should promote kill volumes to lethal runtime damage");
    }

    {
        Value::Object data{};
        data["fovOverride"] = 170.0;
        data["priority"] = -140.0;
        data["blendRadius"] = 128.0;
        data["cameraShakeAmplitude"] = 5.0;
        data["cameraShakeFrequency"] = 120.0;
        data["offset"] = Value::Array{Value(1.0), Value(-2.0), Value(0.5)};
        const ri::world::CameraModifierVolume volume = ri::content::BuildCameraModifierVolume(data);
        Expect(volume.type == "camera_modifier_volume",
               "Content world-volume bridge should classify camera modifier volumes");
        Expect(NearlyEqual(volume.fov, 140.0f),
               "Content world-volume bridge should clamp authored camera-modifier FOV");
        Expect(NearlyEqual(volume.priority, -100.0f),
               "Content world-volume bridge should clamp authored camera-modifier priority");
        Expect(NearlyEqual(volume.blendDistance, 64.0f)
                   && NearlyEqual(volume.shakeAmplitude, 4.0f)
                   && NearlyEqual(volume.shakeFrequency, 64.0f)
                   && NearlyEqual(volume.cameraOffset.x, 1.0f)
                   && NearlyEqual(volume.cameraOffset.y, -2.0f)
                   && NearlyEqual(volume.cameraOffset.z, 0.5f),
               "Content world-volume bridge should parse and clamp camera blend/shake/offset channels");
    }

    {
        Value::Object data{};
        data["dropAggro"] = false;
        const ri::world::SafeZoneVolume volume = ri::content::BuildSafeZoneVolume(data);
        Expect(volume.type == "safe_zone_volume",
               "Content world-volume bridge should classify safe-zone volumes");
        Expect(!volume.dropAggro,
               "Content world-volume bridge should preserve authored safe-zone drop-aggro behavior");
    }

    {
        Value::Object data{};
        data["lockAxes"] = Value::Array{Value("x"), Value("z"), Value("bogus"), Value("x")};
        const ri::world::PhysicsConstraintVolume volume = ri::content::BuildPhysicsConstraintVolume(data);
        Expect(volume.type == "physics_constraint_volume",
               "Content world-volume bridge should classify physics constraint volumes");
        Expect(volume.lockAxes.size() == 2U,
               "Content world-volume bridge should dedupe and filter authored lock axes");
        Expect(volume.lockAxes[0] == ri::world::ConstraintAxis::X &&
                   volume.lockAxes[1] == ri::world::ConstraintAxis::Z,
               "Content world-volume bridge should preserve authored lock-axis order");
        ExpectVec3(volume.size, {6.0f, 4.0f, 6.0f},
                   "Content world-volume bridge should preserve physics-constraint fallback size");
    }

    {
        Value::Object data{};
        data["lockPlane"] = "yz";
        data["freeze"] = true;
        const ri::world::PhysicsConstraintVolume volume = ri::content::BuildPhysicsConstraintVolume(data);
        Expect(volume.lockAxes.size() == 3U &&
                   volume.lockAxes[0] == ri::world::ConstraintAxis::X &&
                   volume.lockAxes[1] == ri::world::ConstraintAxis::Y &&
                   volume.lockAxes[2] == ri::world::ConstraintAxis::Z,
               "Content world-volume bridge should support lock-plane aliases and full freeze locks");
    }

    {
        Value::Object data{};
        data["translationAxis"] = MakeVec3(0.0, 0.0, 4.0);
        data["distance"] = 12.0;
        data["period"] = 0.01;
        data["pingPong"] = false;
        const ri::world::KinematicTranslationPrimitive volume = ri::content::BuildKinematicTranslationPrimitive(data);
        Expect(volume.type == "kinematic_translation_primitive" &&
                   NearlyEqual(volume.axis.z, 1.0f) &&
                   NearlyEqual(volume.distance, 12.0f) &&
                   NearlyEqual(volume.cycleSeconds, 0.1f) &&
                   !volume.pingPong,
               "Content world-volume bridge should classify kinematic translation primitives for moving platforms");
    }

    {
        Value::Object data{};
        data["rotationAxis"] = MakeVec3(1.0, 0.0, 0.0);
        data["degreesPerSecond"] = 5000.0;
        data["angleLimit"] = 90.0;
        data["pingPong"] = true;
        const ri::world::KinematicRotationPrimitive volume = ri::content::BuildKinematicRotationPrimitive(data);
        Expect(volume.type == "kinematic_rotation_primitive" &&
                   NearlyEqual(volume.axis.x, 1.0f) &&
                   NearlyEqual(volume.angularSpeedDegreesPerSecond, 1440.0f) &&
                   NearlyEqual(volume.maxAngleDegrees, 90.0f) &&
                   volume.pingPong,
               "Content world-volume bridge should classify kinematic rotation primitives for rotating hazards");
    }

    {
        Value::Object data{};
        data["path"] = Value::Array{MakeVec3(0.0, 0.0, 0.0), MakeVec3(4.0, 0.0, 0.0)};
        data["speed"] = 8.0;
        data["loop"] = false;
        const ri::world::SplinePathFollowerPrimitive volume = ri::content::BuildSplinePathFollowerPrimitive(data);
        Expect(volume.type == "spline_path_follower_primitive" &&
                   volume.splinePoints.size() == 2U &&
                   NearlyEqual(volume.speedUnitsPerSecond, 8.0f) &&
                   !volume.loop,
               "Content world-volume bridge should classify spline-path follower primitives for deterministic path motion");
    }

    {
        Value::Object data{};
        data["pointA"] = MakeVec3(0.0, 1.0, 0.0);
        data["pointB"] = MakeVec3(0.0, -3.0, 0.0);
        data["amplitude"] = 8.0;
        data["frequency"] = 20.0;
        data["collides"] = true;
        const ri::world::CablePrimitive volume = ri::content::BuildCablePrimitive(data);
        Expect(volume.type == "cable_primitive" &&
                   NearlyEqual(volume.start.y, 1.0f) &&
                   NearlyEqual(volume.end.y, -3.0f) &&
                   NearlyEqual(volume.swayAmplitude, 4.0f) &&
                   NearlyEqual(volume.swayFrequency, 16.0f) &&
                   volume.collisionEnabled,
               "Content world-volume bridge should classify cable primitives and clamp ambient sway parameters");
    }

    {
        Value::Object data{};
        data["clipModes"] = Value::Array{Value("visibility"), Value("collision")};
        data["enabled"] = false;
        const ri::world::ClippingRuntimeVolume volume = ri::content::BuildClippingVolume(data);
        Expect(volume.type == "clipping_volume" &&
                   volume.modes.size() == 2U &&
                   !volume.enabled,
               "Content world-volume bridge should classify clipping volumes with authored mode toggles");
    }

    {
        Value::Object data{};
        data["channels"] = Value::Array{Value("player"), Value("camera"), Value("player")};
        const ri::world::FilteredCollisionRuntimeVolume volume = ri::content::BuildFilteredCollisionRuntimeVolume(data);
        Expect(volume.type == "filtered_collision_volume" &&
                   volume.channels.size() == 3U &&
                   volume.channels.front() == "player",
               "Content world-volume bridge should classify filtered-collision runtime volumes with channel lists");
    }

    {
        Value::Object data{};
        data["type"] = "ladder_volume";
        data["climbSpeed"] = 30.0;
        data["scale"] = MakeVec3(2.0, 6.0, 2.0);
        const ri::world::TraversalLinkVolume volume = ri::content::BuildTraversalLinkVolume(data);
        Expect(volume.type == "ladder_volume" &&
                   volume.kind == ri::world::TraversalLinkKind::Ladder,
               "Content world-volume bridge should classify ladder traversal links");
        Expect(NearlyEqual(volume.climbSpeed, 12.0f),
               "Content world-volume bridge should clamp authored traversal climb speed");
        ExpectVec3(volume.size, {2.0f, 6.0f, 2.0f},
                   "Content world-volume bridge should preserve traversal-link extents");
    }

    {
        Value::Object data{};
        data["gridSize"] = 0.001;
        data["snapAxes"] = Value::Array{Value("x"), Value("z")};
        data["priority"] = 5000.0;
        data["size"] = MakeVec3(0.0, 4.0, 6.0);
        const ri::world::LocalGridSnapVolume volume = ri::content::BuildLocalGridSnapVolume(data);
        Expect(volume.type == "local_grid_snap_volume",
               "Content world-volume bridge should classify local grid snap volumes");
        Expect(NearlyEqual(volume.snapSize, 0.01f),
               "Content world-volume bridge should clamp authored local grid snap size");
        Expect(volume.snapX && !volume.snapY && volume.snapZ && volume.priority == 1000,
               "Content world-volume bridge should parse local-grid-snap axes and clamp priority");
        ExpectVec3(volume.size, {0.001f, 4.0f, 6.0f},
                   "Content world-volume bridge should clamp local-grid-snap extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["partitionMode"] = "skip";
        const ri::world::HintPartitionVolume volume = ri::content::BuildHintPartitionVolume(data);
        Expect(volume.type == "hint_skip_brush" &&
                   volume.mode == ri::world::HintPartitionMode::Skip,
               "Content world-volume bridge should classify hint-skip brushes");
    }

    {
        Value::Object data{};
        data["scale"] = MakeVec3(6.0, 4.0, 6.0);
        const ri::world::CameraConfinementVolume volume = ri::content::BuildCameraConfinementVolume(data);
        Expect(volume.type == "camera_confinement_volume",
               "Content world-volume bridge should classify camera confinement volumes");
        ExpectVec3(volume.size, {6.0f, 4.0f, 6.0f},
                   "Content world-volume bridge should preserve camera confinement extents");
    }

    {
        Value::Object data{};
        data["forcedLod"] = "far";
        data["targetIds"] = Value::Array{Value("mesh_a"), Value("mesh_b")};
        const ri::world::LodOverrideVolume volume = ri::content::BuildLodOverrideVolume(data);
        Expect(volume.type == "lod_override_volume" &&
                   volume.forcedLod == ri::world::ForcedLod::Far,
               "Content world-volume bridge should classify lod override volumes");
        Expect(volume.targetIds.size() == 2U &&
                   volume.targetIds[0] == "mesh_a" &&
                   volume.targetIds[1] == "mesh_b",
               "Content world-volume bridge should preserve authored lod override target IDs");
    }

    {
        Value::Object nearRepresentationPayload{};
        nearRepresentationPayload["id"] = "mesh_near";
        Value::Object nearRepresentation{};
        nearRepresentation["kind"] = "mesh";
        nearRepresentation["payload"] = nearRepresentationPayload;
        Value::Object nearLevel{};
        nearLevel["name"] = "near";
        nearLevel["representation"] = nearRepresentation;
        nearLevel["collisionProfile"] = "full";
        nearLevel["distanceEnter"] = 0.0;
        nearLevel["distanceExit"] = 28.0;

        Value::Object farRepresentationPayload{};
        farRepresentationPayload["clusterId"] = "cluster_far";
        Value::Object farRepresentation{};
        farRepresentation["kind"] = "cluster";
        farRepresentation["payload"] = farRepresentationPayload;
        Value::Object farLevel{};
        farLevel["name"] = "far";
        farLevel["representation"] = farRepresentation;
        farLevel["collisionProfile"] = "simplified_or_none";
        farLevel["distanceEnter"] = 24.0;
        farLevel["distanceExit"] = 100000.0;

        Value::Object policy{};
        policy["metric"] = "camera_distance";
        policy["hysteresisEnabled"] = true;
        policy["transitionMode"] = "crossfade";
        policy["crossfadeSeconds"] = 2.0;

        Value::Object debug{};
        debug["showActiveLevel"] = true;
        debug["showRanges"] = true;

        Value::Object transform{};
        transform["position"] = MakeVec3(5.0, 1.0, -2.0);
        transform["rotation"] = MakeVec3(0.0, 0.5, 0.0);
        transform["scale"] = MakeVec3(2.0, 3.0, 4.0);

        Value::Object data{};
        data["transform"] = transform;
        data["levels"] = Value::Array{nearLevel, farLevel};
        data["policy"] = policy;
        data["debug"] = debug;
        const ri::world::LodSwitchPrimitive volume = ri::content::BuildLodSwitchPrimitive(data);
        Expect(volume.type == "lod_switch_primitive"
                   && volume.levels.size() == 2U
                   && volume.levels.front().representation.kind == ri::world::LodSwitchRepresentationKind::Mesh
                   && volume.levels.front().representation.payloadId == "mesh_near"
                   && volume.levels.back().representation.kind == ri::world::LodSwitchRepresentationKind::Cluster
                   && volume.levels.back().representation.payloadId == "cluster_far"
                   && volume.levels.back().collisionProfile == ri::world::LodSwitchCollisionProfile::Simplified
                   && volume.policy.metric == ri::world::LodSwitchMetric::CameraDistance
                   && volume.policy.transitionMode == ri::world::LodSwitchTransitionMode::Crossfade
                   && NearlyEqual(volume.policy.crossfadeSeconds, 2.0f)
                   && NearlyEqual(volume.position.x, 5.0f)
                   && NearlyEqual(volume.size.y, 3.0f)
                   && volume.debug.showActiveLevel,
               "Content world-volume bridge should parse lod-switch primitive levels and deterministic policy settings");
    }

    {
        Value::Object sourcePayload{};
        sourcePayload["meshId"] = "debris_mesh_a";
        Value::Object sourceRepresentation{};
        sourceRepresentation["kind"] = "mesh";
        sourceRepresentation["payload"] = sourcePayload;

        Value::Object density{};
        density["count"] = 40000.0;
        density["densityPerSquareMeter"] = 1000.0;
        density["maxPoints"] = 100.0;

        Value::Object distribution{};
        distribution["seed"] = 77.0;
        distribution["slopeMin"] = 70.0;
        distribution["slopeMax"] = 25.0;
        distribution["minHeight"] = 200.0;
        distribution["maxHeight"] = -20.0;
        distribution["minNormalY"] = 9.0;
        distribution["minSeparation"] = -3.0;
        distribution["scaleJitter"] = MakeVec3(-1.0, 0.4, 9.0);

        Value::Object culling{};
        culling["maxActiveDistance"] = 250000.0;
        culling["frustumCulling"] = false;

        Value::Object animation{};
        animation["windSwayEnabled"] = true;
        animation["swayAmplitude"] = 100.0;
        animation["swayFrequency"] = -2.0;

        Value::Object baseTransform{};
        baseTransform["position"] = MakeVec3(6.0, 0.2, -11.0);
        baseTransform["rotation"] = MakeVec3(0.0, 0.8, 0.0);
        baseTransform["scale"] = MakeVec3(0.0, 4.0, 9.0);

        Value::Object data{};
        data["targetIds"] = Value::Array{"receiver_floor_a", "", "receiver_floor_b"};
        data["sourceRepresentation"] = sourceRepresentation;
        data["density"] = density;
        data["distribution"] = distribution;
        data["collisionPolicy"] = "proxy";
        data["culling"] = culling;
        data["animation"] = animation;
        data["baseTransform"] = baseTransform;
        const ri::world::SurfaceScatterVolume volume = ri::content::BuildSurfaceScatterVolume(data);
        Expect(volume.type == "surface_scatter_volume"
                   && volume.targetIds.size() == 2U
                   && volume.targetIds.front() == "receiver_floor_a"
                   && volume.sourceRepresentation.kind == ri::world::SurfaceScatterRepresentationKind::Mesh
                   && volume.sourceRepresentation.payloadId == "debris_mesh_a"
                   && volume.density.count == 20000U
                   && NearlyEqual(volume.density.densityPerSquareMeter, 500.0f)
                   && volume.density.maxPoints == 20000U
                   && volume.distribution.seed == 77U
                   && NearlyEqual(volume.distribution.minSlopeDegrees, 25.0f)
                   && NearlyEqual(volume.distribution.maxSlopeDegrees, 70.0f)
                   && NearlyEqual(volume.distribution.minHeight, -20.0f)
                   && NearlyEqual(volume.distribution.maxHeight, 200.0f)
                   && NearlyEqual(volume.distribution.minNormalY, 1.0f)
                   && NearlyEqual(volume.distribution.minSeparation, 0.0f)
                   && NearlyEqual(volume.distribution.scaleJitter.x, 0.0f)
                   && NearlyEqual(volume.distribution.scaleJitter.z, 8.0f)
                   && volume.collisionPolicy == ri::world::SurfaceScatterCollisionPolicy::Proxy
                   && NearlyEqual(volume.culling.maxActiveDistance, 100000.0f)
                   && !volume.culling.frustumCulling
                   && volume.animation.windSwayEnabled
                   && NearlyEqual(volume.animation.swayAmplitude, 10.0f)
                   && NearlyEqual(volume.animation.swayFrequency, 0.0f)
                   && NearlyEqual(volume.position.x, 6.0f)
                   && NearlyEqual(volume.size.x, 0.001f)
                   && NearlyEqual(volume.size.y, 4.0f)
                   && NearlyEqual(volume.size.z, 9.0f),
               "Content world-volume bridge should parse deterministic surface-scatter controls with graceful budget and filter clamping");
    }

    {
        Value::Object transform{};
        transform["position"] = MakeVec3(2.0, 0.5, -4.0);
        transform["scale"] = MakeVec3(0.0, 3.0, 7.0);

        Value::Object data{};
        data["targetIds"] = Value::Array{"segment_a", "", "segment_b"};
        data["spline"] = Value::Array{
            MakeVec3(-4.0, 0.0, 0.0),
            MakeVec3(0.0, 0.3, 3.0),
            MakeVec3(4.0, 0.0, 0.0),
        };
        data["count"] = 9000.0;
        data["sectionCount"] = 800.0;
        data["segmentLength"] = -5.0;
        data["tangentSmoothing"] = 2.0;
        data["collisionEnabled"] = true;
        data["dynamicEnabled"] = true;
        data["seed"] = 404.0;
        data["maxSamples"] = 128.0;
        data["maxActiveDistance"] = -30.0;
        data["frustumCulling"] = false;
        data["transform"] = transform;
        const ri::world::SplineMeshDeformerPrimitive volume = ri::content::BuildSplineMeshDeformerPrimitive(data);
        Expect(volume.type == "spline_mesh_deformer"
                   && volume.targetIds.size() == 2U
                   && volume.sampleCount == 2048U
                   && volume.sectionCount == 256U
                   && NearlyEqual(volume.segmentLength, 0.05f)
                   && NearlyEqual(volume.tangentSmoothing, 1.0f)
                   && volume.collisionEnabled
                   && volume.dynamicEnabled
                   && volume.seed == 404U
                   && volume.maxSamples == 2048U
                   && NearlyEqual(volume.maxActiveDistance, 1.0f)
                   && !volume.frustumCulling
                   && NearlyEqual(volume.position.x, 2.0f)
                   && NearlyEqual(volume.size.x, 0.001f)
                   && NearlyEqual(volume.size.y, 3.0f)
                   && NearlyEqual(volume.size.z, 7.0f),
               "Content world-volume bridge should parse spline mesh deformers with deterministic caps and transform aliases");
    }

    {
        Value::Object transform{};
        transform["position"] = MakeVec3(-6.0, 0.2, -2.0);
        transform["scale"] = MakeVec3(0.0, 1.0, 9.0);

        Value::Object data{};
        data["spline"] = Value::Array{
            MakeVec3(-6.0, 0.0, -4.0),
            MakeVec3(-2.0, 0.0, 2.0),
            MakeVec3(3.0, 0.0, -1.0),
        };
        data["width"] = -2.0;
        data["tessellationFactor"] = 9999.0;
        data["offsetY"] = 20.0;
        data["uvScaleU"] = -4.0;
        data["uvScaleV"] = 3000.0;
        data["tangentSmoothing"] = 2.0;
        data["transparentBlend"] = true;
        data["depthWrite"] = true;
        data["collisionEnabled"] = true;
        data["dynamicEnabled"] = true;
        data["seed"] = 808.0;
        data["maxSamples"] = 64.0;
        data["maxActiveDistance"] = -20.0;
        data["frustumCulling"] = false;
        data["transform"] = transform;
        const ri::world::SplineDecalRibbonPrimitive volume = ri::content::BuildSplineDecalRibbonPrimitive(data);
        Expect(volume.type == "spline_decal_ribbon"
                   && NearlyEqual(volume.width, 0.01f)
                   && volume.tessellation == 4096U
                   && NearlyEqual(volume.offsetY, 10.0f)
                   && NearlyEqual(volume.uvScaleU, 0.01f)
                   && NearlyEqual(volume.uvScaleV, 1000.0f)
                   && NearlyEqual(volume.tangentSmoothing, 1.0f)
                   && volume.transparentBlend
                   && volume.depthWrite
                   && volume.collisionEnabled
                   && volume.dynamicEnabled
                   && volume.seed == 808U
                   && volume.maxSamples == 4096U
                   && NearlyEqual(volume.maxActiveDistance, 1.0f)
                   && !volume.frustumCulling
                   && NearlyEqual(volume.position.x, -6.0f)
                   && NearlyEqual(volume.size.x, 0.001f)
                   && NearlyEqual(volume.size.z, 9.0f),
               "Content world-volume bridge should parse spline decal ribbons with deterministic sampling caps and safe rendering defaults");
    }

    {
        Value::Object transform{};
        transform["position"] = MakeVec3(1.0, 0.0, -3.0);
        transform["scale"] = MakeVec3(0.0, 2.0, 8.0);

        Value::Object textures{};
        textures["shared"] = Value("atlas_main");
        textures["x"] = Value("ignored_when_shared");

        Value::Object debug{};
        debug["previewTint"] = Value(true);
        debug["showAxisContributions"] = Value(true);

        Value::Object data{};
        data["targetIds"] = Value::Array{"segment_x", "", "segment_y"};
        data["remapMode"] = Value("Axis_Dominant");
        data["textures"] = textures;
        data["projectionScale"] = -2.0;
        data["blendSharpness"] = 0.0;
        data["axisWeights"] = MakeVec3(-4.0, 0.25, 20.0);
        data["maxMaterialPatches"] = 9000.0;
        data["maxActiveDistance"] = -10.0;
        data["frustumCulling"] = Value(false);
        data["debug"] = debug;
        data["transform"] = transform;
        const ri::world::TopologicalUvRemapperVolume volume = ri::content::BuildTopologicalUvRemapperVolume(data);
        Expect(volume.type == "topological_uv_remapper"
                   && volume.targetIds.size() == 2U
                   && volume.remapMode == "axis_dominant"
                   && volume.sharedTextureId == "atlas_main"
                   && NearlyEqual(volume.projectionScale, 1.0e-6f)
                   && NearlyEqual(volume.blendSharpness, 0.25f)
                   && NearlyEqual(volume.axisWeights.x, 0.001f)
                   && NearlyEqual(volume.axisWeights.y, 0.25f)
                   && NearlyEqual(volume.axisWeights.z, 8.0f)
                   && volume.maxMaterialPatches == 4096U
                   && NearlyEqual(volume.maxActiveDistance, 1.0f)
                   && !volume.frustumCulling
                   && volume.debug.previewTint
                   && volume.debug.axisContributionPreview
                   && NearlyEqual(volume.position.x, 1.0f)
                   && NearlyEqual(volume.size.x, 0.001f)
                   && NearlyEqual(volume.size.y, 2.0f)
                   && NearlyEqual(volume.size.z, 8.0f),
               "Content world-volume bridge should parse topological UV remappers with texture-set aliases and clamped projection controls");
    }

    {
        Value::Object textures{};
        textures["x"] = Value("rock_x");
        textures["y"] = Value("rock_y");
        textures["z"] = Value("rock_z");

        Value::Object data{};
        data["targets"] = Value::Array{"cliff_a", "cliff_b"};
        data["textures"] = textures;
        data["projectionScale"] = 8000.0;
        data["blend"] = 100.0;
        data["objectSpaceAxes"] = Value(true);
        data["maxPatches"] = 0.0;
        const ri::world::TriPlanarNode volume = ri::content::BuildTriPlanarNode(data);
        Expect(volume.type == "tri_planar_node"
                   && volume.targetIds.size() == 2U
                   && volume.textureX == "rock_x"
                   && volume.textureY == "rock_y"
                   && volume.textureZ == "rock_z"
                   && volume.sharedTextureId.empty()
                   && NearlyEqual(volume.projectionScale, 4096.0f)
                   && NearlyEqual(volume.blendSharpness, 64.0f)
                   && volume.maxMaterialPatches == 1U
                   && volume.objectSpaceAxes,
               "Content world-volume bridge should parse tri-planar nodes with per-axis texture maps and strict clamps");
    }

    {
        Value::Object sourcePayload{};
        sourcePayload["meshId"] = "crate_mesh_a";
        Value::Object sourceRepresentation{};
        sourceRepresentation["kind"] = "mesh";
        sourceRepresentation["payload"] = sourcePayload;

        Value::Object variation{};
        variation["rotationJitter"] = MakeVec3(0.0, 0.4, 0.0);
        variation["scaleJitter"] = MakeVec3(0.2, -2.0, 0.1);
        variation["positionJitter"] = MakeVec3(0.3, 0.1, 0.3);

        Value::Object culling{};
        culling["maxActiveDistance"] = 250000.0;
        culling["frustumCulling"] = false;

        Value::Object baseTransform{};
        baseTransform["position"] = MakeVec3(3.0, 0.5, -9.0);
        baseTransform["rotation"] = MakeVec3(0.0, 1.0, 0.0);
        baseTransform["scale"] = MakeVec3(0.0, 6.0, 8.0);

        Value::Object data{};
        data["sourceRepresentation"] = sourceRepresentation;
        data["count"] = 40960.0;
        data["offsetStep"] = MakeVec3(0.5, 0.0, 0.5);
        data["distribution"] = MakeVec3(12.0, 5.0, 12.0);
        data["seed"] = 12345.0;
        data["variation"] = variation;
        data["collisionPolicy"] = "per-instance";
        data["culling"] = culling;
        data["baseTransform"] = baseTransform;
        const ri::world::InstanceCloudPrimitive volume = ri::content::BuildInstanceCloudPrimitive(data);
        Expect(volume.type == "instance_cloud_primitive"
                   && volume.sourceRepresentation.kind == ri::world::InstanceCloudRepresentationKind::Mesh
                   && volume.sourceRepresentation.payloadId == "crate_mesh_a"
                   && volume.count == 20000U
                   && NearlyEqual(volume.offsetStep.x, 0.5f)
                   && NearlyEqual(volume.distributionExtents.y, 5.0f)
                   && volume.seed == 12345U
                   && NearlyEqual(volume.variation.rotationJitterRadians.y, 0.4f)
                   && NearlyEqual(volume.variation.scaleJitter.y, 0.0f)
                   && volume.collisionPolicy == ri::world::InstanceCloudCollisionPolicy::PerInstance
                   && NearlyEqual(volume.culling.maxActiveDistance, 100000.0f)
                   && !volume.culling.frustumCulling
                   && NearlyEqual(volume.position.x, 3.0f)
                   && NearlyEqual(volume.size.x, 0.001f)
                   && NearlyEqual(volume.size.y, 6.0f)
                   && NearlyEqual(volume.size.z, 8.0f),
               "Content world-volume bridge should parse deterministic instance-cloud layout, transform aliases, and policy caps");
    }

    {
        Value::Object data{};
        data["targetIds"] = Value::Array{"wall_a", "", "wall_b"};
        data["cellCount"] = 2048.0;
        data["noiseJitter"] = 2.0;
        data["seed"] = 99.0;
        data["capFaces"] = Value(false);
        const ri::world::VoronoiFracturePrimitive primitive = ri::content::BuildVoronoiFracturePrimitive(data);
        Expect(primitive.type == "voronoi_fracture_primitive"
                   && primitive.targetIds.size() == 2U
                   && primitive.cellCount == 1024U
                   && NearlyEqual(primitive.noiseJitter, 1.0f)
                   && primitive.seed == 99U
                   && !primitive.capOpenFaces,
               "Content world-volume bridge should parse voronoi fracture primitives with deterministic fracture budget clamps");
    }

    {
        Value::Object data{};
        data["points"] = Value::Array{MakeVec3(0.0, 0.0, 0.0), MakeVec3(1.0, 0.2, 0.0)};
        data["threshold"] = 9.0;
        data["blend"] = -2.0;
        data["resolution"] = 2.0;
        const ri::world::MetaballPrimitive primitive = ri::content::BuildMetaballPrimitive(data);
        Expect(primitive.type == "metaball_primitive"
                   && primitive.controlPoints.size() == 2U
                   && NearlyEqual(primitive.isoLevel, 4.0f)
                   && NearlyEqual(primitive.smoothing, 0.0f)
                   && primitive.resolution == 4U,
               "Content world-volume bridge should parse metaball primitives with deterministic field and marching resolution clamps");
    }

    {
        Value::Object data{};
        data["targets"] = Value::Array{"beam_target"};
        data["grid"] = MakeVec3(-2.0, 0.3, 90.0);
        data["thickness"] = -4.0;
        data["cellBudget"] = 2.0;
        const ri::world::LatticeVolume volume = ri::content::BuildLatticeVolume(data);
        Expect(volume.type == "lattice_volume"
                   && volume.targetIds.size() == 1U
                   && NearlyEqual(volume.cellSize.x, 0.01f)
                   && NearlyEqual(volume.cellSize.y, 0.3f)
                   && NearlyEqual(volume.cellSize.z, 64.0f)
                   && NearlyEqual(volume.beamRadius, 0.001f)
                   && volume.maxCells == 8U,
               "Content world-volume bridge should parse lattice volumes with deterministic cell and beam clamps");
    }

    {
        Value::Object data{};
        data["type"] = "spline_ribbon";
        data["targetIds"] = Value::Array{"walkway_a"};
        data["path"] = Value::Array{MakeVec3(-2.0, 0.0, 0.0), MakeVec3(2.0, 0.0, 0.0)};
        data["width"] = 1.5;
        const ri::world::SplineDecalRibbonPrimitive primitive = ri::content::BuildSplineDecalRibbonPrimitive(data);
        Expect((primitive.type == "spline_ribbon" || primitive.type == "spline_decal_ribbon")
                   && primitive.splinePoints.size() == 2U
                   && NearlyEqual(primitive.width, 1.5f),
               "Content world-volume bridge should preserve spline_ribbon alias typing while reusing spline-decal ribbon parsing");
    }

    {
        Value::Object data{};
        data["targets"] = Value::Array{"pipe_a"};
        data["path"] = Value::Array{MakeVec3(0.0, 0.0, 0.0), MakeVec3(3.0, 0.0, 0.0)};
        data["radius"] = -1.0;
        data["count"] = 5000.0;
        data["capEnds"] = Value(false);
        const ri::world::ManifoldSweepPrimitive primitive = ri::content::BuildManifoldSweepPrimitive(data);
        Expect(primitive.type == "manifold_sweep"
                   && primitive.targetIds.size() == 1U
                   && primitive.splinePoints.size() == 2U
                   && NearlyEqual(primitive.profileRadius, 0.001f)
                   && primitive.sampleCount == 2048U
                   && !primitive.capEnds,
               "Content world-volume bridge should parse manifold sweep primitives with deterministic profile and sampling clamps");
    }

    {
        Value::Object data{};
        data["targets"] = Value::Array{"rail_a"};
        data["path"] = Value::Array{MakeVec3(0.0, 0.0, 0.0), MakeVec3(4.0, 0.0, 0.0)};
        data["trimSheet"] = Value("sheet_city");
        data["tileU"] = -2.0;
        data["tileV"] = 4000.0;
        data["segments"] = 1.0;
        const ri::world::TrimSheetSweepPrimitive primitive = ri::content::BuildTrimSheetSweepPrimitive(data);
        Expect(primitive.type == "trim_sheet_sweep"
                   && primitive.targetIds.size() == 1U
                   && primitive.trimSheetId == "sheet_city"
                   && NearlyEqual(primitive.uvTileU, 0.001f)
                   && NearlyEqual(primitive.uvTileV, 1024.0f)
                   && primitive.tessellation == 2U,
               "Content world-volume bridge should parse trim-sheet sweeps with deterministic UV tiling and tessellation clamps");
    }

    {
        Value::Object data{};
        data["targets"] = Value::Array{"branch_target"};
        data["depth"] = 20.0;
        data["length"] = -3.0;
        data["angle"] = 240.0;
        const ri::world::LSystemBranchPrimitive primitive = ri::content::BuildLSystemBranchPrimitive(data);
        Expect(primitive.type == "l_system_branch_primitive"
                   && primitive.targetIds.size() == 1U
                   && primitive.iterations == 10U
                   && NearlyEqual(primitive.segmentLength, 0.01f)
                   && NearlyEqual(primitive.branchAngleDegrees, 180.0f),
               "Content world-volume bridge should parse l-system branch primitives with deterministic recursion and angle clamps");
    }

    {
        Value::Object data{};
        data["detail"] = 99.0;
        data["radius"] = -2.0;
        const ri::world::GeodesicSpherePrimitive primitive = ri::content::BuildGeodesicSpherePrimitive(data);
        Expect(primitive.type == "geodesic_sphere"
                   && primitive.subdivisionLevel == 8U
                   && NearlyEqual(primitive.radiusScale, 0.001f),
               "Content world-volume bridge should parse geodesic sphere primitives with deterministic detail and radius clamps");
    }

    {
        Value::Object data{};
        data["targets"] = Value::Array{"normal_target"};
        data["amount"] = 200.0;
        data["layers"] = 900.0;
        data["capEdges"] = Value(false);
        const ri::world::ExtrudeAlongNormalPrimitive primitive = ri::content::BuildExtrudeAlongNormalPrimitive(data);
        Expect(primitive.type == "extrude_along_normal_primitive"
                   && primitive.targetIds.size() == 1U
                   && NearlyEqual(primitive.distance, 100.0f)
                   && primitive.shellCount == 256U
                   && !primitive.capOpenEdges,
               "Content world-volume bridge should parse extrude-along-normal primitives with deterministic shell and distance clamps");
    }

    {
        Value::Object data{};
        data["powerX"] = -2.0;
        data["powerY"] = 40.0;
        data["powerZ"] = 3.0;
        data["segments"] = 2.0;
        data["rings"] = 500.0;
        const ri::world::SuperellipsoidPrimitive primitive = ri::content::BuildSuperellipsoidPrimitive(data);
        Expect(primitive.type == "superellipsoid"
                   && NearlyEqual(primitive.exponentX, 0.1f)
                   && NearlyEqual(primitive.exponentY, 16.0f)
                   && NearlyEqual(primitive.exponentZ, 3.0f)
                   && primitive.radialSegments == 3U
                   && primitive.rings == 256U,
               "Content world-volume bridge should parse superellipsoid primitives with deterministic exponent and topology clamps");
    }

    {
        Value::Object data{};
        data["targets"] = Value::Array{"demo_a"};
        data["grid"] = MakeVec3(-2.0, 0.4, 200.0);
        data["cellBudget"] = 2.0;
        const ri::world::PrimitiveDemoLattice primitive = ri::content::BuildPrimitiveDemoLattice(data);
        Expect(primitive.type == "primitive_demo_lattice"
                   && primitive.targetIds.size() == 1U
                   && NearlyEqual(primitive.cellSize.x, 0.01f)
                   && primitive.maxCells == 8U,
               "Content world-volume bridge should parse primitive-demo lattice helpers with deterministic clamp behavior");
    }

    {
        Value::Object data{};
        data["targets"] = Value::Array{"demo_b"};
        data["cells"] = 4000.0;
        data["jitter"] = 9.0;
        const ri::world::PrimitiveDemoVoronoi primitive = ri::content::BuildPrimitiveDemoVoronoi(data);
        Expect(primitive.type == "primitive_demo_voronoi"
                   && primitive.targetIds.size() == 1U
                   && primitive.cellCount == 1024U
                   && NearlyEqual(primitive.jitter, 1.0f),
               "Content world-volume bridge should parse primitive-demo voronoi helpers with deterministic clamp behavior");
    }

    {
        Value::Object data{};
        data["points"] = Value::Array{MakeVec3(0.0, 0.0, 0.0), MakeVec3(1.0, 0.0, 0.0), MakeVec3(0.0, 1.0, 0.0)};
        data["depth"] = -3.0;
        const ri::world::ThickPolygonPrimitive primitive = ri::content::BuildThickPolygonPrimitive(data);
        Expect(primitive.type == "thick_polygon_primitive"
                   && primitive.points.size() == 3U
                   && NearlyEqual(primitive.thickness, 0.001f),
               "Content world-volume bridge should parse thick polygon primitives with deterministic extrusion clamps");
    }

    {
        Value::Object data{};
        data["profile"] = Value("profile_arcade_wall");
        data["scale"] = -2.0;
        data["segments"] = 10000.0;
        const ri::world::StructuralProfilePrimitive primitive = ri::content::BuildStructuralProfilePrimitive(data);
        Expect(primitive.type == "structural_profile"
                   && primitive.profileId == "profile_arcade_wall"
                   && NearlyEqual(primitive.profileScale, 0.001f)
                   && primitive.segmentCount == 4096U,
               "Content world-volume bridge should parse structural profile helpers with deterministic profile constraints");
    }

    {
        Value::Object data{};
        data["radius"] = -8.0;
        data["length"] = 0.0;
        data["segments"] = 1.0;
        const ri::world::HalfPipePrimitive primitive = ri::content::BuildHalfPipePrimitive(data);
        Expect(primitive.type == "half_pipe"
                   && NearlyEqual(primitive.radius, 0.01f)
                   && NearlyEqual(primitive.length, 0.01f)
                   && primitive.radialSegments == 3U,
               "Content world-volume bridge should parse half-pipe primitives with deterministic geometric clamps");
    }

    {
        Value::Object data{};
        data["radius"] = 20000.0;
        data["length"] = -4.0;
        data["segments"] = 999.0;
        const ri::world::QuarterPipePrimitive primitive = ri::content::BuildQuarterPipePrimitive(data);
        Expect(primitive.type == "quarter_pipe"
                   && NearlyEqual(primitive.radius, 1000.0f)
                   && NearlyEqual(primitive.length, 0.01f)
                   && primitive.radialSegments == 512U,
               "Content world-volume bridge should parse quarter-pipe primitives with deterministic geometric clamps");
    }

    {
        Value::Object data{};
        data["radius"] = -2.0;
        data["angle"] = 400.0;
        data["segments"] = 1.0;
        data["bendSegments"] = 900.0;
        const ri::world::PipeElbowPrimitive primitive = ri::content::BuildPipeElbowPrimitive(data);
        Expect(primitive.type == "pipe_elbow"
                   && NearlyEqual(primitive.radius, 0.01f)
                   && NearlyEqual(primitive.bendDegrees, 359.0f)
                   && primitive.radialSegments == 3U
                   && primitive.bendSegments == 512U,
               "Content world-volume bridge should parse pipe-elbow primitives with deterministic bend and tessellation clamps");
    }

    {
        Value::Object data{};
        data["radius"] = -2.0;
        data["tubeRadius"] = 900.0;
        data["angle"] = 400.0;
        data["radialSegments"] = 1.0;
        data["segments"] = 1000.0;
        const ri::world::TorusSlicePrimitive primitive = ri::content::BuildTorusSlicePrimitive(data);
        Expect(primitive.type == "torus_slice"
                   && NearlyEqual(primitive.majorRadius, 0.01f)
                   && NearlyEqual(primitive.minorRadius, 500.0f)
                   && NearlyEqual(primitive.sweepDegrees, 360.0f)
                   && primitive.radialSegments == 3U
                   && primitive.tubularSegments == 512U,
               "Content world-volume bridge should parse torus-slice primitives with deterministic ring and sweep clamps");
    }

    {
        Value::Object data{};
        data["targets"] = Value::Array{"spline_target"};
        data["path"] = Value::Array{MakeVec3(0.0, 0.0, 0.0), MakeVec3(3.0, 0.0, 0.0)};
        data["radius"] = -4.0;
        data["count"] = 10000.0;
        const ri::world::SplineSweepPrimitive primitive = ri::content::BuildSplineSweepPrimitive(data);
        Expect(primitive.type == "spline_sweep"
                   && primitive.targetIds.size() == 1U
                   && primitive.splinePoints.size() == 2U
                   && NearlyEqual(primitive.profileRadius, 0.001f)
                   && primitive.sampleCount == 4096U,
               "Content world-volume bridge should parse spline-sweep primitives with deterministic profile and sampling clamps");
    }

    {
        Value::Object data{};
        data["profile"] = Value::Array{MakeVec3(1.0, 0.0, 0.0), MakeVec3(0.5, 1.0, 0.0)};
        data["angle"] = 0.0;
        data["segments"] = 1.0;
        const ri::world::RevolvePrimitive primitive = ri::content::BuildRevolvePrimitive(data);
        Expect(primitive.type == "revolve"
                   && primitive.profilePoints.size() == 2U
                   && NearlyEqual(primitive.sweepDegrees, 1.0f)
                   && primitive.segmentCount == 3U,
               "Content world-volume bridge should parse revolve primitives with deterministic profile and sweep clamps");
    }

    {
        Value::Object data{};
        data["radius"] = -8.0;
        data["thickness"] = -2.0;
        data["heightScale"] = 3.0;
        data["segments"] = 1.0;
        const ri::world::DomeVaultPrimitive primitive = ri::content::BuildDomeVaultPrimitive(data);
        Expect(primitive.type == "dome_vault"
                   && NearlyEqual(primitive.radius, 0.01f)
                   && NearlyEqual(primitive.thickness, 0.001f)
                   && NearlyEqual(primitive.heightRatio, 1.0f)
                   && primitive.radialSegments == 3U,
               "Content world-volume bridge should parse dome-vault primitives with deterministic geometry clamps");
    }

    {
        Value::Object data{};
        data["path"] = Value::Array{MakeVec3(0.0, 0.0, 0.0), MakeVec3(2.0, 0.0, 0.0)};
        data["profile"] = Value::Array{MakeVec3(0.5, 0.0, 0.0), MakeVec3(0.0, 0.5, 0.0)};
        data["segments"] = 1.0;
        const ri::world::LoftPrimitive primitive = ri::content::BuildLoftPrimitive(data);
        Expect(primitive.type == "loft_primitive"
                   && primitive.pathPoints.size() == 2U
                   && primitive.profilePoints.size() == 2U
                   && primitive.segmentCount == 2U,
               "Content world-volume bridge should parse loft primitives with deterministic profile/path topology clamps");
    }

    {
        Value::Object data{};
        data["cost"] = 500.0;
        data["areaType"] = "slow";
        const ri::world::NavmeshModifierVolume volume = ri::content::BuildNavmeshModifierVolume(data);
        Expect(volume.type == "navmesh_modifier_volume" &&
                   NearlyEqual(volume.traversalCost, 100.0f) &&
                   volume.tag == "slow",
               "Content world-volume bridge should classify and clamp navmesh modifier volumes");
    }

    {
        Value::Object data{};
        data["type"] = "ambient_audio_spline";
        data["audioPath"] = "Assets/Audio/ambience_loop.ogg";
        data["baseVolume"] = 0.8;
        data["radius"] = 400.0;
        data["label"] = "wind_tunnel";
        data["spline"] = Value::Array{
            MakeVec3(0.0, 0.0, 0.0),
            MakeVec3(10.0, 0.0, 0.0),
            MakeVec3(20.0, 0.0, 10.0),
        };
        const ri::world::AmbientAudioVolume volume = ri::content::BuildAmbientAudioVolume(data);
        Expect(volume.type == "ambient_audio_spline",
               "Content world-volume bridge should classify ambient audio spline volumes");
        Expect(volume.audioPath == "Assets/Audio/ambience_loop.ogg" &&
                   volume.label == "wind_tunnel",
               "Content world-volume bridge should preserve authored ambient audio labels and paths");
        Expect(NearlyEqual(volume.baseVolume, 0.8f) &&
                   NearlyEqual(volume.maxDistance, 256.0f),
               "Content world-volume bridge should clamp authored ambient audio tuning");
        Expect(volume.splinePoints.size() == 3U,
               "Content world-volume bridge should preserve authored ambient audio spline points");
    }

    {
        Value::Object data{};
        data["type"] = "anti_portal";
        data["position"] = MakeVec3(10.0, 2.0, -4.0);
        data["rotation"] = MakeVec3(0.0, 1.57079632679, 0.0);
        data["scale"] = MakeVec3(8.0, 10.0, 0.0);
        const ri::world::VisibilityPrimitive primitive = ri::content::BuildVisibilityPrimitive(data);
        Expect(primitive.type == "anti_portal" &&
                   primitive.kind == ri::world::VisibilityPrimitiveKind::AntiPortal,
               "Content world-volume bridge should classify anti-portal visibility primitives");
        ExpectVec3(primitive.position, {10.0f, 2.0f, -4.0f},
                   "Content world-volume bridge should preserve visibility-primitive positions");
        ExpectVec3(primitive.rotationRadians, {0.0f, 1.570796f, 0.0f},
                   "Content world-volume bridge should preserve visibility-primitive rotation");
        ExpectVec3(primitive.size, {8.0f, 10.0f, 0.001f},
                   "Content world-volume bridge should clamp visibility-primitive extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["isClosed"] = false;
        data["size"] = MakeVec3(6.0, 3.0, 0.0);
        const ri::world::OcclusionPortalVolume volume = ri::content::BuildOcclusionPortalVolume(data);
        Expect(volume.type == "occlusion_portal",
               "Content world-volume bridge should classify occlusion portals");
        Expect(!volume.closed,
               "Content world-volume bridge should preserve authored occlusion-portal open state");
        ExpectVec3(volume.size, {6.0f, 3.0f, 0.001f},
                   "Content world-volume bridge should clamp occlusion-portal extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["scale"] = MakeVec3(0.0, 6.0, 8.0);
        data["probeIntensity"] = 2.4;
        data["blendRadius"] = 99.0;
        data["probeResolution"] = 4096.0;
        data["parallaxCorrection"] = false;
        data["dynamic"] = true;
        const ri::world::ReflectionProbeVolume volume = ri::content::BuildReflectionProbeVolume(data);
        Expect(volume.type == "reflection_probe_volume"
                   && NearlyEqual(volume.intensity, 2.4f)
                   && NearlyEqual(volume.blendDistance, 64.0f)
                   && volume.captureResolution == 2048U
                   && !volume.boxProjection
                   && volume.dynamicCapture,
               "Content world-volume bridge should classify reflection probe volumes");
        ExpectVec3(volume.size, {0.001f, 6.0f, 8.0f},
                   "Content world-volume bridge should clamp reflection probe extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["type"] = "probe_grid_bounds";
        data["size"] = MakeVec3(8.0, 0.0, 8.0);
        const ri::world::LightImportanceVolume volume = ri::content::BuildLightImportanceVolume(data);
        Expect(volume.type == "probe_grid_bounds" && volume.probeGridBounds,
               "Content world-volume bridge should preserve probe-grid bounds classification");
        ExpectVec3(volume.size, {8.0f, 0.001f, 8.0f},
                   "Content world-volume bridge should clamp light-importance extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["type"] = "probe_grid";
        const ri::world::LightImportanceVolume volume = ri::content::BuildLightImportanceVolume(data);
        Expect(volume.type == "probe_grid_bounds" && volume.probeGridBounds,
               "Content world-volume bridge should accept probe_grid alias for probe-grid bounds volumes");
    }

    {
        Value::Object data{};
        data["portalTransmission"] = 3.5;
        data["edgeSoftness"] = -4.0;
        data["priority"] = 130.0;
        data["doubleSided"] = true;
        const ri::world::LightPortalVolume volume = ri::content::BuildLightPortalVolume(data);
        Expect(volume.type == "light_portal"
                   && NearlyEqual(volume.transmission, 3.5f)
                   && NearlyEqual(volume.softness, 0.0f)
                   && NearlyEqual(volume.priority, 100.0f)
                   && volume.twoSided,
               "Content world-volume bridge should classify light portal volumes");
        ExpectVec3(volume.size, {5.0f, 5.0f, 1.0f},
                   "Content world-volume bridge should preserve light-portal fallback size");
    }

    {
        Value::Object data{};
        data["voxel_size"] = 0.01;
        data["cascades"] = 99.0;
        data["dynamic"] = false;
        data["size"] = MakeVec3(0.0, 4.0, 8.0);
        const ri::world::VoxelGiBoundsVolume volume = ri::content::BuildVoxelGiBoundsVolume(data);
        Expect(volume.type == "voxel_gi_bounds"
                   && NearlyEqual(volume.voxelSize, 0.05f)
                   && volume.cascadeCount == 8U
                   && !volume.updateDynamics,
               "Content world-volume bridge should classify voxel-gi bounds and clamp authored runtime settings");
        ExpectVec3(volume.size, {0.001f, 4.0f, 8.0f},
                   "Content world-volume bridge should clamp voxel-gi bounds extents to a non-zero minimum");
    }

    {
        Value::Object data{};
        data["minDensity"] = 2048.0;
        data["maxDensity"] = 128.0;
        data["density"] = 6000.0;
        data["surfaceAreaClamp"] = false;
        const ri::world::LightmapDensityVolume volume = ri::content::BuildLightmapDensityVolume(data);
        Expect(volume.type == "lightmap_density_volume"
                   && NearlyEqual(volume.minimumTexelsPerMeter, 128.0f)
                   && NearlyEqual(volume.maximumTexelsPerMeter, 2048.0f)
                   && NearlyEqual(volume.texelsPerMeter, 2048.0f)
                   && !volume.clampBySurfaceArea,
               "Content world-volume bridge should classify lightmap-density volumes and normalize density ranges");
    }

    {
        Value::Object data{};
        data["excludeStatic"] = false;
        data["excludeDynamic"] = true;
        data["excludeVolumetric"] = true;
        data["fade"] = -12.0;
        const ri::world::ShadowExclusionVolume volume = ri::content::BuildShadowExclusionVolume(data);
        Expect(volume.type == "shadow_exclusion_volume"
                   && !volume.excludeStaticShadows
                   && volume.excludeDynamicShadows
                   && volume.affectVolumetricShadows
                   && NearlyEqual(volume.fadeDistance, 0.0f),
               "Content world-volume bridge should classify shadow-exclusion volumes with sanitized fade controls");
    }

    {
        Value::Object data{};
        data["minDistance"] = 220.0;
        data["maxDistance"] = 20.0;
        data["static"] = true;
        data["dynamic"] = false;
        data["allowHLOD"] = false;
        const ri::world::CullingDistanceVolume volume = ri::content::BuildCullingDistanceVolume(data);
        Expect(volume.type == "culling_distance_volume"
                   && NearlyEqual(volume.nearDistance, 20.0f)
                   && NearlyEqual(volume.farDistance, 220.0f)
                   && volume.applyToStaticObjects
                   && !volume.applyToDynamicObjects
                   && !volume.allowHlod,
               "Content world-volume bridge should classify culling-distance volumes and sort authored distance ranges");
    }

    {
        Value::Object data{};
        data["textureId"] = "caution_stripes_refined.png";
        data["imageUrl"] = "https://example.invalid/ref.png";
        data["color"] = "#80ffaa";
        data["opacity"] = 2.0;
        data["renderOrder"] = 9999.0;
        data["alwaysFaceCamera"] = true;
        const ri::world::ReferenceImagePlane plane = ri::content::BuildReferenceImagePlane(data);
        Expect(plane.type == "reference_image_plane"
                   && plane.textureId == "caution_stripes_refined.png"
                   && plane.imageUrl == "https://example.invalid/ref.png"
                   && NearlyEqual(plane.opacity, 1.0f)
                   && plane.renderOrder == 200
                   && plane.alwaysFaceCamera,
               "Content world-volume bridge should classify reference image planes and clamp render settings");
        ExpectVec3(plane.tintColor, {0.502f, 1.0f, 0.667f},
                   "Content world-volume bridge should parse authored reference-plane tint color");
    }

    {
        Value::Object data{};
        data["text"] = "GO FAST";
        data["fontFamily"] = "display";
        data["textColor"] = "#7affd1";
        data["outlineColor"] = "#1a1a1a";
        data["textScale"] = 0.001;
        data["depth"] = 10.0;
        data["extrusionBevel"] = 10.0;
        data["letterSpacing"] = -8.0;
        data["alwaysFaceCamera"] = true;
        data["doubleSided"] = false;
        const ri::world::Text3dPrimitive text3d = ri::content::BuildText3dPrimitive(data);
        Expect(text3d.type == "text_3d_primitive"
                   && text3d.text == "GO FAST"
                   && text3d.fontFamily == "display"
                   && text3d.textColor == "#7affd1"
                   && text3d.outlineColor == "#1a1a1a"
                   && text3d.alwaysFaceCamera
                   && !text3d.doubleSided,
               "Content world-volume bridge should classify 3D text primitives and preserve key authored labels");
        Expect(NearlyEqual(text3d.textScale, 0.05f)
                   && NearlyEqual(text3d.depth, 4.0f)
                   && NearlyEqual(text3d.extrusionBevel, 1.0f)
                   && NearlyEqual(text3d.letterSpacing, -2.0f),
               "Content world-volume bridge should clamp 3D text primitive extrusion and spacing ranges");
    }

    {
        Value::Object data{};
        data["comment"] = "ALIGN THIS HALLWAY";
        data["textScale"] = 0.05;
        data["fontSize"] = 900.0;
        const ri::world::AnnotationCommentPrimitive annotation = ri::content::BuildAnnotationCommentPrimitive(data);
        Expect(annotation.type == "annotation_comment_primitive"
                   && annotation.text == "ALIGN THIS HALLWAY"
                   && annotation.accentColor == "#ffd36a"
                   && annotation.backgroundColor == "rgba(26, 22, 16, 0.88)",
               "Content world-volume bridge should classify annotation comments and preserve styled defaults");
        Expect(NearlyEqual(annotation.textScale, 0.2f) && NearlyEqual(annotation.fontSize, 128.0f),
               "Content world-volume bridge should clamp authored annotation comment scale and font size");
    }

    {
        Value::Object data{};
        data["mode"] = "line";
        data["start"] = MakeVec3(0.0, 0.0, 0.0);
        data["end"] = MakeVec3(3.0, 4.0, 0.0);
        data["units"] = "m";
        data["textScale"] = 100.0;
        data["fontSize"] = 2.0;
        data["showWireframe"] = false;
        data["showFill"] = false;
        const ri::world::MeasureToolPrimitive measure = ri::content::BuildMeasureToolPrimitive(data);
        Expect(measure.type == "measure_tool_primitive"
                   && measure.mode == ri::world::MeasureToolMode::Line
                   && measure.unitSuffix == "m"
                   && NearlyEqual(measure.position.x, 1.5f)
                   && NearlyEqual(measure.position.y, 2.0f)
                   && NearlyEqual(measure.textScale, 24.0f)
                   && NearlyEqual(measure.fontSize, 14.0f),
               "Content world-volume bridge should classify line-based measure tools and sanitize rendering values");
        Expect(measure.showWireframe && !measure.showFill,
               "Content world-volume bridge should ensure measure tools always keep at least one helper visual enabled");
    }

    {
        Value::Object data{};
        data["cameraPosition"] = MakeVec3(2.0, 3.0, 4.0);
        data["cameraLookAt"] = MakeVec3(0.0, 2.0, -2.0);
        data["renderResolution"] = 1500.0;
        data["resolutionCap"] = 180.0;
        data["updateEveryFrames"] = 6.0;
        data["maxActiveDistance"] = 0.1;
        data["editorOnly"] = true;
        const ri::world::RenderTargetSurface surface = ri::content::BuildRenderTargetSurface(data);
        Expect(surface.type == "render_target_surface"
                   && NearlyEqual(surface.cameraPosition.x, 2.0f)
                   && NearlyEqual(surface.cameraLookAt.z, -2.0f)
                   && surface.renderResolution == 1024
                   && surface.resolutionCap == 180
                   && surface.updateEveryFrames == 6U
                   && NearlyEqual(surface.maxActiveDistance, 1.0f)
                   && surface.editorOnly,
               "Content world-volume bridge should parse and clamp render-target surface update controls");
    }

    {
        Value::Object data{};
        data["normal"] = MakeVec3(0.0, 0.0, 5.0);
        data["strength"] = -4.0;
        data["targetResolution"] = 32.0;
        data["maxResolution"] = 4096.0;
        data["updateFrequency"] = 90.0;
        data["enableDistanceGate"] = false;
        const ri::world::PlanarReflectionSurface surface = ri::content::BuildPlanarReflectionSurface(data);
        Expect(surface.type == "planar_reflection_surface"
                   && NearlyEqual(surface.planeNormal.z, 1.0f)
                   && NearlyEqual(surface.reflectionStrength, 0.0f)
                   && surface.renderResolution == 64
                   && surface.resolutionCap == 2048
                   && surface.updateEveryFrames == 90U
                   && !surface.enableDistanceGate,
               "Content world-volume bridge should classify planar-reflection surfaces and sanitize quality controls");
    }

    {
        Value::Object material{};
        material["opacity"] = 1.7;
        material["blendMode"] = "additive";
        material["depthWrite"] = true;
        material["baseColor"] = "#66cfff";

        Value::Object visuals{};
        visuals["pulseEnabled"] = true;
        visuals["pulseSpeed"] = 2.2;
        visuals["pulseMinOpacity"] = 0.8;
        visuals["pulseMaxOpacity"] = 0.3;
        visuals["distanceFadeEnabled"] = true;
        visuals["fadeNear"] = 9.0;
        visuals["fadeFar"] = 3.0;

        Value::Object interaction{};
        interaction["blocksPlayer"] = true;
        interaction["blocksNPC"] = false;
        interaction["blocksProjectiles"] = true;
        interaction["affectsNavigation"] = true;
        interaction["raycastSelectable"] = false;

        Value::Object events{};
        events["onUse"] = "inspect_hologram";

        Value::Object debug{};
        debug["label"] = "SOFT BOUNDARY";
        debug["showBounds"] = true;

        Value::Object data{};
        data["shape"] = "custom_mesh";
        data["material"] = material;
        data["visualBehavior"] = visuals;
        data["interactionProfile"] = interaction;
        data["events"] = events;
        data["debug"] = debug;
        const ri::world::PassThroughPrimitive primitive = ri::content::BuildPassThroughPrimitive(data);
        Expect(primitive.type == "pass_through_primitive"
                   && primitive.primitiveShape == ri::world::PassThroughPrimitiveShape::Box
                   && NearlyEqual(primitive.material.opacity, 1.0f)
                   && primitive.material.blendMode == ri::world::PassThroughBlendMode::Additive
                   && primitive.material.depthWrite
                   && NearlyEqual(primitive.visualBehavior.pulseMinOpacity, 0.3f)
                   && NearlyEqual(primitive.visualBehavior.pulseMaxOpacity, 0.8f)
                   && NearlyEqual(primitive.visualBehavior.fadeNear, 9.0f)
                   && NearlyEqual(primitive.visualBehavior.fadeFar, 9.0f)
                   && primitive.interactionProfile.blocksPlayer
                   && primitive.interactionProfile.blocksProjectiles
                   && primitive.interactionProfile.affectsNavigation
                   && !primitive.interactionProfile.raycastSelectable
                   && primitive.events.onUse == "inspect_hologram"
                   && primitive.debug.label == "SOFT BOUNDARY"
                   && primitive.debug.showBounds
                   && !primitive.passThrough,
               "Content world-volume bridge should parse pass-through primitives with nested render and interaction schema");
    }

    {
        Value::Object visual{};
        visual["mode"] = "texture";
        visual["opacity"] = -2.0;
        visual["textureId"] = "horizon_tex_a";

        Value::Object behavior{};
        behavior["followCameraYaw"] = true;
        behavior["parallaxFactor"] = 1.9;
        behavior["depthWrite"] = false;
        behavior["renderLayer"] = "foreground";

        Value::Object data{};
        data["primitiveType"] = "plane";
        data["visual"] = visual;
        data["behavior"] = behavior;
        data["scale"] = MakeVec3(18.0, 9.0, 1.0);
        const ri::world::SkyProjectionSurface surface = ri::content::BuildSkyProjectionSurface(data);
        Expect(surface.type == "sky_projection_surface"
                   && surface.primitiveType == "plane"
                   && surface.visual.mode == ri::world::SkyProjectionVisualMode::Texture
                   && surface.visual.textureId == "horizon_tex_a"
                   && NearlyEqual(surface.visual.opacity, 0.0f)
                   && surface.visual.doubleSided
                   && surface.visual.unlit
                   && surface.behavior.followCameraYaw
                   && NearlyEqual(surface.behavior.parallaxFactor, 1.0f)
                   && !surface.behavior.depthWrite
                   && surface.behavior.renderLayer == ri::world::SkyProjectionRenderLayer::Foreground
                   && surface.skyProjectionSurface,
               "Content world-volume bridge should parse sky-projection surface visual and behavior settings");
    }

    {
        Value::Object emission{};
        emission["particleCount"] = 9000.0;
        emission["spawnMode"] = "surface";
        emission["lifetimeSeconds"] = Value::Array{8.0, 1.0};
        emission["spawnRatePerSecond"] = -4.0;
        emission["loop"] = false;

        Value::Object particle{};
        particle["size"] = -5.0;
        particle["sizeJitter"] = 8.0;
        particle["opacity"] = 3.0;
        particle["velocity"] = MakeVec3(0.0, 0.08, 0.0);

        Value::Object render{};
        render["blendMode"] = "additive";
        render["depthWrite"] = true;
        render["billboard"] = false;

        Value::Object culling{};
        culling["maxActiveDistance"] = -10.0;
        culling["frustumCulling"] = false;
        culling["pauseWhenOffscreen"] = false;

        Value::Object debug{};
        debug["showBounds"] = true;
        debug["showSpawnPoints"] = true;
        debug["label"] = "Dust Room";

        Value::Object data{};
        data["shape"] = "cylinder";
        data["emission"] = emission;
        data["particle"] = particle;
        data["render"] = render;
        data["culling"] = culling;
        data["debug"] = debug;
        const ri::world::VolumetricEmitterBounds volume = ri::content::BuildVolumetricEmitterBounds(data);
        Expect(volume.type == "volumetric_emitter_bounds"
                   && volume.shape == ri::world::VolumeShape::Cylinder
                   && volume.emission.particleCount == 2048U
                   && volume.emission.spawnMode == ri::world::VolumetricEmitterSpawnMode::Surface
                   && NearlyEqual(volume.emission.lifetimeMinSeconds, 1.0f)
                   && NearlyEqual(volume.emission.lifetimeMaxSeconds, 8.0f)
                   && NearlyEqual(volume.emission.spawnRatePerSecond, 0.0f)
                   && !volume.emission.loop
                   && NearlyEqual(volume.particle.size, 0.001f)
                   && NearlyEqual(volume.particle.sizeJitter, 5.0f)
                   && NearlyEqual(volume.particle.opacity, 1.0f)
                   && volume.render.blendMode == ri::world::VolumetricEmitterBlendMode::Additive
                   && volume.render.depthWrite
                   && !volume.render.billboard
                   && NearlyEqual(volume.culling.maxActiveDistance, 1.0f)
                   && !volume.culling.frustumCulling
                   && !volume.culling.pauseWhenOffscreen
                   && volume.debug.showBounds
                   && volume.debug.showSpawnPoints
                   && volume.debug.label == "Dust Room",
               "Content world-volume bridge should parse and sanitize volumetric emitter bounds schema");
    }

    {
        Value::Object activation{};
        activation["outerProximityRadius"] = 25.0;
        activation["strictInnerVolumeOnly"] = true;
        activation["alwaysOnAmbient"] = false;

        Value::Object emissionPolicy{};
        emissionPolicy["burstCountOnEnter"] = 32.0;
        emissionPolicy["oneShot"] = true;

        Value::Object budget{};
        budget["maxOnScreenCostHint"] = 500.0;
        budget["disableAtOrBelowQualityTier"] = 1.0;

        Value::Object binding{};
        binding["followNodeId"] = "vent_a";
        binding["followSocketName"] = "steam_out";

        Value::Object environment{};
        environment["applyGlobalWind"] = true;
        environment["localWindFieldVolumeId"] = "wind_tunnel_01";
        environment["reduceWhenOccluded"] = true;
        environment["reduceWhenIndoor"] = true;

        Value::Object particleSpawn{};
        particleSpawn["displayName"] = "Steam vent";
        particleSpawn["presetId"] = "fx/steam_soft";
        particleSpawn["meshAssetId"] = "quad_billboard";
        particleSpawn["materialAssetId"] = "mat/steam_add";
        particleSpawn["worldCollision"] = true;
        particleSpawn["activation"] = activation;
        particleSpawn["emissionPolicy"] = emissionPolicy;
        particleSpawn["budget"] = budget;
        particleSpawn["binding"] = binding;
        particleSpawn["environment"] = environment;

        Value::Object emission{};
        emission["particleCount"] = 128.0;
        emission["loop"] = true;

        Value::Object culling{};
        culling["maxActiveDistance"] = 40.0;

        Value::Object data{};
        data["particleSpawn"] = particleSpawn;
        data["emission"] = emission;
        data["culling"] = culling;

        const ri::world::VolumetricEmitterBounds psv = ri::content::BuildParticleSpawnVolume(data);
        Expect(psv.type == "particle_spawn_volume" && psv.particleSpawn.has_value(),
               "Particle spawn volumes should carry gameplay authoring metadata");
        const ri::world::ParticleSpawnAuthoring& ps = *psv.particleSpawn;
        Expect(ps.displayName == "Steam vent" && ps.particleSystemPresetId == "fx/steam_soft"
                   && ps.meshAssetId == "quad_billboard" && ps.materialAssetId == "mat/steam_add" && ps.worldCollision
                   && NearlyEqual(ps.activation.outerProximityRadius, 25.0f) && ps.activation.strictInnerVolumeOnly
                   && !ps.activation.alwaysOnAmbient && ps.emissionPolicy.burstCountOnEnter == 32U
                   && ps.emissionPolicy.oneShot && ps.budget.maxOnScreenCostHint == 500U
                   && ps.budget.disableAtOrBelowQualityTier == 1U && ps.binding.followNodeId == "vent_a"
                   && ps.binding.followSocketName == "steam_out" && ps.environment.applyGlobalWind
                   && ps.environment.localWindFieldVolumeId == "wind_tunnel_01" && ps.environment.reduceWhenOccluded
                   && ps.environment.reduceWhenIndoor,
               "Particle spawn authoring should round-trip content fields");
    }

    {
        Value::Object data{};
        data["tintStrength"] = 3.0;
        data["blurAmount"] = 0.5;
        const ri::world::LocalizedFogVolume volume = ri::content::BuildLocalizedFogVolume(data);
        Expect(volume.type == "localized_fog_volume",
               "Content world-volume bridge should classify localized fog volumes");
        Expect(NearlyEqual(volume.tintStrength, 1.0f),
               "Content world-volume bridge should clamp localized-fog tint strength");
        Expect(NearlyEqual(volume.blurAmount, 0.02f),
               "Content world-volume bridge should clamp localized-fog blur amount");
        ExpectVec3(volume.size, {6.0f, 4.0f, 6.0f},
                   "Content world-volume bridge should preserve localized-fog fallback size");
    }

    {
        Value::Object data{};
        const ri::world::FogBlockerVolume volume = ri::content::BuildVolumetricFogBlocker(data);
        Expect(volume.type == "volumetric_fog_blocker",
               "Content world-volume bridge should classify volumetric fog blockers");
        ExpectVec3(volume.size, {4.0f, 4.0f, 4.0f},
                   "Content world-volume bridge should preserve fog-blocker fallback size");
    }
}

// --- merged from TestDataSchema.cpp ---
namespace detail_DataSchema {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool NearlyEqualD(double lhs, double rhs) {
    return std::fabs(lhs - rhs) < 1e-9;
}

}

 // namespace

void TestDataSchema() {
    using namespace detail_DataSchema;
    ri::validate::ValidationReport a{};
    Expect(a.Ok(), "Empty validation report should be ok");
    a.Add(ri::validate::IssueCode::MissingField, "/root/name", "required");
    Expect(!a.Ok() && a.issues.size() == 1U && a.issues[0].path == "/root/name"
               && a.issues[0].code == ri::validate::IssueCode::MissingField,
           "Validation report should record issues");

    Expect(std::string(ri::validate::IssueCodeLabel(ri::validate::IssueCode::MissingField)) == "missing_field",
           "Issue codes should have stable string labels");
    Expect(std::string(ri::validate::IssueCodeLabel(ri::validate::IssueCode::NoUnionMatch)) == "no_union_match",
           "Union failure codes should have stable string labels");

    ri::validate::ValidationReport b{};
    b.AddWithContext(ri::validate::IssueCode::TypeMismatch,
                     "/x",
                     "bad",
                     "expected",
                     "int");
    a.Merge(b);
    Expect(a.issues.size() == 2U, "Merged validation report should combine issues");

    ri::validate::ValidationReport moved{};
    moved.Add(ri::validate::IssueCode::UnknownKey, "/z", "z");
    a.Merge(std::move(moved));
    Expect(a.issues.size() == 3U, "Merge of rvalue should append issues");

    Expect(a.SummaryLine().find("missing_field@/root/name") != std::string::npos,
           "Validation summary line should include code and path");

    ri::validate::ValidationReport child{};
    child.Add(ri::validate::IssueCode::ConstraintViolation, "field", "bad");
    ri::validate::ValidationReport parent{};
    parent.AbsorbPrefixed("/doc", child);
    Expect(parent.issues.size() == 1U && parent.issues[0].path == "/doc/field",
           "Prefixed absorb should join paths");

    const ri::validate::ValidationReport batch =
        ri::validate::MergeReports({&a, &parent});
    Expect(batch.issues.size() == a.issues.size() + 1U, "Batch merge should concatenate reports");

    ri::data::schema::SchemaRegistry reg{};
    reg.Register("level", 1, 2);
    Expect(reg.Contains("level", 1, 2) && !reg.Contains("level", 1, 3),
           "Schema registry should track registered ids");
    reg.Register("level", 1, 2);
    reg.Register(ri::data::schema::SchemaId{.kind = "save", .versionMajor = 0, .versionMinor = 9});
    Expect(reg.Contains({.kind = "save", .versionMajor = 0, .versionMinor = 9}),
           "Schema registry should accept struct ids");
    reg.RegisterTagged("net", 1, 0, "NetMessageV1");
    const std::optional<std::string_view> tag = reg.TagFor({.kind = "net", .versionMajor = 1, .versionMinor = 0});
    Expect(tag.has_value() && tag.value() == "NetMessageV1", "Tagged schemas should expose dispatch tags");
    reg.Clear();
    Expect(!reg.Contains("level", 1, 2), "Cleared schema registry should be empty");

    ri::data::schema::ObjectShape shape{};
    shape.requiredKeys = {"id"};
    shape.optionalKeys = {"label"};
    shape.unknownPolicy = ri::data::schema::UnknownKeyPolicy::Forbid;
    const ri::validate::ValidationReport shapeOk =
        ri::data::schema::ValidateObjectShape(shape, std::unordered_set<std::string>{"id", "label"});
    Expect(shapeOk.Ok(), "Object shape should accept required and optional keys only");
    const ri::validate::ValidationReport shapeBad =
        ri::data::schema::ValidateObjectShape(shape, std::unordered_set<std::string>{"id", "extra"});
    Expect(!shapeBad.Ok() && shapeBad.issues[0].code == ri::validate::IssueCode::UnknownKey,
           "Object shape should reject unknown keys when policy forbids extras");
    const ri::validate::ValidationReport shapeMissing =
        ri::data::schema::ValidateObjectShape(shape, std::unordered_set<std::string>{"label"});
    Expect(!shapeMissing.Ok() && shapeMissing.issues[0].code == ri::validate::IssueCode::MissingField,
           "Object shape should require listed keys");

    ri::data::schema::ObjectShape allowExtraShape = shape;
    allowExtraShape.unknownPolicy = ri::data::schema::UnknownKeyPolicy::AllowExtra;
    Expect(ri::data::schema::ValidateObjectShape(allowExtraShape, std::unordered_set<std::string>{"id", "surprise"})
               .Ok(),
           "AllowExtra should ignore unknown top-level keys");

    ri::data::schema::ObjectShape stripShape = shape;
    stripShape.unknownPolicy = ri::data::schema::UnknownKeyPolicy::Strip;
    Expect(ri::data::schema::ValidateObjectShape(stripShape, std::unordered_set<std::string>{"id", "noise"}).Ok(),
           "Strip policy should tolerate unknown keys during validation (normalize separately)");

    Expect(ri::validate::ValidateEachObjectKeyMatchesPattern("/props", {"aa", "bb"}, "^[a-z]+$").Ok(),
           "Object key pattern validation should accept matching names");
    Expect(!ri::validate::ValidateEachObjectKeyMatchesPattern("/props", {"ok", "NO"}, "^[a-z]+$").Ok(),
           "Object key pattern validation should reject non-matching names");

    ri::data::schema::ObjectShape bagShape{};
    bagShape.requiredKeys = {"id"};
    bagShape.unknownPolicy = ri::data::schema::UnknownKeyPolicy::PassthroughBag;
    bagShape.extensionsBagKey = "extensions";
    Expect(ri::data::schema::ValidateObjectShape(bagShape, std::unordered_set<std::string>{"id", "extensions"}).Ok(),
           "PassthroughBag should allow the configured extensions bag key");
    Expect(!ri::data::schema::ValidateObjectShape(bagShape, std::unordered_set<std::string>{"id", "loot"}).Ok(),
           "PassthroughBag should still reject arbitrary unknown keys");

    Expect(ri::validate::ValidateDistinctStrings("/tags", {"a", "b", "c"}).Ok(),
           "Distinct string check should accept unique values");
    Expect(!ri::validate::ValidateDistinctStrings("/tags", {"a", "b", "a"}).Ok(),
           "Distinct string check should flag duplicates");

    Expect(ri::validate::ValidateRegexMatch("abc123", "^[a-z]+[0-9]+$", "/code").Ok(),
           "Regex match should accept ECMA patterns");
    Expect(!ri::validate::ValidateRegexMatch("no-digits", "^[a-z]+$", "/code").Ok(),
           "Regex match should reject non-matching text");

    const ri::validate::SafeParseResult<std::array<std::uint8_t, 4U>> col =
        ri::validate::TryParseColorRgba8("#f00");
    Expect(col.value.has_value() && (*col.value)[0] == 255U && (*col.value)[3] == 255U && col.report.Ok(),
           "Color parse should expand #rgb and default alpha");
    const ri::validate::SafeParseResult<std::array<std::uint8_t, 4U>> col8 =
        ri::validate::TryParseColorRgba8("#11223344");
    Expect(col8.value.has_value() && (*col8.value)[0] == 17U && (*col8.value)[3] == 68U && col8.report.Ok(),
           "Color parse should read #rrggbbaa");

    const ri::validate::SafeParseResult<double> okNum = ri::validate::TryCoerceDouble("12.25");
    Expect(okNum.value.has_value() && NearlyEqualD(*okNum.value, 12.25) && okNum.report.Ok(),
           "Coercion should parse decimal strings");
    const ri::validate::SafeParseResult<double> badNum = ri::validate::TryCoerceDouble("nope");
    Expect(!badNum.value.has_value() && !badNum.report.Ok(), "Coercion should reject non-numeric strings");

    const ri::validate::SafeParseResult<std::int32_t> okInt = ri::validate::TryCoerceInt32("-40");
    Expect(okInt.value.has_value() && *okInt.value == -40 && okInt.report.Ok(),
           "Coercion should parse base-10 integers");

    const ri::validate::ValidationReport fin =
        ri::validate::RequireFiniteDouble(std::numeric_limits<double>::infinity(), "/v");
    Expect(!fin.Ok(), "Primitive check should reject non-finite doubles");

    const std::vector<std::string> refs = {"a", "missing"};
    const std::unordered_set<std::string> table = {"a", "b"};
    const ri::validate::ValidationReport refRep = ri::validate::ValidateIdsInTable("/refs", refs, table);
    Expect(!refRep.Ok() && refRep.issues.size() == 1U
               && refRep.issues[0].code == ri::validate::IssueCode::InvalidReference,
           "Reference integrity should flag ids outside the document table");

    ri::data::schema::MigrationRegistry migrations{};
    const ri::data::schema::SchemaId v1{.kind = "doc", .versionMajor = 1, .versionMinor = 0};
    const ri::data::schema::SchemaId v2{.kind = "doc", .versionMajor = 2, .versionMinor = 0};
    const ri::data::schema::SchemaId v3{.kind = "doc", .versionMajor = 3, .versionMinor = 0};
    migrations.AddEdge({.from = v1, .to = v2});
    migrations.AddEdge({.from = v2, .to = v3});
    Expect(migrations.CanReach(v1, v3) && !migrations.CanReach(v3, v1),
           "Migration registry should answer reachability along directed edges");
    const std::optional<std::vector<ri::data::schema::SchemaId>> chain = migrations.ShortestPath(v1, v3);
    Expect(chain.has_value() && chain->size() == 3U && (*chain)[0] == v1 && (*chain)[2] == v3,
           "Migration registry should return shortest version chain");

    const ri::validate::SafeParseResult<ri::data::schema::DocumentHeader> headerOk =
        ri::data::schema::ParseDocumentHeader("level_chunk", "2.1");
    Expect(headerOk.value.has_value() && headerOk.value->kind == "level_chunk"
               && headerOk.value->schemaMajor == 2U && headerOk.value->schemaMinor == 1U,
           "Document header parse should split kind and dotted version");
    const ri::validate::SafeParseResult<ri::data::schema::DocumentHeader> headerBad =
        ri::data::schema::ParseDocumentHeader("x", "not-a-version");
    Expect(!headerBad.value.has_value() && !headerBad.report.Ok(),
           "Document header parse should reject non-numeric version tokens");
    const ri::validate::SafeParseResult<ri::data::schema::DocumentHeader> headerBadMajorSuffix =
        ri::data::schema::ParseDocumentHeader("x", "2abc");
    Expect(!headerBadMajorSuffix.value.has_value() && !headerBadMajorSuffix.report.Ok(),
           "Document header parse should reject major version suffix garbage");
    const ri::validate::SafeParseResult<ri::data::schema::DocumentHeader> headerBadMinorSuffix =
        ri::data::schema::ParseDocumentHeader("x", "2.1extra");
    Expect(!headerBadMinorSuffix.value.has_value() && !headerBadMinorSuffix.report.Ok(),
           "Document header parse should reject minor version suffix garbage");

    bool threw = false;
    try {
        ri::validate::UnwrapOrThrow(headerBad, "strict");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Expect(threw, "Strict unwrap should throw when safe-parse has no value");

    ri::validate::ValidationReport jsonReport{};
    jsonReport.Add(ri::validate::IssueCode::MissingField, "/a/b", "msg \"quoted\"");
    const std::string json = jsonReport.IssuesToJsonArray();
    Expect(json.find(R"("code":"missing_field")") != std::string::npos
               && json.find("\\\"") != std::string::npos,
           "JSON issue export should escape strings and emit stable keys");

    ri::validate::ValidationReport typed{};
    typed.AddTypeMismatch("/slot", "object", "array", "wrong shape");
    const std::string typedJson = typed.IssuesToJsonArray();
    Expect(typedJson.find(R"("expectedType":"object")") != std::string::npos
               && typedJson.find(R"("actualType":"array")") != std::string::npos,
           "JSON issue export should include optional type hints");

    Expect(ri::validate::ValidateAsciiIdentifier("_id", "/id").Ok(),
           "ASCII identifier validation should accept underscore-leading names");
    Expect(!ri::validate::ValidateAsciiIdentifier("9bad", "/id").Ok(),
           "ASCII identifier validation should reject leading digits");

    Expect(ri::validate::ValidateIso8601UtcTimestampString("2020-01-15T08:30:00Z", "/ts").Ok(),
           "ISO-8601 UTC timestamp validation should accept basic Zulu form");
    Expect(ri::validate::ValidateIso8601UtcTimestampString("2020-01-15T08:30:00.5Z", "/ts").Ok(),
           "ISO-8601 UTC timestamp validation should accept fractional seconds");
    Expect(!ri::validate::ValidateIso8601UtcTimestampString("2020-01-15", "/ts").Ok(),
           "ISO-8601 UTC timestamp validation should reject date-only strings");

    Expect(ri::validate::NormalizePathSeparators("foo\\bar/baz") == "foo/bar/baz",
           "Path normalization should canonicalize separators");

    const ri::validate::SafeParseResult<bool> boolOk = ri::validate::TryCoerceBool("YES");
    Expect(boolOk.value.has_value() && *boolOk.value && boolOk.report.Ok(),
           "Boolean coercion should accept common truthy spellings");

    Expect(ri::validate::ValidateAllowedString("read", {"read", "write"}, "/perm").Ok(),
           "Allowlist validation should accept listed strings");
    Expect(!ri::validate::ValidateAllowedString("delete", {"read", "write"}, "/perm").Ok(),
           "Allowlist validation should reject values outside the set");

    Expect(ri::validate::ValidateCollectionSize(4U, 2U, 8U, "/items").Ok(),
           "Collection size validation should accept in-range counts");
    Expect(!ri::validate::ValidateCollectionSize(0U, 1U, 3U, "/items").Ok(),
           "Collection size validation should reject underflow");

    Expect(ri::validate::ValidateStringLength("abcd", 2U, 6U, "/name").Ok(),
           "String length validation should accept in-range text");
    Expect(!ri::validate::ValidateStringLength("x", 2U, 6U, "/name").Ok(),
           "String length validation should reject too-short strings");

    Expect(ri::validate::ValidateDoubleInRange(0.5, 0.0, 1.0, "/t").Ok(),
           "Scalar range validation should accept in-range doubles");
    Expect(!ri::validate::ValidateDoubleInRange(2.0, 0.0, 1.0, "/t").Ok(),
           "Scalar range validation should reject out-of-range doubles");

    ri::validate::ValidationReport refined{};
    ri::validate::RunRefinements(
        {[](ri::validate::ValidationReport& r) {
            r.Add(ri::validate::IssueCode::ConstraintViolation, "/weights", "sum must be 1");
        }},
        refined);
    Expect(!refined.Ok() && refined.issues[0].path == "/weights",
           "Refinement hooks should append cross-field issues");

    shape.fieldDocs.push_back(ri::data::schema::ObjectFieldDoc{
        .key = "id",
        .required = true,
        .optionalSinceMajor = 0,
        .optionalSinceMinor = 0,
        .description = "stable entity id",
    });
    Expect(shape.fieldDocs.size() == 1U, "Object shape should carry optional field introspection metadata");

    RawIron::DataSchema::ValidationReport facadeReport{};
    facadeReport.Add(RawIron::DataSchema::IssueCode::ConstraintViolation, "/p", "c");
    Expect(!facadeReport.Ok(), "Facade type aliases should match ri::validate types");
}

// --- merged from TestDialogueCueState.cpp ---
namespace detail_DialogueCueState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestDialogueCueState() {
    using namespace detail_DialogueCueState;
    ri::world::DialogueCueState state({
        .mode = ri::world::DialogueCueMode::Verbose,
        .allowObjectiveUpdates = true,
        .allowGuidanceHints = true,
        .allowRepeatHints = true,
        .historyLimit = 3U,
    });

    state.Present({
        .sourceId = "npc_vale",
        .speakerLabel = "Dr. Vale",
        .dialogueText = "DR. VALE: KEEP YOUR VOICE DOWN.",
        .repeatHint = "Nothing new.",
        .objectiveText = "Inspect the isolation ward.",
        .guidanceHint = "Check the red-marked corridor.",
        .speakOnce = true,
        .dialogueDurationMs = 4500.0,
        .repeatDurationMs = 2500.0,
    });

    Expect(state.ActiveDialogue().has_value() && state.ActiveDialogue()->text == "DR. VALE: KEEP YOUR VOICE DOWN.",
           "Dialogue cue state should preserve verbose authored dialogue");
    Expect(state.ActiveGuidanceHint().has_value() && state.ActiveGuidanceHint()->text == "Check the red-marked corridor.",
           "Dialogue cue state should surface guidance hints when enabled");
    const std::optional<std::string> objective = state.ConsumePendingObjective();
    Expect(objective.has_value() && *objective == "Inspect the isolation ward.",
           "Dialogue cue state should expose pending objective updates");

    state.Present({
        .sourceId = "npc_vale",
        .speakerLabel = "Dr. Vale",
        .dialogueText = "SHOULD NOT REPEAT",
        .repeatHint = "Nothing new.",
        .objectiveText = "",
        .guidanceHint = "",
        .speakOnce = true,
        .dialogueDurationMs = 4500.0,
        .repeatDurationMs = 2500.0,
    });
    Expect(state.ActiveDialogue().has_value() && state.ActiveDialogue()->text == "Nothing new.",
           "Dialogue cue state should honor repeat-hint behavior for speak-once interactions");

    state.SetPolicy({
        .mode = ri::world::DialogueCueMode::Disabled,
        .allowObjectiveUpdates = true,
        .allowGuidanceHints = true,
        .allowRepeatHints = true,
        .historyLimit = 2U,
    });
    state.Present({
        .sourceId = "npc_guard",
        .speakerLabel = "Guard",
        .dialogueText = "Should not appear.",
        .repeatHint = "Should not appear.",
        .objectiveText = "Should not appear.",
        .guidanceHint = "Should not appear.",
        .speakOnce = false,
        .dialogueDurationMs = 2000.0,
        .repeatDurationMs = 1000.0,
    });
    Expect(!state.ActiveDialogue().has_value()
               && !state.ActiveGuidanceHint().has_value()
               && !state.ConsumePendingObjective().has_value()
               && state.History().empty(),
           "Dialogue cue state should be fully optional when disabled");
}

// --- merged from TestFiniteComponents.cpp ---
namespace detail_FiniteComponents {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool NearlyEqual(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

}

 // namespace

void TestFiniteComponents() {
    using namespace detail_FiniteComponents;
    using ri::math::FiniteQuatComponents;
    using ri::math::FiniteScaleComponents;
    using ri::math::FiniteVec2FromSpan;
    using ri::math::FiniteVec3FromSpan;

    const ri::math::Vec3 fb{10.0f, 20.0f, 30.0f};
    const float shortIn[] = {1.0f, 2.0f};
    const ri::math::Vec3 vShort = FiniteVec3FromSpan(shortIn, fb);
    Expect(NearlyEqual(vShort.x, 10.0f) && NearlyEqual(vShort.y, 20.0f) && NearlyEqual(vShort.z, 30.0f),
           "Short span should use fallback tuple as base");

    const float bad[] = {std::numeric_limits<float>::quiet_NaN(), 5.0f, 6.0f};
    const ri::math::Vec3 vBad = FiniteVec3FromSpan(bad, fb);
    Expect(NearlyEqual(vBad.x, 10.0f) && NearlyEqual(vBad.y, 5.0f) && NearlyEqual(vBad.z, 6.0f),
           "NaN axis should fall back");

    const float good[] = {1.0f, 2.0f, 3.0f};
    const ri::math::Vec3 vGood = FiniteVec3FromSpan(good, fb);
    Expect(NearlyEqual(vGood.x, 1.0f) && NearlyEqual(vGood.y, 2.0f) && NearlyEqual(vGood.z, 3.0f),
           "Finite triple should pass through");

    const float one[] = {99.0f};
    const std::array<float, 2> uv = FiniteVec2FromSpan(std::span<const float>(one), std::array<float, 2>{7.0f, 8.0f});
    Expect(NearlyEqual(uv[0], 7.0f) && NearlyEqual(uv[1], 8.0f), "Short vec2 should use fallback");

    const float uvBad[] = {std::numeric_limits<float>::infinity(), 3.0f};
    const std::array<float, 2> uv2 = FiniteVec2FromSpan(uvBad, 1.0f, 2.0f);
    Expect(NearlyEqual(uv2[0], 1.0f) && NearlyEqual(uv2[1], 3.0f), "Vec2 should replace non-finite with fallback");

    const std::array<float, 4> id = FiniteQuatComponents(std::span<const float>());
    Expect(NearlyEqual(id[0], 0.0f) && NearlyEqual(id[1], 0.0f) && NearlyEqual(id[2], 0.0f) && NearlyEqual(id[3], 1.0f),
           "Empty quat should be identity");

    const float qIn[] = {0.0f, 0.0f, 0.0f, 2.0f};
    const std::array<float, 4> q = FiniteQuatComponents(qIn);
    Expect(NearlyEqual(q[0], 0.0f) && NearlyEqual(q[1], 0.0f) && NearlyEqual(q[2], 0.0f) && NearlyEqual(q[3], 1.0f),
           "Scaled identity should normalize to unit w");

    const float qNan[] = {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f};
    const std::array<float, 4> q2 = FiniteQuatComponents(qNan);
    Expect(NearlyEqual(q2[3], 1.0f) && NearlyEqual(q2[0], 0.0f), "NaN quat x should zero then normalize");

    const float hugeScale[] = {1e9f, 1.0f, 1.0f};
    const ri::math::Vec3 sc = FiniteScaleComponents(hugeScale, ri::math::Vec3{1.0f, 1.0f, 1.0f});
    Expect(NearlyEqual(sc.x, 512.0f) && NearlyEqual(sc.y, 1.0f) && NearlyEqual(sc.z, 1.0f),
           "Scale should cap extreme magnitude");

    const float tiny[] = {1e-5f, 2.0f, 3.0f};
    const ri::math::Vec3 scTiny = FiniteScaleComponents(tiny, ri::math::Vec3{1.0f, 1.0f, 1.0f});
    Expect(NearlyEqual(scTiny.x, 1.0f), "Near-zero scale should become 1");
}

// --- merged from TestGameManifest.cpp ---
namespace detail_GameManifest {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestGameManifest() {
    using namespace detail_GameManifest;
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "rawiron_game_manifest_test";
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root / "Source");
    fs::create_directories(root / "Games" / "DemoA");
    fs::create_directories(root / "Games" / "FolderOnly");
    {
        std::ofstream(root / "CMakeLists.txt") << "cmake_minimum_required(VERSION 3.26)\n";
        std::ofstream(root / "Games" / "DemoA" / "manifest.json")
            << "{\n"
            << "  \"id\": \"demo-a\",\n"
            << "  \"name\": \"Demo A\",\n"
            << "  \"format\": \"rawiron-game-v1.3.7\",\n"
            << "  \"type\": \"test-game\",\n"
            << "  \"entry\": \"RawIron.DemoA\",\n"
            << "  \"version\": \"1.3.7\",\n"
            << "  \"author\": \"RawIron Team\",\n"
            << "  \"editorProjectArg\": \"--game=demo-a\",\n"
            << "  \"primaryLevel\": \"levels/assembly.primitives.csv\",\n"
            << "  \"description\": \"Demo game\",\n"
            << "  \"editorOpenArgs\": [\"--game=demo-a\", \"--mode=test\"],\n"
            << "  \"editorPreviewScene\": \"demo-preview\"\n"
            << "}\n";
        std::ofstream(root / "Games" / "FolderOnly" / "manifest.json")
            << "{\n"
            << "  \"id\": \"folder-only\",\n"
            << "  \"entry\": \"RawIron.FolderOnly\"\n"
            << "}\n";
    }

    Expect(ri::content::LooksLikeWorkspaceRoot(root), "Game manifest resolver should recognize a workspace root");
    Expect(ri::content::DetectWorkspaceRoot(root / "Games" / "DemoA" / "manifest.json") == root,
           "Game manifest resolver should walk upward to the workspace root");

    const auto loaded = ri::content::LoadGameManifest(root / "Games" / "DemoA" / "manifest.json");
    Expect(loaded.has_value(), "Game manifest loader should parse a valid manifest");
    Expect(loaded->id == "demo-a" && loaded->name == "Demo A" && loaded->entry == "RawIron.DemoA",
           "Game manifest loader should expose id/name/entry fields");
    Expect(loaded->format == "rawiron-game-v1.3.7" && loaded->version == "1.3.7" && loaded->author == "RawIron Team",
           "Game manifest loader should expose v1.3 format/version/author fields");
    Expect(loaded->editorProjectArg == "--game=demo-a"
               && loaded->primaryLevel == "levels/assembly.primitives.csv",
           "Game manifest loader should expose v1.3 editorProjectArg/primaryLevel fields");
    Expect(loaded->editorOpenArgs.size() == 2U && loaded->editorOpenArgs[0] == "--game=demo-a",
           "Game manifest loader should preserve editor-open arguments");
    Expect(loaded->editorPreviewScene == "demo-preview",
           "Game manifest loader should read editorPreviewScene for editor workspace dispatch");

    const auto resolved = ri::content::ResolveGameManifest(root, "demo-a");
    Expect(resolved.has_value() && resolved->rootPath == (root / "Games" / "DemoA"),
           "Game manifest resolver should locate a game by manifest id");

    const auto folderFallback = ri::content::ResolveGameManifest(root, "FolderOnly");
    Expect(!folderFallback.has_value(),
           "Game manifest resolver should use manifest ids rather than silently relying on folder names");

    const fs::path demoRoot = root / "Games" / "DemoA";
    const fs::path joined = ri::content::ResolveGameAssetPath(demoRoot, "scripts/gameplay.riscript");
    Expect(!joined.empty() && joined == demoRoot / "scripts" / "gameplay.riscript",
           "ResolveGameAssetPath should join a safe relative path under the game root");
    Expect(ri::content::ResolveGameAssetPath(demoRoot, "../FolderOnly/manifest.json").empty(),
           "ResolveGameAssetPath should reject paths that leave the game root");
    Expect(ri::content::ResolveGameAssetPath({}, "a.txt").empty(),
           "ResolveGameAssetPath should reject an empty game root");

    fs::create_directories(demoRoot / "scripts");
    fs::create_directories(demoRoot / "config");
    fs::create_directories(demoRoot / "levels");
    fs::create_directories(demoRoot / "assets");
    std::ofstream(demoRoot / "README.md") << "# DemoA\n";
    std::ofstream(demoRoot / "scripts" / "gameplay.riscript") << "// gameplay\n";
    std::ofstream(demoRoot / "scripts" / "rendering.riscript") << "// rendering\n";
    std::ofstream(demoRoot / "scripts" / "logic.riscript") << "// logic\n";
    std::ofstream(demoRoot / "scripts" / "ui.riscript") << "// ui\n";
    std::ofstream(demoRoot / "scripts" / "audio.riscript") << "// audio\n";
    std::ofstream(demoRoot / "scripts" / "streaming.riscript") << "// streaming\n";
    std::ofstream(demoRoot / "scripts" / "localization.riscript") << "default_locale=0\n";
    std::ofstream(demoRoot / "scripts" / "physics.riscript") << "global_gravity_scale=1.0\n";
    std::ofstream(demoRoot / "scripts" / "postprocess.riscript") << "postprocess_quality=1\n";
    std::ofstream(demoRoot / "scripts" / "init.riscript") << "warmup_frames=2\n";
    std::ofstream(demoRoot / "scripts" / "state.riscript") << "checkpoint_autosave_enabled=1\n";
    std::ofstream(demoRoot / "scripts" / "network.riscript") << "network_tick_hz=30\n";
    std::ofstream(demoRoot / "scripts" / "persistence.riscript") << "save_slot_count=3\n";
    std::ofstream(demoRoot / "scripts" / "ai.riscript") << "ai_tick_hz=20\n";
    std::ofstream(demoRoot / "scripts" / "plugins.riscript") << "plugins_enabled=1\n";
    std::ofstream(demoRoot / "scripts" / "animation.riscript") << "animation_graph_rate=1.0\n";
    std::ofstream(demoRoot / "scripts" / "vfx.riscript") << "vfx_density=0.7\n";
    std::ofstream(demoRoot / "config" / "game.cfg") << "runtime_profile=1\n";
    std::ofstream(demoRoot / "config" / "input.map") << "move=WASD\n";
    std::ofstream(demoRoot / "config" / "project.dev") << "local_debug_overlay=0\n";
    std::ofstream(demoRoot / "config" / "network.cfg") << "transport=udp\n";
    std::ofstream(demoRoot / "config" / "build.profile") << "profile_name=demo\n";
    std::ofstream(demoRoot / "config" / "security.policy") << "allow_mod_scripts=0\n";
    std::ofstream(demoRoot / "config" / "plugins.policy") << "allow_unsigned_plugins=0\n";
    std::ofstream(demoRoot / "levels" / "assembly.primitives.csv") << "id,type\n";
    std::ofstream(demoRoot / "levels" / "assembly.colliders.csv") << "id,type\n";
    std::ofstream(demoRoot / "levels" / "assembly.navmesh") << "regionId,minX\n";
    std::ofstream(demoRoot / "levels" / "assembly.zones.csv")
        << "zone_id,target_level,min_x,min_y,min_z,max_x,max_y,max_z,priority\n"
        << "stream.core,assembly,0,0,0,64,32,64,100\n";
    std::ofstream(demoRoot / "levels" / "assembly.ai.nodes")
        << "nodeId,type,x,y,z,group,links\n"
        << "node_a,patrol,0,0,0,default,node_b\n";
    std::ofstream(demoRoot / "levels" / "assembly.lighting.csv")
        << "zone_id,key,value\n"
        << "stream.core,exposure,0.85\n";
    std::ofstream(demoRoot / "levels" / "assembly.cinematics.csv")
        << "shot_id,duration,track\n"
        << "intro,2.0,cam_a\n";
    std::ofstream(demoRoot / "assets" / "palette.ripalette") << "palette\n";
    std::ofstream(demoRoot / "assets" / "layers.config") << "layer.world=0\n";
    std::ofstream(demoRoot / "assets" / "manifest.assets") << "ui.crosshair=assets/ui/crosshair.png\n";
    std::ofstream(demoRoot / "assets" / "metadata.json")
        << "{ \"rawironMetadataVersion\": 1 }\n";
    std::ofstream(demoRoot / "assets" / "dependencies.json")
        << "{ \"dependencies\": ["
        << "{ \"asset\": \"assets/manifest.assets\", \"dependsOn\": [\"assets/palette.ripalette\"] }"
        << "] }\n";
    std::ofstream(demoRoot / "assets" / "streaming.manifest")
        << "assets/manifest.assets=90\n"
        << "assets/palette.ripalette=10\n";
    std::ofstream(demoRoot / "assets" / "shaders.manifest")
        << "default=assets/shaders/default.rishader\n";
    std::ofstream(demoRoot / "assets" / "animation.graph")
        << "idle->walk,0.2\n";
    std::ofstream(demoRoot / "assets" / "vfx.manifest")
        << "mist.loop=assets/vfx/mist.riparticle\n";
    fs::create_directories(demoRoot / "data");
    std::ofstream(demoRoot / "data" / "schema.db") << "schema_version=1\n";
    std::ofstream(demoRoot / "data" / "lookup.index")
        << "levels.navmesh=levels/assembly.navmesh\n"
        << "assets.streaming_manifest=assets/streaming.manifest\n";
    std::ofstream(demoRoot / "data" / "entity.registry")
        << "test_entity,npc,test,neutral,bt_test\n";
    std::ofstream(demoRoot / "data" / "telemetry.db") << "SQLite format 3\000";
    fs::create_directories(demoRoot / "plugins");
    std::ofstream(demoRoot / "plugins" / "manifest.plugins")
        << "demo.telemetry,1.0,telemetry,plugins/hooks.riplugin\n";
    std::ofstream(demoRoot / "plugins" / "load_order.cfg")
        << "demo.telemetry\n";
    std::ofstream(demoRoot / "plugins" / "registry.json")
        << "{ \"plugins\": [\"demo.telemetry\"] }\n";
    std::ofstream(demoRoot / "plugins" / "hooks.riplugin")
        << "on_startup=demo.telemetry\n";
    fs::create_directories(demoRoot / "ai");
    std::ofstream(demoRoot / "ai" / "behavior.tree") << "tree bt_test { task idle }\n";
    std::ofstream(demoRoot / "ai" / "blackboard.json") << "{ \"schemaVersion\": 1 }\n";
    std::ofstream(demoRoot / "ai" / "factions.cfg") << "neutral,neutral,ally\n";
    std::ofstream(demoRoot / "levels" / "assembly.triggers.csv")
        << "# trigger_id,event_type,min_x,min_y,min_z,max_x,max_y,max_z,param\n"
        << "t_demo,ambient,0,0,0,4,4,4,ping\n";
    std::ofstream(demoRoot / "levels" / "assembly.occlusion.csv")
        << "# volume_id,cull_group,min_x,min_y,min_z,max_x,max_y,max_z\n"
        << "vol_demo,environment,0,0,0,8,8,8\n";
    std::ofstream(demoRoot / "levels" / "assembly.audio.zones")
        << "# zone_id,reverb_preset,occlusion_dampening,min_x,min_y,min_z,max_x,max_y,max_z\n"
        << "a_demo,room_small,0.45,0,0,0,10,5,10\n";
    std::ofstream(demoRoot / "levels" / "assembly.lods.csv")
        << "# lod_id,near_distance_m,far_distance_m,min_scale,max_scale\n"
        << "lod_demo,8,45,0.6,1.0\n";
    std::ofstream(demoRoot / "assets" / "materials.manifest")
        << "# material_id,binding,source\n"
        << "m_demo,pbr,assets/materials/demo.rimat\n";
    std::ofstream(demoRoot / "assets" / "audio.banks")
        << "# bank_id,path\n"
        << "master_bank,assets/audio/demo.bank\n";
    std::ofstream(demoRoot / "assets" / "fonts.manifest")
        << "# font_id,weight,path\n"
        << "body,regular,assets/fonts/body.json\n";
    std::ofstream(demoRoot / "data" / "save.schema")
        << "{ \"schemaVersion\": 1, \"requiredSections\": [ \"player\" ] }\n";
    std::ofstream(demoRoot / "data" / "achievements.registry")
        << "{ \"achievements\": [ { \"id\": \"demo_first\", \"steam\": \"ACH_DEMO_FIRST\" } ] }\n";
    std::ofstream(demoRoot / "ai" / "perception.cfg") << "sight_range_m=40\n";
    std::ofstream(demoRoot / "ai" / "squad.tactics")
        << "# squad_id,formation,spacing_m\n"
        << "alpha,line,2.5\n";
    fs::create_directories(demoRoot / "ui");
    fs::create_directories(demoRoot / "tests");
    std::ofstream(demoRoot / "ui" / "layout.xml") << "<?xml version=\"1.0\"?><layout root=\"demo\"/>\n";
    std::ofstream(demoRoot / "ui" / "styling.css") << ":root { --accent: #fff; }\n";
    std::ofstream(demoRoot / "tests" / "gameplay.test.riscript") << "test demo_ok { assert true }\n";
    std::ofstream(demoRoot / "tests" / "rendering.test.riscript") << "test render_ok { assert true }\n";
    std::ofstream(demoRoot / "tests" / "network.test.riscript") << "test network_ok { assert true }\n";
    std::ofstream(demoRoot / "tests" / "ui.test.riscript") << "test ui_ok { assert true }\n";

    const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(*loaded);
    Expect(formatIssues.empty(),
           "Game manifest validation should accept v1.3.7 manifests with required files/version/author");
    const ri::content::GameRuntimeSupportData runtimeSupportData =
        ri::content::LoadGameRuntimeSupportData(demoRoot);
    Expect(runtimeSupportData.streamingPrioritiesByPath.size() == 2U
               && runtimeSupportData.lookupIndex.size() == 2U,
           "Game runtime support should parse streaming-manifest and lookup-index mappings");
    Expect(runtimeSupportData.levelTriggers.size() == 1U && runtimeSupportData.occlusionVolumes.size() == 1U
               && runtimeSupportData.audioZones.size() == 1U && runtimeSupportData.lodRanges.size() == 1U
               && runtimeSupportData.materialsById.size() == 1U && runtimeSupportData.audioBankPathById.size() == 1U
               && runtimeSupportData.fontPathByFontKey.size() == 1U && runtimeSupportData.squadTactics.size() == 1U,
           "Game runtime support should ingest v1.3.7 assembly and manifest support data");
    Expect(runtimeSupportData.saveSchemaVersion.has_value() && *runtimeSupportData.saveSchemaVersion == 1,
           "Game runtime support should read save.schema schemaVersion");
    const auto achievementSteam =
        ri::content::ResolveAchievementExternalId("demo_first", "steam", runtimeSupportData);
    Expect(achievementSteam.has_value() && achievementSteam.value() == "ACH_DEMO_FIRST",
           "Game runtime support should resolve achievement registry platform ids");
    Expect(ri::content::TryGetPerceptionScalar("sight_range_m", runtimeSupportData).value() == "40",
           "Game runtime support should parse perception.cfg scalars");
    const ri::content::AudioZoneRow* zoneAtOrigin =
        ri::content::FindAudioZoneAtPoint(1.0f, 1.0f, 1.0f, runtimeSupportData);
    Expect(zoneAtOrigin != nullptr && zoneAtOrigin->id == "a_demo",
           "Game runtime support should resolve audio zones at sample positions");
    const float lodNear = ri::content::ComputeLodScaleForDistance("lod_demo", 8.0f, runtimeSupportData);
    const float lodFar = ri::content::ComputeLodScaleForDistance("lod_demo", 45.0f, runtimeSupportData);
    Expect(lodNear > lodFar && lodFar > 0.0f,
           "Game runtime support should compute distance-based LOD scale interpolation");
    Expect(ri::content::ResolveLookupValueOr("levels.navmesh", "levels/assembly.navmesh", runtimeSupportData)
               == "levels/assembly.navmesh",
           "Game runtime support should resolve lookup-index overrides");
    const int manifestAssetScore =
        ri::content::ComputeResourcePriorityScore(runtimeSupportData, "assets/manifest.assets");
    const int paletteAssetScore =
        ri::content::ComputeResourcePriorityScore(runtimeSupportData, "assets/palette.ripalette");
    Expect(manifestAssetScore > paletteAssetScore,
           "Game runtime support should prioritize high-weight streaming assets and dependency providers");
    const int levelScore = ri::content::ComputeResourcePriorityScore(runtimeSupportData, "levels/assembly.primitives.csv");
    Expect(levelScore > 0,
           "Game runtime support should project zone priorities into authored level resource ordering");
    const std::vector<std::string_view> preloadLevels = {"assembly", "unknown_level"};
    ri::content::LevelPreloadPlan preloadPlan = ri::content::BuildLevelPreloadPlan(preloadLevels, runtimeSupportData);
    Expect(preloadPlan.entries.size() == 2U && !preloadPlan.pendingPayloads.empty(),
           "Game runtime support should build preload plans with pending payload tracking");
    const std::string firstPayload = preloadPlan.entries.front().payloadPath;
    ri::content::MarkPreloadPayloadReady(preloadPlan, firstPayload);
    Expect(!preloadPlan.pendingPayloads.contains(firstPayload),
           "Game runtime support preload plans should allow pending payload completion updates");

    ri::content::GameManifest invalid = *loaded;
    invalid.format = "rawiron-game-v1.2";
    invalid.version.clear();
    invalid.author.clear();
    invalid.type = "prototype";
    invalid.editorProjectArg = "--game=wrong-id";
    invalid.primaryLevel = "levels/assembly.csv";
    invalid.editorPreviewScene = "Demo Preview";
    invalid.editorOpenArgs = {"--mode=test", "", "--mode=test"};
    const std::vector<std::string> invalidIssues = ri::content::ValidateGameProjectFormat(invalid);
    const auto hasIssue = [&invalidIssues](const std::string& expected) {
        return std::find(invalidIssues.begin(), invalidIssues.end(), expected) != invalidIssues.end();
    };
    Expect(hasIssue("manifest.format must be \"rawiron-game-v1.3.7\"."),
           "Game manifest validation should enforce v1.3.7 format id");
    Expect(hasIssue("manifest.version must be a non-empty string.")
               && hasIssue("manifest.author must be a non-empty string."),
           "Game manifest validation should require v1.3 version and author fields");
    Expect(hasIssue("manifest.type must be one of: test-game, game, experience."),
           "Game manifest validation should restrict manifest.type to supported project categories");
    Expect(hasIssue("manifest.editorProjectArg must match \"--game=demo-a\".")
               && hasIssue("manifest.primaryLevel must point to a .primitives.csv file."),
           "Game manifest validation should enforce v1.3 editor dispatch and primary-level invariants");
    Expect(hasIssue("manifest.editorOpenArgs must include manifest.editorProjectArg."),
           "Game manifest validation should enforce editorOpenArgs parity with editorProjectArg");
    Expect(hasIssue("manifest.editorOpenArgs cannot contain empty tokens.")
               && hasIssue("manifest.editorPreviewScene must use lowercase slug format (a-z, 0-9, hyphen)."),
           "Game manifest validation should enforce editor open-arg hygiene and preview-scene slug format");

    fs::remove(demoRoot / "README.md", error);
    fs::remove(demoRoot / "scripts" / "ui.riscript", error);
    fs::remove(demoRoot / "scripts" / "audio.riscript", error);
    fs::remove(demoRoot / "scripts" / "streaming.riscript", error);
    fs::remove(demoRoot / "scripts" / "localization.riscript", error);
    fs::remove(demoRoot / "scripts" / "physics.riscript", error);
    fs::remove(demoRoot / "scripts" / "postprocess.riscript", error);
    fs::remove(demoRoot / "scripts" / "init.riscript", error);
    fs::remove(demoRoot / "scripts" / "state.riscript", error);
    fs::remove(demoRoot / "scripts" / "network.riscript", error);
    fs::remove(demoRoot / "scripts" / "persistence.riscript", error);
    fs::remove(demoRoot / "scripts" / "ai.riscript", error);
    fs::remove(demoRoot / "scripts" / "plugins.riscript", error);
    fs::remove(demoRoot / "scripts" / "animation.riscript", error);
    fs::remove(demoRoot / "scripts" / "vfx.riscript", error);
    fs::remove(demoRoot / "config" / "game.cfg", error);
    fs::remove(demoRoot / "config" / "input.map", error);
    fs::remove(demoRoot / "config" / "project.dev", error);
    fs::remove(demoRoot / "config" / "network.cfg", error);
    fs::remove(demoRoot / "config" / "build.profile", error);
    fs::remove(demoRoot / "config" / "security.policy", error);
    fs::remove(demoRoot / "config" / "plugins.policy", error);
    fs::remove(demoRoot / "levels" / "assembly.navmesh", error);
    fs::remove(demoRoot / "levels" / "assembly.zones.csv", error);
    fs::remove(demoRoot / "levels" / "assembly.ai.nodes", error);
    fs::remove(demoRoot / "levels" / "assembly.lighting.csv", error);
    fs::remove(demoRoot / "levels" / "assembly.cinematics.csv", error);
    fs::remove(demoRoot / "assets" / "layers.config", error);
    fs::remove(demoRoot / "assets" / "manifest.assets", error);
    fs::remove(demoRoot / "assets" / "metadata.json", error);
    fs::remove(demoRoot / "assets" / "dependencies.json", error);
    fs::remove(demoRoot / "assets" / "streaming.manifest", error);
    fs::remove(demoRoot / "assets" / "shaders.manifest", error);
    fs::remove(demoRoot / "assets" / "animation.graph", error);
    fs::remove(demoRoot / "assets" / "vfx.manifest", error);
    fs::remove(demoRoot / "data" / "schema.db", error);
    fs::remove(demoRoot / "data" / "lookup.index", error);
    fs::remove(demoRoot / "data" / "entity.registry", error);
    fs::remove(demoRoot / "data" / "telemetry.db", error);
    fs::remove(demoRoot / "plugins" / "manifest.plugins", error);
    fs::remove(demoRoot / "plugins" / "load_order.cfg", error);
    fs::remove(demoRoot / "plugins" / "registry.json", error);
    fs::remove(demoRoot / "plugins" / "hooks.riplugin", error);
    fs::remove(demoRoot / "ai" / "behavior.tree", error);
    fs::remove(demoRoot / "ai" / "blackboard.json", error);
    fs::remove(demoRoot / "ai" / "factions.cfg", error);
    fs::remove(demoRoot / "levels" / "assembly.triggers.csv", error);
    fs::remove(demoRoot / "levels" / "assembly.occlusion.csv", error);
    fs::remove(demoRoot / "levels" / "assembly.audio.zones", error);
    fs::remove(demoRoot / "levels" / "assembly.lods.csv", error);
    fs::remove(demoRoot / "assets" / "materials.manifest", error);
    fs::remove(demoRoot / "assets" / "audio.banks", error);
    fs::remove(demoRoot / "assets" / "fonts.manifest", error);
    fs::remove(demoRoot / "data" / "save.schema", error);
    fs::remove(demoRoot / "data" / "achievements.registry", error);
    fs::remove(demoRoot / "ai" / "perception.cfg", error);
    fs::remove(demoRoot / "ai" / "squad.tactics", error);
    fs::remove(demoRoot / "ui" / "layout.xml", error);
    fs::remove(demoRoot / "ui" / "styling.css", error);
    fs::remove(demoRoot / "tests" / "gameplay.test.riscript", error);
    fs::remove(demoRoot / "tests" / "rendering.test.riscript", error);
    fs::remove(demoRoot / "tests" / "network.test.riscript", error);
    fs::remove(demoRoot / "tests" / "ui.test.riscript", error);
    const std::vector<std::string> missingFileIssues = ri::content::ValidateGameProjectFormat(*loaded);
    const auto hasMissingIssue = [&missingFileIssues](const std::string& expected) {
        return std::find(missingFileIssues.begin(), missingFileIssues.end(), expected) != missingFileIssues.end();
    };
    Expect(hasMissingIssue("missing README.md.")
               && hasMissingIssue("missing scripts/ui.riscript.")
               && hasMissingIssue("missing scripts/audio.riscript.")
               && hasMissingIssue("missing scripts/streaming.riscript.")
               && hasMissingIssue("missing scripts/localization.riscript.")
               && hasMissingIssue("missing scripts/physics.riscript.")
               && hasMissingIssue("missing scripts/postprocess.riscript.")
               && hasMissingIssue("missing scripts/init.riscript.")
               && hasMissingIssue("missing scripts/state.riscript.")
               && hasMissingIssue("missing scripts/network.riscript.")
               && hasMissingIssue("missing scripts/persistence.riscript.")
               && hasMissingIssue("missing scripts/ai.riscript.")
               && hasMissingIssue("missing config/game.cfg.")
               && hasMissingIssue("missing config/input.map.")
               && hasMissingIssue("missing config/project.dev.")
               && hasMissingIssue("missing config/network.cfg.")
               && hasMissingIssue("missing config/build.profile.")
               && hasMissingIssue("missing config/security.policy.")
               && hasMissingIssue("missing levels/assembly.navmesh.")
               && hasMissingIssue("missing levels/assembly.zones.csv.")
               && hasMissingIssue("missing levels/assembly.ai.nodes.")
               && hasMissingIssue("missing assets/layers.config.")
               && hasMissingIssue("missing assets/manifest.assets.")
               && hasMissingIssue("missing assets/metadata.json.")
               && hasMissingIssue("missing assets/dependencies.json.")
               && hasMissingIssue("missing assets/streaming.manifest.")
               && hasMissingIssue("missing assets/shaders.manifest.")
               && hasMissingIssue("missing data/schema.db.")
               && hasMissingIssue("missing data/lookup.index.")
               && hasMissingIssue("missing data/entity.registry.")
               && hasMissingIssue("missing ai/behavior.tree.")
               && hasMissingIssue("missing ai/blackboard.json.")
               && hasMissingIssue("missing ai/factions.cfg.")
               && hasMissingIssue("missing levels/assembly.triggers.csv.")
               && hasMissingIssue("missing levels/assembly.occlusion.csv.")
               && hasMissingIssue("missing levels/assembly.audio.zones.")
               && hasMissingIssue("missing levels/assembly.lods.csv.")
               && hasMissingIssue("missing assets/materials.manifest.")
               && hasMissingIssue("missing assets/audio.banks.")
               && hasMissingIssue("missing assets/fonts.manifest.")
               && hasMissingIssue("missing data/save.schema.")
               && hasMissingIssue("missing data/achievements.registry.")
               && hasMissingIssue("missing ai/perception.cfg.")
               && hasMissingIssue("missing ai/squad.tactics.")
               && hasMissingIssue("missing ui/layout.xml.")
               && hasMissingIssue("missing ui/styling.css.")
               && hasMissingIssue("missing tests/gameplay.test.riscript.")
               && hasMissingIssue("missing tests/rendering.test.riscript.")
               && hasMissingIssue("missing tests/network.test.riscript.")
               && hasMissingIssue("missing tests/ui.test.riscript."),
           "Game manifest validation should enforce newly required README.md and script contract files");

    std::ofstream(demoRoot / "levels" / "assembly.zones.csv", std::ios::trunc);
    std::ofstream(demoRoot / "levels" / "assembly.ai.nodes", std::ios::trunc);
    std::ofstream(demoRoot / "assets" / "streaming.manifest", std::ios::trunc);
    std::ofstream(demoRoot / "assets" / "shaders.manifest", std::ios::trunc);
    std::ofstream(demoRoot / "data" / "lookup.index", std::ios::trunc);
    std::ofstream(demoRoot / "data" / "entity.registry", std::ios::trunc);
    std::ofstream(demoRoot / "scripts" / "ai.riscript", std::ios::trunc);
    std::ofstream(demoRoot / "config" / "security.policy", std::ios::trunc);
    std::ofstream(demoRoot / "ai" / "behavior.tree", std::ios::trunc);
    std::ofstream(demoRoot / "ai" / "blackboard.json", std::ios::trunc);
    std::ofstream(demoRoot / "ai" / "factions.cfg", std::ios::trunc);
    std::ofstream(demoRoot / "levels" / "assembly.triggers.csv", std::ios::trunc);
    std::ofstream(demoRoot / "levels" / "assembly.occlusion.csv", std::ios::trunc);
    std::ofstream(demoRoot / "levels" / "assembly.audio.zones", std::ios::trunc);
    std::ofstream(demoRoot / "levels" / "assembly.lods.csv", std::ios::trunc);
    std::ofstream(demoRoot / "assets" / "materials.manifest", std::ios::trunc);
    std::ofstream(demoRoot / "assets" / "audio.banks", std::ios::trunc);
    std::ofstream(demoRoot / "assets" / "fonts.manifest", std::ios::trunc);
    std::ofstream(demoRoot / "data" / "save.schema", std::ios::trunc);
    std::ofstream(demoRoot / "data" / "achievements.registry", std::ios::trunc);
    std::ofstream(demoRoot / "ai" / "perception.cfg", std::ios::trunc);
    std::ofstream(demoRoot / "ai" / "squad.tactics", std::ios::trunc);
    std::ofstream(demoRoot / "ui" / "layout.xml", std::ios::trunc);
    std::ofstream(demoRoot / "ui" / "styling.css", std::ios::trunc);
    std::ofstream(demoRoot / "tests" / "gameplay.test.riscript", std::ios::trunc);
    std::ofstream(demoRoot / "tests" / "rendering.test.riscript", std::ios::trunc);
    std::ofstream(demoRoot / "tests" / "network.test.riscript", std::ios::trunc);
    std::ofstream(demoRoot / "tests" / "ui.test.riscript", std::ios::trunc);
    const std::vector<std::string> emptyNewFileIssues = ri::content::ValidateGameProjectFormat(*loaded);
    const auto hasEmptyIssue = [&emptyNewFileIssues](const std::string& expected) {
        return std::find(emptyNewFileIssues.begin(), emptyNewFileIssues.end(), expected) != emptyNewFileIssues.end();
    };
    Expect(hasEmptyIssue("levels/assembly.zones.csv must be a non-empty file.")
               && hasEmptyIssue("levels/assembly.ai.nodes must be a non-empty file.")
               && hasEmptyIssue("assets/streaming.manifest must be a non-empty file.")
               && hasEmptyIssue("assets/shaders.manifest must be a non-empty file.")
               && hasEmptyIssue("data/lookup.index must be a non-empty file.")
               && hasEmptyIssue("data/entity.registry must be a non-empty file.")
               && hasEmptyIssue("scripts/ai.riscript must be a non-empty file.")
               && hasEmptyIssue("config/security.policy must be a non-empty file.")
               && hasEmptyIssue("ai/behavior.tree must be a non-empty file.")
               && hasEmptyIssue("ai/blackboard.json must be a non-empty file.")
               && hasEmptyIssue("ai/factions.cfg must be a non-empty file.")
               && hasEmptyIssue("levels/assembly.triggers.csv must be a non-empty file.")
               && hasEmptyIssue("levels/assembly.occlusion.csv must be a non-empty file.")
               && hasEmptyIssue("levels/assembly.audio.zones must be a non-empty file.")
               && hasEmptyIssue("levels/assembly.lods.csv must be a non-empty file.")
               && hasEmptyIssue("assets/materials.manifest must be a non-empty file.")
               && hasEmptyIssue("assets/audio.banks must be a non-empty file.")
               && hasEmptyIssue("assets/fonts.manifest must be a non-empty file.")
               && hasEmptyIssue("data/save.schema must be a non-empty file.")
               && hasEmptyIssue("data/achievements.registry must be a non-empty file.")
               && hasEmptyIssue("ai/perception.cfg must be a non-empty file.")
               && hasEmptyIssue("ai/squad.tactics must be a non-empty file.")
               && hasEmptyIssue("ui/layout.xml must be a non-empty file.")
               && hasEmptyIssue("ui/styling.css must be a non-empty file.")
               && hasEmptyIssue("tests/gameplay.test.riscript must be a non-empty file.")
               && hasEmptyIssue("tests/rendering.test.riscript must be a non-empty file.")
               && hasEmptyIssue("tests/network.test.riscript must be a non-empty file.")
               && hasEmptyIssue("tests/ui.test.riscript must be a non-empty file."),
           "Game manifest validation should enforce non-empty checks for new streaming and lookup files");

    fs::remove_all(root, error);
}

// --- merged from TestHeadlessModuleVerifier.cpp ---
namespace detail_HeadlessModuleVerifier {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct CounterSnapshot {
    int value = 0;
    std::uint16_t modeCode = 0;
};

class CounterModule {
public:
    void Increment() {
        ++value_;
    }

    void Arm(std::uint16_t code) {
        modeCode_ = code;
    }

    [[nodiscard]] CounterSnapshot Read() const {
        return CounterSnapshot{.value = value_, .modeCode = modeCode_};
    }

private:
    int value_ = 0;
    std::uint16_t modeCode_ = 0;
};

void TestHeadlessSuccessChartToyModule() {
    CounterModule module;
    const std::function<CounterSnapshot()> read = [&module] { return module.Read(); };

    const std::vector<ri::world::headless::HeadlessSuccessChartRow<CounterSnapshot>> chart = {
        {.apply = {}, .match = [](const CounterSnapshot& s, std::string& f) {
             if (s.value != 0 || s.modeCode != 0) {
                 f = "expected initial idle snapshot";
                 return false;
             }
             return true;
         }},
        {.apply = [&module] { module.Increment(); },
         .match = [](const CounterSnapshot& s, std::string& f) {
             if (s.value != 1) {
                 f = "expected counter 1 after first increment";
                 return false;
             }
             return true;
         }},
        {.apply = [&module] {
             module.Increment();
             module.Arm(0x30);
         },
         .match = [](const CounterSnapshot& s, std::string& f) {
             if (s.value != 2 || s.modeCode != 0x30) {
                 f = "expected counter 2 and armed mode code";
                 return false;
             }
             return true;
         }},
    };

    std::string failure;
    Expect(ri::world::headless::RunHeadlessSuccessChart(read, chart, failure),
           "Toy headless chart should pass: " + failure);
}

struct NpcHeadlessSnapshot {
    ri::world::NpcInteractionOutcomeCode interactionOutcome = ri::world::NpcInteractionOutcomeCode::Unspecified;
    ri::world::NpcPatrolPhaseCode patrolPhase = ri::world::NpcPatrolPhaseCode::Unspecified;
    std::uint8_t patrolIntentOrdinal = 0;
};

void TestHeadlessSuccessChartNpcAgent() {
    ri::world::NpcAgentState agent({
        .mode = ri::world::NpcAgentMode::Patrol,
        .allowInteraction = true,
        .allowPatrol = true,
        .allowSpeakOnce = true,
        .historyLimit = 4U,
    });
    agent.Configure({
        .id = "chart_npc",
        .displayName = "Chart",
        .defaultAnimation = "idle",
        .interactionAnimation = "talk",
        .resumeAnimation = "idle",
        .pathAnimation = "walk",
        .pathRunAnimation = "run",
        .pathTurnAnimation = "alert",
        .pathIdleAnimation = "idle",
        .patrolSpeed = 1.8f,
        .pathRunThreshold = 1.55f,
        .pathEpsilon = 0.25f,
        .pathWaitMs = 500.0,
        .interactionCooldownMs = 2000.0,
        .patrolMode = ri::world::NpcPatrolMode::PingPong,
        .patrolLoop = true,
        .lookAtPath = true,
        .speakOnce = true,
        .pathPoints = {
            {0.0f, 0.0f, 0.0f},
            {2.0f, 0.0f, 0.0f},
        },
    });

    ri::world::NpcInteractionResult lastInteraction{};
    ri::world::NpcPatrolUpdate lastPatrol{};

    const std::function<NpcHeadlessSnapshot()> read = [&] {
        return NpcHeadlessSnapshot{
            .interactionOutcome = lastInteraction.outcomeCode,
            .patrolPhase = lastPatrol.phaseCode,
            .patrolIntentOrdinal = lastPatrol.animationIntentOrdinal,
        };
    };

    const std::vector<ri::world::headless::HeadlessSuccessChartRow<NpcHeadlessSnapshot>> chart = {
        {.apply = [&] {
             lastInteraction = agent.Interact(0.0, 4500.0, 2200.0, "HELLO.", "Nothing new.");
             lastPatrol = agent.AdvancePatrol(0.0, 0.016f, {0.0f, 0.0f, 0.0f});
         },
         .match = [](const NpcHeadlessSnapshot& s, std::string& f) {
             if (s.interactionOutcome != ri::world::NpcInteractionOutcomeCode::AcceptedPrimaryDialogue) {
                 f = "interaction outcome mismatch";
                 return false;
             }
             if (static_cast<std::uint16_t>(s.interactionOutcome) != 0x0105) {
                 f = "interaction serial mismatch";
                 return false;
             }
             if (s.patrolPhase != ri::world::NpcPatrolPhaseCode::PausedWaitAtWaypoint) {
                 f = "patrol should start in waypoint wait";
                 return false;
             }
             if (s.patrolIntentOrdinal != ri::world::NpcAnimationIntentOrdinal(ri::world::NpcAnimationIntent::Idle)) {
                 f = "patrol intent ordinal should be idle during wait";
                 return false;
             }
             return true;
         }},
        {.apply = [&] {
             lastPatrol = agent.AdvancePatrol(1.0, 0.016f, {0.0f, 0.0f, 0.0f});
         },
         .match = [](const NpcHeadlessSnapshot& s, std::string& f) {
             if (s.patrolPhase != ri::world::NpcPatrolPhaseCode::AdvancingRun) {
                 f = "patrol should advance at run phase when speed exceeds threshold";
                 return false;
             }
             if (s.patrolIntentOrdinal != 2U) {
                 f = "run intent ordinal should be 2";
                 return false;
             }
             return true;
         }},
    };

    std::string failure;
    Expect(ri::world::headless::RunHeadlessSuccessChart(read, chart, failure),
           "NPC headless success chart should pass: " + failure);
}

}

 // namespace

void TestHeadlessModuleVerifier() {
    using namespace detail_HeadlessModuleVerifier;
    TestHeadlessSuccessChartToyModule();
    TestHeadlessSuccessChartNpcAgent();
}

// --- merged from TestHostileCharacterAi.cpp ---
namespace detail_HostileCharacterAi {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ri::world::HostileCharacterAiFrameInput Frame(double time,
                                              float dt,
                                              ri::math::Vec3 character,
                                              float yaw,
                                              ri::math::Vec3 player,
                                              bool visible,
                                              bool flashlightOn = false,
                                              bool flashlightHit = false,
                                              bool safeZone = false) {
    return ri::world::HostileCharacterAiFrameInput{
        .timeSeconds = time,
        .deltaSeconds = dt,
        .characterPosition = character,
        .characterYawRadians = yaw,
        .playerPosition = player,
        .playerVisible = visible,
        .playerFlashlightActive = flashlightOn,
        .flashlightIlluminatesCharacter = flashlightHit,
        .playerInSafeZone = safeZone,
        .playerInSafeLight = false,
        .characterInSafeLight = false,
    };
}

}

 // namespace

void TestHostileCharacterAi() {
    using namespace detail_HostileCharacterAi;
    ri::world::HostileCharacterAi ai{};
    ai.Configure({
        .id = "stalker_a",
        .patrolPath = {{0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}},
        .homePosition = {0.0f, 0.0f, 0.0f},
        .patrolSpeed = 2.0f,
        .chaseSpeed = 4.0f,
        .attackDistance = 2.5f,
        .attackCooldownSeconds = 0.0,
        .attackWindupSeconds = 0.1,
        .patrolWaitSeconds = 0.0,
        .alertDurationSeconds = 0.2,
        .chaseForgetSeconds = 5.0,
        .leashDistance = -1.0f,
        .lightAversionStrongHoldSeconds = 0.3,
        .lightAversionWeakHoldSeconds = 0.15,
        .lightAversionDecayPerSecond = 0.0,
    });

    Expect(ai.CurrentPhase() == ri::world::HostileCharacterAiPhase::Patrol,
           "Hostile AI should start in patrol when configured");

    const ri::world::HostileCharacterAiFrameOutput patrolMove =
        ai.Advance(Frame(0.0, 0.05f, {0.0f, 0.0f, 0.0f}, 0.0f, {20.0f, 0.0f, 0.0f}, false));
    Expect(patrolMove.phase == ri::world::HostileCharacterAiPhase::Patrol
               && patrolMove.horizontalDisplacementRequest.x > 0.0f,
           "Hostile AI should advance along patrol when the player is not visible");

    const ri::world::HostileCharacterAiFrameOutput spotted =
        ai.Advance(Frame(0.1, 0.05f, {0.0f, 0.0f, 0.0f}, 0.0f, {2.0f, 0.0f, 0.0f}, true));
    Expect(ri::math::Distance(ai.LastKnownPlayerPosition(), ri::math::Vec3{2.0f, 0.0f, 0.0f}) < 0.0001f,
           "LastKnownPlayerPosition should mirror the last visible player sample for host-side steering");
    Expect(spotted.phase == ri::world::HostileCharacterAiPhase::Alert && spotted.showFirstAwarenessMessage
               && spotted.chaseLayerActive,
           "Hostile AI should enter alert with a first-awareness signal when sighting the player from patrol");

    const ri::world::HostileCharacterAiFrameOutput chase =
        ai.Advance(Frame(0.35, 0.05f, {0.0f, 0.0f, 0.0f}, 0.0f, {2.0f, 0.0f, 0.0f}, true));
    Expect(chase.phase == ri::world::HostileCharacterAiPhase::Chase && chase.chaseLayerActive,
           "Hostile AI should commit to chase after the alert window with sustained visibility");

    ri::world::HostileCharacterAi closeAi{};
    closeAi.Configure({
        .id = "grapple_a",
        .patrolPath = {},
        .homePosition = {0.0f, 0.0f, 0.0f},
        .chaseSpeed = 6.0f,
        .attackDistance = 3.0f,
        .attackCooldownSeconds = 0.0,
        .attackWindupSeconds = 0.05,
        .alertDurationSeconds = 0.01,
        .leashDistance = -1.0f,
        .lightAversionDecayPerSecond = 10.0,
    });
    const ri::world::HostileCharacterAiFrameOutput a0 =
        closeAi.Advance(Frame(0.0, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.0f}, true));
    Expect(a0.phase == ri::world::HostileCharacterAiPhase::Alert,
           "Hostile AI should still use alert before chase even at close range");
    [[maybe_unused]] const auto chaseStep =
        closeAi.Advance(Frame(0.05, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.0f}, true));
    const ri::world::HostileCharacterAiFrameOutput commit =
        closeAi.Advance(Frame(0.12, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.0f}, true));
    Expect(commit.phase == ri::world::HostileCharacterAiPhase::AttackCommit,
           "Hostile AI should enter attack commit inside melee reach with cooldown satisfied");
    const ri::world::HostileCharacterAiFrameOutput resolved =
        closeAi.Advance(Frame(0.2, 0.08f, {0.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 0.0f, 0.0f}, true));
    Expect(resolved.hostMeleeHitResolved && resolved.phase == ri::world::HostileCharacterAiPhase::Return,
           "Hostile AI should fire host melee resolution after windup and fall back to return");

    ri::world::HostileCharacterAi safeAi{};
    safeAi.Configure({
        .id = "safe_test",
        .patrolPath = {},
        .homePosition = {0.0f, 0.0f, 0.0f},
        .alertDurationSeconds = 10.0,
        .leashDistance = -1.0f,
    });
    [[maybe_unused]] const auto safeWarmup =
        safeAi.Advance(Frame(0.0, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {2.0f, 0.0f, 0.0f}, true));
    const ri::world::HostileCharacterAiFrameOutput safeOut =
        safeAi.Advance(Frame(0.1, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {2.0f, 0.0f, 0.0f}, true, false, false, true));
    Expect(safeOut.phase == ri::world::HostileCharacterAiPhase::Return && !safeOut.chaseLayerActive,
           "Hostile AI should abandon chase-related phases when the player enters a safe zone");

    ri::world::HostileCharacterAi lightAi{};
    lightAi.Configure({
        .id = "light_test",
        .patrolPath = {{0.0f, 0.0f, 0.0f}},
        .homePosition = {0.0f, 0.0f, 0.0f},
        .leashDistance = -1.0f,
        .lightAversionStrongHoldSeconds = 0.5,
        .lightAversionDecayPerSecond = 4.0,
    });
    [[maybe_unused]] const auto lightWarmup =
        lightAi.Advance(Frame(0.0, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {3.0f, 0.0f, 0.0f}, true, true, true));
    const ri::world::HostileCharacterAiFrameOutput litPatrol =
        lightAi.Advance(Frame(0.05, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {3.0f, 0.0f, 0.0f}, true, true, true));
    Expect(litPatrol.phase == ri::world::HostileCharacterAiPhase::Patrol,
           "Hostile AI should refuse patrol→alert escalation while the player’s flashlight validates on the character");

    [[maybe_unused]] const auto lightOff =
        lightAi.Advance(Frame(0.2, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {3.0f, 0.0f, 0.0f}, true, false, false));
    const ri::world::HostileCharacterAiFrameOutput afterDecay =
        lightAi.Advance(Frame(2.0, 2.0f, {0.0f, 0.0f, 0.0f}, 0.0f, {3.0f, 0.0f, 0.0f}, true, false, false));
    Expect(afterDecay.phase == ri::world::HostileCharacterAiPhase::Alert,
           "Hostile AI should allow alert once light aversion decays and visibility remains");

    ri::world::HostileCharacterAi leashAi{};
    leashAi.Configure({
        .id = "leash_test",
        .patrolPath = {},
        .homePosition = {0.0f, 0.0f, 0.0f},
        .alertDurationSeconds = 0.01,
        .chaseForgetSeconds = 30.0,
        .leashDistance = 5.0f,
    });
    [[maybe_unused]] const auto leashA =
        leashAi.Advance(Frame(0.0, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {2.0f, 0.0f, 0.0f}, true));
    [[maybe_unused]] const auto leashB =
        leashAi.Advance(Frame(0.05, 0.02f, {0.0f, 0.0f, 0.0f}, 0.0f, {2.0f, 0.0f, 0.0f}, true));
    const ri::world::HostileCharacterAiFrameOutput leashed =
        leashAi.Advance(Frame(0.2, 0.02f, {9.0f, 0.0f, 0.0f}, 0.0f, {10.0f, 0.0f, 0.0f}, true));
    Expect(leashed.phase == ri::world::HostileCharacterAiPhase::Return,
           "Hostile AI should force return when the leash distance from home is exceeded");

    ri::world::HostileCharacterAi scripted{};
    scripted.Configure({.id = "script", .patrolPath = {}, .homePosition = {0.0f, 0.0f, 0.0f}});
    scripted.ApplyFalseSignal({8.0f, 0.0f, 0.0f}, 6.0f);
    Expect(scripted.CurrentPhase() == ri::world::HostileCharacterAiPhase::Alert,
           "Hostile AI should honor scripted false-signal alerts");
    scripted.ForcePhase(ri::world::HostileCharacterAiPhase::Patrol);
    Expect(scripted.HasForcedPhase() && scripted.CurrentPhase() == ri::world::HostileCharacterAiPhase::Patrol,
           "Hostile AI should honor forced phases for cinematic overrides");
    scripted.ClearForcedPhase();
    Expect(!scripted.HasForcedPhase(), "Hostile AI should release forced phase locks when cleared");

    ri::world::HostileCharacterAi legacyYaw{};
    legacyYaw.Configure({
        .id = "legacy_yaw_turn",
        .patrolPath = {{0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}},
        .homePosition = {0.0f, 0.0f, 0.0f},
        .patrolSpeed = 2.0f,
        .chaseSpeed = 4.0f,
        .attackDistance = 2.5f,
        .attackCooldownSeconds = 0.0,
        .attackWindupSeconds = 0.1,
        .patrolWaitSeconds = 0.0,
        .alertDurationSeconds = 0.2,
        .chaseForgetSeconds = 5.0,
        .leashDistance = -1.0f,
        .lightAversionStrongHoldSeconds = 0.3,
        .lightAversionWeakHoldSeconds = 0.15,
        .lightAversionDecayPerSecond = 0.0,
        .safeZoneAttackCooldownSeconds = 1.1,
        .initialPhase = ri::world::HostileCharacterAiPhase::Patrol,
        .patrolWrap = ri::world::HostilePatrolWrapMode::Loop,
        .yawTurnMath = ri::math::YawShortestDeltaMode::LegacyIterativePi,
    });
    const ri::world::HostileCharacterAiFrameOutput legacyPatrol =
        legacyYaw.Advance(Frame(0.0, 0.05f, {0.0f, 0.0f, 0.0f}, 0.0f, {20.0f, 0.0f, 0.0f}, false));
    Expect(std::isfinite(legacyPatrol.yawDeltaThisFrame),
           "Optional legacy iterative yaw math should still advance patrol locomotion safely");
}

// --- merged from TestInputLabelFormat.cpp ---
namespace detail_InputLabelFormat {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestInputLabelFormat() {
    using namespace detail_InputLabelFormat;
    using ri::core::FormatInputLabelFromInputId;
    using ri::core::FormatNormalizedKeyboardLabel;
    using ri::core::NormalizeKeyboardInputId;

    Expect(FormatNormalizedKeyboardLabel("KeyE") == "E", "Key* strip");
    Expect(FormatNormalizedKeyboardLabel("Digit7") == "7", "Digit* strip");
    Expect(FormatNormalizedKeyboardLabel("ShiftLeft") == "Shift", "Modifier collapse");
    Expect(NormalizeKeyboardInputId("  mouse1  ") == "MouseLeft", "Friendly mouse alias normalizes");

    Expect(FormatInputLabelFromInputId("KeyQ") == "Q", "Full path KeyQ");
    Expect(FormatInputLabelFromInputId("shift") == "Shift", "Lowercase alias through full format");

    Expect(ri::core::ActionBindings::FormatInputLabel("KeyF") == FormatInputLabelFromInputId("KeyF"),
           "ActionBindings::FormatInputLabel should delegate to FormatInputLabelFromInputId");
}

// --- merged from TestInteractionPromptState.cpp ---
namespace detail_InteractionPromptState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestInteractionPromptState() {
    using namespace detail_InteractionPromptState;
    ri::world::InteractionPromptState state;

    state.Show({
        .actionLabel = "E",
        .verb = "Open",
        .targetLabel = "Maintenance Door",
    });

    ri::world::InteractionPromptView view = state.BuildView();
    Expect(view.visible, "Interaction prompt should become visible when an action is present");
    Expect(view.text == "[E] OPEN MAINTENANCE DOOR",
           "Interaction prompt should format action, verb, and target labels");

    state.Show({
        .actionLabel = "Mouse 1",
        .verb = "Interact",
        .targetLabel = "Security Console",
    });
    view = state.BuildView();
    Expect(view.text == "[Mouse 1] SECURITY CONSOLE",
           "Interaction prompt should omit the generic INTERACT verb when a target label exists");

    state.Show({
        .actionLabel = "F",
        .verb = "Toggle",
        .targetLabel = "",
        .fallbackLabel = "Flashlight",
    });
    view = state.BuildView();
    Expect(view.text == "[F] FLASHLIGHT",
           "Interaction prompt should use a fallback label when there is no target label");

    state.SetSuppressed(ri::world::InteractionPromptSuppression::Paused, true);
    view = state.BuildView();
    Expect(!view.visible,
           "Interaction prompt should hide itself while any suppression state is active");
    state.SetSuppressed(ri::world::InteractionPromptSuppression::Paused, false);
    Expect(!state.Suppressed(),
           "Interaction prompt should clear suppression flags when they are disabled");

    state.Clear();
    view = state.BuildView();
    Expect(!view.visible && view.text.empty(),
           "Interaction prompt should clear visible state when the prompt is reset");
}

// --- merged from TestInventoryState.cpp ---
namespace detail_InventoryState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ri::world::InventoryItemDefinition MakeFlashlight() {
    return {
        .id = "flashlight",
        .displayName = "Flashlight",
        .kind = ri::world::InventoryItemKind::Utility,
        .preferredEquipSlot = ri::world::InventoryEquipSlot::OffHand,
        .unique = true,
    };
}

ri::world::InventoryItemDefinition MakeMedkit() {
    return {
        .id = "medkit",
        .displayName = "Medkit",
        .kind = ri::world::InventoryItemKind::Consumable,
        .preferredEquipSlot = ri::world::InventoryEquipSlot::None,
        .unique = false,
        .healAmount = 50,
    };
}

ri::world::InventoryItemDefinition MakeKeycard() {
    return {
        .id = "level_a_key",
        .displayName = "Level A Key",
        .kind = ri::world::InventoryItemKind::Key,
        .preferredEquipSlot = ri::world::InventoryEquipSlot::None,
        .unique = true,
    };
}

ri::world::InventoryItemDefinition MakeAmmo() {
    return {
        .id = "ammo_box",
        .displayName = "Ammo Box",
        .kind = ri::world::InventoryItemKind::Generic,
        .preferredEquipSlot = ri::world::InventoryEquipSlot::None,
        .unique = false,
    };
}

}

 // namespace

void TestInventoryState() {
    using namespace detail_InventoryState;
    ri::world::InventoryLoadout loadout{};

    const auto flashlightResult = loadout.AddItem(MakeFlashlight());
    Expect(flashlightResult.accepted, "Inventory loadout should accept a valid utility item");
    Expect(loadout.OffHand().has_value() && loadout.OffHand()->id == "flashlight",
           "Inventory loadout should auto-equip off-hand utilities when free");

    const auto duplicateFlashlight = loadout.AddItem(MakeFlashlight());
    Expect(!duplicateFlashlight.accepted,
           "Inventory loadout should reject duplicate unique items");

    const auto medkitResult = loadout.AddItem(MakeMedkit());
    Expect(medkitResult.accepted && medkitResult.changedSlot.has_value()
               && medkitResult.changedSlot->area == ri::world::InventorySlotArea::Hotbar
               && medkitResult.changedSlot->index == 0U,
           "Inventory loadout should route quick-use consumables to the hotbar");

    const auto keyResult = loadout.AddItem(MakeKeycard());
    Expect(keyResult.accepted && keyResult.changedSlot.has_value()
               && keyResult.changedSlot->area == ri::world::InventorySlotArea::Hotbar
               && keyResult.changedSlot->index == 1U,
           "Inventory loadout should keep filling hotbar slots before backpack slots");

    const auto consumeMedkit = loadout.QuickUseSelectedHotbar({.currentHealth = 40, .maxHealth = 100});
    Expect(consumeMedkit.accepted && consumeMedkit.quickUse.has_value()
               && consumeMedkit.quickUse->kind == ri::world::InventoryQuickUseKind::ConsumeHealth
               && consumeMedkit.quickUse->consumesItem
               && consumeMedkit.quickUse->amount == 50,
           "Inventory loadout quick-use should emit a health-consume effect");
    Expect(!loadout.Hotbar()[0].has_value(),
           "Inventory loadout should remove consumed medkits from the hotbar");

    loadout.SetSelectedHotbarSlot(1);
    const auto keyQuickUse = loadout.QuickUseSelectedHotbar({});
    Expect(keyQuickUse.accepted && keyQuickUse.quickUse.has_value()
               && keyQuickUse.quickUse->kind == ri::world::InventoryQuickUseKind::PresentKey
               && !keyQuickUse.quickUse->consumesItem,
           "Inventory loadout quick-use should present keys without consuming them");
    Expect(loadout.Hotbar()[1].has_value() && loadout.Hotbar()[1]->id == "level_a_key",
           "Inventory loadout should keep keys after quick-use");

    loadout.SetSelectedHotbarSlot(0);
    const auto ammoResult = loadout.AddItem(MakeAmmo());
    Expect(ammoResult.accepted,
           "Inventory loadout should accept generic items after quick-use leaves a free slot");
    Expect(loadout.TotalStoredItems() == 3U,
           "Inventory loadout should report total carried items across hotbar, backpack, and off-hand");
    Expect(loadout.CountItemId("ammo_box") == 1U,
           "Inventory loadout should count repeated item ids accurately");
    const auto swapHandsResult = loadout.SwapHands();
    Expect(swapHandsResult.accepted,
           "Inventory loadout should support swapping the selected hotbar item with the off-hand");
    Expect(loadout.OffHand().has_value() && loadout.OffHand()->id == "ammo_box",
           "Inventory loadout should put the selected hotbar item into the off-hand after a swap");
    Expect(loadout.Hotbar()[0].has_value() && loadout.Hotbar()[0]->id == "flashlight",
           "Inventory loadout should return the old off-hand item to the selected hotbar slot");

    const auto stashResult = loadout.StashHotbarSlotToBackpack(0);
    Expect(stashResult.accepted,
           "Inventory loadout should allow stashing a hotbar item into the backpack");
    Expect(loadout.Backpack()[0].has_value() && loadout.Backpack()[0]->id == "flashlight",
           "Inventory loadout should move stashed items into the first backpack slot");

    loadout.SetSelectedHotbarSlot(2);
    const auto moveBackResult = loadout.MoveBackpackSlotToSelectedHotbar(0);
    Expect(moveBackResult.accepted,
           "Inventory loadout should support moving backpack items onto the selected hotbar slot");
    Expect(loadout.Hotbar()[2].has_value() && loadout.Hotbar()[2]->id == "flashlight",
           "Inventory loadout should place backpack items onto the selected hotbar slot");
    const auto keyUseFromBackpack = loadout.QuickUseSlot(
        {.area = ri::world::InventorySlotArea::Hotbar, .index = 1U},
        {}
    );
    Expect(keyUseFromBackpack.accepted && keyUseFromBackpack.quickUse.has_value()
               && keyUseFromBackpack.quickUse->kind == ri::world::InventoryQuickUseKind::PresentKey,
           "Inventory loadout should support direct quick-use from explicit slot refs");

    const ri::world::InventorySnapshot snapshot = loadout.CaptureSnapshot();
    Expect(snapshot.selectedHotbar == 2U,
           "Inventory snapshot should preserve the selected hotbar slot");
    Expect(snapshot.offHandId == "ammo_box",
           "Inventory snapshot should preserve the off-hand item id");
    Expect(snapshot.hotbarSize == 5U && snapshot.backpackSize == 20U && snapshot.allowOffHand,
           "Inventory snapshots should preserve policy-driven storage sizing and off-hand rules");

    const std::unordered_map<std::string, ri::world::InventoryItemDefinition> catalog{
        {"flashlight", MakeFlashlight()},
        {"medkit", MakeMedkit()},
        {"level_a_key", MakeKeycard()},
        {"ammo_box", MakeAmmo()},
    };

    ri::world::InventoryLoadout restored{};
    restored.RestoreSnapshot(snapshot, [&catalog](std::string_view id) -> std::optional<ri::world::InventoryItemDefinition> {
        const auto found = catalog.find(std::string(id));
        if (found == catalog.end()) {
            return std::nullopt;
        }
        return found->second;
    });

    Expect(restored.SelectedHotbarSlot() == 2U,
           "Inventory loadout restore should preserve the selected slot");
    Expect(restored.OffHand().has_value() && restored.OffHand()->id == "ammo_box",
           "Inventory loadout restore should preserve the off-hand item");
    Expect(restored.Hotbar()[2].has_value() && restored.Hotbar()[2]->id == "flashlight",
           "Inventory loadout restore should rebuild hotbar assignments through the resolver");
    Expect(restored.TotalStoredItems() == loadout.TotalStoredItems(),
           "Inventory loadout restore should rebuild the same total number of items");

    ri::world::InventoryLoadout hiddenLoadout{
        ri::world::InventoryPolicy{
            .presentation = ri::world::InventoryPresentationMode::HiddenDataOnly,
            .hotbarSize = 3,
            .backpackSize = 8,
            .allowOffHand = true,
        }
    };
    Expect(hiddenLoadout.IsEnabled() && hiddenLoadout.StoresGameplayItemData() && !hiddenLoadout.IsUiVisible(),
           "Hidden-data inventory should still store gameplay item data while exposing no UI");
    const auto hiddenKey = hiddenLoadout.AddItem(MakeKeycard());
    Expect(hiddenKey.accepted,
           "Hidden-data inventory should still accept collected gameplay items like keys");
    Expect(hiddenLoadout.OccupiedHotbarSlots() == 1U && hiddenLoadout.OccupiedBackpackSlots() == 0U,
           "Hidden-data inventory should still track slot occupancy for runtime systems");
    const auto hiddenSnapshot = hiddenLoadout.CaptureSnapshot();
    Expect(hiddenSnapshot.presentation == ri::world::InventoryPresentationMode::HiddenDataOnly,
           "Inventory snapshots should preserve hidden-data presentation mode");
    ri::world::FlashlightBatteryState battery;
    battery.SetFlashlightEnabled(true);
    battery.Tick(2.0);
    const double drained = battery.Charge01();
    Expect(drained < 1.0,
           "Flashlight battery simulation should drain charge while enabled");
    battery.SetFlashlightEnabled(false);
    battery.Tick(10.0);
    Expect(battery.Charge01() > drained,
           "Flashlight battery simulation should recharge while disabled");
    battery.SetCharge01(0.0);
    battery.SetFlashlightEnabled(true);
    Expect(!battery.IsBeamActive(),
           "Flashlight battery simulation should disable beam output when depleted");

    ri::world::InventoryLoadout disabledLoadout{
        ri::world::InventoryPolicy{
            .presentation = ri::world::InventoryPresentationMode::Disabled,
            .hotbarSize = 4,
            .backpackSize = 10,
            .allowOffHand = false,
        }
    };
    Expect(!disabledLoadout.IsEnabled() && !disabledLoadout.StoresGameplayItemData() && !disabledLoadout.IsUiVisible(),
           "Disabled inventory should opt the creator out of both UI and gameplay item storage");
    const auto disabledAdd = disabledLoadout.AddItem(MakeKeycard());
    Expect(!disabledAdd.accepted,
           "Disabled inventory should reject item storage entirely");

    {
        ri::world::InventoryLoadout possession{};
        Expect(possession.PrimaryHandSlot().area == ri::world::InventorySlotArea::Hotbar
                   && possession.PrimaryHandSlot().index == 0U,
               "Primary hand should default to hotbar slot zero");
        const auto k = possession.AddItem(MakeKeycard());
        Expect(k.accepted && possession.HoldsItemInHands("level_a_key"),
               "Primary hand should count as holding the selected hotbar item");
        possession.SetSelectedHotbarSlot(1);
        Expect(!possession.HoldsItemInHands("level_a_key"),
               "Changing selection should move primary hand away from prior slot");
    }

    {
        ri::world::InventoryLoadout stacked{};
        ri::world::InventoryItemDefinition ammo = MakeAmmo();
        ammo.stackCount = 3;
        Expect(stacked.AddItem(ammo).accepted,
               "Inventory loadout should accept stackable pickups with explicit counts");
        Expect(stacked.CountItemId("ammo_box") == 3U && stacked.OccupiedHotbarSlots() == 1U,
               "Stackable items should merge units into a single occupied slot");
        Expect(stacked.TotalItemUnits() == 3U,
               "Total item units should sum stack counts across storage");
        Expect(stacked.TryInventoryGate("ammo_box", 2, false) && stacked.CountItemId("ammo_box") == 3U,
               "Non-consuming gate checks should not remove items");
        Expect(stacked.TryInventoryGate("ammo_box", 2, true) && stacked.CountItemId("ammo_box") == 1U,
               "Consuming gates should peel quantity using backpack-first removal order");
        ri::logic::InventoryQuery query = stacked.BindInventoryGateQuery("player_one");
        Expect(!query("player_two", std::string("ammo_box"), 1, false),
               "Bound inventory queries should reject mismatched instigators");
        Expect(query("player_one", std::string("ammo_box"), 1, true) && stacked.CountItemId("ammo_box") == 0U,
               "Bound inventory queries should route consume through TryInventoryGate");
    }

    {
        ri::world::InventoryLoadout snapshotStack{};
        ri::world::InventoryItemDefinition bulk = MakeAmmo();
        bulk.stackCount = 5;
        Expect(snapshotStack.AddItem(bulk).accepted,
               "Snapshot restore should preserve stacked quantities");
        const ri::world::InventorySnapshot snap = snapshotStack.CaptureSnapshot();
        Expect(!snap.hotbarCounts.empty() && snap.hotbarCounts[0] == 5,
               "Inventory snapshots should parallel stack counts with slot ids");

        const std::unordered_map<std::string, ri::world::InventoryItemDefinition> ammoCatalog{
            {"ammo_box", MakeAmmo()},
        };

        ri::world::InventoryLoadout stackRestored{};
        stackRestored.RestoreSnapshot(snap, [&ammoCatalog](std::string_view id) -> std::optional<ri::world::InventoryItemDefinition> {
            const auto found = ammoCatalog.find(std::string(id));
            if (found == ammoCatalog.end()) {
                return std::nullopt;
            }
            return found->second;
        });
        Expect(stackRestored.CountItemId("ammo_box") == 5U && stackRestored.TotalItemUnits() == 5U,
               "Restore snapshot should rebuild stacked totals from saved counts");
    }

    {
        ri::world::InventoryLoadout clamped{};
        ri::world::InventoryItemDefinition nearlyFull = MakeAmmo();
        nearlyFull.stackCount = INT_MAX;
        Expect(clamped.AddItem(nearlyFull).accepted,
               "Inventory loadout should accept a stack sized up to INT_MAX");
        ri::world::InventoryItemDefinition extra = MakeAmmo();
        extra.stackCount = 100;
        Expect(clamped.AddItem(extra).accepted,
               "Merging past INT_MAX should clamp rather than overflow signed int");
        Expect(clamped.CountItemId("ammo_box") == static_cast<std::size_t>(INT_MAX),
               "Stack totals should saturate at INT_MAX units");
    }
}

// --- merged from TestLevelFlowPresentationState.cpp ---
namespace detail_LevelFlowPresentationState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestLevelFlowPresentationState() {
    using namespace detail_LevelFlowPresentationState;
    ri::world::LevelFlowPresentationState state({
        .mode = ri::world::LevelFlowPresentationMode::Verbose,
        .showLoadStatus = true,
        .showLevelToast = true,
        .showStoryIntro = true,
        .showCheckpointRestore = true,
        .historyLimit = 4U,
    });

    state.BeginLoad("K-41 Prototype Hall");
    Expect(state.IsLoading() && state.LoadingStatus() == "Loading K-41 Prototype Hall...",
           "Level-flow presentation should report the active load status");

    state.CompleteLoad({
        .levelId = "level1",
        .levelLabel = "K-41 Prototype Hall",
        .storyIntro = "Inspect the annex and secure the keycard.",
        .firstVisit = true,
        .restoredFromCheckpoint = true,
    });
    Expect(!state.IsLoading() && state.LoadingStatus() == "Ready.",
           "Level-flow presentation should mark load completion as ready");
    Expect(state.LocationLabel() == "K-41 Prototype Hall",
           "Level-flow presentation should preserve the active location label");
    Expect(state.ActiveToast().has_value() && state.ActiveToast()->text == "K-41 Prototype Hall",
           "Level-flow presentation should expose a level-name toast when enabled");
    Expect(state.ActiveStoryIntro().has_value()
               && state.ActiveStoryIntro()->text == "Inspect the annex and secure the keycard.",
           "Level-flow presentation should surface story intro text on first visit in verbose mode");
    Expect(state.ActiveCheckpointNotice().has_value()
               && state.ActiveCheckpointNotice()->text == "Checkpoint restored.",
           "Level-flow presentation should expose checkpoint restore callouts when enabled");

    state.Advance(9500.0);
    Expect(!state.ActiveToast().has_value()
               && !state.ActiveStoryIntro().has_value()
               && !state.ActiveCheckpointNotice().has_value(),
           "Level-flow presentation should expire transient callouts over time");

    state.SetPolicy({
        .mode = ri::world::LevelFlowPresentationMode::Disabled,
        .showLoadStatus = true,
        .showLevelToast = true,
        .showStoryIntro = true,
        .showCheckpointRestore = true,
        .historyLimit = 2U,
    });
    state.BeginLoad("Maintenance Deck");
    state.CompleteLoad({
        .levelId = "level2",
        .levelLabel = "Maintenance Deck",
        .storyIntro = "Should remain hidden.",
        .firstVisit = true,
        .restoredFromCheckpoint = true,
    });
    Expect(state.LoadingStatus().empty()
               && !state.ActiveToast().has_value()
               && !state.ActiveStoryIntro().has_value()
               && !state.ActiveCheckpointNotice().has_value()
               && state.History().empty()
               && state.LocationLabel() == "Maintenance Deck",
           "Level-flow presentation should be optional and suppress transient UI while preserving core location data");
}

// --- merged from TestLogicEntityIoTelemetry.cpp ---
namespace detail_LogicEntityIoTelemetry {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string Field(const ri::runtime::RuntimeEvent& event, std::string_view key) {
    const auto found = event.fields.find(std::string(key));
    if (found == event.fields.end()) {
        return {};
    }
    return found->second;
}

}

 // namespace

void TestLogicEntityIoTelemetry() {
    using namespace detail_LogicEntityIoTelemetry;
    using namespace ri::logic;

    LogicGraphSpec spec;
    spec.nodes.push_back(RelayNode{.id = "r1", .def = {}});
    spec.nodes.push_back(RelayNode{.id = "r2", .def = {.startEnabled = true}});
    spec.routes.push_back({.sourceId = "r1",
                           .outputName = "OnTrigger",
                           .targets = {{.targetId = "r2", .inputName = "Trigger"}}});

    ri::runtime::RuntimeEventBus bus;
    std::vector<std::string> kinds;

    ri::runtime::RuntimeEventBus::ListenerId listener = bus.On("entityIo", [&](const ri::runtime::RuntimeEvent& event) {
        kinds.push_back(Field(event, ri::runtime::entity_io::kFieldKind));
    });

    LogicGraph graph(std::move(spec));
    ri::world::EntityIoTracker tracker;

    std::vector<std::string> gameplayLog;
    ri::world::AttachLogicGraphEntityIoTelemetry(graph, bus, &tracker, [&](const LogicOutputEvent& ev) {
        gameplayLog.push_back(ev.sourceId + ":" + ev.outputName);
    });

    LogicContext ctx;
    graph.DispatchInput("r1", "Trigger", ctx);

    Expect(listener != 0U, "entityIo listener should register");
    Expect(gameplayLog.size() == 2U, "Gameplay output handler should observe both relay emissions");
    Expect(kinds.size() == 4U, "Telemetry should emit input+output pairs for the two-hop relay chain");

    std::size_t inputs = 0;
    std::size_t outputs = 0;
    for (const std::string& k : kinds) {
        if (k == std::string(ri::runtime::entity_io::kKindInput)) {
            ++inputs;
        } else if (k == std::string(ri::runtime::entity_io::kKindOutput)) {
            ++outputs;
        }
    }
    Expect(inputs == 2U && outputs == 2U, "Relay chain should record two inputs and two outputs on the bus");

    const ri::world::EntityIoStats& stats = tracker.GetStats();
    Expect(stats.inputsDispatched == 2U && stats.outputsFired == 2U,
           "EntityIoTracker counters should mirror routed inputs and outputs");

    bus.Off("entityIo", listener);
}

// --- merged from TestLogicGraph.cpp ---
namespace detail_LogicGraph {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestLogicGraph() {
    using namespace detail_LogicGraph;
    using namespace ri::logic;

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RelayNode{.id = "r1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "r2", .def = {.startEnabled = true}});
        spec.routes.push_back({.sourceId = "r1",
                               .outputName = "OnTrigger",
                               .targets = {{.targetId = "r2", .inputName = "Trigger"}}});

        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.sourceId + ":" + ev.outputName); });

        LogicContext ctx;
        graph.DispatchInput("r1", "Trigger", ctx);
        Expect(log.size() == 2U, "Relay chain should emit OnTrigger from r1 then r2");
        Expect(log[0] == "r1:ontrigger", "First emission should be r1");
        Expect(log[1] == "r2:ontrigger", "Routed relay should fire");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(TimerNode{.id = "t1", .def = {.intervalMs = 100, .repeating = false, .autoStart = false}});
        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.outputName); });

        graph.DispatchInput("t1", "Start", {});
        graph.AdvanceTime(50);
        Expect(log.empty(), "Timer should not fire before interval");
        graph.AdvanceTime(60);
        Expect(log.size() == 2U, "One-shot timer should emit OnTimer then OnFinished");
        Expect(log[0] == "ontimer" && log[1] == "onfinished", "Timer outputs should normalize");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(CounterNode{
            .id = "c1",
            .def = {.startValue = 0, .minValue = 0, .maxValue = 2, .step = 1},
        });
        spec.nodes.push_back(RelayNode{.id = "rx", .def = {}});
        spec.routes.push_back({.sourceId = "c1",
                               .outputName = "OnHitMax",
                               .targets = {{.targetId = "rx", .inputName = "Trigger"}}});

        int hits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "rx") {
                ++hits;
            }
        });

        graph.DispatchInput("c1", "Increment", {});
        graph.DispatchInput("c1", "Increment", {});
        Expect(hits == 1, "Counter should hit max once and route to relay");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(CompareNode{
            .id = "cmp",
            .def = {.equalsValue = 3, .constantValue = 3, .evaluateOnSpawn = false, .startEnabled = true},
        });
        int become = 0;
        int truth = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.outputName == "onbecometrue") {
                ++become;
            }
            if (ev.outputName == "ontrue") {
                ++truth;
            }
        });
        graph.DispatchInput("cmp", "Evaluate", {});
        Expect(truth == 1, "Compare should emit OnTrue");
        Expect(become == 0, "First evaluation should not emit OnBecomeTrue");
        graph.DispatchInput("cmp", "Evaluate", {});
        Expect(become == 0, "Stable result should not emit become");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RelayNode{.id = "a", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "b", .def = {}});
        spec.routes.push_back({.sourceId = "a",
                               .outputName = "OnTrigger",
                               .targets = {{.targetId = "b", .inputName = "Trigger", .delayMs = 40}}});
        bool got = false;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "b") {
                got = true;
            }
        });
        graph.DispatchInput("a", "Trigger", {});
        Expect(!got, "Delayed edge should not fire immediately");
        graph.AdvanceTime(39);
        Expect(!got, "Delayed edge should respect delayMs");
        graph.AdvanceTime(2);
        Expect(got, "Delayed edge should deliver after clock advance");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(ChannelNode{.id = "pub", .def = {.channelName = "bus", .role = ChannelRole::Publish}});
        spec.nodes.push_back(ChannelNode{.id = "sub", .def = {.channelName = "bus", .role = ChannelRole::Subscribe}});
        int msgs = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sub" && ev.outputName == "onmessage") {
                ++msgs;
            }
        });
        graph.DispatchInput("pub", "Send", {});
        Expect(msgs == 1, "Channel publish should reach subscriber OnMessage");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(ChannelNode{.id = "pubDisabled",
                                         .def = {.channelName = "busDisabled", .role = ChannelRole::Publish}});
        spec.nodes.push_back(ChannelNode{.id = "subDisabled",
                                         .def = {.channelName = "busDisabled",
                                                 .role = ChannelRole::Subscribe,
                                                 .startEnabled = false}});
        int msgs = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "subDisabled" && ev.outputName == "onmessage") {
                ++msgs;
            }
        });
        graph.DispatchInput("pubDisabled", "Send", {});
        Expect(msgs == 0, "Disabled channel subscriber should ignore published messages");
        graph.DispatchInput("subDisabled", "Enable", {});
        graph.DispatchInput("pubDisabled", "Send", {});
        Expect(msgs == 1, "Enabled channel subscriber should receive published messages");
        graph.DispatchInput("subDisabled", "Disable", {});
        graph.DispatchInput("pubDisabled", "Send", {});
        Expect(msgs == 1, "Disabled subscriber should stop receiving further published messages");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(TriggerDetectorNode{
            .id = "det",
            .def = {.oncePerInstigator = true,
                    .cooldownMs = 0,
                    .instigatorFilter = TriggerInstigatorFilter::Player,
                    .requireExitBeforeRetrigger = false},
        });
        int pass = 0;
        int reject = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.outputName == "onpass") {
                ++pass;
            }
            if (ev.outputName == "onreject") {
                ++reject;
            }
        });
        LogicContext npc;
        npc.instigatorId = "n1";
        npc.fields["instigatorKind"] = "npc";
        graph.DispatchInput("det", "Trigger", npc);
        Expect(pass == 0 && reject == 1, "Player-only detector should reject NPC");

        LogicContext p1;
        p1.instigatorId = "p1";
        p1.fields["instigatorKind"] = "player";
        graph.DispatchInput("det", "Trigger", p1);
        Expect(pass == 1 && reject == 1, "First player touch should pass");
        graph.DispatchInput("det", "Trigger", p1);
        Expect(pass == 1 && reject == 2, "Once-per-instigator should reject repeat");
        graph.DispatchInput("det", "Reset", {});
        graph.DispatchInput("det", "Trigger", p1);
        Expect(pass == 2, "Reset should allow another pass");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(CompareNode{
            .id = "cText",
            .def = {.constantText = "go", .evaluateOnSpawn = false, .startEnabled = true},
        });
        int trueCount = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.outputName == "ontrue") {
                ++trueCount;
            }
        });
        graph.DispatchInput("cText", "Evaluate", {});
        Expect(trueCount == 1, "Non-empty constantText should be truthy when no numeric bounds");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(CounterNode{.id = "cnt", .def = {.startValue = 0, .step = 1}});
        spec.nodes.push_back(CompareNode{
            .id = "cObs",
            .def = {.sourceLogicEntityId = "cnt",
                    .sourceProperty = "value",
                    .equalsValue = 2,
                    .evaluateOnSpawn = false,
                    .startEnabled = true},
        });
        int trueCount = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "cObs" && ev.outputName == "ontrue") {
                ++trueCount;
            }
        });
        graph.DispatchInput("cnt", "Increment", {});
        graph.DispatchInput("cnt", "Increment", {});
        graph.DispatchInput("cObs", "Evaluate", {});
        Expect(trueCount == 1, "Compare should read counter value via sourceLogicEntityId");
        Expect(graph.TryGetLogicNumericProperty("cnt", "value").value_or(0) == 2.0, "TryGetLogicNumericProperty should read counter");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(PulseNode{.id = "pul", .def = {.holdMs = 50, .retrigger = PulseRetriggerMode::Ignore}});
        int activeTicks = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.outputName == "onactive") {
                ++activeTicks;
            }
        });
        graph.DispatchInput("pul", "Trigger", {});
        Expect(activeTicks == 1, "Pulse should emit OnActive once on trigger (with OnRise separate)");
        graph.AdvanceTime(25);
        Expect(activeTicks == 2, "Pulse should emit OnActive each clock tick while held");
        graph.AdvanceTime(30);
        Expect(activeTicks == 2, "After hold expires, OnActive should not tick again");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(ChannelNode{.id = "pub2", .def = {.channelName = "b2", .role = ChannelRole::Publish}});
        spec.nodes.push_back(ChannelNode{.id = "sub2", .def = {.channelName = "b2", .role = ChannelRole::Subscribe}});
        std::optional<double> lastParam;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sub2") {
                lastParam = ev.context.parameter;
            }
        });
        LogicContext withParam;
        withParam.parameter = 7.5;
        graph.DispatchInput("pub2", "Send", withParam);
        Expect(lastParam.has_value() && *lastParam == 7.5, "Channel Send should forward parameter");
        graph.DispatchInput("pub2", "Trigger", withParam);
        Expect(!lastParam.has_value(), "Channel Trigger should clear parameter on subscribers");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(SplitNode{.id = "sp", .def = {.branchCount = 2}});
        spec.nodes.push_back(LatchNode{.id = "lat", .def = {.startValue = false, .mode = LatchMode::PrioritySet}});
        spec.routes.push_back({.sourceId = "sp",
                               .outputName = "Branch0",
                               .targets = {{.targetId = "lat", .inputName = "Set"}}});
        spec.routes.push_back({.sourceId = "sp",
                               .outputName = "Branch1",
                               .targets = {{.targetId = "lat", .inputName = "Reset"}}});
        bool latchedTrue = false;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "lat" && ev.outputName == "ontrue") {
                latchedTrue = true;
            }
        });
        graph.DispatchInput("sp", "Trigger", {});
        Expect(latchedTrue, "PrioritySet latch should prefer Set when both arrive in one activation wave");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(TriggerDetectorNode{
            .id = "tagDet",
            .def = {.instigatorFilter = TriggerInstigatorFilter::Tag, .instigatorTag = "vip"},
        });
        int pass = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.outputName == "onpass") {
                ++pass;
            }
        });
        LogicContext noTag;
        noTag.instigatorId = "x";
        graph.DispatchInput("tagDet", "Trigger", noTag);
        Expect(pass == 0, "Tag filter should reject without matching tags field");

        LogicContext tagged;
        tagged.instigatorId = "x";
        tagged.fields[std::string{ri::logic::ports::kFieldTags}] = "ally, vip, crew";
        graph.DispatchInput("tagDet", "Trigger", tagged);
        Expect(pass == 1, "Tag filter should accept comma-separated tags");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(InventoryGateNode{.id = "gate", .def = {.itemId = "key", .consumeOnPass = true}});
        bool sawConsume = false;
        spec.inventoryQuery = [&](std::string_view /*inst*/,
                                  const std::string& item,
                                  int qty,
                                  bool consume) -> bool {
            if (consume) {
                sawConsume = true;
            }
            return item == "key" && qty == 1;
        };
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([](const LogicOutputEvent&) {});
        LogicContext buyer;
        buyer.instigatorId = "p1";
        graph.DispatchInput("gate", "Evaluate", buyer);
        Expect(sawConsume, "Inventory gate should pass consume flag when consumeOnPass is true");
    }

    {
        LogicContext ctx;
        SetAnalogSignal(ctx, 12.0);
        ApplyAnalogAttenuation(ctx, 3.0);
        const std::optional<double> after = TryGetAnalogSignal(ctx);
        Expect(after.has_value() && std::fabs(*after - 9.0) < 1e-9,
               "Attenuation should reduce analog signal along a chain");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RelayNode{.id = "r1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "r2", .def = {}});
        spec.routes.push_back({.sourceId = "r1",
                               .outputName = "OnTrigger",
                               .targets = {{.targetId = "r2", .inputName = "power"}}});
        int onTriggers = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.outputName == "ontrigger") {
                ++onTriggers;
            }
        });
        graph.DispatchInput("r1", "pulse", {});
        Expect(onTriggers == 2, "`power`/`pulse` should behave like `Trigger` on relays");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RelayNode{.id = "r1", .def = {}});
        std::optional<double> seen;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { seen = TryGetAnalogSignal(ev.context); });
        graph.DispatchInput("r1", "Trigger", {});
        Expect(seen.has_value() && std::fabs(*seen - 1.0) < 1e-9,
               "Relays should stamp a default analog level when none was authored");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RelayNode{.id = "r1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "r2", .def = {}});
        spec.routes.push_back({.sourceId = "r1",
                               .outputName = "OnTrigger",
                               .targets = {{.targetId = "r2", .inputName = "Trigger"}}});
        std::optional<double> atR2;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "r2") {
                atR2 = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext ctx;
        SetAnalogSignal(ctx, 7.0);
        graph.DispatchInput("r1", "Trigger", ctx);
        Expect(atR2.has_value() && std::fabs(*atR2 - 7.0) < 1e-9, "Relay chains should preserve authored analog levels");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(LatchNode{.id = "mem", .def = {.startValue = true, .mode = LatchMode::Sr}});
        spec.nodes.push_back(RelayNode{.id = "bus", .def = {}});
        LogicGraph graph(std::move(spec));
        const std::vector<LogicCircuitNodeProbe> probes = graph.ProbeCircuitNodes();
        Expect(probes.size() == 2U, "Probe should visit every node");
        const LogicCircuitNodeProbe* latchProbe = nullptr;
        for (const LogicCircuitNodeProbe& p : probes) {
            if (p.id == "mem") {
                latchProbe = &p;
            }
        }
        Expect(latchProbe != nullptr && latchProbe->powered && std::fabs(latchProbe->signalStrength - 1.0) < 1e-9
                   && latchProbe->kind == "latch",
               "Probe should report latched-on memory as fully powered");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(GateAndNode{.id = "and1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sink", .def = {}});
        spec.routes.push_back({.sourceId = "and1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sink", .inputName = "Trigger"}}});
        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.sourceId + ":" + ev.outputName); });
        LogicContext ctx;
        graph.DispatchInput("and1", "A", ctx);
        Expect(log.empty(), "gate_and should not fire after only A");
        graph.DispatchInput("and1", "B", ctx);
        Expect(log.size() == 2U, "gate_and should route Out to sink after A then B");
        graph.DispatchInput("and1", "A", ctx);
        Expect(log.size() == 2U, "gate_and should require both arms again after firing");
        graph.DispatchInput("and1", "B", ctx);
        Expect(log.size() == 4U, "gate_and should fire again after B following a new A");
        graph.DispatchInput("and1", "B", ctx);
        graph.DispatchInput("and1", "A", ctx);
        Expect(log.size() == 6U, "gate_and should accept B-then-A ordering");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(GateOrNode{.id = "or1", .def = {}});
        int count = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "or1") {
                ++count;
            }
        });
        graph.DispatchInput("or1", "A", {});
        Expect(count == 1, "gate_or should fire on A");
        graph.DispatchInput("or1", "B", {});
        Expect(count == 2, "gate_or should fire on B");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(GateNotNode{.id = "n1", .def = {}});
        std::optional<double> outSig;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "n1") {
                outSig = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext hi;
        SetAnalogSignal(hi, 1.0);
        graph.DispatchInput("n1", "In", hi);
        Expect(outSig.has_value() && *outSig < 0.5, "gate_not should flip high to low");
        LogicContext lo;
        SetAnalogSignal(lo, 0.0);
        graph.DispatchInput("n1", "In", lo);
        Expect(outSig.has_value() && *outSig > 0.5, "gate_not should flip low to high");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(FlowRelayNode{.id = "fr1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sink", .def = {}});
        spec.routes.push_back({.sourceId = "fr1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sink", .inputName = "Trigger"}}});
        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.sourceId + ":" + ev.outputName); });
        graph.DispatchInput("fr1", "Trig", {});
        Expect(log.size() == 2U, "flow_relay Trig should emit Out when enabled");
        graph.DispatchInput("fr1", "Dis", {});
        graph.DispatchInput("fr1", "Trig", {});
        Expect(log.size() == 2U, "flow_relay should ignore Trig while disabled");
        graph.DispatchInput("fr1", "En", {});
        graph.DispatchInput("fr1", "Trig", {});
        Expect(log.size() == 4U, "flow_relay should conduct again after En");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(IoButtonNode{.id = "btn1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkBtn", .def = {}});
        spec.routes.push_back({.sourceId = "btn1",
                               .outputName = "Press",
                               .targets = {{.targetId = "sinkBtn", .inputName = "Trigger"}}});
        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.sourceId + ":" + ev.outputName); });
        graph.DispatchInput("btn1", "Press", {});
        Expect(log.size() == 2U, "io_button Press input should emit Press output and route");
        graph.DispatchInput("btn1", "Release", {});
        Expect(log.size() == 3U, "io_button should emit Release output");
        graph.DispatchInput("btn1", "Press", {});
        Expect(log.size() == 5U, "second press should emit Press and route again");
        graph.DispatchInput("btn1", "Disable", {});
        Expect(log.size() == 6U, "disable while held should emit Release");
        graph.DispatchInput("btn1", "Press", {});
        Expect(log.size() == 6U, "io_button should ignore press while disabled");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(IoKeypadNode{.id = "kp", .def = {.startEnabled = false}});
        std::size_t valFires = 0;
        std::string lastEnterStr;
        std::optional<double> lastEnterParam;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "kp") {
                return;
            }
            if (ev.outputName == "val") {
                ++valFires;
            }
            if (ev.outputName == "enter") {
                const auto it = ev.context.fields.find("value");
                lastEnterStr = it != ev.context.fields.end() ? it->second : std::string{};
                lastEnterParam = ev.context.parameter;
            }
        });
        graph.DispatchInput("kp", "1", {});
        Expect(valFires == 0U, "io_keypad should ignore digit keys until enabled");
        graph.DispatchInput("kp", "Enable", {});
        graph.DispatchInput("kp", "3", {});
        LogicContext d5;
        d5.parameter = 5;
        graph.DispatchInput("kp", "Digit", d5);
        Expect(valFires == 2U, "io_keypad should emit Val after each digit");
        graph.DispatchInput("kp", "Reset", {});
        Expect(valFires == 3U, "io_keypad Reset should emit Val");
        graph.DispatchInput("kp", "4", {});
        graph.DispatchInput("kp", "2", {});
        graph.DispatchInput("kp", "Enter", {});
        Expect(valFires == 5U, "two digits after reset should yield two Val pulses");
        Expect(lastEnterStr == "42", "Enter should carry the full entry string");
        Expect(lastEnterParam.has_value() && std::fabs(*lastEnterParam - 42.0) < 1e-9,
               "Enter should expose a numeric parameter for all-digit codes");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(IoDisplayNode{.id = "lcd", .def = {}});
        std::size_t doneCount = 0;
        std::string lastText;
        std::string lastColor;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "lcd" || ev.outputName != "done") {
                return;
            }
            ++doneCount;
            const auto itT = ev.context.fields.find("text");
            lastText = itT != ev.context.fields.end() ? itT->second : std::string{};
            const auto itC = ev.context.fields.find("color");
            lastColor = itC != ev.context.fields.end() ? itC->second : std::string{};
        });
        LogicContext t1;
        t1.fields["text"] = "hello";
        graph.DispatchInput("lcd", "SetText", t1);
        Expect(doneCount == 1U && lastText == "hello" && lastColor.empty(),
               "io_display SetText should emit Done with text payload");
        LogicContext c1;
        c1.fields["color"] = "#aabbcc";
        graph.DispatchInput("lcd", "SetColor", c1);
        Expect(doneCount == 2U && lastText == "hello" && lastColor == "#aabbcc",
               "io_display SetColor should preserve text and set color");
        LogicContext t2;
        t2.fields["value"] = "99";
        graph.DispatchInput("lcd", "SetText", t2);
        Expect(doneCount == 3U && lastText == "99", "io_display SetText should accept value field as text source");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(IoAudioNode{.id = "snd", .def = {}});
        std::size_t done = 0;
        bool lastPlaying = false;
        double lastVol = -1.0;
        std::string lastClip;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "snd" || ev.outputName != "done") {
                return;
            }
            ++done;
            const auto itP = ev.context.fields.find("playing");
            lastPlaying = itP != ev.context.fields.end() && itP->second == "true";
            lastVol = ev.context.parameter.value_or(-1.0);
            const auto itC = ev.context.fields.find("clip");
            lastClip = itC != ev.context.fields.end() ? itC->second : std::string{};
        });
        LogicContext p1;
        p1.fields["clip"] = "footsteps";
        graph.DispatchInput("snd", "Play", p1);
        Expect(done == 1U && lastPlaying && lastClip == "footsteps", "io_audio Play should emit Done with clip id");
        LogicContext vol;
        vol.parameter = 0.25;
        graph.DispatchInput("snd", "SetVol", vol);
        Expect(done == 2U && std::fabs(lastVol - 0.25) < 1e-9, "io_audio SetVol should update volume on Done");
        graph.DispatchInput("snd", "Stop", {});
        Expect(done == 3U && !lastPlaying, "io_audio Stop should report playing=false on Done");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(IoLoggerNode{.id = "logz", .def = {}});
        std::size_t graphOutputs = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "logz") {
                ++graphOutputs;
            }
        });
        LogicContext line;
        line.fields["message"] = "hello logger";
        graph.DispatchInput("logz", "Log", line);
        Expect(graphOutputs == 0U, "io_logger should not emit logic graph outputs");
        std::string detailAfterLog;
        std::string kindAfterLog;
        for (const LogicCircuitNodeProbe& p : graph.ProbeCircuitNodes()) {
            if (p.id == "logz") {
                detailAfterLog = p.detail;
                kindAfterLog = p.kind;
                break;
            }
        }
        Expect(kindAfterLog == "io_logger" && detailAfterLog.find("hello") != std::string::npos,
               "io_logger probe should reflect last log line");
        LogicContext err;
        err.fields["text"] = "boom";
        graph.DispatchInput("logz", "Err", err);
        double strengthAfterErr = 0.0;
        for (const LogicCircuitNodeProbe& p : graph.ProbeCircuitNodes()) {
            if (p.id == "logz") {
                strengthAfterErr = p.signalStrength;
                break;
            }
        }
        Expect(std::fabs(strengthAfterErr - 1.0) < 1e-9, "io_logger Err should drive probe signal strength to max");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(IoTriggerNode{.id = "tr", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkTr", .def = {}});
        spec.routes.push_back({.sourceId = "tr",
                               .outputName = "Touch",
                               .targets = {{.targetId = "sinkTr", .inputName = "Trigger"}}});
        spec.routes.push_back({.sourceId = "tr",
                               .outputName = "Untouch",
                               .targets = {{.targetId = "sinkTr", .inputName = "Trigger"}}});
        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.sourceId + ":" + ev.outputName); });
        graph.DispatchInput("tr", "Touch", {});
        Expect(log.empty(), "io_trigger should ignore touch while disarmed");
        graph.DispatchInput("tr", "Arm", {});
        graph.DispatchInput("tr", "Touch", {});
        Expect(log.size() == 2U, "armed io_trigger should emit Touch and route");
        graph.DispatchInput("tr", "Untouch", {});
        Expect(log.size() == 4U, "io_trigger should emit Untouch and route");
        graph.DispatchInput("tr", "Disarm", {});
        graph.DispatchInput("tr", "Touch", {});
        Expect(log.size() == 4U, "disarmed io_trigger should ignore touch");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(IoTriggerNode{.id = "tr2", .def = {.startArmed = true}});
        std::vector<std::string> outs;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "tr2") {
                outs.push_back(ev.outputName);
            }
        });
        graph.DispatchInput("tr2", "Touch", {});
        Expect(outs.size() == 1U && outs[0] == "touch", "io_trigger startArmed should accept touch without Arm");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(GateBufNode{.id = "gbuf", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkBuf", .def = {}});
        spec.routes.push_back({.sourceId = "gbuf",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkBuf", .inputName = "Trigger"}}});
        std::optional<double> atBuf;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "gbuf") {
                atBuf = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext c;
        c.analogSignal = 0.7;
        graph.DispatchInput("gbuf", "In", c);
        Expect(atBuf.has_value() && std::fabs(*atBuf - 0.7) < 1e-9, "gate_buf should pass analog through unchanged");
        double probeStr = 0.0;
        for (const LogicCircuitNodeProbe& p : graph.ProbeCircuitNodes()) {
            if (p.id == "gbuf") {
                probeStr = p.signalStrength;
                break;
            }
        }
        Expect(std::fabs(probeStr - 0.7) < 1e-9, "gate_buf probe should reflect last analog level");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(GateXnorNode{.id = "xn1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkXn", .def = {}});
        spec.routes.push_back({.sourceId = "xn1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkXn", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "xn1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext hi;
        hi.analogSignal = 1.0;
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("xn1", "A", hi);
        graph.DispatchInput("xn1", "B", hi);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9,
               "gate_xnor should output high when both arms are high");
        lastOut.reset();
        graph.DispatchInput("xn1", "A", hi);
        graph.DispatchInput("xn1", "B", lo);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 0.0) < 1e-9,
               "gate_xnor should output low when arms disagree");
        lastOut.reset();
        graph.DispatchInput("xn1", "A", lo);
        graph.DispatchInput("xn1", "B", lo);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9,
               "gate_xnor should output high when both arms are low");
        lastOut.reset();
        graph.DispatchInput("xn1", "B", hi);
        graph.DispatchInput("xn1", "A", hi);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9,
               "gate_xnor should accept B-then-A ordering");
        std::string xnDetail;
        for (const LogicCircuitNodeProbe& p : graph.ProbeCircuitNodes()) {
            if (p.id == "xn1") {
                xnDetail = p.detail;
                break;
            }
        }
        Expect(xnDetail == "waiting",
               "gate_xnor probe should show waiting after a coincident fire clears arms (same as gate_and)");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(GateXorNode{.id = "xr1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkXr", .def = {}});
        spec.routes.push_back({.sourceId = "xr1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkXr", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "xr1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext hi;
        hi.analogSignal = 1.0;
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("xr1", "A", hi);
        graph.DispatchInput("xr1", "B", hi);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 0.0) < 1e-9,
               "gate_xor should output low when both arms are high");
        lastOut.reset();
        graph.DispatchInput("xr1", "A", hi);
        graph.DispatchInput("xr1", "B", lo);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9,
               "gate_xor should output high when arms disagree");
        lastOut.reset();
        graph.DispatchInput("xr1", "A", lo);
        graph.DispatchInput("xr1", "B", lo);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 0.0) < 1e-9,
               "gate_xor should output low when both arms are low");
        lastOut.reset();
        graph.DispatchInput("xr1", "B", lo);
        graph.DispatchInput("xr1", "A", hi);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9,
               "gate_xor should accept B-then-A ordering for disagreeing arms");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(GateNandNode{.id = "nd1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkNd", .def = {}});
        spec.routes.push_back({.sourceId = "nd1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkNd", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "nd1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext hi;
        hi.analogSignal = 1.0;
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("nd1", "A", hi);
        graph.DispatchInput("nd1", "B", hi);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 0.0) < 1e-9, "gate_nand should be low when both arms are high");
        lastOut.reset();
        graph.DispatchInput("nd1", "A", hi);
        graph.DispatchInput("nd1", "B", lo);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9, "gate_nand should be high unless both arms are high");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(GateNorNode{.id = "nr1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkNr", .def = {}});
        spec.routes.push_back({.sourceId = "nr1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkNr", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "nr1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext hi;
        hi.analogSignal = 1.0;
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("nr1", "A", lo);
        graph.DispatchInput("nr1", "B", lo);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9, "gate_nor should be high when both arms are low");
        lastOut.reset();
        graph.DispatchInput("nr1", "A", hi);
        graph.DispatchInput("nr1", "B", lo);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 0.0) < 1e-9, "gate_nor should be low when any arm is high");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathAbsNode{.id = "abs1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkAbs", .def = {}});
        spec.routes.push_back({.sourceId = "abs1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkAbs", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "abs1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext c;
        c.analogSignal = -3.5;
        graph.DispatchInput("abs1", "In", c);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 3.5) < 1e-9, "math_abs should emit absolute value");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathMinNode{.id = "mn1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkMn", .def = {}});
        spec.routes.push_back({.sourceId = "mn1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkMn", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "mn1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext a;
        a.analogSignal = 5.0;
        LogicContext b;
        b.analogSignal = 2.0;
        graph.DispatchInput("mn1", "A", a);
        graph.DispatchInput("mn1", "B", b);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 2.0) < 1e-9, "math_min should emit min after coincident A and B");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathMaxNode{.id = "mx1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkMx", .def = {}});
        spec.routes.push_back({.sourceId = "mx1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkMx", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "mx1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext a;
        a.analogSignal = 5.0;
        LogicContext b;
        b.analogSignal = 2.0;
        graph.DispatchInput("mx1", "A", a);
        graph.DispatchInput("mx1", "B", b);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 5.0) < 1e-9, "math_max should emit max after coincident A and B");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathClampNode{.id = "cl1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkCl", .def = {}});
        spec.routes.push_back({.sourceId = "cl1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkCl", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "cl1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext v;
        v.analogSignal = 10.0;
        LogicContext lo;
        lo.analogSignal = 0.0;
        LogicContext hi;
        hi.analogSignal = 1.0;
        graph.DispatchInput("cl1", "Val", v);
        graph.DispatchInput("cl1", "Lo", lo);
        graph.DispatchInput("cl1", "Hi", hi);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9, "math_clamp should clamp value to [lo,hi]");
        lastOut.reset();
        LogicContext v2;
        v2.analogSignal = 3.0;
        LogicContext lo2;
        lo2.analogSignal = 5.0;
        LogicContext hi2;
        hi2.analogSignal = 2.0;
        graph.DispatchInput("cl1", "Val", v2);
        graph.DispatchInput("cl1", "Lo", lo2);
        graph.DispatchInput("cl1", "Hi", hi2);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 3.0) < 1e-9,
               "math_clamp should treat lo/hi as unordered endpoints");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathRoundNode{.id = "rd1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkRd", .def = {}});
        spec.routes.push_back({.sourceId = "rd1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkRd", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "rd1") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext c;
        c.analogSignal = 2.6;
        graph.DispatchInput("rd1", "In", c);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 3.0) < 1e-9, "math_round should round to nearest integer");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RouteTeeNode{.id = "tee1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkTeeA", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkTeeB", .def = {}});
        spec.routes.push_back({.sourceId = "tee1",
                               .outputName = "A",
                               .targets = {{.targetId = "sinkTeeA", .inputName = "Trigger"}}});
        spec.routes.push_back({.sourceId = "tee1",
                               .outputName = "B",
                               .targets = {{.targetId = "sinkTeeB", .inputName = "Trigger"}}});
        std::vector<std::string> hits;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sinkTeeA" || ev.sourceId == "sinkTeeB") {
                hits.push_back(ev.sourceId);
            }
        });
        graph.DispatchInput("tee1", "In", {});
        Expect(hits.size() == 2U, "route_tee should fan In to both A and B outputs");
        graph.DispatchInput("tee1", "Disable", {});
        hits.clear();
        graph.DispatchInput("tee1", "In", {});
        Expect(hits.empty(), "route_tee should ignore In while disabled");
        graph.DispatchInput("tee1", "Enable", {});
        graph.DispatchInput("tee1", "In", {});
        Expect(hits.size() == 2U, "route_tee should accept In again after Enable");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathLerpNode{.id = "lrp", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkLerp", .def = {}});
        spec.routes.push_back({.sourceId = "lrp",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkLerp", .inputName = "Trigger"}}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "lrp") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext a;
        a.analogSignal = 0.0;
        LogicContext b;
        b.analogSignal = 10.0;
        LogicContext t;
        t.analogSignal = 0.25;
        graph.DispatchInput("lrp", "A", a);
        graph.DispatchInput("lrp", "B", b);
        graph.DispatchInput("lrp", "T", t);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 2.5) < 1e-9,
               "math_lerp should interpolate with T clamped to [0,1]");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathSignNode{.id = "sgn", .def = {}});
        LogicGraph graph(std::move(spec));
        std::vector<std::string> outs;
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sgn") {
                outs.push_back(ev.outputName);
            }
        });
        LogicContext pos;
        pos.analogSignal = 3.0;
        graph.DispatchInput("sgn", "In", pos);
        Expect(outs.size() == 1U && outs[0] == "sign", "math_sign should emit Sign for non-zero input");
        outs.clear();
        LogicContext zero;
        zero.analogSignal = 0.0;
        graph.DispatchInput("sgn", "In", zero);
        Expect(outs.size() == 2U && outs[0] == "sign" && outs[1] == "zero",
               "math_sign should emit Sign then Zero for near-zero input");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RoutePassNode{.id = "rp1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkRp", .def = {}});
        spec.routes.push_back({.sourceId = "rp1",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkRp", .inputName = "Trigger"}}});
        int hits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sinkRp") {
                ++hits;
            }
        });
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("rp1", "En", lo);
        graph.DispatchInput("rp1", "In", {});
        Expect(hits == 0, "route_pass should block In when En is low");
        LogicContext hi;
        hi.analogSignal = 1.0;
        graph.DispatchInput("rp1", "En", hi);
        graph.DispatchInput("rp1", "In", {});
        Expect(hits == 1, "route_pass should pass In to Out when En is high");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RouteMuxNode{.id = "mxu", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkMux", .def = {}});
        spec.routes.push_back({.sourceId = "mxu",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkMux", .inputName = "Trigger"}}});
        int hits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sinkMux") {
                ++hits;
            }
        });
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("mxu", "Sel", lo);
        graph.DispatchInput("mxu", "A", {});
        Expect(hits == 1, "route_mux should forward A when Sel is low");
        graph.DispatchInput("mxu", "B", {});
        Expect(hits == 1, "route_mux should ignore B when Sel is low");
        LogicContext hi;
        hi.analogSignal = 1.0;
        graph.DispatchInput("mxu", "Sel", hi);
        graph.DispatchInput("mxu", "B", {});
        Expect(hits == 2, "route_mux should forward B when Sel is high");
        graph.DispatchInput("mxu", "A", {});
        Expect(hits == 2, "route_mux should ignore A when Sel is high");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RouteDemuxNode{.id = "dmx", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkDmA", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkDmB", .def = {}});
        spec.routes.push_back({.sourceId = "dmx",
                               .outputName = "A",
                               .targets = {{.targetId = "sinkDmA", .inputName = "Trigger"}}});
        spec.routes.push_back({.sourceId = "dmx",
                               .outputName = "B",
                               .targets = {{.targetId = "sinkDmB", .inputName = "Trigger"}}});
        std::vector<std::string> hits;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sinkDmA" || ev.sourceId == "sinkDmB") {
                hits.push_back(ev.sourceId);
            }
        });
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("dmx", "Sel", lo);
        graph.DispatchInput("dmx", "In", {});
        Expect(hits.size() == 1U && hits[0] == "sinkDmA", "route_demux should route In to A when Sel is low");
        hits.clear();
        LogicContext hi;
        hi.analogSignal = 1.0;
        graph.DispatchInput("dmx", "Sel", hi);
        graph.DispatchInput("dmx", "In", {});
        Expect(hits.size() == 1U && hits[0] == "sinkDmB", "route_demux should route In to B when Sel is high");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathAddNode{.id = "madd", .def = {}});
        std::optional<double> sum;
        std::optional<double> carry;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "madd") {
                return;
            }
            if (ev.outputName == "sum") {
                sum = TryGetAnalogSignal(ev.context);
            } else if (ev.outputName == "carry") {
                carry = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext a;
        a.analogSignal = 0.3;
        LogicContext b;
        b.analogSignal = 0.4;
        graph.DispatchInput("madd", "A", a);
        graph.DispatchInput("madd", "B", b);
        Expect(sum.has_value() && std::fabs(*sum - 0.7) < 1e-9 && carry.has_value() && std::fabs(*carry - 0.0) < 1e-9,
               "math_add Sum should be analog sum; Carry low when both arms are not high");
        sum.reset();
        carry.reset();
        LogicContext a2;
        a2.analogSignal = 1.0;
        LogicContext b2;
        b2.analogSignal = 1.0;
        graph.DispatchInput("madd", "A", a2);
        graph.DispatchInput("madd", "B", b2);
        Expect(sum.has_value() && std::fabs(*sum - 2.0) < 1e-9 && carry.has_value() && std::fabs(*carry - 1.0) < 1e-9,
               "math_add Carry should be high when both operands read as logic high");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathSubNode{.id = "msub", .def = {}});
        std::optional<double> diff;
        std::optional<double> borrow;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "msub") {
                return;
            }
            if (ev.outputName == "diff") {
                diff = TryGetAnalogSignal(ev.context);
            } else if (ev.outputName == "borrow") {
                borrow = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext a;
        a.analogSignal = 1.0;
        LogicContext b;
        b.analogSignal = 3.0;
        graph.DispatchInput("msub", "A", a);
        graph.DispatchInput("msub", "B", b);
        Expect(diff.has_value() && std::fabs(*diff - (-2.0)) < 1e-9 && borrow.has_value() && std::fabs(*borrow - 1.0) < 1e-9,
               "math_sub should emit Diff and Borrow when A < B");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathMultNode{.id = "mmul", .def = {}});
        std::optional<double> prod;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "mmul" && ev.outputName == "prod") {
                prod = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext a;
        a.analogSignal = 2.5;
        LogicContext b;
        b.analogSignal = 4.0;
        graph.DispatchInput("mmul", "A", a);
        graph.DispatchInput("mmul", "B", b);
        Expect(prod.has_value() && std::fabs(*prod - 10.0) < 1e-9, "math_mult should emit product after A and B");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathDivNode{.id = "mdiv", .def = {}});
        std::optional<double> quot;
        std::optional<double> rem;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "mdiv") {
                return;
            }
            if (ev.outputName == "quot") {
                quot = TryGetAnalogSignal(ev.context);
            } else if (ev.outputName == "rem") {
                rem = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext a;
        a.analogSignal = 7.0;
        LogicContext b;
        b.analogSignal = 3.0;
        graph.DispatchInput("mdiv", "A", a);
        graph.DispatchInput("mdiv", "B", b);
        Expect(quot.has_value() && std::fabs(*quot - 2.0) < 1e-9 && rem.has_value() && std::fabs(*rem - 1.0) < 1e-9,
               "math_div Quot/Rem should follow truncating division");
        quot.reset();
        rem.reset();
        LogicContext z;
        z.analogSignal = 0.0;
        graph.DispatchInput("mdiv", "A", a);
        graph.DispatchInput("mdiv", "B", z);
        Expect(quot.has_value() && std::fabs(*quot - 0.0) < 1e-9 && rem.has_value() && std::fabs(*rem - 0.0) < 1e-9,
               "math_div should emit zeros when divisor is near zero");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathModNode{.id = "mmod", .def = {}});
        std::optional<double> modRem;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "mmod" && ev.outputName == "rem") {
                modRem = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext v;
        v.analogSignal = 10.0;
        LogicContext m;
        m.analogSignal = 3.0;
        graph.DispatchInput("mmod", "Val", v);
        graph.DispatchInput("mmod", "Mod", m);
        Expect(modRem.has_value() && std::fabs(*modRem - 1.0) < 1e-9, "math_mod should emit fmod(Val, Mod)");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MathCompareNode{.id = "cmp", .def = {}});
        std::string which;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "cmp") {
                which = ev.outputName;
            }
        });
        LogicContext a;
        a.analogSignal = 5.0;
        LogicContext b;
        b.analogSignal = 3.0;
        graph.DispatchInput("cmp", "A", a);
        graph.DispatchInput("cmp", "B", b);
        Expect(which == "a>b", "math_compare should emit A>B branch when A exceeds B");
        which.clear();
        LogicContext eqa;
        eqa.analogSignal = 2.0;
        LogicContext eqb;
        eqb.analogSignal = 2.0;
        graph.DispatchInput("cmp", "A", eqa);
        graph.DispatchInput("cmp", "B", eqb);
        Expect(which == "a==b", "math_compare should emit A==B when operands are nearly equal");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RouteSelectNode{.id = "rsel", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkSel", .def = {}});
        spec.routes.push_back({.sourceId = "rsel",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkSel", .inputName = "Trigger"}}});
        int hits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sinkSel") {
                ++hits;
            }
        });
        LogicContext sel1;
        sel1.analogSignal = 1.0;
        graph.DispatchInput("rsel", "Sel", sel1);
        graph.DispatchInput("rsel", "In0", {});
        Expect(hits == 0, "route_select should ignore In0 when Sel floor maps to index 1");
        graph.DispatchInput("rsel", "In1", {});
        Expect(hits == 1, "route_select should forward In1 when Sel is 1");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RouteMergeNode{.id = "mrg", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkMrg", .def = {}});
        spec.routes.push_back({.sourceId = "mrg",
                               .outputName = "Out",
                               .targets = {{.targetId = "sinkMrg", .inputName = "Trigger"}}});
        int hits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sinkMrg") {
                ++hits;
            }
        });
        graph.DispatchInput("mrg", "In_1", {});
        graph.DispatchInput("mrg", "In_2", {});
        graph.DispatchInput("mrg", "In_3", {});
        Expect(hits == 1, "route_merge should emit Out after coincident In_1 In_2 In_3");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RouteUnpackNode{.id = "unp", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "u0", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "u1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "u2", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "u3", .def = {}});
        spec.routes.push_back({.sourceId = "unp", .outputName = "B0", .targets = {{.targetId = "u0", .inputName = "Trigger"}}});
        spec.routes.push_back({.sourceId = "unp", .outputName = "B1", .targets = {{.targetId = "u1", .inputName = "Trigger"}}});
        spec.routes.push_back({.sourceId = "unp", .outputName = "B2", .targets = {{.targetId = "u2", .inputName = "Trigger"}}});
        spec.routes.push_back({.sourceId = "unp", .outputName = "B3", .targets = {{.targetId = "u3", .inputName = "Trigger"}}});
        int hits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "u0" || ev.sourceId == "u1" || ev.sourceId == "u2" || ev.sourceId == "u3") {
                ++hits;
            }
        });
        graph.DispatchInput("unp", "BusIn", {});
        Expect(hits == 4, "route_unpack should fan BusIn to B0 through B3");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(FlowDoOnceNode{.id = "once", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "sinkOnce", .def = {}});
        spec.routes.push_back({.sourceId = "once",
                               .outputName = "Fired",
                               .targets = {{.targetId = "sinkOnce", .inputName = "Trigger"}}});
        int hits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "sinkOnce") {
                ++hits;
            }
        });
        graph.DispatchInput("once", "Trigger", {});
        graph.DispatchInput("once", "Trigger", {});
        Expect(hits == 1, "flow_do_once should only fire Fired once until Reset");
        graph.DispatchInput("once", "Reset", {});
        graph.DispatchInput("once", "Trigger", {});
        Expect(hits == 2, "flow_do_once should accept a new Trigger after Reset");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RoutePackNode{.id = "pk", .def = {}});
        std::optional<double> busAnalog;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "pk" && ev.outputName == "busout") {
                busAnalog = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext c1;
        c1.analogSignal = 1.0;
        LogicContext c2;
        c2.analogSignal = 2.0;
        LogicContext c3;
        c3.analogSignal = 3.0;
        LogicContext c4;
        c4.analogSignal = 4.0;
        graph.DispatchInput("pk", "B0", c1);
        graph.DispatchInput("pk", "B1", c2);
        graph.DispatchInput("pk", "B2", c3);
        graph.DispatchInput("pk", "B3", c4);
        Expect(busAnalog.has_value() && std::fabs(*busAnalog - 10.0) < 1e-9,
               "route_pack should emit BusOut with analog sum after coincident B0..B3");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MemEdgeNode{.id = "edge", .def = {}});
        int rises = 0;
        int falls = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "edge") {
                return;
            }
            if (ev.outputName == "rise") {
                ++rises;
            } else if (ev.outputName == "fall") {
                ++falls;
            }
        });
        LogicContext lo;
        lo.analogSignal = 0.0;
        LogicContext hi;
        hi.analogSignal = 1.0;
        graph.DispatchInput("edge", "Sig", lo);
        graph.DispatchInput("edge", "Sig", hi);
        Expect(rises == 1 && falls == 0, "mem_edge should emit Rise on low-to-high transition");
        graph.DispatchInput("edge", "Sig", lo);
        Expect(rises == 1 && falls == 1, "mem_edge should emit Fall on high-to-low transition");
        graph.DispatchInput("edge", "Reset", {});
        graph.DispatchInput("edge", "Sig", hi);
        Expect(rises == 1 && falls == 1, "mem_edge Reset should drop history so the next Sig establishes baseline");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(FlowRandomNode{.id = "rnd", .def = {}});
        std::optional<double> val;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "rnd" && ev.outputName == "val") {
                val = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext trig;
        trig.fields["min"] = "7";
        trig.fields["max"] = "7";
        graph.DispatchInput("rnd", "Trigger", trig);
        Expect(val.has_value() && std::fabs(*val - 7.0) < 1e-9,
               "flow_random Val should equal min when min and max coincide");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(RouteSplitNode{.id = "rsp", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "s0", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "s1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "s2", .def = {}});
        spec.routes.push_back({.sourceId = "rsp",
                               .outputName = "Out_1",
                               .targets = {{.targetId = "s0", .inputName = "Trigger"}}});
        spec.routes.push_back({.sourceId = "rsp",
                               .outputName = "Out_2",
                               .targets = {{.targetId = "s1", .inputName = "Trigger"}}});
        spec.routes.push_back({.sourceId = "rsp",
                               .outputName = "Out_3",
                               .targets = {{.targetId = "s2", .inputName = "Trigger"}}});
        int hits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "s0" || ev.sourceId == "s1" || ev.sourceId == "s2") {
                ++hits;
            }
        });
        graph.DispatchInput("rsp", "In", {});
        Expect(hits == 3, "route_split should fan In to Out_1 Out_2 Out_3");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(FlowRiseNode{.id = "fr", .def = {}});
        int pulses = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "fr" && ev.outputName == "pulse") {
                ++pulses;
            }
        });
        LogicContext armOn;
        armOn.analogSignal = 1.0;
        LogicContext armOff;
        armOff.analogSignal = 0.0;
        LogicContext lo;
        lo.analogSignal = 0.0;
        LogicContext hi;
        hi.analogSignal = 1.0;
        graph.DispatchInput("fr", "Arm", armOn);
        graph.DispatchInput("fr", "In", lo);
        graph.DispatchInput("fr", "In", hi);
        Expect(pulses == 1, "flow_rise should pulse once on first rising edge while armed");
        graph.DispatchInput("fr", "In", lo);
        graph.DispatchInput("fr", "In", hi);
        Expect(pulses == 2, "flow_rise should pulse again on a subsequent rising edge");
        graph.DispatchInput("fr", "Arm", armOff);
        graph.DispatchInput("fr", "Arm", armOn);
        graph.DispatchInput("fr", "In", hi);
        Expect(pulses == 2, "flow_rise should re-arm without treating steady-high In as a new rise");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(FlowFallNode{.id = "ff", .def = {}});
        int pulses = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "ff" && ev.outputName == "pulse") {
                ++pulses;
            }
        });
        LogicContext armOn;
        armOn.analogSignal = 1.0;
        LogicContext hi;
        hi.analogSignal = 1.0;
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("ff", "Arm", armOn);
        graph.DispatchInput("ff", "In", hi);
        graph.DispatchInput("ff", "In", lo);
        Expect(pulses == 1, "flow_fall should pulse once on a high-to-low transition while armed");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(FlowDbncNode{.id = "db", .def = {.defaultDebounceMs = 0}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "db" && ev.outputName == "out") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext hi;
        hi.analogSignal = 1.0;
        LogicContext lo;
        lo.analogSignal = 0.0;
        graph.DispatchInput("db", "In", hi);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9,
               "flow_dbnc with 0 ms should commit high immediately");
        graph.DispatchInput("db", "In", lo);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 0.0) < 1e-9,
               "flow_dbnc with 0 ms should commit low immediately");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(FlowDbncNode{.id = "db2", .def = {.defaultDebounceMs = 50}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "db2" && ev.outputName == "out") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext hi;
        hi.analogSignal = 1.0;
        graph.DispatchInput("db2", "In", hi);
        Expect(!lastOut.has_value(), "flow_dbnc should not emit before the debounce window elapses");
        graph.AdvanceTime(50);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 1.0) < 1e-9,
               "flow_dbnc should emit stable high after debounceMs");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(FlowOneshotNode{.id = "os", .def = {.defaultPulseMs = 40}});
        std::optional<double> lastBusy;
        int trigCount = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "os") {
                return;
            }
            if (ev.outputName == "busy") {
                lastBusy = TryGetAnalogSignal(ev.context);
            }
            if (ev.outputName == "out" && TryGetAnalogSignal(ev.context).value_or(0) > 0.5) {
                ++trigCount;
            }
        });
        LogicContext trig;
        trig.analogSignal = 1.0;
        graph.DispatchInput("os", "Trig", trig);
        graph.DispatchInput("os", "Trig", trig);
        Expect(trigCount == 1, "flow_oneshot should ignore retrigger while busy");
        Expect(lastBusy.has_value() && std::fabs(*lastBusy - 1.0) < 1e-9, "flow_oneshot Busy should be high during pulse");
        graph.AdvanceTime(40);
        Expect(lastBusy.has_value() && std::fabs(*lastBusy - 0.0) < 1e-9,
               "flow_oneshot Busy should return low after pulseMs");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(TimeDelayNode{.id = "td", .def = {.defaultDelayMs = 40}});
        int outs = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "td" && ev.outputName == "out") {
                ++outs;
            }
        });
        graph.DispatchInput("td", "In", {});
        graph.AdvanceTime(39);
        Expect(outs == 0, "time_delay should not emit Out before delayMs elapses");
        graph.AdvanceTime(2);
        Expect(outs == 1, "time_delay should emit Out once after the configured delay");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(TimeClockNode{.id = "clk", .def = {.defaultHz = 10.0}});
        int ticks = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "clk" && ev.outputName == "tick") {
                ++ticks;
            }
        });
        LogicContext en;
        en.analogSignal = 1.0;
        graph.DispatchInput("clk", "Enable", en);
        graph.AdvanceTime(150);
        Expect(ticks >= 1, "time_clock should emit Tick periodically when enabled");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(TimeWatchNode{.id = "tw", .def = {}});
        std::optional<double> lastMs;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "tw" && ev.outputName == "ms") {
                lastMs = TryGetAnalogSignal(ev.context);
            }
        });
        graph.DispatchInput("tw", "Start", {});
        graph.AdvanceTime(30);
        graph.DispatchInput("tw", "Stop", {});
        Expect(lastMs.has_value() && std::fabs(*lastMs - 30.0) < 2.0,
               "time_watch Ms should reflect elapsed time when stopped after AdvanceTime");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MemSampleNode{.id = "msa", .def = {}});
        std::optional<double> lastOut;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId == "msa" && ev.outputName == "out") {
                lastOut = TryGetAnalogSignal(ev.context);
            }
        });
        LogicContext hiHold;
        hiHold.analogSignal = 1.0;
        LogicContext loHold;
        loHold.analogSignal = 0.0;
        LogicContext v3;
        v3.analogSignal = 3.0;
        LogicContext v5;
        v5.analogSignal = 5.0;
        graph.DispatchInput("msa", "Sig", v3);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 3.0) < 1e-9, "mem_sample should track Sig on Out");
        graph.DispatchInput("msa", "Hold", hiHold);
        graph.DispatchInput("msa", "Sig", v5);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 3.0) < 1e-9,
               "mem_sample should hold latched value while Hold is high");
        graph.DispatchInput("msa", "Hold", loHold);
        graph.DispatchInput("msa", "Sig", v5);
        Expect(lastOut.has_value() && std::fabs(*lastOut - 5.0) < 1e-9,
               "mem_sample should resume tracking Sig when Hold releases");
    }

    {
        LogicGraphSpec spec;
        spec.nodes.push_back(MemChatterNode{.id = "mch", .def = {.defaultDebounceMs = 0}});
        int rawHits = 0;
        int stableHits = 0;
        LogicGraph graph(std::move(spec));
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) {
            if (ev.sourceId != "mch") {
                return;
            }
            if (ev.outputName == "raw") {
                ++rawHits;
            }
            if (ev.outputName == "stable") {
                ++stableHits;
            }
        });
        LogicContext hi;
        hi.analogSignal = 1.0;
        graph.DispatchInput("mch", "Sig", hi);
        Expect(rawHits == 1 && stableHits == 1,
               "mem_chatter with 0 ms should emit Raw and Stable together on Sig");
    }

    Expect(!std::string_view{ri::logic::ports::kDoorOpen}.empty()
               && !std::string_view{ri::logic::ports::kTriggerOnStartTouch}.empty(),
           "WorldActorPorts should expose non-empty door/trigger port names");
}

// --- merged from TestLogicWorldActors.cpp ---
namespace detail_LogicWorldActors {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestLogicWorldActors() {
    using namespace detail_LogicWorldActors;
    using namespace ri::logic;

    {
        ri::world::RuntimeEnvironmentService env;
        env.RegisterLogicDoor("door_a");

        LogicGraphSpec spec;
        spec.nodes.push_back(RelayNode{.id = "r1", .def = {}});
        spec.nodes.push_back(RelayNode{.id = "r2", .def = {}});
        spec.routes.push_back({.sourceId = "r1",
                               .outputName = "OnTrigger",
                               .targets = {{.targetId = "door_a", .inputName = "Open"}}});
        spec.routes.push_back({.sourceId = "door_a",
                               .outputName = "OnOpened",
                               .targets = {{.targetId = "r2", .inputName = "Trigger"}}});

        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        ri::world::BindWorldActorsToLogicGraph(graph, env);
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.sourceId + ":" + ev.outputName); });

        graph.DispatchInput("r1", "Trigger", {});
        Expect(log.size() == 3U,
               "Relay -> door Open -> OnOpened -> relay should produce three output handler callbacks");
        Expect(log[0] == "r1:ontrigger", "First emission should be r1");
        Expect(log[1] == "door_a:onopened", "Door should emit OnOpened");
        Expect(log[2] == "r2:ontrigger", "Routed relay r2 should fire");
    }

    {
        ri::world::RuntimeEnvironmentService env;
        env.RegisterLogicSpawner("sp_a", {.activeSpawn = false, .enabled = false});

        LogicGraphSpec spec;
        spec.nodes.push_back(RelayNode{.id = "r1", .def = {}});
        spec.routes.push_back({.sourceId = "r1",
                               .outputName = "OnTrigger",
                               .targets = {{.targetId = "sp_a", .inputName = "Spawn"}}});

        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        ri::world::BindWorldActorsToLogicGraph(graph, env);
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.sourceId + ":" + ev.outputName); });

        graph.DispatchInput("r1", "Trigger", {});
        Expect(log.size() == 2U, "Relay plus spawner OnFailed");
        Expect(log[0] == "r1:ontrigger", "Relay should still emit");
        Expect(log[1] == "sp_a:onfailed", "Disabled spawner Spawn should emit OnFailed");
    }

    {
        ri::world::RuntimeEnvironmentService env;
        env.RegisterLogicSpawner("sp_b", {.activeSpawn = false, .enabled = true});

        LogicGraphSpec spec;
        spec.nodes.push_back(RelayNode{.id = "r1", .def = {}});
        spec.routes.push_back({.sourceId = "r1",
                               .outputName = "OnTrigger",
                               .targets = {{.targetId = "sp_b", .inputName = "Spawn"}}});

        std::vector<std::string> log;
        LogicGraph graph(std::move(spec));
        ri::world::BindWorldActorsToLogicGraph(graph, env);
        graph.SetOutputHandler([&](const LogicOutputEvent& ev) { log.push_back(ev.sourceId + ":" + ev.outputName); });

        graph.DispatchInput("r1", "Trigger", {});
        Expect(log.size() == 2U, "Relay plus spawner OnSpawned");
        Expect(log[0] == "r1:ontrigger", "Relay should emit");
        Expect(log[1] == "sp_b:onspawned", "Enabled spawner should emit OnSpawned");
    }

    {
        ri::world::RuntimeEnvironmentService env;
        ri::world::ProceduralDoorEntity door{};
        door.id = "door_exit_a";
        door.position = {0.0f, 0.0f, 0.0f};
        door.size = {2.0f, 3.0f, 1.0f};
        door.startsLocked = true;
        door.transitionLevel = "levels/finale.json";
        door.endingTrigger = "ending_escape";
        door.accessFeedbackTag = "door_locked";
        door.deniedPrompt = "Access denied";
        env.SetProceduralDoorEntities({door});

        std::string deniedFeedback;
        Expect(env.TryInteractWithProceduralDoor("door_exit_a", false, &deniedFeedback),
               "Runtime world actor service should handle procedural-door interaction lookups by id");
        Expect(deniedFeedback == "Access denied",
               "Procedural doors should surface denied feedback when interaction access requirements fail");
        const auto deniedTransitions = env.ConsumePendingDoorTransitions();
        Expect(deniedTransitions.size() == 1U
                   && deniedTransitions.front().doorId == "door_exit_a"
                   && deniedTransitions.front().transitionLevel.empty()
                   && deniedTransitions.front().accessFeedbackTag == "door_locked",
               "Procedural doors should emit access-feedback transition metadata without level swap on denied access");

        std::string grantedFeedback;
        Expect(env.TryInteractWithProceduralDoor("door_exit_a", true, &grantedFeedback),
               "Runtime world actor service should allow procedural-door interactions when access is granted");
        const auto grantedTransitions = env.ConsumePendingDoorTransitions();
        Expect(grantedTransitions.size() == 1U
                   && grantedTransitions.front().transitionLevel == "levels/finale.json"
                   && grantedTransitions.front().endingTrigger == "ending_escape",
               "Procedural doors should emit level/ending transition metadata after successful interaction");
        env.ApplyDoorTransitionMetadata(grantedTransitions.front());
        Expect(env.HasWorldFlag("access_feedback.door.locked")
                   && env.HasWorldFlag("ending.ending.escape")
                   && env.HasWorldFlag("level_transition.levels.finale.json"),
               "Door transition metadata should bridge access/ending/level tags into deterministic world flags");
        Expect(env.GetWorldValueOr("door.transition.count", 0.0) >= 1.0,
               "Door transition metadata should increment transition counters for runtime objective/narrative hooks");

        ri::world::DynamicInfoPanelSpawner panel{};
        panel.id = "panel_a";
        panel.position = {0.0f, 1.0f, 0.0f};
        panel.size = {2.0f, 2.0f, 1.0f};
        panel.interactionPrompt = "Inspect Panel";
        env.SetDynamicInfoPanelSpawners({panel});
        Expect(env.GetDynamicInfoPanelSpawnerAt({0.0f, 1.0f, 0.0f}) != nullptr
                   && env.GetInfoPanelInteractionPromptAt({0.0f, 1.0f, 0.0f}) == "Inspect Panel",
               "Runtime world actor service should expose focus/interact queries for dynamic info panels");
        const ri::world::InteractionTargetState panelTarget = env.ResolveInteractionTarget(
            {0.0f, 1.0f, -1.5f},
            {0.0f, 0.0f, 1.0f},
            {.maxDistance = 4.0f, .overlapRadius = 1.0f, .actionLabel = "E"});
        Expect(panelTarget.kind == ri::world::InteractionTargetKind::InfoPanel
                   && panelTarget.targetId == "panel_a"
                   && panelTarget.promptText.find("[E]") != std::string::npos,
               "Interaction target resolver should build deterministic prompt text for ray/overlap info-panel candidates");

        ri::world::ProceduralDoorEntity overlapDoor{};
        overlapDoor.id = "door_overlap";
        overlapDoor.position = {0.0f, 1.0f, -0.8f};
        overlapDoor.size = {1.2f, 2.0f, 0.3f};
        overlapDoor.interactionPrompt = "Close Door";
        overlapDoor.interactionHook = "door_overlap_interact";

        ri::world::DynamicInfoPanelSpawner rayPanel{};
        rayPanel.id = "panel_ray_a";
        rayPanel.position = {0.0f, 1.0f, 1.2f};
        rayPanel.size = {1.4f, 1.2f, 0.3f};
        rayPanel.interactionPrompt = "Inspect A";
        rayPanel.interactionHook = "panel_a_interact";

        ri::world::DynamicInfoPanelSpawner tiePanel{};
        tiePanel.id = "panel_ray_b";
        tiePanel.position = rayPanel.position;
        tiePanel.size = rayPanel.size;
        tiePanel.interactionPrompt = "Inspect B";
        tiePanel.interactionHook = "panel_b_interact";

        env.SetProceduralDoorEntities({overlapDoor});
        env.SetDynamicInfoPanelSpawners({tiePanel, rayPanel});

        const ri::world::InteractionTargetState prioritizedTarget = env.ResolveInteractionTarget(
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {.maxDistance = 4.0f, .overlapRadius = 1.25f, .actionLabel = "E"});
        Expect(prioritizedTarget.kind == ri::world::InteractionTargetKind::InfoPanel
                   && prioritizedTarget.targetId == "panel_ray_a"
                   && prioritizedTarget.inRay,
               "Interaction target resolver should prefer deterministic in-ray candidates over overlap-only contenders and break ties by stable ids");

        env.SetWorldFlag("power_on", true);
        env.SetWorldValue("progress", 2.0);
        ri::world::WorldStateCondition condition{};
        condition.requiredFlags = {"power_on"};
        condition.minimumValues = {{"progress", 1.5}};
        Expect(env.CheckWorldStateCondition(condition),
               "Runtime world actor service should evaluate world-flag/value state-machine conditions deterministically");

        ri::world::AudioReverbVolume reverb{};
        reverb.id = "audio_room";
        reverb.position = {0.0f, 1.0f, 0.0f};
        reverb.size = {6.0f, 4.0f, 6.0f};
        reverb.shape = ri::world::VolumeShape::Box;
        reverb.reverbMix = 0.4f;
        env.SetAudioReverbVolumes({reverb});
        ri::world::AmbientAudioVolume ambient{};
        ambient.id = "ambient_a";
        ambient.position = {0.0f, 1.0f, 0.0f};
        ambient.size = {8.0f, 4.0f, 8.0f};
        ambient.shape = ri::world::VolumeShape::Box;
        ambient.baseVolume = 0.5f;
        ambient.maxDistance = 12.0f;
        env.SetAmbientAudioVolumes({ambient});
        env.SetWorldFlag("audio.chase", true);
        const ri::world::AudioRoutingState routing = env.GetAudioRoutingStateAt({0.0f, 1.0f, 0.0f});
        Expect(routing.environmentLabel == "audio_room" && routing.ambientLayer > 0.0f
                   && routing.chaseLayer == 1.0f && routing.endingLayer == 0.0f,
               "Audio routing state should blend ambient/environment layers and world-flag chase routing");
    }
}

// --- merged from TestNpcAgentState.cpp ---
namespace detail_NpcAgentState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestNpcAgentState() {
    using namespace detail_NpcAgentState;
    ri::world::NpcAgentState state({
        .mode = ri::world::NpcAgentMode::Patrol,
        .allowInteraction = true,
        .allowPatrol = true,
        .allowSpeakOnce = true,
        .historyLimit = 4U,
    });

    state.Configure({
        .id = "npc_vale",
        .displayName = "Dr. Vale",
        .defaultAnimation = "idle",
        .interactionAnimation = "talk",
        .resumeAnimation = "idle",
        .pathAnimation = "walk",
        .pathRunAnimation = "run",
        .pathTurnAnimation = "alert",
        .pathIdleAnimation = "idle",
        .patrolSpeed = 1.8f,
        .pathRunThreshold = 1.55f,
        .pathEpsilon = 0.25f,
        .pathWaitMs = 500.0,
        .interactionCooldownMs = 2000.0,
        .patrolMode = ri::world::NpcPatrolMode::PingPong,
        .patrolLoop = true,
        .lookAtPath = true,
        .speakOnce = true,
        .pathPoints = {
            {0.0f, 0.0f, 0.0f},
            {2.0f, 0.0f, 0.0f},
        },
    });

    const ri::world::NpcInteractionResult first = state.Interact(0.0, 4500.0, 2200.0,
                                                                 "DR. VALE: KEEP YOUR VOICE DOWN.",
                                                                 "Nothing new.");
    Expect(first.accepted && !first.repeated
               && first.displayText == "DR. VALE: KEEP YOUR VOICE DOWN."
               && first.animationAction == "talk"
               && first.outcomeCode == ri::world::NpcInteractionOutcomeCode::AcceptedPrimaryDialogue
               && static_cast<std::uint16_t>(first.outcomeCode) == 0x0105,
           "NPC agent state should preserve the first authored interaction");

    const ri::world::NpcInteractionResult cooling = state.Interact(1.0, 4500.0, 2200.0,
                                                                   "SHOULD NOT REPEAT",
                                                                   "Nothing new.");
    Expect(!cooling.accepted && cooling.coolingDown
               && cooling.outcomeCode == ri::world::NpcInteractionOutcomeCode::RejectedCooldownActive
               && static_cast<std::uint16_t>(cooling.outcomeCode) == 0x0103,
           "NPC agent state should expose interaction cooldowns before repeat handling");

    ri::world::NpcPatrolUpdate paused = state.AdvancePatrol(0.0, 0.016f, {0.0f, 0.0f, 0.0f});
    Expect(paused.active && paused.paused && paused.animationIntent == ri::world::NpcAnimationIntent::Idle
               && paused.phaseCode == ri::world::NpcPatrolPhaseCode::PausedWaitAtWaypoint
               && paused.animationIntentOrdinal == ri::world::NpcAnimationIntentOrdinal(ri::world::NpcAnimationIntent::Idle)
               && static_cast<std::uint16_t>(paused.phaseCode) == 0x0210,
           "NPC agent state should pause at waypoints when a wait time is authored");

    const ri::world::NpcPatrolUpdate moving = state.AdvancePatrol(1.0, 0.016f, {0.0f, 0.0f, 0.0f});
    Expect(moving.active && !moving.paused
               && moving.phaseCode == ri::world::NpcPatrolPhaseCode::AdvancingRun
               && moving.animationIntent == ri::world::NpcAnimationIntent::Run
               && moving.animationIntentOrdinal == 2U,
           "NPC agent state should advance patrol with run intent when speed crosses the run threshold");

    ri::world::NpcPatrolUpdate pingPongPause = state.AdvancePatrol(2.0, 0.016f, {2.0f, 0.0f, 0.0f});
    Expect(pingPongPause.active && pingPongPause.paused
               && pingPongPause.phaseCode == ri::world::NpcPatrolPhaseCode::PausedWaitAtWaypoint,
           "NPC agent state should respect authored waypoint pauses at the far end of a patrol");

    [[maybe_unused]] const ri::world::NpcPatrolUpdate cooldownAdvance =
        state.AdvancePatrol(2.1, 2.1f, {0.0f, 0.0f, 0.0f});
    [[maybe_unused]] const ri::world::NpcPatrolUpdate repeatAdvance =
        state.AdvancePatrol(4.3, 2.1f, {0.0f, 0.0f, 0.0f});
    const ri::world::NpcInteractionResult repeated = state.Interact(4.4, 4500.0, 2200.0,
                                                                    "SHOULD NOT REPEAT",
                                                                    "Nothing new.");
    Expect(repeated.accepted && repeated.repeated && repeated.displayText == "Nothing new."
               && repeated.outcomeCode == ri::world::NpcInteractionOutcomeCode::AcceptedRepeatedSpeakOnce
               && static_cast<std::uint16_t>(repeated.outcomeCode) == 0x0104,
           "NPC agent state should honor speak-once repeat behavior");

    state.SetPolicy({
        .mode = ri::world::NpcAgentMode::Disabled,
        .allowInteraction = true,
        .allowPatrol = true,
        .allowSpeakOnce = true,
        .historyLimit = 2U,
    });
    const ri::world::NpcInteractionResult disabled = state.Interact(2.0, 1000.0, 1000.0, "Should not appear", "Should not appear");
    const ri::world::NpcPatrolUpdate disabledPatrol = state.AdvancePatrol(2.0, 0.016f, {0.0f, 0.0f, 0.0f});
    Expect(!disabled.accepted && disabled.outcomeCode == ri::world::NpcInteractionOutcomeCode::RejectedAgentDisabled
               && !disabledPatrol.active
               && disabledPatrol.phaseCode == ri::world::NpcPatrolPhaseCode::InactiveAgentDisabled
               && state.History().empty(),
           "NPC agent state should be fully optional when disabled");

    Expect(ri::world::NpcInteractionOutcomeLabel(ri::world::NpcInteractionOutcomeCode::AcceptedPrimaryDialogue)
               == "accepted_primary_dialogue",
           "Headless labels should stay aligned with the interaction outcome chart");
    Expect(ri::world::headless::kNpcInteractionOutcomeBand == 0x0100U
               && ri::world::headless::kNpcPatrolPhaseBand == 0x0200U,
           "Subsystem bands should remain stable for serial assertions");

    ri::world::NpcAgentState silent({
        .mode = ri::world::NpcAgentMode::Patrol,
        .allowInteraction = false,
        .allowPatrol = false,
        .allowSpeakOnce = true,
        .historyLimit = 2U,
    });
    silent.Configure({.id = "silent_npc", .displayName = "Silent"});
    const ri::world::NpcInteractionResult locked = silent.Interact(0.0, 1000.0, 1000.0, "no", "no");
    Expect(!locked.accepted && locked.outcomeCode == ri::world::NpcInteractionOutcomeCode::RejectedInteractionDisabled
               && static_cast<std::uint16_t>(locked.outcomeCode) == 0x0102,
           "NPC agent state should report a distinct outcome when interaction is disallowed by policy");
}

// --- merged from TestPickupFeedbackState.cpp ---
namespace detail_PickupFeedbackState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestPickupFeedbackState() {
    using namespace detail_PickupFeedbackState;
    ri::world::PickupFeedbackState state({
        .mode = ri::world::PickupFeedbackMode::Verbose,
        .allowObjectiveUpdates = true,
        .allowHints = true,
        .historyLimit = 3U,
    });

    state.RecordPickup({
        .itemId = "level1_key",
        .itemLabel = "Level 1 Keycard",
        .pickupMessage = "Keycard secured.",
        .objectiveText = "Reach the annex gate.",
        .hintText = "Check the yellow reader.",
        .itemClass = "keycard",
        .pickupAudioCue = "pickup/keycard",
        .uiAccentCue = "ui/pickup_keycard",
    });

    Expect(state.ActiveMessage().has_value() && state.ActiveMessage()->text == "Keycard secured.",
           "Pickup feedback should honor verbose pickup messages");
    Expect(state.ActiveHint().has_value() && state.ActiveHint()->text == "Check the yellow reader.",
           "Pickup feedback should surface verbose pickup hints when enabled");
    const std::optional<std::string> objective = state.ConsumePendingObjective();
    Expect(objective.has_value() && *objective == "Reach the annex gate.",
           "Pickup feedback should expose pending objective updates when enabled");
    Expect(!state.ConsumePendingObjective().has_value(),
           "Pickup feedback should clear pending objective updates once consumed");
    Expect(state.ConsumePendingUiAccentPulse(),
           "Pickup feedback should expose a UI accent pulse for pickup confirmations");

    state.RecordAlreadyCarrying("Level 1 Keycard");
    Expect(state.ActiveMessage().has_value() && state.ActiveMessage()->text == "Already carrying Level 1 Keycard",
           "Pickup feedback should report duplicate item pickup attempts");

    state.RecordConsumed("Medkit");
    Expect(state.ActiveMessage().has_value() && state.ActiveMessage()->text == "Used Medkit",
           "Pickup feedback should report consumed support items");

    state.RecordUnavailable("Flashlight");
    Expect(state.History().size() == 3U && state.History().front().message == "Already carrying Level 1 Keycard",
           "Pickup feedback should keep a capped rolling history");
    state.SetPolicy({
        .mode = ri::world::PickupFeedbackMode::Verbose,
        .allowObjectiveUpdates = true,
        .allowHints = true,
        .antiSpamWindowMs = 400.0,
        .maxBurstsPerWindow = 1U,
        .historyLimit = 4U,
    });
    state.RecordPickup({
        .itemId = "ammo_a",
        .itemLabel = "Rifle Ammo",
        .pickupMessage = "Ammo secured.",
        .itemClass = "ammo",
        .pickupAudioCue = "pickup/ammo",
        .uiAccentCue = "ui/pickup_ammo",
    });
    state.RecordPickup({
        .itemId = "ammo_b",
        .itemLabel = "Rifle Ammo",
        .pickupMessage = "Ammo secured.",
        .itemClass = "ammo",
        .pickupAudioCue = "pickup/ammo",
        .uiAccentCue = "ui/pickup_ammo",
    });
    Expect(state.History().back().suppressedByAntiSpam,
           "Pickup feedback should anti-spam rapid pickup confirmation cues");

    state.Advance(7000.0);
    Expect(!state.ActiveMessage().has_value() && !state.ActiveHint().has_value(),
           "Pickup feedback should expire transient message and hint channels");

    state.SetPolicy({
        .mode = ri::world::PickupFeedbackMode::Disabled,
        .allowObjectiveUpdates = true,
        .allowHints = true,
        .antiSpamWindowMs = 200.0,
        .maxBurstsPerWindow = 1U,
        .historyLimit = 2U,
    });
    state.RecordPickup({
        .itemId = "flashlight",
        .itemLabel = "Flashlight",
        .pickupMessage = "Flashlight secured.",
        .objectiveText = "Test disabled mode.",
        .hintText = "Should not appear.",
    });
    Expect(!state.ActiveMessage().has_value()
               && !state.ActiveHint().has_value()
               && !state.ConsumePendingObjective().has_value()
               && state.History().empty(),
           "Pickup feedback should become fully optional when disabled");
}

// --- merged from TestPipelineArtifacts.cpp ---
namespace detail_PipelineArtifacts {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestPipelineArtifacts() {
    using namespace detail_PipelineArtifacts;
    namespace fs = std::filesystem;

    ri::content::DeclarativeModelDefinition model{};
    model.modelId = "crate_test";
    ri::content::DeclarativeModelPart part{};
    part.name = "body";
    part.parentName = "";
    part.meshId = "meshes/crate.rmesh";
    part.materialId = "materials/wood";
    part.translation.x = 1.25F;
    part.translation.y = -0.5F;
    part.translation.z = 0.0F;
    part.rotation = {0.0F, 0.707106769F, 0.0F, 0.707106769F};
    part.scale = {2.0F, 2.0F, 2.0F};
    part.tags = {"opaque", "physics"};
    model.parts.push_back(part);

    const std::string modelJson = ri::content::SerializeDeclarativeModelDefinition(model);
    const auto parsedModel = ri::content::ParseDeclarativeModelDefinition(modelJson);
    Expect(parsedModel.has_value(), "Declarative model JSON should parse after serialization");
    Expect(parsedModel->modelId == model.modelId && parsedModel->parts.size() == 1U,
           "Declarative model parse should preserve top-level entries");
    const ri::content::DeclarativeModelPart& roundTripPart = parsedModel->parts[0];
    Expect(roundTripPart.name == part.name && roundTripPart.meshId == part.meshId,
           "Declarative model parse should preserve part identifiers");
    Expect(std::fabs(roundTripPart.translation.x - part.translation.x) < 1e-5F,
           "Declarative model parse should preserve translation");

    ri::content::pipeline::AssetExtractionInventory inventory{};
    inventory.generatedAtUtc = "2026-04-18T12:34:56Z";
    ri::content::pipeline::ArchiveInventoryEntry archive{};
    archive.path = "Imports/source_pack.zip";
    archive.identifier = "pack_a";
    archive.signature = "sha256:abcd";
    archive.extractionStatus = ri::content::pipeline::ExtractionStatus::Complete;
    archive.extractedOutputCurrent = true;
    ri::content::pipeline::ExtractedOutputEntry extracted{};
    extracted.relativePath = "Unpack/pack_a/model.bin";
    extracted.sizeBytes = 4096ULL;
    extracted.modifiedUtc = "2026-04-18T11:00:00Z";
    archive.extractedOutputs.push_back(extracted);
    inventory.archives.push_back(archive);

    const std::string inventoryJson = ri::content::pipeline::SerializeAssetExtractionInventory(inventory);
    const auto parsedInventory = ri::content::pipeline::ParseAssetExtractionInventory(inventoryJson);
    Expect(parsedInventory.has_value(), "Asset extraction inventory JSON should parse after serialization");
    Expect(parsedInventory->generatedAtUtc == inventory.generatedAtUtc,
           "Inventory parse should preserve generation timestamp");
    Expect(parsedInventory->archives.size() == 1U && parsedInventory->archives[0].identifier == archive.identifier,
           "Inventory parse should preserve archive identifiers");
    Expect(parsedInventory->archives[0].extractedOutputs.size() == 1U
               && parsedInventory->archives[0].extractedOutputs[0].relativePath == extracted.relativePath,
           "Inventory parse should preserve extracted outputs");

    const fs::path root = fs::temp_directory_path() / "rawiron_pipeline_artifacts_test";
    std::error_code errorCode;
    fs::remove_all(root, errorCode);
    fs::create_directories(root);

    Expect(ri::content::SaveDeclarativeModelDefinition(root / "model.decl.json", model),
           "Declarative model save helper should persist JSON");
    const auto loadedModel = ri::content::LoadDeclarativeModelDefinition(root / "model.decl.json");
    Expect(loadedModel.has_value() && loadedModel->parts.size() == model.parts.size(),
           "Declarative model load helper should reload saved JSON");

    Expect(ri::content::pipeline::SaveAssetExtractionInventory(root / "asset_inventory.json", inventory),
           "Inventory save helper should persist JSON");
    const auto loadedInventory = ri::content::pipeline::LoadAssetExtractionInventory(root / "asset_inventory.json");
    Expect(loadedInventory.has_value() && loadedInventory->archives.size() == inventory.archives.size(),
           "Inventory load helper should reload saved JSON");
    const ri::content::pipeline::ArchiveExtractionCacheDecision cacheHit =
        ri::content::pipeline::EvaluateArchiveExtractionCache(*loadedInventory, "pack_a", "sha256:abcd");
    Expect(!cacheHit.shouldExtract && !cacheHit.signatureChanged && !cacheHit.missingOutputs,
           "Extraction cache should skip re-extracting archives when signature and outputs are current");
    const ri::content::pipeline::ArchiveExtractionCacheDecision cacheMiss =
        ri::content::pipeline::EvaluateArchiveExtractionCache(*loadedInventory, "pack_a", "sha256:newsig");
    Expect(cacheMiss.shouldExtract && cacheMiss.signatureChanged,
           "Extraction cache should request extraction when archive signature changes");

    ri::content::pipeline::ArchiveInventoryEntry updatedArchive = archive;
    updatedArchive.signature = "sha256:updated";
    updatedArchive.extractedOutputCurrent = false;
    ri::content::pipeline::UpsertArchiveInventoryEntry(inventory, updatedArchive);
    const ri::content::pipeline::ArchiveInventoryEntry* foundUpdated =
        ri::content::pipeline::FindArchiveInventoryEntry(inventory, "pack_a");
    Expect(foundUpdated != nullptr && foundUpdated->signature == "sha256:updated" && !foundUpdated->extractedOutputCurrent,
           "Inventory upsert helper should replace existing entries by identifier");

    ri::content::AssetDocument assetDocument{};
    assetDocument.id = "crate";
    assetDocument.type = "mesh";
    assetDocument.displayName = "Crate";
    assetDocument.sourcePath = "Assets/Source/Crate.fbx";
    fs::create_directories(root / "package" / "assets");
    Expect(ri::content::SaveAssetDocument(root / "package" / "assets" / "crate.fbx.ri_asset.json", assetDocument),
           "Package fixture should write a standardized asset document");

    ri::content::AssetPackageManifest package = ri::content::BuildAssetPackageManifest(
        root / "package",
        "crate_pack",
        "Crate Pack",
        "Assets/Source/CratePack",
        "2026-04-18T12:34:56Z");
    Expect(package.assets.size() == 1U && package.assets[0].id == "crate",
           "Package build should discover standardized asset documents");
    package.packageKind = "resource-pack";
    package.packageVersion = "1.0.0";
    package.installScope = "either";
    package.mountPoint = "Packages/crate_pack";
    package.tags = {"crate", "test"};
    package.dependencies.push_back(ri::content::AssetPackageDependency{
        .packageId = "base_materials",
        .versionRequirement = ">=1.0.0",
        .optional = true,
    });
    package.conflicts = {"legacy_crate_pack"};
    package.assets[0].installPath = "assets/props/crate.fbx.ri_asset.json";
    const std::string packageJson = ri::content::SerializeAssetPackageManifest(package);
    const auto parsedPackage = ri::content::ParseAssetPackageManifest(packageJson);
    Expect(parsedPackage.has_value() && parsedPackage->packageId == "crate_pack"
               && parsedPackage->packageKind == "resource-pack"
               && parsedPackage->dependencies.size() == 1U
               && parsedPackage->assets[0].installPath == "assets/props/crate.fbx.ri_asset.json",
           "Package manifest JSON should preserve portable package metadata");
    const ri::content::AssetPackageValidationReport validPackage =
        ri::content::ValidateAssetPackageManifest(package, root / "package");
    Expect(validPackage.valid && validPackage.issues.empty(),
           "Package validation should accept matching assets, sizes, signatures, and source paths");

    ri::content::AssetPackageManifest duplicatePackage = package;
    duplicatePackage.assets.push_back(package.assets.front());
    const ri::content::AssetPackageValidationReport duplicateReport =
        ri::content::ValidateAssetPackageManifest(duplicatePackage, root / "package");
    Expect(!duplicateReport.valid,
           "Package validation should reject duplicate asset ids and paths");

    ri::content::AssetPackageManifest unsafePackage = package;
    unsafePackage.assets.front().path = "../crate.fbx.ri_asset.json";
    const ri::content::AssetPackageValidationReport unsafeReport =
        ri::content::ValidateAssetPackageManifest(unsafePackage, root / "package");
    Expect(!unsafeReport.valid,
           "Package validation should reject paths that escape the package root");

    ri::content::AssetPackageManifest badInstallPackage = package;
    badInstallPackage.assets.front().installPath = "../scripts/bad.ri_asset.json";
    const ri::content::AssetPackageValidationReport badInstallReport =
        ri::content::ValidateAssetPackageManifest(badInstallPackage, root / "package");
    Expect(!badInstallReport.valid,
           "Package validation should reject install paths that escape the project root");

    ri::content::AssetPackageManifest selfDependencyPackage = package;
    selfDependencyPackage.dependencies.push_back(ri::content::AssetPackageDependency{.packageId = "crate_pack"});
    const ri::content::AssetPackageValidationReport selfDependencyReport =
        ri::content::ValidateAssetPackageManifest(selfDependencyPackage, root / "package");
    Expect(!selfDependencyReport.valid,
           "Package validation should reject packages that depend on themselves");

    Expect(ri::content::SaveAssetPackageManifest(root / "package" / "package.ri_package.json", package),
           "Package save helper should persist JSON");
    const auto loadedPackage = ri::content::LoadAssetPackageManifest(root / "package" / "package.ri_package.json");
    Expect(loadedPackage.has_value() && loadedPackage->assets.size() == package.assets.size(),
           "Package load helper should reload saved package manifests");
    fs::create_directories(root / "game" / "Packages" / "crate_pack");
    Expect(ri::content::SaveAssetPackageManifest(root / "game" / "Packages" / "crate_pack" / "package.ri_package.json", package),
           "Package fixture should write a mounted package manifest");
    const std::vector<fs::path> packagePaths = ri::content::FindAssetPackageManifestPaths(root / "game");
    Expect(packagePaths.size() == 1U,
           "Package discovery should find one manifest placed under a project Packages folder; found "
               + std::to_string(packagePaths.size()));
    const std::vector<ri::content::InstalledAssetPackage> installedPackages =
        ri::content::DiscoverInstalledAssetPackages(root / "game");
    Expect(installedPackages.size() == 1U
               && ri::content::FindInstalledAssetPackage(installedPackages, "crate_pack") != nullptr,
           "Package discovery should load and index installed package manifests by package id");

    fs::remove_all(root, errorCode);
}

// --- merged from TestPlayerVitality.cpp ---
namespace detail_PlayerVitality {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestPlayerVitality() {
    using namespace detail_PlayerVitality;
    ri::world::PlayerVitality v(ri::world::PlayerVitalityConfig{.maxHealth = 100.0f});
    v.ResetToFullHealth();
    Expect(v.CurrentHealth() == 100.0f && v.IsAlive(), "Full reset should cap at max health");

    const auto healFull = v.Heal(10.0f);
    Expect(!healFull.healed && healFull.healthAfter == 100.0f, "Heal at full should no-op");

    v.SetHealth(40.0f);
    const auto healOk = v.Heal(25.0f);
    Expect(healOk.healed && healOk.healthAfter == 65.0f, "Heal should add up to max clamp");

    const auto dmg = v.ApplyDamage(0.0, 20.0f);
    Expect(dmg.applied && !dmg.died && dmg.healthAfter == 45.0f, "Damage should subtract before death");

    v.SetInvulnerableUntil(5.0);
    const auto blocked = v.ApplyDamage(1.0, 50.0f);
    Expect(!blocked.applied && blocked.blockedByInvulnerability && blocked.healthAfter == 45.0f,
           "Damage during invulnerability window should be ignored");

    const auto afterWindow = v.ApplyDamage(6.0, 100.0f);
    Expect(afterWindow.applied && afterWindow.died && afterWindow.healthAfter == 0.0f,
           "Damage after invulnerability should apply and can kill");

    const auto deadHeal = v.Heal(50.0f);
    Expect(!deadHeal.healed, "Heal should not revive the dead");

    ri::world::PlayerVitality u(ri::world::PlayerVitalityConfig{.maxHealth = 100.0f});
    u.ResetToFullHealth();
    u.SetInvulnerableUntil(1.0e9);
    Expect(!u.ApplyDamage(0.0, 30.0f).applied, "Long invulnerability window should block damage");
    u.ClearInvulnerability();
    Expect(u.InvulnerableUntil() == 0.0, "ClearInvulnerability should zero the deadline");
    const auto cleared = u.ApplyDamage(0.0, 30.0f);
    Expect(cleared.applied && cleared.healthAfter == 70.0f, "Damage should apply after ClearInvulnerability");

    ri::world::PlayerVitality w(ri::world::PlayerVitalityConfig{.maxHealth = 80.0f});
    w.SetHealth(200.0f);
    Expect(w.CurrentHealth() == 80.0f, "SetHealth should clamp to configured max");
}

// --- merged from TestPresentationState.cpp ---
namespace detail_PresentationState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestPresentationState() {
    using namespace detail_PresentationState;
    ri::world::PresentationState state(4U);

    state.ShowMessage("ACCESS DENIED", 2500.0, ri::world::PresentationSeverity::Critical);
    Expect(state.ActiveMessage().has_value()
               && state.ActiveMessage()->text == "ACCESS DENIED"
               && state.ActiveMessage()->severity == ri::world::PresentationSeverity::Critical,
           "Presentation state should store active critical messages");

    state.Advance(1000.0);
    Expect(state.ActiveMessage().has_value() && state.ActiveMessage()->remainingMs < 2500.0,
           "Presentation state should advance active message timers");

    state.ShowSubtitle("UNVERIFIED SIGNAL", 1800.0);
    Expect(state.ActiveSubtitle().has_value()
               && state.ActiveSubtitle()->text == "UNVERIFIED SIGNAL",
           "Presentation state should store active subtitle callouts");

    state.Advance(1900.0);
    Expect(!state.ActiveSubtitle().has_value(),
           "Presentation state should expire subtitle callouts after their duration");

    state.UpdateObjective("Recover the level key", {
        .announce = true,
        .flash = true,
        .hint = "Search the annex office.",
    });
    Expect(state.Objective().text == "Recover the level key"
               && state.Objective().flashRemainingMs > 0.0,
           "Presentation state should preserve objective text and active flash windows");
    Expect(state.ActiveMessage().has_value()
               && state.ActiveMessage()->text == "NEW OBJECTIVE: Recover the level key",
           "Presentation state should announce new objectives as active messages");

    const std::uint64_t revisionBefore = state.Objective().revision;
    state.Advance(1600.0);
    Expect(state.Objective().flashRemainingMs <= 0.0,
           "Presentation state should expire objective flash after the standard window");

    state.UpdateObjective("Restore power", {
        .announce = false,
        .flash = false,
        .hint = "Follow the cable trench.",
        .hintDurationMs = 6200.0,
    });
    Expect(state.Objective().text == "Restore power"
               && state.Objective().revision > revisionBefore
               && state.Objective().flashRemainingMs == 0.0,
           "Presentation state should support silent objective updates with revision tracking");
    Expect(state.ActiveMessage().has_value()
               && state.ActiveMessage()->text == "Follow the cable trench.",
           "Presentation state should surface non-announced objective hints as timed messages");

    state.ShowMessage("one", 1000.0);
    state.ShowMessage("two", 1000.0);
    state.ShowMessage("three", 1000.0);
    state.ShowMessage("four", 1000.0);
    state.ShowMessage("five", 1000.0);
    Expect(state.History().size() == 4U
               && state.History().front().text == "two"
               && state.History().back().text == "five",
           "Presentation state should keep a capped rolling history of presentation events");

    state.PushNarrativeMessage("Reactor unstable", 1200.0, true);
    Expect(state.ActiveMessage().has_value()
               && state.ActiveMessage()->severity == ri::world::PresentationSeverity::Critical
               && state.UrgencyFlashRemainingMs() > 0.0,
           "Presentation state narrative helpers should support urgent timed messages with urgency flash windows");
    state.Advance(1300.0);
    Expect(state.UrgencyFlashRemainingMs() <= 0.0,
           "Presentation state urgency flashes should dismiss automatically after their timed window");

    state.ClearTransientState();
    Expect(!state.ActiveMessage().has_value()
               && !state.ActiveSubtitle().has_value()
               && state.Objective().text == "Restore power",
           "Presentation state should clear transient channels without losing the current objective");
}

// --- merged from TestTextOverlayState.cpp ---
namespace detail_TextOverlayState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestTextOverlayState() {
    using namespace detail_TextOverlayState;
    ri::world::TextOverlayState overlay{};

    overlay.ShowMessage("ACCESS DENIED", 2500.0, ri::world::PresentationSeverity::Critical);
    ri::world::TextOverlaySnapshot snapshot = overlay.Snapshot();
    Expect(snapshot.hudVisible, "Overlay HUD should become visible when a message is shown");
    Expect(snapshot.messageBox.visible
               && snapshot.messageBox.text == "ACCESS DENIED"
               && snapshot.messageBox.severity == ri::world::PresentationSeverity::Critical,
           "Message channel should preserve text + severity");
    Expect(overlay.DismissTimers().contains(ri::world::TextOverlayChannel::MessageBox),
           "Message channel should be tracked by centralized dismiss timers");

    overlay.ShowSubtitle("UNVERIFIED SIGNAL", 1200.0);
    overlay.ShowLevelNameToast("Wilderness Ruins", 900.0);
    snapshot = overlay.Snapshot();
    Expect(snapshot.subtitleLine.visible && snapshot.levelNameToast.visible,
           "Subtitle and toast channels should activate independently");

    overlay.Advance(1000.0);
    snapshot = overlay.Snapshot();
    Expect(!snapshot.levelNameToast.visible && snapshot.subtitleLine.visible,
           "Independent channels should dismiss on their own timers");

    overlay.Advance(400.0);
    snapshot = overlay.Snapshot();
    Expect(!snapshot.subtitleLine.visible,
           "Subtitle should dismiss once its timer expires");

    overlay.UpdateObjective("Restore power", {
        .announce = true,
        .flash = true,
        .hint = "Find the annex breaker.",
    });
    snapshot = overlay.Snapshot();
    Expect(snapshot.objectiveReadout.text == "Restore power"
               && snapshot.objectiveReadout.flashing,
           "Objective updates should refresh objective text and flash state");
    Expect(snapshot.messageBox.visible
               && snapshot.messageBox.text == "NEW OBJECTIVE: Restore power",
           "Announced objective updates should route through the message channel");

    overlay.Advance(1600.0);
    snapshot = overlay.Snapshot();
    Expect(!snapshot.objectiveReadout.flashing,
           "Objective flash should auto-expire after its timer");

    overlay.SetLoadingVisible(true, "Loading level...");
    overlay.SetLoadingProgress(0.4, "Streaming assets");
    overlay.SetPauseVisible(true);
    overlay.SetDebugTerminalVisible(true);
    snapshot = overlay.Snapshot();
    Expect(snapshot.blockers.loadingVisible
               && snapshot.blockers.pauseVisible
               && snapshot.blockers.debugTerminalVisible
               && snapshot.blockers.loadingStatus == "Streaming assets",
           "Blocker overlays should track semantic game-state toggles");

    overlay.ClearTransientChannels();
    snapshot = overlay.Snapshot();
    Expect(!snapshot.messageBox.visible
               && !snapshot.subtitleLine.visible
               && !snapshot.levelNameToast.visible
               && snapshot.objectiveReadout.text == "Restore power",
           "Clearing transient channels should preserve persistent objective readout");
}

// --- merged from TestTextOverlayEventBridge.cpp ---
namespace detail_TextOverlayEventBridge {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestTextOverlayEventBridge() {
    using namespace detail_TextOverlayEventBridge;
    ri::runtime::RuntimeEventBus eventBus{};
    ri::world::TextOverlayState overlay{};
    ri::world::TextOverlayEventBridge bridge{};
    bridge.Attach(eventBus, overlay);

    eventBus.Emit("message", {
        .id = "evt_msg",
        .type = "message",
        .fields = {
            {"text", "Access denied"},
            {"severity", "critical"},
            {"durationMs", "2300"},
        },
    });
    auto snapshot = overlay.Snapshot();
    Expect(snapshot.messageBox.visible
               && snapshot.messageBox.text == "Access denied"
               && snapshot.messageBox.severity == ri::world::PresentationSeverity::Critical,
           "Message runtime events should map to the message overlay channel");

    eventBus.Emit("objectiveChanged", {
        .id = "evt_obj",
        .type = "objectiveChanged",
        .fields = {
            {"text", "Restore power"},
            {"announce", "false"},
            {"flash", "true"},
            {"hint", "Find the breaker room"},
            {"hintDurationMs", "6100"},
        },
    });
    snapshot = overlay.Snapshot();
    Expect(snapshot.objectiveReadout.text == "Restore power" && snapshot.objectiveReadout.flashing,
           "Objective events should update objective readout state");
    Expect(snapshot.messageBox.text == "Find the breaker room",
           "Silent objective updates should route hint text to message channel");

    eventBus.Emit("stateChanged", {
        .id = "evt_pause",
        .type = "stateChanged",
        .fields = {
            {"key", "isPaused"},
            {"value", "true"},
        },
    });
    eventBus.Emit("stateChanged", {
        .id = "evt_debug",
        .type = "stateChanged",
        .fields = {
            {"key", "debugTerminalOpen"},
            {"value", "true"},
        },
    });
    eventBus.Emit("loadingProgress", {
        .id = "evt_loading",
        .type = "loadingProgress",
        .fields = {
            {"visible", "true"},
            {"progress01", "0.35"},
            {"status", "Streaming terrain"},
        },
    });
    snapshot = overlay.Snapshot();
    Expect(snapshot.blockers.pauseVisible
               && snapshot.blockers.debugTerminalVisible
               && snapshot.blockers.loadingVisible
               && snapshot.blockers.loadingStatus == "Streaming terrain",
           "State/loading runtime events should drive blocker panel state");

    eventBus.Emit("levelLoaded", {
        .id = "evt_loaded",
        .type = "levelLoaded",
        .fields = {
            {"levelName", "Wilderness Ruins"},
        },
    });
    snapshot = overlay.Snapshot();
    Expect(!snapshot.blockers.loadingVisible
               && snapshot.levelNameToast.visible
               && snapshot.levelNameToast.text == "Wilderness Ruins",
           "Level-loaded events should hide loading and trigger a level-name toast");

    bridge.Detach();
    eventBus.Emit("message", {
        .id = "evt_after_detach",
        .type = "message",
        .fields = {
            {"text", "Should not appear"},
        },
    });
    snapshot = overlay.Snapshot();
    Expect(snapshot.messageBox.text != "Should not appear",
           "Detached bridge should stop forwarding event-bus updates");
}

// --- merged from TestTextOverlayEvents.cpp ---
namespace detail_TextOverlayEvents {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestTextOverlayEvents() {
    using namespace detail_TextOverlayEvents;
    ri::runtime::RuntimeEventBus eventBus{};
    ri::world::TextOverlayState overlay{};
    ri::world::TextOverlayEventBridge bridge{};
    bridge.Attach(eventBus, overlay);

    ri::world::text_overlay_events::EmitMessage(
        eventBus, "System online", 1800.0, ri::world::PresentationSeverity::Normal);
    ri::world::text_overlay_events::EmitSubtitle(eventBus, "Power restored", 1200.0);
    ri::world::text_overlay_events::EmitLevelToast(eventBus, "Annex", 900.0);
    ri::world::text_overlay_events::EmitObjectiveChanged(
        eventBus, "Reach extraction", true, true, "Follow the beacon", 3200.0);
    ri::world::text_overlay_events::EmitLoadingProgress(eventBus, true, 0.66, "Streaming geometry");
    ri::world::text_overlay_events::EmitVoiceLine(
        eventBus, "", "Hold position.", "Control", 1.0, 1400.0);
    ri::world::text_overlay_events::EmitVoiceStop(eventBus);

    ri::world::TextOverlaySnapshot snapshot = overlay.Snapshot();
    Expect(snapshot.messageBox.visible && snapshot.messageBox.text == "NEW OBJECTIVE: Reach extraction",
           "Typed objective emitter should flow through announce/message path");
    Expect(snapshot.subtitleLine.visible && snapshot.subtitleLine.text == "Control: Hold position.",
           "Typed voice-line emitter should route speaker-formatted subtitles");
    Expect(snapshot.levelNameToast.visible && snapshot.levelNameToast.text == "Annex",
           "Typed level-toast emitter should populate toast channel");
    Expect(snapshot.objectiveReadout.text == "Reach extraction" && snapshot.objectiveReadout.flashing,
           "Typed objective emitter should update objective state");
    Expect(snapshot.blockers.loadingVisible
               && snapshot.blockers.loadingStatus == "Streaming geometry"
               && snapshot.blockers.loadingProgress01 > 0.65,
           "Typed loading emitter should update loading blocker state");
}

// --- merged from TestDeveloperConsoleState.cpp ---
namespace detail_DeveloperConsoleState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestDeveloperConsoleState() {
    using namespace detail_DeveloperConsoleState;
    ri::world::DeveloperConsoleState console(8U, 5U);

    Expect(!console.IsOpen(), "Console should start closed");
    console.ToggleOpen();
    Expect(console.IsOpen(), "Toggle should open console");
    console.SetOpen(false);
    Expect(!console.IsOpen(), "SetOpen(false) should close console");

    console.SubmitCommand("help");
    Expect(!console.Scrollback().empty() && console.Scrollback().back().text.find("Commands:") != std::string::npos,
           "Help command should render built-in command list");
    console.SubmitCommand("help tune");
    Expect(console.Scrollback().back().text.find("tune list|get|set|reset|query") != std::string::npos,
           "Help with a topic should print command-specific help text");
    console.SubmitCommand("find tune");
    Expect(console.Scrollback().back().text.find("tune") != std::string::npos,
           "Find command should search command/cvar namespaces");
    console.SubmitCommand("cvarlist");
    Expect(console.Scrollback().back().text.find("walkSpeed=") != std::string::npos,
           "cvarlist should dump tuning cvars");

    console.SubmitCommand("tune get walkSpeed");
    const std::size_t beforeSetLines = console.Scrollback().size();
    console.SubmitCommand("tune set walkSpeed 99");
    Expect(console.TuningValues().at("walkSpeed") <= 12.0,
           "Tune set should clamp values using runtime tuning limits");
    Expect(!console.Scrollback().empty()
               && console.Scrollback().back().text.find("walkSpeed=") != std::string::npos
               && console.Scrollback().size() >= beforeSetLines,
           "Tune set should emit a value response line");

    console.SubmitCommand("tune query");
    Expect(console.Scrollback().back().text.find("?walkSpeed=") != std::string::npos,
           "Tune query should emit URL-style mechanics tuning query");
    const std::string tuningQuery = console.ExportTuningState();
    Expect(tuningQuery.find("?walkSpeed=") != std::string::npos,
           "Developer console should export current tuning state as a query string");

    console.SubmitCommand("tune save speedrun");
    console.SubmitCommand("tune set walkSpeed 1");
    Expect(std::abs(console.TuningValues().at("walkSpeed") - 1.0) <= 1.0e-6,
           "Tune set should update values before loading presets");
    console.SubmitCommand("tune load speedrun");
    Expect(std::abs(console.TuningValues().at("walkSpeed") - 12.0) <= 1.0e-6,
           "Tune load should restore previously saved named presets");
    console.SubmitCommand("tune apply ?walkSpeed=0.25");
    Expect(std::abs(console.TuningValues().at("walkSpeed") - 0.5) <= 1.0e-6,
           "Tune apply should sanitize imported tuning values");
    std::string importError;
    Expect(!console.ImportTuningState("?unknownTuningKey=10", &importError),
           "Developer console tuning imports should reject unknown tuning keys");

    console.SubmitCommand("tune reset walkSpeed");
    Expect(std::abs(console.TuningValues().at("walkSpeed") - 5.0) <= 1.0e-6,
           "Tune reset should restore default values per key");

    console.SubmitCommand("set walkSpeed 6.5");
    Expect(std::abs(console.TuningValues().at("walkSpeed") - 6.5) <= 1.0e-6,
           "set should route through cvar plumbing and update tuning state");
    console.SubmitCommand("toggle walkSpeed");
    Expect(std::abs(console.TuningValues().at("walkSpeed") - 0.5) <= 1.0e-6,
           "toggle should flip cvars between their configured min/max bounds");
    console.SubmitCommand("incrementvar walkSpeed 0 12 2");
    Expect(std::abs(console.TuningValues().at("walkSpeed") - 2.5) <= 1.0e-6,
           "incrementvar should apply bounded increments");
    console.SubmitCommand("echo alpha; echo beta");
    Expect(console.Scrollback().back().text == "beta",
           "Semicolon command chaining should execute multiple commands in-order");
    console.SubmitCommand("alias qhelp help tune");
    console.SubmitCommand("qhelp");
    Expect(console.Scrollback().back().text.find("tune list|get|set|reset|query") != std::string::npos,
           "alias should expand to a command chain");
    console.RegisterScript("dev_bootstrap", "set walkSpeed 7; echo booted");
    console.SubmitCommand("exec dev_bootstrap");
    Expect(console.Scrollback().back().text.find("exec complete: dev_bootstrap") != std::string::npos,
           "exec should run registered scripts and print completion output");
    Expect(std::abs(console.TuningValues().at("walkSpeed") - 7.0) <= 1.0e-6,
           "exec script commands should mutate cvar state");

    console.SubmitCommand("clear");
    Expect(console.Scrollback().size() == 1U && console.Scrollback().front().text == "Console cleared.",
           "Clear should wipe existing scrollback and emit a clear confirmation");

    bool customHandled = false;
    console.RegisterCommand("sv_test", [&](std::string_view args, std::string& out, bool& isError) {
        (void)isError;
        customHandled = true;
        out = std::string("ok:") + std::string(args);
        return true;
    });
    console.SubmitCommand("sv_test ping");
    Expect(customHandled && console.Scrollback().back().text == "ok:ping",
           "Registered custom commands should execute and append output");

    console.SubmitCommand("unknown_cmd");
    Expect(console.Scrollback().back().isError,
           "Unknown commands should append an error line");

    const std::vector<std::string> completions = console.Autocomplete("tu");
    Expect(!completions.empty() && completions.front() == "tune",
           "Autocomplete should return Source-style command suggestions by prefix");

    console.SubmitCommand("echo one");
    console.SubmitCommand("echo two");
    console.SubmitCommand("echo three");
    const std::optional<std::string> histUpA = console.HistoryUp();
    const std::optional<std::string> histUpB = console.HistoryUp();
    const std::optional<std::string> histDown = console.HistoryDown();
    Expect(histUpA.has_value() && histUpB.has_value() && histDown.has_value(),
           "History navigation should provide up/down entries");
    Expect(histUpA.value() == "echo three" && histUpB.value() == "echo two" && histDown.value() == "echo three",
           "History navigation should behave like Source console cursor traversal");

    console.SubmitCommand("developer 1");
    Expect(console.IsOpen(), "developer 1 should enable/open console");
    console.SubmitCommand("con_enable 0");
    Expect(!console.IsOpen(), "con_enable 0 should disable/close console");

    Expect(console.CommandHistory().size() <= 5U,
           "Console should enforce capped command history");
}

// --- merged from TestCheckpointPersistence.cpp ---
namespace detail_CheckpointPersistence {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestCheckpointPersistence() {
    using namespace detail_CheckpointPersistence;

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "rawiron_checkpoint_persistence_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    ri::world::FileCheckpointStore store(root);
    ri::world::RuntimeCheckpointSnapshot snapshot{};
    snapshot.slot = "autosave";
    snapshot.state.level = "hub.ri_scene";
    snapshot.state.checkpointId = "checkpoint_a";
    snapshot.state.flags = {"power_on", "door_unlocked"};
    snapshot.state.eventIds = {"event_alpha", "event_beta"};
    snapshot.state.values = {{"progress", 3.5}, {"difficulty", 2.0}};
    snapshot.playerPosition = ri::math::Vec3{10.0f, 2.0f, -5.0f};
    snapshot.playerRotation = ri::math::Vec3{5.0f, 90.0f, 0.0f};

    std::string storeError;
    Expect(store.Save(snapshot, &storeError),
           "Checkpoint store should persist snapshots to disk");
    Expect(storeError.empty(), "Checkpoint persistence should not report write errors for valid snapshots");

    const std::optional<ri::world::RuntimeCheckpointSnapshot> loaded = store.Load("autosave", &storeError);
    Expect(loaded.has_value()
               && loaded->state.level.has_value() && loaded->state.level.value() == "hub.ri_scene"
               && loaded->playerPosition.has_value(),
           "Checkpoint store should restore persisted snapshot payloads");
    Expect(std::abs(loaded->playerPosition->x - 10.0f) <= 1.0e-6f
               && std::abs(loaded->playerRotation->y - 90.0f) <= 1.0e-6f,
           "Checkpoint store should roundtrip persisted player transform fields");

    ri::world::CheckpointStartupOptions startupFromQuery =
        ri::world::ParseCheckpointStartupOptions("?startFromCheckpoint=1&checkpointSlot=autosave");
    Expect(startupFromQuery.startFromCheckpoint && startupFromQuery.slot == "autosave",
           "Checkpoint query parsing should decode startFromCheckpoint and slot fields");

    ri::world::CheckpointStartupOptions startupOptions{};
    startupOptions.startFromCheckpoint = false;
    startupOptions.slot = "manual";
    startupOptions.queryString = "?startFromCheckpoint=true&checkpointSlot=autosave";
    const ri::world::CheckpointStartupDecision startupDecision =
        ri::world::ResolveCheckpointStartupDecision(startupOptions, store, &storeError);
    Expect(startupDecision.startFromCheckpoint
               && startupDecision.slot == "autosave"
               && startupDecision.snapshot.has_value()
               && startupDecision.snapshot->state.checkpointId.has_value()
               && startupDecision.snapshot->state.checkpointId.value() == "checkpoint_a",
           "Checkpoint startup resolver should honor URL query overrides and load persisted slots");

    Expect(store.Clear("autosave", &storeError),
           "Checkpoint store should clear persisted slots");
    const std::optional<ri::world::RuntimeCheckpointSnapshot> afterClear = store.Load("autosave", &storeError);
    Expect(!afterClear.has_value(),
           "Checkpoint loads should return empty after slot clear");

    std::filesystem::remove_all(root, ec);
}

// --- merged from TestRuntimeLocalGridSnap.cpp ---
namespace detail_RuntimeLocalGridSnap {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    const bool matches = NearlyEqual(actual.x, expected.x) &&
                         NearlyEqual(actual.y, expected.y) &&
                         NearlyEqual(actual.z, expected.z);
    Expect(matches,
           label + " expected (" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " +
               std::to_string(expected.z) + ") but got (" + std::to_string(actual.x) + ", " +
               std::to_string(actual.y) + ", " + std::to_string(actual.z) + ")");
}

}

 // namespace

void TestRuntimeLocalGridSnap() {
    using namespace detail_RuntimeLocalGridSnap;
    ri::world::RuntimeEnvironmentService service{};

    ri::world::LocalGridSnapVolume lowPriorityGrid{};
    lowPriorityGrid.id = "grid_low";
    lowPriorityGrid.type = "local_grid_snap_volume";
    lowPriorityGrid.shape = ri::world::VolumeShape::Box;
    lowPriorityGrid.position = {10.0f, 4.0f, -2.0f};
    lowPriorityGrid.size = {8.0f, 8.0f, 8.0f};
    lowPriorityGrid.snapSize = 1.0f;
    lowPriorityGrid.priority = 0;

    ri::world::LocalGridSnapVolume highPriorityGrid = lowPriorityGrid;
    highPriorityGrid.id = "grid_high";
    highPriorityGrid.snapSize = 0.25f;
    highPriorityGrid.snapY = false;
    highPriorityGrid.priority = 10;

    service.SetLocalGridSnapVolumes({lowPriorityGrid, highPriorityGrid});

    const ri::math::Vec3 snapped = service.SnapPositionToLocalGrid({10.36f, 4.12f, -1.74f});
    ExpectVec3(snapped, {10.25f, 4.12f, -1.75f},
               "Runtime environment should use the best local grid and honor per-axis snap flags");

    const ri::math::Vec3 untouched = service.SnapPositionToLocalGrid({40.0f, 2.0f, 40.0f});
    ExpectVec3(untouched, {40.0f, 2.0f, 40.0f},
               "Runtime environment should not snap positions outside local grid volumes");
}

// --- merged from TestRuntimeStatsOverlay.cpp ---
namespace detail_RuntimeStatsOverlay {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestRuntimeStatsOverlay() {
    using namespace detail_RuntimeStatsOverlay;
    ri::world::RuntimeStatsOverlayState overlay(true);
    overlay.SetAttached(true);
    overlay.SetVisible(true);
    overlay.RecordFrameDeltaSeconds(1.0 / 60.0);
    overlay.RecordFrameDeltaSeconds(1.0 / 30.0);

    const ri::world::RuntimeStatsOverlayMetrics metrics = overlay.GetMetrics();
    Expect(metrics.enabled && metrics.attached && metrics.visible,
           "Runtime stats overlay metrics should preserve enabled/attached/visible state");
    Expect(metrics.frameTimeMs > 0.0 && metrics.framesPerSecond > 0.0,
           "Runtime stats overlay metrics should track frame time and FPS");
    Expect(metrics.frameTimeMs < 33.34 && metrics.frameTimeMs > 16.60,
           "Runtime stats overlay smoothing should land between successive frame samples");

    ri::world::RuntimeStatsOverlaySnapshot snapshot{};
    snapshot.metrics = metrics;
    snapshot.sceneNodes = 18;
    snapshot.rootNodes = 2;
    snapshot.renderables = 9;
    snapshot.lights = 3;
    snapshot.cameras = 1;
    snapshot.selectedNode = 4;
    snapshot.modeLabel = "project";
    snapshot.sceneLabel = "liminal-hall";

    const std::vector<std::string> lines = ri::world::FormatRuntimeStatsOverlayLines(snapshot, 6);
    Expect(lines.size() == 6U, "Runtime stats overlay formatter should emit the requested line set");
    Expect(lines[0].find("RAW IRON STATS [project]") != std::string::npos,
           "Runtime stats overlay header should include the mode label");
    Expect(lines[1].find("fps ") != std::string::npos && lines[1].find("frame ") != std::string::npos,
           "Runtime stats overlay formatter should include FPS and frame timing");
    Expect(lines[2].find("nodes 18") != std::string::npos && lines[2].find("drawables 9") != std::string::npos,
           "Runtime stats overlay formatter should include scene and renderable counts");
    Expect(lines[5].find("liminal-hall") != std::string::npos,
           "Runtime stats overlay formatter should include the active scene label");
}

// --- merged from TestRuntimeTriggerVolumes.cpp ---
namespace detail_RuntimeTriggerVolumes {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    const bool matches = NearlyEqual(actual.x, expected.x) &&
                         NearlyEqual(actual.y, expected.y) &&
                         NearlyEqual(actual.z, expected.z);
    Expect(matches,
           label + " expected (" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " +
               std::to_string(expected.z) + ") but got (" + std::to_string(actual.x) + ", " +
               std::to_string(actual.y) + ", " + std::to_string(actual.z) + ")");
}

ri::world::RuntimeVolumeSeed MakeSeed(std::string id,
                                      std::string type = {},
                                      std::optional<ri::world::VolumeShape> shape = std::nullopt,
                                      std::optional<ri::math::Vec3> position = std::nullopt,
                                      std::optional<ri::math::Vec3> size = std::nullopt) {
    ri::world::RuntimeVolumeSeed seed{};
    seed.id = std::move(id);
    seed.type = std::move(type);
    seed.shape = shape;
    seed.position = position;
    seed.size = size;
    return seed;
}

}

 // namespace

void TestRuntimeTriggerVolumes() {
    using namespace detail_RuntimeTriggerVolumes;
    {
        ri::world::RuntimeVolumeSeed disarmedSeed =
            MakeSeed("trigger_a", "generic_trigger_volume", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{2.0f, 2.0f, 2.0f});
        disarmedSeed.startArmed = false;
        const ri::world::GenericTriggerVolume trigger = ri::world::CreateGenericTriggerVolume(
            disarmedSeed,
            2.0,
            {.runtimeId = "trigger_volume", .type = "generic_trigger_volume", .shape = ri::world::VolumeShape::Sphere, .size = {2.0f, 2.0f, 2.0f}});
        Expect(trigger.type == "generic_trigger_volume" &&
                   NearlyEqual(static_cast<float>(trigger.broadcastFrequency), 2.0f) && !trigger.armed,
               "World volume descriptors should create typed generic trigger volumes and honor startArmed");

        const ri::world::SpatialQueryVolume spatialQuery = ri::world::CreateSpatialQueryVolume(
            MakeSeed("query_a", "spatial_query_volume", ri::world::VolumeShape::Cylinder, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{2.0f, 4.0f, 2.0f}),
            3.0,
            7U,
            {.runtimeId = "query_volume", .type = "spatial_query_volume", .shape = ri::world::VolumeShape::Sphere, .size = {2.0f, 2.0f, 2.0f}});
        Expect(spatialQuery.type == "spatial_query_volume" &&
                   NearlyEqual(static_cast<float>(spatialQuery.broadcastFrequency), 3.0f) &&
                   spatialQuery.filterMask == 7U,
               "World volume descriptors should create typed spatial query volumes");

        const ri::world::StreamingLevelVolume streaming = ri::world::CreateStreamingLevelVolume(
            MakeSeed("stream_a", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{6.0f, 4.0f, 6.0f}),
            "facility_b.ri_scene",
            {.runtimeId = "streaming_level", .type = "streaming_level_volume", .size = {6.0f, 4.0f, 6.0f}});
        Expect(streaming.type == "streaming_level_volume" && streaming.targetLevel == "facility_b.ri_scene",
               "World volume descriptors should create typed streaming level volumes");

        const ri::world::CheckpointSpawnVolume checkpoint = ri::world::CreateCheckpointSpawnVolume(
            MakeSeed("checkpoint_a", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{3.0f, 3.0f, 3.0f}),
            "hub.ri_scene",
            {1.0f, 0.0f, 2.0f},
            {0.0f, 1.57f, 0.0f},
            {.runtimeId = "checkpoint_spawn", .type = "checkpoint_spawn_volume", .size = {3.0f, 3.0f, 3.0f}});
        Expect(checkpoint.type == "checkpoint_spawn_volume" && checkpoint.targetLevel == "hub.ri_scene",
               "World volume descriptors should create typed checkpoint spawn volumes");
        ExpectVec3(checkpoint.respawn, {1.0f, 0.0f, 2.0f}, "Checkpoint respawn");

        const ri::world::TeleportVolume teleport = ri::world::CreateTeleportVolume(
            MakeSeed("teleport_a", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{4.0f, 4.0f, 4.0f}),
            "door_exit",
            {20.0f, 1.0f, -5.0f},
            {0.0f, 3.14f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {.runtimeId = "teleport", .type = "teleport_volume", .size = {4.0f, 4.0f, 4.0f}});
        Expect(teleport.type == "teleport_volume" && teleport.targetId == "door_exit",
               "World volume descriptors should create typed teleport volumes");

        const ri::world::LaunchVolume launch = ri::world::CreateLaunchVolume(
            MakeSeed("launch_a", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{4.0f, 4.0f, 4.0f}),
            {0.0f, 18.0f, 0.0f},
            false,
            {.runtimeId = "launch", .type = "launch_volume", .size = {4.0f, 4.0f, 4.0f}});
        Expect(launch.type == "launch_volume" && !launch.affectPhysics,
               "World volume descriptors should create typed launch volumes");
        ExpectVec3(launch.impulse, {0.0f, 18.0f, 0.0f}, "Launch impulse");

        const ri::world::AnalyticsHeatmapVolume analytics = ri::world::CreateAnalyticsHeatmapVolume(
            MakeSeed("analytics_a", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{6.0f, 4.0f, 6.0f}),
            {.runtimeId = "analytics_heatmap", .type = "analytics_heatmap_volume", .size = {6.0f, 4.0f, 6.0f}});
        Expect(analytics.type == "analytics_heatmap_volume" && analytics.entryCount == 0U,
               "World volume descriptors should create typed analytics heatmap volumes");
    }

    ri::world::RuntimeEnvironmentService environmentService;
    ri::world::SpatialQueryTracker spatialTracker;
    environmentService.SetSpatialQueryTracker(&spatialTracker);

    ri::world::GenericTriggerVolume genericTrigger{};
    genericTrigger.id = "trigger_a";
    genericTrigger.type = "generic_trigger_volume";
    genericTrigger.shape = ri::world::VolumeShape::Sphere;
    genericTrigger.position = {0.0f, 0.0f, 0.0f};
    genericTrigger.radius = 1.5f;
    genericTrigger.size = {2.0f, 2.0f, 2.0f};
    genericTrigger.broadcastFrequency = 2.0;
    genericTrigger.onEnterEvent = "evt_trigger_enter";
    environmentService.SetGenericTriggerVolumes({genericTrigger});

    ri::world::SpatialQueryVolume spatialQuery{};
    spatialQuery.id = "query_a";
    spatialQuery.type = "spatial_query_volume";
    spatialQuery.shape = ri::world::VolumeShape::Cylinder;
    spatialQuery.position = {0.0f, 0.0f, 0.0f};
    spatialQuery.size = {2.0f, 4.0f, 2.0f};
    spatialQuery.radius = 1.0f;
    spatialQuery.height = 4.0f;
    spatialQuery.broadcastFrequency = 3.0;
    spatialQuery.filterMask = 7U;
    environmentService.SetSpatialQueryVolumes({spatialQuery});

    ri::world::StreamingLevelVolume streaming{};
    streaming.id = "streaming_a";
    streaming.shape = ri::world::VolumeShape::Box;
    streaming.position = {0.0f, 0.0f, 0.0f};
    streaming.size = {6.0f, 4.0f, 6.0f};
    streaming.targetLevel = "facility_b.ri_scene";
    environmentService.SetStreamingLevelVolumes({streaming});

    ri::world::CheckpointSpawnVolume checkpoint{};
    checkpoint.id = "checkpoint_a";
    checkpoint.shape = ri::world::VolumeShape::Box;
    checkpoint.position = {0.0f, 0.0f, 0.0f};
    checkpoint.size = {3.0f, 3.0f, 3.0f};
    checkpoint.targetLevel = "hub.ri_scene";
    checkpoint.respawn = {1.0f, 0.0f, 2.0f};
    checkpoint.respawnRotation = {0.0f, 1.57f, 0.0f};
    environmentService.SetCheckpointSpawnVolumes({checkpoint});

    ri::world::TeleportVolume teleport{};
    teleport.id = "teleport_a";
    teleport.shape = ri::world::VolumeShape::Box;
    teleport.position = {0.0f, 0.0f, 0.0f};
    teleport.size = {4.0f, 4.0f, 4.0f};
    teleport.targetId = "door_exit";
    teleport.targetPosition = {20.0f, 1.0f, -5.0f};
    teleport.targetRotation = {0.0f, 3.14f, 0.0f};
    teleport.offset = {0.0f, 1.0f, 0.0f};
    environmentService.SetTeleportVolumes({teleport});

    ri::world::LaunchVolume launch{};
    launch.id = "launch_a";
    launch.shape = ri::world::VolumeShape::Box;
    launch.position = {0.0f, 0.0f, 0.0f};
    launch.size = {4.0f, 4.0f, 4.0f};
    launch.impulse = {0.0f, 18.0f, 0.0f};
    launch.affectPhysics = false;
    environmentService.SetLaunchVolumes({launch});

    ri::world::RuntimeDiagnosticsLayer diagnostics{};
    diagnostics.SetVisible(true);
    diagnostics.Rebuild(environmentService);
    const ri::world::RuntimeDiagnosticsSnapshot firstDiagnostics = diagnostics.Snapshot();
    Expect(firstDiagnostics.visible && firstDiagnostics.revision > 0U,
           "Runtime diagnostics layer should track visibility and rebuild revisions");
    Expect(firstDiagnostics.helpers.size() == 6U,
           "Runtime diagnostics layer should build helpers for active trigger/teleport/launch diagnostics");

    genericTrigger.debugVisible = false;
    environmentService.SetGenericTriggerVolumes({genericTrigger});
    diagnostics.Rebuild(environmentService);
    const ri::world::RuntimeDiagnosticsSnapshot secondDiagnostics = diagnostics.Snapshot();
    Expect(secondDiagnostics.helpers.size() == 5U,
           "Runtime diagnostics layer should respect authored debugVisible metadata when rebuilding helpers");
    diagnostics.ToggleVisible();
    Expect(!diagnostics.IsVisible(),
           "Runtime diagnostics layer should support runtime visibility toggling");
    diagnostics.SetDebugHelpersRoot("editor.debug");
    diagnostics.SetDebugHelpersVisible(true);
    const ri::world::RuntimeDiagnosticsSnapshot rootedDiagnostics = diagnostics.Snapshot();
    Expect(rootedDiagnostics.debugHelpersVisible
               && rootedDiagnostics.debugHelpersRoot == "editor.debug",
           "Runtime diagnostics layer should expose debug helper visibility/root convenience fields");
    diagnostics.ToggleDebugHelpersVisible();
    Expect(!diagnostics.DebugHelpersVisible(),
           "Runtime diagnostics layer should keep helper-specific visibility toggles in sync");

    ri::world::AnalyticsHeatmapVolume analyticsA{};
    analyticsA.id = "analytics_a";
    analyticsA.shape = ri::world::VolumeShape::Box;
    analyticsA.position = {0.0f, 0.0f, 0.0f};
    analyticsA.size = {6.0f, 4.0f, 6.0f};

    ri::world::AnalyticsHeatmapVolume analyticsB{};
    analyticsB.id = "analytics_b";
    analyticsB.shape = ri::world::VolumeShape::Box;
    analyticsB.position = {20.0f, 0.0f, 0.0f};
    analyticsB.size = {6.0f, 4.0f, 6.0f};
    environmentService.SetAnalyticsHeatmapVolumes({analyticsA, analyticsB});

    const ri::world::StreamingLevelVolume* activeStreaming = environmentService.GetStreamingLevelVolumeAt({0.0f, 0.0f, 0.0f});
    Expect(activeStreaming != nullptr && activeStreaming->targetLevel == "facility_b.ri_scene",
           "World environment service should report the active streaming level volume at a position");
    const std::vector<const ri::world::GenericTriggerVolume*> activeGenericTriggers =
        environmentService.GetGenericTriggerVolumesAt({0.0f, 0.0f, 0.0f});
    Expect(activeGenericTriggers.size() == 1U && activeGenericTriggers.front()->id == "trigger_a",
           "World environment service should report active generic trigger volumes at a position");
    const std::vector<const ri::world::SpatialQueryVolume*> activeSpatialQueries =
        environmentService.GetSpatialQueryVolumesAt({0.0f, 0.0f, 0.0f});
    Expect(activeSpatialQueries.size() == 1U && activeSpatialQueries.front()->filterMask == 7U,
           "World environment service should report active spatial query volumes at a position");

    const ri::world::CheckpointSpawnVolume* activeCheckpoint =
        environmentService.GetCheckpointSpawnVolumeAt({0.0f, 0.0f, 0.0f});
    Expect(activeCheckpoint != nullptr && activeCheckpoint->id == "checkpoint_a",
           "World environment service should report the active checkpoint spawn volume at a position");

    const ri::world::TeleportVolume* activeTeleport = environmentService.GetTeleportVolumeAt({0.0f, 0.0f, 0.0f});
    Expect(activeTeleport != nullptr && activeTeleport->targetId == "door_exit",
           "World environment service should report the active teleport volume at a position");
    ExpectVec3(activeTeleport->offset, {0.0f, 1.0f, 0.0f},
               "World environment service should preserve teleport offsets");

    const ri::world::LaunchVolume* activeLaunch = environmentService.GetLaunchVolumeAt({0.0f, 0.0f, 0.0f});
    Expect(activeLaunch != nullptr && !activeLaunch->affectPhysics,
           "World environment service should report the active launch volume at a position");
    ExpectVec3(activeLaunch->impulse, {0.0f, 18.0f, 0.0f},
               "World environment service should preserve launch impulses");

    const std::vector<const ri::world::AnalyticsHeatmapVolume*> analyticsVolumes =
        environmentService.GetAnalyticsHeatmapVolumesAt({0.0f, 0.0f, 0.0f});
    Expect(analyticsVolumes.size() == 1U && analyticsVolumes.front()->id == "analytics_a",
           "World environment service should report overlapping analytics heatmap volumes by position");

    const std::size_t entryCount = environmentService.MarkAnalyticsHeatmapEntryAt({0.0f, 0.0f, 0.0f});
    Expect(entryCount == 1U,
           "World environment service should increment analytics heatmap entries for active trigger volumes");
    environmentService.AccumulateAnalyticsHeatmapTimeAt({0.0f, 0.0f, 0.0f}, 1.5);
    environmentService.AccumulateAnalyticsHeatmapTimeAt({40.0f, 0.0f, 0.0f}, 2.0);

    const std::vector<ri::world::AnalyticsHeatmapVolume>& storedAnalytics = environmentService.GetAnalyticsHeatmapVolumes();
    Expect(storedAnalytics[0].entryCount == 1U
               && NearlyEqual(static_cast<float>(storedAnalytics[0].dwellSeconds), 1.5f),
           "World environment service should accumulate analytics heatmap counters only for active volumes");
    Expect(storedAnalytics[1].entryCount == 0U
               && NearlyEqual(static_cast<float>(storedAnalytics[1].dwellSeconds), 0.0f),
           "World environment service should leave inactive analytics heatmap volumes untouched");

    ri::runtime::RuntimeEventBus eventBus;
    std::vector<std::string> triggerEvents;
    std::vector<std::string> triggerNamedEvents;
    eventBus.On("triggerChanged", [&triggerEvents](const ri::runtime::RuntimeEvent& event) {
        const std::string triggerId = event.fields.contains("triggerId") ? event.fields.at("triggerId") : "";
        const std::string state = event.fields.contains("state") ? event.fields.at("state") : "";
        triggerEvents.push_back(triggerId + ":" + state);
    });
    eventBus.On("triggerNamedEvent", [&triggerNamedEvents](const ri::runtime::RuntimeEvent& event) {
        const std::string eventId = event.fields.contains("value") ? event.fields.at("value") : "";
        triggerNamedEvents.push_back(eventId);
    });

    const ri::world::TriggerUpdateResult firstUpdate =
        environmentService.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 1.0, &eventBus);
    Expect(firstUpdate.transitions.size() == 7U,
           "World trigger runtime should report enter transitions for all overlapping trigger helper families");
    Expect(firstUpdate.streamingRequests.size() == 1U &&
               firstUpdate.checkpointRequests.size() == 1U &&
               firstUpdate.teleportRequests.size() == 1U &&
               firstUpdate.launchRequests.size() == 1U &&
               firstUpdate.analyticsEnteredVolumes.size() == 1U,
           "World trigger runtime should emit typed directives for trigger helper families on enter");
    Expect(triggerEvents.size() == 7U,
           "World trigger runtime should emit triggerChanged events for enter transitions");
    Expect(triggerNamedEvents.size() == 1U && triggerNamedEvents.front() == "evt_trigger_enter",
           "World trigger runtime should dispatch deterministic authored trigger events on enter transitions");
    const ri::world::SpawnStabilizationState spawnStabilization = environmentService.GetSpawnStabilizationState();
    Expect(spawnStabilization.pendingSpawnStabilization
               && NearlyEqual(spawnStabilization.anchor.x, checkpoint.respawn.x),
           "Checkpoint-trigger enter should arm spawn stabilization around checkpoint respawn");
    const ri::world::CameraShakePresentationState armedShake = environmentService.GetCameraShakePresentationState(1.0);
    Expect(armedShake.active && armedShake.shakeAmount > 0.0f,
           "Checkpoint-trigger enter should arm presentation-only camera shake feedback");
    ri::math::Vec3 freshSpawnPos{99.0f, 99.0f, 99.0f};
    ri::math::Vec3 freshSpawnVel{3.0f, -1.0f, 0.0f};
    Expect(environmentService.StabilizeFreshSpawnIfNeeded(1.01, freshSpawnPos, &freshSpawnVel)
               && NearlyEqual(freshSpawnPos.x, checkpoint.respawn.x)
               && NearlyEqual(freshSpawnVel.x, 0.0f),
           "Spawn stabilization helper should snap the first fresh spawn settle tick deterministically");
    environmentService.UpdatePresentationFeedback(2.0, 1.0 / 60.0);
    const ri::world::CameraShakePresentationState expiredShake = environmentService.GetCameraShakePresentationState(2.0);
    Expect(!expiredShake.active && NearlyEqual(expiredShake.shakeAmount, 0.0f),
           "Presentation feedback update should safely expire camera shake after shake-until window");
    const ri::world::SpatialQueryStats& firstStats = spatialTracker.GetStats();
    Expect(environmentService.GetTriggerIndexEntryCount() == 8U
               && firstStats.triggerIndexBuilds == 1U
               && firstStats.triggerPointQueries == 1U
               && firstStats.triggerCandidates == 7U,
           "World trigger runtime should build and query a native trigger spatial index");

    const ri::world::TriggerUpdateResult secondUpdate =
        environmentService.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 3.5, &eventBus);
    Expect(secondUpdate.transitions.size() == 2U &&
               secondUpdate.transitions[0].kind == ri::world::TriggerTransitionKind::Stay &&
               secondUpdate.transitions[1].kind == ri::world::TriggerTransitionKind::Stay,
           "World trigger runtime should report stay transitions when broadcast frequency elapses");
    Expect(triggerEvents.size() == 7U,
           "World trigger runtime should not emit triggerChanged events for stay transitions");
    const ri::world::SpatialQueryStats& secondStats = spatialTracker.GetStats();
    Expect(secondStats.triggerIndexBuilds == 1U
               && secondStats.triggerPointQueries == 2U
               && secondStats.triggerCandidates == 14U,
           "World trigger runtime should reuse the native trigger index across subsequent queries");

    const ri::world::TriggerUpdateResult exitUpdate =
        environmentService.UpdateTriggerVolumesAt({50.0f, 0.0f, 0.0f}, 5.0, &eventBus);
    Expect(exitUpdate.transitions.size() == 7U,
           "World trigger runtime should report exit transitions when leaving active trigger helper volumes");
    Expect(triggerEvents.size() == 14U,
           "World trigger runtime should emit triggerChanged events for exit transitions");
    Expect(environmentService.GetAnalyticsHeatmapVolumes().front().entryCount == 2U
               && NearlyEqual(static_cast<float>(environmentService.GetAnalyticsHeatmapVolumes().front().dwellSeconds),
                              4.0f),
           "World trigger runtime should accumulate analytics dwell across trigger updates");

    const double dwellBeforePause =
        environmentService.GetAnalyticsHeatmapVolumes().front().dwellSeconds;
    [[maybe_unused]] const ri::world::TriggerUpdateResult pausedStep =
        environmentService.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 10.0, nullptr, false);
    [[maybe_unused]] const ri::world::TriggerUpdateResult resumeStep =
        environmentService.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 10.2, nullptr, true);
    Expect(NearlyEqual(static_cast<float>(
                           environmentService.GetAnalyticsHeatmapVolumes().front().dwellSeconds - dwellBeforePause),
                       0.2f),
           "World trigger runtime should not accumulate dwell while simulation time is paused");

    environmentService.ResetAnalyticsHeatmapStatistics();
    const std::vector<ri::world::AnalyticsHeatmapVolume>& cleared = environmentService.GetAnalyticsHeatmapVolumes();
    Expect(cleared[0].entryCount == 0U && NearlyEqual(static_cast<float>(cleared[0].dwellSeconds), 0.0f)
               && !cleared[0].playerInside,
           "World environment service should clear analytics heatmap session counters on reset");

    const std::vector<ri::world::AnalyticsHeatmapExportRow> exportRows =
        ri::world::BuildAnalyticsHeatmapExportRows(environmentService, "level_a", "sess_1", "build_9");
    Expect(exportRows.size() == 2U && exportRows[0].volumeId == "analytics_a"
               && exportRows[0].levelId == "level_a" && exportRows[0].sessionId == "sess_1"
               && exportRows[0].buildId == "build_9",
           "World environment service should expose analytics heatmap rows for structured export");
    const ri::world::SpatialQueryStats& finalStats = spatialTracker.GetStats();
    Expect(finalStats.triggerIndexBuilds == 1U
               && finalStats.triggerPointQueries == 5U
               && finalStats.triggerCandidates == 28U
               && finalStats.triggerCandidatesScanned >= finalStats.triggerCandidates,
           "World trigger runtime should use indexed candidates and only carry active volumes for exits");

    const ri::world::RuntimeHelperMetricsSnapshot helperSnapshot = ri::world::BuildRuntimeHelperMetricsSnapshot(
        "session_trigger",
        ri::runtime::RuntimeEventBusMetrics{},
        ri::validation::SchemaValidationMetrics{},
        ri::audio::AudioManagerMetrics{},
        ri::world::RuntimeStatsOverlayMetrics{},
        ri::world::HelperActivityState{},
        environmentService,
        std::nullopt,
        ri::world::PostProcessState{});
    Expect(helperSnapshot.genericTriggerVolumes == 1U
           && helperSnapshot.spatialQueryVolumes == 1U
           && helperSnapshot.streamingLevelVolumes == 1U
           && helperSnapshot.checkpointSpawnVolumes == 1U
           && helperSnapshot.teleportVolumes == 1U
           && helperSnapshot.launchVolumes == 1U
           && helperSnapshot.analyticsHeatmapVolumes == 2U,
           "World helper metrics should report native trigger-helper volume counts");

    {
        ri::world::PostProcessVolume post{};
        post.id = "pp_env";
        post.type = "post_process_volume";
        post.shape = ri::world::VolumeShape::Box;
        post.position = {0.0f, 0.0f, 0.0f};
        post.size = {4.0f, 4.0f, 4.0f};
        environmentService.SetPostProcessVolumes({post});
        const ri::world::EnvironmentalVolumeUpdateResult entered = environmentService.UpdateEnvironmentalVolumesAt({0.0f, 0.0f, 0.0f});
        const ri::world::EnvironmentalVolumeUpdateResult exited = environmentService.UpdateEnvironmentalVolumesAt({8.0f, 0.0f, 0.0f});
        Expect(!entered.transitions.empty()
                   && entered.transitions.front().volumeId == "pp_env"
                   && entered.transitions.front().kind == ri::world::EnvironmentalTransitionKind::Enter,
               "Environmental volume updater should report deterministic enter transitions");
        Expect(!exited.transitions.empty()
                   && exited.transitions.front().volumeId == "pp_env"
                   && exited.transitions.front().kind == ri::world::EnvironmentalTransitionKind::Exit,
               "Environmental volume updater should report deterministic exit transitions");
    }

    {
        std::vector<ri::trace::TraceCollider> colliders;
        colliders.push_back({
            .id = "wall_a",
            .bounds = {.min = {-1.0f, -1.0f, 2.0f}, .max = {1.0f, 1.0f, 3.0f}},
            .structural = true,
            .dynamic = false,
        });
        ri::trace::TraceScene traceScene(std::move(colliders));
        const std::optional<ri::trace::TraceHit> rayHit =
            ri::trace::QueryBlockingRay(traceScene, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 10.0f);
        const std::optional<ri::trace::TraceHit> boxHit =
            ri::trace::QueryBlockingBox(traceScene, {.min = {-0.5f, -0.5f, 2.4f}, .max = {0.5f, 0.5f, 2.8f}});
        const std::optional<ri::trace::TraceHit> sweepHit = ri::trace::QueryBlockingSweep(
            traceScene,
            {.bounds = {.min = {-0.5f, -0.5f, 0.0f}, .max = {0.5f, 0.5f, 1.0f}},
             .delta = {0.0f, 0.0f, 3.0f}});
        const ri::trace::TraceSceneMetrics metrics = traceScene.Metrics();
        Expect(rayHit.has_value() && boxHit.has_value() && sweepHit.has_value(),
               "Trace query helpers should provide reusable ray/box/sweep obstruction checks");
        Expect(metrics.traceRayQueries >= 1U && metrics.traceBoxQueries >= 1U && metrics.sweptBoxQueries >= 1U,
               "Trace query helpers should integrate with scene metrics instrumentation counters");
    }

    {
        ri::structural::StructuralNode wall{};
        wall.id = "wall";
        wall.type = "box";
        wall.position = {0.0f, 0.0f, 0.0f};
        wall.scale = {4.0f, 2.0f, 1.0f};

        ri::structural::StructuralNode reconciler{};
        reconciler.id = "reconcile";
        reconciler.type = "non_manifold_reconciler";
        reconciler.position = {0.0f, 0.0f, 0.0f};
        reconciler.scale = {6.0f, 4.0f, 4.0f};
        reconciler.forceHull = true;

        ri::structural::StructuralCompileOptions cheapPass{};
        cheapPass.enableHighCostBooleanPasses = false;
        cheapPass.enableHighCostNonManifoldFallback = false;
        const ri::structural::StructuralGeometryCompileResult cheapResult =
            ri::structural::CompileStructuralGeometryNodes({wall, reconciler}, cheapPass);
        const auto cheapNodeIt =
            std::find_if(cheapResult.passthroughNodes.begin(),
                         cheapResult.passthroughNodes.end(),
                         [](const ri::structural::StructuralNode& node) { return node.id == "wall"; });
        Expect(cheapNodeIt != cheapResult.passthroughNodes.end()
                   && !cheapNodeIt->reconciledNonManifold
                   && cheapNodeIt->type == "box",
               "Non-manifold reconciliation should avoid hull fallback in cheap/default mode");

        ri::structural::StructuralCompileOptions highCostPass = cheapPass;
        highCostPass.enableHighCostNonManifoldFallback = true;
        const ri::structural::StructuralGeometryCompileResult highCostResult =
            ri::structural::CompileStructuralGeometryNodes({wall, reconciler}, highCostPass);
        const auto highCostNodeIt =
            std::find_if(highCostResult.passthroughNodes.begin(),
                         highCostResult.passthroughNodes.end(),
                         [](const ri::structural::StructuralNode& node) { return node.id == "wall"; });
        Expect(highCostNodeIt != highCostResult.passthroughNodes.end()
                   && highCostNodeIt->reconciledNonManifold
                   && highCostNodeIt->type == "convex_hull",
               "Non-manifold reconciliation should allow hull fallback only when high-cost fallback is enabled");
    }

    {
        ri::debug::RenderGameStateSnapshot snap{};
        snap.mode = "runtime";
        snap.level = "test_level";
        const std::filesystem::path tempRoot = std::filesystem::temp_directory_path() / "rawiron_snapshot_export_test";
        std::error_code exportEc;
        std::filesystem::create_directories(tempRoot, exportEc);
        const std::filesystem::path txtPath = tempRoot / "render_snapshot.txt";
        const std::filesystem::path jsonPath = tempRoot / "render_snapshot.json";
        std::string exportError;
        Expect(ri::debug::ExportRenderGameStateSnapshot(snap, txtPath, false, &exportError),
               "Runtime snapshot utility should export text snapshots");
        Expect(ri::debug::ExportRenderGameStateSnapshot(snap, jsonPath, true, &exportError),
               "Runtime snapshot utility should export JSON snapshots");
        Expect(std::filesystem::exists(txtPath) && std::filesystem::exists(jsonPath),
               "Runtime snapshot utility should create output files");
        std::filesystem::remove(txtPath, exportEc);
        std::filesystem::remove(jsonPath, exportEc);
        std::filesystem::remove_all(tempRoot, exportEc);
    }

    {
        ri::world::RuntimeEnvironmentService env;
        ri::world::GenericTriggerVolume vol{};
        vol.id = "vol_logic";
        vol.type = "generic_trigger_volume";
        vol.shape = ri::world::VolumeShape::Sphere;
        vol.position = {0.0f, 0.0f, 0.0f};
        vol.radius = 2.0f;
        vol.size = {2.0f, 2.0f, 2.0f};
        env.SetGenericTriggerVolumes({vol});

        ri::logic::LogicGraphSpec spec;
        spec.nodes.push_back(ri::logic::RelayNode{.id = "relay_from_vol", .def = {}});
        spec.routes.push_back({.sourceId = "vol_logic",
                               .outputName = "OnStartTouch",
                               .targets = {{.targetId = "relay_from_vol", .inputName = "Trigger"}}});
        ri::logic::LogicGraph graph(std::move(spec));
        int relayHits = 0;
        graph.SetOutputHandler([&](const ri::logic::LogicOutputEvent& ev) {
            if (ev.sourceId == "relay_from_vol" && ev.outputName == "ontrigger") {
                ++relayHits;
            }
        });
        const ri::logic::LogicContext trigCtx = ri::world::MakePlayerTriggerContext("pawn_1");
        [[maybe_unused]] const ri::world::TriggerUpdateResult logicStep =
            env.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 0.0, nullptr, true, &graph, &trigCtx);
        Expect(relayHits == 1, "Generic trigger enter should emit logic routes from volume id");
        Expect(graph.NowMs() == 0, "Trigger logic fan-out should not advance the logic clock");
    }

    {
        ri::world::RuntimeEnvironmentService env;
        ri::world::GenericTriggerVolume vol{};
        vol.id = "arm_test";
        vol.type = "generic_trigger_volume";
        vol.armed = false;
        vol.shape = ri::world::VolumeShape::Sphere;
        vol.position = {0.0f, 0.0f, 0.0f};
        vol.radius = 3.0f;
        vol.size = {2.0f, 2.0f, 2.0f};
        env.SetGenericTriggerVolumes({vol});
        const ri::world::TriggerUpdateResult disarmedStep =
            env.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 0.0, nullptr);
        const std::size_t genericEntersWhenDisarmed = std::count_if(
            disarmedStep.transitions.begin(),
            disarmedStep.transitions.end(),
            [](const ri::world::TriggerTransition& t) {
                return t.volumeId == "arm_test" && t.kind == ri::world::TriggerTransitionKind::Enter;
            });
        Expect(genericEntersWhenDisarmed == 0U,
               "Disarmed generic trigger should not emit enter while probe is inside geometry");
    }

    {
        ri::world::RuntimeEnvironmentService env;
        ri::world::GenericTriggerVolume vol{};
        vol.id = "gate_vol";
        vol.type = "generic_trigger_volume";
        vol.armed = true;
        vol.shape = ri::world::VolumeShape::Sphere;
        vol.position = {0.0f, 0.0f, 0.0f};
        vol.radius = 3.0f;
        vol.size = {2.0f, 2.0f, 2.0f};
        env.SetGenericTriggerVolumes({vol});

        ri::logic::LogicGraphSpec spec;
        spec.nodes.push_back(ri::logic::RelayNode{.id = "relay_gate", .def = {}});
        spec.routes.push_back({.sourceId = "relay_gate",
                               .outputName = "OnTrigger",
                               .targets = {{.targetId = "gate_vol", .inputName = "Disable"}}});
        ri::logic::LogicGraph graph(std::move(spec));
        ri::world::BindGenericTriggerVolumesToLogicGraph(graph, env);

        [[maybe_unused]] const ri::world::TriggerUpdateResult enterStep =
            env.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 0.0, nullptr);
        const bool hadEnter =
            std::any_of(enterStep.transitions.begin(), enterStep.transitions.end(), [](const ri::world::TriggerTransition& t) {
                return t.volumeId == "gate_vol" && t.kind == ri::world::TriggerTransitionKind::Enter;
            });
        Expect(hadEnter, "Armed volume should enter before logic disables it");

        ri::logic::LogicContext ctx;
        graph.DispatchInput("relay_gate", "Trigger", ctx);

        const ri::world::TriggerUpdateResult afterDisable =
            env.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 0.1, nullptr);
        const bool hadExit =
            std::any_of(afterDisable.transitions.begin(), afterDisable.transitions.end(), [](const ri::world::TriggerTransition& t) {
                return t.volumeId == "gate_vol" && t.kind == ri::world::TriggerTransitionKind::Exit;
            });
        Expect(hadExit, "Disabling while inside should synthesize an exit transition");

        graph.DispatchInput("gate_vol", "Enable", ctx);
        const ri::world::TriggerUpdateResult reenter =
            env.UpdateTriggerVolumesAt({0.0f, 0.0f, 0.0f}, 0.2, nullptr);
        const bool hadReenter =
            std::any_of(reenter.transitions.begin(), reenter.transitions.end(), [](const ri::world::TriggerTransition& t) {
                return t.volumeId == "gate_vol" && t.kind == ri::world::TriggerTransitionKind::Enter;
            });
        Expect(hadReenter, "Re-arming while still inside should allow a fresh enter");
    }
}

// --- merged from TestScalarClamp.cpp ---
namespace detail_ScalarClamp {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool NearlyEqual(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

}

 // namespace

void TestScalarClamp() {
    using namespace detail_ScalarClamp;
    using ri::math::ClampFinite;
    using ri::math::ClampFiniteToInt;

    Expect(NearlyEqual(ClampFinite(3.5, -1.0, 0.0, 10.0), 3.5), "Finite in-range value should pass through");
    Expect(NearlyEqual(ClampFinite(12.0, -1.0, 0.0, 10.0), 10.0), "Finite high should clamp to max");
    Expect(NearlyEqual(ClampFinite(-3.0, -1.0, 0.0, 10.0), 0.0), "Finite low should clamp to min");
    Expect(NearlyEqual(ClampFinite(std::numeric_limits<double>::quiet_NaN(), 7.0, 0.0, 10.0), 7.0),
           "NaN should yield fallback");
    Expect(NearlyEqual(ClampFinite(std::numeric_limits<double>::infinity(), 7.0, 0.0, 10.0), 7.0),
           "Infinity should yield fallback");

    Expect(NearlyEqual(ClampFinite(5.0, 0.0, 10.0, 0.0), 5.0), "Inverted bounds should normalize to [0,10]");
    Expect(NearlyEqual(ClampFinite(15.0, 0.0, 10.0, 0.0), 10.0), "Clamp should respect swapped bounds");

    Expect(ClampFiniteToInt(2.4, 0, 0, 10) == 2, "Integer clamp should round toward nearest");
    Expect(ClampFiniteToInt(2.6, 0, 0, 10) == 3, "Integer clamp should round .6 up");
    Expect(ClampFiniteToInt(std::numeric_limits<double>::quiet_NaN(), 4, 0, 10) == 4,
           "Integer clamp should use fallback on NaN");
}

// --- merged from TestScriptedCameraReview.cpp ---
namespace detail_ScriptedCameraReview {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestScriptedCameraReview() {
    using namespace detail_ScriptedCameraReview;
    ri::scene::ScriptedCameraSequence builtin = ri::scene::BuildDefaultStarterSandboxReview();
    Expect(!builtin.Steps().empty(), "Default starter review sequence should contain steps");

    ri::scene::StarterScene starter = ri::scene::BuildStarterScene("scripted_review_test");
    ri::scene::ScriptedCameraReviewPlayer player;
    player.Start(std::move(builtin));

    int iterations = 0;
    while (player.IsActive()) {
        player.Tick(starter.scene, starter.handles.orbitCamera, 1.0 / 60.0);
        ++iterations;
        Expect(iterations < 500000, "Scripted camera review should finish in bounded time");
    }
    Expect(player.Completed(), "Scripted camera review should report completion");

    static constexpr std::string_view kJson = R"json({
  "formatVersion": 1,
  "steps": [
    { "kind": "wait", "seconds": 0.01 },
    {
      "kind": "snapOrbit",
      "orbit": {
        "target": { "x": 0, "y": 1, "z": 0 },
        "distance": 6,
        "yawDegrees": 175,
        "pitchDegrees": -11
      }
    },
    {
      "kind": "moveOrbit",
      "durationSeconds": 0.05,
      "ease": "easeInOut",
      "orbit": {
        "target": { "x": 0.2, "y": 1.1, "z": 0.1 },
        "distance": 5.5,
        "yawDegrees": 350,
        "pitchDegrees": -8
      }
    },
    {
      "kind": "moveOrbit",
      "durationSeconds": 0.02,
      "ease": "linear",
      "orbit": {
        "target": { "x": 0, "y": 1, "z": 0 },
        "distance": 6,
        "yawDegrees": 10,
        "pitchDegrees": -10
      }
    },
    { "kind": "frameNodes", "nodeNames": ["Crate"], "padding": 1.4 }
  ]
})json";

    ri::scene::ScriptedCameraSequence parsed{};
    std::string parseError;
    Expect(ri::scene::TryParseScriptedCameraSequenceFromJson(kJson, parsed, &parseError),
           std::string("JSON sequence should parse: ") + parseError);
    Expect(parsed.Steps().size() == 5U, "Parsed sequence should preserve step count");

    ri::scene::ScriptedCameraSequence loopMeta{};
    static constexpr std::string_view kLoopFlagJson = R"({"formatVersion":1,"loop":true,"steps":[{"kind":"wait","seconds":0.001}]})";
    Expect(ri::scene::TryParseScriptedCameraSequenceFromJson(kLoopFlagJson, loopMeta, nullptr),
           "JSON loop metadata should parse");
    Expect(loopMeta.LoopPlayback(), R"(Top-level "loop": true should deserialize)");

    ri::scene::ScriptedCameraReviewPlayer jsonPlayer;
    jsonPlayer.Start(std::move(parsed));
    iterations = 0;
    while (jsonPlayer.IsActive()) {
        jsonPlayer.Tick(starter.scene, starter.handles.orbitCamera, 1.0 / 120.0);
        ++iterations;
        Expect(iterations < 500000, "JSON-driven review should finish");
    }

    ri::scene::ScriptedCameraSequence tinyLoop{};
    tinyLoop.AddWait(0.002);
    tinyLoop.SetLoopPlayback(true);
    ri::scene::ScriptedCameraReviewPlayer loopPlayer;
    loopPlayer.Start(std::move(tinyLoop));
    iterations = 0;
    while (loopPlayer.CompletedLoopCount() < 3U && iterations < 500000) {
        loopPlayer.Tick(starter.scene, starter.handles.orbitCamera, 1.0 / 60.0);
        ++iterations;
    }
    Expect(loopPlayer.CompletedLoopCount() >= 3U, "Loop playback should count completed cycles");
    loopPlayer.Stop();

    std::size_t stepHits = 0;
    ri::scene::ScriptedCameraSequence announce{};
    announce.AddSnapOrbit(starter.handles.orbitCamera.orbit);
    ri::scene::ScriptedCameraReviewPlayer verbosePlayer;
    verbosePlayer.SetStepBeganCallback([&stepHits](std::size_t, ri::scene::ScriptedReviewStepKind, std::string_view) {
        stepHits += 1;
    });
    verbosePlayer.Start(std::move(announce));
    verbosePlayer.Tick(starter.scene, starter.handles.orbitCamera, 1.0 / 60.0);
    Expect(stepHits == 1U, "Step-began callback should fire once per step entry");
}

// --- merged from TestSignalBroadcastState.cpp ---
namespace detail_SignalBroadcastState {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestSignalBroadcastState() {
    using namespace detail_SignalBroadcastState;
    ri::world::SignalBroadcastState state({
        .mode = ri::world::SignalBroadcastMode::Verbose,
        .allowGuidanceHints = true,
        .historyLimit = 3U,
    });

    state.Record({
        .message = "UNKNOWN VOICE: Leave now.",
        .subtitle = "UNVERIFIED SIGNAL: Leave now.",
        .guidanceHint = "Check the east corridor.",
        .durationMs = 4200.0,
    });
    Expect(state.ActiveMessage().has_value() && state.ActiveMessage()->text == "UNKNOWN VOICE: Leave now.",
           "Signal broadcast state should preserve the active message");
    Expect(state.ActiveSubtitle().has_value() && state.ActiveSubtitle()->text == "UNVERIFIED SIGNAL: Leave now.",
           "Signal broadcast state should expose subtitle callouts in verbose mode");
    Expect(state.ActiveGuidanceHint().has_value() && state.ActiveGuidanceHint()->text == "Check the east corridor.",
           "Signal broadcast state should expose guidance hints when enabled");

    state.Advance(5000.0);
    Expect(!state.ActiveMessage().has_value()
               && !state.ActiveSubtitle().has_value()
               && !state.ActiveGuidanceHint().has_value(),
           "Signal broadcast state should expire transient channels over time");

    state.SetPolicy({
        .mode = ri::world::SignalBroadcastMode::Disabled,
        .allowGuidanceHints = true,
        .historyLimit = 2U,
    });
    state.Record({
        .message = "Should not appear.",
        .subtitle = "Should not appear.",
        .guidanceHint = "Should not appear.",
        .durationMs = 3000.0,
    });
    Expect(!state.ActiveMessage().has_value()
               && !state.ActiveSubtitle().has_value()
               && !state.ActiveGuidanceHint().has_value()
               && state.History().empty(),
           "Signal broadcast state should be fully optional when disabled");
}

// --- merged from TestSnapshotFormatting.cpp ---
namespace detail_SnapshotFormatting {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool NearlyEqual(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

}

 // namespace

void TestSnapshotFormatting() {
    using namespace detail_SnapshotFormatting;
    using ri::debug::RoundSnapshotComponent;
    using ri::debug::RoundSnapshotScalar;
    using ri::debug::RoundSnapshotVec3;

    Expect(NearlyEqual(RoundSnapshotScalar(1.234567, 2), 1.23), "Two-decimal rounding");
    Expect(NearlyEqual(RoundSnapshotScalar(std::numeric_limits<double>::quiet_NaN(), 2), 0.0), "NaN → 0");
    Expect(NearlyEqual(RoundSnapshotScalar(std::numeric_limits<double>::infinity(), 2), 0.0), "Inf → 0");
    Expect(NearlyEqual(RoundSnapshotScalar(3.14159, -1), 0.0), "Negative places → 0");

    Expect(NearlyEqual(RoundSnapshotComponent(2.6f, 0), 3.0f), "Float component rounds to nearest integer");

    const ri::math::Vec3 v{1.111f, 2.222f, std::numeric_limits<float>::quiet_NaN()};
    const ri::math::Vec3 r = RoundSnapshotVec3(v, 2);
    Expect(NearlyEqual(r.x, 1.11f, 1e-3f) && NearlyEqual(r.y, 2.22f, 1e-3f) && NearlyEqual(r.z, 0.0f),
           "Vec3 rounds per axis; non-finite axis → 0");
}

// --- merged from TestStructuralDeferredOperations.cpp ---
namespace detail_StructuralDeferredOperations {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
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

ri::math::Vec3 GetMeshCenter(const ri::structural::CompiledMesh& mesh) {
    return (mesh.boundsMin + mesh.boundsMax) * 0.5f;
}

}

 // namespace

void TestStructuralDeferredOperations() {
    using namespace detail_StructuralDeferredOperations;
    ri::structural::CompiledGeometryNode scatterTargetA{};
    scatterTargetA.node = MakeNode("scatter_target_a", "Scatter Target A", "box");
    scatterTargetA.node.position = {0.0f, 0.0f, 0.0f};
    scatterTargetA.compiledMesh = ri::structural::BuildCompiledMeshFromConvexSolid(
        ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({0.0f, 0.0f, 0.0f})));

    ri::structural::CompiledGeometryNode scatterTargetB{};
    scatterTargetB.node = MakeNode("scatter_target_b", "Scatter Target B", "box");
    scatterTargetB.node.position = {4.0f, 0.0f, 4.0f};
    scatterTargetB.compiledMesh = ri::structural::BuildCompiledMeshFromConvexSolid(
        ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({4.0f, 0.0f, 4.0f})));

    ri::structural::StructuralDeferredTargetOperation scatterOperation{};
    scatterOperation.node = MakeNode("scatter_exec", "Scatter Exec", "surface_scatter_volume");
    scatterOperation.node.count = 4;
    scatterOperation.node.basePrimitive = std::make_shared<ri::structural::StructuralNode>(
        MakeNode("scatter_seed", "Scatter Seed", "box"));
    scatterOperation.node.basePrimitive->scale = {0.2f, 0.2f, 0.2f};
    scatterOperation.node.basePrimitive->position = {5.0f, 5.0f, 5.0f};
    scatterOperation.node.basePrimitive->rotation = {0.0f, 45.0f, 0.0f};
    scatterOperation.normalizedType = "surface_scatter_volume";
    scatterOperation.targetIds = {"scatter_target_a", "scatter_target_b"};

    const ri::structural::StructuralDeferredExecutionResult scatterResult =
        ri::structural::ExecuteStructuralDeferredTargetOperations({scatterOperation}, {scatterTargetA, scatterTargetB});

    std::vector<ri::structural::CompiledGeometryNode> scatteredNodes;
    for (const ri::structural::CompiledGeometryNode& node : scatterResult.nodes) {
        if (node.node.id.rfind("scatter_exec_", 0) == 0) {
            scatteredNodes.push_back(node);
        }
    }

    Expect(scatteredNodes.size() == 4U,
           "Structural deferred surface scatter execution should generate one compiled node per requested scatter instance");
    Expect(scatterResult.nodes.size() == 6U,
           "Structural deferred surface scatter execution should append generated scatter geometry without removing targets");
    for (const ri::structural::CompiledGeometryNode& node : scatteredNodes) {
        const ri::math::Vec3 center = GetMeshCenter(node.compiledMesh);
        const bool inTargetA = center.x >= -0.51f && center.x <= 0.51f && center.z >= -0.51f && center.z <= 0.51f;
        const bool inTargetB = center.x >= 3.49f && center.x <= 4.51f && center.z >= 3.49f && center.z <= 4.51f;
        Expect(NearlyEqual(center.y, 0.5f) && (inTargetA || inTargetB),
               "Structural deferred surface scatter execution should place generated instances on top of targeted compiled geometry bounds");
    }

    ri::structural::CompiledGeometryNode splineTarget{};
    splineTarget.node = MakeNode("rail_a", "Rail A", "box");
    splineTarget.node.position = {0.0f, 0.0f, 0.0f};
    splineTarget.compiledMesh = ri::structural::BuildCompiledMeshFromConvexSolid(
        ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({0.0f, 0.0f, 0.0f})));

    ri::structural::StructuralDeferredTargetOperation splineOperation{};
    splineOperation.node = MakeNode("spline_exec", "Spline Exec", "spline_mesh_deformer");
    splineOperation.node.count = 3;
    splineOperation.node.points = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 3.0f},
        {2.0f, 0.0f, 5.0f},
    };
    splineOperation.normalizedType = "spline_mesh_deformer";
    splineOperation.targetIds = {"rail_a"};

    const ri::structural::StructuralDeferredExecutionResult splineResult =
        ri::structural::ExecuteStructuralDeferredTargetOperations({splineOperation}, {splineTarget});

    Expect(std::find(splineResult.replacedTargetIds.begin(), splineResult.replacedTargetIds.end(), "rail_a")
               != splineResult.replacedTargetIds.end(),
           "Structural deferred spline-mesh execution should replace targeted source geometry by default");
    Expect(splineResult.nodes.size() == 3U,
           "Structural deferred spline-mesh execution should emit one compiled clone per spline sample when the source is hidden");
    const auto findSplineNode = [&](const std::string& id) -> const ri::structural::CompiledGeometryNode* {
        auto found = std::find_if(splineResult.nodes.begin(), splineResult.nodes.end(), [&](const ri::structural::CompiledGeometryNode& node) {
            return node.node.id == id;
        });
        return found != splineResult.nodes.end() ? &(*found) : nullptr;
    };
    const ri::structural::CompiledGeometryNode* firstSpline = findSplineNode("spline_exec_1_1");
    const ri::structural::CompiledGeometryNode* lastSpline = findSplineNode("spline_exec_1_3");
    Expect(firstSpline != nullptr && lastSpline != nullptr,
           "Structural deferred spline-mesh execution should generate stable native clone IDs");
    Expect(NearlyEqual(GetMeshCenter(firstSpline->compiledMesh).x, 0.0f)
               && NearlyEqual(GetMeshCenter(firstSpline->compiledMesh).z, 0.0f)
               && NearlyEqual(GetMeshCenter(lastSpline->compiledMesh).x, 2.0f)
               && NearlyEqual(GetMeshCenter(lastSpline->compiledMesh).z, 5.0f),
           "Structural deferred spline-mesh execution should place generated clones along the authored spline path endpoints");

    ri::structural::CompiledGeometryNode offsetSplineTarget{};
    offsetSplineTarget.node = MakeNode("rail_offset", "Rail Offset", "box");
    offsetSplineTarget.node.position = {2.0f, 0.0f, 0.0f};
    offsetSplineTarget.compiledMesh = ri::structural::BuildCompiledMeshFromConvexSolid(
        ri::structural::CreateWorldSpaceBoxSolid(ri::math::TranslationMatrix({2.5f, 0.0f, 0.0f})));

    ri::structural::StructuralDeferredTargetOperation offsetSplineOperation{};
    offsetSplineOperation.node = MakeNode("spline_offset", "Spline Offset", "spline_mesh_deformer");
    offsetSplineOperation.node.count = 2;
    offsetSplineOperation.node.points = {
        {0.0f, 0.0f, 0.0f},
        {4.0f, 0.0f, 0.0f},
    };
    offsetSplineOperation.normalizedType = "spline_mesh_deformer";
    offsetSplineOperation.targetIds = {"rail_offset"};

    const ri::structural::StructuralDeferredExecutionResult offsetSplineResult =
        ri::structural::ExecuteStructuralDeferredTargetOperations({offsetSplineOperation}, {offsetSplineTarget});
    const auto findOffsetSplineNode = [&](const std::string& id) -> const ri::structural::CompiledGeometryNode* {
        auto found = std::find_if(offsetSplineResult.nodes.begin(), offsetSplineResult.nodes.end(), [&](const ri::structural::CompiledGeometryNode& node) {
            return node.node.id == id;
        });
        return found != offsetSplineResult.nodes.end() ? &(*found) : nullptr;
    };
    const ri::structural::CompiledGeometryNode* offsetFirst = findOffsetSplineNode("spline_offset_1_1");
    const ri::math::Vec3 offsetCenter = offsetFirst != nullptr ? GetMeshCenter(offsetFirst->compiledMesh) : ri::math::Vec3{};
    Expect(offsetFirst != nullptr
               && NearlyEqual(ri::math::Distance(offsetCenter, {0.0f, 0.0f, 0.0f}), 0.5f),
           "Structural deferred spline-mesh execution should preserve the source mesh local offset instead of recentering authored geometry");

    ri::structural::StructuralDeferredTargetOperation keepSourceSpline = splineOperation;
    keepSourceSpline.node.id = "spline_keep";
    keepSourceSpline.node.keepSource = true;
    keepSourceSpline.node.count = 2;
    const ri::structural::StructuralDeferredExecutionResult keepSourceResult =
        ri::structural::ExecuteStructuralDeferredTargetOperations({keepSourceSpline}, {splineTarget});
    Expect(std::none_of(keepSourceResult.replacedTargetIds.begin(), keepSourceResult.replacedTargetIds.end(), [](const std::string& id) {
               return id == "rail_a";
           })
               && std::any_of(keepSourceResult.nodes.begin(), keepSourceResult.nodes.end(), [](const ri::structural::CompiledGeometryNode& node) {
                      return node.node.id == "rail_a";
                  }),
           "Structural deferred spline-mesh execution should preserve targeted source geometry when keepSource is enabled");

    ri::structural::CompiledGeometryNode ribbonProjectionTarget{};
    ribbonProjectionTarget.node = MakeNode("ribbon_ground", "Ribbon Ground", "box");
    ribbonProjectionTarget.compiledMesh = ri::structural::BuildCompiledMeshFromConvexSolid(
        ri::structural::CreateWorldSpaceBoxSolid(
            ri::math::Multiply(
                ri::math::TranslationMatrix({2.0f, 0.0f, 0.0f}),
                ri::math::ScaleMatrix({6.0f, 1.0f, 2.0f}))));

    ri::structural::StructuralDeferredTargetOperation ribbonOperation{};
    ribbonOperation.node = MakeNode("ribbon_exec", "Ribbon Exec", "spline_decal_ribbon");
    ribbonOperation.node.points = {
        {0.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {4.0f, 0.0f, 0.0f},
    };
    ribbonOperation.node.width = 1.0f;
    ribbonOperation.node.segments = 4;
    ribbonOperation.node.projectionHeight = 10.0f;
    ribbonOperation.node.projectionDistance = 20.0f;
    ribbonOperation.node.offsetY = 0.25f;
    ribbonOperation.normalizedType = "spline_decal_ribbon";

    const ri::structural::StructuralDeferredExecutionResult ribbonResult =
        ri::structural::ExecuteStructuralDeferredTargetOperations({ribbonOperation}, {ribbonProjectionTarget});
    const auto findRibbonNode = [&](const std::string& id) -> const ri::structural::CompiledGeometryNode* {
        auto found = std::find_if(ribbonResult.nodes.begin(), ribbonResult.nodes.end(), [&](const ri::structural::CompiledGeometryNode& node) {
            return node.node.id == id;
        });
        return found != ribbonResult.nodes.end() ? &(*found) : nullptr;
    };
    const ri::structural::CompiledGeometryNode* ribbonNode = findRibbonNode("ribbon_exec");
    Expect(ribbonNode != nullptr
               && ribbonNode->compiledMesh.triangleCount == 8U
               && ribbonNode->compiledMesh.hasBounds
               && NearlyEqual(ribbonNode->compiledMesh.boundsMin.y, 0.75f)
               && NearlyEqual(ribbonNode->compiledMesh.boundsMax.y, 0.75f),
           "Structural deferred spline-decal execution should generate a projected ribbon mesh over compiled structural bounds even without explicit target IDs");
}

// --- merged from TestStructuralPhaseClassification.cpp ---
namespace detail_StructuralPhaseClassification {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestStructuralPhaseClassification() {
    using namespace detail_StructuralPhaseClassification;
    using ri::structural::ClassifyStructuralPhase;
    using ri::structural::StructuralPhase;

    Expect(ClassifyStructuralPhase("portal") == StructuralPhase::Runtime, "portal should be runtime phase");
    Expect(ClassifyStructuralPhase("terrain_hole_cutout") == StructuralPhase::PostBuild,
           "terrain_hole_cutout should be post-build phase");
    Expect(ClassifyStructuralPhase("surface_velocity_primitive") == StructuralPhase::Frame,
           "surface_velocity_primitive should be frame phase");
    Expect(ClassifyStructuralPhase("box_primitive") == StructuralPhase::Compile, "unknown types compile by default");
    Expect(ClassifyStructuralPhase("") == StructuralPhase::Compile, "empty type string should compile");
}

// --- merged from TestSummarizeHelperActivity.cpp ---
namespace detail_SummarizeHelperActivity {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestSummarizeHelperActivity() {
    using namespace detail_SummarizeHelperActivity;
    using ri::world::SummarizeHelperActivity;

    Expect(SummarizeHelperActivity("  this   is   noisy   text  ", 12) == "this is noi~",
           "Whitespace collapse + truncation should match proto instrumentation");

    Expect(SummarizeHelperActivity("") == "none", "Empty input should become none");
    Expect(SummarizeHelperActivity("   \t\n  ") == "none", "Whitespace-only should become none");

    Expect(SummarizeHelperActivity("hello", 24) == "hello", "Short string should pass through");
    Expect(SummarizeHelperActivity("hello", 5) == "hello", "Exact max length should not add tilde");

    Expect(SummarizeHelperActivity("abcdef", 5) == "abcd~",
           "One past max length should use prefix max(1, maxLength-1) plus tilde");

    Expect(SummarizeHelperActivity("a\tb", 24) == "a b", "Tab should collapse to single space");
    Expect(SummarizeHelperActivity("x\r\ny", 24) == "x y", "CR/LF should collapse like spaces");
}

// --- merged from TestValueSchema.cpp ---
namespace detail_ValueSchema {

bool NearlyEqual(double lhs, double rhs) {
    return std::fabs(lhs - rhs) < 1e-9;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ri::validate::ValidationReport BranchNumber(const ri::content::Value& value, std::string path) {
    ri::validate::ValidationReport report;
    if (!value.IsNumber()) {
        report.Add(ri::validate::IssueCode::TypeMismatch, std::move(path), "expected number branch");
    }
    return report;
}

ri::validate::ValidationReport BranchString(const ri::content::Value& value, std::string path) {
    ri::validate::ValidationReport report;
    if (!value.IsString()) {
        report.Add(ri::validate::IssueCode::TypeMismatch, std::move(path), "expected string branch");
    }
    return report;
}

ri::validate::ValidationReport KindNumPayload(const ri::content::Value& value, std::string path) {
    ri::validate::ValidationReport report;
    const ri::content::Value::Object* object = value.TryGetObject();
    if (object == nullptr) {
        report.AddTypeMismatch(std::move(path),
                               "object",
                               ri::content::ValueKindDebugName(value));
        return report;
    }
    const auto it = object->find("payload");
    if (it == object->end()) {
        report.Add(ri::validate::IssueCode::MissingField, path + "/payload", "missing payload");
        return report;
    }
    if (!it->second.IsNumber()) {
        report.AddTypeMismatch(path + "/payload",
                               "number",
                               ri::content::ValueKindDebugName(it->second));
    }
    return report;
}

ri::validate::ValidationReport KindStrPayload(const ri::content::Value& value, std::string path) {
    ri::validate::ValidationReport report;
    const ri::content::Value::Object* object = value.TryGetObject();
    if (object == nullptr) {
        report.AddTypeMismatch(std::move(path),
                               "object",
                               ri::content::ValueKindDebugName(value));
        return report;
    }
    const auto it = object->find("payload");
    if (it == object->end()) {
        report.Add(ri::validate::IssueCode::MissingField, path + "/payload", "missing payload");
        return report;
    }
    if (!it->second.IsString()) {
        report.AddTypeMismatch(path + "/payload",
                               "string",
                               ri::content::ValueKindDebugName(it->second));
    }
    return report;
}

}

 // namespace

void TestValueSchema() {
    using namespace detail_ValueSchema;
    using ri::content::Value;

    ri::data::schema::ObjectShape shape{};
    shape.requiredKeys = {"id"};
    shape.optionalKeys = {"meta"};

    const Value notObject{42.0};
    const ri::validate::ValidationReport badRoot =
        ri::content::ValidateValueObjectShape(notObject, shape, {});
    Expect(!badRoot.Ok() && badRoot.issues[0].code == ri::validate::IssueCode::TypeMismatch
               && badRoot.issues[0].expectedType == "object" && badRoot.issues[0].actualType == "number",
           "Value object-shape validation should reject non-object roots");

    const Value okObject = Value::Object{
        {"id", Value("entity_a")},
        {"meta", Value(1.0)},
    };
    const ri::validate::ValidationReport okRep =
        ri::content::ValidateValueObjectShape(okObject, shape, "chunk");
    Expect(okRep.Ok(), "Value object-shape validation should accept matching object keys");

    const Value extraKey = Value::Object{
        {"id", Value("x")},
        {"unexpected", Value(true)},
    };
    const ri::validate::ValidationReport extraRep =
        ri::content::ValidateValueObjectShape(extraKey, shape, {});
    Expect(!extraRep.Ok() && extraRep.issues[0].code == ri::validate::IssueCode::UnknownKey,
           "Value object-shape validation should forbid unknown keys when policy requires");

    ri::data::schema::ObjectShape bagShape{};
    bagShape.requiredKeys = {"id"};
    bagShape.unknownPolicy = ri::data::schema::UnknownKeyPolicy::PassthroughBag;
    bagShape.extensionsBagKey = "extensions";
    const Value bagOk = Value::Object{
        {"id", Value("x")},
        {"extensions", Value(Value::Object{})},
    };
    Expect(ri::content::ValidateValueObjectShape(bagOk, bagShape, {}).Ok(),
           "PassthroughBag should permit the extensions key on value objects");

    const Value arrOk = Value(Value::Array{Value(1.0), Value(2.0)});
    Expect(ri::content::ValidateValueArrayShape(arrOk, 1U, 4U, ri::content::ValueSlotKind::Number, "/items").Ok(),
           "Array shape validation should accept homogeneous numeric elements");
    const Value arrShort = Value(Value::Array{Value(1.0)});
    Expect(!ri::content::ValidateValueArrayShape(arrShort, 2U, 4U, std::nullopt, "/items").Ok(),
           "Array shape validation should enforce size bounds");
    const Value arrBad = Value(Value::Array{Value(1.0), Value(true)});
    Expect(!ri::content::ValidateValueArrayShape(arrBad, 0U, 10U, ri::content::ValueSlotKind::Number, "/items").Ok(),
           "Array shape validation should reject element type mismatches");
    Expect(!ri::content::ValidateValueArrayShape(Value(3.0), 0U, 10U, std::nullopt, "/items").Ok(),
           "Array shape validation should reject non-array roots");
    const ri::validate::ValidationReport nonArrayRoot =
        ri::content::ValidateValueArrayShape(Value(3.0), 0U, 10U, std::nullopt, "/items");
    Expect(!nonArrayRoot.Ok() && nonArrayRoot.issues[0].expectedType == "array"
               && nonArrayRoot.issues[0].actualType == "number",
           "Array root failures should carry expected/actual kind hints");

    Expect(ri::content::ValueKindDebugName(Value(3.0)) == "number"
               && ri::content::ValueKindDebugName(Value(std::string("x"))) == "string",
           "Value kind debug names should stay stable for tools");

    Value::Object stripMe{{"id", Value("x")}, {"noise", Value(true)}};
    ri::content::StripUnknownKeysFromValueObject(stripMe, shape);
    Expect(stripMe.size() == 1U && stripMe.find("id") != stripMe.end(),
           "Strip should drop keys outside the declared object surface");

    Value stripValidate = Value(Value::Object{{"id", Value("y")}, {"trash", Value(true)}});
    const ri::validate::ValidationReport stripValRep =
        ri::content::StripThenValidateValueObjectShape(stripValidate, shape, "doc");
    Expect(stripValRep.Ok(), "Strip-then-validate should accept documents after normalization");
    const ri::content::Value::Object* stripped = stripValidate.TryGetObject();
    Expect(stripped != nullptr && stripped->size() == 1U,
           "Strip-then-validate should mutate the object in place");

    ri::data::schema::ObjectShape bagTop{};
    bagTop.requiredKeys = {"id"};
    bagTop.unknownPolicy = ri::data::schema::UnknownKeyPolicy::PassthroughBag;
    bagTop.extensionsBagKey = "extensions";
    Value::Object stripBag{{"id", Value("z")},
                           {"extensions", Value(Value::Object{})},
                           {"orphan", Value(1.0)}};
    ri::content::StripUnknownKeysFromValueObject(stripBag, bagTop);
    Expect(stripBag.size() == 2U && stripBag.find("extensions") != stripBag.end(),
           "Strip should retain the extensions bag key when declared");

    const Value nested = Value(Value::Object{
        {"child", Value(Value::Object{{"id", Value("a")}})},
    });
    Expect(ri::content::ValidateValueObjectShapeAtPath(nested, "child", shape, "doc").Ok(),
           "Path-based validation should resolve dotted object paths");
    const Value nestedBad = Value(Value::Object{
        {"child", Value(Value::Object{
            {"grandchild", Value(Value::Object{{"meta", Value(1.0)}})},
        })},
    });
    const ri::validate::ValidationReport nestedBadRep =
        ri::content::ValidateValueObjectShapeAtPath(nestedBad, "child.grandchild", shape, "doc");
    Expect(!nestedBadRep.Ok() && nestedBadRep.issues[0].path == "doc/child/grandchild/id",
           "Path-based validation should preserve the fully resolved nested path in reported issues");
    Expect(!ri::content::ValidateValueObjectShapeAtPath(nested, "missing", shape, "doc").Ok(),
           "Path-based validation should fail when a segment is absent");

    ri::data::schema::ObjectShape innerExt{};
    innerExt.requiredKeys = {"mod"};
    const Value extOk = Value(Value::Object{
        {"id", Value("i")},
        {"extensions", Value(Value::Object{{"mod", Value(1.0)}})},
    });
    Expect(ri::content::ValidateValueObjectShapeWithExtensions(extOk, bagTop, "extensions", innerExt, "root").Ok(),
           "Extensions object should validate against a nested object shape");
    const Value extBadType = Value(Value::Object{
        {"id", Value("i")},
        {"extensions", Value(3.0)},
    });
    Expect(!ri::content::ValidateValueExtensionsObjectShape(extBadType, "extensions", innerExt, "root").Ok(),
           "Extensions field must be an object when present");

    const Value tup = Value(Value::Array{Value(1.0), Value(std::string("x"))});
    Expect(ri::content::ValidateValueArrayTupleShape(
               tup,
               {ri::content::ValueSlotKind::Number, ri::content::ValueSlotKind::String},
               "/tuple")
               .Ok(),
           "Tuple validation should enforce per-index kinds");
    Expect(!ri::content::ValidateValueArrayTupleShape(Value(Value::Array{Value(1.0)}),
                                                      {ri::content::ValueSlotKind::Number,
                                                       ri::content::ValueSlotKind::String},
                                                      "/tuple")
                .Ok(),
           "Tuple validation should reject length mismatches");

    const Value keyOk = Value(Value::Object{{"aa", Value(1.0)}, {"bb", Value(2.0)}});
    Expect(ri::content::ValidateValueObjectKeyPattern(keyOk, "^[a-z]{2}$", "/props").Ok(),
           "Value object key regex should accept conforming property names");
    const Value keyBad = Value(Value::Object{{"ok", Value(1.0)}, {"BAD", Value(2.0)}});
    Expect(!ri::content::ValidateValueObjectKeyPattern(keyBad, "^[a-z]+$", "/props").Ok(),
           "Value object key regex should reject non-conforming property names");

    const ri::content::ValueUnionBranchValidator unionBranches[] = {BranchNumber, BranchString};
    const ri::validate::SafeParseResult<std::size_t> unionNum =
        ri::content::TryValidateValueOrderedUnion(Value(2.5), "/u", unionBranches);
    Expect(unionNum.value.has_value() && *unionNum.value == 0U && unionNum.report.Ok(),
           "Ordered union should accept the first matching branch");
    const ri::validate::SafeParseResult<std::size_t> unionStr =
        ri::content::TryValidateValueOrderedUnion(Value(std::string("hi")), "/u", unionBranches);
    Expect(unionStr.value.has_value() && *unionStr.value == 1U && unionStr.report.Ok(),
           "Ordered union should fall through to later branches");
    const ri::validate::SafeParseResult<std::size_t> unionMiss =
        ri::content::TryValidateValueOrderedUnion(Value(true), "/u", unionBranches);
    Expect(!unionMiss.value.has_value() && !unionMiss.report.Ok()
               && unionMiss.report.issues.back().code == ri::validate::IssueCode::NoUnionMatch,
           "Ordered union should surface a no-match code when every branch fails");
    Expect(unionMiss.report.issues[0].path.find("__branch") != std::string::npos,
           "Ordered union failures should be attributed to branch paths");

    const ri::content::StringDiscriminatedUnionBranch discBranches[] = {
        {std::string_view("num"), KindNumPayload},
        {std::string_view("str"), KindStrPayload},
    };
    const Value discOk = Value(Value::Object{
        {std::string("kind"), Value(std::string("num"))},
        {std::string("payload"), Value(7.0)},
    });
    const ri::validate::SafeParseResult<std::size_t> discParsed =
        ri::content::TryValidateValueStringDiscriminatedUnion(discOk, "/d", "kind", discBranches);
    Expect(discParsed.value.has_value() && *discParsed.value == 0U && discParsed.report.Ok(),
           "String discriminated union should dispatch on the discriminator field");
    const Value discBadTag = Value(Value::Object{
        {std::string("kind"), Value(std::string("nope"))},
    });
    const ri::validate::SafeParseResult<std::size_t> discUnknown =
        ri::content::TryValidateValueStringDiscriminatedUnion(discBadTag, "/d", "kind", discBranches);
    Expect(!discUnknown.value.has_value() && !discUnknown.report.Ok()
               && discUnknown.report.issues[0].code == ri::validate::IssueCode::InvalidEnum,
           "Unknown discriminators should fail validation");
    const Value discBadPayload = Value(Value::Object{
        {std::string("kind"), Value(std::string("num"))},
        {std::string("payload"), Value(std::string("x"))},
    });
    const ri::validate::SafeParseResult<std::size_t> discPayloadFail =
        ri::content::TryValidateValueStringDiscriminatedUnion(discBadPayload, "/d", "kind", discBranches);
    Expect(!discPayloadFail.value.has_value() && !discPayloadFail.report.Ok()
               && discPayloadFail.report.issues[0].code == ri::validate::IssueCode::TypeMismatch,
           "Discriminated union should run branch validation after tag match");

    const std::vector<ri::content::PatchFieldSpec> patchSpecs{
        ri::content::PatchFieldSpec{.key = "name", .valueKind = ri::content::ValueSlotKind::String},
        ri::content::PatchFieldSpec{.key = "count", .valueKind = ri::content::ValueSlotKind::Number},
    };
    const Value patchOk =
        Value(Value::Object{{"name", Value(std::string("x"))}, {"count", Value(1.0)}});
    Expect(ri::content::ValidateValuePatchObject(patchOk, "/patch", std::span(patchSpecs)).Ok(),
           "Patch validation should accept allowed keys with matching value kinds");
    const Value patchBadKey = Value(Value::Object{{"surprise", Value(true)}});
    Expect(!ri::content::ValidateValuePatchObject(patchBadKey, "/patch", std::span(patchSpecs)).Ok(),
           "Patch validation should reject undeclared keys");
    const Value patchBadVal = Value(Value::Object{{"name", Value(9.0)}});
    const ri::validate::ValidationReport patchType = ri::content::ValidateValuePatchObject(
        patchBadVal, "/patch", std::span(patchSpecs));
    Expect(!patchType.Ok() && patchType.issues[0].code == ri::validate::IssueCode::TypeMismatch
               && patchType.issues[0].expectedType == "string",
           "Patch validation should type-check declared fields");

    Value::Object defaultsTest{};
    const std::vector<std::pair<std::string, Value>> defs = {
        {"alpha", Value(10.0)},
        {"beta", Value(std::string("x"))},
    };
    ri::content::ApplyValueObjectDefaults(defaultsTest, defs);
    Expect(defaultsTest.size() == 2U && defaultsTest["alpha"].IsNumber(),
           "Defaults should insert only missing keys");

    ri::content::ApplyValueObjectDefaults(defaultsTest, {{"alpha", Value(99.0)}});
    Expect(NearlyEqual(*defaultsTest["alpha"].TryGetNumber(), 10.0),
           "Defaults should not overwrite existing keys");

    Value::Object renameObj = Value::Object{{"legacy", Value(true)}, {"keep", Value(1.0)}};
    ri::content::RenameKeyInValueObject(renameObj, "legacy", "modern");
    Expect(renameObj.find("legacy") == renameObj.end() && renameObj["modern"].IsBoolean(),
           "Key rename should move values when destination is free");

    const Value discObj = Value::Object{{"kind", Value(std::string("sphere"))}, {"radius", Value(2.0)}};
    const ri::validate::SafeParseResult<std::string> disc =
        ri::content::TryReadDiscriminatorString(discObj, "kind", "item");
    Expect(disc.value.has_value() && *disc.value == "sphere" && disc.report.Ok(),
           "Discriminator read should return string branch ids");

    const Value discMissing = Value::Object{{"radius", Value(1.0)}};
    Expect(!ri::content::TryReadDiscriminatorString(discMissing, "kind", "item").value.has_value(),
           "Discriminator read should fail when field is absent");

    const Value discBadType = Value::Object{{"kind", Value(3.0)}};
    Expect(!ri::content::TryReadDiscriminatorString(discBadType, "kind", "item").value.has_value(),
           "Discriminator read should require string discriminators");

    Expect(ri::validate::ValidateHexString("deadbeef", 8U, "/hash").Ok(),
           "Hex validator should accept matching-length lowercase hashes");
    Expect(!ri::validate::ValidateHexString("deadbeez", 8U, "/hash").Ok(),
           "Hex validator should reject non-hex characters");

    Expect(ri::validate::ValidateUuidString("550e8400-e29b-41d4-a716-446655440000", "/id").Ok(),
           "UUID validator should accept canonical hyphenated form");
    Expect(!ri::validate::ValidateUuidString("not-a-uuid", "/id").Ok(),
           "UUID validator should reject malformed strings");
}

// --- merged from TestVolumeContainment.cpp ---
namespace detail_VolumeContainment {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}

 // namespace

void TestVolumeContainment() {
    using namespace detail_VolumeContainment;
    using ri::math::Vec3;
    using ri::spatial::AuthoringVolumeDesc;
    using ri::spatial::PointInsideAuthoringVolume;

    AuthoringVolumeDesc box{};
    box.shape = AuthoringVolumeDesc::Shape::Box;
    box.center = {0.0f, 1.0f, 0.0f};
    box.boxSize = {4.0f, 2.0f, 4.0f};

    Expect(PointInsideAuthoringVolume(Vec3{0.0f, 1.0f, 0.0f}, box), "Box should contain its center");
    Expect(PointInsideAuthoringVolume(Vec3{1.9f, 1.9f, 1.9f}, box), "Box should contain interior point");
    Expect(!PointInsideAuthoringVolume(Vec3{2.1f, 1.0f, 0.0f}, box), "Box should reject outside X");
    Expect(!PointInsideAuthoringVolume(Vec3{std::numeric_limits<float>::quiet_NaN(), 1.0f, 0.0f}, box),
           "Non-finite query point should not hit");

    AuthoringVolumeDesc cyl{};
    cyl.shape = AuthoringVolumeDesc::Shape::CylinderY;
    cyl.center = {0.0f, 2.0f, 0.0f};
    cyl.cylinderRadius = 1.0f;
    cyl.cylinderHeight = 2.0f;
    Expect(PointInsideAuthoringVolume(Vec3{0.5f, 2.0f, 0.5f}, cyl), "Cylinder should contain interior point");
    Expect(!PointInsideAuthoringVolume(Vec3{2.0f, 2.0f, 0.0f}, cyl), "Cylinder should reject outside radius");
    Expect(!PointInsideAuthoringVolume(Vec3{0.0f, 3.1f, 0.0f}, cyl), "Cylinder should reject outside height");

    AuthoringVolumeDesc sphere{};
    sphere.shape = AuthoringVolumeDesc::Shape::Sphere;
    sphere.center = {10.0f, 0.0f, -3.0f};
    sphere.sphereRadius = 2.0f;
    Expect(PointInsideAuthoringVolume(Vec3{11.0f, 0.0f, -3.0f}, sphere), "Sphere should contain interior");
    Expect(!PointInsideAuthoringVolume(Vec3{13.0f, 0.0f, -3.0f}, sphere), "Sphere should reject exterior");
    const float r = sphere.sphereRadius;
    Expect(PointInsideAuthoringVolume(Vec3{10.0f + r, 0.0f, -3.0f}, sphere),
           "Sphere containment should include boundary (squared test)");

    AuthoringVolumeDesc badSphere = sphere;
    badSphere.sphereRadius = std::numeric_limits<float>::quiet_NaN();
    Expect(!PointInsideAuthoringVolume(Vec3{10.0f, 0.0f, -3.0f}, badSphere), "NaN sphere radius should miss");
}

// --- merged from TestWorldVolumeDescriptors.cpp ---
namespace detail_WorldVolumeDescriptors {

bool NearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void ExpectVec3(const ri::math::Vec3& actual, const ri::math::Vec3& expected, const std::string& label) {
    const bool matches = NearlyEqual(actual.x, expected.x) &&
                         NearlyEqual(actual.y, expected.y) &&
                         NearlyEqual(actual.z, expected.z);
    Expect(matches,
           label + " expected (" + std::to_string(expected.x) + ", " + std::to_string(expected.y) + ", " +
               std::to_string(expected.z) + ") but got (" + std::to_string(actual.x) + ", " +
               std::to_string(actual.y) + ", " + std::to_string(actual.z) + ")");
}

ri::world::RuntimeVolumeSeed MakeSeed(std::string id = {},
                                      std::string type = {},
                                      std::optional<ri::world::VolumeShape> shape = std::nullopt,
                                      std::optional<ri::math::Vec3> position = std::nullopt,
                                      std::optional<ri::math::Vec3> size = std::nullopt,
                                      std::optional<float> radius = std::nullopt,
                                      std::optional<float> height = std::nullopt) {
    ri::world::RuntimeVolumeSeed seed{};
    seed.id = std::move(id);
    seed.type = std::move(type);
    seed.shape = shape;
    seed.position = position;
    seed.size = size;
    seed.radius = radius;
    seed.height = height;
    return seed;
}

}

 // namespace

void TestWorldVolumeDescriptors() {
    using namespace detail_WorldVolumeDescriptors;
    using ri::world::ClipVolumeMode;
    using ri::world::CollisionChannel;
    using ri::world::ConstraintAxis;

    const std::vector<ClipVolumeMode> clipModes = ri::world::ParseClipVolumeModes({
        "physics",
        "AI",
        "physics",
        "bogus",
        "visibility",
    });
    Expect(clipModes.size() == 3U,
           "World volume descriptors should keep unique valid clip modes in authoring order");
    Expect(clipModes[0] == ClipVolumeMode::Physics &&
               clipModes[1] == ClipVolumeMode::AI &&
               clipModes[2] == ClipVolumeMode::Visibility,
           "World volume descriptors should parse prototype clip mode strings");
    Expect(ri::world::ParseClipVolumeModes({}).size() == 1U &&
               ri::world::ParseClipVolumeModes({}).front() == ClipVolumeMode::Physics,
           "World volume descriptors should default clip modes to physics");
    Expect(ri::world::ToString(ClipVolumeMode::Visibility) == "visibility",
           "World volume descriptors should stringify clip modes");

    const std::vector<CollisionChannel> channels = ri::world::ParseCollisionChannels({
        "camera",
        "Player",
        "camera",
        "vehicle",
        "garbage",
    });
    Expect(channels.size() == 3U,
           "World volume descriptors should keep unique valid collision channels in authoring order");
    Expect(channels[0] == CollisionChannel::Camera &&
               channels[1] == CollisionChannel::Player &&
               channels[2] == CollisionChannel::Vehicle,
           "World volume descriptors should parse prototype collision-channel strings");
    Expect(ri::world::ParseCollisionChannels({}).size() == 1U &&
               ri::world::ParseCollisionChannels({}).front() == CollisionChannel::Player,
           "World volume descriptors should default collision channels to player");
    Expect(ri::world::ToString(CollisionChannel::Bullet) == "bullet",
           "World volume descriptors should stringify collision channels");

    const std::vector<ConstraintAxis> axes = ri::world::ParseConstraintAxes({
        "x",
        "yz",
        "Z",
        "x",
        "bogus",
    });
    Expect(axes.size() == 3U &&
               axes[0] == ConstraintAxis::X &&
               axes[1] == ConstraintAxis::Y &&
               axes[2] == ConstraintAxis::Z,
           "World volume descriptors should parse axis groups for physics constraints");
    Expect(ri::world::ToString(ConstraintAxis::Y) == "y",
           "World volume descriptors should stringify constraint axes");
    Expect(ri::world::ToString(ri::world::TraversalLinkKind::Climb) == "climb_volume",
           "World volume descriptors should stringify traversal-link kinds");
    Expect(ri::world::ToString(ri::world::HintPartitionMode::Skip) == "skip",
           "World volume descriptors should stringify hint partition modes");
    Expect(ri::world::ToString(ri::world::ForcedLod::Far) == "far",
           "World volume descriptors should stringify forced lod values");
    Expect(ri::world::ToString(ri::world::VisibilityPrimitiveKind::OcclusionPortal) == "occlusion_portal",
           "World volume descriptors should stringify visibility primitive kinds");
    Expect(ri::world::ToString(ri::world::TriggerTransitionKind::Stay) == "stay",
           "World volume descriptors should stringify trigger transition kinds");

    ri::world::RuntimeVolumeSeed seed{};
    seed.position = ri::math::Vec3{1.0f, 2.0f, 3.0f};
    seed.rotationRadians = ri::math::Vec3{0.2f, 0.0f, 0.0f};
    seed.size = ri::math::Vec3{-6.0f, 0.0f, -4.0f};
    seed.shape = ri::world::VolumeShape::Cylinder;

    const ri::world::RuntimeVolume baseVolume = ri::world::CreateRuntimeVolume(seed, {
        .runtimeId = "camera_modifier",
        .type = "camera_modifier_volume",
        .size = {5.0f, 5.0f, 5.0f},
    });
    Expect(baseVolume.id.rfind("camera_modifier_", 0) == 0,
           "World volume descriptors should synthesize runtime IDs from defaults");
    Expect(baseVolume.type == "camera_modifier_volume",
           "World volume descriptors should preserve the fallback runtime type");
    Expect(baseVolume.shape == ri::world::VolumeShape::Cylinder,
           "World volume descriptors should preserve explicit shapes");
    ExpectVec3(baseVolume.position, {1.0f, 2.0f, 3.0f}, "Runtime volume position");
    ExpectVec3(baseVolume.size, {6.0f, 0.0f, 4.0f}, "Runtime volume size");
    Expect(NearlyEqual(baseVolume.radius, 3.0f),
           "World volume descriptors should derive runtime radius from size when omitted");
    Expect(NearlyEqual(baseVolume.height, 2.0f),
           "World volume descriptors should fall back zero-height cylinders to the prototype default");

    const ri::world::FilteredCollisionVolume filteredCollision = ri::world::CreateFilteredCollisionVolume(
        MakeSeed("camera_block", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}),
        {"camera", "physics", "camera"},
        {.runtimeId = "filtered_collision", .type = "filtered_collision_volume", .size = {2.0f, 2.0f, 2.0f}});
    Expect(filteredCollision.channels.size() == 2U,
           "World volume descriptors should parse filtered collision channels");
    Expect(ri::world::TraceTagMatchesVolume("camera", filteredCollision),
           "World volume descriptors should match trace tags against parsed channels");
    Expect(!ri::world::TraceTagMatchesVolume("player", filteredCollision),
           "World volume descriptors should reject missing trace channels");

    const ri::world::ClipRuntimeVolume clipVolume = ri::world::CreateClipRuntimeVolume(
        MakeSeed("ai_clip"),
        {"ai", "visibility", "ai"},
        false,
        {.runtimeId = "clip_volume", .type = "ai_perception_blocker_volume", .size = {2.0f, 2.0f, 2.0f}});
    Expect(clipVolume.modes.size() == 2U &&
               clipVolume.modes[0] == ClipVolumeMode::AI &&
               clipVolume.modes[1] == ClipVolumeMode::Visibility,
           "World volume descriptors should parse clip-runtime modes");
    Expect(!clipVolume.enabled,
           "World volume descriptors should preserve enabled state for clip-runtime volumes");

    const ri::world::DamageVolume damageVolume = ri::world::CreateDamageVolume(
        MakeSeed("lava", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}),
        9000.0f,
        false,
        {},
        {.runtimeId = "damage_volume", .type = "damage_volume", .size = {4.0f, 4.0f, 4.0f}});
    Expect(NearlyEqual(damageVolume.damagePerSecond, 5000.0f),
           "World volume descriptors should clamp non-lethal damage volumes like the prototype");
    Expect(damageVolume.label == "lava",
           "World volume descriptors should fall back damage labels to runtime IDs");

    const ri::world::DamageVolume killVolume = ri::world::CreateDamageVolume(
        MakeSeed("", "kill_volume"),
        18.0f,
        true,
        "kill",
        {.runtimeId = "kill_volume", .type = "kill_volume", .size = {4.0f, 4.0f, 4.0f}});
    Expect(killVolume.killInstant && NearlyEqual(killVolume.damagePerSecond, 9999.0f),
           "World volume descriptors should map kill volumes to lethal damage");

    const ri::world::PhysicsConstraintVolume constraintVolume = ri::world::CreatePhysicsConstraintVolume(
        MakeSeed("constraint_a"),
        {"y", "x", "bad", "y"},
        {.runtimeId = "physics_constraint", .type = "physics_constraint_volume", .size = {6.0f, 4.0f, 6.0f}});
    Expect(constraintVolume.lockAxes.size() == 2U &&
               constraintVolume.lockAxes[0] == ConstraintAxis::Y &&
               constraintVolume.lockAxes[1] == ConstraintAxis::X,
           "World volume descriptors should create typed physics-constraint axes");

    const ri::world::WaterSurfacePrimitive waterSurface = ri::world::CreateWaterSurfacePrimitive(
        MakeSeed("water_surface_a", "", std::nullopt, ri::math::Vec3{0.0f, 1.0f, 0.0f}, ri::math::Vec3{10.0f, 0.5f, 10.0f}),
        9.0f,
        -1.0f,
        50.0f,
        true,
        {.runtimeId = "water_surface", .type = "water_surface_primitive", .size = {8.0f, 0.5f, 8.0f}});
    Expect(waterSurface.type == "water_surface_primitive" &&
               NearlyEqual(waterSurface.waveAmplitude, 4.0f) &&
               NearlyEqual(waterSurface.waveFrequency, 0.0f) &&
               NearlyEqual(waterSurface.flowSpeed, 30.0f) &&
               waterSurface.blocksUnderwaterFog,
           "World volume descriptors should sanitize authored water-surface primitive dynamics");

    const ri::world::KinematicTranslationPrimitive kinematicTranslation =
        ri::world::CreateKinematicTranslationPrimitive(
            MakeSeed("kin_tx_a"),
            {0.0f, 0.0f, 0.0f},
            5000.0f,
            0.01f,
            false,
            {.runtimeId = "kinematic_translation", .type = "kinematic_translation_primitive", .size = {2.0f, 1.0f, 2.0f}});
    Expect(kinematicTranslation.type == "kinematic_translation_primitive" &&
               NearlyEqual(kinematicTranslation.axis.x, 1.0f) &&
               NearlyEqual(kinematicTranslation.distance, 1024.0f) &&
               NearlyEqual(kinematicTranslation.cycleSeconds, 0.1f) &&
               !kinematicTranslation.pingPong,
           "World volume descriptors should sanitize authored kinematic translation primitives");

    const ri::world::KinematicRotationPrimitive kinematicRotation =
        ri::world::CreateKinematicRotationPrimitive(
            MakeSeed("kin_rot_a"),
            {0.0f, 0.0f, 0.0f},
            -9999.0f,
            9999.0f,
            true,
            {.runtimeId = "kinematic_rotation", .type = "kinematic_rotation_primitive", .size = {2.0f, 2.0f, 2.0f}});
    Expect(kinematicRotation.type == "kinematic_rotation_primitive" &&
               NearlyEqual(kinematicRotation.axis.y, 1.0f) &&
               NearlyEqual(kinematicRotation.angularSpeedDegreesPerSecond, -1440.0f) &&
               NearlyEqual(kinematicRotation.maxAngleDegrees, 360.0f) &&
               kinematicRotation.pingPong,
           "World volume descriptors should sanitize authored kinematic rotation primitives");

    const ri::world::SplinePathFollowerPrimitive splineFollower =
        ri::world::CreateSplinePathFollowerPrimitive(
            MakeSeed("spline_follower_a"),
            {{0.0f, 0.0f, 0.0f}, {4.0f, 1.0f, 0.0f}},
            99.0f,
            false,
            99.0f,
            {.runtimeId = "spline_path_follower", .type = "spline_path_follower_primitive", .size = {2.0f, 2.0f, 2.0f}});
    Expect(splineFollower.type == "spline_path_follower_primitive" &&
               splineFollower.splinePoints.size() == 2U &&
               NearlyEqual(splineFollower.speedUnitsPerSecond, 99.0f) &&
               !splineFollower.loop &&
               NearlyEqual(splineFollower.phaseOffset, 10.0f),
           "World volume descriptors should sanitize spline-path follower primitive playback tuning");

    const ri::world::CablePrimitive cable = ri::world::CreateCablePrimitive(
        MakeSeed("cable_a"),
        {0.0f, 1.0f, 0.0f},
        {0.0f, -3.0f, 0.0f},
        9.0f,
        -8.0f,
        true,
        {.runtimeId = "cable", .type = "cable_primitive", .size = {1.0f, 2.0f, 1.0f}});
    Expect(cable.type == "cable_primitive" &&
               NearlyEqual(cable.swayAmplitude, 4.0f) &&
               NearlyEqual(cable.swayFrequency, 0.0f) &&
               cable.collisionEnabled,
           "World volume descriptors should sanitize cable primitive sway and collision tuning");

    const ri::world::ClippingRuntimeVolume clipping = ri::world::CreateClippingRuntimeVolume(
        MakeSeed("clip_a"),
        {},
        false,
        {.runtimeId = "clipping", .type = "clipping_volume", .size = {4.0f, 4.0f, 4.0f}});
    Expect(clipping.type == "clipping_volume" &&
               clipping.modes.size() == 1U &&
               clipping.modes.front() == "visibility" &&
               !clipping.enabled,
           "World volume descriptors should create clipping runtime volumes with defaults");

    const ri::world::FilteredCollisionRuntimeVolume filtered = ri::world::CreateFilteredCollisionRuntimeVolume(
        MakeSeed("filtered_runtime_a"),
        {},
        {.runtimeId = "filtered_collision_runtime", .type = "filtered_collision_volume", .size = {2.0f, 2.0f, 2.0f}});
    Expect(filtered.type == "filtered_collision_volume" &&
               filtered.channels.size() == 1U &&
               filtered.channels.front() == "player",
           "World volume descriptors should create filtered-collision runtime volumes with safe defaults");

    const ri::world::TraversalLinkVolume climbLink = ri::world::CreateTraversalLinkVolume(
        MakeSeed("climb_a", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{2.0f, 5.0f, 2.0f}),
        ri::world::TraversalLinkKind::Climb,
        20.0f,
        {.runtimeId = "climb", .type = "climb_volume", .size = {2.0f, 4.0f, 2.0f}});
    Expect(climbLink.kind == ri::world::TraversalLinkKind::Climb &&
               climbLink.type == "climb_volume" &&
               NearlyEqual(climbLink.climbSpeed, 12.0f),
           "World volume descriptors should create typed traversal-link volumes");

    const ri::world::LocalGridSnapVolume snapVolume = ri::world::CreateLocalGridSnapVolume(
        MakeSeed("snap_a", "", std::nullopt, std::nullopt, ri::math::Vec3{6.0f, 4.0f, 6.0f}),
        32.0f,
        false,
        true,
        false,
        9000,
        {.runtimeId = "local_grid_snap", .type = "local_grid_snap_volume", .size = {6.0f, 4.0f, 6.0f}});
    Expect(snapVolume.type == "local_grid_snap_volume" && NearlyEqual(snapVolume.snapSize, 16.0f),
           "World volume descriptors should create and clamp local-grid-snap volumes");
    Expect(!snapVolume.snapX && snapVolume.snapY && !snapVolume.snapZ && snapVolume.priority == 1000,
           "World volume descriptors should preserve snap axes and clamp local-grid-snap priority");

    const ri::world::HintPartitionVolume hintPartition = ri::world::CreateHintPartitionVolume(
        MakeSeed("hint_a"),
        ri::world::HintPartitionMode::Skip,
        {.runtimeId = "hint_skip", .type = "hint_skip_brush", .size = {6.0f, 4.0f, 6.0f}});
    Expect(hintPartition.type == "hint_skip_brush" &&
               hintPartition.mode == ri::world::HintPartitionMode::Skip,
           "World volume descriptors should create typed hint partition volumes");

    const ri::world::CameraConfinementVolume confinement = ri::world::CreateCameraConfinementVolume(
        MakeSeed("camera_box"),
        {.runtimeId = "camera_confinement", .type = "camera_confinement_volume", .size = {6.0f, 4.0f, 6.0f}});
    Expect(confinement.type == "camera_confinement_volume",
           "World volume descriptors should create camera confinement volumes");

    const ri::world::LodOverrideVolume lodOverride = ri::world::CreateLodOverrideVolume(
        MakeSeed("lod_box"),
        {"mesh_a", "mesh_b"},
        ri::world::ForcedLod::Far,
        {.runtimeId = "lod_override", .type = "lod_override_volume", .size = {8.0f, 6.0f, 8.0f}});
    Expect(lodOverride.type == "lod_override_volume" &&
               lodOverride.forcedLod == ri::world::ForcedLod::Far &&
               lodOverride.targetIds.size() == 2U,
           "World volume descriptors should create lod override volumes");

    std::vector<ri::world::LodSwitchLevel> lodLevels = {
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
    const ri::world::LodSwitchPrimitive lodSwitch = ri::world::CreateLodSwitchPrimitive(
        MakeSeed("lod_switch_a"),
        std::move(lodLevels),
        {.metric = ri::world::LodSwitchMetric::ScreenSize,
         .hysteresisEnabled = true,
         .transitionMode = ri::world::LodSwitchTransitionMode::Crossfade,
         .crossfadeSeconds = 99.0f},
        {.showActiveLevel = true, .showRanges = true},
        {.runtimeId = "lod_switch", .type = "lod_switch_primitive", .size = {1.0f, 1.0f, 1.0f}});
    Expect(lodSwitch.type == "lod_switch_primitive"
               && lodSwitch.levels.size() == 2U
               && lodSwitch.levels.front().name == "near"
               && lodSwitch.levels.back().representation.kind == ri::world::LodSwitchRepresentationKind::Cluster
               && lodSwitch.policy.metric == ri::world::LodSwitchMetric::ScreenSize
               && lodSwitch.policy.transitionMode == ri::world::LodSwitchTransitionMode::Crossfade
               && NearlyEqual(lodSwitch.policy.crossfadeSeconds, 8.0f)
               && lodSwitch.debug.showActiveLevel
               && lodSwitch.crossfadeAlpha == 1.0f,
           "World volume descriptors should create deterministic lod-switch primitives with clamped policy");

    ri::world::SurfaceScatterSourceRepresentation scatterSource{};
    scatterSource.kind = ri::world::SurfaceScatterRepresentationKind::Mesh;
    scatterSource.payloadId = "mesh_pebble";
    ri::world::SurfaceScatterDensityControls scatterDensity{};
    scatterDensity.count = 80000U;
    scatterDensity.densityPerSquareMeter = 999.0f;
    scatterDensity.maxPoints = 32U;
    ri::world::SurfaceScatterDistributionControls scatterDistribution{};
    scatterDistribution.seed = 123U;
    scatterDistribution.minSlopeDegrees = 80.0f;
    scatterDistribution.maxSlopeDegrees = 20.0f;
    scatterDistribution.minHeight = 150.0f;
    scatterDistribution.maxHeight = -50.0f;
    scatterDistribution.minNormalY = 3.0f;
    scatterDistribution.minSeparation = -4.0f;
    scatterDistribution.scaleJitter = {-2.0f, 0.4f, 12.0f};
    ri::world::SurfaceScatterCullingPolicy scatterCulling{};
    scatterCulling.maxActiveDistance = -10.0f;
    scatterCulling.frustumCulling = false;
    ri::world::SurfaceScatterAnimationSettings scatterAnimation{};
    scatterAnimation.windSwayEnabled = true;
    scatterAnimation.swayAmplitude = 50.0f;
    scatterAnimation.swayFrequency = -5.0f;
    const ri::world::SurfaceScatterVolume surfaceScatter = ri::world::CreateSurfaceScatterVolume(
        MakeSeed("scatter_a"),
        {"target_floor", "", "target_wall"},
        scatterSource,
        scatterDensity,
        scatterDistribution,
        ri::world::SurfaceScatterCollisionPolicy::Proxy,
        scatterCulling,
        scatterAnimation,
        {.runtimeId = "surface_scatter", .type = "surface_scatter_volume", .size = {4.0f, 2.0f, 4.0f}});
    Expect(surfaceScatter.type == "surface_scatter_volume"
               && surfaceScatter.targetIds.size() == 2U
               && surfaceScatter.targetIds.front() == "target_floor"
               && surfaceScatter.sourceRepresentation.kind == ri::world::SurfaceScatterRepresentationKind::Mesh
               && surfaceScatter.sourceRepresentation.payloadId == "mesh_pebble"
               && surfaceScatter.density.count == 20000U
               && NearlyEqual(surfaceScatter.density.densityPerSquareMeter, 500.0f)
               && surfaceScatter.density.maxPoints == 20000U
               && NearlyEqual(surfaceScatter.distribution.minSlopeDegrees, 20.0f)
               && NearlyEqual(surfaceScatter.distribution.maxSlopeDegrees, 80.0f)
               && NearlyEqual(surfaceScatter.distribution.minHeight, -50.0f)
               && NearlyEqual(surfaceScatter.distribution.maxHeight, 150.0f)
               && NearlyEqual(surfaceScatter.distribution.minNormalY, 1.0f)
               && NearlyEqual(surfaceScatter.distribution.minSeparation, 0.0f)
               && NearlyEqual(surfaceScatter.distribution.scaleJitter.x, 0.0f)
               && NearlyEqual(surfaceScatter.distribution.scaleJitter.z, 8.0f)
               && surfaceScatter.collisionPolicy == ri::world::SurfaceScatterCollisionPolicy::Proxy
               && NearlyEqual(surfaceScatter.culling.maxActiveDistance, 1.0f)
               && !surfaceScatter.culling.frustumCulling
               && surfaceScatter.animation.windSwayEnabled
               && NearlyEqual(surfaceScatter.animation.swayAmplitude, 10.0f)
               && NearlyEqual(surfaceScatter.animation.swayFrequency, 0.0f),
           "World volume descriptors should sanitize deterministic surface-scatter controls and enforce explicit budgets");

    const ri::world::SplineMeshDeformerPrimitive splineDeformer = ri::world::CreateSplineMeshDeformerPrimitive(
        MakeSeed("spline_deformer_a"),
        {"target_a", "", "target_b"},
        {{0.0f, 0.0f, 0.0f}, {4.0f, 0.2f, 2.0f}, {8.0f, 0.0f, 0.0f}},
        5000U,
        999U,
        -2.0f,
        2.0f,
        true,
        true,
        true,
        true,
        42U,
        64U,
        -3.0f,
        false,
        {.runtimeId = "spline_mesh_deformer", .type = "spline_mesh_deformer", .size = {8.0f, 4.0f, 8.0f}});
    Expect(splineDeformer.type == "spline_mesh_deformer"
               && splineDeformer.targetIds.size() == 2U
               && splineDeformer.sampleCount == 2048U
               && splineDeformer.sectionCount == 256U
               && NearlyEqual(splineDeformer.segmentLength, 0.05f)
               && NearlyEqual(splineDeformer.tangentSmoothing, 1.0f)
               && splineDeformer.collisionEnabled
               && splineDeformer.navInfluence
               && splineDeformer.dynamicEnabled
               && splineDeformer.maxSamples == 2048U
               && NearlyEqual(splineDeformer.maxActiveDistance, 1.0f)
               && !splineDeformer.frustumCulling,
           "World volume descriptors should clamp spline-deformer settings and keep deterministic topology controls");

    const ri::world::SplineDecalRibbonPrimitive splineRibbon = ri::world::CreateSplineDecalRibbonPrimitive(
        MakeSeed("spline_ribbon_a"),
        {{-2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}},
        -1.0f,
        9999U,
        20.0f,
        -5.0f,
        9000.0f,
        2.0f,
        true,
        true,
        true,
        true,
        true,
        99U,
        32U,
        -10.0f,
        false,
        {.runtimeId = "spline_decal_ribbon", .type = "spline_decal_ribbon", .size = {8.0f, 2.0f, 8.0f}});
    Expect(splineRibbon.type == "spline_decal_ribbon"
               && NearlyEqual(splineRibbon.width, 0.01f)
               && splineRibbon.tessellation == 4096U
               && NearlyEqual(splineRibbon.offsetY, 10.0f)
               && NearlyEqual(splineRibbon.uvScaleU, 0.01f)
               && NearlyEqual(splineRibbon.uvScaleV, 1000.0f)
               && NearlyEqual(splineRibbon.tangentSmoothing, 1.0f)
               && splineRibbon.transparentBlend
               && splineRibbon.depthWrite
               && splineRibbon.collisionEnabled
               && splineRibbon.navInfluence
               && splineRibbon.dynamicEnabled
               && splineRibbon.maxSamples == 4096U
               && NearlyEqual(splineRibbon.maxActiveDistance, 1.0f)
               && !splineRibbon.frustumCulling,
           "World volume descriptors should clamp spline-decal ribbon inputs and preserve explicit integration flags");

    const ri::world::TopologicalUvRemapperVolume uvRemapper = ri::world::CreateTopologicalUvRemapperVolume(
        MakeSeed("uv_remapper_a"),
        {"mesh_a", "", "mesh_b"},
        "UNKNOWN_MODE",
        "tx",
        "ty",
        "tz",
        {},
        -5.0f,
        200.0f,
        {-9.0f, 0.5f, 12.0f},
        9000U,
        -40.0f,
        false,
        {.previewTint = true, .axisContributionPreview = true},
        {.runtimeId = "topological_uv_remapper", .type = "topological_uv_remapper", .size = {12.0f, 4.0f, 12.0f}});
    Expect(uvRemapper.type == "topological_uv_remapper"
               && uvRemapper.targetIds.size() == 2U
               && uvRemapper.remapMode == "triplanar"
               && uvRemapper.textureX == "tx"
               && uvRemapper.textureY == "ty"
               && uvRemapper.textureZ == "tz"
               && uvRemapper.sharedTextureId.empty()
               && NearlyEqual(uvRemapper.projectionScale, 1.0e-6f)
               && NearlyEqual(uvRemapper.blendSharpness, 64.0f)
               && NearlyEqual(uvRemapper.axisWeights.x, 0.001f)
               && NearlyEqual(uvRemapper.axisWeights.y, 0.5f)
               && NearlyEqual(uvRemapper.axisWeights.z, 8.0f)
               && uvRemapper.maxMaterialPatches == 4096U
               && NearlyEqual(uvRemapper.maxActiveDistance, 1.0f)
               && !uvRemapper.frustumCulling
               && uvRemapper.debug.previewTint
               && uvRemapper.debug.axisContributionPreview,
           "World volume descriptors should clamp procedural UV remapper inputs and normalize remap modes");

    const ri::world::TriPlanarNode triPlanar = ri::world::CreateTriPlanarNode(
        MakeSeed("tri_planar_a"),
        {"rock_cluster"},
        {},
        {},
        {},
        "shared_atlas",
        0.0f,
        0.0f,
        {1.0f, 1.0f, 1.0f},
        0U,
        true,
        1000000.0f,
        true,
        {},
        {.runtimeId = "tri_planar_node", .type = "tri_planar_node", .size = {10.0f, 4.0f, 10.0f}});
    Expect(triPlanar.type == "tri_planar_node"
               && triPlanar.targetIds.size() == 1U
               && triPlanar.sharedTextureId == "shared_atlas"
               && NearlyEqual(triPlanar.projectionScale, 1.0e-6f)
               && NearlyEqual(triPlanar.blendSharpness, 0.25f)
               && triPlanar.maxMaterialPatches == 1U
               && triPlanar.objectSpaceAxes
               && NearlyEqual(triPlanar.maxActiveDistance, 100000.0f)
               && triPlanar.frustumCulling,
           "World volume descriptors should clamp tri-planar node budgets and honor object-space sampling flags");

    ri::world::InstanceCloudSourceRepresentation cloudSource{};
    cloudSource.kind = ri::world::InstanceCloudRepresentationKind::Mesh;
    cloudSource.payloadId = "mesh_crate";
    ri::world::InstanceCloudVariationRanges cloudVariation{};
    cloudVariation.rotationJitterRadians = {1.0f, 2.0f, 3.0f};
    cloudVariation.scaleJitter = {-2.0f, 9.0f, 0.5f};
    cloudVariation.positionJitter = {0.3f, 0.5f, 0.7f};
    ri::world::InstanceCloudCullingPolicy cloudCulling{};
    cloudCulling.maxActiveDistance = -6.0f;
    cloudCulling.frustumCulling = false;
    const ri::world::InstanceCloudPrimitive instanceCloud = ri::world::CreateInstanceCloudPrimitive(
        MakeSeed("instance_cloud_a"),
        cloudSource,
        90000U,
        {0.5f, 0.0f, 0.25f},
        {12.0f, 6.0f, 12.0f},
        4242U,
        cloudVariation,
        ri::world::InstanceCloudCollisionPolicy::PerInstance,
        cloudCulling,
        {.runtimeId = "instance_cloud", .type = "instance_cloud_primitive", .size = {2.0f, 2.0f, 2.0f}});
    Expect(instanceCloud.type == "instance_cloud_primitive"
               && instanceCloud.sourceRepresentation.kind == ri::world::InstanceCloudRepresentationKind::Mesh
               && instanceCloud.sourceRepresentation.payloadId == "mesh_crate"
               && instanceCloud.count == 20000U
               && NearlyEqual(instanceCloud.offsetStep.x, 0.5f)
               && NearlyEqual(instanceCloud.distributionExtents.y, 6.0f)
               && instanceCloud.seed == 4242U
               && NearlyEqual(instanceCloud.variation.scaleJitter.x, 0.0f)
               && NearlyEqual(instanceCloud.variation.scaleJitter.y, 8.0f)
               && instanceCloud.collisionPolicy == ri::world::InstanceCloudCollisionPolicy::PerInstance
               && NearlyEqual(instanceCloud.culling.maxActiveDistance, 1.0f)
               && !instanceCloud.culling.frustumCulling,
           "World volume descriptors should sanitize instance-cloud primitives and preserve deterministic options");

    const ri::world::NavmeshModifierVolume navmeshModifier = ri::world::CreateNavmeshModifierVolume(
        MakeSeed("nav_a"),
        1000.0f,
        "slow",
        {.runtimeId = "navmesh_modifier", .type = "navmesh_modifier_volume", .size = {6.0f, 4.0f, 6.0f}});
    Expect(navmeshModifier.type == "navmesh_modifier_volume" &&
               NearlyEqual(navmeshModifier.traversalCost, 100.0f) &&
               navmeshModifier.tag == "slow",
           "World volume descriptors should create and clamp navmesh modifier volumes");

    const ri::world::AmbientAudioVolume ambientAudio = ri::world::CreateAmbientAudioVolume(
        MakeSeed("ambient_a", "ambient_audio_spline", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{8.0f, 4.0f, 8.0f}),
        "Assets/Audio/wind.ogg",
        0.75f,
        400.0f,
        "wind_tunnel",
        {ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{10.0f, 0.0f, 0.0f}},
        {.runtimeId = "ambient_audio", .type = "ambient_audio_spline", .size = {8.0f, 4.0f, 8.0f}});
    Expect(ambientAudio.type == "ambient_audio_spline" &&
               ambientAudio.audioPath == "Assets/Audio/wind.ogg" &&
               ambientAudio.label == "wind_tunnel",
           "World volume descriptors should create ambient audio volumes");
    Expect(NearlyEqual(ambientAudio.baseVolume, 0.75f) &&
               NearlyEqual(ambientAudio.maxDistance, 256.0f) &&
               ambientAudio.splinePoints.size() == 2U,
           "World volume descriptors should clamp ambient audio distance and preserve spline points");

    const ri::world::GenericTriggerVolume triggerVolume = ri::world::CreateGenericTriggerVolume(
        MakeSeed("trigger_a", "generic_trigger_volume", std::nullopt, ri::math::Vec3{1.0f, 0.0f, 0.0f}, ri::math::Vec3{2.0f, 2.0f, 2.0f}),
        2.0,
        {.runtimeId = "trigger_volume", .type = "generic_trigger_volume", .shape = ri::world::VolumeShape::Sphere, .size = {2.0f, 2.0f, 2.0f}});
    Expect(triggerVolume.type == "generic_trigger_volume" &&
               NearlyEqual(static_cast<float>(triggerVolume.broadcastFrequency), 2.0f),
           "World volume descriptors should create generic trigger volumes");

    const ri::world::SpatialQueryVolume spatialQuery = ri::world::CreateSpatialQueryVolume(
        MakeSeed("query_a", "spatial_query_volume", ri::world::VolumeShape::Cylinder, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{2.0f, 4.0f, 2.0f}),
        3.0,
        7U,
        {.runtimeId = "query_volume", .type = "spatial_query_volume", .shape = ri::world::VolumeShape::Sphere, .size = {2.0f, 2.0f, 2.0f}});
    Expect(spatialQuery.type == "spatial_query_volume" &&
               NearlyEqual(static_cast<float>(spatialQuery.broadcastFrequency), 3.0f) &&
               spatialQuery.filterMask == 7U,
           "World volume descriptors should create spatial query volumes");

    const ri::world::VisibilityPrimitive antiPortal = ri::world::CreateVisibilityPrimitive(
        MakeSeed("portal_a", "", std::nullopt, ri::math::Vec3{1.0f, 2.0f, 3.0f}, ri::math::Vec3{8.0f, 10.0f, 0.5f}),
        ri::world::VisibilityPrimitiveKind::AntiPortal,
        {.runtimeId = "anti_portal", .type = "anti_portal", .size = {1.0f, 1.0f, 1.0f}});
    Expect(antiPortal.type == "anti_portal" &&
               antiPortal.kind == ri::world::VisibilityPrimitiveKind::AntiPortal,
           "World volume descriptors should create typed visibility primitives");
    ExpectVec3(antiPortal.position, {1.0f, 2.0f, 3.0f},
               "World volume descriptors should preserve visibility primitive position");
    ExpectVec3(antiPortal.size, {8.0f, 10.0f, 0.5f},
               "World volume descriptors should preserve visibility primitive extents");

    const ri::world::OcclusionPortalVolume occlusionPortal = ri::world::CreateOcclusionPortalVolume(
        MakeSeed("occ_portal"),
        false,
        {.runtimeId = "occlusion_portal", .type = "occlusion_portal", .size = {4.0f, 4.0f, 0.18f}});
    Expect(occlusionPortal.type == "occlusion_portal" && !occlusionPortal.closed,
           "World volume descriptors should create occlusion portals with explicit closed state");

    const ri::world::ReflectionProbeVolume reflectionProbe = ri::world::CreateReflectionProbeVolume(
        MakeSeed("probe_a"),
        10.0f,
        100.0f,
        16U,
        false,
        true,
        {.runtimeId = "reflection_probe", .type = "reflection_probe_volume", .size = {6.0f, 6.0f, 6.0f}});
    Expect(reflectionProbe.type == "reflection_probe_volume"
               && NearlyEqual(reflectionProbe.intensity, 8.0f)
               && NearlyEqual(reflectionProbe.blendDistance, 64.0f)
               && reflectionProbe.captureResolution == 64U
               && !reflectionProbe.boxProjection
               && reflectionProbe.dynamicCapture,
           "World volume descriptors should create reflection probe volumes");

    const ri::world::LightImportanceVolume probeGridBounds = ri::world::CreateLightImportanceVolume(
        MakeSeed("light_bounds"),
        true,
        {.runtimeId = "light_importance", .type = "light_importance_volume", .size = {8.0f, 6.0f, 8.0f}});
    Expect(probeGridBounds.probeGridBounds && probeGridBounds.type == "probe_grid_bounds",
           "World volume descriptors should preserve probe-grid-bounds typing");

    const ri::world::LightPortalVolume lightPortal = ri::world::CreateLightPortalVolume(
        MakeSeed("light_portal_a"),
        5.0f,
        -2.0f,
        -200.0f,
        true,
        {.runtimeId = "light_portal", .type = "light_portal", .size = {5.0f, 5.0f, 1.0f}});
    Expect(lightPortal.type == "light_portal"
               && NearlyEqual(lightPortal.transmission, 4.0f)
               && NearlyEqual(lightPortal.softness, 0.0f)
               && NearlyEqual(lightPortal.priority, -100.0f)
               && lightPortal.twoSided,
           "World volume descriptors should create light portal volumes");

    const ri::world::ReferenceImagePlane referenceImagePlane = ri::world::CreateReferenceImagePlane(
        MakeSeed("ref_plane_a", "", std::nullopt, ri::math::Vec3{1.0f, 2.0f, 3.0f}, ri::math::Vec3{8.0f, 5.0f, 1.0f}),
        "caution_stripes_refined.png",
        "https://example.invalid/ref.png",
        {1.3f, 0.6f, -0.5f},
        2.0f,
        500,
        true,
        {.runtimeId = "reference_image_plane", .type = "reference_image_plane", .size = {8.0f, 5.0f, 1.0f}});
    Expect(referenceImagePlane.type == "reference_image_plane"
               && referenceImagePlane.textureId == "caution_stripes_refined.png"
               && referenceImagePlane.imageUrl == "https://example.invalid/ref.png"
               && NearlyEqual(referenceImagePlane.opacity, 1.0f)
               && referenceImagePlane.renderOrder == 200
               && referenceImagePlane.alwaysFaceCamera,
           "World volume descriptors should sanitize and preserve reference image plane render data");
    ExpectVec3(referenceImagePlane.tintColor, {1.0f, 0.6f, 0.0f},
               "World volume descriptors should clamp reference image plane tint colors");

    const ri::world::AnnotationCommentPrimitive annotation = ri::world::CreateAnnotationCommentPrimitive(
        MakeSeed("comment_a", "", std::nullopt, ri::math::Vec3{4.0f, 2.0f, -1.0f}, ri::math::Vec3{2.0f, 2.0f, 2.0f}),
        {},
        {},
        {},
        99.0f,
        1.0f,
        true,
        {.runtimeId = "annotation_comment", .type = "annotation_comment_primitive", .size = {2.0f, 2.0f, 2.0f}});
    Expect(annotation.type == "annotation_comment_primitive"
               && annotation.text == "NOTE"
               && annotation.accentColor == "#ffd36a"
               && annotation.backgroundColor == "rgba(26, 22, 16, 0.88)"
               && NearlyEqual(annotation.textScale, 20.0f)
               && NearlyEqual(annotation.fontSize, 8.0f)
               && annotation.alwaysFaceCamera,
           "World volume descriptors should apply robust defaults and clamping for annotation comments");

    const ri::world::Text3dPrimitive text3d = ri::world::CreateText3dPrimitive(
        MakeSeed("text3d_a", "", std::nullopt, ri::math::Vec3{1.0f, 2.0f, 3.0f}, ri::math::Vec3{2.0f, 2.0f, 0.4f}),
        {},
        {},
        "mat_holo",
        {},
        {},
        99.0f,
        0.0001f,
        -4.0f,
        99.0f,
        true,
        false,
        {.runtimeId = "text_3d", .type = "text_3d_primitive", .size = {2.0f, 2.0f, 0.4f}});
    Expect(text3d.type == "text_3d_primitive"
               && text3d.text == "TEXT"
               && text3d.fontFamily == "default"
               && text3d.materialId == "mat_holo"
               && text3d.textColor == "#ffffff"
               && text3d.outlineColor == "#000000"
               && NearlyEqual(text3d.textScale, 48.0f)
               && NearlyEqual(text3d.depth, 0.001f)
               && NearlyEqual(text3d.extrusionBevel, 0.0f)
               && NearlyEqual(text3d.letterSpacing, 8.0f)
               && text3d.alwaysFaceCamera
               && !text3d.doubleSided,
           "World volume descriptors should sanitize 3D text primitives and preserve material identifiers");

    const ri::world::MeasureToolPrimitive measureLine = ri::world::CreateMeasureToolPrimitive(
        MakeSeed("measure_line_a", "", std::nullopt, ri::math::Vec3{}, ri::math::Vec3{2.0f, 2.0f, 2.0f}),
        ri::world::MeasureToolMode::Line,
        {0.0f, 0.0f, 0.0f},
        {3.0f, 4.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {},
        {},
        {},
        {},
        99.0f,
        1.0f,
        false,
        false,
        true,
        {.runtimeId = "measure_tool", .type = "measure_tool_primitive", .size = {2.0f, 2.0f, 2.0f}});
    Expect(measureLine.type == "measure_tool_primitive"
               && measureLine.mode == ri::world::MeasureToolMode::Line
               && NearlyEqual(measureLine.position.x, 1.5f)
               && NearlyEqual(measureLine.position.y, 2.0f)
               && NearlyEqual(measureLine.position.z, 0.0f)
               && NearlyEqual(measureLine.textScale, 24.0f)
               && NearlyEqual(measureLine.fontSize, 14.0f)
               && measureLine.showWireframe
               && !measureLine.showFill
               && measureLine.unitSuffix == "u",
           "World volume descriptors should create robust measure-tool line primitives with sane defaults");

    const ri::world::RenderTargetSurface renderTarget = ri::world::CreateRenderTargetSurface(
        MakeSeed("rt_surface_a", "", std::nullopt, ri::math::Vec3{0.0f, 3.0f, -4.0f}, ri::math::Vec3{8.0f, 5.0f, 1.0f}),
        {0.0f, 3.0f, 8.0f},
        {0.0f, 3.0f, 0.0f},
        170.0f,
        4096,
        32,
        -4.0f,
        0U,
        true,
        true,
        {.runtimeId = "render_target_surface", .type = "render_target_surface", .size = {8.0f, 5.0f, 1.0f}});
    Expect(renderTarget.type == "render_target_surface"
               && NearlyEqual(renderTarget.cameraFovDegrees, 120.0f)
               && renderTarget.renderResolution == 1024
               && renderTarget.resolutionCap == 64
               && NearlyEqual(renderTarget.maxActiveDistance, 1.0f)
               && renderTarget.updateEveryFrames == 1U
               && renderTarget.enableDistanceGate
               && renderTarget.editorOnly,
           "World volume descriptors should clamp render-target surface quality and update controls");

    const ri::world::PlanarReflectionSurface planarReflection = ri::world::CreatePlanarReflectionSurface(
        MakeSeed("mirror_surface_a", "", std::nullopt, ri::math::Vec3{0.0f, 3.0f, -4.0f}, ri::math::Vec3{8.0f, 6.0f, 1.0f}),
        {0.0f, 0.0f, 0.0f},
        3.0f,
        24,
        4096,
        9999.0f,
        500U,
        false,
        true,
        {.runtimeId = "planar_reflection_surface", .type = "planar_reflection_surface", .size = {8.0f, 6.0f, 1.0f}});
    Expect(planarReflection.type == "planar_reflection_surface"
               && NearlyEqual(planarReflection.planeNormal.x, 0.0f)
               && NearlyEqual(planarReflection.planeNormal.y, 0.0f)
               && NearlyEqual(planarReflection.planeNormal.z, 1.0f)
               && NearlyEqual(planarReflection.reflectionStrength, 1.0f)
               && planarReflection.renderResolution == 64
               && planarReflection.resolutionCap == 2048
               && NearlyEqual(planarReflection.maxActiveDistance, 1000.0f)
               && planarReflection.updateEveryFrames == 120U
               && !planarReflection.enableDistanceGate
               && planarReflection.editorOnly,
           "World volume descriptors should sanitize planar-reflection surface controls and normal vectors");

    ri::world::PassThroughMaterialSettings passThroughMaterial{};
    passThroughMaterial.opacity = 2.0f;
    passThroughMaterial.depthWrite = true;
    passThroughMaterial.blendMode = ri::world::PassThroughBlendMode::Additive;
    ri::world::PassThroughVisualBehavior passThroughVisual{};
    passThroughVisual.pulseEnabled = true;
    passThroughVisual.pulseSpeed = -8.0f;
    passThroughVisual.pulseMinOpacity = 0.95f;
    passThroughVisual.pulseMaxOpacity = 0.20f;
    passThroughVisual.fadeNear = 12.0f;
    passThroughVisual.fadeFar = 3.0f;
    passThroughVisual.rimPower = 90.0f;
    ri::world::PassThroughInteractionProfile passThroughInteraction{};
    passThroughInteraction.blocksNpc = true;
    passThroughInteraction.affectsNavigation = true;
    passThroughInteraction.raycastSelectable = false;
    const ri::world::PassThroughPrimitive passThrough = ri::world::CreatePassThroughPrimitive(
        MakeSeed("pass_through_a", "", std::nullopt, ri::math::Vec3{2.0f, 1.0f, -2.0f}, ri::math::Vec3{2.0f, 3.0f, 0.3f}),
        ri::world::PassThroughPrimitiveShape::CustomMesh,
        {},
        passThroughMaterial,
        passThroughVisual,
        passThroughInteraction,
        {.onEnter = "enter_hook"},
        {.label = "ghost wall"},
        {.runtimeId = "pass_through", .type = "pass_through_primitive", .size = {2.0f, 3.0f, 0.3f}});
    Expect(passThrough.type == "pass_through_primitive"
               && passThrough.primitiveShape == ri::world::PassThroughPrimitiveShape::Box
               && NearlyEqual(passThrough.material.opacity, 1.0f)
               && passThrough.material.depthWrite
               && passThrough.material.blendMode == ri::world::PassThroughBlendMode::Additive
               && NearlyEqual(passThrough.visualBehavior.pulseSpeed, 0.0f)
               && NearlyEqual(passThrough.visualBehavior.pulseMinOpacity, 0.20f)
               && NearlyEqual(passThrough.visualBehavior.pulseMaxOpacity, 0.95f)
               && NearlyEqual(passThrough.visualBehavior.fadeNear, 12.0f)
               && NearlyEqual(passThrough.visualBehavior.fadeFar, 12.0f)
               && NearlyEqual(passThrough.visualBehavior.rimPower, 16.0f)
               && !passThrough.passThrough
               && passThrough.interactionProfile.blocksNpc
               && passThrough.interactionProfile.affectsNavigation
               && !passThrough.interactionProfile.raycastSelectable
               && passThrough.events.onEnter == "enter_hook",
           "World volume descriptors should sanitize pass-through primitive visual and interaction contracts");

    ri::world::SkyProjectionVisualSettings skyVisual{};
    skyVisual.mode = ri::world::SkyProjectionVisualMode::Texture;
    skyVisual.textureId = {};
    skyVisual.opacity = 3.0f;
    skyVisual.color = {};
    skyVisual.topColor = {};
    skyVisual.bottomColor = {};
    ri::world::SkyProjectionBehaviorSettings skyBehavior{};
    skyBehavior.followCameraYaw = true;
    skyBehavior.parallaxFactor = 2.0f;
    skyBehavior.depthWrite = true;
    skyBehavior.renderLayer = ri::world::SkyProjectionRenderLayer::Foreground;
    const ri::world::SkyProjectionSurface skyProjection = ri::world::CreateSkyProjectionSurface(
        MakeSeed("sky_projection_a", "", std::nullopt, ri::math::Vec3{0.0f, 6.0f, -12.0f}, ri::math::Vec3{16.0f, 8.0f, 1.0f}),
        "plane",
        skyVisual,
        skyBehavior,
        {.label = "horizon"},
        {.runtimeId = "sky_projection_surface", .type = "sky_projection_surface", .size = {16.0f, 8.0f, 1.0f}});
    Expect(skyProjection.type == "sky_projection_surface"
               && skyProjection.primitiveType == "plane"
               && skyProjection.visual.mode == ri::world::SkyProjectionVisualMode::Solid
               && skyProjection.visual.color == "#8ab4ff"
               && skyProjection.visual.topColor == "#b8d4ff"
               && skyProjection.visual.bottomColor == "#6f94cc"
               && NearlyEqual(skyProjection.visual.opacity, 1.0f)
               && skyProjection.visual.doubleSided
               && skyProjection.visual.unlit
               && skyProjection.behavior.followCameraYaw
               && NearlyEqual(skyProjection.behavior.parallaxFactor, 1.0f)
               && skyProjection.behavior.depthWrite
               && skyProjection.behavior.renderLayer == ri::world::SkyProjectionRenderLayer::Foreground
               && skyProjection.skyProjectionSurface,
           "World volume descriptors should sanitize sky-projection surfaces for stable background rendering");

    ri::world::VolumetricEmitterEmissionSettings emission{};
    emission.particleCount = 5000U;
    emission.spawnMode = ri::world::VolumetricEmitterSpawnMode::NoiseClustered;
    emission.lifetimeMinSeconds = 9.0f;
    emission.lifetimeMaxSeconds = 2.0f;
    emission.spawnRatePerSecond = -10.0f;
    ri::world::VolumetricEmitterParticleSettings particle{};
    particle.size = -1.0f;
    particle.sizeJitter = 9.0f;
    particle.opacity = 5.0f;
    particle.color = {};
    ri::world::VolumetricEmitterCullingSettings culling{};
    culling.maxActiveDistance = -2.0f;
    const ri::world::VolumetricEmitterBounds emitter = ri::world::CreateVolumetricEmitterBounds(
        MakeSeed("emitter_a", "", std::nullopt, ri::math::Vec3{0.0f, 3.0f, 0.0f}, ri::math::Vec3{8.0f, 5.0f, 8.0f}),
        emission,
        particle,
        {.blendMode = ri::world::VolumetricEmitterBlendMode::Additive},
        culling,
        {.showBounds = true, .label = "mist pocket"},
        {.runtimeId = "volumetric_emitter", .type = "volumetric_emitter_bounds", .size = {8.0f, 5.0f, 8.0f}});
    Expect(emitter.type == "volumetric_emitter_bounds"
               && emitter.emission.particleCount == 2048U
               && emitter.emission.spawnMode == ri::world::VolumetricEmitterSpawnMode::NoiseClustered
               && NearlyEqual(emitter.emission.lifetimeMinSeconds, 2.0f)
               && NearlyEqual(emitter.emission.lifetimeMaxSeconds, 9.0f)
               && NearlyEqual(emitter.emission.spawnRatePerSecond, 0.0f)
               && NearlyEqual(emitter.particle.size, 0.001f)
               && NearlyEqual(emitter.particle.sizeJitter, 5.0f)
               && emitter.particle.color == "#d9dce4"
               && NearlyEqual(emitter.particle.opacity, 1.0f)
               && emitter.render.blendMode == ri::world::VolumetricEmitterBlendMode::Additive
               && NearlyEqual(emitter.culling.maxActiveDistance, 1.0f)
               && emitter.debug.showBounds
               && emitter.debug.label == "mist pocket",
           "World volume descriptors should sanitize volumetric emitter bounds and keep defaults stable");

    const std::vector<ri::world::CameraModifierVolume> cameraVolumes = {
        ri::world::CreateCameraModifierVolume(
            MakeSeed("wide", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{6.0f, 6.0f, 6.0f}),
            100.0f,
            -5.0f,
            2.0f,
            0.2f,
            1.5f,
            {0.1f, 0.0f, -0.1f},
            {.runtimeId = "camera_modifier", .type = "camera_modifier_volume", .size = {5.0f, 5.0f, 5.0f}}),
        ri::world::CreateCameraModifierVolume(
            MakeSeed("tight", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{3.0f, 3.0f, 3.0f}),
            10.0f,
            25.0f,
            3.0f,
            1.0f,
            4.0f,
            {-0.2f, 0.1f, 0.3f},
            {.runtimeId = "camera_modifier", .type = "camera_modifier_volume", .size = {5.0f, 5.0f, 5.0f}}),
    };
    const ri::world::CameraModifierVolume* activeCameraModifier =
        ri::world::GetActiveCameraModifierAt({0.0f, 0.0f, 0.0f}, cameraVolumes);
    Expect(activeCameraModifier != nullptr && activeCameraModifier->id == "tight",
           "World volume descriptors should pick the highest-priority active camera modifier");
    Expect(ri::world::GetActiveCameraModifierAt({10.0f, 0.0f, 0.0f}, cameraVolumes) == nullptr,
           "World volume descriptors should return null when no camera modifier is active");
    const ri::world::CameraModifierBlendState blendedModifier =
        ri::world::BlendCameraModifierAt({0.0f, 0.0f, 0.0f}, cameraVolumes);
    Expect(blendedModifier.active && blendedModifier.activeVolumeIds.size() == 2U,
           "World volume descriptors should report active blended camera modifiers at overlap");
    Expect(blendedModifier.fov > 10.0f && blendedModifier.fov < 100.0f,
           "World volume descriptors should blend camera-modifier FOV using stable weighted overlap");
    Expect(blendedModifier.shakeAmplitude > 0.2f && blendedModifier.shakeAmplitude < 1.0f,
           "World volume descriptors should blend camera-modifier shake amplitude");
    Expect(blendedModifier.shakeFrequency > 1.5f && blendedModifier.shakeFrequency < 4.0f,
           "World volume descriptors should blend camera-modifier shake frequency");

    const std::vector<ri::world::SafeZoneVolume> safeZones = {
        ri::world::CreateSafeZoneVolume(
            MakeSeed("hub", "", std::nullopt, ri::math::Vec3{0.0f, 0.0f, 0.0f}, ri::math::Vec3{6.0f, 4.0f, 6.0f}),
            false,
            {.runtimeId = "safe_zone", .type = "safe_zone_volume", .size = {6.0f, 4.0f, 6.0f}}),
    };
    Expect(!safeZones.front().dropAggro,
           "World volume descriptors should preserve safe-zone tuning flags");
    Expect(ri::world::IsPositionInSafeZone({1.0f, 0.0f, 1.0f}, safeZones),
           "World volume descriptors should report points inside safe zones");
    Expect(!ri::world::IsPositionInSafeZone({10.0f, 0.0f, 10.0f}, safeZones),
           "World volume descriptors should reject points outside safe zones");
}

void TestRawIronGameplayInfrastructureStacks() {
    using namespace detail_WorldVolumeDescriptors;

    ri::trace::BvhLifetimeRegistry bvh{};
    bvh.MarkParticipating("collider_a", true);
    bvh.MarkParticipating("collider_b", false);
    Expect(bvh.ActiveParticipationCount() == 1U, "BVH registry should count only BVH-accelerated ids");
    Expect(bvh.ReleaseParticipating("collider_a"), "BVH release should succeed for registered id");
    Expect(bvh.ActiveParticipationCount() == 0U, "BVH registry should decrement after release");

    const std::string assetsJson = R"({
      "animationLibraryProfiles":[
        {
          "profile":"hazmat_survivor",
          "libraries":[
            {"path":"./packs/hazmat.glb","priority":5},
            {"path":"./packs/common.glb","priority":1}
          ]
        }
      ]
    })";
    const auto defs = ri::scene::GetProfileAnimationLibraryDefinitions(assetsJson);
    Expect(defs.contains("hazmat_survivor") && defs.at("hazmat_survivor").size() == 2U,
           "Profile animation library definitions should parse profile->library entries");

    ri::scene::AnimationLibraryDefinition missingLib{};
    missingLib.sourcePath = "definitely_missing_library.glb";
    missingLib.profileName = "hazmat_survivor";
    const auto loadedMissing = ri::scene::LoadAnimationLibrarySource(missingLib).get();
    Expect(!loadedMissing.success && !loadedMissing.error.empty(),
           "Streaming library ingest should surface an error for missing source packs");

    ri::scene::Scene scene{};
    const int hipsNode = scene.CreateNode("mixamorig:Hips");
    const int spineNode = scene.CreateNode("mixamorig:Spine");
    Expect(hipsNode >= 0 && spineNode >= 0, "Scene test setup should create source skeleton nodes");
    ri::scene::LoadedAnimationLibrarySource source{};
    source.success = true;
    source.sourcePath = "./packs/common.glb";
    source.profileName = "hazmat_survivor";
    source.priority = 3;
    source.clipAlias["Armature|Walk_Loop"] = "walk";
    source.clips.push_back(ri::scene::AnimationClip{.name = "Armature|Walk_Loop", .durationSeconds = 1.0});
    ri::scene::HumanoidAnimationSourceRegistry registry{};
    const auto registered = ri::scene::RegisterHumanoidAnimationSource(registry, scene, source);
    Expect(registered.clipMap.contains("walk"),
           "registerHumanoidAnimationSource should apply clip aliases into canonical clip map");
    Expect(registered.normalizedBoneAliasLookup.contains("mixamorighips"),
           "registerHumanoidAnimationSource should build normalized bone alias lookup");

    ri::render::software::RgbaImagePathCache texCache{};
    const auto missingOnce = texCache.Load(std::filesystem::path("rawiron_texture_cache_missing.png"));
    const auto missingTwice = texCache.Load(std::filesystem::path("rawiron_texture_cache_missing.png"));
    Expect(missingOnce == nullptr && missingTwice == nullptr,
           "Texture cache should dedupe failed loads without crashing");
    Expect(texCache.CachedEntryCount() == 0U, "Failed texture loads should not populate positive cache rows");

    ri::runtime::RuntimeEventBus bus{};
    std::size_t missionEvents = 0U;
    const auto listener =
        bus.On("missionCompletion", [&](const ri::runtime::RuntimeEvent&) { ++missionEvents; });
    ri::world::MissionCompletionTelemetry mt{};
    mt.missionId = "unit_test";
    mt.outcome = "success";
    mt.missionElapsedSeconds = 12.5;
    ri::world::ApplyAuthoritativeMissionCompletionTransition(&bus, mt, {});
    Expect(missionEvents == 1U, "Mission completion transition should emit terminal telemetry");
    bus.Off("missionCompletion", listener);

    ri::world::HudChannelTtlScheduler hud{};
    hud.Schedule("pickup", "Item acquired", 1000.0);
    hud.Schedule("pickup", "Different item", 500.0);
    Expect(hud.ClearHudDismissTimer("nonexistent") == false,
           "clearHudDismissTimer should report false for unknown channels");
    Expect(hud.ClearHudDismissTimer("pickup"),
           "clearHudDismissTimer should remove active channel timers");
    Expect(!hud.Active("pickup").has_value(),
           "clearHudDismissTimer should clear the channel immediately");
    hud.Schedule("pickup", "Different item", 500.0);
    const auto active = hud.Active("pickup");
    Expect(active.has_value() && active->text.find("Different") != std::string::npos,
           "HUD channel TTL should overwrite prior timers on reschedule");
    hud.Advance(600.0);
    Expect(!hud.Active("pickup").has_value(), "HUD channel TTL should expire after advance");

    const auto startProfile = ri::scene::GetPrimitiveProfilePreset("duct");
    const auto endProfile = ri::scene::GetPrimitiveProfilePreset("beam");
    const auto resampled = ri::scene::ResamplePrimitiveProfileLoop(startProfile, 12U);
    Expect(resampled.size() == 12U,
           "resamplePrimitiveProfileLoop should produce requested closed-loop sample count");
    Expect(ri::scene::GetLoftInterpolationValue(0.5f) > 0.49f
            && ri::scene::GetLoftInterpolationValue(0.5f) < 0.51f,
           "getLoftInterpolationValue should remain centered at mid blend");
    const ri::scene::Mesh loft =
        ri::scene::BuildLoftPrimitiveGeometryFromProfiles(startProfile, endProfile, 8U, true);
    Expect(loft.vertexCount > 0 && loft.indexCount > 0,
           "buildLoftPrimitiveGeometryFromProfiles should emit watertight loft geometry");
}
