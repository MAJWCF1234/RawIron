#include "RawIron/Content/NativeSculptDocument.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

int main() {
    namespace fs = std::filesystem;
    ri::content::NativeSculptDocument document{};
    document.id = "clay_wedge";
    document.displayName = "Clay Wedge";
    document.cage = "sphere";
    document.mesh.name = "Clay Wedge";
    document.mesh.primitive = ri::scene::PrimitiveType::Custom;
    document.mesh.positions = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    };
    document.mesh.normals = {
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
    };
    document.mesh.texCoords = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
    document.mesh.indices = {0, 1, 2};
    document.mesh.vertexCount = 3;
    document.mesh.indexCount = 3;
    const ri::content::NativeSculptValidationReport valid =
        ri::content::ValidateNativeSculptDocument(document);
    if (!valid.valid || valid.vertexCount != 3U || valid.triangleCount != 1U) {
        std::cerr << "Valid sculpt fixture was rejected.\n";
        return EXIT_FAILURE;
    }

    const std::string json = ri::content::SerializeNativeSculptDocument(document);
    const auto parsed = ri::content::ParseNativeSculptDocument(json);
    if (!parsed.has_value() || parsed->mesh.positions.size() != 3U || parsed->mesh.indices[2] != 2
        || parsed->mesh.positions[1].x != 1.0f) {
        std::cerr << "Sculpt document did not round-trip.\n";
        return EXIT_FAILURE;
    }

    document.rigPath = "rigs/humanoid.ri_rig.json";
    document.vertexBoneNames = {"pelvis", "spine", "chest"};
    document.vertexInfluences = {
        {{.boneName = "pelvis", .weight = 0.6f}, {.boneName = "spine", .weight = 0.4f}},
        {{.boneName = "spine", .weight = 1.0f}},
        {{.boneName = "chest", .weight = 1.0f}},
    };
    const std::string boundJson = ri::content::SerializeNativeSculptDocument(document);
    const auto boundParsed = ri::content::ParseNativeSculptDocument(boundJson);
    if (!boundParsed.has_value()
        || boundParsed->rigPath != document.rigPath
        || boundParsed->vertexBoneNames != document.vertexBoneNames
        || boundParsed->vertexInfluences.size() != 3U
        || boundParsed->vertexInfluences.front().size() != 2U
        || boundParsed->vertexInfluences.front()[1].boneName != "spine"
        || std::abs(boundParsed->vertexInfluences.front()[0].weight - 0.6f) > 0.0001f
        || !ri::content::ValidateNativeSculptDocument(*boundParsed).valid) {
        std::cerr << "Bound sculpt document did not round-trip.\n";
        return EXIT_FAILURE;
    }
    document.vertexBoneNames.pop_back();
    if (ri::content::ValidateNativeSculptDocument(document).valid) {
        std::cerr << "Mismatched sculpt bone names were accepted.\n";
        return EXIT_FAILURE;
    }
    document.vertexBoneNames = {"pelvis", "spine", "chest"};
    document.vertexInfluences.pop_back();
    if (ri::content::ValidateNativeSculptDocument(document).valid) {
        std::cerr << "Mismatched sculpt influences were accepted.\n";
        return EXIT_FAILURE;
    }
    document.vertexInfluences.clear();

    document.vertexBoneNames = {"pelvis", "", "chest"};
    const std::string clearedJson = ri::content::SerializeNativeSculptDocument(document);
    const auto clearedParsed = ri::content::ParseNativeSculptDocument(clearedJson);
    if (!clearedParsed.has_value()
        || clearedParsed->vertexBoneNames != document.vertexBoneNames
        || !clearedParsed->vertexBoneNames[1].empty()) {
        std::cerr << "Cleared sculpt bone names did not round-trip.\n";
        return EXIT_FAILURE;
    }

    document.vertexBoneNames = {"pelvis", "spine", "chest"};
    document.vertexInfluences = {
        {{.boneName = "pelvis", .weight = 0.6f}, {.boneName = "spine", .weight = 0.4f}},
        {{.boneName = "spine", .weight = 1.0f}},
        {{.boneName = "chest", .weight = 1.0f}},
    };
    const fs::path temp = fs::temp_directory_path() / "rawiron_sculpt_weight_smoke.ri_sculpt.json";
    if (!ri::content::SaveNativeSculptDocument(temp, document)) {
        std::cerr << "Could not save blended sculpt weights to disk.\n";
        fs::remove(temp);
        return EXIT_FAILURE;
    }
    const auto diskLoaded = ri::content::LoadNativeSculptDocument(temp);
    fs::remove(temp);
    if (!diskLoaded.has_value() || diskLoaded->vertexInfluences.size() != 3U
        || diskLoaded->vertexInfluences.front().size() != 2U
        || diskLoaded->vertexInfluences.front()[1].boneName != "spine") {
        std::cerr << "Disk load dropped blended sculpt weights.\n";
        return EXIT_FAILURE;
    }

    ri::content::NativeSculptDocument empty{};
    empty.id = "broken";
    if (ri::content::ValidateNativeSculptDocument(empty).valid
        || ri::content::ParseNativeSculptDocument("{\"id\":\"no-mesh\"}").has_value()) {
        std::cerr << "Empty sculpt was accepted.\n";
        return EXIT_FAILURE;
    }

    ri::content::NativeSculptDocument boundWarning{};
    boundWarning.id = "warn";
    boundWarning.displayName = "Warn";
    boundWarning.cage = "sphere";
    boundWarning.rigPath = "rigs/humanoid.ri_rig.json";
    boundWarning.mesh.name = "Warn";
    boundWarning.mesh.positions = {{0.0f, 0.0f, 0.0f}, {0.1f, 0.0f, 0.0f}, {0.0f, 0.1f, 0.0f}};
    boundWarning.mesh.normals = boundWarning.mesh.positions;
    boundWarning.mesh.indices = {0, 1, 2};
    boundWarning.mesh.primitive = ri::scene::PrimitiveType::Custom;
    const auto warnReport = ri::content::ValidateNativeSculptDocument(boundWarning);
    if (!warnReport.valid || warnReport.unboundVertexCount == 0U || warnReport.warnings.empty()) {
        std::cerr << "Bound sculpt without weights did not warn about unbound verts.\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
