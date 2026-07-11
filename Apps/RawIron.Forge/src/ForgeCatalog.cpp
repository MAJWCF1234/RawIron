#include "ForgeCatalog.h"

#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <system_error>

namespace ri::forge {
namespace {

namespace fs = std::filesystem;

std::string LowerAscii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string ModelSummary(const fs::path& path) {
    const std::string extension = LowerAscii(path.extension().string());
    if (extension == ".blend") {
        return "Blender authoring source. Export to glTF, GLB, or FBX before packaging for runtime use.";
    }
    if (extension == ".gltf" || extension == ".glb") {
        return "glTF model source supported by Raw Iron mesh and animation import.";
    }
    if (extension == ".fbx") {
        return "FBX model source supported by Raw Iron static, skinned-mesh, and animation import.";
    }
    return "OBJ model source supported by Raw Iron mesh import.";
}

AssetEntry InspectRig(const fs::path& absolutePath, std::string relativePath) {
    AssetEntry entry{
        .absolutePath = absolutePath,
        .relativePath = std::move(relativePath),
        .kind = AssetKind::Rig,
        .valid = false,
        .summary = "Rig could not be parsed.",
    };

    const std::optional<ri::scene::RigDefinition> rig = ri::scene::LoadRigDefinition(absolutePath);
    if (!rig.has_value()) {
        return entry;
    }

    const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(*rig);
    entry.valid = report.valid;
    entry.summary = rig->displayName + " | " + std::to_string(rig->bones.size()) + " bones | "
        + std::to_string(report.rootBoneCount) + " root";
    if (report.rootBoneCount != 1U) {
        entry.summary += "s";
    }
    entry.summary += report.valid ? " | valid" : " | invalid";
    if (rig->profile == ri::scene::RigProfile::Humanoid) {
        entry.summary += " | humanoid " + std::to_string(report.humanoidMatchedBoneCount) + "/"
            + std::to_string(report.humanoidRequiredBoneCount);
    }
    if (!report.errors.empty()) {
        entry.summary += " | " + report.errors.front();
    } else if (!report.warnings.empty()) {
        entry.summary += " | " + report.warnings.front();
    }
    return entry;
}

} // namespace

bool IsModelSourcePath(const fs::path& path) {
    const std::string extension = LowerAscii(path.extension().string());
    return extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb"
        || extension == ".blend";
}

bool IsRigPath(const fs::path& path) {
    return LowerAscii(path.filename().string()).ends_with(".ri_rig.json");
}

AssetCatalog ScanAssetCatalog(const fs::path& workspaceRoot) {
    AssetCatalog catalog{};
    std::error_code canonicalError{};
    catalog.workspaceRoot = fs::weakly_canonical(workspaceRoot, canonicalError);
    if (canonicalError) {
        catalog.workspaceRoot = workspaceRoot;
    }
    catalog.sourceRoot = catalog.workspaceRoot / "Assets" / "Source";

    std::error_code existsError{};
    if (!fs::is_directory(catalog.sourceRoot, existsError)) {
        return catalog;
    }

    std::error_code iteratorError{};
    fs::recursive_directory_iterator iterator(
        catalog.sourceRoot,
        fs::directory_options::skip_permission_denied,
        iteratorError);
    const fs::recursive_directory_iterator end{};
    while (!iteratorError && iterator != end) {
        const fs::directory_entry& directoryEntry = *iterator;
        std::error_code typeError{};
        if (directoryEntry.is_regular_file(typeError)) {
            const fs::path absolutePath = directoryEntry.path();
            std::error_code relativeError{};
            fs::path relativePath = fs::relative(absolutePath, catalog.sourceRoot, relativeError);
            if (relativeError) {
                relativePath = absolutePath.filename();
            }

            if (IsRigPath(absolutePath)) {
                AssetEntry entry = InspectRig(absolutePath, relativePath.generic_string());
                ++catalog.rigCount;
                if (!entry.valid) {
                    ++catalog.invalidRigCount;
                }
                catalog.entries.push_back(std::move(entry));
            } else if (IsModelSourcePath(absolutePath)) {
                catalog.entries.push_back(AssetEntry{
                    .absolutePath = absolutePath,
                    .relativePath = relativePath.generic_string(),
                    .kind = AssetKind::ModelSource,
                    .valid = true,
                    .summary = ModelSummary(absolutePath),
                });
                ++catalog.modelCount;
            }
        }
        iterator.increment(iteratorError);
    }

    std::sort(catalog.entries.begin(), catalog.entries.end(), [](const AssetEntry& left, const AssetEntry& right) {
        if (left.kind != right.kind) {
            return left.kind == AssetKind::Rig;
        }
        return LowerAscii(left.relativePath) < LowerAscii(right.relativePath);
    });
    return catalog;
}

ModelSourceValidationReport ValidateModelSource(const fs::path& sourcePath) {
    const ri::scene::ModelSourceValidationReport shared = ri::scene::ValidateModelSource(sourcePath);
    ModelSourceValidationReport report{};
    report.valid = shared.valid;
    report.runtimeImportable = shared.runtimeImportable;
    report.nodeCount = shared.nodeCount;
    report.meshCount = shared.meshCount;
    report.materialCount = shared.materialCount;
    report.summary = shared.summary;
    return report;
}

fs::path CreateUniqueHumanoidRig(const fs::path& workspaceRoot, std::string* errorMessage) {
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    const fs::path rigFolder = workspaceRoot / "Assets" / "Source" / "rigs";
    std::error_code folderError{};
    fs::create_directories(rigFolder, folderError);
    if (folderError) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not create the rig source folder: " + folderError.message();
        }
        return {};
    }

    fs::path output = rigFolder / "humanoid.ri_rig.json";
    std::string id = "humanoid";
    for (int suffix = 2; fs::exists(output) && suffix < 10000; ++suffix) {
        id = "humanoid_" + std::to_string(suffix);
        output = rigFolder / (id + ".ri_rig.json");
    }
    if (fs::exists(output)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not choose an unused humanoid rig filename.";
        }
        return {};
    }

    const ri::scene::RigDefinition rig = ri::scene::CreateHumanoidRigDefinition(id, "Humanoid Rig");
    const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(rig);
    if (!report.valid || !ri::scene::SaveRigDefinition(output, rig)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not save a valid humanoid rig definition.";
        }
        return {};
    }
    return output;
}

} // namespace ri::forge
