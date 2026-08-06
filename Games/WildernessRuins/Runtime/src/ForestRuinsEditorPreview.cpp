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
    // Keep in sync with Games/WildernessRuins/scripts/rendering.riscript.
    options.fogStartDepth = 10.0f;
    options.fogEndDepth = 85.0f;
    options.fogStrength = 0.55f;
    options.clearTop = ri::math::Vec3{0.42f, 0.48f, 0.52f};
    options.clearBottom = ri::math::Vec3{0.18f, 0.22f, 0.18f};
    options.fogColor = ri::math::Vec3{0.28f, 0.32f, 0.30f};
    options.fogColorFar = ri::math::Vec3{0.34f, 0.38f, 0.36f};
    options.ambientLight = ri::math::Vec3{0.22f, 0.24f, 0.20f};
}

namespace {

ri::scene::StarterScene BuildHook(const std::string_view sceneName, const std::filesystem::path& gameRoot) {
    return BuildForestRuinsEditorScene(sceneName, gameRoot);
}

void AnimateHook(ri::scene::StarterScene& starterScene, const double elapsedSeconds) {
    AnimateForestRuinsEditorScene(starterScene, elapsedSeconds);
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
