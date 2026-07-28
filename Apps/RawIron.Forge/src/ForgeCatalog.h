#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ri::forge {

enum class AssetKind {
    ModelSource,
    PrimitiveModel,
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
    std::size_t primitiveModelCount = 0;
    std::size_t rigCount = 0;
    std::size_t invalidPrimitiveModelCount = 0;
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
[[nodiscard]] bool IsPrimitiveModelPath(const std::filesystem::path& path);
[[nodiscard]] bool IsRigPath(const std::filesystem::path& path);
[[nodiscard]] AssetCatalog ScanAssetCatalog(const std::filesystem::path& workspaceRoot);
/// Case-insensitive creator-facing filter over asset path, summary, and asset kind.
[[nodiscard]] std::vector<std::size_t> FilterAssetCatalogIndices(
    const AssetCatalog& catalog,
    std::string_view query);
/// Runs the real engine importer for OBJ/glTF/GLB/FBX sources. Blend files are reported as export-required.
[[nodiscard]] ModelSourceValidationReport ValidateModelSource(const std::filesystem::path& sourcePath);
[[nodiscard]] std::filesystem::path CreateUniqueHumanoidRig(
    const std::filesystem::path& workspaceRoot,
    std::string* errorMessage = nullptr);

[[nodiscard]] std::filesystem::path CreateUniquePrimitiveModel(
    const std::filesystem::path& workspaceRoot,
    std::string* errorMessage = nullptr);
[[nodiscard]] bool AppendPrimitiveToModel(const std::filesystem::path& modelPath,
                                          std::string_view primitivePreset,
                                          std::string_view groupId,
                                          std::string* insertedPartId = nullptr,
                                          std::string* errorMessage = nullptr);
[[nodiscard]] bool AppendGroupToModel(const std::filesystem::path& modelPath,
                                      std::string_view name,
                                      std::string_view parentId,
                                      std::string_view boneName,
                                      std::string* insertedGroupId = nullptr,
                                      std::string* errorMessage = nullptr);

struct PrimitiveModelBakeSummary {
    bool valid = false;
    std::filesystem::path outputPath;
    std::filesystem::path rigMapPath;
    std::size_t inputPartCount = 0;
    std::size_t inputTriangleCount = 0;
    std::size_t outputTriangleCount = 0;
    std::size_t culledTriangleCount = 0;
    std::size_t boneBoundVertexCount = 0;
    std::string summary;
};

[[nodiscard]] PrimitiveModelBakeSummary BakePrimitiveModelAsset(
    const std::filesystem::path& modelPath,
    const std::filesystem::path& outputPath = {});

} // namespace ri::forge
