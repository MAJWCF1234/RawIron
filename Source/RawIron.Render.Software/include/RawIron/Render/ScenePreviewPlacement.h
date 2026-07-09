#pragma once

#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Scene/Scene.h"

#include <optional>
#include <vector>

namespace ri::render::software {

struct CameraViewRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct CameraViewScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    bool inFront = false;
};

/// Perspective camera ray expressed in world space, including the camera clipping range that
/// viewport placement and semantic queries must respect.
struct CameraViewRay {
    ri::scene::Ray ray{};
    float nearClip = 0.05f;
    float farClip = 1000.0f;
};

/// Builds a world-space perspective ray for a client point inside a camera plot.
[[nodiscard]] std::optional<CameraViewRay> BuildCameraViewRay(const CameraViewRect& plot,
                                                               int mouseX,
                                                               int mouseY,
                                                               const ri::scene::Scene& scene,
                                                               int cameraNodeHandle);

/// Ray-picks a precise world placement point from a perspective camera plot (mesh geometry + y=0 ground).
[[nodiscard]] std::optional<ri::math::Vec3> PickPlacementPointInCameraView(const CameraViewRect& plot,
                                                                            int mouseX,
                                                                            int mouseY,
                                                                            const ri::scene::Scene& scene,
                                                                            int cameraNodeHandle);

/// Projects a world point into client coordinates relative to the camera plot.
[[nodiscard]] std::optional<CameraViewScreenPoint> ProjectWorldPointToCameraView(const CameraViewRect& plot,
                                                                                 const ri::math::Vec3& worldPoint,
                                                                                 const ri::scene::Scene& scene,
                                                                                 int cameraNodeHandle);

/// Draws a depth-tested wire box into a software preview image (occludes against scene geometry).
void DrawWireBoxOverlayIntoScenePreview(SoftwareImage& image,
                                        std::vector<float>& depthBuffer,
                                        const ri::scene::Scene& scene,
                                        int cameraNodeHandle,
                                        const ScenePreviewOptions& options,
                                        const ri::math::Vec3& center,
                                        const ri::math::Vec3& halfExtents,
                                        const ri::math::Vec3& lineColor = {0.44f, 0.83f, 1.0f});

} // namespace ri::render::software
