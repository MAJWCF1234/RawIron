#include "RawIron/Scene/StructuralBrush.h"

#include "RawIron/Structural/StructuralCompiler.h"
#include "RawIron/Structural/StructuralPrimitives.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace ri::scene {

namespace {

void AppendTriangle(ri::structural::CompiledMesh& mesh,
                    const ri::math::Vec3& a,
                    const ri::math::Vec3& b,
                    const ri::math::Vec3& c,
                    const ri::math::Vec3& normal) {
    mesh.positions.push_back(a);
    mesh.positions.push_back(b);
    mesh.positions.push_back(c);
    mesh.normals.push_back(normal);
    mesh.normals.push_back(normal);
    mesh.normals.push_back(normal);
    ++mesh.triangleCount;
}

void AppendAxisQuad(ri::structural::CompiledMesh& mesh,
                    const ri::math::Vec3& a,
                    const ri::math::Vec3& b,
                    const ri::math::Vec3& c,
                    const ri::math::Vec3& d,
                    const ri::math::Vec3& normal) {
    AppendTriangle(mesh, a, b, c, normal);
    AppendTriangle(mesh, a, c, d, normal);
}

void AppendSubdividedPlane(ri::structural::CompiledMesh& mesh,
                           const ri::math::Vec3& origin,
                           const ri::math::Vec3& axisU,
                           const ri::math::Vec3& axisV,
                           const ri::math::Vec3& normal,
                           const int segmentsU,
                           const int segmentsV) {
    const int safeSegmentsU = std::max(segmentsU, 1);
    const int safeSegmentsV = std::max(segmentsV, 1);
    for (int v = 0; v < safeSegmentsV; ++v) {
        const float v0 = static_cast<float>(v) / static_cast<float>(safeSegmentsV);
        const float v1 = static_cast<float>(v + 1) / static_cast<float>(safeSegmentsV);
        for (int u = 0; u < safeSegmentsU; ++u) {
            const float u0 = static_cast<float>(u) / static_cast<float>(safeSegmentsU);
            const float u1 = static_cast<float>(u + 1) / static_cast<float>(safeSegmentsU);
            const ri::math::Vec3 corner00 = origin + (axisU * u0) + (axisV * v0);
            const ri::math::Vec3 corner10 = origin + (axisU * u1) + (axisV * v0);
            const ri::math::Vec3 corner01 = origin + (axisU * u0) + (axisV * v1);
            const ri::math::Vec3 corner11 = origin + (axisU * u1) + (axisV * v1);
            AppendAxisQuad(mesh, corner00, corner10, corner11, corner01, normal);
        }
    }
}

/// Unit cube with subdivided top/bottom so large scaled floors do not rely on giant single-triangle
/// interpolation (which makes world-position shading swim against nearby props when the view pitches).
ri::structural::CompiledMesh BuildSubdividedUnitBoxCompiledMesh(const int segmentsX, const int segmentsZ) {
    ri::structural::CompiledMesh mesh{};
    constexpr float kHalf = 0.5f;
    const ri::math::Vec3 axisX{1.0f, 0.0f, 0.0f};
    const ri::math::Vec3 axisZ{0.0f, 0.0f, 1.0f};

    AppendSubdividedPlane(mesh,
                          ri::math::Vec3{-kHalf, kHalf, -kHalf},
                          axisX,
                          axisZ,
                          ri::math::Vec3{0.0f, 1.0f, 0.0f},
                          segmentsX,
                          segmentsZ);
    AppendSubdividedPlane(mesh,
                          ri::math::Vec3{-kHalf, -kHalf, kHalf},
                          axisX,
                          axisZ * -1.0f,
                          ri::math::Vec3{0.0f, -1.0f, 0.0f},
                          segmentsX,
                          segmentsZ);

    AppendAxisQuad(mesh,
                   ri::math::Vec3{-kHalf, -kHalf, -kHalf},
                   ri::math::Vec3{kHalf, -kHalf, -kHalf},
                   ri::math::Vec3{kHalf, kHalf, -kHalf},
                   ri::math::Vec3{-kHalf, kHalf, -kHalf},
                   ri::math::Vec3{0.0f, 0.0f, -1.0f});
    AppendAxisQuad(mesh,
                   ri::math::Vec3{kHalf, -kHalf, kHalf},
                   ri::math::Vec3{-kHalf, -kHalf, kHalf},
                   ri::math::Vec3{-kHalf, kHalf, kHalf},
                   ri::math::Vec3{kHalf, kHalf, kHalf},
                   ri::math::Vec3{0.0f, 0.0f, 1.0f});
    AppendAxisQuad(mesh,
                   ri::math::Vec3{kHalf, -kHalf, -kHalf},
                   ri::math::Vec3{kHalf, -kHalf, kHalf},
                   ri::math::Vec3{kHalf, kHalf, kHalf},
                   ri::math::Vec3{kHalf, kHalf, -kHalf},
                   ri::math::Vec3{1.0f, 0.0f, 0.0f});
    AppendAxisQuad(mesh,
                   ri::math::Vec3{-kHalf, -kHalf, kHalf},
                   ri::math::Vec3{-kHalf, -kHalf, -kHalf},
                   ri::math::Vec3{-kHalf, kHalf, -kHalf},
                   ri::math::Vec3{-kHalf, kHalf, kHalf},
                   ri::math::Vec3{-1.0f, 0.0f, 0.0f});

    mesh.hasBounds = true;
    mesh.boundsMin = ri::math::Vec3{-kHalf, -kHalf, -kHalf};
    mesh.boundsMax = ri::math::Vec3{kHalf, kHalf, kHalf};
    return mesh;
}

int PlanarSegmentsForScale(const float worldSpan, const float targetCellSizeMeters) {
    return std::clamp(
        static_cast<int>(std::ceil(worldSpan / std::max(targetCellSizeMeters, 0.5f))),
        1,
        96);
}

bool BakeWorldTileUvsInMesh(Mesh& mesh,
                            const ri::math::Vec3& scale,
                            const ri::math::Vec2& tiling) {
    if (mesh.positions.empty() || mesh.texCoords.size() != mesh.positions.size()) {
        return false;
    }
    const float maxPlanar = std::max(scale.x, scale.z);
    if (maxPlanar <= 8.0f || std::max(tiling.x, tiling.y) <= 16.0f) {
        return false;
    }
    const float tileWorldSizeX = scale.x / std::max(tiling.x, 1.0f);
    const float tileWorldSizeZ = scale.z / std::max(tiling.y, 1.0f);
    const float tileWorldSizeY = scale.y / std::max(tiling.y, 1.0f);
    const bool hasNormals = mesh.normals.size() == mesh.positions.size();
    for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
        const ri::math::Vec3& p = mesh.positions[i];
        const ri::math::Vec3 faceHint =
            hasNormals ? mesh.normals[i] : ri::math::Vec3{p.x, p.y, p.z};
        const ri::math::Vec3 an{std::fabs(faceHint.x), std::fabs(faceHint.y), std::fabs(faceHint.z)};
        if (an.y >= an.x && an.y >= an.z) {
            mesh.texCoords[i] = ri::math::Vec2{
                (p.x + 0.5f) * scale.x / tileWorldSizeX,
                (p.z + 0.5f) * scale.z / tileWorldSizeZ,
            };
        } else if (an.x >= an.z) {
            mesh.texCoords[i] = ri::math::Vec2{
                (p.z + 0.5f) * scale.z / tileWorldSizeZ,
                (p.y + 0.5f) * scale.y / tileWorldSizeY,
            };
        } else {
            mesh.texCoords[i] = ri::math::Vec2{
                (p.x + 0.5f) * scale.x / tileWorldSizeX,
                (p.y + 0.5f) * scale.y / tileWorldSizeY,
            };
        }
    }
    return true;
}

