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
        std::ofstream(source / "models" / "crate.fbx") << "fixture";
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

    std::string createError;
    const fs::path first = ri::forge::CreateUniqueHumanoidRig(root, &createError);
    const fs::path second = ri::forge::CreateUniqueHumanoidRig(root, &createError);
    if (first.empty() || second.empty() || first == second || !fs::exists(first) || !fs::exists(second)) {
        fs::remove_all(root, error);
        return 4;
    }

    const ri::forge::AssetCatalog finalCatalog = ri::forge::ScanAssetCatalog(root);
    if (finalCatalog.modelCount != 2U || finalCatalog.rigCount != 4U || finalCatalog.invalidRigCount != 1U) {
        fs::remove_all(root, error);
        return 5;
    }

    fs::remove_all(root, error);
    if (error) {
        return 6;
    }
    std::cout << "Raw Iron Forge catalog smoke passed.\n";
    return 0;
}
