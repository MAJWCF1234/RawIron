#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneStructuralTraceFeed.h"
#include "RawIron/Scene/StructuralAssemblyIO.h"
#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Scene/StructuralPrimitiveBundle.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iostream>

bool TestSurfaceCollection() {
    using namespace ri::scene;
    for (const auto key : {"revolve_open", "spline_sweep", "spline_loop", "torus", "mobius", "parametric_patch"}) {
        const auto preset=FindStructuralPreset(key);
        if (!preset) return false;
        const auto shape=ShapeFromStructuralPreset(*preset);
        const auto source=ri::structural::BuildPrimitiveMesh(preset->structuralType,shape);
        if (source.positions.empty() || source.texCoords.size()!=source.positions.size()) return false;
        Scene scene("structural surface integration");
        const int root=scene.CreateNode("root");
        StructuralPrimitiveBundleParams params;
        params.parent=root; params.presetField=key;
        const auto result=SpawnStructuralPrimitiveBundle(scene,params);
        if (result.node==kInvalidHandle) return false;
        const auto& node=scene.GetNode(result.node);
        const auto mesh=scene.GetMesh(result.mesh);
        if (node.structuralBrush.brushId.empty() || node.structuralBrush.physicsMesh.meshId.empty()
            || node.structuralBrush.queryMesh.meshId.empty() || mesh.texCoords.size()!=source.texCoords.size()) return false;
        for (std::size_t i=0;i<mesh.texCoords.size();++i)
            if (mesh.texCoords[i].x!=source.texCoords[i].x || mesh.texCoords[i].y!=source.texCoords[i].y) return false;
        // The graph/assembly route must retain options and the exact UV stream too.
        auto graphNode=MakeStructuralPrimitiveGraphNode("surface",preset->structuralType,{2,1,3},{1,1,1},shape);
        StructuralPrimitiveAssemblyOptions assembly;
        assembly.parent=root; assembly.nodes={graphNode};
        const auto assembled=AddStructuralPrimitiveAssembly(scene,assembly);
        if (assembled.meshNodes.size()!=1) return false;
        const auto& assembledMesh=scene.GetMesh(scene.GetNode(assembled.meshNodes[0]).mesh);
        if (assembledMesh.texCoords.size()!=mesh.texCoords.size()) return false;
        for (std::size_t i=0;i<assembledMesh.texCoords.size();++i)
            if (assembledMesh.texCoords[i].x!=source.texCoords[i].x || assembledMesh.texCoords[i].y!=source.texCoords[i].y) return false;
        const auto signature=ri::structural::BuildStructuralCompileSignature({graphNode});
        graphNode.closedPath=!graphNode.closedPath;
        if (signature==ri::structural::BuildStructuralCompileSignature({graphNode})) return false;
    }
    // Custom samples cannot silently become the catalog default when passed through an assembly.
    Scene scene("authored spline graph");
    const int root=scene.CreateNode("root");
    ri::structural::StructuralPrimitiveOptions shape;
    shape.points={{2,0,0},{3,1,0},{4,0,0}}; shape.sides=12; shape.pathSegments=32; shape.capEnds=false;
    StructuralPrimitiveAssemblyOptions assembly;
    assembly.parent=root;
    assembly.nodes={MakeStructuralPrimitiveGraphNode("custom", "spline_sweep",{},{1,1,1},shape)};
    const auto output=AddStructuralPrimitiveAssembly(scene,assembly);
    const auto expected=ri::structural::BuildPrimitiveMesh("spline_sweep",shape);
    if (output.meshNodes.size()!=1) return false;
    const auto& actual=scene.GetMesh(scene.GetNode(output.meshNodes[0]).mesh);
    if (actual.positions.size()!=expected.positions.size()) return false;
    for (std::size_t i=0;i<actual.positions.size();++i)
        if (ri::math::Distance(actual.positions[i],expected.positions[i])>1.e-5f) return false;
    return true;
}

