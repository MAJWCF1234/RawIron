#include "ForgeCatalog.h"

#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Scene/PrimitiveModelBake.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"

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

AssetEntry InspectPrimitiveModel(const fs::path& absolutePath, std::string relativePath) {
    AssetEntry entry{
        .absolutePath = absolutePath,
        .relativePath = std::move(relativePath),
        .kind = AssetKind::PrimitiveModel,
        .valid = false,
        .summary = "Primitive model could not be parsed.",
    };
    const auto model = ri::content::LoadPrimitiveModelDocument(absolutePath);
    if (!model.has_value()) {
        return entry;
    }
    const ri::content::PrimitiveModelValidationReport report =
        ri::content::ValidatePrimitiveModelDocument(*model);
    entry.valid = report.valid;
    entry.summary = model->displayName + " | " + std::to_string(model->groups.size()) + " groups | "
        + std::to_string(model->parts.size()) + " parts | "
        + std::to_string(report.enabledPartCount) + " enabled";
    if (!model->rigPath.empty()) {
        entry.summary += " | rig " + model->rigPath;
    }
    entry.summary += report.valid ? " | valid" : " | invalid";
    if (!report.errors.empty()) {
        entry.summary += " | " + report.errors.front();
    } else if (!report.warnings.empty()) {
        entry.summary += " | " + report.warnings.front();
    }
    for (const auto& part : model->parts) {
        if (!ri::scene::FindStructuralPreset(part.primitivePreset).has_value()) {
            entry.valid = false;
            entry.summary += " | unknown primitive " + part.primitivePreset;
            break;
        }
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

bool IsPrimitiveModelPath(const fs::path& path) {
    return LowerAscii(path.filename().string()).ends_with(".ri_model.json");
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

            if (IsPrimitiveModelPath(absolutePath)) {
                AssetEntry entry = InspectPrimitiveModel(absolutePath, relativePath.generic_string());
                ++catalog.primitiveModelCount;
                if (!entry.valid) {
                    ++catalog.invalidPrimitiveModelCount;
                }
                catalog.entries.push_back(std::move(entry));
            } else if (IsRigPath(absolutePath)) {
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
            return static_cast<int>(left.kind) > static_cast<int>(right.kind);
        }
        return LowerAscii(left.relativePath) < LowerAscii(right.relativePath);
    });
    return catalog;
}

std::vector<std::size_t> FilterAssetCatalogIndices(
    const AssetCatalog& catalog,
    const std::string_view query) {
    const std::string needle = LowerAscii(std::string(query));
    std::vector<std::size_t> indices{};
    indices.reserve(catalog.entries.size());
    for (std::size_t index = 0; index < catalog.entries.size(); ++index) {
        const AssetEntry& entry = catalog.entries[index];
        std::string kind = "model source";
        if (entry.kind == AssetKind::PrimitiveModel) {
            kind = "primitive forge model";
        } else if (entry.kind == AssetKind::Rig) {
            kind = "rig skeleton";
        }
        const std::string searchable =
            LowerAscii(entry.relativePath + "\n" + entry.summary + "\n" + kind);
        if (needle.empty() || searchable.find(needle) != std::string::npos) {
            indices.push_back(index);
        }
    }
    return indices;
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

fs::path CreateUniquePrimitiveModel(const fs::path& workspaceRoot, std::string* errorMessage) {
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    const fs::path modelFolder = workspaceRoot / "Assets" / "Source" / "models";
    std::error_code folderError{};
    fs::create_directories(modelFolder, folderError);
    if (folderError) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not create the model source folder: " + folderError.message();
        }
        return {};
    }
    fs::path output = modelFolder / "primitive_model.ri_model.json";
    std::string id = "primitive_model";
    for (int suffix = 2; fs::exists(output) && suffix < 10000; ++suffix) {
        id = "primitive_model_" + std::to_string(suffix);
        output = modelFolder / (id + ".ri_model.json");
    }
    if (fs::exists(output)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not choose an unused primitive model filename.";
        }
        return {};
    }
    ri::content::PrimitiveModelDocument document =
        ri::content::CreatePrimitiveModelDocument(id, "Primitive Model");
    (void)ri::content::AddPrimitiveModelPart(document, "rounded_box", "root", "Body");
    if (!ri::content::SavePrimitiveModelDocument(output, document)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not save a valid primitive model document.";
        }
        return {};
    }
    return output;
}

