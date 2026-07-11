#include "RawIron/Render/ScenePreviewPlacement.h"

#include <cmath>
#include <cstdlib>
#include <limits>

int main() {
    ri::scene::Scene scene{"PlacementSmoke"};
    const ri::render::software::CameraViewRect plot{.left = 0, .top = 0, .right = 640, .bottom = 480};
    if (ri::render::software::BuildCameraViewRay(plot, 320, 240, scene, -2).has_value()
        || ri::render::software::ProjectWorldPointToCameraView(plot, {}, scene, -2).has_value()) {
        return EXIT_FAILURE;
    }

    const int cameraNode = scene.CreateNode("Camera");
    const int camera = scene.AddCamera(ri::scene::Camera{
        .name = "Camera",
        .nearClip = std::numeric_limits<float>::quiet_NaN(),
        .farClip = std::numeric_limits<float>::infinity(),
    });
    scene.AttachCamera(cameraNode, camera);
    const auto ray = ri::render::software::BuildCameraViewRay(plot, 320, 240, scene, cameraNode);
    if (!ray.has_value() || !std::isfinite(ray->nearClip) || !std::isfinite(ray->farClip)
        || ray->nearClip <= 0.0f || ray->farClip <= ray->nearClip) {
        return EXIT_FAILURE;
    }

    ri::render::software::SoftwareImage image{
        .width = 16,
        .height = 16,
        .pixels = std::vector<std::uint8_t>(16U * 16U * 3U, 0U),
    };
    std::vector<float> depthBuffer(16U * 16U, std::numeric_limits<float>::infinity());
    ri::render::software::DrawWireBoxOverlayIntoScenePreview(
        image,
        depthBuffer,
        scene,
        cameraNode,
        {.width = 16, .height = 16},
        {0.0f, 0.0f, 1.0f},
        {1'000'000.0f, 0.1f, 0.1f});

    scene.GetNode(cameraNode).camera = 99;
    if (ri::render::software::BuildCameraViewRay(plot, 320, 240, scene, cameraNode).has_value()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
