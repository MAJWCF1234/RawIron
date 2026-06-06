#pragma once

#include <filesystem>

namespace ri::ui {

/// Primary game-local UI flow manifest.
[[nodiscard]] constexpr std::string_view PrimaryUiManifestRelativePath() noexcept {
    return "ui/main.ui.json";
}

[[nodiscard]] inline std::filesystem::path PrimaryUiManifestPath(const std::filesystem::path& gameRoot) {
    return gameRoot / PrimaryUiManifestRelativePath();
}

/// Primary game-local VN/dialogue flow manifest.
[[nodiscard]] constexpr std::string_view PrimaryVisualNovelManifestRelativePath() noexcept {
    return "ui/vn_intro.ui.json";
}

[[nodiscard]] inline std::filesystem::path PrimaryVisualNovelManifestPath(const std::filesystem::path& gameRoot) {
    return gameRoot / PrimaryVisualNovelManifestRelativePath();
}

/// Relative path from workspace/repository root to the stock menu template manifest.
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
