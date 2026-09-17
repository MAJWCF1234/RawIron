#include "RawIron/Content/PrimitiveModelDocument.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    const auto require = [](const bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << "\n";
        }
        return condition;
    };
    ri::content::PrimitiveModelDocument document =
        ri::content::CreatePrimitiveModelDocument("test walker", "Test Walker");
    const std::string torso =
        ri::content::AddPrimitiveModelGroup(document, "Torso", "root", "spine");
    const std::string arm =
        ri::content::AddPrimitiveModelGroup(document, "Left Arm", torso, "upper_arm_l");
    const std::string body =
        ri::content::AddPrimitiveModelPart(document, "rounded_box", torso, "Body");
    const std::string shoulder =
        ri::content::AddPrimitiveModelPart(document, "uv_sphere", arm, "Shoulder");
    if (!require(
            !torso.empty() && !arm.empty() && !body.empty() && !shoulder.empty(),
            "Could not append initial groups and parts.")) {
        return EXIT_FAILURE;
    }

    document.parts[1].transform.translation.x = -0.75F;
    document.parts[1].transform.scale = {0.35F, 0.35F, 0.35F};
    document.parts[0].albedoColor = {0.82F, 0.28F, 0.16F};
    document.parts[0].roughness = 0.35F;
    document.parts[0].metallic = 0.2F;
    document.parts[0].albedoTexture = "iron_plate.png";
    const ri::content::PrimitiveModelValidationReport valid =
        ri::content::ValidatePrimitiveModelDocument(document);
    if (!require(
            valid.valid && valid.rootGroupCount == 1U && valid.enabledPartCount == 2U,
            "Initial document validation failed.")) {
        return EXIT_FAILURE;
    }

    const std::string json = ri::content::SerializePrimitiveModelDocument(document);
    const auto parsed = ri::content::ParsePrimitiveModelDocument(json);
    if (!require(
            parsed.has_value() && ri::content::ValidatePrimitiveModelDocument(*parsed).valid
                && parsed->groups.size() == 3U && parsed->parts.size() == 2U
                && parsed->parts[1].transform.translation.x == -0.75F
                && parsed->parts[0].albedoColor.x == 0.82F && parsed->parts[0].roughness == 0.35F
                && parsed->parts[0].metallic == 0.2F && parsed->parts[0].albedoTexture == "iron_plate.png",
            "Serialized document did not round trip.")) {
        return EXIT_FAILURE;
    }

    const std::string duplicatedPart =
        ri::content::DuplicatePrimitiveModelPart(document, shoulder);
    const std::string duplicatedGroup =
        ri::content::DuplicatePrimitiveModelGroup(document, arm);
    if (!require(
            !duplicatedPart.empty() && document.parts.size() == 3U
                && document.parts.back().primitivePreset == "uv_sphere"
                && document.parts.back().groupId == arm
                && std::abs(document.parts.back().transform.translation.x + 0.50F) < 0.0001F
                && !duplicatedGroup.empty() && document.groups.size() == 4U,
            "Could not duplicate part or group.")) {
        return EXIT_FAILURE;
    }
    if (!require(
            ri::content::RemovePrimitiveModelPart(document, duplicatedPart)
                && document.parts.size() == 2U
                && !ri::content::RemovePrimitiveModelGroup(document, torso)
                && ri::content::RemovePrimitiveModelGroup(document, duplicatedGroup)
                && document.groups.size() == 3U,
            "Could not delete duplicated part or empty copied group.")) {
        return EXIT_FAILURE;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path folder =
        fs::temp_directory_path() / ("RawIronPrimitiveModelDocument_" + std::to_string(stamp));
    const fs::path path = folder / "walker.ri_model.json";
    if (!require(
            ri::content::SavePrimitiveModelDocument(path, document),
            "Could not save primitive model document.")) {
        return EXIT_FAILURE;
    }
    const auto loaded = ri::content::LoadPrimitiveModelDocument(path);
    std::error_code error{};
    fs::remove_all(folder, error);
    if (!require(
            loaded.has_value() && loaded->modelId == "test_walker" && loaded->parts.size() == 2U,
            "Saved document did not load.")) {
        return EXIT_FAILURE;
    }

    ri::content::PrimitiveModelDocument cycle = document;
    cycle.groups[0].parentId = arm;
    if (!require(!ri::content::ValidatePrimitiveModelDocument(cycle).valid, "Cycle was accepted.")) {
        return EXIT_FAILURE;
    }
    ri::content::PrimitiveModelDocument zeroScale = document;
    zeroScale.groups[1].transform.scale.y = 0.0F;
    if (!require(
            !ri::content::ValidatePrimitiveModelDocument(zeroScale).valid,
            "Zero group scale was accepted.")) {
        return EXIT_FAILURE;
    }
    if (!require(
            ri::content::AddPrimitiveModelGroup(document, "Orphan", "missing").empty()
                && ri::content::AddPrimitiveModelPart(document, "box", "missing").empty(),
            "Missing group references were accepted.")) {
        return EXIT_FAILURE;
    }

    std::cout << "Primitive model document smoke passed.\n";
    return EXIT_SUCCESS;
}
