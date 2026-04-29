#include "RawIron/Scene/SceneRenderSubmission.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/PhotoModeCamera.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace ri::scene {

namespace {

void WriteMat4ColumnMajor(const ri::math::Mat4& matrix, float* destination) {
    if (destination == nullptr) {
        return;
    }

    int writeIndex = 0;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            destination[writeIndex++] = matrix.m[row][column];
        }
    }
}

ri::math::Mat4 BuildViewMatrix(const ri::scene::Scene& scene, int cameraNodeHandle) {
    const ri::math::Mat4 world = scene.ComputeWorldMatrix(cameraNodeHandle);
    const ri::math::Vec3 right = ri::math::ExtractRight(world);
    const ri::math::Vec3 up = ri::math::ExtractUp(world);
    const ri::math::Vec3 forward = ri::math::ExtractForward(world);
    const ri::math::Vec3 position = ri::math::ExtractTranslation(world);

    ri::math::Mat4 view = ri::math::IdentityMatrix();
    view.m[0][0] = right.x;
    view.m[0][1] = right.y;
    view.m[0][2] = right.z;
    view.m[0][3] = -ri::math::Dot(right, position);

    view.m[1][0] = up.x;
    view.m[1][1] = up.y;
    view.m[1][2] = up.z;
    view.m[1][3] = -ri::math::Dot(up, position);

    view.m[2][0] = forward.x;
    view.m[2][1] = forward.y;
    view.m[2][2] = forward.z;
    view.m[2][3] = -ri::math::Dot(forward, position);
    return view;
}

ri::math::Mat4 BuildPerspectiveMatrix(float fieldOfViewDegrees, float nearClip, float farClip, float aspectRatio) {
    const float safeAspect = std::max(aspectRatio, 0.001f);
    const float safeNear = std::max(nearClip, 0.001f);
    const float safeFar = std::max(farClip, safeNear + 0.001f);
    const float focalLength = 1.0f / std::tan(ri::math::DegreesToRadians(fieldOfViewDegrees * 0.5f));

    ri::math::Mat4 projection{};
    projection.m[0][0] = focalLength / safeAspect;
    projection.m[1][1] = focalLength;
    projection.m[2][2] = safeFar / (safeFar - safeNear);
    projection.m[2][3] = (-safeFar * safeNear) / (safeFar - safeNear);
    projection.m[3][2] = 1.0f;
    return projection;
}

std::uint16_t ResolvePipelineBucket(const Scene& scene,
                                    const Node& node,
                                    const SceneRenderSubmissionOptions& options) {
    if (node.material == kInvalidHandle) {
        return options.litPipelineBucket;
    }

    const Material& material = scene.GetMaterial(node.material);
    if (material.transparent || material.opacity < 0.999f) {
        return options.transparentPipelineBucket;
    }
    if (material.shadingModel == ShadingModel::Unlit) {
        return options.unlitPipelineBucket;
    }
    return options.litPipelineBucket;
}

std::uint16_t ResolveDepthBucket(const ri::math::Mat4& viewMatrix, const ri::math::Vec3& worldPosition) {
    const ri::math::Vec3 viewPosition = ri::math::TransformPoint(viewMatrix, worldPosition);
    const float depth = std::clamp(viewPosition.z, 0.0f, 65535.0f);
    return static_cast<std::uint16_t>(depth);
}

std::uint16_t ResolveTransparentDepthBucket(const ri::math::Mat4& viewMatrix, const ri::math::Vec3& worldPosition) {
    const std::uint16_t opaqueDepth = ResolveDepthBucket(viewMatrix, worldPosition);
    return static_cast<std::uint16_t>(std::numeric_limits<std::uint16_t>::max() - opaqueDepth);
}

std::uint32_t ResolveMeshIndexCount(const Mesh& mesh) {
    if (mesh.indexCount > 0) {
        return static_cast<std::uint32_t>(mesh.indexCount);
    }
    if (!mesh.indices.empty()) {
        return static_cast<std::uint32_t>(mesh.indices.size());
    }
    if (mesh.vertexCount > 0) {
        return static_cast<std::uint32_t>(mesh.vertexCount);
    }
    switch (mesh.primitive) {
    case PrimitiveType::Cube:
        return 36U;
    case PrimitiveType::Plane:
        return 6U;
    case PrimitiveType::Sphere:
        return 288U;
    case PrimitiveType::Custom:
    default:
        break;
    }
    return static_cast<std::uint32_t>(mesh.positions.size());
}

