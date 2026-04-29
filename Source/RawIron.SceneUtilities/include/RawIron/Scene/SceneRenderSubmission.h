#pragma once

#include "RawIron/Core/RenderCommandStream.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/PhotoModeCamera.h"
#include "RawIron/Scene/Scene.h"

#include <cstddef>
#include <optional>

namespace ri::scene {

struct SceneRenderSubmissionOptions {
    int viewportWidth = 1280;
    int viewportHeight = 720;
    ri::math::Vec3 clearColor{0.05f, 0.07f, 0.10f};
    float clearAlpha = 1.0f;
    std::uint8_t clearPassIndex = 0;
    std::uint8_t drawPassIndex = 1;
    std::uint16_t utilityPipelineBucket = 0;
    std::uint16_t litPipelineBucket = 10;
    std::uint16_t unlitPipelineBucket = 20;
    std::uint16_t transparentPipelineBucket = 30;
    bool enableFrustumCulling = true;
    bool enableFarHorizon = false;
    float farHorizonStartDistance = 90.0f;
    float farHorizonEndDistance = 260.0f;
    float farHorizonMaxDistance = 600.0f;
    std::uint8_t farHorizonMaxNodeStride = 3;
    std::uint8_t farHorizonMaxInstanceStride = 5;
    bool farHorizonCullTransparent = true;
    bool enableCoarseOcclusion = false;
    int coarseOcclusionGridWidth = 64;
    int coarseOcclusionGridHeight = 36;
    float coarseOcclusionDepthBias = 0.6f;
    /// Wider or explicit FOV for capture-style frames (`BuildSceneRenderSubmission` / Vulkan bridge).
    PhotoModeCameraOverrides photoMode{};
};

struct SceneRenderSubmissionStats {
    int cameraNodeHandle = kInvalidHandle;
    std::size_t drawCommandCount = 0;
    std::size_t skippedNodes = 0;
};

struct SceneRenderSubmission {
    ri::core::RenderCommandStream commands{};
    SceneRenderSubmissionStats stats{};
};

[[nodiscard]] std::optional<int> ResolveSubmissionCameraNode(const Scene& scene,
                                                             int preferredCameraNodeHandle = kInvalidHandle);
[[nodiscard]] ri::math::Mat4 ComputeCameraViewProjection(const Scene& scene,
                                                         int cameraNodeHandle,
                                                         float aspectRatio,
                                                         const PhotoModeCameraOverrides* photoMode = nullptr);
/// Column-major view matrix for the active camera node (includes translation).
[[nodiscard]] ri::math::Mat4 BuildCameraViewMatrix(const Scene& scene, int cameraNodeHandle);
/// Column-major perspective projection for the active camera node.
[[nodiscard]] ri::math::Mat4 BuildCameraProjectionMatrix(const Scene& scene,
                                                         int cameraNodeHandle,
                                                         float aspectRatio,
                                                         const PhotoModeCameraOverrides* photoMode = nullptr);
/// View matrix with translation removed (for native skybox rendering).
[[nodiscard]] ri::math::Mat4 BuildCameraSkyRotationMatrix(const Scene& scene, int cameraNodeHandle);
[[nodiscard]] SceneRenderSubmission BuildSceneRenderSubmission(
    const Scene& scene,
    int cameraNodeHandle = kInvalidHandle,
    const SceneRenderSubmissionOptions& options = {});

} // namespace ri::scene