bool AppendPrimitiveToModel(const fs::path& modelPath,
                            const std::string_view primitivePreset,
                            const std::string_view groupId,
                            std::string* insertedPartId,
                            std::string* errorMessage) {
    if (insertedPartId != nullptr) {
        insertedPartId->clear();
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    if (!ri::scene::FindStructuralPreset(primitivePreset).has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unknown Raw Iron primitive preset: " + std::string(primitivePreset);
        }
        return false;
    }
    auto document = ri::content::LoadPrimitiveModelDocument(modelPath);
    if (!document.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not load primitive model: " + modelPath.string();
        }
        return false;
    }
    const std::string id = ri::content::AddPrimitiveModelPart(
        *document,
        std::string(primitivePreset),
        std::string(groupId),
        std::string(primitivePreset));
    if (id.empty() || !ri::content::SavePrimitiveModelDocument(modelPath, *document)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not append primitive to model.";
        }
        return false;
    }
    if (insertedPartId != nullptr) {
        *insertedPartId = id;
    }
    return true;
}

bool AppendGroupToModel(const fs::path& modelPath,
                        const std::string_view name,
                        const std::string_view parentId,
                        const std::string_view boneName,
                        std::string* insertedGroupId,
                        std::string* errorMessage) {
    if (insertedGroupId != nullptr) {
        insertedGroupId->clear();
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    auto document = ri::content::LoadPrimitiveModelDocument(modelPath);
    if (!document.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not load primitive model: " + modelPath.string();
        }
        return false;
    }
    const std::string id = ri::content::AddPrimitiveModelGroup(
        *document,
        std::string(name),
        std::string(parentId),
        std::string(boneName));
    if (id.empty() || !ri::content::SavePrimitiveModelDocument(modelPath, *document)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not append group to primitive model.";
        }
        return false;
    }
    if (insertedGroupId != nullptr) {
        *insertedGroupId = id;
    }
    return true;
}

PrimitiveModelBakeSummary BakePrimitiveModelAsset(const fs::path& modelPath,
                                                  const fs::path& requestedOutputPath) {
    PrimitiveModelBakeSummary summary{};
    const auto document = ri::content::LoadPrimitiveModelDocument(modelPath);
    if (!document.has_value()) {
        summary.summary = "Could not load primitive model: " + modelPath.string();
        return summary;
    }
    const ri::scene::PrimitiveModelBakeResult bake =
        ri::scene::BakePrimitiveModel(*document, modelPath.parent_path());
    summary.inputPartCount = bake.inputPartCount;
    summary.inputTriangleCount = bake.inputTriangleCount;
    summary.outputTriangleCount = bake.outputTriangleCount;
    summary.culledTriangleCount =
        bake.culledInternalTriangleCount + bake.culledDuplicateTriangleCount;
    summary.boneBoundVertexCount = static_cast<std::size_t>(std::count_if(
        bake.vertexBoneNames.begin(),
        bake.vertexBoneNames.end(),
        [](const std::string& bone) { return !bone.empty(); }));
    if (!bake.valid) {
        summary.summary = bake.errors.empty() ? "Primitive model bake produced no geometry."
                                              : bake.errors.front();
        return summary;
    }
    summary.outputPath = requestedOutputPath.empty()
        ? fs::path(modelPath.string().substr(0, modelPath.string().size() - std::string(".ri_model.json").size())
                   + ".baked.obj")
        : requestedOutputPath;
    summary.valid = ri::scene::SavePrimitiveModelBakeObj(summary.outputPath, bake);
    if (summary.valid && summary.boneBoundVertexCount > 0U) {
        summary.rigMapPath = summary.outputPath;
        summary.rigMapPath.replace_extension(".ri_skin.json");
        summary.valid = ri::scene::SavePrimitiveModelBakeRigMap(summary.rigMapPath, bake);
    }
    summary.summary = summary.valid
        ? "Baked " + std::to_string(summary.inputPartCount) + " parts: "
            + std::to_string(summary.inputTriangleCount) + " -> "
            + std::to_string(summary.outputTriangleCount) + " triangles, culled "
            + std::to_string(summary.culledTriangleCount) + ", rigid-bound "
            + std::to_string(summary.boneBoundVertexCount) + " vertices."
        : "Could not write primitive model bake: " + summary.outputPath.string();
    return summary;
}

} // namespace ri::forge
