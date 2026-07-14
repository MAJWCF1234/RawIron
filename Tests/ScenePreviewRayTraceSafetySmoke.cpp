#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/Helpers.h"

#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

bool IsValidImage(const ri::render::software::SoftwareImage& image) {
    return image.width > 0 && image.height > 0
        && image.pixels.size() == static_cast<std::size_t>(image.width * image.height * 3);
}

} // namespace

int main() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    ri::scene::Scene scene{"RayTraceSafety"};
    ri::scene::OrbitCameraHandles orbit = ri::scene::AddOrbitCamera(scene, {});
    ri::scene::SetOrbitCameraState(
        scene,
        orbit,
        ri::scene::OrbitCameraState{
            .target = {0.0f, 0.0f, 0.0f},
            .distance = 6.0f,
            .yawDegrees = 180.0f,
            .pitchDegrees = 0.0f,
        });

    const int cubeNode = ri::scene::AddPrimitiveNode(
        scene,
        ri::scene::PrimitiveNodeOptions{
            .nodeName = "MovingCube",
            .primitive = ri::scene::PrimitiveType::Cube,
            .metallic = 1.0f,
            .roughness = 0.0f,
        });

    const int invalidMeshNode = scene.CreateNode("InvalidMesh");
    scene.GetNode(invalidMeshNode).mesh = INT_MAX;
    scene.GetNode(invalidMeshNode).material = 0;
    const int invalidMaterialNode = scene.CreateNode("InvalidMaterial");
    scene.GetNode(invalidMaterialNode).mesh = 0;
    scene.GetNode(invalidMaterialNode).material = INT_MAX;
    const int invalidTransformNode = ri::scene::AddPrimitiveNode(scene, {});
    scene.GetNode(invalidTransformNode).localTransform.position.x = nan;
    const int invalidLightNode = scene.CreateNode("InvalidLight");
    scene.GetNode(invalidLightNode).light = INT_MAX;
    const int invalidBatch = scene.AddMeshInstanceBatch(ri::scene::MeshInstanceBatch{
        .name = "InvalidBatch",
        .parent = INT_MAX,
        .mesh = 0,
        .material = 0,
        .transforms = {ri::scene::Transform{}},
    });
    if (invalidBatch == ri::scene::kInvalidHandle) {
        return EXIT_FAILURE;
    }

    ri::scene::Camera& camera = scene.GetCamera(orbit.camera);
    camera.fieldOfViewDegrees = nan;
    camera.nearClip = nan;
    camera.farClip = infinity;

    ri::render::software::ScenePreviewOptions options{};
    options.width = 128;
    options.height = 128;
    options.renderer = ri::render::software::ScenePreviewRenderer::RayTrace;
    options.rayTracingResolutionScale = nan;
    options.rayTracingMaxBounces = INT_MAX;
    options.rayTracingShadowRays = INT_MAX;
    options.rayTracingSunRadius = infinity;
    options.rayTracingAmbientOcclusionRadius = nan;
    options.rayTracingAmbientOcclusionStrength = infinity;
    options.rayTracingParallelRowsThreshold = 32;
    options.animationTimeSeconds = std::numeric_limits<double>::infinity();
    options.clearTop = {nan, infinity, -infinity};

    ri::render::software::ScenePreviewCache cache{};
    ri::render::software::SoftwareImage image{};
    ri::render::software::RenderScenePreviewRayTraceInto(scene, orbit.cameraNode, options, image, &cache);
    if (!IsValidImage(image) || cache.rayTraceScene.triV0.empty() || cache.rayTraceBvh.empty()) {
        return EXIT_FAILURE;
    }
    const std::uint64_t firstStamp = cache.rayTraceScene.geometryStamp;
    const ri::math::Vec3 firstVertex = cache.rayTraceScene.triV0.front();

    scene.GetNode(cubeNode).localTransform.position.x = 3.0f;
    ri::render::software::RenderScenePreviewRayTraceInto(scene, orbit.cameraNode, options, image, &cache);
    if (!IsValidImage(image) || cache.rayTraceScene.geometryStamp == firstStamp
        || cache.rayTraceScene.triV0.empty()) {
        return EXIT_FAILURE;
    }
    const ri::math::Vec3 movedVertex = cache.rayTraceScene.triV0.front();
    if (!std::isfinite(movedVertex.x) || std::fabs(movedVertex.x - firstVertex.x) < 2.5f) {
        return EXIT_FAILURE;
    }

    ri::render::software::SoftwareImage invalidCameraImage{};
    ri::render::software::RenderScenePreviewRayTraceInto(scene, INT_MAX, options, invalidCameraImage, &cache);
    if (!IsValidImage(invalidCameraImage)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
