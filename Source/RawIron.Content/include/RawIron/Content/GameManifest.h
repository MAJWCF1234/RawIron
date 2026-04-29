#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

struct GameManifest {
    std::string id;
    std::string name;
    /// Required game format contract identifier (expected: `rawiron-game-v1.3.7`).
    std::string format;
    std::string type;
    std::string entry;
    /// Human-readable game content version (for editor/runtime display), e.g. `1.3.7`.
    std::string version;
    /// Primary creator/team label shown in tooling UX.
    std::string author;
    /// Canonical editor launch token (must be `--game=<id>` for standardized project dispatch).
    std::string editorProjectArg;
    /// Relative path to the primary authored level primitives CSV.
    std::string primaryLevel;
    std::string description;
    std::vector<std::string> editorOpenArgs;
    /// Declares which built-in editor workspace scene to load (e.g. `"starter"`, `"liminal-hall"`).
    /// Empty means the default starter sandbox — same for all games unless they opt in via manifest data.
    std::string editorPreviewScene;
    std::filesystem::path manifestPath;
    std::filesystem::path rootPath;
};

[[nodiscard]] bool LooksLikeWorkspaceRoot(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path DetectWorkspaceRoot(const std::filesystem::path& startPath);
[[nodiscard]] std::optional<GameManifest> LoadGameManifest(const std::filesystem::path& manifestPath);
[[nodiscard]] std::optional<GameManifest> ResolveGameManifest(const std::filesystem::path& workspaceRoot,
                                                              std::string_view gameId);

/// Returns missing/invalid format issues for the game root.
/// Empty list means the game satisfies the current RawIron game contract.
[[nodiscard]] std::vector<std::string> ValidateGameProjectFormat(const GameManifest& manifest);

/// Safe join of `gameRoot` and a relative asset path. Rejects absolute paths and results that escape
/// `gameRoot` (e.g. via `..`). Returns empty path on failure.
[[nodiscard]] std::filesystem::path ResolveGameAssetPath(const std::filesystem::path& gameRoot,
                                                         std::string_view relativeUtf8);

} // namespace ri::content


