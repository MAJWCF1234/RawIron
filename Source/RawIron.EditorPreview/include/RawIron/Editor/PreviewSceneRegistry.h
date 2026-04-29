#pragma once

#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/WorkspaceSandbox.h"

#include <filesystem>
#include <string_view>

namespace ri::editor {

struct EditorPreviewHooks {
    ri::scene::StarterScene (*build)(std::string_view workspaceSceneName, const std::filesystem::path& gameRoot);
    void (*animate)(ri::scene::StarterScene& starterScene, double elapsedSeconds);
    void (*configureViewport)(ri::render::software::ScenePreviewOptions& options);
};

void RegisterEditorPreviewScene(std::string_view previewSceneId, EditorPreviewHooks hooks);

[[nodiscard]] ri::scene::StarterScene BuildEditorWorkspaceScene(std::string_view editorPreviewScene,
                                                                std::string_view workspaceSceneName,
                                                                const std::filesystem::path& gameRoot);

void AnimateEditorWorkspaceScene(std::string_view editorPreviewScene,
                                 ri::scene::StarterScene& starterScene,
                                 double elapsedSeconds);

void ConfigureEditorViewportForPreview(std::string_view editorPreviewScene,
                                         ri::render::software::ScenePreviewOptions& options);

} // namespace ri::editor
