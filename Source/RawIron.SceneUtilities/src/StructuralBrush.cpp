#include "RawIron/Scene/StructuralBrush.h"

namespace ri::scene {

Mesh MeshFromStructuralCompiledMesh(const ri::structural::CompiledMesh& compiled, std::string meshName) {
    Mesh mesh{};
    mesh.name = std::move(meshName);
    mesh.primitive = PrimitiveType::Custom;
    mesh.positions = compiled.positions;
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    mesh.indexCount = static_cast<int>(mesh.positions.size());
    return mesh;
}

int AddStructuralBrushNode(Scene& scene, const StructuralBrushSpawnOptions& options) {
    const ri::structural::CompiledMesh compiled =
        ri::structural::BuildPrimitiveMesh(options.structuralType, options.shape);
    if (compiled.positions.empty()) {
        return kInvalidHandle;
    }

    Mesh mesh = MeshFromStructuralCompiledMesh(compiled, options.nodeName + "_Mesh");

    const int materialHandle = scene.AddMaterial(Material{
        .name = options.materialName,
        .shadingModel = options.shadingModel,
        .baseColor = options.baseColor,
        .baseColorTexture = options.baseColorTexture,
        .textureTiling = options.textureTiling,
    });
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int nodeHandle = scene.CreateNode(options.nodeName, options.parent);
    scene.GetNode(nodeHandle).localTransform = options.transform;
    scene.AttachMesh(nodeHandle, meshHandle, materialHandle);
    return nodeHandle;
}

} // namespace ri::scene
