#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxEditorPreview.h"

#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxWorld.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Render/ScenePreview.h"

namespace ri::games::multiplayersandbox {

namespace {

ri::scene::StarterScene BuildHook(const std::string_view sceneName, const std::filesystem::path& gameRoot) {
    World world = BuildWorld(sceneName, gameRoot);
    return ri::scene::StarterScene{
        .scene = std::move(world.scene),
        .handles = world.handles,
    };
}

void AnimateHook(ri::scene::StarterScene& starterScene, const double elapsedSeconds) {
    World world{};
    world.scene = std::move(starterScene.scene);
    world.handles = starterScene.handles;
    AnimateWorld(world, elapsedSeconds);
    starterScene.scene = std::move(world.scene);
}

void ConfigureHook(ri::render::software::ScenePreviewOptions& options) {
    options.pointSampleTextures = false;
    options.clearTop = ri::math::Vec3{0.34f, 0.42f, 0.55f};
    options.clearBottom = ri::math::Vec3{0.05f, 0.08f, 0.12f};
    options.fogColor = ri::math::Vec3{0.40f, 0.47f, 0.55f};
    options.ambientLight = ri::math::Vec3{0.06f, 0.07f, 0.09f};
}

} // namespace

void RegisterMultiplayerSandboxEditorPreview() {
    ri::editor::EditorPreviewHooks hooks{};
    hooks.build = &BuildHook;
    hooks.animate = &AnimateHook;
    hooks.configureViewport = &ConfigureHook;
    ri::editor::RegisterEditorPreviewScene("rawiron-multiplayer-sandbox", hooks);
}

} // namespace ri::games::multiplayersandbox