ri::structural::CompiledMesh BuildStructuralPrimitiveCompiledMesh(const StructuralBrushSpawnOptions& options) {
    const std::string typeKey = ri::structural::NormalizeStructuralPrimitiveTypeKey(options.structuralType);
    if (typeKey == "box") {
        const float maxPlanar = std::max(options.transform.scale.x, options.transform.scale.z);
        if (maxPlanar > 8.0f) {
            constexpr float kTargetCellSizeMeters = 1.25f;
            return BuildSubdividedUnitBoxCompiledMesh(
                PlanarSegmentsForScale(options.transform.scale.x, kTargetCellSizeMeters),
                PlanarSegmentsForScale(options.transform.scale.z, kTargetCellSizeMeters));
        }
    }
    return ri::structural::BuildPrimitiveMesh(options.structuralType, options.shape);
}

} // namespace

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
    const ri::structural::CompiledMesh compiled = BuildStructuralPrimitiveCompiledMesh(options);
    if (compiled.positions.empty()) {
        return kInvalidHandle;
    }

    Mesh mesh = MeshFromStructuralCompiledMesh(compiled, options.nodeName + "_Mesh");

    ri::math::Vec2 materialTiling = options.textureTiling;
    const bool bakedWorldTileUv =
        BakeWorldTileUvsInMesh(mesh, options.transform.scale, materialTiling);
    if (bakedWorldTileUv) {
        materialTiling = {1.0f, 1.0f};
    }

    const int materialHandle = scene.AddMaterial(Material{
        .name = options.materialName,
        .shadingModel = options.shadingModel,
        .materialStyle = options.materialStyle,
        .materialWorkflow = options.materialWorkflow,
        .baseColor = options.baseColor,
        .baseColorTexture = options.baseColorTexture,
        .textureTiling = materialTiling,
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
        .bakedWorldTileUv = bakedWorldTileUv,
    });
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int nodeHandle = scene.CreateNode(options.nodeName, options.parent);
    scene.GetNode(nodeHandle).localTransform = options.transform;
    scene.AttachMesh(nodeHandle, meshHandle, materialHandle);
    return nodeHandle;
}

