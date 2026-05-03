#pragma once

#include <filesystem>

namespace ri::ui {

/// Relative path from workspace/repository root to the stock menu manifest (`Assets/UI/default_menu.ui.json`).
[[nodiscard]] constexpr std::string_view DefaultUiManifestRelativePath() noexcept {
    return "Assets/UI/default_menu.ui.json";
}

[[nodiscard]] inline std::filesystem::path DefaultUiManifestPath(const std::filesystem::path& workspaceRoot) {
    return workspaceRoot / DefaultUiManifestRelativePath();
}

/// Branching dialogue sample (`RawIron.UiMenu --demo-vn`).
[[nodiscard]] constexpr std::string_view VisualNovelDemoManifestRelativePath() noexcept {
    return "Assets/UI/visual_novel_demo.ui.json";
}

[[nodiscard]] inline std::filesystem::path VisualNovelDemoManifestPath(const std::filesystem::path& workspaceRoot) {
    return workspaceRoot / VisualNovelDemoManifestRelativePath();
}

} // namespace ri::ui