struct MeshCullBounds {
    ri::math::Vec3 center{};
    float radius = 0.0f;
    bool valid = false;
};

MeshCullBounds BuildMeshCullBounds(const Mesh& mesh) {
    if (!mesh.positions.empty()) {
        ri::math::Vec3 minPoint = mesh.positions.front();
        ri::math::Vec3 maxPoint = mesh.positions.front();
        for (const ri::math::Vec3& point : mesh.positions) {
            minPoint.x = std::min(minPoint.x, point.x);
            minPoint.y = std::min(minPoint.y, point.y);
            minPoint.z = std::min(minPoint.z, point.z);
            maxPoint.x = std::max(maxPoint.x, point.x);
            maxPoint.y = std::max(maxPoint.y, point.y);
            maxPoint.z = std::max(maxPoint.z, point.z);
        }
        const ri::math::Vec3 center = (minPoint + maxPoint) * 0.5f;
        float radiusSq = 0.0f;
        for (const ri::math::Vec3& point : mesh.positions) {
            radiusSq = std::max(radiusSq, ri::math::LengthSquared(point - center));
        }
        return MeshCullBounds{
            .center = center,
            .radius = std::sqrt(std::max(0.0f, radiusSq)),
            .valid = true,
        };
    }

    switch (mesh.primitive) {
    case PrimitiveType::Cube:
        return MeshCullBounds{.center = {}, .radius = 0.8660254f, .valid = true};
    case PrimitiveType::Plane:
        return MeshCullBounds{.center = {}, .radius = 0.7071068f, .valid = true};
    case PrimitiveType::Sphere:
        return MeshCullBounds{.center = {}, .radius = 1.0f, .valid = true};
    case PrimitiveType::Custom:
    default:
        return MeshCullBounds{};
    }
}

bool IsSphereVisibleInView(const ri::math::Vec3& viewCenter,
                           float sphereRadius,
                           float nearClip,
                           float farClip,
                           float tanHalfFovY,
                           float tanHalfFovX) {
    const float radius = std::max(0.0f, sphereRadius);
    if (viewCenter.z + radius <= nearClip) {
        return false;
    }
    if (viewCenter.z - radius >= farClip) {
        return false;
    }

    const float clampedDepth = std::max(viewCenter.z, nearClip);
    const float halfHeight = clampedDepth * tanHalfFovY;
    const float halfWidth = clampedDepth * tanHalfFovX;
    if (std::fabs(viewCenter.x) - radius > halfWidth) {
        return false;
    }
    if (std::fabs(viewCenter.y) - radius > halfHeight) {
        return false;
    }
    return true;
}

float ComputeFarHorizonFactor(float viewDepth, const SceneRenderSubmissionOptions& options) {
    if (!options.enableFarHorizon) {
        return 0.0f;
    }
    if (!std::isfinite(viewDepth) || viewDepth <= options.farHorizonStartDistance) {
        return 0.0f;
    }
    const float endDistance = std::max(options.farHorizonEndDistance, options.farHorizonStartDistance + 1.0f);
    const float normalized = (viewDepth - options.farHorizonStartDistance) / (endDistance - options.farHorizonStartDistance);
    return std::clamp(normalized, 0.0f, 1.0f);
}

std::uint8_t ComputeFarHorizonStride(float factor, std::uint8_t maxStrideSetting) {
    const std::uint8_t maxStride = std::max<std::uint8_t>(1U, maxStrideSetting);
    if (maxStride <= 1U || factor <= 0.0f) {
        return 1U;
    }
    const float stepped = 1.0f + (factor * static_cast<float>(maxStride - 1U));
    return static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::floor(stepped + 1.0e-4f)), 1, static_cast<int>(maxStride)));
}

bool KeepForFarHorizonStride(std::size_t stableIndex, std::uint8_t stride) {
    if (stride <= 1U) {
        return true;
    }
    return (stableIndex % static_cast<std::size_t>(stride)) == 0U;
}

