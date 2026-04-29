#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

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

using TextureAliasManifest = std::unordered_map<std::string, std::string>;
using AssetVariantMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
[[nodiscard]] const TextureAliasManifest& GetTextureAliasManifest();
/// Combined lookup: known aliases, then exact path keys produced by discovery (see `BuildHydratedTextureAliasManifest`).
[[nodiscard]] std::string ResolveTextureAliasWithManifest(std::string_view authoredTextureName,
                                                          const TextureAliasManifest& manifest);
[[nodiscard]] std::string ResolveTextureAlias(std::string_view authoredTextureName);
[[nodiscard]] TextureAliasManifest MergeTextureAliasManifestsOverlayWins(const TextureAliasManifest& base,
                                                                         const TextureAliasManifest& overlay);
/// Walk `texturesRoot` for `.png` files and map relative paths (and unique stems) to path-without-extension keys.
[[nodiscard]] TextureAliasManifest DiscoverTextureAliasesUnderTexturesRoot(const std::filesystem::path& texturesRoot,
                                                                           std::size_t maxEntries = 4096U);
/// Built-in manifest overlaid with on-disk names under an `Assets/Textures`-style root (authoritative resolver glue).
[[nodiscard]] TextureAliasManifest BuildHydratedTextureAliasManifest(const std::filesystem::path& texturesRoot,
                                                                     std::size_t maxEntries = 4096U);

struct AssetVariantResolveRequest {
    std::string logicalAssetId;
    std::string variant;
    std::string platform;
    std::string qualityTier;
};

/// Resolves `logicalAssetId` to a concrete variant id/path using explicit variant maps first, then texture aliases.
[[nodiscard]] std::string ResolveAssetVariantId(const AssetVariantResolveRequest& request,
                                                const AssetVariantMap& variantManifest,
                                                const TextureAliasManifest& textureAliases);
/// Returns false and fills `error` when variant keys or concrete ids are malformed.
[[nodiscard]] bool ValidateAssetVariantManifest(const AssetVariantMap& variantManifest, std::string* error = nullptr);

} // namespace ri::content