namespace {

int AddMaterialFromSpawnOptions(Scene& scene, StructuralBrushSpawnOptions material) {
    if (material.materialName.empty()) {
        material.materialName = "struct_brush";
    }
    return scene.AddMaterial(Material{
        .name = material.materialName,
        .shadingModel = material.shadingModel,
        .materialStyle = material.materialStyle,
        .materialWorkflow = material.materialWorkflow,
        .baseColor = material.baseColor,
        .baseColorTexture = material.baseColorTexture,
        .textureTiling = material.textureTiling,
        .emissiveColor = material.emissiveColor,
        .metallic = material.metallic,
        .roughness = material.roughness,
        .opacity = material.opacity,
        .alphaCutoff = material.alphaCutoff,
        .doubleSided = material.doubleSided,
        .transparent = material.transparent,
        .additiveBlend = material.additiveBlend,
        .normalTexture = material.normalTexture,
        .ormTexture = material.ormTexture,
        .roughnessTexture = material.roughnessTexture,
        .metallicTexture = material.metallicTexture,
        .emissiveTexture = material.emissiveTexture,
        .opacityTexture = material.opacityTexture,
        .occlusionTexture = material.occlusionTexture,
        .detailTexture = material.detailTexture,
    });
}

StructuralBrushSpawnOptions SpawnOptionsFromAssemblyNode(const StructuralPrimitiveAssemblyOptions& assembly,
                                                         const ri::structural::StructuralNode& node) {
    StructuralBrushSpawnOptions spawn = assembly.material;
    spawn.nodeName = !node.name.empty() ? node.name : (!node.id.empty() ? node.id : "structural_primitive");
    spawn.transform.position = node.position;
    spawn.transform.rotationDegrees = node.rotation;
    spawn.transform.scale = node.scale;
    return spawn;
}

int SpawnWorldSpaceBrushMesh(Scene& scene,
                             const int parent,
                             const int materialHandle,
                             const ri::structural::CompiledGeometryNode& compiled,
                             const std::string& nodeName) {
    if (compiled.compiledMesh.positions.empty()) {
        return kInvalidHandle;
    }

    Mesh mesh = MeshFromStructuralCompiledMesh(compiled.compiledMesh, nodeName + "_Mesh");
    const int meshHandle = scene.AddMesh(std::move(mesh));
    const int nodeHandle = scene.CreateNode(nodeName, parent);
    scene.GetNode(nodeHandle).localTransform = Transform{};
    scene.AttachMesh(nodeHandle, meshHandle, materialHandle);
    return nodeHandle;
}

int SpawnPassthroughPrimitiveFromNode(Scene& scene,
                                      const StructuralPrimitiveAssemblyOptions& assembly,
                                      const int parent,
                                      const ri::structural::StructuralNode& node) {
    const std::string resolvedType = ri::structural::NormalizeStructuralPrimitiveTypeKey(
        !node.primitiveType.empty() ? node.primitiveType : node.type);
    if (!ri::structural::IsNativeStructuralPrimitive(resolvedType)) {
        return kInvalidHandle;
    }

    ri::structural::StructuralPrimitiveOptions shape{};
    if (node.radialSegments > 0) {
        shape.radialSegments = node.radialSegments;
    }
    if (node.sides > 0) {
        shape.sides = node.sides;
    }
    if (node.steps > 0) {
        shape.steps = node.steps;
    }
    if (node.cellsX > 0) {
        shape.cellsX = node.cellsX;
    }
    if (node.cellsY > 0) {
        shape.cellsY = node.cellsY;
    }
    if (node.thickness > 0.0f) {
        shape.thickness = node.thickness;
    }
    if (node.depth > 0.0f) {
        shape.depth = node.depth;
    }
    if (node.spanDegrees > 0.0f) {
        shape.spanDegrees = node.spanDegrees;
    }
    if (!node.archStyle.empty()) {
        shape.archStyle = node.archStyle;
    }
    if (!node.latticeStyle.empty()) {
        shape.latticeStyle = node.latticeStyle;
    }

    StructuralBrushSpawnOptions spawn = SpawnOptionsFromAssemblyNode(assembly, node);
    spawn.structuralType = resolvedType;
    spawn.shape = shape;
    spawn.parent = parent;
    return AddStructuralBrushNode(scene, spawn);
}

bool ShouldSkipAssemblyPassthroughNode(const ri::structural::StructuralNode& node) {
    if (node.type == "boolean_subtractor" || node.type == "boolean_union" || node.type == "boolean_difference") {
        return true;
    }
    if (node.type == "boolean_intersection") {
        return true;
    }
    if (node.type == "bevel_modifier_primitive" || node.type == "structural_detail_modifier"
        || node.type == "non_manifold_reconciler" || node.type == "convex_hull_aggregate"
        || node.type == "door_window_cutout") {
        return true;
    }
    return false;
}

} // namespace