bool ResolveCoarseOcclusionCell(const ri::math::Vec3& viewPosition,
                                float tanHalfFovY,
                                float tanHalfFovX,
                                int gridWidth,
                                int gridHeight,
                                std::size_t& outCellIndex) {
    if (gridWidth <= 0 || gridHeight <= 0 || viewPosition.z <= 0.0f) {
        return false;
    }
    const float projectedHalfWidth = viewPosition.z * tanHalfFovX;
    const float projectedHalfHeight = viewPosition.z * tanHalfFovY;
    if (projectedHalfWidth <= 1e-6f || projectedHalfHeight <= 1e-6f) {
        return false;
    }

    const float xNdc = viewPosition.x / projectedHalfWidth;
    const float yNdc = viewPosition.y / projectedHalfHeight;
    if (!std::isfinite(xNdc) || !std::isfinite(yNdc) || std::fabs(xNdc) > 1.0f || std::fabs(yNdc) > 1.0f) {
        return false;
    }

    const float u = (xNdc * 0.5f) + 0.5f;
    const float v = (yNdc * 0.5f) + 0.5f;
    const int x = std::clamp(static_cast<int>(u * static_cast<float>(gridWidth)), 0, gridWidth - 1);
    const int y = std::clamp(static_cast<int>(v * static_cast<float>(gridHeight)), 0, gridHeight - 1);
    outCellIndex = static_cast<std::size_t>(y) * static_cast<std::size_t>(gridWidth) + static_cast<std::size_t>(x);
    return true;
}

ri::math::Mat4 ResolveBatchParentWorldMatrix(const Scene& scene, const MeshInstanceBatch& batch) {
    if (batch.parent == kInvalidHandle) {
        return ri::math::IdentityMatrix();
    }
    if (batch.parent < 0 || static_cast<std::size_t>(batch.parent) >= scene.NodeCount()) {
        return ri::math::IdentityMatrix();
    }
    return scene.ComputeWorldMatrix(batch.parent);
}

} // namespace

std::optional<int> ResolveSubmissionCameraNode(const Scene& scene, int preferredCameraNodeHandle) {
    if (preferredCameraNodeHandle != kInvalidHandle) {
        if (preferredCameraNodeHandle >= 0
            && static_cast<std::size_t>(preferredCameraNodeHandle) < scene.NodeCount()
            && scene.GetNode(preferredCameraNodeHandle).camera != kInvalidHandle) {
            return preferredCameraNodeHandle;
        }
    }

    const std::vector<int> cameraNodes = CollectCameraNodes(scene);
    if (cameraNodes.empty()) {
        return std::nullopt;
    }
    return cameraNodes.front();
}

ri::math::Mat4 ComputeCameraViewProjection(const Scene& scene,
                                            int cameraNodeHandle,
                                            float aspectRatio,
                                            const PhotoModeCameraOverrides* photoMode) {
    const Node& cameraNode = scene.GetNode(cameraNodeHandle);
    const Camera& camera = scene.GetCamera(cameraNode.camera);
    const PhotoModeCameraOverrides effective = photoMode != nullptr ? *photoMode : PhotoModeCameraOverrides{};
    const float fieldOfViewDegrees =
        ResolvePhotoModeFieldOfViewDegrees(camera.fieldOfViewDegrees, effective, aspectRatio);
    const ri::math::Mat4 view = BuildViewMatrix(scene, cameraNodeHandle);
    const ri::math::Mat4 projection =
        BuildPerspectiveMatrix(fieldOfViewDegrees, camera.nearClip, camera.farClip, aspectRatio);
    return ri::math::Multiply(projection, view);
}

ri::math::Mat4 BuildCameraViewMatrix(const Scene& scene, int cameraNodeHandle) {
    return BuildViewMatrix(scene, cameraNodeHandle);
}

ri::math::Mat4 BuildCameraProjectionMatrix(const Scene& scene,
                                           int cameraNodeHandle,
                                           float aspectRatio,
                                           const PhotoModeCameraOverrides* photoMode) {
    const Node& cameraNode = scene.GetNode(cameraNodeHandle);
    const Camera& camera = scene.GetCamera(cameraNode.camera);
    const PhotoModeCameraOverrides effective = photoMode != nullptr ? *photoMode : PhotoModeCameraOverrides{};
    const float fieldOfViewDegrees =
        ResolvePhotoModeFieldOfViewDegrees(camera.fieldOfViewDegrees, effective, aspectRatio);
    return BuildPerspectiveMatrix(fieldOfViewDegrees, camera.nearClip, camera.farClip, aspectRatio);
}

ri::math::Mat4 BuildCameraSkyRotationMatrix(const Scene& scene, int cameraNodeHandle) {
    ri::math::Mat4 view = BuildViewMatrix(scene, cameraNodeHandle);
    view.m[0][3] = 0.0f;
    view.m[1][3] = 0.0f;
    view.m[2][3] = 0.0f;
    return view;
}

