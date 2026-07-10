#pragma once

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Structural/StructuralCompiler.h"
#include "RawIron/Structural/StructuralGraph.h"
#include "RawIron/Structural/StructuralPrimitives.h"

#include <string>
#include <vector>

namespace ri::scene {

/// Converts triangle-soup structural output into a `PrimitiveType::Custom` mesh for the scene renderer.
[[nodiscard]] Mesh MeshFromStructuralCompiledMesh(const ri::structural::CompiledMesh& compiled,
                                                   std::string meshName);

/// Placement and material defaults when spawning one structural primitive into the scene graph.
struct StructuralBrushSpawnOptions {
    std::string nodeName = "StructuralPrimitive";
    std::string_view structuralType = "box";
    ri::structural::StructuralPrimitiveOptions shape{};
    int parent = kInvalidHandle;
    StructuralBrushMetadata metadata{};
    Transform transform{};
    std::string materialName = "struct_primitive";
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

/// Instantiates one structural primitive as **`PrimitiveType::Custom`** geometry under `parent`.
[[nodiscard]] int AddStructuralBrushNode(Scene& scene, const StructuralBrushSpawnOptions& options);

/// World-space half extents for editor placement ghosts and previews.
[[nodiscard]] ri::math::Vec3 EstimateStructuralBrushHalfExtents(const StructuralBrushSpawnOptions& options);

/// Compile a structural graph (solids, subtract cutters, mesh primitives) and spawn under one root.
struct StructuralPrimitiveAssemblyOptions {
    int parent = kInvalidHandle;
    Transform transform{};
    std::string rootNodeName = "StructuralPrimitiveAssembly";
    std::vector<ri::structural::StructuralNode> nodes;
    ri::structural::StructuralCompileOptions compileOptions{};
    StructuralBrushSpawnOptions material{};
};

struct StructuralPrimitiveAssemblyResult {
    int root = kInvalidHandle;
    std::size_t compiledFragmentCount = 0;
    std::size_t passthroughCount = 0;
    std::vector<int> meshNodes;
    std::vector<std::string> compileWarnings;
};

struct StructuralBrushValidationReport {
    bool valid = false;
    std::vector<std::string> errors{};
    std::vector<std::string> warnings{};
};

/// Validates M/P/Q/I ownership and semantic-policy combinations for editor/build diagnostics.
[[nodiscard]] StructuralBrushValidationReport ValidateStructuralBrushMetadata(
    const StructuralBrushMetadata& metadata);

[[nodiscard]] ri::structural::StructuralNode MakeStructuralPrimitiveSolid(std::string id,
                                                                          std::string_view structuralType,
                                                                          ri::math::Vec3 position,
                                                                          ri::math::Vec3 scale,
                                                                          ri::math::Vec3 rotationDegrees = {});

[[nodiscard]] ri::structural::StructuralNode MakeStructuralPrimitiveSubtract(std::string id,
                                                                             std::string_view cutterType,
                                                                             std::vector<std::string> targetIds,
                                                                             ri::math::Vec3 position,
                                                                             ri::math::Vec3 scale,
                                                                             ri::math::Vec3 rotationDegrees = {});

[[nodiscard]] ri::structural::StructuralNode MakeStructuralPrimitiveGraphNode(
    std::string id,
    std::string_view structuralType,
    ri::math::Vec3 position,
    ri::math::Vec3 scale,
    const ri::structural::StructuralPrimitiveOptions& shape = {},
    ri::math::Vec3 rotationDegrees = {});

[[nodiscard]] StructuralPrimitiveAssemblyResult AddStructuralPrimitiveAssembly(
    Scene& scene,
    const StructuralPrimitiveAssemblyOptions& options);

/// Relative to `Assets/Textures` — LRT tuff brick albedo for editor structural stamps.
[[nodiscard]] const char* DefaultStructuralBrushAlbedoTexture();

/// Tint paired with `DefaultStructuralBrushAlbedoTexture()`.
[[nodiscard]] ri::math::Vec3 DefaultStructuralBrushBaseColor();

} // namespace ri::scene
