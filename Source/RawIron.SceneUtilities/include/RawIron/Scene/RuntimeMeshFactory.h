#pragma once

#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/Transform.h"

#include <string>
#include <string_view>
#include <vector>

namespace ri::scene {

class Scene;
class LevelObjectRegistry;

/// Authoring-friendly material fields with safe defaults; maps to \ref Material on creation.
struct RuntimeMaterialParams {
    std::string materialName = "RuntimeMaterial";
    ShadingModel shadingModel = ShadingModel::Lit;
    ri::math::Vec3 baseColor{1.0f, 1.0f, 1.0f};
    std::string baseColorTexture{};
    std::vector<std::string> baseColorTextureFrames{};
    float baseColorTextureFramesPerSecond = 0.0f;
    ri::math::Vec2 textureTiling{1.0f, 1.0f};
    ri::math::Vec3 emissiveColor{0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float opacity = 1.0f;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool transparent = false;
};

/// Loose primitive description for tools + gameplay (same path as \ref AddPrimitiveNode).
struct RuntimePrimitiveParams {
    std::string nodeName = "RuntimePrimitive";
    int parent = kInvalidHandle;
    Transform transform{};
    /// When non-empty, overrides \ref primitive with parsed tokens (\ref ParsePrimitiveKindLoose).
    std::string primitiveKind{};
    PrimitiveType primitive = PrimitiveType::Cube;
    RuntimeMaterialParams material{};
    /// When set, successful registration into \p registry after node creation.
    std::string registryId{};
};

/// Case-insensitive token → \ref PrimitiveType. Unknown or empty → \p fallback.
[[nodiscard]] PrimitiveType ParsePrimitiveKindLoose(std::string_view token, PrimitiveType fallback = PrimitiveType::Cube);

/// Single-path primitive mesh + material + node (uses \ref AddPrimitiveNode). Optionally registers \p params.registryId.
[[nodiscard]] int InstantiateRuntimePrimitive(Scene& scene,
                                              const RuntimePrimitiveParams& params,
                                              LevelObjectRegistry* registry = nullptr);

} // namespace ri::scene
