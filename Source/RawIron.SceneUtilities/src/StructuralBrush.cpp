#include "RawIron/Scene/StructuralBrush.h"

#include <cmath>
#include <cstddef>

namespace ri::scene {

Mesh MeshFromStructuralCompiledMesh(const ri::structural::CompiledMesh& compiled, std::string meshName) {
    Mesh mesh{};
    mesh.name = std::move(meshName);
    mesh.primitive = PrimitiveType::Custom;
    mesh.positions = compiled.positions;
    if (compiled.normals.size() == compiled.positions.size()) {
        mesh.normals = compiled.normals;
    }
    mesh.texCoords.reserve(mesh.positions.size());
    for (std::size_t index = 0; index < mesh.positions.size(); index += 3U) {
        const ri::math::Vec3 a = mesh.positions[index];
        const ri::math::Vec3 b = (index + 1U) < mesh.positions.size() ? mesh.positions[index + 1U] : a;
        const ri::math::Vec3 c = (index + 2U) < mesh.positions.size() ? mesh.positions[index + 2U] : a;
        const ri::math::Vec3 n = ri::math::Normalize(ri::math::Cross(b - a, c - a));
        const ri::math::Vec3 an{std::fabs(n.x), std::fabs(n.y), std::fabs(n.z)};
        const auto project = [&](const ri::math::Vec3& p) {
            if (an.y >= an.x && an.y >= an.z) {
                return ri::math::Vec2{p.x + 0.5f, p.z + 0.5f};
            }
            if (an.x >= an.z) {
                return ri::math::Vec2{p.z + 0.5f, p.y + 0.5f};
            }
            return ri::math::Vec2{p.x + 0.5f, p.y + 0.5f};
        };
        mesh.texCoords.push_back(project(a));
        if ((index + 1U) < mesh.positions.size()) {
            mesh.texCoords.push_back(project(b));
        }
        if ((index + 2U) < mesh.positions.size()) {
            mesh.texCoords.push_back(project(c));
        }
    }
    mesh.vertexCount = static_cast<int>(mesh.positions.size());
    if (compiled.triangleCount > 0U) {
        mesh.indexCount = static_cast<int>(compiled.triangleCount * 3U);
    } else if (!mesh.positions.empty()) {
        mesh.indexCount = mesh.vertexCount;
    }
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
        .materialStyle = options.materialStyle,
        .materialWorkflow = options.materialWorkflow,
        .baseColor = options.baseColor,
        .baseColorTexture = options.baseColorTexture,
        .textureTiling = options.textureTiling,
        .emissiveColor = options.emissiveColor,
        .metallic = options.metallic,
        .roughness = options.roughness,
        .opacity = options.opacity,
        .alphaCutoff = options.alphaCutoff,
        .doubleSided = options.doubleSided,
        .transparent = options.transparent,
        .additiveBlend = options.additiveBlend,
        .normalTexture = options.normalTexture,
        .ormTexture = options.ormTexture,
        .roughnessTexture = options.roughnessTexture,
        .metallicTexture = options.metallicTexture,
        .emissiveTexture = options.emissiveTexture,
        .opacityTexture = options.opacityTexture,
        .occlusionTexture = options.occlusionTexture,
        .detailTexture = options.detailTexture,
    });
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int nodeHandle = scene.CreateNode(options.nodeName, options.parent);
    scene.GetNode(nodeHandle).localTransform = options.transform;
    scene.AttachMesh(nodeHandle, meshHandle, materialHandle);
    return nodeHandle;
}

} // namespace ri::scene
