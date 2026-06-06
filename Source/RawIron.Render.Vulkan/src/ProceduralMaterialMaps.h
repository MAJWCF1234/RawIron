#pragma once

#include "RawIron/Render/PreviewTexture.h"

#include <cstdint>

namespace ri::render::vulkan {

using ri::render::software::RgbaImage;

/// Tuning for deriving a tangent-space normal map from an albedo/height image.
/// The albedo *linear* luminance is treated as a height field and a Sobel
/// operator estimates the surface slope, like common "normal map from heightmap"
/// tools. This is the CPU reference path; the GPU compute generator produces
/// equivalent output and is preferred when available.
struct ProceduralNormalMapOptions {
    float strength = 1.2f;       ///< Image-gradient amplification (not world height).
    float blur = 0.0f;           ///< 0..1 pre-smoothing of the height field.
    bool wrap = true;            ///< Sample edges as tiling (true) or clamped (false).
    bool invertHeight = false;   ///< Treat dark pixels as raised instead of recessed.
    float heightBias = 0.0f;     ///< Offset applied before scaling the height field.
    float heightScale = 1.0f;    ///< Contrast applied to the height field.
};

/// Tuning for deriving an occlusion/roughness/metallic (ORM) map from albedo.
/// R = ambient occlusion (cavity), G = roughness, B = metallic.
struct ProceduralOrmMapOptions {
    float baseRoughness = 0.85f;       ///< Roughness when no surface detail is present.
    float baseMetallic = 0.0f;         ///< Metallic channel (usually author-driven).
    float aoStrength = 0.6f;           ///< 0..1 strength of derived cavity occlusion.
    float roughnessDetail = 0.25f;     ///< How much local micro-contrast modulates roughness.
    bool wrap = true;                  ///< Sample edges as tiling (true) or clamped (false).
};

/// Bumped whenever the generation math changes so on-disk caches are invalidated.
inline constexpr int kProceduralMapGeneratorVersion = 2;

/// Stable generator identity folded into the procedural-map cache key. Bump when the
/// generation math changes so caches invalidate exactly once -- never on an unrelated
/// recompile (a volatile build-time hash would needlessly regenerate every map).
[[nodiscard]] std::uint32_t ProceduralGeneratorBuildId();

/// Derives a tangent-space normal map (RGBA8) from an albedo image. Fully transparent
/// albedo pixels emit a neutral, transparent normal so cutout cards do not gain relief.
[[nodiscard]] RgbaImage GenerateNormalMapFromAlbedo(const RgbaImage& albedo,
                                                    const ProceduralNormalMapOptions& options = {});

/// Derives an ORM map (RGBA8: r=AO, g=roughness, b=metallic) from an albedo image.
/// Fully transparent albedo pixels emit neutral occlusion/roughness/metallic.
[[nodiscard]] RgbaImage GenerateOrmMapFromAlbedo(const RgbaImage& albedo,
                                                 const ProceduralOrmMapOptions& options = {});

/// Derives an ORM map by cross-referencing an authored tangent-space normal map.
/// Occlusion comes from local normal curvature (concavities self-occlude) and
/// roughness from surface slope plus high-frequency normal detail, which is far
/// more faithful than luminance-only cavity estimation. Prefer this whenever a
/// real normal map is available (authored or auto-discovered) and an ORM is missing.
[[nodiscard]] RgbaImage GenerateOrmMapFromNormal(const RgbaImage& normal,
                                                 const ProceduralOrmMapOptions& options = {});

/// Solid neutral maps for materials that should not be procedurally hydrated
/// (decals, UI, sky) while keeping the binding path uniform.
[[nodiscard]] RgbaImage GenerateNeutralNormalMap(int width, int height);
[[nodiscard]] RgbaImage GenerateNeutralOrmMap(int width, int height, float roughness, float metallic);

} // namespace ri::render::vulkan
