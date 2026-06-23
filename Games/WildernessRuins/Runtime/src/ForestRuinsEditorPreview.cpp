#include "RawIron/Games/ForestRuins/ForestRuinsEditorPreview.h"

#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "RawIron/Games/ForestRuins/ForestRuinsRuntime.h"
#include "RawIron/Render/ScenePreview.h"

namespace ri::games::forestruins {

void ApplyForestRuinsScenePreviewProfile(ri::render::software::ScenePreviewOptions& options) {
    options.pointSampleTextures = false;
    options.adaptiveTextureSampling = true;
    options.adaptivePointSampleStartDepth = 42.0f;
    options.enableFarHorizon = true;
    options.farHorizonStartDistance = 72.0f;
    options.farHorizonEndDistance = 220.0f;
    options.farHorizonMaxDistance = 480.0f;
    options.farHorizonMaxNodeStride = 3U;
    options.farHorizonMaxInstanceStride = 4U;
    options.orderedDither = true;
    options.fogStartDepth = 14.0f;
    options.fogEndDepth = 110.0f;
    options.fogStrength = 0.40f;
    options.clearTop = ri::math::Vec3{0.22f, 0.32f, 0.38f};
    options.clearBottom = ri::math::Vec3{0.06f, 0.10f, 0.07f};
    options.fogColor = ri::math::Vec3{0.14f, 0.20f, 0.18f};
    options.fogColorFar = ri::math::Vec3{0.26f, 0.32f, 0.27f};
    options.ambientLight = ri::math::Vec3{0.10f, 0.12f, 0.09f};
}

namespace {

ri::scene::StarterScene BuildHook(const std::string_view sceneName, const std::filesystem::path& gameRoot) {
    (void)gameRoot;
    ri::scene::StarterScene starterScene = ri::scene::BuildStarterScene(sceneName);
    starterScene.scene.GetNode(starterScene.handles.root).name = "WildernessRuinsLayer";
    starterScene.scene.GetNode(starterScene.handles.grid).name = "ForestAuthoringGrid";
    return starterScene;
}

void AnimateHook(ri::scene::StarterScene& starterScene, const double elapsedSeconds) {
    ri::scene::AnimateStarterScene(starterScene, elapsedSeconds);
}

void ConfigureHook(ri::render::software::ScenePreviewOptions& options) {
    ApplyForestRuinsScenePreviewProfile(options);
}

} // namespace

void RegisterForestRuinsEditorPreview() {
    ri::editor::EditorPreviewHooks hooks{};
    hooks.build = &BuildHook;
    hooks.animate = &AnimateHook;
    hooks.configureViewport = &ConfigureHook;
    ri::editor::RegisterEditorPreviewScene("wilderness-ruins", hooks);
}

} // namespace ri::games::forestruins
