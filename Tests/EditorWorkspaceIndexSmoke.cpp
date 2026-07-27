#include "Apps/RawIron.Editor/src/EditorWorkspace.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

int main() {
    namespace fs = std::filesystem;
    using namespace std::chrono_literals;

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("RawIronWorkspaceIndex-" + std::to_string(unique));
    std::error_code ec;
    fs::create_directories(root / "levels", ec);
    fs::create_directories(root / "assets" / "textures", ec);
    fs::create_directories(root / "ui", ec);
    if (ec) {
        return EXIT_FAILURE;
    }

    std::ofstream(root / "manifest.json") << R"({"id":"index-smoke","name":"Index Smoke"})";
    std::ofstream(root / "levels" / "main.scene") << "scene";
    std::ofstream(root / "assets" / "textures" / "albedo.png") << "texture";
    std::ofstream(root / "ui" / "main.json") << "{}";

    ri::editor::WorkspaceResourceIndex index;
    index.Request(root);
    // A second request while the first scan is active must supersede its result
    // without starting an overlapping recursive walk.
    index.Request(root);

    std::optional<std::vector<ri::editor::WorkspaceResourceEntry>> result;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline && !result.has_value()) {
        result = index.Poll();
        std::this_thread::sleep_for(1ms);
    }

    bool foundManifest = false;
    bool foundLevel = false;
    bool foundTexture = false;
    bool foundUi = false;
    if (result.has_value()) {
        for (const ri::editor::WorkspaceResourceEntry& entry : *result) {
            foundManifest = foundManifest || entry.relativePathUtf8 == "manifest.json";
            foundLevel = foundLevel || entry.relativePathUtf8 == "levels/main.scene";
            foundTexture = foundTexture || entry.relativePathUtf8 == "assets/textures/albedo.png";
            foundUi = foundUi || entry.relativePathUtf8 == "ui/main.json";
        }
    }

    fs::remove_all(root, ec);
    return result.has_value() && !index.Busy() && foundManifest && foundLevel && foundTexture && foundUi
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
