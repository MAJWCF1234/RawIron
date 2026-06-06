#pragma once

#include "RawIron/Scene/PrimitiveTypeCanonical.h"
#include "RawIron/Scene/RuntimeMeshFactory.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Structural/StructuralPrimitives.h"

#include <optional>
#include <string>
#include <vector>

namespace ri::scene {

/// Single pipeline: structural authored primitive → GPU mesh + material + node (shared by levels and tooling).
struct StructuralPrimitiveBundleParams {
    std::optional<std::string> presetField{};
    std::optional<std::string> primitiveTypeField{};
    std::optional<std::string> typeAliasField{};
    std::string nodeName = "StructuralPrimitive";
    int parent = kInvalidHandle;
    Transform transform{};
    ri::structural::StructuralPrimitiveOptions shape{};
    RuntimeMaterialParams material{};
};

struct StructuralPrimitiveBundleResult {
    int node = kInvalidHandle;
    int mesh = kInvalidHandle;
    int material = kInvalidHandle;
};

struct StructuralGalleryMaterialPreset {
    std::string label;
    RuntimeMaterialParams material{};
};

struct StructuralPrimitiveGalleryOptions {
    int parent = kInvalidHandle;
    Transform transform{};
    ri::math::Vec2 cellSpacing{4.2f, 5.0f};
    ri::math::Vec3 itemScale{2.2f, 2.2f, 2.2f};
    ri::math::Vec3 platformMargin{7.0f, 0.0f, 7.0f};
    float platformThickness = 0.7f;
    /// Approximate world-space size of one texture repeat on the gallery platform (meters).
    float platformTileWorldSize = 2.0f;
    bool includePlatform = true;
    std::string nodeNamePrefix = "StructuralPrimitiveGallery";
    std::vector<StructuralGalleryMaterialPreset> materialRows{};
};

struct StructuralPrimitiveGalleryResult {
    int root = kInvalidHandle;
    int platform = kInvalidHandle;
    int spawned = 0;
    int failed = 0;
    int columns = 0;
    int rows = 0;
    ri::math::Vec3 boundsCenter{};
    ri::math::Vec3 boundsExtents{};
};

/// Multi-node structural graph + material — same spawn pipeline as `SpawnStructuralPrimitiveBundle`.
struct StructuralPrimitiveAssemblyParams {
    std::string rootNodeName = "StructuralPrimitiveAssembly";
    int parent = kInvalidHandle;
    Transform transform{};
    std::vector<ri::structural::StructuralNode> nodes;
    ri::structural::StructuralCompileOptions compileOptions{};
    RuntimeMaterialParams material{};
};

[[nodiscard]] StructuralPrimitiveBundleResult SpawnStructuralPrimitiveBundle(Scene& scene,
                                                                             const StructuralPrimitiveBundleParams& params);
[[nodiscard]] StructuralPrimitiveAssemblyResult SpawnStructuralPrimitiveAssembly(
    Scene& scene,
    const StructuralPrimitiveAssemblyParams& params);
[[nodiscard]] std::vector<StructuralGalleryMaterialPreset> BuildDefaultStructuralGalleryMaterials();
[[nodiscard]] StructuralPrimitiveGalleryResult SpawnStructuralPrimitiveGallery(Scene& scene,
                                                                               const StructuralPrimitiveGalleryOptions& options);

/// Sandbox showcase: hollow shell with subtract cuts, perforated partition, ramp, and arch.
[[nodiscard]] std::vector<ri::structural::StructuralNode> BuildSandboxStructuralHallNodes();
[[nodiscard]] StructuralPrimitiveAssemblyResult SpawnSandboxStructuralHall(Scene& scene,
                                                                           int parent,
                                                                           ri::math::Vec3 worldOrigin,
                                                                           std::string_view rootName = "SandboxStructuralHall");

} // namespace ri::scene
