#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/SceneRenderSubmission.h"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

bool IsFinite(const ri::math::Vec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool IsFinite(const ri::math::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFinite(const ri::scene::Transform& transform) {
    return IsFinite(transform.position) && IsFinite(transform.rotationDegrees) && IsFinite(transform.scale);
}

bool IsFinite(const ri::math::Mat4& matrix) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (!std::isfinite(matrix.m[row][column])) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

int main() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    ri::scene::Scene scene{"AuthoringHelpersSafety"};
    ri::scene::PrimitiveNodeOptions primitive{};
    primitive.nodeName = "SanitizedPrimitive";
    primitive.primitive = ri::scene::PrimitiveType::Cube;
    primitive.transform.position = {nan, infinity, -infinity};
    primitive.transform.rotationDegrees = {nan, infinity, 0.0f};
    primitive.transform.scale = {nan, infinity, 1.0f};
    primitive.baseColor = {nan, infinity, 0.5f};
    primitive.emissiveColor = {infinity, nan, 0.0f};
    primitive.textureTiling = {nan, infinity};
    primitive.metallic = nan;
    primitive.roughness = infinity;
    primitive.opacity = -infinity;
    primitive.alphaCutoff = nan;
    primitive.baseColorTextureFramesPerSecond = infinity;

    const int primitiveNode = ri::scene::AddPrimitiveNode(scene, primitive);
    const ri::scene::Node& sanitizedNode = scene.GetNode(primitiveNode);
    const ri::scene::Material& sanitizedMaterial = scene.GetMaterial(sanitizedNode.material);
    if (!IsFinite(sanitizedNode.localTransform) || !IsFinite(sanitizedMaterial.baseColor)
        || !IsFinite(sanitizedMaterial.emissiveColor) || !IsFinite(sanitizedMaterial.textureTiling)
        || !std::isfinite(sanitizedMaterial.metallic) || !std::isfinite(sanitizedMaterial.roughness)
        || !std::isfinite(sanitizedMaterial.opacity) || !std::isfinite(sanitizedMaterial.alphaCutoff)
        || !std::isfinite(sanitizedMaterial.baseColorTextureFramesPerSecond)) {
        return EXIT_FAILURE;
    }

    ri::scene::ProceduralTerrainOptions terrain{};
    terrain.nodeName = "SanitizedTerrain";
    terrain.resolutionX = 8;
    terrain.resolutionZ = 8;
    terrain.sizeX = nan;
    terrain.sizeZ = infinity;
    terrain.heightAmplitude = nan;
    terrain.heightFrequency = infinity;
    terrain.detailAmplitude = infinity;
    terrain.detailFrequency = nan;
    terrain.roughness = nan;
    terrain.textureTiling = {nan, infinity};
    terrain.transform.position = {infinity, nan, 0.0f};

    const int terrainNode = ri::scene::AddProceduralTerrainNode(scene, terrain);
    const ri::scene::Node& sanitizedTerrainNode = scene.GetNode(terrainNode);
    const ri::scene::Mesh& terrainMesh = scene.GetMesh(sanitizedTerrainNode.mesh);
    const ri::scene::Material& terrainMaterial = scene.GetMaterial(sanitizedTerrainNode.material);
    if (terrainMesh.positions.empty() || !IsFinite(sanitizedTerrainNode.localTransform)
        || !IsFinite(terrainMaterial.textureTiling) || !std::isfinite(terrainMaterial.roughness)) {
        return EXIT_FAILURE;
    }
    for (const ri::math::Vec3 position : terrainMesh.positions) {
        if (!IsFinite(position)) {
            return EXIT_FAILURE;
        }
    }

    const ri::scene::AxesHelperHandles axes = ri::scene::AddAxesHelper(
        scene,
        ri::scene::AxesHelperOptions{
            .transform = ri::scene::Transform{.position = {nan, infinity, 0.0f}},
            .axisLength = infinity,
            .axisThickness = nan,
        });
    if (!IsFinite(scene.GetNode(axes.root).localTransform)
        || !IsFinite(scene.GetNode(axes.xAxis).localTransform)) {
        return EXIT_FAILURE;
    }

    ri::scene::OrbitCameraHandles orbit = ri::scene::AddOrbitCamera(scene, {});
    ri::scene::SetOrbitCameraState(
        scene,
        orbit,
        ri::scene::OrbitCameraState{
            .target = {nan, infinity, -infinity},
            .distance = infinity,
            .yawDegrees = nan,
            .pitchDegrees = infinity,
        });
    if (!IsFinite(orbit.orbit.target) || !std::isfinite(orbit.orbit.distance)
        || !std::isfinite(orbit.orbit.yawDegrees) || !std::isfinite(orbit.orbit.pitchDegrees)
        || !IsFinite(scene.GetNode(orbit.root).localTransform)
        || !IsFinite(scene.GetNode(orbit.swivel).localTransform)
        || !IsFinite(scene.GetNode(orbit.cameraNode).localTransform)) {
        return EXIT_FAILURE;
    }

    if (ri::scene::ComputeOrbitCameraStateFromPosition({nan, 0.0f, 0.0f}, {}).has_value()) {
        return EXIT_FAILURE;
    }

    std::string importError;
    const int placeholderNode = ri::scene::AddModelNode(
        scene,
        ri::scene::ImportedModelOptions{
            .sourcePath = "missing-model.obj",
            .nodeName = "SanitizedMissingModel",
            .transform = ri::scene::Transform{
                .position = {nan, infinity, -infinity},
                .rotationDegrees = {infinity, nan, 0.0f},
                .scale = {nan, infinity, 1.0f},
            },
            .lockToPrimaryBackend = true,
            .createPlaceholderOnFailure = true,
        },
        &importError);
    if (placeholderNode == ri::scene::kInvalidHandle || importError.empty()
        || !IsFinite(scene.GetNode(placeholderNode).localTransform)) {
        return EXIT_FAILURE;
    }

    scene.GetCamera(orbit.camera).fieldOfViewDegrees = nan;
    scene.GetCamera(orbit.camera).nearClip = nan;
    scene.GetCamera(orbit.camera).farClip = infinity;
    if (!IsFinite(ri::scene::ComputeCameraViewProjection(scene, orbit.cameraNode, nan))) {
        return EXIT_FAILURE;
    }

    const int invalidMeshNode = scene.CreateNode("InvalidMeshHandle");
    scene.GetNode(invalidMeshNode).mesh = std::numeric_limits<int>::max();
    const int invalidMaterialNode = ri::scene::AddPrimitiveNode(scene, {});
    scene.GetNode(invalidMaterialNode).material = std::numeric_limits<int>::max();
    const int invalidTransformNode = ri::scene::AddPrimitiveNode(scene, {});
    scene.GetNode(invalidTransformNode).localTransform.position.x = nan;
    const ri::scene::SceneRenderSubmission submission = ri::scene::BuildSceneRenderSubmission(
        scene,
        orbit.cameraNode,
        ri::scene::SceneRenderSubmissionOptions{
            .enableCoarseOcclusion = true,
            .coarseOcclusionGridWidth = std::numeric_limits<int>::max(),
            .coarseOcclusionGridHeight = std::numeric_limits<int>::max(),
        });
    if (submission.stats.drawCommandCount == 0U || submission.stats.skippedNodes < 2U) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
