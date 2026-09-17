#pragma once

#include "RawIron/Scene/RigAuthoring.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::forge {

enum class AssetKind {
    ModelSource,
    PrimitiveModel,
    Sculpt,
    Rig,
    Animation,
};

struct AssetEntry {
    std::filesystem::path absolutePath;
    std::string relativePath;
    AssetKind kind = AssetKind::ModelSource;
    bool valid = true;
    std::string summary;
    /// Sidecar rig relative to `Assets/Source`, or the entry's own relative path for rigs.
    std::string rigPath;
};

struct AssetCatalog {
    std::filesystem::path workspaceRoot;
    std::filesystem::path sourceRoot;
    std::vector<AssetEntry> entries;
    std::size_t modelCount = 0;
    std::size_t primitiveModelCount = 0;
    std::size_t sculptCount = 0;
    std::size_t rigCount = 0;
    std::size_t animationCount = 0;
    std::size_t invalidPrimitiveModelCount = 0;
    std::size_t invalidSculptCount = 0;
    std::size_t invalidRigCount = 0;
    std::size_t invalidAnimationCount = 0;
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
[[nodiscard]] bool IsSculptPath(const std::filesystem::path& path);
[[nodiscard]] bool IsRigPath(const std::filesystem::path& path);
[[nodiscard]] bool IsAnimationPath(const std::filesystem::path& path);
[[nodiscard]] AssetCatalog ScanAssetCatalog(const std::filesystem::path& workspaceRoot);
/// Case-insensitive creator-facing filter over asset path, summary, and asset kind.
[[nodiscard]] std::vector<std::size_t> FilterAssetCatalogIndices(
    const AssetCatalog& catalog,
    std::string_view query);
[[nodiscard]] std::filesystem::path ResolveCatalogRigPath(
    const AssetCatalog& catalog,
    std::string_view rigPath,
    const std::filesystem::path& documentPath = {});
/// Catalog indices of valid clips whose sidecar rig resolves to the same file as `rigPath`.
[[nodiscard]] std::vector<std::size_t> AnimationIndicesForRig(
    const AssetCatalog& catalog,
    std::string_view rigPath,
    const std::filesystem::path& documentPath = {});
/// Runs the real engine importer for OBJ/glTF/GLB/FBX sources. Blend files are reported as export-required.
[[nodiscard]] ModelSourceValidationReport ValidateModelSource(const std::filesystem::path& sourcePath);
[[nodiscard]] std::filesystem::path CreateUniqueHumanoidRig(
    const std::filesystem::path& workspaceRoot,
    std::string* errorMessage = nullptr);
[[nodiscard]] std::filesystem::path DuplicateRig(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& rigPath,
    std::string* errorMessage = nullptr);
[[nodiscard]] bool DeleteRig(
    const std::filesystem::path& rigPath,
    std::string* errorMessage = nullptr);

[[nodiscard]] std::filesystem::path CreateUniquePrimitiveModel(
    const std::filesystem::path& workspaceRoot,
    std::string* errorMessage = nullptr);
[[nodiscard]] std::filesystem::path DuplicatePrimitiveModel(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& modelPath,
    std::string* errorMessage = nullptr);
[[nodiscard]] bool DeletePrimitiveModel(
    const std::filesystem::path& modelPath,
    std::string* errorMessage = nullptr);
[[nodiscard]] std::filesystem::path CreateUniqueNativeSculpt(
    const std::filesystem::path& workspaceRoot,
    std::string_view cage = "sphere",
    std::string* errorMessage = nullptr);
[[nodiscard]] std::filesystem::path DuplicateNativeSculpt(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& sculptPath,
    std::string* errorMessage = nullptr);
[[nodiscard]] bool DeleteNativeSculpt(
    const std::filesystem::path& sculptPath,
    std::string* errorMessage = nullptr);
[[nodiscard]] std::filesystem::path CreateUniqueNativeAnimation(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& rigPath,
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
[[nodiscard]] bool DuplicatePrimitiveModelElement(
    const std::filesystem::path& modelPath,
    std::string_view elementId,
    std::string* insertedId = nullptr,
    std::string* errorMessage = nullptr);
[[nodiscard]] bool RemovePrimitiveModelElement(
    const std::filesystem::path& modelPath,
    std::string_view elementId,
    std::string* errorMessage = nullptr);
[[nodiscard]] std::filesystem::path DuplicateNativeAnimation(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& clipPath,
    std::string* errorMessage = nullptr);
/// Deletes a motion clip file from disk. Returns false when the path is missing or locked.
[[nodiscard]] bool DeleteNativeAnimation(
    const std::filesystem::path& clipPath,
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

[[nodiscard]] std::string RelativeSourcePath(
    const std::filesystem::path& workspaceRoot,
    const std::filesystem::path& absolutePath);
[[nodiscard]] std::optional<ri::scene::RigDefinition> LoadSidecarRig(
    std::string_view rigPath,
    const std::filesystem::path& documentPath);
[[nodiscard]] bool BindSculptToRig(
    const std::filesystem::path& sculptPath,
    const std::filesystem::path& rigPath,
    const std::filesystem::path& workspaceRoot,
    std::string* errorMessage = nullptr);
[[nodiscard]] bool BindPrimitiveModelToRig(
    const std::filesystem::path& modelPath,
    const std::filesystem::path& rigPath,
    const std::filesystem::path& workspaceRoot,
    std::string* errorMessage = nullptr);
[[nodiscard]] bool BindPrimitiveElementToBone(
    const std::filesystem::path& modelPath,
    std::string_view elementId,
    std::string_view boneName,
    std::string* errorMessage = nullptr);

} // namespace ri::forge
