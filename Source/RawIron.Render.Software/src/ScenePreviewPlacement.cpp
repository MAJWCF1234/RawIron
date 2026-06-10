#include "RawIron/Render/ScenePreviewPlacement.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/PhotoModeCamera.h"
#include "RawIron/Spatial/Aabb.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ri::render::software {
namespace {

struct CameraViewBasis {
    float plotLeft = 0.0f;
    float plotTop = 0.0f;
    float plotWidth = 1.0f;
    float plotHeight = 1.0f;
    float aspectRatio = 1.0f;
    float tanHalfFovX = 1.0f;
    float tanHalfFovY = 1.0f;
    float nearClip = 0.05f;
    float farClip = 1000.0f;
    ri::math::Vec3 position{};
    ri::math::Vec3 right{};
    ri::math::Vec3 up{};
    ri::math::Vec3 forward{};
};

[[nodiscard]] std::optional<CameraViewBasis> BuildCameraViewBasis(const CameraViewRect& plot,
                                                                  const ri::scene::Scene& scene,
                                                                  const int cameraNodeHandle) {
    if (plot.right <= plot.left + 4 || plot.bottom <= plot.top + 4
        || cameraNodeHandle == ri::scene::kInvalidHandle
        || cameraNodeHandle >= static_cast<int>(scene.NodeCount())) {
        return std::nullopt;
    }

    const ri::scene::Node& cameraNode = scene.GetNode(cameraNodeHandle);
    if (cameraNode.camera == ri::scene::kInvalidHandle) {
        return std::nullopt;
    }

    const ri::scene::Camera& camera = scene.GetCamera(cameraNode.camera);
    CameraViewBasis basis{};
    basis.plotLeft = static_cast<float>(plot.left);
    basis.plotTop = static_cast<float>(plot.top);
    basis.plotWidth = static_cast<float>(plot.right - plot.left);
    basis.plotHeight = static_cast<float>(plot.bottom - plot.top);
    basis.aspectRatio = basis.plotWidth / std::max(basis.plotHeight, 1.0f);
    const float fieldOfViewDegrees =
        ri::scene::ResolvePhotoModeFieldOfViewDegrees(camera.fieldOfViewDegrees, {}, basis.aspectRatio);
    basis.tanHalfFovY = std::tan(ri::math::DegreesToRadians(fieldOfViewDegrees * 0.5f));
    basis.tanHalfFovX = basis.tanHalfFovY * basis.aspectRatio;
    basis.nearClip = std::max(camera.nearClip, 0.01f);
    basis.farClip = std::max(basis.nearClip + 0.01f, camera.farClip);

    const ri::math::Mat4 worldMatrix = scene.ComputeWorldMatrix(cameraNodeHandle);
    basis.position = ri::math::ExtractTranslation(worldMatrix);
    basis.right = ri::math::ExtractRight(worldMatrix);
    basis.up = ri::math::ExtractUp(worldMatrix);
    basis.forward = ri::math::ExtractForward(worldMatrix);
    return basis;
}

[[nodiscard]] ri::math::Vec3 RayDirectionFromMouse(const CameraViewBasis& basis, const int mouseX, const int mouseY) {
    const float ndcX = ((static_cast<float>(mouseX) - basis.plotLeft) / basis.plotWidth) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((static_cast<float>(mouseY) - basis.plotTop) / basis.plotHeight) * 2.0f;
    return ri::math::Normalize(basis.forward + (basis.right * (ndcX * basis.tanHalfFovX))
                               + (basis.up * (ndcY * basis.tanHalfFovY)));
}

struct ScenePreviewCameraBasis {
    ri::math::Vec3 position{};
    ri::math::Vec3 right{};
    ri::math::Vec3 up{};
    ri::math::Vec3 forward{};
    float focalLength = 1.0f;
    float aspectRatio = 1.0f;
    float nearClip = 0.05f;
    float farClip = 1000.0f;
    int width = 1;
    int height = 1;
};

[[nodiscard]] std::optional<ScenePreviewCameraBasis> BuildScenePreviewCameraBasis(
    const ri::scene::Scene& scene,
    const int cameraNodeHandle,
    const ScenePreviewOptions& options) {
    if (options.width <= 0 || options.height <= 0 || cameraNodeHandle == ri::scene::kInvalidHandle
        || cameraNodeHandle >= static_cast<int>(scene.NodeCount())) {
        return std::nullopt;
    }
    const ri::scene::Node& cameraNode = scene.GetNode(cameraNodeHandle);
    if (cameraNode.camera == ri::scene::kInvalidHandle) {
        return std::nullopt;
    }
    const ri::scene::Camera& camera = scene.GetCamera(cameraNode.camera);
    ScenePreviewCameraBasis basis{};
    basis.width = options.width;
    basis.height = options.height;
    basis.aspectRatio = static_cast<float>(options.width) / static_cast<float>(std::max(options.height, 1));
    const float fieldOfViewDegrees = ri::scene::ResolvePhotoModeFieldOfViewDegrees(
        camera.fieldOfViewDegrees, options.photoMode, basis.aspectRatio);
    basis.focalLength = 1.0f / std::tan(ri::math::DegreesToRadians(fieldOfViewDegrees * 0.5f));
    basis.nearClip = std::max(0.01f, camera.nearClip);
    basis.farClip = std::max(basis.nearClip + 0.01f, camera.farClip);
    const ri::math::Mat4 worldMatrix = scene.ComputeWorldMatrix(cameraNodeHandle);
    basis.position = ri::math::ExtractTranslation(worldMatrix);
    basis.right = ri::math::ExtractRight(worldMatrix);
    basis.up = ri::math::ExtractUp(worldMatrix);
    basis.forward = ri::math::ExtractForward(worldMatrix);
    return basis;
}

[[nodiscard]] ri::math::Vec3 ToScenePreviewCameraSpace(const ScenePreviewCameraBasis& camera,
                                                       const ri::math::Vec3& worldPoint) {
    const ri::math::Vec3 offset = worldPoint - camera.position;
    return ri::math::Vec3{
        ri::math::Dot(offset, camera.right),
        ri::math::Dot(offset, camera.up),
        ri::math::Dot(offset, camera.forward),
    };
}

struct ScenePreviewScreenVertex {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    bool visible = false;
};

[[nodiscard]] ScenePreviewScreenVertex ProjectWorldToScenePreviewImage(const ScenePreviewCameraBasis& camera,
                                                                       const ri::math::Vec3& worldPoint) {
    const ri::math::Vec3 view = ToScenePreviewCameraSpace(camera, worldPoint);
    if (view.z <= camera.nearClip || view.z >= camera.farClip) {
        return {.depth = view.z, .visible = false};
    }
    const float ndcX = (view.x / view.z) * (camera.focalLength / camera.aspectRatio);
    const float ndcY = (view.y / view.z) * camera.focalLength;
    return ScenePreviewScreenVertex{
        .x = (ndcX * 0.5f + 0.5f) * static_cast<float>(camera.width) - 0.5f,
        .y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(camera.height) - 0.5f,
        .depth = view.z,
        .visible = true,
    };
}

void PlotOverlayPixel(SoftwareImage& image,
                      std::vector<float>& depthBuffer,
                      const int x,
                      const int y,
                      const float depth,
                      const ri::math::Vec3& lineColor) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        return;
    }
    const std::size_t pixelIndex = static_cast<std::size_t>(y * image.width + x);
    if (pixelIndex >= depthBuffer.size() || depth >= depthBuffer[pixelIndex]) {
        return;
    }
    const std::size_t colorIndex = pixelIndex * 3U;
    if (colorIndex + 2U >= image.pixels.size()) {
        return;
    }
    const auto channel = [&](const float value) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::clamp(value * 255.0f, 0.0f, 255.0f));
    };
    image.pixels[colorIndex] = channel(lineColor.x);
    image.pixels[colorIndex + 1U] = channel(lineColor.y);
    image.pixels[colorIndex + 2U] = channel(lineColor.z);
}

