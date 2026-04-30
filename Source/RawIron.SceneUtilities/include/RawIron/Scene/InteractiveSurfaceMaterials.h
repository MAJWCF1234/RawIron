#pragma once

#include "RawIron/Scene/Components.h"

#include <string>

namespace ri::scene {

/// Authoring helpers for interactive props (doors, panels, terminals): consistent UV tiling and optional emissive that
/// reuses the base UV layout when maps are absent.
[[nodiscard]] inline Material MakeInteractiveSurfaceMaterial(std::string_view name,
                                                             const ri::math::Vec3& baseColor,
                                                             float roughness,
                                                             float metallic,
                                                             std::string_view baseColorTexture = {},
                                                             const ri::math::Vec2& uvRepeat = {1.0f, 1.0f}) {
    Material m{};
    m.name = std::string(name);
    m.shadingModel = ShadingModel::Lit;
    m.baseColor = baseColor;
    m.roughness = roughness;
    m.metallic = metallic;
    m.textureTiling = uvRepeat;
    if (!baseColorTexture.empty()) {
        m.baseColorTexture = std::string(baseColorTexture);
    }
    return m;
}

[[nodiscard]] inline Material MakePaintedMetalPanelMaterial(std::string_view name,
                                                            const ri::math::Vec3& paintTint,
                                                            std::string_view scratchedAlbedo = {},
                                                            const ri::math::Vec2& uvRepeat = {2.0f, 2.0f}) {
    return MakeInteractiveSurfaceMaterial(name, paintTint, 0.42f, 0.65f, scratchedAlbedo, uvRepeat);
}

[[nodiscard]] inline Material MakePlasticPanelMaterial(std::string_view name,
                                                       const ri::math::Vec3& color,
                                                       std::string_view noiseAlbedo = {},
                                                       const ri::math::Vec2& uvRepeat = {3.0f, 3.0f}) {
    return MakeInteractiveSurfaceMaterial(name, color, 0.78f, 0.02f, noiseAlbedo, uvRepeat);
}

[[nodiscard]] inline Material MakeEmissiveAccentMaterial(std::string_view name,
                                                         const ri::math::Vec3& baseColor,
                                                         const ri::math::Vec3& emissiveColor,
                                                         std::string_view emissiveMaskTexture = {},
                                                         const ri::math::Vec2& uvRepeat = {1.0f, 1.0f}) {
    Material m = MakeInteractiveSurfaceMaterial(name, baseColor, 0.55f, 0.08f, {}, uvRepeat);
    m.emissiveColor = emissiveColor;
    if (!emissiveMaskTexture.empty()) {
        m.emissiveTexture = std::string(emissiveMaskTexture);
    }
    return m;
}

} // namespace ri::scene
