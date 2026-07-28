#include "RawIron/Content/AuthoringHandoff.h"
#include "RawIron/Content/PrimitiveModelDocument.h"

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

    ri::content::PrimitiveModelDocument primitive =
        ri::content::CreatePrimitiveModelDocument("crate_native", "Native Crate");
    if (ri::content::AddPrimitiveModelPart(primitive, "rounded_box", "root", "Body").empty()) {
        fs::remove_all(root, error);
        return EXIT_FAILURE;
    }
    const fs::path primitivePath =
        root / "Assets" / "Source" / "models" / "crate_native.ri_model.json";
    if (!ri::content::SavePrimitiveModelDocument(primitivePath, primitive)) {
        fs::remove_all(root, error);
        return EXIT_FAILURE;
    }
    const ri::content::AuthoringHandoffReport primitiveHandoff =
        ri::content::BuildAuthoringHandoff({
            .workspaceRoot = root,
            .assetPath = primitivePath,
        });
    if (!primitiveHandoff.valid
        || primitiveHandoff.assetKind != ri::content::AuthoringAssetKind::PrimitiveModel
        || ri::content::ToString(primitiveHandoff.assetKind) != "primitive-model") {
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
