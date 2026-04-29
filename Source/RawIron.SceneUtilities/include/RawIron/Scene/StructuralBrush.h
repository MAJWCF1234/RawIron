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
    ri::math::Vec3 baseColor{0.62f, 0.66f, 0.72f};
    std::string baseColorTexture{};
    ri::math::Vec2 textureTiling{2.0f, 2.0f};
};

/// Instantiates the structural primitive as **`PrimitiveType::Custom`** geometry under `parent`.
/// Returns [`kInvalidHandle`] when the structural compiler produced no geometry.
[[nodiscard]] int AddStructuralBrushNode(Scene& scene, const StructuralBrushSpawnOptions& options);

} // namespace ri::scene
