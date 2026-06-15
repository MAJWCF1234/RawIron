#include "RawIron/Editor/PreviewSceneRegistry.h"

#include "RawIron/Render/ScenePreviewRenderingScript.h"

#include "RawIron/Scene/WorkspaceSandbox.h"

#include <string>
#include <unordered_map>

namespace ri::editor {

namespace {

std::unordered_map<std::string, EditorPreviewHooks> g_hooks;

const EditorPreviewHooks* Lookup(std::string_view previewSceneId) {
    const std::string key(previewSceneId);
    const auto it = g_hooks.find(key);
    return it == g_hooks.end() ? nullptr : &it->second;
}

} // namespace

void RegisterEditorPreviewScene(const std::string_view previewSceneId, EditorPreviewHooks hooks) {
    g_hooks[std::string(previewSceneId)] = hooks;
}

ri::scene::StarterScene BuildEditorWorkspaceScene(const std::string_view editorPreviewScene,
                                                  const std::string_view workspaceSceneName,
                                                  const std::filesystem::path& gameRoot) {
    if (const EditorPreviewHooks* h = Lookup(editorPreviewScene); h != nullptr && h->build != nullptr) {
        return h->build(workspaceSceneName, gameRoot);
    }
    return ri::scene::BuildStarterScene(workspaceSceneName);
}

void AnimateEditorWorkspaceScene(const std::string_view editorPreviewScene,
                                 ri::scene::StarterScene& starterScene,
                                 const double elapsedSeconds,
                                 const bool editorOrbitAuthoritative) {
    if (const EditorPreviewHooks* h = Lookup(editorPreviewScene); h != nullptr && h->animate != nullptr) {
        h->animate(starterScene, elapsedSeconds);
        return;
    }
    ri::scene::AnimateStarterSceneProps(starterScene, elapsedSeconds);
    if (!editorOrbitAuthoritative) {
        ri::scene::AnimateStarterSceneOrbitPreview(starterScene, elapsedSeconds);
    }
}

void ConfigureEditorViewportForPreview(const std::string_view editorPreviewScene,
                                       ri::render::software::ScenePreviewOptions& options,
                                       const std::filesystem::path* gameRoot) {
    if (const EditorPreviewHooks* h = Lookup(editorPreviewScene); h != nullptr && h->configureViewport != nullptr) {
        h->configureViewport(options);
    }
    if (gameRoot != nullptr && !gameRoot->empty()) {
        const std::filesystem::path renderingScript = *gameRoot / "scripts" / "rendering.riscript";
        const std::filesystem::path postprocessScript = *gameRoot / "scripts" / "postprocess.riscript";
        (void)ri::render::software::TryApplyRenderingScriptFileToScenePreview(renderingScript, options);
        (void)ri::render::software::TryApplyPostprocessScriptFileToScenePreview(
            postprocessScript, renderingScript, options);
    }
}

} // namespace ri::editor
