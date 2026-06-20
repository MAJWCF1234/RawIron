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
    options.metadata.visualMesh.meshId = "wall_a_visual";
    options.metadata.visualMesh.materialSetId = "atrium_concrete";
    options.metadata.physicsMesh.meshId = "wall_a_physics";
    options.metadata.physicsMesh.physicalMaterial = "concrete_heavy";
    options.metadata.physicsMesh.rigidBodyShape = "static_hull";
    options.metadata.queryMesh.meshId = "wall_a_query";
    options.metadata.queryMesh.raycastShape = "coarse_box";
    options.metadata.informationLayer.semanticGraphId = "ssg.atrium.wall_a";
    options.metadata.informationLayer.gameplayMeaning = "cover_wall";
    options.metadata.informationLayer.relations.push_back("supports:atrium_ceiling");

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
        || metadata.rebuildScope != ri::scene::StructuralBrushRebuildScope::Local
        || metadata.visualMesh.meshId != "wall_a_visual"
        || metadata.visualMesh.materialSetId != "atrium_concrete"
        || metadata.physicsMesh.meshId != "wall_a_physics"
        || metadata.physicsMesh.physicalMaterial != "concrete_heavy"
        || metadata.physicsMesh.rigidBodyShape != "static_hull"
        || metadata.queryMesh.meshId != "wall_a_query"
        || metadata.queryMesh.raycastShape != "coarse_box"
        || metadata.informationLayer.semanticGraphId != "ssg.atrium.wall_a"
        || metadata.informationLayer.gameplayMeaning != "cover_wall"
        || metadata.informationLayer.relations.size() != 1
        || metadata.informationLayer.relations[0] != "supports:atrium_ceiling") {
        return EXIT_FAILURE;
    }

    if (ri::scene::ToString(metadata.role) != "wall"
        || ri::scene::ToString(metadata.collision) != "player"
        || ri::scene::ToString(metadata.visibility) != "occluder"
        || ri::scene::ToString(ri::scene::StructuralBrushChannel::VisualMesh) != "visual_mesh"
        || ri::scene::ToString(ri::scene::StructuralBrushChannel::PhysicsMesh) != "physics_mesh"
        || ri::scene::ToString(ri::scene::StructuralBrushChannel::QueryMesh) != "query_mesh"
        || ri::scene::ToString(ri::scene::StructuralBrushChannel::InformationLayer) != "information_layer") {
        return EXIT_FAILURE;
    }

    ri::scene::StructuralBrushMetadata channelMetadata = metadata;
    if (!ri::scene::StructuralBrushParticipatesInChannel(channelMetadata,
                                                         ri::scene::StructuralBrushChannel::VisualMesh)
        || !ri::scene::StructuralBrushParticipatesInChannel(channelMetadata,
                                                            ri::scene::StructuralBrushChannel::PhysicsMesh)
        || !ri::scene::StructuralBrushParticipatesInChannel(channelMetadata,
                                                            ri::scene::StructuralBrushChannel::QueryMesh)
        || !ri::scene::StructuralBrushParticipatesInChannel(
            channelMetadata, ri::scene::StructuralBrushChannel::InformationLayer)) {
        return EXIT_FAILURE;
    }

    channelMetadata.visualMesh.renderable = false;
    channelMetadata.physicsMesh.participatesInSimulation = false;
    channelMetadata.queryMesh.raycastable = false;
    channelMetadata.queryMesh.traceable = false;
    channelMetadata.queryMesh.placeable = false;
    channelMetadata.queryMesh.interactable = false;
    channelMetadata.informationLayer.reportable = false;
    if (ri::scene::StructuralBrushParticipatesInChannel(channelMetadata,
                                                        ri::scene::StructuralBrushChannel::VisualMesh)
        || ri::scene::StructuralBrushParticipatesInChannel(channelMetadata,
                                                           ri::scene::StructuralBrushChannel::PhysicsMesh)
        || ri::scene::StructuralBrushParticipatesInChannel(channelMetadata,
                                                           ri::scene::StructuralBrushChannel::QueryMesh)
        || ri::scene::StructuralBrushParticipatesInChannel(
            channelMetadata, ri::scene::StructuralBrushChannel::InformationLayer)) {
        return EXIT_FAILURE;
    }

    ri::scene::StructuralBrushSpawnOptions defaultMetadataOptions{};
    defaultMetadataOptions.nodeName = "DefaultOwnedBrush";
    defaultMetadataOptions.structuralType = "box";
    defaultMetadataOptions.parent = root;
    const int defaultMetadataNode = ri::scene::AddStructuralBrushNode(scene, defaultMetadataOptions);
    if (defaultMetadataNode == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }
    const ri::scene::StructuralBrushMetadata& defaultMetadata =
        scene.GetNode(defaultMetadataNode).structuralBrush;
    if (defaultMetadata.brushId != "DefaultOwnedBrush"
        || defaultMetadata.role != ri::scene::StructuralBrushSemanticRole::Structure
        || defaultMetadata.collision != ri::scene::StructuralBrushCollisionPolicy::Solid
        || defaultMetadata.visualMesh.meshId != "DefaultOwnedBrush_Mesh"
        || defaultMetadata.visualMesh.materialSetId != "struct_primitive"
        || defaultMetadata.physicsMesh.meshId != "DefaultOwnedBrush_PMesh"
        || defaultMetadata.physicsMesh.rigidBodyShape != "structural_hull"
        || defaultMetadata.physicsMesh.simulationShape != "structural_sim"
        || defaultMetadata.queryMesh.meshId != "DefaultOwnedBrush_QMesh"
        || defaultMetadata.queryMesh.raycastShape != "structural_query"
        || defaultMetadata.queryMesh.placementShape != "structural_placement"
        || defaultMetadata.queryMesh.interactionShape != "structural_interaction"
        || defaultMetadata.informationLayer.semanticGraphId != "structural.DefaultOwnedBrush"
        || defaultMetadata.informationLayer.gameplayMeaning != "structure") {
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
