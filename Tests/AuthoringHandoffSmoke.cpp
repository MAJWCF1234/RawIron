#include "RawIron/Content/AuthoringHandoff.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    const fs::path root = fs::temp_directory_path() / "rawiron_authoring_handoff_smoke";
    std::error_code error{};
    fs::remove_all(root, error);
    fs::create_directories(root / "Assets" / "Source" / "models", error);
    std::ofstream(root / "Assets" / "Source" / "models" / "crate.obj") << "o crate\n";

    const ri::content::AuthoringHandoffReport valid = ri::content::BuildAuthoringHandoff({
        .workspaceRoot = root,
        .assetPath = fs::path("Assets") / "Source" / "models" / "crate.obj",
        .gameId = "sample-game",
    });
    if (!valid.valid || valid.assetKind != ri::content::AuthoringAssetKind::ModelSource
        || valid.workspaceRelativePath.generic_string() != "Assets/Source/models/crate.obj"
        || valid.editorArguments.size() != 7U || valid.editorArguments[3] != "--open-asset") {
        fs::remove_all(root, error);
        return EXIT_FAILURE;
    }

    const fs::path outside = root.parent_path() / "rawiron_handoff_outside.obj";
    std::ofstream(outside) << "o outside\n";
    const ri::content::AuthoringHandoffReport rejected = ri::content::BuildAuthoringHandoff({
        .workspaceRoot = root,
        .assetPath = outside,
    });
    fs::remove(outside, error);
    fs::remove_all(root, error);
    return rejected.valid ? EXIT_FAILURE : EXIT_SUCCESS;
}
