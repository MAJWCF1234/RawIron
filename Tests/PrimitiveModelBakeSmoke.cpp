#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Scene/PrimitiveModelBake.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/Scene.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main() {
    namespace fs = std::filesystem;
    const auto require = [](const bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << "\n";
        }
        return condition;
    };
    ri::content::PrimitiveModelDocument document =
        ri::content::CreatePrimitiveModelDocument("joined_boxes", "Joined Boxes");
    const std::string leftId =
        ri::content::AddPrimitiveModelPart(document, "box", "root", "Left");
    const std::string rightId =
        ri::content::AddPrimitiveModelPart(document, "box", "root", "Right");
    if (!require(!leftId.empty() && !rightId.empty(), "Could not append box parts.")) {
        return EXIT_FAILURE;
    }
    document.parts[0].transform.translation.x = -0.5F;
    document.parts[1].transform.translation.x = 0.5F;

    const ri::scene::PrimitiveModelBakeResult bake = ri::scene::BakePrimitiveModel(document);
    if (!require(
            bake.valid && bake.bakedPartCount == 2U && bake.inputTriangleCount == 24U
                && bake.culledInternalTriangleCount == 4U && bake.outputTriangleCount == 20U
                && bake.mesh.positions.size() == 60U,
            ("Unexpected culled bake counts: input=" + std::to_string(bake.inputTriangleCount)
             + " internal=" + std::to_string(bake.culledInternalTriangleCount)
             + " output=" + std::to_string(bake.outputTriangleCount)).c_str())) {
        return EXIT_FAILURE;
    }

    ri::content::PrimitiveModelDocument uncull = document;
    uncull.bake.cullInternalFaces = false;
    const ri::scene::PrimitiveModelBakeResult uncullBake =
        ri::scene::BakePrimitiveModel(uncull);
    if (!require(
            uncullBake.valid && uncullBake.outputTriangleCount == 24U,
            "Unculled bake triangle count was incorrect.")) {
        return EXIT_FAILURE;
    }

    ri::scene::Scene scene{"ForgeInterop"};
    const int editorRoot = scene.CreateNode("EditorRoot");
    const ri::scene::PrimitiveModelInstantiationResult instantiated =
        ri::scene::InstantiatePrimitiveModel(scene, editorRoot, document);
    if (!require(
            instantiated.valid && instantiated.rootNode != ri::scene::kInvalidHandle
                && instantiated.groupNodes.size() == 1U && instantiated.partNodes.size() == 2U
                && scene.GetNode(instantiated.rootNode).parent == editorRoot
                && scene.GetNode(instantiated.partNodes[0]).parent == instantiated.groupNodes[0],
            "Editor hierarchy instantiation was incorrect.")) {
        return EXIT_FAILURE;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path folder =
        fs::temp_directory_path() / ("RawIronPrimitiveModelBake_" + std::to_string(stamp));
    const fs::path output = folder / "joined_boxes.obj";
    if (!require(
            ri::scene::SavePrimitiveModelBakeObj(output, bake) && fs::is_regular_file(output)
                && fs::file_size(output) > 0U,
            "OBJ bake output was not written.")) {
        return EXIT_FAILURE;
    }

    const ri::scene::RigDefinition rig{
        .id = "rigid_test",
        .displayName = "Rigid Test",
        .profile = ri::scene::RigProfile::Generic,
        .bones = {
            ri::scene::RigBone{
                .name = "body",
                .parentIndex = -1,
            },
        },
    };
    const fs::path rigPath = folder / "rigid_test.ri_rig.json";
    if (!require(ri::scene::SaveRigDefinition(rigPath, rig), "Could not save rig fixture.")) {
        return EXIT_FAILURE;
    }
    ri::content::PrimitiveModelDocument rigged = document;
    rigged.rigPath = rigPath.filename().generic_string();
    rigged.groups[0].boneName = "body";
    const ri::scene::PrimitiveModelBakeResult riggedBake =
        ri::scene::BakePrimitiveModel(rigged, folder);
    if (!require(
            riggedBake.valid
                && riggedBake.vertexBoneNames.size() == riggedBake.mesh.positions.size()
                && std::all_of(
                    riggedBake.vertexBoneNames.begin(),
                    riggedBake.vertexBoneNames.end(),
                    [](const std::string& bone) { return bone == "body"; }),
            "Inherited rigid bone bindings were not preserved in the bake.")) {
        return EXIT_FAILURE;
    }
    const fs::path rigMapPath = folder / "joined_boxes.baked.ri_skin.json";
    if (!require(
            ri::scene::SavePrimitiveModelBakeRigMap(rigMapPath, riggedBake),
            "Could not save rigid bone map.")) {
        return EXIT_FAILURE;
    }
    std::ifstream rigMapInput(rigMapPath);
    const std::string rigMapText{
        std::istreambuf_iterator<char>(rigMapInput),
        std::istreambuf_iterator<char>()};
    if (!require(
            rigMapText.find("\"bone\": \"body\"") != std::string::npos
                && rigMapText.find("\"vertexCount\": 60") != std::string::npos,
            "Rigid bone map did not contain the expected binding range.")) {
        return EXIT_FAILURE;
    }
    ri::scene::Scene riggedScene{"RiggedForgeInterop"};
    const ri::scene::PrimitiveModelInstantiationResult riggedInstantiation =
        ri::scene::InstantiatePrimitiveModel(
            riggedScene,
            ri::scene::kInvalidHandle,
            rigged,
            folder);
    if (!require(
            riggedInstantiation.valid && !riggedInstantiation.partNodes.empty()
                && riggedScene.GetNode(riggedInstantiation.partNodes[0])
                       .structuralBrush.informationLayer.gameplayMeaning
                    == "bone:body",
            "Editor instantiation lost inherited rigid bone binding.")) {
        return EXIT_FAILURE;
    }
    std::error_code error{};
    fs::remove_all(folder, error);

    ri::content::PrimitiveModelDocument broken = document;
    broken.parts[0].primitivePreset = "does_not_exist";
    const ri::scene::PrimitiveModelBakeResult rejected =
        ri::scene::BakePrimitiveModel(broken);
    if (!require(!rejected.valid && !rejected.errors.empty(), "Unknown preset was accepted.")) {
        return EXIT_FAILURE;
    }

    std::cout << "Primitive model bake and Editor interop smoke passed.\n";
    return EXIT_SUCCESS;
}
