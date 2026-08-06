#include "RawIron/Content/PluginProjectData.h"

#include <array>
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
    const fs::path workspaceRoot(argv[1]);
    const std::array<const char*, 4> gameDirectories{
        "EditorUiSmoke",
        "LiminalHall",
        "RawIronMultiplayerSandbox",
        "WildernessRuins",
    };

    for (const char* directory : gameDirectories) {
        const fs::path gameRoot = workspaceRoot / "Games" / directory;
        const ri::content::PluginProjectData data = ri::content::LoadPluginProjectData(gameRoot);
        if (!data.issues.empty()) {
            std::cerr << directory << ": plugin project data invalid: " << data.issues.front().message << '\n';
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
