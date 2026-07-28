#include "RawIron/Content/AuthoringHandoff.h"

#include "RawIron/Content/PrimitiveModelDocument.h"

#include <algorithm>
#include <cctype>

namespace ri::content {
namespace {

namespace fs = std::filesystem;

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

AuthoringAssetKind ClassifyAsset(const fs::path& path) {
    const std::string filename = LowerAscii(path.filename().string());
    if (filename.ends_with(".ri_rig.json")) {
        return AuthoringAssetKind::Rig;
    }
    if (filename.ends_with(".ri_model.json")) {
        return AuthoringAssetKind::PrimitiveModel;
    }
    const std::string extension = LowerAscii(path.extension().string());
    if (extension == ".obj" || extension == ".gltf" || extension == ".glb"
        || extension == ".fbx" || extension == ".blend") {
        return AuthoringAssetKind::ModelSource;
    }
    return AuthoringAssetKind::Unknown;
}

bool EscapesRoot(const fs::path& relative) {
    if (relative.empty() || relative.is_absolute()) {
        return true;
    }
    const auto first = relative.begin();
    return first != relative.end() && *first == "..";
}

} // namespace

std::string_view ToString(const AuthoringAssetKind kind) noexcept {
    switch (kind) {
        case AuthoringAssetKind::ModelSource:
            return "model-source";
        case AuthoringAssetKind::PrimitiveModel:
            return "primitive-model";
        case AuthoringAssetKind::Rig:
            return "rig";
        case AuthoringAssetKind::Unknown:
            return "unknown";
    }
    return "unknown";
}

AuthoringHandoffReport BuildAuthoringHandoff(const AuthoringHandoffRequest& request) {
    AuthoringHandoffReport report{};
    std::error_code error{};
    if (request.workspaceRoot.empty()) {
        report.issues.push_back("Workspace root is required.");
        return report;
    }
    report.workspaceRoot = fs::weakly_canonical(request.workspaceRoot, error);
    if (error || !fs::is_directory(report.workspaceRoot, error)) {
        report.issues.push_back("Workspace root does not exist or is not a directory.");
        return report;
    }

    fs::path assetCandidate = request.assetPath;
    if (assetCandidate.is_relative()) {
        assetCandidate = report.workspaceRoot / assetCandidate;
    }
    report.assetPath = fs::weakly_canonical(assetCandidate, error);
    if (error || !fs::is_regular_file(report.assetPath, error)) {
        report.issues.push_back("Handoff asset does not exist or is not a file.");
        return report;
    }
    report.workspaceRelativePath = fs::relative(report.assetPath, report.workspaceRoot, error);
    if (error || EscapesRoot(report.workspaceRelativePath)) {
        report.issues.push_back("Handoff asset must stay inside the Raw Iron workspace.");
        return report;
    }

    report.assetKind = ClassifyAsset(report.assetPath);
    if (report.assetKind == AuthoringAssetKind::Unknown) {
        report.issues.push_back("Handoff asset is not a supported model source, primitive model, or Raw Iron rig.");
        return report;
    }
    if (report.assetKind == AuthoringAssetKind::PrimitiveModel) {
        const auto model = LoadPrimitiveModelDocument(report.assetPath);
        if (!model.has_value()) {
            report.issues.push_back("Primitive model handoff document could not be parsed.");
            return report;
        }
        const PrimitiveModelValidationReport validation = ValidatePrimitiveModelDocument(*model);
        if (!validation.valid) {
            report.issues.push_back(
                validation.errors.empty() ? "Primitive model handoff validation failed."
                                          : validation.errors.front());
            return report;
        }
    }
    report.editorArguments = {
        "--editor-ui",
        "--workspace-root",
        report.workspaceRoot.string(),
        "--open-asset",
        report.assetPath.string(),
    };
    if (!request.gameId.empty()) {
        report.editorArguments.push_back("--game");
        report.editorArguments.push_back(request.gameId);
    }
    report.valid = true;
    return report;
}

} // namespace ri::content