int main() {
    if (!TestSurfaceCollection()) { std::cerr << "Structural surface collection integration failed\n"; return EXIT_FAILURE; }
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
    const ri::scene::StructuralBrushValidationReport validMetadata =
        ri::scene::ValidateStructuralBrushMetadata(metadata);
    if (!validMetadata.valid || !validMetadata.errors.empty()) {
        return EXIT_FAILURE;
    }

    ri::scene::StructuralBrushMetadata brokenMetadata = metadata;
    brokenMetadata.visualMesh.meshId.clear();
    brokenMetadata.queryMesh.meshId.clear();
    const ri::scene::StructuralBrushValidationReport brokenReport =
        ri::scene::ValidateStructuralBrushMetadata(brokenMetadata);
    if (brokenReport.valid || brokenReport.errors.size() < 2U) {
        return EXIT_FAILURE;
    }

    if (ri::scene::ToString(metadata.role) != "wall"
        || ri::scene::ToString(metadata.collision) != "player"
        || ri::scene::ToString(metadata.visibility) != "occluder"
        || ri::scene::ToString(ri::scene::StructuralBrushChannel::VisualMesh) != "visual_mesh"
        || ri::scene::ToString(ri::scene::StructuralBrushChannel::PhysicsMesh) != "physics_mesh"
        || ri::scene::ToString(ri::scene::StructuralBrushChannel::QueryMesh) != "query_mesh"
        || ri::scene::ToString(ri::scene::StructuralBrushChannel::InformationLayer) != "information_layer"
        || ri::scene::ToString(ri::scene::StructuralBrushQueryPurpose::Raycast) != "raycast"
        || ri::scene::ToString(ri::scene::StructuralBrushQueryPurpose::Trace) != "trace"
        || ri::scene::ToString(ri::scene::StructuralBrushQueryPurpose::Placement) != "placement"
        || ri::scene::ToString(ri::scene::StructuralBrushQueryPurpose::Interaction) != "interaction") {
        return EXIT_FAILURE;
    }

    ri::scene::StructuralBrushMetadata channelMetadata = metadata;
    const std::uint64_t baseSignature = ri::scene::StructuralBrushMetadataSignature(channelMetadata);
    if (baseSignature == 0
        || baseSignature != ri::scene::StructuralBrushMetadataSignature(metadata)) {
        return EXIT_FAILURE;
    }

    ri::scene::StructuralBrushMetadata changedSignatureMetadata = channelMetadata;
    changedSignatureMetadata.region = "atrium_annex";
    if (ri::scene::StructuralBrushMetadataSignature(changedSignatureMetadata) == baseSignature) {
        return EXIT_FAILURE;
    }
    changedSignatureMetadata = channelMetadata;
    changedSignatureMetadata.queryMesh.traceable = false;
    if (ri::scene::StructuralBrushMetadataSignature(changedSignatureMetadata) == baseSignature) {
        return EXIT_FAILURE;
    }
    changedSignatureMetadata = channelMetadata;
    changedSignatureMetadata.informationLayer.relations.push_back("adjacent:atrium_portal");
    if (ri::scene::StructuralBrushMetadataSignature(changedSignatureMetadata) == baseSignature) {
        return EXIT_FAILURE;
    }

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

    if (!ri::scene::StructuralBrushSupportsQueryPurpose(channelMetadata,
                                                        ri::scene::StructuralBrushQueryPurpose::Raycast)
        || !ri::scene::StructuralBrushSupportsQueryPurpose(channelMetadata,
                                                           ri::scene::StructuralBrushQueryPurpose::Trace)
        || !ri::scene::StructuralBrushSupportsQueryPurpose(channelMetadata,
                                                           ri::scene::StructuralBrushQueryPurpose::Placement)
        || !ri::scene::StructuralBrushSupportsQueryPurpose(
            channelMetadata, ri::scene::StructuralBrushQueryPurpose::Interaction)) {
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

    if (ri::scene::StructuralBrushSupportsQueryPurpose(channelMetadata,
                                                       ri::scene::StructuralBrushQueryPurpose::Raycast)
        || ri::scene::StructuralBrushSupportsQueryPurpose(channelMetadata,
                                                          ri::scene::StructuralBrushQueryPurpose::Trace)
        || ri::scene::StructuralBrushSupportsQueryPurpose(channelMetadata,
                                                          ri::scene::StructuralBrushQueryPurpose::Placement)
        || ri::scene::StructuralBrushSupportsQueryPurpose(
            channelMetadata, ri::scene::StructuralBrushQueryPurpose::Interaction)) {
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

    // CSG output is still an authored structural primitive. Compiled fragments must retain
    // M/P/Q/I ownership so the structural trace feed can use the same generated geometry
    // as gameplay movement and ballistics.
    ri::scene::Scene assemblyScene{"CompiledStructuralAssemblyMetadata"};
    const int assemblyRoot = assemblyScene.CreateNode("Root");
    ri::scene::StructuralPrimitiveAssemblyOptions assembly{};
    assembly.parent = assemblyRoot;
    assembly.rootNodeName = "CompiledAssembly";
    assembly.material.materialName = "compiled_assembly_material";
    assembly.nodes.push_back(ri::scene::MakeStructuralPrimitiveSolid(
        "compiled_shell", "box", {0.0f, 2.0f, 0.0f}, {6.0f, 4.0f, 4.0f}));
    assembly.nodes.push_back(ri::scene::MakeStructuralPrimitiveSubtract(
        "compiled_door_cut", "box", {"compiled_shell"}, {0.0f, 1.5f, 2.1f}, {1.8f, 3.0f, 0.8f}));

    const ri::scene::StructuralPrimitiveAssemblyResult compiledAssembly =
        ri::scene::AddStructuralPrimitiveAssembly(assemblyScene, assembly);
    if (compiledAssembly.root == ri::scene::kInvalidHandle || compiledAssembly.meshNodes.empty()) {
        return EXIT_FAILURE;
    }

    const ri::scene::StructuralBrushMetadata& compiledMetadata =
        assemblyScene.GetNode(compiledAssembly.meshNodes.front()).structuralBrush;
    if (compiledMetadata.brushId.empty()
        || compiledMetadata.operation != ri::scene::StructuralBrushOperation::Solid
        || compiledMetadata.role != ri::scene::StructuralBrushSemanticRole::Structure
        || compiledMetadata.collision != ri::scene::StructuralBrushCollisionPolicy::Solid
        || compiledMetadata.visualMesh.meshId.empty()
        || compiledMetadata.visualMesh.materialSetId != "compiled_assembly_material"
        || compiledMetadata.physicsMesh.meshId.empty()
        || compiledMetadata.queryMesh.meshId.empty()
        || compiledMetadata.informationLayer.semanticGraphId
               != "structural." + compiledMetadata.brushId
        || std::find(compiledMetadata.informationLayer.relations.begin(),
                     compiledMetadata.informationLayer.relations.end(),
                     "structural.source:compiled_shell")
               == compiledMetadata.informationLayer.relations.end()) {
        return EXIT_FAILURE;
    }

    const std::vector<ri::trace::TraceCollider> compiledColliders =
        ri::scene::BuildStructuralTraceCollidersForSubtree(assemblyScene, compiledAssembly.root);
    if (compiledColliders.size() != compiledAssembly.meshNodes.size()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
