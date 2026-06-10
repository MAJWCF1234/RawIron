#include "RawIron/Games/LiminalHall/LiminalHallEditorPreview.h"

#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/WorkspaceSandbox.h"

namespace ri::games::liminal {

void ApplyLiminalHallScenePreviewProfile(ri::render::software::ScenePreviewOptions& options) {
    options.pointSampleTextures = false;
    options.adaptiveTextureSampling = true;
    options.adaptivePointSampleStartDepth = 34.0f;
    options.enableFarHorizon = true;
    options.farHorizonStartDistance = 64.0f;
    options.farHorizonEndDistance = 190.0f;
    options.farHorizonMaxDistance = 340.0f;
    options.farHorizonMaxNodeStride = 3U;
    options.farHorizonMaxInstanceStride = 4U;
    options.orderedDither = true;
    options.fogStartDepth = 2.0f;
    options.fogEndDepth = 48.0f;
    options.fogStrength = 0.90f;
}

namespace {

ri::scene::StarterScene BuildHook(const std::string_view workspaceSceneName, const std::filesystem::path& gameRoot) {
    return BuildEditorStarterScene(workspaceSceneName, gameRoot);
}

void AnimateHook(ri::scene::StarterScene& starterScene, const double elapsedSeconds) {
    AnimateEditorStarterScene(starterScene, elapsedSeconds);
}

void ConfigureHook(ri::render::software::ScenePreviewOptions& options) {
    ApplyLiminalHallScenePreviewProfile(options);
}

} // namespace

void RegisterLiminalHallEditorPreview() {
    ri::editor::EditorPreviewHooks hooks{};
    hooks.build = &BuildHook;
    hooks.animate = &AnimateHook;
    hooks.configureViewport = &ConfigureHook;
    ri::editor::RegisterEditorPreviewScene("liminal-hall", hooks);
}

} // namespace ri::games::liminal
