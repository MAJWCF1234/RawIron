#include "RawIron/Content/PluginProjectData.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

int main() {
    const std::array<fs::path, 4> gameRoots{
        fs::path("O:/RawIron/Games/EditorUiSmoke"),
        fs::path("O:/RawIron/Games/LiminalHall"),
        fs::path("O:/RawIron/Games/RawIronMultiplayerSandbox"),
        fs::path("O:/RawIron/Games/WildernessRuins"),
    };

    for (const fs::path& gameRoot : gameRoots) {
        const ri::content::PluginProjectData data = ri::content::LoadPluginProjectData(gameRoot);
        if (!data.issues.empty()) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
