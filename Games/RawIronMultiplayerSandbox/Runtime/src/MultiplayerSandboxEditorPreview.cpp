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
    options.clearTop = ri::math::Vec3{0.70f, 0.73f, 0.78f};
    options.clearBottom = ri::math::Vec3{0.54f, 0.58f, 0.63f};
    options.fogColor = ri::math::Vec3{0.78f, 0.80f, 0.84f};
    options.ambientLight = ri::math::Vec3{0.18f, 0.19f, 0.21f};
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
