#include "RawIron/Games/LiminalHall/LiminalHallEditorPreview.h"

#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"

#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/WorkspaceSandbox.h"

namespace ri::games::liminal {

namespace {

ri::scene::StarterScene BuildHook(const std::string_view workspaceSceneName, const std::filesystem::path& gameRoot) {
    return BuildEditorStarterScene(workspaceSceneName, gameRoot);
}

void AnimateHook(ri::scene::StarterScene& starterScene, const double elapsedSeconds) {
    AnimateEditorStarterScene(starterScene, elapsedSeconds);
}

void ConfigureHook(ri::render::software::ScenePreviewOptions& options) {
    options.clearTop = ri::math::Vec3{0.08f, 0.09f, 0.11f};
    options.clearBottom = ri::math::Vec3{0.14f, 0.15f, 0.17f};
    options.fogColor = ri::math::Vec3{0.22f, 0.23f, 0.26f};
    options.ambientLight = ri::math::Vec3{0.14f, 0.15f, 0.16f};
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
