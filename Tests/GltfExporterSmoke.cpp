#include "RawIron/Scene/GltfExporter.h"
#include "RawIron/Scene/GltfLoader.h"
#include "RawIron/Scene/Helpers.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {
bool Require(const bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}
}

int main() {
    namespace fs = std::filesystem;
    ri::scene::Scene source("glTF exporter round-trip");
    const int root = source.CreateNode("Root");
    ri::scene::Mesh panel = ri::scene::MakeBillboardQuadMesh("NormalPanel");
    panel.normals.assign(panel.positions.size(), {0.0f, 0.0f, 1.0f});
    const int mesh = source.AddMesh(std::move(panel));
    const fs::path textureFixture = fs::path(__FILE__).parent_path() / ".." / "Games" / "CubeTest"
        / "assets" / "reference" / "threejs-r185" / "textures" / "uv_grid_opengl.jpg";
    const int material = source.AddMaterial({
        .name = "NormalMaterial", .baseColor = {0.3f,0.6f,0.9f},
        .baseColorTexture = textureFixture.lexically_normal().string(),
        .metallic = 0.2f, .roughness = 0.35f});
    const int node = source.CreateNode("NormalPanelNode", root);
    source.AttachMesh(node, mesh, material);
    source.GetNode(node).localTransform.position = {1.0f,2.0f,3.0f};

    ri::scene::Camera camera{.name="ExportCamera", .fieldOfViewDegrees=67.0f, .nearClip=0.05f, .farClip=900.0f};
    const int cameraNode = source.CreateNode("Camera", root);
    source.AttachCamera(cameraNode, source.AddCamera(camera));
    ri::scene::Light light{.name="ExportLight", .type=ri::scene::LightType::Point,
                           .color={1.0f,0.8f,0.6f}, .intensity=2.0f, .range=12.0f};
    const int lightNode = source.CreateNode("Light", root);
    source.AttachLight(lightNode, source.AddLight(light));

    ri::scene::MeshInstanceBatch batch{};
    batch.name="ExportInstances"; batch.parent=root; batch.mesh=mesh; batch.material=material;
    batch.transforms = {
        ri::scene::Transform{.position={-1.0f,0.0f,0.0f}},
        ri::scene::Transform{.position={1.0f,0.0f,0.0f}},
    };
    const int batchHandle = source.AddMeshInstanceBatch(std::move(batch));
    (void)batchHandle;

    const fs::path exportDirectory = fs::temp_directory_path() / "rawiron-gltf-exporter-smoke";
    const fs::path base = exportDirectory / "scene.gltf";
    ri::scene::GltfExportReport report{};
    std::string error;
    bool ok = Require(ri::scene::ExportSceneToGltf(source, base, {}, report, error), error.c_str());
    ok &= Require(fs::is_regular_file(report.jsonPath) && fs::is_regular_file(report.binaryPath),
                  "exporter should write JSON and binary files");
    ok &= Require(report.instanceCount == 2U && report.cameraCount == 1U && report.lightCount == 1U,
                  "export report should include instances, camera, and light");
    ok &= Require(report.textureCount == 1U,
                  "export report should include the copied external texture dependency");

    ri::scene::Scene imported("round trip");
    const int importedRoot = ri::scene::ImportGltfToScene(
        imported, report.jsonPath, {.wrapperNodeName="Imported", .importCameras=true, .importLights=true}, error);
    ok &= Require(importedRoot != ri::scene::kInvalidHandle, error.c_str());
    bool foundNormalsAndUvs = false;
    for (std::size_t index=0; index<imported.MeshCount(); ++index) {
        const auto& importedMesh = imported.GetMesh(static_cast<int>(index));
        foundNormalsAndUvs |= importedMesh.normals.size() == importedMesh.positions.size()
            && importedMesh.texCoords.size() == importedMesh.positions.size() && !importedMesh.positions.empty();
    }
    ok &= Require(foundNormalsAndUvs, "round-trip import should preserve normals and UV0");
    bool foundResolvedTexture = false;
    for (std::size_t index=0; index<imported.MaterialCount(); ++index) {
        const auto& importedMaterial = imported.GetMaterial(static_cast<int>(index));
        foundResolvedTexture |= !importedMaterial.baseColorTexture.empty()
            && fs::is_regular_file(importedMaterial.baseColorTexture);
    }
    ok &= Require(foundResolvedTexture,
                  "round-trip import should resolve a copied texture relative to the glTF source");

    std::error_code ignored;
    fs::remove_all(exportDirectory, ignored);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
