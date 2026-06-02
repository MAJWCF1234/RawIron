#pragma once

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Structural/StructuralPrimitives.h"

namespace ri::scene {

/// Converts triangle-soup structural output into a `PrimitiveType::Custom` mesh for the scene renderer.
[[nodiscard]] Mesh MeshFromStructuralCompiledMesh(const ri::structural::CompiledMesh& compiled,
                                                   std::string meshName);

/// Placement and material defaults for `ri::structural::BuildPrimitiveMesh`.
struct StructuralBrushSpawnOptions {
    std::string nodeName = "StructuralBrush";
    std::string_view structuralType = "box";
    ri::structural::StructuralPrimitiveOptions shape{};
    int parent = kInvalidHandle;
    Transform transform{};
    std::string materialName = "struct_brush";
    ShadingModel shadingModel = ShadingModel::Lit;
    MaterialStyle materialStyle = MaterialStyle::Standard;
    MaterialWorkflow materialWorkflow = MaterialWorkflow::MetalRough;
    ri::math::Vec3 baseColor{0.62f, 0.66f, 0.72f};
    std::string baseColorTexture{};
    std::string normalTexture{};
    std::string ormTexture{};
    std::string roughnessTexture{};
    std::string metallicTexture{};
    std::string emissiveTexture{};
    std::string opacityTexture{};
    std::string occlusionTexture{};
    std::string detailTexture{};
    ri::math::Vec2 textureTiling{2.0f, 2.0f};
    ri::math::Vec3 emissiveColor{0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float opacity = 1.0f;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool transparent = false;
    bool additiveBlend = false;
};

/// Instantiates the structural primitive as **`PrimitiveType::Custom`** geometry under `parent`.
/// Returns [`kInvalidHandle`] when the structural compiler produced no geometry.
[[nodiscard]] int AddStructuralBrushNode(Scene& scene, const StructuralBrushSpawnOptions& options);

} // namespace ri::scene