ri::structural::StructuralNode MakeStructuralPrimitiveSolid(const std::string id,
                                                         const std::string_view structuralType,
                                                         const ri::math::Vec3 position,
                                                         const ri::math::Vec3 scale,
                                                         const ri::math::Vec3 rotationDegrees) {
    ri::structural::StructuralNode node{};
    node.id = id;
    node.name = id;
    node.type = std::string(structuralType);
    node.primitiveType = std::string(structuralType);
    node.position = position;
    node.rotation = rotationDegrees;
    node.scale = scale;
    return node;
}

ri::structural::StructuralNode MakeStructuralPrimitiveSubtract(const std::string id,
                                                           const std::string_view cutterType,
                                                           std::vector<std::string> targetIds,
                                                           const ri::math::Vec3 position,
                                                           const ri::math::Vec3 scale,
                                                           const ri::math::Vec3 rotationDegrees) {
    ri::structural::StructuralNode node{};
    node.id = id;
    node.name = id;
    node.type = "boolean_subtractor";
    node.primitiveType = std::string(cutterType);
    node.targetIds = std::move(targetIds);
    node.position = position;
    node.rotation = rotationDegrees;
    node.scale = scale;
    return node;
}

ri::structural::StructuralNode MakeStructuralPrimitiveGraphNode(const std::string id,
                                                            const std::string_view structuralType,
                                                            const ri::math::Vec3 position,
                                                            const ri::math::Vec3 scale,
                                                            const ri::structural::StructuralPrimitiveOptions& shape,
                                                            const ri::math::Vec3 rotationDegrees) {
    ri::structural::StructuralNode node = MakeStructuralPrimitiveSolid(id, structuralType, position, scale, rotationDegrees);
    if (shape.radialSegments > 0) {
        node.radialSegments = shape.radialSegments;
    }
    if (shape.sides > 0) {
        node.sides = shape.sides;
    }
    if (shape.steps > 0) {
        node.steps = shape.steps;
    }
    if (shape.cellsX > 0) {
        node.cellsX = shape.cellsX;
    }
    if (shape.cellsY > 0) {
        node.cellsY = shape.cellsY;
    }
    if (shape.thickness > 0.0f) {
        node.thickness = shape.thickness;
    }
    if (shape.depth > 0.0f) {
        node.depth = shape.depth;
    }
    if (shape.spanDegrees > 0.0f) {
        node.spanDegrees = shape.spanDegrees;
    }
    if (!shape.archStyle.empty()) {
        node.archStyle = shape.archStyle;
    }
    if (!shape.latticeStyle.empty()) {
        node.latticeStyle = shape.latticeStyle;
    }
    return node;
}

