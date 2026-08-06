#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Expected workspace root argument.\n";
        return EXIT_FAILURE;
    }
    const fs::path workspace(argv[1]);

    // Prefer the committed CI fixture; fall back to the game PsxPack when present.
    const fs::path fixtureFbx = workspace / "Tests" / "fixtures" / "FbxTextureRemap" / "Abandoned_House"
        / "Models" / "Abandoned_House.fbx";
    const fs::path packFbx = workspace / "Games" / "WildernessRuins" / "assets" / "PsxPack" / "World"
        / "Abandoned_House" / "Models" / "Abandoned_House.fbx";
    const fs::path fbx = fs::is_regular_file(fixtureFbx) ? fixtureFbx : packFbx;
    const fs::path texturesDir = fbx.parent_path().parent_path() / "Textures";
    const fs::path packRoot = fbx.parent_path().parent_path(); // Abandoned_House/

    if (!fs::is_regular_file(fbx) || !fs::is_directory(texturesDir)) {
        std::cerr << "Missing Abandoned_House fixture under Tests/fixtures/FbxTextureRemap or "
                     "Games/WildernessRuins/assets/PsxPack/World/Abandoned_House\n";
        return EXIT_FAILURE;
    }

    ri::scene::Scene scene{"FbxTextureRemapSmoke"};
    std::string error;
    const int root = ri::scene::AddModelNode(scene,
                                             ri::scene::ImportedModelOptions{
                                                 .sourcePath = fbx,
                                                 .nodeName = "AbandonedHouse",
                                                 .createPlaceholderOnFailure = false,
                                             },
                                             &error);
    if (root == ri::scene::kInvalidHandle) {
        std::cerr << "FBX import failed: " << error << "\n";
        return EXIT_FAILURE;
    }

    std::error_code packEc{};
    const fs::path canonicalPackRoot = fs::weakly_canonical(packRoot, packEc);
    if (packEc || canonicalPackRoot.empty()) {
        std::cerr << "Could not canonicalize Abandoned_House pack root.\n";
        return EXIT_FAILURE;
    }

    std::size_t texturedMaterials = 0;
    std::size_t normalMappedMaterials = 0;
    std::size_t missingPaths = 0;
    std::size_t outsidePackTextures = 0;
    const auto textureInsidePack = [&](const std::string& texturePath) -> bool {
        if (texturePath.empty()) {
            return true;
        }
        std::error_code ec{};
        const fs::path absolute = fs::weakly_canonical(fs::path(texturePath), ec);
        if (ec) {
            return false;
        }
        auto packIt = canonicalPackRoot.begin();
        auto pathIt = absolute.begin();
        for (; packIt != canonicalPackRoot.end(); ++packIt, ++pathIt) {
            if (pathIt == absolute.end() || *packIt != *pathIt) {
                return false;
            }
        }
        return true;
    };

    for (std::size_t i = 0; i < scene.MaterialCount(); ++i) {
        const ri::scene::Material& material = scene.GetMaterial(static_cast<int>(i));
        const auto checkPath = [&](const std::string& texturePath) {
            if (texturePath.empty()) {
                return;
            }
            std::error_code ec{};
            if (!fs::exists(texturePath, ec) || ec) {
                ++missingPaths;
                std::cerr << "Resolved texture missing on disk: " << texturePath << "\n";
            }
            if (!textureInsidePack(texturePath)) {
                ++outsidePackTextures;
                std::cerr << "Resolved texture escaped Abandoned_House pack: " << texturePath << "\n";
            }
        };
        if (!material.baseColorTexture.empty()) {
            ++texturedMaterials;
            checkPath(material.baseColorTexture);
        }
        if (!material.normalTexture.empty()) {
            ++normalMappedMaterials;
            checkPath(material.normalTexture);
        }
    }

    if (texturedMaterials < 3U) {
        std::cerr << "Expected remapped albedo textures on Abandoned_House materials, got "
                  << texturedMaterials << " textured materials (total materials=" << scene.MaterialCount()
                  << ").\n";
        return EXIT_FAILURE;
    }
    // Normals are optional on this pack; when present they must resolve inside the pack.
    if (missingPaths != 0U) {
        std::cerr << "Resolved " << missingPaths << " texture path(s) that do not exist.\n";
        return EXIT_FAILURE;
    }
    if (outsidePackTextures != 0U) {
        std::cerr << "Resolved " << outsidePackTextures
                  << " texture path(s) outside the Abandoned_House pack (remap incomplete).\n";
        return EXIT_FAILURE;
    }

    std::cout << "FbxTextureRemapSmoke ok: source=" << fbx.generic_string()
              << " texturedMaterials=" << texturedMaterials
              << " normalMappedMaterials=" << normalMappedMaterials
              << " totalMaterials=" << scene.MaterialCount() << "\n";
    return EXIT_SUCCESS;
}
