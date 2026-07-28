#include "ForgeCatalog.h"

#include "RawIron/Scene/RigAuthoring.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

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

    const ri::forge::AssetCatalog finalCatalog = ri::forge::ScanAssetCatalog(root);
    if (finalCatalog.modelCount != 3U || finalCatalog.primitiveModelCount != 1U
        || finalCatalog.rigCount != 4U || finalCatalog.invalidRigCount != 1U
        || finalCatalog.invalidPrimitiveModelCount != 0U) {
        fs::remove_all(root, error);
        return 8;
    }

    fs::remove_all(root, error);
    if (error) {
        return 9;
    }
    std::cout << "Raw Iron Forge catalog smoke passed.\n";
    return 0;
}
