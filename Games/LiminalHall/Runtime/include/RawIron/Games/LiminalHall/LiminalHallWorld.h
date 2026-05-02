#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/TraceScene.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::games::liminal {

/// Full Liminal Hall play space (meshes, player rig, trace colliders). Game-only; not part of core scene utilities.
struct World {
    struct LogicDemo {
        int pressurePlateNode = ri::scene::kInvalidHandle;
        int doorNode = ri::scene::kInvalidHandle;
        int portalNode = ri::scene::kInvalidHandle;
        int logicNodePressure = ri::scene::kInvalidHandle;
        int logicNodeDoor = ri::scene::kInvalidHandle;
        int logicNodePortal = ri::scene::kInvalidHandle;
        int logicWireA = ri::scene::kInvalidHandle;
        int logicWireB = ri::scene::kInvalidHandle;
        int logicIoTrunk = ri::scene::kInvalidHandle;
        std::vector<int> logicPressureVisualNodes{};
        std::vector<int> logicDoorVisualNodes{};
        std::vector<int> logicPortalVisualNodes{};
        std::vector<int> logicWireVisualNodes{};
        std::vector<int> logicLayerNodes{};
        std::vector<ri::math::Vec3> logicLayerVisibleScales{};
        ri::spatial::Aabb pressurePlateBounds{};
        ri::spatial::Aabb portalBounds{};
        ri::math::Vec3 doorClosedPosition{};
        ri::math::Vec3 doorOpenPosition{};
    };

    ri::scene::Scene scene;
    ri::scene::StarterSceneHandles handles;
    int playerRig = ri::scene::kInvalidHandle;
    int playerCameraNode = ri::scene::kInvalidHandle;
    std::vector<ri::trace::TraceCollider> colliders;
    LogicDemo logicDemo{};
};

[[nodiscard]] World BuildWorld(std::string_view sceneName, const std::filesystem::path& gameRoot);

void AnimateWorld(World& world, double elapsedSeconds);

/// Editor workspace slice (orbit camera + hall) using shared `StarterScene` handles layout.
[[nodiscard]] ri::scene::StarterScene BuildEditorStarterScene(std::string_view sceneName,
                                                              const std::filesystem::path& gameRoot);
void AnimateEditorStarterScene(ri::scene::StarterScene& starterScene, double elapsedSeconds);

enum class StandaloneRenderer {
    VulkanNative,
};

enum class RenderQuality {
    Competitive,
    Balanced,
    Cinematic,
};

enum class StandalonePresentMode {
    Auto,
    Mailbox,
    Immediate,
    Fifo,
};

struct StandaloneOptions {
    int width = 1280;
    int height = 720;
    /// Reserved for legacy tooling paths; standalone gameplay uses native Vulkan.
    float softwareRenderScale = 0.5f;
    std::string gameId = "liminal-hall";
    std::filesystem::path workspaceRoot;
    std::filesystem::path gameRoot;
    std::optional<float> mouseSensitivityDegreesPerPixel;
    bool captureMouse = true;
    StandaloneRenderer renderer = StandaloneRenderer::VulkanNative;
    RenderQuality renderQuality = RenderQuality::Balanced;
    StandalonePresentMode presentMode = StandalonePresentMode::Auto;
    int benchmarkFrames = 0;
    std::string windowTitle = "RawIron Liminal Void (WASD + Mouse, ESC to exit)";
    bool startFromCheckpoint = false;
    std::string checkpointSlot = "autosave";
    std::optional<std::string> resumeQuery;
    std::filesystem::path checkpointStorageRoot;
};

bool RunStandalone(const StandaloneOptions& options = {}, std::string* error = nullptr);

struct HeadlessCaptureOptions {
    StandaloneOptions standalone{};
    int frames = 240;
    float deltaSeconds = 1.0f / 60.0f;
    std::filesystem::path outputPath;
    bool autoplay = true;
    bool softwareLowSpec = false;
};

bool RunHeadlessCapture(const HeadlessCaptureOptions& options, std::string* error = nullptr);

} // namespace ri::games::liminal
