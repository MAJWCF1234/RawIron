#pragma once

#include "RawIron/Scene/PrimitiveTypeCanonical.h"
#include "RawIron/Scene/RuntimeMeshFactory.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Structural/StructuralPrimitives.h"

namespace ri::scene {

/// Single pipeline: structural authored primitive → GPU mesh + material + node (shared by levels and tooling).
struct StructuralPrimitiveBundleParams {
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

[[nodiscard]] StructuralPrimitiveBundleResult SpawnStructuralPrimitiveBundle(Scene& scene,
                                                                             const StructuralPrimitiveBundleParams& params);

} // namespace ri::scene
