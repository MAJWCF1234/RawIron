#pragma once

#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Trace/TraceScene.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::games::forestruins {

struct World {
    ri::scene::Scene scene;
    ri::scene::StarterSceneHandles handles;
    int playerRig = ri::scene::kInvalidHandle;
    int playerCameraNode = ri::scene::kInvalidHandle;
    std::vector<ri::trace::TraceCollider> colliders;
};

[[nodiscard]] bool IsForestRuinsGameRoot(const std::filesystem::path& gameRoot);
[[nodiscard]] World BuildForestRuinsWorld(std::string_view sceneName, const std::filesystem::path& gameRoot);
void AnimateForestRuinsWorld(World& world, double elapsedSeconds);

/// Editor workspace slice (orbit camera + wilderness content) using `StarterScene`.
[[nodiscard]] ri::scene::StarterScene BuildForestRuinsEditorScene(std::string_view sceneName,
                                                                  const std::filesystem::path& gameRoot);
void AnimateForestRuinsEditorScene(ri::scene::StarterScene& starterScene, double elapsedSeconds);

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
    float softwareRenderScale = 0.5f;
    std::string gameId = "wilderness-ruins";
    std::filesystem::path workspaceRoot;
    std::filesystem::path gameRoot;
    std::optional<float> mouseSensitivityDegreesPerPixel;
    bool captureMouse = true;
    StandaloneRenderer renderer = StandaloneRenderer::VulkanNative;
    RenderQuality renderQuality = RenderQuality::Balanced;
    StandalonePresentMode presentMode = StandalonePresentMode::Auto;
    int benchmarkFrames = 0;
    std::string windowTitle = "RawIron Forest Ruins";
    bool startFromCheckpoint = false;
    std::string checkpointSlot = "autosave";
    std::optional<std::string> resumeQuery;
    std::filesystem::path checkpointStorageRoot;
};

bool RunStandalone(const StandaloneOptions& options = {}, std::string* error = nullptr);

} // namespace ri::games::forestruins