StructuralPrimitiveAssemblyResult AddStructuralPrimitiveAssembly(Scene& scene,
                                                                 const StructuralPrimitiveAssemblyOptions& options) {
    StructuralPrimitiveAssemblyResult result{};
    if (options.nodes.empty() || options.parent == kInvalidHandle) {
        return result;
    }

    const ri::structural::StructuralGeometryCompileResult compiled =
        ri::structural::CompileStructuralGeometryNodes(options.nodes, options.compileOptions);
    result.compileWarnings = compiled.compileWarnings;

    const int root = scene.CreateNode(options.rootNodeName, options.parent);
    if (root == kInvalidHandle) {
        return result;
    }
    scene.GetNode(root).localTransform = options.transform;

    StructuralBrushSpawnOptions material = options.material;
    if (material.materialName.empty()) {
        material.materialName = options.rootNodeName + "_mat";
    }
    const int materialHandle = AddMaterialFromSpawnOptions(scene, material);

    for (const ri::structural::CompiledGeometryNode& fragment : compiled.compiledNodes) {
        const std::string nodeName = !fragment.node.name.empty() ? fragment.node.name : fragment.node.id;
        const int meshNode = SpawnWorldSpaceBrushMesh(scene, root, materialHandle, fragment, nodeName);
        if (meshNode != kInvalidHandle) {
            result.meshNodes.push_back(meshNode);
            ++result.compiledFragmentCount;
        }
    }

    for (const ri::structural::StructuralNode& passthrough : compiled.passthroughNodes) {
        if (ShouldSkipAssemblyPassthroughNode(passthrough)) {
            continue;
        }
        const std::string resolvedType = ri::structural::NormalizeStructuralPrimitiveTypeKey(
            !passthrough.primitiveType.empty() ? passthrough.primitiveType : passthrough.type);
        if (!ri::structural::IsNativeStructuralPrimitive(resolvedType)) {
            continue;
        }

        const int meshNode = SpawnPassthroughPrimitiveFromNode(scene, options, root, passthrough);
        if (meshNode != kInvalidHandle) {
            result.meshNodes.push_back(meshNode);
            ++result.passthroughCount;
        }
    }

    result.root = root;
    return result;
}

} // namespace ri::scene
