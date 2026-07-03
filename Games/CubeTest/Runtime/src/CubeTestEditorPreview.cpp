#include "RawIron/Games/CubeTest/CubeTestEditorPreview.h"

#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Render/ScenePreview.h"

namespace ri::games::cubetest {

namespace {

ri::scene::StarterScene BuildHook(const std::string_view sceneName, const std::filesystem::path& gameRoot) {
    (void)gameRoot;
    CubeTestWorld world = BuildCubeTestWorld(sceneName);
    ri::scene::StarterScene starterScene{};
    starterScene.scene = std::move(world.scene);
    starterScene.handles.root = world.rootNode;
    starterScene.handles.crate = world.playerRig;
    starterScene.handles.beacon = world.playerCameraNode;
    return starterScene;
}

void AnimateHook(ri::scene::StarterScene& starterScene, const double elapsedSeconds) {
    CubeTestWorld world{};
    world.scene = std::move(starterScene.scene);
    world.rootNode = starterScene.handles.root;
    AnimateCubeTestWorld(world, elapsedSeconds);
    starterScene.scene = std::move(world.scene);
}

void ConfigureHook(ri::render::software::ScenePreviewOptions& options) {
    options.pointSampleTextures = false;
    options.adaptiveTextureSampling = true;
    options.clearTop = ri::math::Vec3{0.58f, 0.66f, 0.72f};
    options.clearBottom = ri::math::Vec3{0.32f, 0.35f, 0.36f};
    options.fogColor = ri::math::Vec3{0.50f, 0.55f, 0.58f};
    options.fogStartDepth = 18.0f;
    options.fogEndDepth = 80.0f;
    options.fogStrength = 0.28f;
    options.ambientLight = ri::math::Vec3{0.26f, 0.28f, 0.30f};
    options.previewExposure = 1.08f;
    options.previewContrast = 1.08f;
}

} // namespace

void RegisterCubeTestEditorPreview() {
    ri::editor::EditorPreviewHooks hooks{};
    hooks.build = &BuildHook;
    hooks.animate = &AnimateHook;
    hooks.configureViewport = &ConfigureHook;
    ri::editor::RegisterEditorPreviewScene("cube-test", hooks);
}

} // namespace ri::games::cubetest
