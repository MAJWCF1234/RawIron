#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/StructuralAssemblyIO.h"
#include "RawIron/Scene/StructuralBrush.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

int main() {
    ri::scene::Scene scene{"StructuralBrushMetadataSmoke"};
    const int root = scene.CreateNode("Root");

    ri::scene::StructuralBrushSpawnOptions options{};
    options.nodeName = "SemanticWall";
    options.structuralType = "box";
    options.parent = root;
    options.metadata.brushId = "wall_a";
    options.metadata.role = ri::scene::StructuralBrushSemanticRole::Wall;
    options.metadata.region = "atrium";
    options.metadata.operation = ri::scene::StructuralBrushOperation::Solid;
    options.metadata.collision = ri::scene::StructuralBrushCollisionPolicy::Player;
    options.metadata.visibility = ri::scene::StructuralBrushVisibilityPolicy::Occluder;
    options.metadata.navigation = ri::scene::StructuralBrushNavigationPolicy::Blocker;
    options.metadata.rebuildScope = ri::scene::StructuralBrushRebuildScope::Local;

    const int node = ri::scene::AddStructuralBrushNode(scene, options);
    if (node == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }

    const ri::scene::StructuralBrushMetadata& metadata = scene.GetNode(node).structuralBrush;
    if (metadata.brushId != "wall_a"
        || metadata.region != "atrium"
        || metadata.role != ri::scene::StructuralBrushSemanticRole::Wall
        || metadata.collision != ri::scene::StructuralBrushCollisionPolicy::Player
        || metadata.visibility != ri::scene::StructuralBrushVisibilityPolicy::Occluder
        || metadata.navigation != ri::scene::StructuralBrushNavigationPolicy::Blocker
        || metadata.rebuildScope != ri::scene::StructuralBrushRebuildScope::Local) {
        return EXIT_FAILURE;
    }

    if (ri::scene::ToString(metadata.role) != "wall"
        || ri::scene::ToString(metadata.collision) != "player"
        || ri::scene::ToString(metadata.visibility) != "occluder") {
        return EXIT_FAILURE;
    }

    const std::filesystem::path csvPath =
        std::filesystem::temp_directory_path() / "rawiron_structural_semantic_smoke.csv";
    {
        std::ofstream csv(csvPath);
        csv << "# header\n";
        csv << "SemanticFloor,plane,0,0,0,2,1,2,0.7,0.7,0.7,lit,-,1,1,-90,0,0,,,,,,standard,MetalRough,-,-,-,-,0,0,0,1,0,"
               "floor,atrium,solid,solid,ignored,walkable,local\n";
    }

    ri::scene::Scene imported{"ImportedSemanticAssembly"};
    const int importedRoot = imported.CreateNode("Root");
    const ri::scene::StructuralAssemblySpawnResult result =
        ri::scene::SpawnStructuralAssemblyFromCsv(imported, csvPath, {.parent = importedRoot});
    std::filesystem::remove(csvPath);

    if (result.spawnedCount != 1 || imported.NodeCount() != 2) {
        return EXIT_FAILURE;
    }

    const ri::scene::StructuralBrushMetadata& importedMetadata = imported.GetNode(1).structuralBrush;
    if (importedMetadata.brushId != "SemanticFloor"
        || importedMetadata.role != ri::scene::StructuralBrushSemanticRole::Floor
        || importedMetadata.region != "atrium"
        || importedMetadata.navigation != ri::scene::StructuralBrushNavigationPolicy::Walkable) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
