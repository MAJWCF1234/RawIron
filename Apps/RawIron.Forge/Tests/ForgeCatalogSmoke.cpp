#include "ForgeCatalog.h"
#include "ForgeCatalogIndex.h"
#include "ForgePreviewBuilder.h"

#include "RawIron/Scene/RigAuthoring.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

int main() {
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("RawIronForgeCatalogSmoke_" + std::to_string(stamp));
    const fs::path source = root / "Assets" / "Source";

    std::error_code error{};
    fs::create_directories(source / "models", error);
    if (error) {
        return 1;
    }

    {
        std::ofstream(source / "models" / "crate.obj")
            << "o triangle\n"
            << "v 0 0 0\n"
            << "v 1 0 0\n"
            << "v 0 1 0\n"
            << "f 1 2 3\n";
        std::ofstream(source / "models" / "crate.blend") << "fixture";
        std::ofstream(source / "models" / "ignore.txt") << "fixture";
        std::ofstream(source / "broken.ri_rig.json") << "{not-json";
    }
    const ri::scene::RigDefinition fixture = ri::scene::CreateHumanoidRigDefinition("fixture", "Fixture");
    if (!ri::scene::SaveRigDefinition(source / "fixture.ri_rig.json", fixture)) {
        fs::remove_all(root, error);
        return 2;
    }

    const ri::forge::AssetCatalog initial = ri::forge::ScanAssetCatalog(root);
    if (initial.modelCount != 2U || initial.rigCount != 2U || initial.invalidRigCount != 1U) {
        fs::remove_all(root, error);
        return 3;
    }
    const ri::forge::ModelSourceValidationReport modelValidation =
        ri::forge::ValidateModelSource(source / "models" / "crate.obj");
    if (!modelValidation.valid || !modelValidation.runtimeImportable || modelValidation.meshCount == 0U) {
        fs::remove_all(root, error);
        return 4;
    }
    const ri::forge::ModelSourceValidationReport blendValidation =
        ri::forge::ValidateModelSource(source / "models" / "crate.blend");
    if (blendValidation.valid || blendValidation.runtimeImportable) {
        fs::remove_all(root, error);
        return 5;
    }

    std::string createError = "stale error";
    const fs::path first = ri::forge::CreateUniqueHumanoidRig(root, &createError);
    if (!createError.empty()) {
        fs::remove_all(root, error);
        return 6;
    }
    const fs::path second = ri::forge::CreateUniqueHumanoidRig(root, &createError);
    if (first.empty() || second.empty() || first == second || !fs::exists(first) || !fs::exists(second)) {
        fs::remove_all(root, error);
        return 6;
    }

    const fs::path primitiveModel = ri::forge::CreateUniquePrimitiveModel(root, &createError);
    std::string groupId;
    std::string partId;
    if (primitiveModel.empty() || !createError.empty()
        || !ri::forge::AppendGroupToModel(
            primitiveModel, "Arm", "root", {}, &groupId, &createError)
        || !ri::forge::AppendPrimitiveToModel(
            primitiveModel, "capsule", groupId, &partId, &createError)
        || groupId.empty() || partId.empty() || !createError.empty()) {
        fs::remove_all(root, error);
        return 7;
    }
    if (ri::forge::AppendPrimitiveToModel(
            primitiveModel, "missing_preset", "root", nullptr, &createError)
        || createError.empty()) {
        fs::remove_all(root, error);
        return 7;
    }
    const ri::forge::PrimitiveModelBakeSummary bake =
        ri::forge::BakePrimitiveModelAsset(primitiveModel);
    if (!bake.valid || !fs::is_regular_file(bake.outputPath)
        || bake.inputPartCount != 2U || bake.outputTriangleCount == 0U) {
        fs::remove_all(root, error);
        return 7;
    }
    const ri::forge::ForgePreviewBuildResult sourcePreview =
        ri::forge::BuildForgePreviewScene(
            root / "Assets" / "Source" / "models" / "crate.obj",
            ri::forge::AssetKind::ModelSource);
    const ri::forge::ForgePreviewBuildResult primitivePreview =
        ri::forge::BuildForgePreviewScene(
            primitiveModel,
            ri::forge::AssetKind::PrimitiveModel);
    const ri::forge::ForgePreviewBuildResult oversizedPreview =
        ri::forge::BuildForgePreviewScene(
            root / "Assets" / "Source" / "models" / "crate.obj",
            ri::forge::AssetKind::ModelSource,
            0,
            1);
    const ri::forge::ForgePreviewBuildResult blendPreview =
        ri::forge::BuildForgePreviewScene(
            root / "Assets" / "Source" / "models" / "crate.blend",
            ri::forge::AssetKind::ModelSource);
    if (!sourcePreview.assetLoaded || sourcePreview.renderableNodeCount == 0U
        || sourcePreview.camera.cameraNode == ri::scene::kInvalidHandle
        || !primitivePreview.assetLoaded || primitivePreview.renderableNodeCount == 0U
        || primitivePreview.camera.cameraNode == ri::scene::kInvalidHandle
        || oversizedPreview.assetLoaded
        || oversizedPreview.status.find("size limit") == std::string::npos
        || blendPreview.assetLoaded
        || blendPreview.status.find("requires export") == std::string::npos) {
        fs::remove_all(root, error);
        return 8;
    }

    const ri::forge::AssetCatalog finalCatalog = ri::forge::ScanAssetCatalog(root);
    if (finalCatalog.modelCount != 3U || finalCatalog.primitiveModelCount != 1U
        || finalCatalog.rigCount != 4U || finalCatalog.invalidRigCount != 1U
        || finalCatalog.invalidPrimitiveModelCount != 0U) {
        fs::remove_all(root, error);
        return 9;
    }
    const std::vector<std::size_t> primitiveMatches =
        ri::forge::FilterAssetCatalogIndices(finalCatalog, "PRIMITIVE FORGE");
    const std::vector<std::size_t> rockMatches =
        ri::forge::FilterAssetCatalogIndices(finalCatalog, "crate.OBJ");
    const std::vector<std::size_t> noMatches =
        ri::forge::FilterAssetCatalogIndices(finalCatalog, "definitely-not-an-asset");
    if (primitiveMatches.size() != 1U || rockMatches.size() != 1U || !noMatches.empty()) {
        fs::remove_all(root, error);
        return 10;
    }

    {
        ri::forge::AsyncAssetCatalogIndex backgroundIndex(root);
        const std::uint64_t supersededGeneration =
            backgroundIndex.Request(root / "Assets" / "Source" / "models" / "crate.obj");
        const std::uint64_t generation = backgroundIndex.Request(primitiveModel);
        if (generation <= supersededGeneration || !backgroundIndex.Busy()) {
            fs::remove_all(root, error);
            return 11;
        }
        std::optional<ri::forge::CatalogIndexResult> indexed{};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline && !indexed.has_value()) {
            indexed = backgroundIndex.Poll();
            if (!indexed.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        if (!indexed.has_value() || indexed->generation != generation
            || indexed->preferredSelection != primitiveModel
            || indexed->catalog.primitiveModelCount != 1U
            || indexed->elapsedMilliseconds < 0.0
            || !indexed->error.empty()
            || backgroundIndex.Busy()) {
            fs::remove_all(root, error);
            return 11;
        }
    }
    {
        ri::forge::AsyncForgePreviewBuilder backgroundPreview;
        const std::uint64_t supersededGeneration = backgroundPreview.Request(
            root / "Assets" / "Source" / "models" / "crate.obj",
            ri::forge::AssetKind::ModelSource);
        const std::uint64_t generation = backgroundPreview.Request(
            primitiveModel,
            ri::forge::AssetKind::PrimitiveModel);
        if (generation <= supersededGeneration || !backgroundPreview.Busy()) {
            fs::remove_all(root, error);
            return 12;
        }
        std::optional<ri::forge::ForgePreviewBuildResult> preview{};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline && !preview.has_value()) {
            preview = backgroundPreview.Poll();
            if (!preview.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        if (!preview.has_value() || preview->generation != generation
            || preview->assetPath != primitiveModel || !preview->assetLoaded
            || preview->renderableNodeCount == 0U || backgroundPreview.Busy()) {
            fs::remove_all(root, error);
            return 12;
        }
    }

    fs::remove_all(root, error);
    if (error) {
        return 13;
    }
    std::cout << "Raw Iron Forge catalog smoke passed.\n";
    return 0;
}
