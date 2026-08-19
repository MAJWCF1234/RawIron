#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    const fs::path root = fs::temp_directory_path() / "rawiron-model-import-rollback-smoke";
    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    fs::create_directories(root, cleanupError);

    // Missing / unreadable sources must not append nodes when placeholders are disabled.
    ri::scene::Scene scene{"ImportRollback"};
    const int keep = scene.CreateNode("Keep");
    const auto before = scene.CaptureAppendWatermark();

    std::string error;
    const int missing = ri::scene::AddModelNode(
        scene,
        ri::scene::ImportedModelOptions{
            .sourcePath = root / "does-not-exist.fbx",
            .nodeName = "MissingModel",
            .parent = keep,
            .createPlaceholderOnFailure = false,
        },
        &error);
    if (missing != ri::scene::kInvalidHandle
        || scene.NodeCount() != before.nodeCount
        || scene.MeshCount() != before.meshCount
        || scene.MaterialCount() != before.materialCount
        || !scene.GetNode(keep).children.empty()
        || error.empty()) {
        fs::remove_all(root, cleanupError);
        return EXIT_FAILURE;
    }

    // Truncated/corrupt FBX bytes must also fail closed without orphaning a wrapper.
    const fs::path corruptPath = root / "corrupt.fbx";
    {
        std::ofstream out(corruptPath, std::ios::binary);
        out << "not-a-real-fbx-file";
    }
    error.clear();
    const int corrupt = ri::scene::AddModelNode(
        scene,
        ri::scene::ImportedModelOptions{
            .sourcePath = corruptPath,
            .nodeName = "CorruptModel",
            .parent = keep,
            .createPlaceholderOnFailure = false,
        },
        &error);
    if (corrupt != ri::scene::kInvalidHandle
        || scene.NodeCount() != before.nodeCount
        || scene.MeshCount() != before.meshCount
        || scene.MaterialCount() != before.materialCount
        || !scene.GetNode(keep).children.empty()
        || error.empty()) {
        fs::remove_all(root, cleanupError);
        return EXIT_FAILURE;
    }

    fs::remove_all(root, cleanupError);
    return EXIT_SUCCESS;
}
