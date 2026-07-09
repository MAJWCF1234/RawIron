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

[[nodiscard]] bool IsModelSourcePath(const std::filesystem::path& path);
[[nodiscard]] bool IsRigPath(const std::filesystem::path& path);
[[nodiscard]] AssetCatalog ScanAssetCatalog(const std::filesystem::path& workspaceRoot);
[[nodiscard]] std::filesystem::path CreateUniqueHumanoidRig(
    const std::filesystem::path& workspaceRoot,
    std::string* errorMessage = nullptr);

} // namespace ri::forge
