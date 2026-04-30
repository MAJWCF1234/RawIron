#include "RawIron/Editor/BundledGamePreviews.h"
#include "RawIron/Editor/PreviewSceneRegistry.h"

#if defined(RAWIRON_EDITOR_BUNDLE_LIMINAL)
#include "RawIron/Games/LiminalHall/LiminalHallEditorPreview.h"
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_WILDERNESS)
#include "RawIron/Games/ForestRuins/ForestRuinsRuntime.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Render/ScenePreview.h"
#endif

#include <filesystem>

namespace ri::editor {

void RegisterBundledGameEditorPreviews() {
#if defined(RAWIRON_EDITOR_BUNDLE_LIMINAL)
    ri::games::liminal::RegisterLiminalHallEditorPreview();
#endif
#if defined(RAWIRON_EDITOR_BUNDLE_WILDERNESS)
    ri::editor::EditorPreviewHooks hooks{};
    hooks.build = [](const std::string_view sceneName, const std::filesystem::path& gameRoot) {
        return ri::games::forestruins::BuildForestRuinsEditorScene(sceneName, gameRoot);
    };
    hooks.animate = [](ri::scene::StarterScene& starterScene, const double elapsedSeconds) {
        ri::games::forestruins::AnimateForestRuinsEditorScene(starterScene, elapsedSeconds);
    };
    hooks.configureViewport = [](ri::render::software::ScenePreviewOptions& options) {
        options.clearTop = ri::math::Vec3{0.33f, 0.42f, 0.49f};
        options.clearBottom = ri::math::Vec3{0.17f, 0.21f, 0.17f};
        options.fogColor = ri::math::Vec3{0.42f, 0.46f, 0.44f};
        options.ambientLight = ri::math::Vec3{0.10f, 0.11f, 0.10f};
    };
    ri::editor::RegisterEditorPreviewScene("wilderness-ruins", hooks);
#endif
}

} // namespace ri::editor
