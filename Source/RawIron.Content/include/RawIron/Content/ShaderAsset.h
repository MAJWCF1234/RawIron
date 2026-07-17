#pragma once

#include "RawIron/Render/ShaderConfig.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

enum class RawIronShaderDomain {
    Surface,
    PostProcess,
    Ui,
    Vfx,
    Compute,
};

struct RawIronShaderTexture {
    std::string name;
    std::string relativePath;
    bool required = true;
    bool srgb = true;
    std::filesystem::path resolvedPath;
};

/// Engine-native shader asset (`*.rishader`). The JSON document carries stable identity/domain metadata,
/// optional texture dependencies, and the same `post`/`effects` presentation stack accepted by `shader.cfg`.
struct RawIronShaderAsset {
    static constexpr int kFormatVersion = 1;

    int formatVersion = kFormatVersion;
    std::string id;
    std::string name;
    RawIronShaderDomain domain = RawIronShaderDomain::Surface;
    std::string stage;
    std::string entryPoint = "main";
    std::vector<RawIronShaderTexture> textures;
    ri::render::ShaderPresentationConfig presentation{};
    std::filesystem::path assetPath;
};

struct RawIronShaderManifestEntry {
    std::string key;
    std::string relativePath;
    RawIronShaderAsset asset;
};

[[nodiscard]] std::string_view ToString(RawIronShaderDomain domain) noexcept;
[[nodiscard]] std::optional<RawIronShaderDomain> TryParseRawIronShaderDomain(std::string_view value) noexcept;

/// Loads and validates one native shader asset. Texture paths are relative to the `.rishader` file.
[[nodiscard]] bool LoadRawIronShaderAsset(const std::filesystem::path& path,
                                          RawIronShaderAsset* out,
                                          std::string* error = nullptr);

/// Loads `<gameRoot>/assets/shaders.manifest` and every referenced `.rishader` asset.
/// The operation is transactional: `out` is unchanged if any row or asset is invalid.
[[nodiscard]] bool LoadRawIronShaderManifest(const std::filesystem::path& gameRoot,
                                             std::vector<RawIronShaderManifestEntry>* out,
                                             std::vector<std::string>* errors = nullptr);

/// Returns validation issues for an existing shader manifest. A project without a shader manifest is valid.
[[nodiscard]] std::vector<std::string> ValidateRawIronShaderManifest(const std::filesystem::path& gameRoot);

/// Composes enabled post-process-domain assets in manifest order. Surface/UI/VFX/compute assets are skipped.
[[nodiscard]] std::optional<ri::render::ShaderPresentationConfig> ComposeRawIronShaderPresentation(
    const std::vector<RawIronShaderManifestEntry>& entries);

/// Loads a game's shader manifest and returns its composed post-process presentation when present.
[[nodiscard]] bool TryLoadRawIronShaderPresentation(const std::filesystem::path& gameRoot,
                                                    ri::render::ShaderPresentationConfig* out,
                                                    std::vector<std::string>* errors = nullptr);

} // namespace ri::content
