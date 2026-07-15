#include "RawIron/Render/ScenePreviewRenderingScript.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    using ri::render::software::DidGamePreviewScriptsChange;
    using ri::render::software::GamePreviewScriptTimestamps;
    using ri::render::software::SnapshotGamePreviewScriptTimestamps;

    const fs::path testRoot = fs::temp_directory_path()
        / ("rawiron_preview_script_watcher_"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path scripts = testRoot / "scripts";
    const fs::path rendering = scripts / "rendering.riscript";
    const fs::path postprocess = scripts / "postprocess.riscript";
    std::error_code ec{};
    fs::create_directories(scripts, ec);
    if (ec) {
        return EXIT_FAILURE;
    }

    const auto cleanup = [&]() { fs::remove_all(testRoot, ec); };
    GamePreviewScriptTimestamps timestamps{};
    SnapshotGamePreviewScriptTimestamps(testRoot, timestamps);
    if (DidGamePreviewScriptsChange(testRoot, timestamps)) {
        cleanup();
        return EXIT_FAILURE;
    }

    {
        std::ofstream output(rendering);
        output << "fog_strength = 0.5\n";
    }
    if (!DidGamePreviewScriptsChange(testRoot, timestamps)
        || DidGamePreviewScriptsChange(testRoot, timestamps)) {
        cleanup();
        return EXIT_FAILURE;
    }

    const fs::file_time_type originalWriteTime = fs::last_write_time(rendering, ec);
    if (ec) {
        cleanup();
        return EXIT_FAILURE;
    }
    fs::last_write_time(rendering, originalWriteTime + std::chrono::seconds(2), ec);
    if (ec || !DidGamePreviewScriptsChange(testRoot, timestamps)) {
        cleanup();
        return EXIT_FAILURE;
    }

    {
        std::ofstream output(postprocess);
        output << "native_exposure = 1.1\n";
    }
    if (!DidGamePreviewScriptsChange(testRoot, timestamps)) {
        cleanup();
        return EXIT_FAILURE;
    }

    fs::remove(rendering, ec);
    if (ec || !DidGamePreviewScriptsChange(testRoot, timestamps)
        || DidGamePreviewScriptsChange(testRoot, timestamps)) {
        cleanup();
        return EXIT_FAILURE;
    }

    cleanup();
    return EXIT_SUCCESS;
}
