#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "RawIron/Scene/PrimitivesCsvIO.h"
#include "RawIron/Scene/Scene.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct ExperienceExpectation {
    const char* directory;
    const char* firstNode;
    int expectedPrimitiveCount;
    ri::math::Vec3 position;
    ri::math::Vec3 scale;
};

bool Near(const float lhs, const float rhs) {
    return std::abs(lhs - rhs) <= 0.001f;
}

bool Matches(const ri::math::Vec3& lhs, const ri::math::Vec3& rhs) {
    return Near(lhs.x, rhs.x) && Near(lhs.y, rhs.y) && Near(lhs.z, rhs.z);
}

const ri::scene::Node* FindNode(const ri::scene::Scene& scene, const std::string& name) {
    for (const ri::scene::Node& node : scene.Nodes()) {
        if (node.name == name) {
            return &node;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Expected workspace root argument.\n";
        return 1;
    }
    const std::filesystem::path workspaceRoot(argv[1]);
    const std::vector<ExperienceExpectation> expectations{
        {"CubeTest", "CubeTest_Platform", 7, {0.0f, -0.12f, 0.0f}, {16.0f, 0.24f, 16.0f}},
        {"EditorUiSmoke", "floor", 10, {0.0f, 0.0f, 0.0f}, {20.0f, 1.0f, 20.0f}},
        {"LiminalHall", "LogicDemoPressurePlate", 63, {58.0f, 0.08f, -42.0f}, {1.4f, 0.08f, 1.4f}},
        {"WildernessRuins", "MaterialShowcase_Pad", 7, {18.0f, 0.0f, 64.5f}, {16.0f, 0.12f, 4.8f}},
        {"RawIronMultiplayerSandbox", "catalog_platform_fallback", 5, {0.0f, -0.4f, 0.0f}, {340.0f, 0.8f, 55.0f}},
    };

    for (const ExperienceExpectation& expectation : expectations) {
        const std::filesystem::path gameRoot = workspaceRoot / "Games" / expectation.directory;
        const std::optional<ri::content::GameManifest> manifest =
            ri::content::LoadGameManifest(gameRoot / "manifest.json");
        if (!manifest.has_value()) {
            std::cerr << expectation.directory << ": manifest could not be loaded.\n";
            return 2;
        }
        const std::vector<std::string> issues = ri::content::ValidateGameProjectFormat(*manifest);
        if (!issues.empty()) {
            std::cerr << expectation.directory << ": project contract failed: " << issues.front() << '\n';
            return 3;
        }

        // Loading support data is part of the runtime boot path. The smoke test intentionally
        // exercises it for editor-only fixtures as well as dedicated game executables.
        (void)ri::content::LoadGameRuntimeSupportData(gameRoot);

        ri::scene::Scene scene(std::string(manifest->name) + " Contract Smoke");
        const int root = scene.CreateNode("ImportedWorld");
        ri::scene::AssemblyPrimitivesImportResult importResult{};
        std::string importError;
        const std::filesystem::path primaryLevel =
            ri::content::ResolveGameAssetPath(gameRoot, manifest->primaryLevel);
        if (!ri::scene::TryImportAssemblyPrimitivesCsv(scene, root, primaryLevel, &importResult, &importError)) {
            std::cerr << expectation.directory << ": primary assembly import failed: " << importError << '\n';
            return 4;
        }
        if (importResult.spawnedCount != expectation.expectedPrimitiveCount) {
            std::cerr << expectation.directory << ": expected " << expectation.expectedPrimitiveCount
                      << " primitives, imported " << importResult.spawnedCount << ".\n";
            return 5;
        }
        const ri::scene::Node* firstNode = FindNode(scene, expectation.firstNode);
        if (firstNode == nullptr) {
            std::cerr << expectation.directory << ": expected node was not imported: "
                      << expectation.firstNode << '\n';
            return 6;
        }
        if (!Matches(firstNode->localTransform.position, expectation.position)
            || !Matches(firstNode->localTransform.scale, expectation.scale)) {
            std::cerr << expectation.directory << ": imported transform does not match its authored CSV schema.\n";
            return 7;
        }
    }

    const std::filesystem::path editorSmokeRoot = workspaceRoot / "Games" / "EditorUiSmoke";
    const ri::scene::StarterScene editorSmokePreview = ri::editor::BuildEditorWorkspaceScene(
        "editor-ui-smoke", "Editor UI Smoke Contract Preview", editorSmokeRoot);
    if (FindNode(editorSmokePreview.scene, "floor") == nullptr
        || FindNode(editorSmokePreview.scene, "spawn_block") == nullptr) {
        std::cerr << "EditorUiSmoke: generic engine-owned editor preview did not load the authored assembly.\n";
        return 8;
    }

    std::cout << "Validated and imported all five bundled Raw Iron experiences.\n";
    return 0;
}