void DrawDepthLine(SoftwareImage& image,
                   std::vector<float>& depthBuffer,
                   const ScenePreviewScreenVertex& start,
                   const ScenePreviewScreenVertex& end,
                   const ri::math::Vec3& lineColor) {
    if (!start.visible || !end.visible) {
        return;
    }
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float dz = end.depth - start.depth;
    const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::fabs(dx), std::fabs(dy)))));
    for (int step = 0; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        PlotOverlayPixel(image,
                         depthBuffer,
                         static_cast<int>(std::lround(start.x + dx * t)),
                         static_cast<int>(std::lround(start.y + dy * t)),
                         start.depth + dz * t,
                         lineColor);
    }
}

} // namespace

std::optional<ri::math::Vec3> PickPlacementPointInCameraView(const CameraViewRect& plot,
                                                              const int mouseX,
                                                              const int mouseY,
                                                              const ri::scene::Scene& scene,
                                                              const int cameraNodeHandle) {
    const std::optional<CameraViewBasis> basis = BuildCameraViewBasis(plot, scene, cameraNodeHandle);
    if (!basis.has_value()) {
        return std::nullopt;
    }

    const ri::spatial::Ray ray{
        .origin = basis->position,
        .direction = RayDirectionFromMouse(*basis, mouseX, mouseY),
    };

    float bestDistance = std::numeric_limits<float>::infinity();
    ri::math::Vec3 bestPoint{};
    bool found = false;
    for (const int handle : ri::scene::CollectRenderableNodes(scene)) {
        const std::optional<ri::scene::WorldBounds> bounds =
            ri::scene::ComputeNodeWorldBounds(scene, handle, true);
        if (!bounds.has_value()) {
            continue;
        }
        const ri::spatial::Aabb box{
            .min = bounds->min,
            .max = bounds->max,
        };
        float hitDistance = 0.0f;
        if (!ri::spatial::IntersectRayAabb(ray, box, basis->farClip, &hitDistance)) {
            continue;
        }
        if (hitDistance < bestDistance) {
            bestDistance = hitDistance;
            bestPoint = ray.origin + ray.direction * hitDistance;
            found = true;
        }
    }

    constexpr float kGroundPlaneY = 0.0f;
    if (std::abs(ray.direction.y) > 1.0e-5f) {
        const float groundT = (kGroundPlaneY - ray.origin.y) / ray.direction.y;
        if (groundT > 0.0f && groundT < basis->farClip && (!found || groundT < bestDistance)) {
            bestPoint = ray.origin + ray.direction * groundT;
            found = true;
        }
    }

    if (!found) {
        return std::nullopt;
    }
    return bestPoint;
}

