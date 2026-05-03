#include "RawIron/Ui/UiCommandLine.h"

#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Ui/UiJsonIO.h"
#include "RawIron/Ui/UiPaths.h"

#include <filesystem>
#include <string>

namespace ri::ui {

void MaybeValidateUiManifestFromCommandLine(const ri::core::CommandLine& commandLine) {
    if (!commandLine.HasFlag("--validate-ui-manifest")) {
        return;
    }
    namespace fs = std::filesystem;
    fs::path workspace = fs::current_path();
    if (const auto ws = commandLine.GetValue("--workspace"); ws.has_value() && !ws->empty()) {
        workspace = fs::weakly_canonical(fs::path(*ws));
    }
    fs::path manifest = DefaultUiManifestPath(workspace);
    if (const auto um = commandLine.GetValue("--ui-manifest"); um.has_value() && !um->empty()) {
        const fs::path userPath(*um);
        manifest = userPath.is_absolute() ? userPath : workspace / userPath;
    }
    UiManifest ui{};
    std::string err;
    if (TryLoadUiManifestFromJsonFile(manifest, ui, &err)) {
        ri::core::LogInfo("UI manifest OK: " + manifest.string() + " screens=" + std::to_string(ui.screens.size()));
    } else {
        ri::core::LogInfo("UI manifest validation failed (" + manifest.string() + "): " + err);
    }
}

} // namespace ri::ui
