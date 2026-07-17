#include "RawIron/Editor/PreviewSceneRegistry.h"

#include "RawIron/Content/GameManifest.h"
#include "RawIron/Render/ScenePreviewRenderingScript.h"
#include "RawIron/Scene/PrimitivesCsvIO.h"
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

ri::scene::StarterScene BuildAuthoredProjectFallback(const std::string_view workspaceSceneName,
                                                     const std::filesystem::path& gameRoot) {
    ri::scene::StarterScene scene = ri::scene::BuildStarterScene(workspaceSceneName);
    const std::optional<ri::content::GameManifest> manifest =
        ri::content::LoadGameManifest(gameRoot / "manifest.json");
    if (!manifest.has_value()) {
        return scene;
    }
    const std::filesystem::path primaryLevel =
        ri::content::ResolveGameAssetPath(gameRoot, manifest->primaryLevel);
    ri::scene::AssemblyPrimitivesImportResult importResult{};
    std::string importError;
    if (!ri::scene::TryImportAssemblyPrimitivesCsv(
            scene.scene,
            scene.handles.root,
            primaryLevel,
            &importResult,
            &importError)) {
        return scene;
    }

    // Keep the engine-owned grid, sun, and orbit camera, but remove starter showcase props once
    // a real authored assembly has loaded. This is the generic editor path for projects that do
    // not need a custom compiled preview module.
    if (scene.handles.crate != ri::scene::kInvalidHandle) {
        scene.scene.GetNode(scene.handles.crate).localTransform.scale = {0.001f, 0.001f, 0.001f};
    }
    if (scene.handles.beacon != ri::scene::kInvalidHandle) {
        scene.scene.GetNode(scene.handles.beacon).localTransform.scale = {0.001f, 0.001f, 0.001f};
    }
    ri::scene::OrbitCameraState orbit = scene.handles.orbitCamera.orbit;
    orbit.target = {0.0f, 0.75f, 0.0f};
    orbit.distance = 18.0f;
    orbit.yawDegrees = 150.0f;
    orbit.pitchDegrees = -24.0f;
    ri::scene::SetOrbitCameraState(scene.scene, scene.handles.orbitCamera, orbit);
    return scene;
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
    if (!gameRoot.empty()) {
        return BuildAuthoredProjectFallback(workspaceSceneName, gameRoot);
    }
    return ri::scene::BuildStarterScene(workspaceSceneName);
}

bool AnimateEditorWorkspaceScene(const std::string_view editorPreviewScene,
                                 ri::scene::StarterScene& starterScene,
                                 const double elapsedSeconds,
                                 const bool editorOrbitAuthoritative) {
    if (const EditorPreviewHooks* h = Lookup(editorPreviewScene); h != nullptr && h->animate != nullptr) {
        h->animate(starterScene, elapsedSeconds);
        return true;
    }
    if (!editorPreviewScene.empty() && editorPreviewScene != "starter") {
        if (!editorOrbitAuthoritative) {
            ri::scene::AnimateStarterSceneOrbitPreview(starterScene, elapsedSeconds);
            return true;
        }
        return false;
    }
    ri::scene::AnimateStarterSceneProps(starterScene, elapsedSeconds);
    if (!editorOrbitAuthoritative) {
        ri::scene::AnimateStarterSceneOrbitPreview(starterScene, elapsedSeconds);
    }
    return true;
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
