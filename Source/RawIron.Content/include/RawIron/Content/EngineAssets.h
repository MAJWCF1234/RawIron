#pragma once

#include <filesystem>

namespace ri::content {

/// True when `dir` is a directory that contains at least one `.png` file (shared preview texture library).
[[nodiscard]] bool IsEngineTextureLibraryDirectory(const std::filesystem::path& dir);

/// Resolves `Assets/Textures` for the shipped library (legacy: `Engine/Textures` if present). Order:
/// 1. Walk upward from the running module directory (install layout: `<exe>/Assets/Textures`).
/// 2. Walk upward from the current working directory.
/// 3. `DetectWorkspaceRoot(...) / Assets / Textures` from module path and from cwd.
///
/// `applicationPath` may be empty (use OS module path). If non-empty, it should be the main
/// executable file path; its parent directory is searched first before other roots.
[[nodiscard]] std::filesystem::path ResolveEngineTexturesDirectory(
    const std::filesystem::path& applicationPath = {});

/// Editor / tools: prefer `<workspaceRoot>/Assets/Textures` when it contains PNGs (stable even when
/// the process cwd is unrelated); otherwise same resolution as `ResolveEngineTexturesDirectory`.
[[nodiscard]] std::filesystem::path PickEngineTexturesDirectory(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& applicationExecutablePath = {});

} // namespace ri::content