SceneRenderSubmission BuildSceneRenderSubmission(const Scene& scene,
                                                 int cameraNodeHandle,
                                                 const SceneRenderSubmissionOptions& options) {
    SceneRenderSubmission submission{};
    submission.commands.Reserve(4096U);

    const std::optional<int> resolvedCamera = ResolveSubmissionCameraNode(scene, cameraNodeHandle);
    if (!resolvedCamera.has_value()) {
        return submission;
    }

    submission.stats.cameraNodeHandle = *resolvedCamera;

    const float aspectRatio = static_cast<float>(std::max(options.viewportWidth, 1))
        / static_cast<float>(std::max(options.viewportHeight, 1));
    const ri::math::Mat4 viewMatrix = BuildViewMatrix(scene, *resolvedCamera);
    const ri::math::Mat4 viewProjection =
        ComputeCameraViewProjection(scene, *resolvedCamera, aspectRatio, &options.photoMode);
    const int cameraHandle = scene.GetNode(*resolvedCamera).camera;
    const Camera& camera = scene.GetCamera(cameraHandle);
    const float fieldOfViewDegrees =
        ResolvePhotoModeFieldOfViewDegrees(camera.fieldOfViewDegrees, options.photoMode, aspectRatio);
    const float tanHalfFovY =
        std::tan(ri::math::DegreesToRadians(std::clamp(fieldOfViewDegrees, 1.0f, 179.0f) * 0.5f));
    const float tanHalfFovX = tanHalfFovY * std::max(aspectRatio, 0.001f);
    std::vector<std::optional<MeshCullBounds>> meshCullCache(scene.MeshCount());
    const int coarseOcclusionGridWidth = std::max(1, options.coarseOcclusionGridWidth);
    const int coarseOcclusionGridHeight = std::max(1, options.coarseOcclusionGridHeight);
    std::vector<float> coarseOcclusionMinDepth(
        static_cast<std::size_t>(coarseOcclusionGridWidth) * static_cast<std::size_t>(coarseOcclusionGridHeight),
        std::numeric_limits<float>::infinity());

    submission.commands.EmitSorted(
        ri::core::RenderCommandType::ClearColor,
        ri::core::ClearColorCommand{
            .r = options.clearColor.x,
            .g = options.clearColor.y,
            .b = options.clearColor.z,
            .a = options.clearAlpha,
        },
        ri::core::PackRenderSortKey(options.clearPassIndex, options.utilityPipelineBucket, 0U, 0U));

    ri::core::SetViewProjectionCommand setViewProjection{};
    WriteMat4ColumnMajor(viewProjection, setViewProjection.viewProjection);
    submission.commands.EmitSorted(
        ri::core::RenderCommandType::SetViewProjection,
        setViewProjection,
        ri::core::PackRenderSortKey(options.clearPassIndex, options.utilityPipelineBucket, 0U, 1U));

    for (const int nodeHandle : scene.GetRenderableNodeHandles()) {
        const Node& node = scene.GetNode(nodeHandle);
        if (node.mesh == kInvalidHandle) {
            submission.stats.skippedNodes += 1U;
            continue;
        }

        const Mesh& mesh = scene.GetMesh(node.mesh);
        const std::uint32_t indexCount = ResolveMeshIndexCount(mesh);
        if (indexCount == 0U) {
            submission.stats.skippedNodes += 1U;
            continue;
        }

        const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
        const ri::math::Vec3 worldPosition = ri::math::ExtractTranslation(world);
        const ri::math::Vec3 viewPosition = ri::math::TransformPoint(viewMatrix, worldPosition);
        const std::uint16_t pipelineBucket = ResolvePipelineBucket(scene, node, options);

        // Do not reject meshes using only the node's pivot depth. Large authored meshes often have
        // origins outside the mesh bounds (floors, merged props); frustum/sphere checks use mesh bounds.
        if (options.enableFarHorizon) {
            if (viewPosition.z >= options.farHorizonMaxDistance) {
                submission.stats.skippedNodes += 1U;
                continue;
            }
            const float farFactor = ComputeFarHorizonFactor(viewPosition.z, options);
            if (farFactor > 0.0f) {
                if (options.farHorizonCullTransparent && pipelineBucket == options.transparentPipelineBucket) {
                    submission.stats.skippedNodes += 1U;
                    continue;
                }
                const std::uint8_t stride = ComputeFarHorizonStride(farFactor, options.farHorizonMaxNodeStride);
                if (!KeepForFarHorizonStride(static_cast<std::size_t>(nodeHandle), stride)) {
                    submission.stats.skippedNodes += 1U;
                    continue;
                }
            }
        }
        if (options.enableFrustumCulling) {
            if (!meshCullCache[node.mesh].has_value()) {
                meshCullCache[node.mesh] = BuildMeshCullBounds(mesh);
            }
            const MeshCullBounds& cull = *meshCullCache[node.mesh];
            if (cull.valid) {
                const ri::math::Vec3 worldCullCenter = ri::math::TransformPoint(world, cull.center);
                const ri::math::Vec3 viewCullCenter = ri::math::TransformPoint(viewMatrix, worldCullCenter);
                const ri::math::Vec3 worldScale = ri::math::ExtractScale(world);
                const float maxScale = std::max({std::fabs(worldScale.x), std::fabs(worldScale.y), std::fabs(worldScale.z)});
                const float worldRadius = cull.radius * std::max(maxScale, 0.0001f);
                if (!IsSphereVisibleInView(
                        viewCullCenter, worldRadius, camera.nearClip, camera.farClip, tanHalfFovY, tanHalfFovX)) {
                    submission.stats.skippedNodes += 1U;
                    continue;
                }
            }
        }

        ri::core::DrawMeshCommand draw{};
        draw.meshHandle = node.mesh;
        draw.materialHandle = node.material;
        draw.firstIndex = 0U;
        draw.indexCount = indexCount;
        draw.instanceCount = 1U;
        WriteMat4ColumnMajor(world, draw.model);

        const std::uint16_t stableMaterialBucket = static_cast<std::uint16_t>(
            std::clamp(node.material == kInvalidHandle ? 0 : node.material + 1,
                       0,
                       static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
        const bool isTransparent = pipelineBucket == options.transparentPipelineBucket;
        if (options.enableCoarseOcclusion && !isTransparent) {
            std::size_t cellIndex = 0;
            if (ResolveCoarseOcclusionCell(
                    viewPosition,
                    tanHalfFovY,
                    tanHalfFovX,
                    coarseOcclusionGridWidth,
                    coarseOcclusionGridHeight,
                    cellIndex)) {
                const float depthBias = std::max(0.01f, options.coarseOcclusionDepthBias);
                const float occluderDepth = coarseOcclusionMinDepth[cellIndex];
                if (std::isfinite(occluderDepth) && viewPosition.z > (occluderDepth + depthBias)) {
                    submission.stats.skippedNodes += 1U;
                    continue;
                }
                coarseOcclusionMinDepth[cellIndex] = std::min(occluderDepth, viewPosition.z);
            }
        }
        const std::uint16_t materialBucket = isTransparent
            ? ResolveTransparentDepthBucket(viewMatrix, worldPosition)
            : stableMaterialBucket;
        const std::uint16_t depthBucket = isTransparent
            ? stableMaterialBucket
            : ResolveDepthBucket(viewMatrix, worldPosition);

        submission.commands.EmitSorted(
            ri::core::RenderCommandType::DrawMesh,
            draw,
            ri::core::PackRenderSortKey(
                options.drawPassIndex,
                pipelineBucket,
                materialBucket,
                depthBucket,
                static_cast<std::uint8_t>(nodeHandle & 0xFF)));
        submission.stats.drawCommandCount += 1U;
    }

    for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
        const MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
        if (batch.mesh == kInvalidHandle || batch.transforms.empty()) {
            submission.stats.skippedNodes += batch.transforms.empty() ? 1U : 0U;
            continue;
        }

        const Mesh& mesh = scene.GetMesh(batch.mesh);
        const std::uint32_t indexCount = ResolveMeshIndexCount(mesh);
        if (indexCount == 0U) {
            submission.stats.skippedNodes += batch.transforms.size();
            continue;
        }

        const ri::math::Mat4 parentWorld = ResolveBatchParentWorldMatrix(scene, batch);
        const std::uint16_t pipelineBucket = [&]() {
            if (batch.material == kInvalidHandle) {
                return options.litPipelineBucket;
            }
            const Material& material = scene.GetMaterial(batch.material);
            if (material.transparent || material.opacity < 0.999f) {
                return options.transparentPipelineBucket;
            }
            if (material.shadingModel == ShadingModel::Unlit) {
                return options.unlitPipelineBucket;
            }
            return options.litPipelineBucket;
        }();

        const std::uint16_t stableMaterialBucket = static_cast<std::uint16_t>(
            std::clamp(batch.material == kInvalidHandle ? 0 : batch.material + 1,
                       0,
                       static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
        const bool isTransparent = pipelineBucket == options.transparentPipelineBucket;

        for (std::size_t instanceIndex = 0; instanceIndex < batch.transforms.size(); ++instanceIndex) {
            const ri::math::Mat4 world = ri::math::Multiply(parentWorld, batch.transforms[instanceIndex].LocalMatrix());
            const ri::math::Vec3 worldPosition = ri::math::ExtractTranslation(world);
            const ri::math::Vec3 viewPosition = ri::math::TransformPoint(viewMatrix, worldPosition);
            if (options.enableFarHorizon) {
                if (viewPosition.z >= options.farHorizonMaxDistance) {
                    submission.stats.skippedNodes += 1U;
                    continue;
                }
                const float farFactor = ComputeFarHorizonFactor(viewPosition.z, options);
                if (farFactor > 0.0f) {
                    if (options.farHorizonCullTransparent && isTransparent) {
                        submission.stats.skippedNodes += 1U;
                        continue;
                    }
                    const std::uint8_t stride =
                        ComputeFarHorizonStride(farFactor, options.farHorizonMaxInstanceStride);
                    const std::size_t stableIndex = (batchIndex * 4099U) + instanceIndex;
                    if (!KeepForFarHorizonStride(stableIndex, stride)) {
                        submission.stats.skippedNodes += 1U;
                        continue;
                    }
                }
            }
            if (options.enableFrustumCulling) {
                if (!meshCullCache[batch.mesh].has_value()) {
                    meshCullCache[batch.mesh] = BuildMeshCullBounds(mesh);
                }
                const MeshCullBounds& cull = *meshCullCache[batch.mesh];
                if (cull.valid) {
                    const ri::math::Vec3 worldCullCenter = ri::math::TransformPoint(world, cull.center);
                    const ri::math::Vec3 viewCullCenter = ri::math::TransformPoint(viewMatrix, worldCullCenter);
                    const ri::math::Vec3 worldScale = ri::math::ExtractScale(world);
                    const float maxScale =
                        std::max({std::fabs(worldScale.x), std::fabs(worldScale.y), std::fabs(worldScale.z)});
                    const float worldRadius = cull.radius * std::max(maxScale, 0.0001f);
                    if (!IsSphereVisibleInView(
                            viewCullCenter, worldRadius, camera.nearClip, camera.farClip, tanHalfFovY, tanHalfFovX)) {
                        submission.stats.skippedNodes += 1U;
                        continue;
                    }
                }
            }

            ri::core::DrawMeshCommand draw{};
            draw.meshHandle = batch.mesh;
            draw.materialHandle = batch.material;
            draw.firstIndex = 0U;
            draw.indexCount = indexCount;
            draw.instanceCount = 1U;
            WriteMat4ColumnMajor(world, draw.model);

            if (options.enableCoarseOcclusion && !isTransparent) {
                std::size_t cellIndex = 0;
                if (ResolveCoarseOcclusionCell(
                        viewPosition,
                        tanHalfFovY,
                        tanHalfFovX,
                        coarseOcclusionGridWidth,
                        coarseOcclusionGridHeight,
                        cellIndex)) {
                    const float depthBias = std::max(0.01f, options.coarseOcclusionDepthBias);
                    const float occluderDepth = coarseOcclusionMinDepth[cellIndex];
                    if (std::isfinite(occluderDepth) && viewPosition.z > (occluderDepth + depthBias)) {
                        submission.stats.skippedNodes += 1U;
                        continue;
                    }
                    coarseOcclusionMinDepth[cellIndex] = std::min(occluderDepth, viewPosition.z);
                }
            }

            const std::uint16_t materialBucket = isTransparent
                ? ResolveTransparentDepthBucket(viewMatrix, worldPosition)
                : stableMaterialBucket;
            const std::uint16_t depthBucket = isTransparent
                ? stableMaterialBucket
                : ResolveDepthBucket(viewMatrix, worldPosition);

            submission.commands.EmitSorted(
                ri::core::RenderCommandType::DrawMesh,
                draw,
                ri::core::PackRenderSortKey(
                    options.drawPassIndex,
                    pipelineBucket,
                    materialBucket,
                    depthBucket,
                    static_cast<std::uint8_t>((batchIndex + instanceIndex) & 0xFF)));
            submission.stats.drawCommandCount += 1U;
        }
    }

    return submission;
}

} // namespace ri::scene
