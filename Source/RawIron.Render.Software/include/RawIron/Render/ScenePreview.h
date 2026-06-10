#pragma once

#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Scene/PhotoModeCamera.h"
#include "RawIron/Scene/Scene.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ri::render::software {

enum class ScenePreviewRenderer {
    Raster,
    RayTrace,
};

struct ScenePreviewOptions {
    int width = 768;
    int height = 768;
    /// Muted late-90s clear / fog (CRT-friendly, low-bit-depth feel).
    ri::math::Vec3 clearTop{0.04f, 0.06f, 0.09f};
    ri::math::Vec3 clearBottom{0.10f, 0.12f, 0.16f};
    ri::math::Vec3 fogColor{0.14f, 0.15f, 0.18f};
    /// Distance / horizon fog tint; defaults to fogColor when left at zero.
    ri::math::Vec3 fogColorFar{};
    ri::math::Vec3 ambientLight{0.10f, 0.11f, 0.13f};
    float fogStartDepth = 3.0f;
    float fogEndDepth = 17.0f;
    float fogStrength = 0.72f;
    /// Lightweight editor preview color grade from `scripts/postprocess.riscript` / `rendering.riscript`.
    float previewExposure = 1.0f;
    float previewContrast = 1.0f;
    float previewSaturation = 1.0f;
    float previewVignetteStrength = 0.0f;
    float previewBloomStrength = 0.0f;
    float previewSharpenAmount = 0.0f;
    float previewTintStrength = 0.0f;
    ri::math::Vec3 previewTintColor{1.0f, 1.0f, 1.0f};
    /// Override directory for `Material::baseColorTexture` filenames. If unset, uses the
    /// canonical `Assets/Textures` folder from the RawIron tree (legacy: `Engine/Textures`).
    std::optional<std::filesystem::path> textureRoot{};
    /// Optional animation clock for frame-sequence materials.
    double animationTimeSeconds = 0.0;
    bool pointSampleTextures = true;
    /// When true and point sampling is otherwise disabled, switches to cheaper point samples in far depth ranges.
    bool adaptiveTextureSampling = true;
    float adaptivePointSampleStartDepth = 40.0f;
    /// Screen-space affine UVs (PS1-style swimming on large tris). Off by default so hall-scale
    /// quads stay stable; set true for deliberate retro warping.
    bool affineTextureMapping = false;
    /// Subtle ordered dither toward 5-bit channels.
    bool orderedDither = true;
    /// Opt-in profile for very old CPUs/GPUs: cheaper sampling, no dither, and distance thinning.
    bool lowSpecMode = false;
    /// Distance-tiered thinning/culling to keep full-res software rendering responsive in large halls.
    bool enableFarHorizon = false;
    float farHorizonStartDistance = 70.0f;
    float farHorizonEndDistance = 180.0f;
    float farHorizonMaxDistance = 320.0f;
    std::uint32_t farHorizonMaxNodeStride = 4U;
    std::uint32_t farHorizonMaxInstanceStride = 6U;
    /// Optional editor/tooling nodes to hide from the camera render (for example helper grid / axes meshes).
    std::vector<int> hiddenNodeHandles{};
    /// Optional FOV for still captures: vertical scale/override, or horizontal override via
    /// `PhotoModeCameraOverrides::fieldOfViewOverrideIsHorizontal` (matches Vulkan `SceneRenderSubmissionOptions::photoMode`).
    ri::scene::PhotoModeCameraOverrides photoMode{};
    /// Primary software preview path. RayTrace gives Bryce-style soft shadows, sky fill, and reflections.
    ScenePreviewRenderer renderer = ScenePreviewRenderer::Raster;
    /// Internal ray resolution = width/height * scale (upscaled to output). 0.5–0.75 keeps the editor responsive.
    float rayTracingResolutionScale = 0.68f;
    int rayTracingMaxBounces = 2;
    int rayTracingShadowRays = 4;
    /// Angular radius (radians) of the directional sun disk for soft shadow penumbra.
    float rayTracingSunRadius = 0.045f;
    bool rayTracingReflections = true;
    /// Subpixel jitter samples per pixel (1, 2, or 4). 2 is a good editor default.
    int rayTracingSamplesPerPixel = 2;
    bool rayTracingAmbientOcclusion = true;
    int rayTracingAmbientOcclusionRays = 3;
    float rayTracingAmbientOcclusionRadius = 0.55f;
    float rayTracingAmbientOcclusionStrength = 0.42f;
    bool rayTracingNormalMaps = true;
    /// Row-parallel trace when height is at least this many pixels.
    int rayTracingParallelRowsThreshold = 96;
};

struct ScenePreviewMeshCullBounds {
    ri::math::Vec3 center{};
    float radius = 0.0f;
    bool valid = false;
};

struct ScenePreviewRayTraceBvhNode {
    ri::math::Vec3 boundsMin{};
    ri::math::Vec3 boundsMax{};
    int left = -1;
    int right = -1;
    int triStart = 0;
    int triCount = 0;
};

struct ScenePreviewRayTraceScene {
    std::uint64_t geometryStamp = 0;
    std::vector<ri::math::Vec3> triV0{};
    std::vector<ri::math::Vec3> triV1{};
    std::vector<ri::math::Vec3> triV2{};
    std::vector<ri::math::Vec3> triN0{};
    std::vector<ri::math::Vec3> triN1{};
    std::vector<ri::math::Vec3> triN2{};
    std::vector<ri::math::Vec2> triUv0{};
    std::vector<ri::math::Vec2> triUv1{};
    std::vector<ri::math::Vec2> triUv2{};
    std::vector<int> triMaterial{};
};

struct ScenePreviewCache {
    std::unordered_map<std::string, RgbaImage> textures{};
    std::vector<float> depthBuffer{};
    std::vector<std::optional<ScenePreviewMeshCullBounds>> meshCullBounds{};
    ScenePreviewRayTraceScene rayTraceScene{};
    std::vector<ScenePreviewRayTraceBvhNode> rayTraceBvh{};
    std::vector<int> rayTraceBvhOrder{};
    std::uint64_t rayTraceBvhStamp = 0;
};

/// Delegates to `ri::content::ResolveEngineTexturesDirectory` (see `EngineAssets.h`).
[[nodiscard]] std::filesystem::path DefaultEngineTextureRoot();

[[nodiscard]] SoftwareImage RenderScenePreview(const ri::scene::Scene& scene,
                                               int cameraNodeHandle,
                                               const ScenePreviewOptions& options = {},
                                               ScenePreviewCache* cache = nullptr);

/// Renders into `outImage`, resizing pixel storage only when width/height change.
void RenderScenePreviewInto(const ri::scene::Scene& scene,
                            int cameraNodeHandle,
                            const ScenePreviewOptions& options,
                            SoftwareImage& outImage,
                            ScenePreviewCache* cache = nullptr);

void RenderScenePreviewRayTraceInto(const ri::scene::Scene& scene,
                                    int cameraNodeHandle,
                                    const ScenePreviewOptions& options,
                                    SoftwareImage& outImage,
                                    ScenePreviewCache* cache = nullptr);

} // namespace ri::render::software
