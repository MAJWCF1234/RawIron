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

struct ScenePreviewOptions {
    int width = 768;
    int height = 768;
    /// Muted late-90s clear / fog (CRT-friendly, low-bit-depth feel).
    ri::math::Vec3 clearTop{0.04f, 0.06f, 0.09f};
    ri::math::Vec3 clearBottom{0.10f, 0.12f, 0.16f};
    ri::math::Vec3 fogColor{0.14f, 0.15f, 0.18f};
    ri::math::Vec3 ambientLight{0.10f, 0.11f, 0.13f};
    /// Override directory for `Material::baseColorTexture` filenames. If unset, uses the bundled
    /// `Assets/Textures` folder from the RawIron tree (copied next to exes by CMake; legacy: `Engine/Textures`).
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
    /// Opt-in profile for very old CPUs/GPUs: cheaper sampling, affine UVs, no dither, and distance thinning.
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
};

struct ScenePreviewMeshCullBounds {
    ri::math::Vec3 center{};
    float radius = 0.0f;
    bool valid = false;
};

struct ScenePreviewCache {
    std::unordered_map<std::string, RgbaImage> textures{};
    std::vector<float> depthBuffer{};
    std::vector<std::optional<ScenePreviewMeshCullBounds>> meshCullBounds{};
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

} // namespace ri::render::software