std::optional<CameraViewScreenPoint> ProjectWorldPointToCameraView(const CameraViewRect& plot,
                                                                     const ri::math::Vec3& worldPoint,
                                                                     const ri::scene::Scene& scene,
                                                                     const int cameraNodeHandle) {
    const std::optional<CameraViewBasis> basis = BuildCameraViewBasis(plot, scene, cameraNodeHandle);
    if (!basis.has_value()) {
        return std::nullopt;
    }

    const ri::math::Vec3 relative = worldPoint - basis->position;
    const float depth = ri::math::Dot(relative, basis->forward);
    if (depth <= basis->nearClip || depth > basis->farClip) {
        return CameraViewScreenPoint{
            .depth = depth,
            .inFront = false,
        };
    }

    const float ndcX = (ri::math::Dot(relative, basis->right) / depth) / basis->tanHalfFovX;
    const float ndcY = (ri::math::Dot(relative, basis->up) / depth) / basis->tanHalfFovY;
    const float screenX = basis->plotLeft + ((ndcX * 0.5f) + 0.5f) * basis->plotWidth;
    const float screenY = basis->plotTop + (1.0f - ((ndcY * 0.5f) + 0.5f)) * basis->plotHeight;
    return CameraViewScreenPoint{
        .x = screenX,
        .y = screenY,
        .depth = depth,
        .inFront = true,
    };
}

void DrawWireBoxOverlayIntoScenePreview(SoftwareImage& image,
                                        std::vector<float>& depthBuffer,
                                        const ri::scene::Scene& scene,
                                        const int cameraNodeHandle,
                                        const ScenePreviewOptions& options,
                                        const ri::math::Vec3& center,
                                        const ri::math::Vec3& halfExtents,
                                        const ri::math::Vec3& lineColor) {
    if (image.width <= 0 || image.height <= 0 || depthBuffer.empty()) {
        return;
    }
    const std::optional<ScenePreviewCameraBasis> camera =
        BuildScenePreviewCameraBasis(scene, cameraNodeHandle, options);
    if (!camera.has_value()) {
        return;
    }

    static constexpr std::array<ri::math::Vec3, 8> kCornerSigns = {{
        {-1.0f, -1.0f, -1.0f},
        {1.0f, -1.0f, -1.0f},
        {1.0f, 1.0f, -1.0f},
        {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 1.0f, 1.0f},
    }};
    static constexpr std::array<std::pair<int, int>, 12> kEdges = {{
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    }};

    std::array<ScenePreviewScreenVertex, 8> projected{};
    for (std::size_t cornerIndex = 0; cornerIndex < kCornerSigns.size(); ++cornerIndex) {
        const ri::math::Vec3& signs = kCornerSigns[cornerIndex];
        const ri::math::Vec3 corner = center + ri::math::Vec3{signs.x * halfExtents.x,
                                                              signs.y * halfExtents.y,
                                                              signs.z * halfExtents.z};
        projected[cornerIndex] = ProjectWorldToScenePreviewImage(*camera, corner);
    }

    for (const std::pair<int, int>& edge : kEdges) {
        DrawDepthLine(image,
                      depthBuffer,
                      projected[static_cast<std::size_t>(edge.first)],
                      projected[static_cast<std::size_t>(edge.second)],
                      lineColor);
    }
}

} // namespace ri::render::software
