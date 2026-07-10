#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace ri::forge {

enum class AssetKind {
    ModelSource,
    Rig,
};

struct AssetEntry {
    std::filesystem::path absolutePath;
    std::string relativePath;
    AssetKind kind = AssetKind::ModelSource;
    bool valid = true;
    std::string summary;
};

struct AssetCatalog {
    std::filesystem::path workspaceRoot;
    std::filesystem::path sourceRoot;
    std::vector<AssetEntry> entries;
    std::size_t modelCount = 0;
    std::size_t rigCount = 0;
    std::size_t invalidRigCount = 0;
};

struct ModelSourceValidationReport {
    bool valid = false;
    bool runtimeImportable = false;
    std::size_t nodeCount = 0;
    std::size_t meshCount = 0;
    std::size_t materialCount = 0;
    std::string summary{};
};

[[nodiscard]] bool IsModelSourcePath(const std::filesystem::path& path);
[[nodiscard]] bool IsRigPath(const std::filesystem::path& path);
[[nodiscard]] AssetCatalog ScanAssetCatalog(const std::filesystem::path& workspaceRoot);
/// Runs the real engine importer for OBJ/glTF/GLB/FBX sources. Blend files are reported as export-required.
[[nodiscard]] ModelSourceValidationReport ValidateModelSource(const std::filesystem::path& sourcePath);
[[nodiscard]] std::filesystem::path CreateUniqueHumanoidRig(
    const std::filesystem::path& workspaceRoot,
    std::string* errorMessage = nullptr);

} // namespace ri::forge
