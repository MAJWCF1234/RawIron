#include "RawIron/Audio/AudioBackendMiniaudio.h"
#include "RawIron/Audio/AudioManager.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Core/MainLoop.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Render/VulkanBootstrap.h"
#include "RawIron/Scene/ScriptedCameraReview.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace {

double ResolveFixedDeltaSeconds(const ri::core::CommandLine& commandLine, int fallbackTickHz) {
    const std::optional<int> tickHz = commandLine.TryGetInt("--tick-hz");
    if (!tickHz.has_value() || *tickHz <= 0) {
        return 1.0 / static_cast<double>(fallbackTickHz);
    }
    return 1.0 / static_cast<double>(*tickHz);
}

class PlayerHost final : public ri::core::Host {
public:
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "RawIron.Player";
    }

    [[nodiscard]] std::string_view GetMode() const noexcept override {
        return "player";
    }

    void OnStartup(const ri::core::CommandLine& commandLine) override {
        headless_ = commandLine.HasFlag("--headless");
        dumpSceneEveryFrame_ = commandLine.HasFlag("--dump-scene-every-frame");
        dumpScene_ = !commandLine.HasFlag("--no-scene-dump");
        logEveryFrame_ = commandLine.HasFlag("--log-every-frame");
        starterScene_ = ri::scene::BuildStarterScene("PlayerSandbox");

        ri::core::LogSection("Player Startup");
        ri::core::LogInfo(headless_ ? "Running headless player stub." : "Running player stub.");
        if (!headless_) {
            std::string audioError;
            std::shared_ptr<ri::audio::AudioBackend> backend = ri::audio::CreateMiniaudioAudioBackend(&audioError);
            if (backend != nullptr) {
                audioManager_ = std::make_shared<ri::audio::AudioManager>(std::move(backend));
                ri::core::LogInfo("Native audio: miniaudio device backend active.");
            } else {
                ri::core::LogInfo("Native audio unavailable: " + (audioError.empty() ? std::string("unknown") : audioError));
            }

            vulkanSummary_ = ri::render::vulkan::RunBootstrap(ri::render::vulkan::VulkanBootstrapOptions{
                .windowTitle = "RawIron Player Vulkan Bootstrap",
                .createSurface = true,
            });
            ri::core::LogSection("Player Vulkan");
            ri::core::LogInfo("Vulkan platform: " + vulkanSummary_.platformName);
            ri::core::LogInfo("Vulkan loader: " + vulkanSummary_.loaderPath);
            ri::core::LogInfo("Vulkan instance API: " + vulkanSummary_.instanceApiVersion);
            ri::core::LogInfo("Vulkan surface: " + vulkanSummary_.surfaceStatus);
            ri::core::LogInfo("Vulkan validation layer: " + std::string(vulkanSummary_.validationLayerAvailable ? "available" : "missing"));
            ri::core::LogInfo("Vulkan instance extensions: " + std::to_string(vulkanSummary_.instanceExtensions.size()));
            ri::core::LogInfo("Vulkan instance layers: " + std::to_string(vulkanSummary_.instanceLayers.size()));
            ri::core::LogInfo("Vulkan selected device: " +
                              (vulkanSummary_.selectedDeviceName.empty() ? std::string("<none>") : vulkanSummary_.selectedDeviceName));
            for (const ri::render::vulkan::VulkanDeviceSummary& device : vulkanSummary_.devices) {
                ri::core::LogInfo(
                    "Device: " + device.name +
                    " type=" + device.type +
                    " api=" + device.apiVersion +
                    " graphicsQueues=" + std::to_string(device.graphicsQueueFamilyCount) +
                    " presentQueues=" + std::to_string(device.presentQueueFamilyCount) +
                    " present=" + std::string(device.presentSupport ? "yes" : "no"));
            }
        }
        ri::core::LogInfo("Bootstrapped scene graph, camera, mesh, material, light, and helper primitives.");
        ri::core::LogInfo("Renderable nodes: " + std::to_string(ri::scene::CollectRenderableNodes(starterScene_.scene).size()));
        ri::core::LogInfo("Orbit camera path: " + ri::scene::DescribeNodePath(starterScene_.scene, starterScene_.handles.orbitCamera.cameraNode));

        if (dumpScene_) {
            ri::core::LogSection("Player Scene");
            ri::core::LogInfo(starterScene_.scene.Describe());
        }

        const std::optional<std::string> scriptedCameraPath = commandLine.GetValue("--scripted-camera");
        scriptedReviewRequested_ = scriptedCameraPath.has_value() || commandLine.HasFlag("--scripted-camera");
        if (scriptedReviewRequested_) {
            ri::scene::ScriptedCameraSequence sequence{};
            std::string loadError;
            if (scriptedCameraPath.has_value() && !scriptedCameraPath->empty()) {
                if (!ri::scene::TryLoadScriptedCameraSequenceFromJsonFile(std::filesystem::path(*scriptedCameraPath),
                                                                          sequence,
                                                                          &loadError)) {
                    ri::core::LogInfo("Scripted camera: could not load JSON (" + loadError +
                                      "). Using built-in starter sequence.");
                    sequence = ri::scene::BuildDefaultStarterSandboxReview();
                }
            } else {
                sequence = ri::scene::BuildDefaultStarterSandboxReview();
            }
            scriptedReview_.Start(std::move(sequence));
            if (commandLine.HasFlag("--scripted-camera-loop")) {
                scriptedReview_.SetLoopPlayback(true);
            }
            if (commandLine.HasFlag("--scripted-camera-verbose")) {
                scriptedReview_.SetStepBeganCallback(
                    [](const std::size_t stepIndex,
                       const ri::scene::ScriptedReviewStepKind /*kind*/,
                       const std::string_view kindName) {
                        ri::core::LogInfo(std::string("Scripted camera step ") + std::to_string(stepIndex) +
                                          " kind=" + std::string(kindName));
                    });
            }
            ri::core::LogInfo("Automated camera review enabled (--scripted-camera). Orbit preview suppressed while the "
                              "sequence runs.");
            if (scriptedReview_.LoopPlayback()) {
                ri::core::LogInfo(R"(Scripted camera loop playback enabled (--scripted-camera-loop or JSON "loop": true).)");
            }
        }
    }

    [[nodiscard]] bool OnFrame(const ri::core::FrameContext& frame) override {
        ri::scene::AnimateStarterSceneProps(starterScene_, frame.elapsedSeconds);
        if (scriptedReviewRequested_ && scriptedReview_.IsActive()) {
            scriptedReview_.Tick(starterScene_.scene, starterScene_.handles.orbitCamera, frame.deltaSeconds);
        } else {
            ri::scene::AnimateStarterSceneOrbitPreview(starterScene_, frame.elapsedSeconds);
        }

        if (audioManager_ != nullptr) {
            const double audioMs =
                frame.frameIndex == 0 ? frame.deltaSeconds * 1000.0 : frame.realDeltaSeconds * 1000.0;
            audioManager_->Tick(std::max(0.0, audioMs));
        }

        if (frame.frameIndex == 0 || logEveryFrame_) {
            const ri::math::Vec3 cratePosition = starterScene_.scene.ComputeWorldPosition(starterScene_.handles.crate);
            const ri::math::Vec3 beaconPosition = starterScene_.scene.ComputeWorldPosition(starterScene_.handles.beacon);
            const ri::math::Vec3 cameraPosition = starterScene_.scene.ComputeWorldPosition(starterScene_.handles.orbitCamera.cameraNode);
            ri::core::LogInfo(
                "Player simulation tick " + std::to_string(frame.frameIndex) +
                " crate=" + ri::math::ToString(cratePosition) +
                " beacon=" + ri::math::ToString(beaconPosition) +
                " camera=" + ri::math::ToString(cameraPosition));
        }

        if (dumpSceneEveryFrame_) {
            ri::core::LogSection("Player Scene Frame " + std::to_string(frame.frameIndex));
            ri::core::LogInfo(starterScene_.scene.Describe());
        }

        return true;
    }

    void OnShutdown() override {
        ri::core::LogSection("Player Shutdown");
        ri::core::LogInfo("Player host shutdown complete.");
    }

private:
    std::shared_ptr<ri::audio::AudioManager> audioManager_{};
    bool scriptedReviewRequested_ = false;
    ri::scene::ScriptedCameraReviewPlayer scriptedReview_{};
    bool headless_ = false;
    bool dumpScene_ = true;
    bool dumpSceneEveryFrame_ = false;
    bool logEveryFrame_ = false;
    ri::render::vulkan::VulkanBootstrapSummary vulkanSummary_{};
    ri::scene::StarterScene starterScene_{};
};

} // namespace

int main(int argc, char** argv) {
    ri::core::InitializeCrashDiagnostics();
    try {
        ri::core::CommandLine commandLine(argc, argv);
        PlayerHost host;

        ri::core::MainLoopOptions options;
        options.maxFrames = commandLine.GetIntOr("--frames", 6);
        options.fixedDeltaSeconds = ResolveFixedDeltaSeconds(commandLine, 60);
        options.verboseFrames = commandLine.HasFlag("--verbose-frames");
        options.paceToFixedDelta = !commandLine.HasFlag("--unpaced");

        return ri::core::RunMainLoop(host, commandLine, options);
    } catch (const std::exception&) {
        ri::core::LogCurrentExceptionWithStackTrace("Player Failure");
        return 1;
    }
}
