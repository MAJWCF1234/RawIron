#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/ScenePreviewRenderingScript.h"

#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Scene/PhotoModeCamera.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Spatial/Aabb.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ri::render::software {

namespace {

namespace fs = std::filesystem;

constexpr std::uint64_t kGeometryHashOffset = 14695981039346656037ULL;
constexpr std::uint64_t kGeometryHashPrime = 1099511628211ULL;

bool IsValidNodeHandle(const ri::scene::Scene& scene, const int handle) {
    return handle >= 0 && static_cast<std::size_t>(handle) < scene.NodeCount();
}

bool IsValidMeshHandle(const ri::scene::Scene& scene, const int handle) {
    return handle >= 0 && static_cast<std::size_t>(handle) < scene.MeshCount();
}

bool IsValidMaterialHandle(const ri::scene::Scene& scene, const int handle) {
    return handle >= 0 && static_cast<std::size_t>(handle) < scene.MaterialCount();
}

bool IsValidCameraNodeHandle(const ri::scene::Scene& scene, const int handle) {
    if (!IsValidNodeHandle(scene, handle)) {
        return false;
    }
    const int camera = scene.GetNode(handle).camera;
    return camera >= 0 && static_cast<std::size_t>(camera) < scene.CameraCount();
}

bool IsFinite(const ri::math::Vec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool IsFinite(const ri::math::Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
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

float FiniteOr(const float value, const float fallback) {
    return std::isfinite(value) ? value : fallback;
}

void HashUint(std::uint64_t& hash, std::uint64_t value) {
    for (int byte = 0; byte < 8; ++byte) {
        hash ^= value & 0xffU;
        hash *= kGeometryHashPrime;
        value >>= 8U;
    }
}

void HashFloat(std::uint64_t& hash, const float value) {
    HashUint(hash, std::bit_cast<std::uint32_t>(value));
}

void HashVec2(std::uint64_t& hash, const ri::math::Vec2 value) {
    HashFloat(hash, value.x);
    HashFloat(hash, value.y);
}

void HashVec3(std::uint64_t& hash, const ri::math::Vec3 value) {
    HashFloat(hash, value.x);
    HashFloat(hash, value.y);
    HashFloat(hash, value.z);
}

void HashMatrix(std::uint64_t& hash, const ri::math::Mat4& matrix) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            HashFloat(hash, matrix.m[row][column]);
        }
    }
}

void HashMesh(std::uint64_t& hash, const ri::scene::Mesh& mesh) {
    HashUint(hash, static_cast<std::uint64_t>(mesh.primitive));
    HashUint(hash, mesh.positions.size());
    for (const ri::math::Vec3 value : mesh.positions) {
        HashVec3(hash, value);
    }
    HashUint(hash, mesh.normals.size());
    for (const ri::math::Vec3 value : mesh.normals) {
        HashVec3(hash, value);
    }
    HashUint(hash, mesh.texCoords.size());
    for (const ri::math::Vec2 value : mesh.texCoords) {
        HashVec2(hash, value);
    }
    HashUint(hash, mesh.indices.size());
    for (const int index : mesh.indices) {
        HashUint(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(index)));
    }
}

struct CameraBasis {
    ri::math::Vec3 position{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 right{1.0f, 0.0f, 0.0f};
    ri::math::Vec3 up{0.0f, 1.0f, 0.0f};
    ri::math::Vec3 forward{0.0f, 0.0f, 1.0f};
    float focalLength = 1.0f;
    float aspectRatio = 1.0f;
    float nearClip = 0.05f;
    float farClip = 1000.0f;
};

struct ResolvedLight {
    ri::scene::LightType type = ri::scene::LightType::Directional;
    ri::math::Vec3 position{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 direction{0.0f, -1.0f, 0.0f};
    ri::math::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float range = 10.0f;
    float spotAngleDegrees = 45.0f;
};

struct RayHit {
    bool hit = false;
    float distance = 0.0f;
    std::size_t triangleIndex = 0;
    float u = 0.0f;
    float v = 0.0f;
};

struct TracePrimaryResult {
    ri::math::Vec3 color{};
    float depth = std::numeric_limits<float>::max();
};

constexpr std::array<ri::math::Vec3, 8> kCubeVertices = {{
    {-0.5f, -0.5f, -0.5f},
    {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f},
    {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f},
    {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, 0.5f},
}};

constexpr std::array<std::array<int, 4>, 6> kCubeFaces = {{
    {4, 5, 6, 7},
    {1, 0, 3, 2},
    {0, 4, 7, 3},
    {5, 1, 2, 6},
    {3, 7, 6, 2},
    {0, 1, 5, 4},
}};

constexpr std::array<std::array<ri::math::Vec2, 4>, 6> kCubeFaceCornerUv = {{
    {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}},
    {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}},
    {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}},
    {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}},
    {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}},
    {{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}}},
}};

constexpr std::array<ri::math::Vec3, 4> kPlaneVertices = {{
    {-0.5f, 0.0f, -0.5f},
    {0.5f, 0.0f, -0.5f},
    {0.5f, 0.0f, 0.5f},
    {-0.5f, 0.0f, 0.5f},
}};

float Clamp01(float value) {
    return std::clamp(FiniteOr(value, 0.0f), 0.0f, 1.0f);
}

std::uint8_t ToByte(float value) {
    return static_cast<std::uint8_t>(Clamp01(value) * 255.0f + 0.5f);
}

ri::math::Vec3 ClampColor(const ri::math::Vec3& color) {
    return ri::math::Vec3{Clamp01(color.x), Clamp01(color.y), Clamp01(color.z)};
}

ri::math::Vec3 MultiplyColor(const ri::math::Vec3& lhs, const ri::math::Vec3& rhs) {
    return ClampColor(ri::math::Vec3{lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z});
}

float Hash01(int x, int y, int salt) {
    std::uint32_t n = static_cast<std::uint32_t>(x) * 1973U
        + static_cast<std::uint32_t>(y) * 9277U
        + static_cast<std::uint32_t>(salt) * 0x7f4a7c15U;
    n ^= (n << 13);
    n ^= (n >> 17);
    n ^= (n << 5);
    return static_cast<float>(n & 1023U) / 1023.0f;
}

struct SunSkyContext {
    ri::math::Vec3 direction{0.0f, -1.0f, 0.0f};
    ri::math::Vec3 color{1.0f, 0.96f, 0.88f};
    float intensity = 0.0f;
    bool valid = false;
};

SunSkyContext BuildSunSkyContext(const std::vector<ResolvedLight>& lights) {
    SunSkyContext sun{};
    for (const ResolvedLight& light : lights) {
        if (light.type != ri::scene::LightType::Directional) {
            continue;
        }
        sun.direction = ri::math::Normalize(light.direction * -1.0f);
        sun.color = light.color;
        sun.intensity = light.intensity;
        sun.valid = true;
        break;
    }
    return sun;
}

ri::math::Vec3 SkyColor(const ScenePreviewOptions& options,
                        const ri::math::Vec3& direction,
                        const SunSkyContext& sun) {
    const float height = Clamp01(direction.y * 0.5f + 0.5f);
    ri::math::Vec3 color = ri::math::Lerp(options.clearBottom, options.clearTop, height);
    const float horizon = std::pow(1.0f - std::fabs(direction.y), 6.0f);
    const ri::math::Vec3 horizonFog = ResolveScenePreviewFogTint(options, 1.0f);
    color = ClampColor(color + horizonFog * (horizon * 0.22f));
    if (sun.valid) {
        const float sunDot = std::max(0.0f, ri::math::Dot(direction, sun.direction));
        const float disk = std::pow(sunDot, 220.0f) * sun.intensity * 2.8f;
        const float glow = std::pow(sunDot, 28.0f) * sun.intensity * 0.35f;
        color = ClampColor(color + sun.color * (disk + glow));
    }
    return color;
}

ri::math::Vec3 HemisphereSkyLight(const ScenePreviewOptions& options,
                                  const ri::math::Vec3& normal,
                                  const SunSkyContext& sun) {
    const float up = normal.y * 0.5f + 0.5f;
    ri::math::Vec3 sky = ri::math::Lerp(options.clearBottom, options.clearTop, up);
    if (sun.valid) {
        const float sunFacing = std::max(0.0f, ri::math::Dot(normal, sun.direction));
        sky = ClampColor(sky + sun.color * (sunFacing * sun.intensity * 0.12f));
    }
    return MultiplyColor(sky, options.ambientLight * 2.6f);
}

void BuildTangentFrame(const ri::math::Vec3& normal, ri::math::Vec3& tangent, ri::math::Vec3& bitangent) {
    const ri::math::Vec3 helper =
        std::fabs(normal.y) < 0.99f ? ri::math::Vec3{0.0f, 1.0f, 0.0f} : ri::math::Vec3{1.0f, 0.0f, 0.0f};
    tangent = ri::math::Normalize(ri::math::Cross(helper, normal));
    bitangent = ri::math::Normalize(ri::math::Cross(normal, tangent));
}

ri::math::Vec3 PerturbNormal(const ri::math::Vec3& geometricNormal,
                             const ri::math::Vec3& tangent,
                             const ri::math::Vec3& bitangent,
                             const ri::math::Vec3& tangentSpaceNormal) {
    const ri::math::Vec3 mapped = ri::math::Normalize(tangent * tangentSpaceNormal.x
                                                      + bitangent * tangentSpaceNormal.y
                                                      + geometricNormal * tangentSpaceNormal.z);
    return IsFinite(mapped) && ri::math::LengthSquared(mapped)>1e-8f ? mapped : geometricNormal;
}

ri::math::Vec3 CosineHemisphereDirection(const ri::math::Vec3& normal,
                                         const int pixelX,
                                         const int pixelY,
                                         const int sampleIndex) {
    const float u1 = Hash01(pixelX, pixelY, sampleIndex * 5 + 11);
    const float u2 = Hash01(pixelX, pixelY, sampleIndex * 5 + 17);
    const float radius = std::sqrt(u1);
    const float theta = u2 * 6.283185307179586f;
    const float localX = radius * std::cos(theta);
    const float localY = radius * std::sin(theta);
    const float localZ = std::sqrt(std::max(0.0f, 1.0f - u1));
    ri::math::Vec3 tangent{};
    ri::math::Vec3 bitangent{};
    BuildTangentFrame(normal, tangent, bitangent);
    return ri::math::Normalize(tangent * localX + bitangent * localY + normal * localZ);
}

void FillGradientBackground(SoftwareImage& image, const ScenePreviewOptions& options) {
    for (int y = 0; y < image.height; ++y) {
        const float t = image.height > 1 ? static_cast<float>(y) / static_cast<float>(image.height - 1) : 0.0f;
        const ri::math::Vec3 rowColor = ri::math::Lerp(options.clearTop, options.clearBottom, t);
        for (int x = 0; x < image.width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * image.width + x) * 3);
            image.pixels[offset + 0] = ToByte(rowColor.x);
            image.pixels[offset + 1] = ToByte(rowColor.y);
            image.pixels[offset + 2] = ToByte(rowColor.z);
        }
    }
}

bool IsHiddenPreviewNode(const ri::scene::Scene& scene, int nodeHandle, const ScenePreviewOptions& options) {
    if (options.hiddenNodeHandles.empty() || nodeHandle == ri::scene::kInvalidHandle) {
        return false;
    }
    int current = nodeHandle;
    std::size_t remainingParents = scene.NodeCount();
    while (current != ri::scene::kInvalidHandle && remainingParents-- > 0U) {
        if (!IsValidNodeHandle(scene, current)) {
            return false;
        }
        if (std::find(options.hiddenNodeHandles.begin(), options.hiddenNodeHandles.end(), current)
            != options.hiddenNodeHandles.end()) {
            return true;
        }
        current = scene.GetNode(current).parent;
    }
    return false;
}

CameraBasis BuildCameraBasis(const ri::scene::Scene& scene, int cameraNodeHandle, const ScenePreviewOptions& options) {
    CameraBasis basis{};
    const float aspectRatio =
        static_cast<float>(options.width) / static_cast<float>(std::max(options.height, 1));
    basis.aspectRatio = aspectRatio;
    const ri::scene::Node& cameraNode = scene.GetNode(cameraNodeHandle);
    const ri::scene::Camera& camera = scene.GetCamera(cameraNode.camera);
    const float fieldOfViewDegrees = std::clamp(
        FiniteOr(ri::scene::ResolvePhotoModeFieldOfViewDegrees(
                     camera.fieldOfViewDegrees, options.photoMode, aspectRatio),
                 60.0f),
        1.0f,
        179.0f);
    const ri::math::Mat4 world = scene.ComputeWorldMatrix(cameraNodeHandle);
    basis.position = ri::math::ExtractTranslation(world);
    basis.right = ri::math::ExtractRight(world);
    basis.up = ri::math::ExtractUp(world);
    basis.forward = ri::math::ExtractForward(world);
    if (!IsFinite(world)) {
        return CameraBasis{};
    }
    basis.focalLength = 1.0f / std::tan(ri::math::DegreesToRadians(fieldOfViewDegrees * 0.5f));
    basis.nearClip = std::clamp(FiniteOr(camera.nearClip, 0.1f), 0.01f, 1.0e6f);
    basis.farClip = std::clamp(FiniteOr(camera.farClip, 1000.0f), basis.nearClip + 0.01f, 1.0e9f);
    return basis;
}

std::vector<ResolvedLight> ResolveLights(const ri::scene::Scene& scene) {
    std::vector<ResolvedLight> lights;
    for (const int lightNodeHandle : ri::scene::CollectLightNodes(scene)) {
        const ri::scene::Node& node = scene.GetNode(lightNodeHandle);
        if (node.light < 0 || static_cast<std::size_t>(node.light) >= scene.LightCount()) {
            continue;
        }
        const ri::scene::Light& light = scene.GetLight(node.light);
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(lightNodeHandle);
        if (!IsFinite(world)) {
            continue;
        }
        lights.push_back(ResolvedLight{
            .type = light.type,
            .position = ri::math::ExtractTranslation(world),
            .direction = ri::math::ExtractForward(world),
            .color = IsFinite(light.color) ? light.color : ri::math::Vec3{1.0f, 1.0f, 1.0f},
            .intensity = std::clamp(FiniteOr(light.intensity, 1.0f), 0.0f, 1.0e6f),
            .range = std::clamp(FiniteOr(light.range, 10.0f), 0.0f, 1.0e9f),
            .spotAngleDegrees = std::clamp(FiniteOr(light.spotAngleDegrees, 45.0f), 1.0f, 179.0f),
        });
    }
    return lights;
}

std::uint64_t ComputeSceneGeometryStamp(const ri::scene::Scene& scene, const ScenePreviewOptions& options) {
    std::uint64_t stamp = kGeometryHashOffset;
    HashUint(stamp, scene.NodeCount());
    HashUint(stamp, scene.MeshCount());
    HashUint(stamp, scene.MeshInstanceBatchCount());
    for (const int hiddenHandle : options.hiddenNodeHandles) {
        HashUint(stamp, static_cast<std::uint64_t>(static_cast<std::int64_t>(hiddenHandle)));
    }
    std::vector<bool> hashedMeshes(scene.MeshCount(), false);
    std::vector<bool> hashedMaterials(scene.MaterialCount(), false);
    const auto hashResources = [&](const int meshHandle, const int materialHandle) {
        if (IsValidMeshHandle(scene, meshHandle)) {
            const std::size_t index = static_cast<std::size_t>(meshHandle);
            if (!hashedMeshes[index]) {
                hashedMeshes[index] = true;
                HashUint(stamp, index);
                HashMesh(stamp, scene.GetMesh(meshHandle));
            }
        }
        if (IsValidMaterialHandle(scene, materialHandle)) {
            const std::size_t index = static_cast<std::size_t>(materialHandle);
            if (!hashedMaterials[index]) {
                hashedMaterials[index] = true;
                HashUint(stamp, index);
                const ri::scene::Material& material = scene.GetMaterial(materialHandle);
                HashVec2(stamp, material.textureTiling);
                HashUint(stamp, material.additiveBlend ? 1U : 0U);
            }
        }
    };
    for (const int nodeHandle : scene.GetRenderableNodeHandles()) {
        if (IsHiddenPreviewNode(scene, nodeHandle, options)) {
            continue;
        }
        const ri::scene::Node& node = scene.GetNode(nodeHandle);
        HashUint(stamp, static_cast<std::uint64_t>(nodeHandle));
        HashUint(stamp, static_cast<std::uint64_t>(static_cast<std::int64_t>(node.mesh)));
        HashUint(stamp, static_cast<std::uint64_t>(static_cast<std::int64_t>(node.material)));
        hashResources(node.mesh, node.material);
        HashMatrix(stamp, scene.ComputeWorldMatrix(nodeHandle));
    }
    for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
        const ri::scene::MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
        HashUint(stamp, batchIndex);
        HashUint(stamp, static_cast<std::uint64_t>(static_cast<std::int64_t>(batch.mesh)));
        HashUint(stamp, static_cast<std::uint64_t>(static_cast<std::int64_t>(batch.material)));
        hashResources(batch.mesh, batch.material);
        if (IsValidNodeHandle(scene, batch.parent)) {
            HashMatrix(stamp, scene.ComputeWorldMatrix(batch.parent));
        }
        HashUint(stamp, batch.transforms.size());
        for (const ri::scene::Transform& transform : batch.transforms) {
            HashMatrix(stamp, transform.LocalMatrix());
        }
    }
    return stamp;
}

void PushTriangle(ScenePreviewRayTraceScene& accel,
                  const ri::math::Vec3& v0,
                  const ri::math::Vec3& v1,
                  const ri::math::Vec3& v2,
                  const ri::math::Vec3& n0,
                  const ri::math::Vec3& n1,
                  const ri::math::Vec3& n2,
                  const ri::math::Vec2& uv0,
                  const ri::math::Vec2& uv1,
                  const ri::math::Vec2& uv2,
                  const int materialHandle) {
    accel.triV0.push_back(v0);
    accel.triV1.push_back(v1);
    accel.triV2.push_back(v2);
    accel.triN0.push_back(n0);
    accel.triN1.push_back(n1);
    accel.triN2.push_back(n2);
    accel.triUv0.push_back(uv0);
    accel.triUv1.push_back(uv1);
    accel.triUv2.push_back(uv2);
    accel.triMaterial.push_back(materialHandle);
}

std::vector<ri::math::Vec3> BuildMeshVertexNormals(const ri::scene::Mesh& mesh) {
    if (mesh.positions.empty()) {
        return {};
    }
    if (mesh.normals.size() == mesh.positions.size()) {
        std::vector<ri::math::Vec3> normalized = mesh.normals;
        for (ri::math::Vec3& normal : normalized) {
            normal = ri::math::LengthSquared(normal) <= 1e-8f ? ri::math::Vec3{0.0f, 1.0f, 0.0f}
                                                                : ri::math::Normalize(normal);
        }
        return normalized;
    }
    std::vector<ri::math::Vec3> normals(mesh.positions.size(), ri::math::Vec3{0.0f, 0.0f, 0.0f});
    const bool hasIndices = mesh.indices.size() >= 3U;
    const int triangleCount = hasIndices
        ? static_cast<int>(mesh.indices.size() / 3U)
        : static_cast<int>(mesh.positions.size() / 3U);
    for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
        int ia = 0;
        int ib = 0;
        int ic = 0;
        if (hasIndices) {
            ia = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 0)];
            ib = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 1)];
            ic = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 2)];
        } else {
            ia = triangleIndex * 3 + 0;
            ib = triangleIndex * 3 + 1;
            ic = triangleIndex * 3 + 2;
        }
        if (ia < 0 || ib < 0 || ic < 0 || ia >= static_cast<int>(mesh.positions.size())
            || ib >= static_cast<int>(mesh.positions.size()) || ic >= static_cast<int>(mesh.positions.size())) {
            continue;
        }
        const ri::math::Vec3 faceNormal = ri::math::Cross(
            mesh.positions[static_cast<std::size_t>(ib)] - mesh.positions[static_cast<std::size_t>(ia)],
            mesh.positions[static_cast<std::size_t>(ic)] - mesh.positions[static_cast<std::size_t>(ia)]);
        if (ri::math::LengthSquared(faceNormal) <= 1e-8f) {
            continue;
        }
        normals[static_cast<std::size_t>(ia)] = normals[static_cast<std::size_t>(ia)] + faceNormal;
        normals[static_cast<std::size_t>(ib)] = normals[static_cast<std::size_t>(ib)] + faceNormal;
        normals[static_cast<std::size_t>(ic)] = normals[static_cast<std::size_t>(ic)] + faceNormal;
    }
    for (ri::math::Vec3& normal : normals) {
        normal = ri::math::LengthSquared(normal) <= 1e-8f ? ri::math::Vec3{0.0f, 1.0f, 0.0f}
                                                            : ri::math::Normalize(normal);
    }
    return normals;
}

void AppendMeshTriangles(ScenePreviewRayTraceScene& accel,
                         const ri::scene::Mesh& mesh,
                         const ri::scene::Material& material,
                         const ri::math::Mat4& world,
                         const int materialHandle) {
    const ri::math::Vec2 tiling = IsFinite(material.textureTiling)
        ? material.textureTiling
        : ri::math::Vec2{1.0f, 1.0f};
    const bool hasExplicitNormals = mesh.normals.size() == mesh.positions.size();
    const bool needsSmoothNormals = mesh.primitive == ri::scene::PrimitiveType::Sphere && !hasExplicitNormals;
    const std::vector<ri::math::Vec3> vertexNormals =
        hasExplicitNormals || needsSmoothNormals ? BuildMeshVertexNormals(mesh) : std::vector<ri::math::Vec3>{};

    const auto emitTri = [&](const ri::math::Vec3& localA,
                             const ri::math::Vec3& localB,
                             const ri::math::Vec3& localC,
                             const ri::math::Vec2& uvA,
                             const ri::math::Vec2& uvB,
                             const ri::math::Vec2& uvC,
                             const ri::math::Vec3* localNormalA,
                             const ri::math::Vec3* localNormalB,
                             const ri::math::Vec3* localNormalC) {
        const ri::math::Vec3 worldA = ri::math::TransformPoint(world, localA);
        const ri::math::Vec3 worldB = ri::math::TransformPoint(world, localB);
        const ri::math::Vec3 worldC = ri::math::TransformPoint(world, localC);
        if (!IsFinite(worldA) || !IsFinite(worldB) || !IsFinite(worldC)
            || !IsFinite(uvA) || !IsFinite(uvB) || !IsFinite(uvC)) {
            return;
        }
        const ri::math::Vec3 faceNormal = ri::math::Normalize(ri::math::Cross(worldB - worldA, worldC - worldA));
        if (!IsFinite(faceNormal) || ri::math::LengthSquared(faceNormal) <= 1e-8f) {
            return;
        }
        ri::math::Vec3 n0 = faceNormal;
        ri::math::Vec3 n1 = n0;
        ri::math::Vec3 n2 = n0;
        if (localNormalA != nullptr && localNormalB != nullptr && localNormalC != nullptr) {
            n0 = ri::math::TransformNormal(world, *localNormalA);
            n1 = ri::math::TransformNormal(world, *localNormalB);
            n2 = ri::math::TransformNormal(world, *localNormalC);
            if (!IsFinite(n0) || ri::math::LengthSquared(n0) <= 1e-8f) {
                n0 = faceNormal;
            }
            if (!IsFinite(n1) || ri::math::LengthSquared(n1) <= 1e-8f) {
                n1 = faceNormal;
            }
            if (!IsFinite(n2) || ri::math::LengthSquared(n2) <= 1e-8f) {
                n2 = faceNormal;
            }
        }
        PushTriangle(accel,
                       worldA,
                       worldB,
                       worldC,
                       n0,
                       n1,
                       n2,
                       ri::math::Vec2{uvA.x * tiling.x, uvA.y * tiling.y},
                       ri::math::Vec2{uvB.x * tiling.x, uvB.y * tiling.y},
                       ri::math::Vec2{uvC.x * tiling.x, uvC.y * tiling.y},
                       materialHandle);
    };

    const auto emitQuad = [&](const std::array<ri::math::Vec3, 4>& localVertices,
                              const std::array<ri::math::Vec2, 4>& localUv) {
        emitTri(localVertices[0], localVertices[1], localVertices[2], localUv[0], localUv[1], localUv[2], nullptr, nullptr, nullptr);
        emitTri(localVertices[0], localVertices[2], localVertices[3], localUv[0], localUv[2], localUv[3], nullptr, nullptr, nullptr);
    };

    switch (mesh.primitive) {
    case ri::scene::PrimitiveType::Cube:
        for (std::size_t face = 0; face < kCubeFaces.size(); ++face) {
            std::array<ri::math::Vec3, 4> lv{};
            std::array<ri::math::Vec2, 4> luv{};
            for (int k = 0; k < 4; ++k) {
                lv[static_cast<std::size_t>(k)] = kCubeVertices[static_cast<std::size_t>(kCubeFaces[face][static_cast<std::size_t>(k)])];
                luv[static_cast<std::size_t>(k)] = kCubeFaceCornerUv[face][static_cast<std::size_t>(k)];
            }
            emitQuad(lv, luv);
        }
        break;
    case ri::scene::PrimitiveType::Plane: {
        std::array<ri::math::Vec2, 4> luv{};
        for (std::size_t i = 0; i < 4; ++i) {
            luv[i] = ri::math::Vec2{kPlaneVertices[i].x + 0.5f, kPlaneVertices[i].z + 0.5f};
        }
        emitQuad(kPlaneVertices, luv);
        break;
    }
    case ri::scene::PrimitiveType::Custom:
    case ri::scene::PrimitiveType::Sphere:
    default:
        if (mesh.positions.empty()) {
            break;
        }
        const bool hasIndices = mesh.indices.size() >= 3U;
        const bool hasUv = mesh.texCoords.size() == mesh.positions.size();
        const int triangleCount = hasIndices
            ? static_cast<int>(mesh.indices.size() / 3U)
            : static_cast<int>(mesh.positions.size() / 3U);
        for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
            int ia = 0;
            int ib = 0;
            int ic = 0;
            if (hasIndices) {
                ia = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 0)];
                ib = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 1)];
                ic = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 2)];
            } else {
                ia = triangleIndex * 3 + 0;
                ib = triangleIndex * 3 + 1;
                ic = triangleIndex * 3 + 2;
            }
            if (ia < 0 || ib < 0 || ic < 0 || ia >= static_cast<int>(mesh.positions.size())
                || ib >= static_cast<int>(mesh.positions.size()) || ic >= static_cast<int>(mesh.positions.size())) {
                continue;
            }
            ri::math::Vec2 uva{0.0f, 0.0f};
            ri::math::Vec2 uvb{1.0f, 0.0f};
            ri::math::Vec2 uvc{1.0f, 1.0f};
            if (hasUv) {
                uva = mesh.texCoords[static_cast<std::size_t>(ia)];
                uvb = mesh.texCoords[static_cast<std::size_t>(ib)];
                uvc = mesh.texCoords[static_cast<std::size_t>(ic)];
            }
            emitTri(mesh.positions[static_cast<std::size_t>(ia)],
                    mesh.positions[static_cast<std::size_t>(ib)],
                    mesh.positions[static_cast<std::size_t>(ic)],
                    uva,
                    uvb,
                    uvc,
                    vertexNormals.empty() ? nullptr : &vertexNormals[static_cast<std::size_t>(ia)],
                    vertexNormals.empty() ? nullptr : &vertexNormals[static_cast<std::size_t>(ib)],
                    vertexNormals.empty() ? nullptr : &vertexNormals[static_cast<std::size_t>(ic)]);
        }
        break;
    }
}

void BuildRayTraceScene(const ri::scene::Scene& scene,
                        const ScenePreviewOptions& options,
                        ScenePreviewRayTraceScene& accel) {
    const std::uint64_t stamp = ComputeSceneGeometryStamp(scene, options);
    if (accel.geometryStamp == stamp && !accel.triV0.empty()) {
        return;
    }
    accel = ScenePreviewRayTraceScene{};
    accel.geometryStamp = stamp;
    for (const int nodeHandle : scene.GetRenderableNodeHandles()) {
        if (IsHiddenPreviewNode(scene, nodeHandle, options)) {
            continue;
        }
        const ri::scene::Node& node = scene.GetNode(nodeHandle);
        if (!IsValidMeshHandle(scene, node.mesh) || !IsValidMaterialHandle(scene, node.material)) {
            continue;
        }
        if (scene.GetMaterial(node.material).additiveBlend) {
            continue;
        }
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
        if (IsFinite(world)) {
            AppendMeshTriangles(
                accel, scene.GetMesh(node.mesh), scene.GetMaterial(node.material), world, node.material);
        }
    }
    for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
        const ri::scene::MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
        if (IsHiddenPreviewNode(scene, batch.parent, options)) {
            continue;
        }
        if (!IsValidMeshHandle(scene, batch.mesh) || !IsValidMaterialHandle(scene, batch.material)) {
            continue;
        }
        if (scene.GetMaterial(batch.material).additiveBlend) {
            continue;
        }
        if (batch.parent != ri::scene::kInvalidHandle && !IsValidNodeHandle(scene, batch.parent)) {
            continue;
        }
        const ri::math::Mat4 parentWorld = batch.parent != ri::scene::kInvalidHandle
            ? scene.ComputeWorldMatrix(batch.parent)
            : ri::math::IdentityMatrix();
        if (!IsFinite(parentWorld)) {
            continue;
        }
        const ri::scene::Mesh& mesh = scene.GetMesh(batch.mesh);
        const ri::scene::Material& material = scene.GetMaterial(batch.material);
        for (const ri::scene::Transform& transform : batch.transforms) {
            const ri::math::Mat4 world = ri::math::Multiply(parentWorld, transform.LocalMatrix());
            if (IsFinite(world)) {
                AppendMeshTriangles(accel, mesh, material, world, batch.material);
            }
        }
    }
}

ri::spatial::Aabb BvhNodeBounds(const ScenePreviewRayTraceBvhNode& node) {
    return ri::spatial::Aabb{.min = node.boundsMin, .max = node.boundsMax};
}

void SetBvhNodeBounds(ScenePreviewRayTraceBvhNode& node, const ri::spatial::Aabb& bounds) {
    node.boundsMin = bounds.min;
    node.boundsMax = bounds.max;
}

ri::spatial::Aabb TriangleBounds(const ScenePreviewRayTraceScene& accel, const std::size_t triangleIndex) {
    const ri::math::Vec3& a = accel.triV0[triangleIndex];
    const ri::math::Vec3& b = accel.triV1[triangleIndex];
    const ri::math::Vec3& c = accel.triV2[triangleIndex];
    ri::spatial::Aabb box = ri::spatial::MakeEmptyAabb();
    box = ri::spatial::ExpandByPoint(box, a);
    box = ri::spatial::ExpandByPoint(box, b);
    box = ri::spatial::ExpandByPoint(box, c);
    return box;
}

int BuildBvhRecursive(const ScenePreviewRayTraceScene& accel,
                      std::vector<int>& order,
                      std::vector<ScenePreviewRayTraceBvhNode>& nodes,
                      const int start,
                      const int end) {
    ScenePreviewRayTraceBvhNode node{};
    node.triStart = start;
    node.triCount = end - start;
    ri::spatial::Aabb bounds = ri::spatial::MakeEmptyAabb();
    for (int index = start; index < end; ++index) {
        bounds = ri::spatial::Union(
            bounds,
            TriangleBounds(accel, static_cast<std::size_t>(order[static_cast<std::size_t>(index)])));
    }
    SetBvhNodeBounds(node, bounds);
    const int nodeIndex = static_cast<int>(nodes.size());
    nodes.push_back(node);
    if (node.triCount <= 8) {
        return nodeIndex;
    }

    const ri::math::Vec3 extent = ri::spatial::Size(bounds);
    int axis = 0;
    if (extent.y >= extent.x && extent.y >= extent.z) {
        axis = 1;
    } else if (extent.z >= extent.x && extent.z >= extent.y) {
        axis = 2;
    }
    const int mid = start + node.triCount / 2;
    const auto axisComponent = [](const ri::math::Vec3& point, const int axis) {
        return axis == 0 ? point.x : (axis == 1 ? point.y : point.z);
    };
    std::nth_element(order.begin() + start,
                     order.begin() + mid,
                     order.begin() + end,
                     [&](const int lhs, const int rhs) {
                         const ri::math::Vec3 centerL = (accel.triV0[static_cast<std::size_t>(lhs)]
                                                         + accel.triV1[static_cast<std::size_t>(lhs)]
                                                         + accel.triV2[static_cast<std::size_t>(lhs)])
                             * (1.0f / 3.0f);
                         const ri::math::Vec3 centerR = (accel.triV0[static_cast<std::size_t>(rhs)]
                                                         + accel.triV1[static_cast<std::size_t>(rhs)]
                                                         + accel.triV2[static_cast<std::size_t>(rhs)])
                             * (1.0f / 3.0f);
                         return axisComponent(centerL, axis) < axisComponent(centerR, axis);
                     });
    nodes[static_cast<std::size_t>(nodeIndex)].left = BuildBvhRecursive(accel, order, nodes, start, mid);
    nodes[static_cast<std::size_t>(nodeIndex)].right = BuildBvhRecursive(accel, order, nodes, mid, end);
    nodes[static_cast<std::size_t>(nodeIndex)].triStart = 0;
    nodes[static_cast<std::size_t>(nodeIndex)].triCount = 0;
    return nodeIndex;
}

void BuildBvh(const ScenePreviewRayTraceScene& accel,
              std::vector<ScenePreviewRayTraceBvhNode>& nodes,
              std::vector<int>& order) {
    order.resize(accel.triV0.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        order[static_cast<std::size_t>(index)] = static_cast<int>(index);
    }
    nodes.clear();
    if (!order.empty()) {
        BuildBvhRecursive(accel, order, nodes, 0, static_cast<int>(order.size()));
    }
}

void EnsureRayTraceBvh(const ScenePreviewRayTraceScene& accel,
                       ScenePreviewCache* cache,
                       std::vector<ScenePreviewRayTraceBvhNode>& localBvh,
                       std::vector<int>& localOrder) {
    if (cache != nullptr && cache->rayTraceBvhStamp == accel.geometryStamp && !cache->rayTraceBvh.empty()) {
        return;
    }
    BuildBvh(accel, localBvh, localOrder);
    if (cache != nullptr) {
        cache->rayTraceBvh = localBvh;
        cache->rayTraceBvhOrder = localOrder;
        cache->rayTraceBvhStamp = accel.geometryStamp;
    }
}

bool IntersectTriangle(const ri::spatial::Ray& ray,
                       const ri::math::Vec3& v0,
                       const ri::math::Vec3& v1,
                       const ri::math::Vec3& v2,
                       const float tMin,
                       const float tMax,
                       float& outT,
                       float& outU,
                       float& outV) {
    const ri::math::Vec3 edge1 = v1 - v0;
    const ri::math::Vec3 edge2 = v2 - v0;
    const ri::math::Vec3 pvec = ri::math::Cross(ray.direction, edge2);
    const float det = ri::math::Dot(edge1, pvec);
    if (std::fabs(det) <= 1e-8f) {
        return false;
    }
    const float invDet = 1.0f / det;
    const ri::math::Vec3 tvec = ray.origin - v0;
    const float u = ri::math::Dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    const ri::math::Vec3 qvec = ri::math::Cross(tvec, edge1);
    const float v = ri::math::Dot(ray.direction, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    const float t = ri::math::Dot(edge2, qvec) * invDet;
    if (t < tMin || t > tMax) {
        return false;
    }
    outT = t;
    outU = u;
    outV = v;
    return true;
}

RayHit TraceScene(const ScenePreviewRayTraceScene& accel,
                  const std::vector<ScenePreviewRayTraceBvhNode>& nodes,
                  const std::vector<int>& order,
                  const ri::spatial::Ray& ray,
                  const float tMin,
                  const float tMax,
                  const std::optional<std::size_t> skipTriangle = std::nullopt) {
    RayHit best{};
    best.distance = tMax;
    if (nodes.empty()) {
        return best;
    }

    std::vector<int> stack;
    stack.push_back(0);
    while (!stack.empty()) {
        const int nodeIndex = stack.back();
        stack.pop_back();
        const ScenePreviewRayTraceBvhNode& node = nodes[static_cast<std::size_t>(nodeIndex)];
        float boundsDistance = 0.0f;
        if (!ri::spatial::IntersectRayAabb(ray, BvhNodeBounds(node), best.distance, &boundsDistance)) {
            continue;
        }
        if (node.triCount > 0) {
            for (int offset = 0; offset < node.triCount; ++offset) {
                const std::size_t triangleIndex =
                    static_cast<std::size_t>(order[static_cast<std::size_t>(node.triStart + offset)]);
                if (skipTriangle.has_value() && triangleIndex == *skipTriangle) {
                    continue;
                }
                float t = 0.0f;
                float u = 0.0f;
                float v = 0.0f;
                if (!IntersectTriangle(ray,
                                       accel.triV0[triangleIndex],
                                       accel.triV1[triangleIndex],
                                       accel.triV2[triangleIndex],
                                       tMin,
                                       best.distance,
                                       t,
                                       u,
                                       v)) {
                    continue;
                }
                best.hit = true;
                best.distance = t;
                best.triangleIndex = triangleIndex;
                best.u = u;
                best.v = v;
            }
        } else {
            if (node.left >= 0) {
                stack.push_back(node.left);
            }
            if (node.right >= 0) {
                stack.push_back(node.right);
            }
        }
    }
    return best;
}

using TextureCache = std::unordered_map<std::string, RgbaImage>;

std::string ResolveBaseColorTextureName(const ScenePreviewOptions& options,
                                        const ri::scene::Material& material) {
    std::string textureName = material.baseColorTexture;
    if (!material.baseColorTextureFrames.empty()) {
        std::size_t frameIndex = 0;
        if (material.baseColorTextureFramesPerSecond > 0.0f && material.baseColorTextureFrames.size() > 1U) {
            const double safeAnimationTime = std::isfinite(options.animationTimeSeconds)
                ? std::max(0.0, options.animationTimeSeconds)
                : 0.0;
            const double frameCursor = std::floor(safeAnimationTime * material.baseColorTextureFramesPerSecond);
            if (std::isfinite(frameCursor)
                && frameCursor <= static_cast<double>(std::numeric_limits<long long>::max())) {
                frameIndex = static_cast<std::size_t>(
                    static_cast<long long>(frameCursor) % static_cast<long long>(material.baseColorTextureFrames.size()));
            }
        }
        textureName = material.baseColorTextureFrames[frameIndex];
        if (textureName.empty()) {
            textureName = material.baseColorTexture;
        }
    }
    return textureName;
}

void PreloadTexture(TextureCache& cache, const fs::path& textureRoot, const std::string& textureName) {
    if (textureName.empty() || textureRoot.empty()) {
        return;
    }
    const fs::path path = textureRoot / textureName;
    const std::string key = path.generic_string();
    if (!cache.contains(key)) {
        cache.emplace(key, LoadRgbaImageFile(path));
    }
}

const RgbaImage* FindTexture(const TextureCache& cache,
                             const fs::path& textureRoot,
                             const std::string& textureName) {
    if (textureName.empty() || textureRoot.empty()) {
        return nullptr;
    }
    const auto it = cache.find((textureRoot / textureName).generic_string());
    return it != cache.end() && it->second.Valid() ? &it->second : nullptr;
}

const RgbaImage* ResolveTexture(const TextureCache& cache,
                                const fs::path& textureRoot,
                                const ScenePreviewOptions& options,
                                const ri::scene::Material& material) {
    return FindTexture(cache, textureRoot, ResolveBaseColorTextureName(options, material));
}

const RgbaImage* ResolveNormalTexture(const TextureCache& cache,
                                      const fs::path& textureRoot,
                                      const ri::scene::Material& material) {
    return FindTexture(cache, textureRoot, material.normalTexture);
}

void PreloadRayTraceTextures(TextureCache& cache,
                             const fs::path& textureRoot,
                             const ri::scene::Scene& scene,
                             const ScenePreviewRayTraceScene& accel,
                             const ScenePreviewOptions& options) {
    if (textureRoot.empty()) {
        return;
    }
    std::vector<bool> loadedMaterials(scene.MaterialCount(), false);
    for (const int materialHandle : accel.triMaterial) {
        if (!IsValidMaterialHandle(scene, materialHandle)) {
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(materialHandle);
        if (loadedMaterials[index]) {
            continue;
        }
        loadedMaterials[index] = true;
        const ri::scene::Material& material = scene.GetMaterial(materialHandle);
        PreloadTexture(cache, textureRoot, ResolveBaseColorTextureName(options, material));
        if (options.rayTracingNormalMaps) {
            PreloadTexture(cache, textureRoot, material.normalTexture);
        }
    }
}

[[nodiscard]] std::size_t WrapTexturePixelIndex(const RgbaImage& texture, int x, int y) {
    int sx = x % texture.width;
    int sy = y % texture.height;
    if (sx < 0) {
        sx += texture.width;
    }
    if (sy < 0) {
        sy += texture.height;
    }
    return (static_cast<std::size_t>(sy) * static_cast<std::size_t>(texture.width) + static_cast<std::size_t>(sx)) * 4U;
}

ri::math::Vec3 SampleTexture(const RgbaImage* texture, const ri::math::Vec2& uv) {
    if (texture == nullptr || !texture->Valid()) {
        return ri::math::Vec3{1.0f, 1.0f, 1.0f};
    }
    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);
    const int x = static_cast<int>(std::floor(u * static_cast<float>(texture->width - 1) + 0.5f));
    const int y = static_cast<int>(std::floor(v * static_cast<float>(texture->height - 1) + 0.5f));
    const std::size_t offset = WrapTexturePixelIndex(*texture, x, y);
    return ri::math::Vec3{
        static_cast<float>(texture->rgba[offset + 0U]) / 255.0f,
        static_cast<float>(texture->rgba[offset + 1U]) / 255.0f,
        static_cast<float>(texture->rgba[offset + 2U]) / 255.0f,
    };
}

float SampleTextureAlpha(const RgbaImage* texture, const ri::math::Vec2& uv) {
    if (texture == nullptr || !texture->Valid()) {
        return 1.0f;
    }
    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);
    const int x = static_cast<int>(std::floor(u * static_cast<float>(texture->width - 1) + 0.5f));
    const int y = static_cast<int>(std::floor(v * static_cast<float>(texture->height - 1) + 0.5f));
    const std::size_t offset = WrapTexturePixelIndex(*texture, x, y);
    return static_cast<float>(texture->rgba[offset + 3U]) / 255.0f;
}

float ResolveMaterialRoughness(const ri::scene::Material& material, const RgbaImage* texture, const ri::math::Vec2& uv) {
    float roughness = std::clamp(material.roughness, 0.04f, 1.0f);
    if (material.albedoAlphaIsSmoothness && texture != nullptr) {
        const float smoothness = std::clamp(SampleTextureAlpha(texture, uv), 0.0f, 1.0f);
        roughness = std::clamp(material.roughness * 0.08f + (1.0f - smoothness) * 0.92f, 0.04f, 1.0f);
    }
    return roughness;
}

ri::math::Vec3 SampleTangentSpaceNormal(const RgbaImage* texture, const ri::math::Vec2& uv,
                                         const ri::math::Vec2& strength) {
    if (!std::isfinite(strength.x) || !std::isfinite(strength.y)) return {0,0,1};
    const ri::math::Vec3 sampled = SampleTexture(texture, uv);
    return ri::math::Normalize(ri::math::Vec3{
        (sampled.x * 2.0f - 1.0f) * strength.x,
        (sampled.y * 2.0f - 1.0f) * strength.y,
        sampled.z * 2.0f - 1.0f,
    });
}

bool BuildTriangleTangentFrame(const ScenePreviewRayTraceScene& accel,
                               const RayHit& hit,
                               const ri::math::Vec3& geometricNormal,
                               ri::math::Vec3& tangent,
                               ri::math::Vec3& bitangent) {
    const std::size_t index = hit.triangleIndex;
    const ri::math::Vec3 edge1 = accel.triV1[index] - accel.triV0[index];
    const ri::math::Vec3 edge2 = accel.triV2[index] - accel.triV0[index];
    const ri::math::Vec2 duv1 = accel.triUv1[index] - accel.triUv0[index];
    const ri::math::Vec2 duv2 = accel.triUv2[index] - accel.triUv0[index];
    const float determinant = duv1.x * duv2.y - duv1.y * duv2.x;
    const float uvMagnitude=std::max(duv1.x*duv1.x+duv1.y*duv1.y,duv2.x*duv2.x+duv2.y*duv2.y);
    if (std::fabs(determinant) <= std::max(uvMagnitude*1e-6f,1e-30f)) return false;
    const float invDeterminant = 1.0f / determinant;
    tangent = ri::math::Normalize((edge1 * duv2.y - edge2 * duv1.y) * invDeterminant);
    bitangent = ri::math::Normalize((edge2 * duv1.x - edge1 * duv2.x) * invDeterminant);
    tangent = ri::math::Normalize(tangent - geometricNormal * ri::math::Dot(geometricNormal, tangent));
    // UV orientation can be mirrored independently of the geometric normal.
    const auto perpendicular = ri::math::Normalize(ri::math::Cross(geometricNormal, tangent));
    bitangent = perpendicular * (ri::math::Dot(perpendicular, bitangent) < 0.0f ? -1.0f : 1.0f);
    return IsFinite(tangent) && IsFinite(bitangent) && ri::math::LengthSquared(tangent)>1e-8f
        && ri::math::LengthSquared(bitangent)>1e-8f;
}

ri::math::Vec3 InterpolateHit(const ScenePreviewRayTraceScene& accel, const RayHit& hit) {
    const float w = 1.0f - hit.u - hit.v;
    const std::size_t index = hit.triangleIndex;
    return accel.triV0[index] * w + accel.triV1[index] * hit.u + accel.triV2[index] * hit.v;
}

ri::math::Vec3 InterpolateNormal(const ScenePreviewRayTraceScene& accel, const RayHit& hit) {
    const float w = 1.0f - hit.u - hit.v;
    const std::size_t index = hit.triangleIndex;
    const ri::math::Vec3 normal = accel.triN0[index] * w + accel.triN1[index] * hit.u + accel.triN2[index] * hit.v;
    return ri::math::LengthSquared(normal) <= 1e-8f ? ri::math::Vec3{0.0f, 1.0f, 0.0f} : ri::math::Normalize(normal);
}

ri::math::Vec2 InterpolateUv(const ScenePreviewRayTraceScene& accel, const RayHit& hit) {
    const float w = 1.0f - hit.u - hit.v;
    const std::size_t index = hit.triangleIndex;
    return accel.triUv0[index] * w + accel.triUv1[index] * hit.u + accel.triUv2[index] * hit.v;
}

float ComputeAmbientOcclusion(const ScenePreviewRayTraceScene& accel,
                              const std::vector<ScenePreviewRayTraceBvhNode>& nodes,
                              const std::vector<int>& order,
                              const ri::math::Vec3& position,
                              const ri::math::Vec3& normal,
                              const ScenePreviewOptions& options,
                              const int pixelX,
                              const int pixelY) {
    if (!options.rayTracingAmbientOcclusion) {
        return 1.0f;
    }
    const int samples = std::clamp(options.rayTracingAmbientOcclusionRays, 1, 8);
    const float radius = std::max(0.05f, options.rayTracingAmbientOcclusionRadius);
    float occlusion = 0.0f;
    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex) {
        const ri::math::Vec3 direction =
            CosineHemisphereDirection(normal, pixelX, pixelY, sampleIndex + 400);
        const ri::spatial::Ray occlusionRay{
            .origin = position + normal * 0.003f,
            .direction = direction,
        };
        const RayHit hit = TraceScene(accel, nodes, order, occlusionRay, 0.001f, radius);
        if (hit.hit) {
            occlusion += 1.0f;
        }
    }
    const float strength = std::clamp(options.rayTracingAmbientOcclusionStrength, 0.0f, 1.0f);
    return 1.0f - (occlusion / static_cast<float>(samples)) * strength;
}

float ShadowVisibility(const ScenePreviewRayTraceScene& accel,
                       const std::vector<ScenePreviewRayTraceBvhNode>& nodes,
                       const std::vector<int>& order,
                       const ri::math::Vec3& position,
                       const ri::math::Vec3& lightDirection,
                       const float maxDistance,
                       const float angularRadius,
                       const int pixelX,
                       const int pixelY,
                       const int sampleIndex) {
    const float jitterX = (Hash01(pixelX, pixelY, sampleIndex * 3 + 1) - 0.5f) * 2.0f;
    const float jitterY = (Hash01(pixelX, pixelY, sampleIndex * 3 + 2) - 0.5f) * 2.0f;
    ri::math::Vec3 direction = lightDirection;
    if (sampleIndex > 0) {
        const ri::math::Vec3 tangent = std::fabs(lightDirection.y) < 0.99f
            ? ri::math::Normalize(ri::math::Cross(ri::math::Vec3{0.0f, 1.0f, 0.0f}, lightDirection))
            : ri::math::Normalize(ri::math::Cross(ri::math::Vec3{1.0f, 0.0f, 0.0f}, lightDirection));
        const ri::math::Vec3 bitangent = ri::math::Normalize(ri::math::Cross(lightDirection, tangent));
        const float spread = std::max(angularRadius, 0.01f);
        direction = ri::math::Normalize(lightDirection + (tangent * jitterX + bitangent * jitterY) * spread);
    }
    const ri::spatial::Ray shadowRay{
        .origin = position + direction * 0.002f,
        .direction = direction,
    };
    const RayHit shadowHit = TraceScene(accel, nodes, order, shadowRay, 0.001f, maxDistance);
    return shadowHit.hit ? 0.0f : 1.0f;
}

ri::math::Vec3 ShadeHit(const ri::scene::Scene& scene,
                        const ScenePreviewRayTraceScene& accel,
                        const std::vector<ScenePreviewRayTraceBvhNode>& nodes,
                        const std::vector<int>& order,
                        const RayHit& hit,
                        const ri::spatial::Ray& incomingRay,
                        const CameraBasis& camera,
                        const std::vector<ResolvedLight>& lights,
                        const SunSkyContext& sun,
                        const ScenePreviewOptions& options,
                        const TextureCache& textureCache,
                        const fs::path& textureRoot,
                        const int pixelX,
                        const int pixelY,
                        const int depth) {
    const std::size_t triangleIndex = hit.triangleIndex;
    const int materialHandle = accel.triMaterial[triangleIndex];
    const ri::scene::Material& material = scene.GetMaterial(materialHandle);
    const ri::math::Vec3 position = InterpolateHit(accel, hit);
    ri::math::Vec3 normal = InterpolateNormal(accel, hit);
    if (ri::math::Dot(normal, incomingRay.direction) > 0.0f) {
        normal = normal * -1.0f;
    }
    const ri::math::Vec2 surfaceUv = InterpolateUv(accel, hit);
    if (options.rayTracingNormalMaps && !material.normalTexture.empty() && !textureRoot.empty()) {
        const RgbaImage* normalTexture = ResolveNormalTexture(textureCache, textureRoot, material);
        if (normalTexture != nullptr && normalTexture->Valid()) {
            ri::math::Vec3 tangent{};
            ri::math::Vec3 bitangent{};
            if (BuildTriangleTangentFrame(accel, hit, normal, tangent, bitangent)) normal = PerturbNormal(normal,
                                   tangent,
                                   bitangent,
                                   SampleTangentSpaceNormal(normalTexture, surfaceUv, material.normalScale));
        }
    }
    const ri::math::Vec3 viewDirection = ri::math::Normalize(camera.position - position);
    const RgbaImage* texture = textureRoot.empty() ? nullptr
                                                   : ResolveTexture(textureCache, textureRoot, options, material);
    const ri::math::Vec3 albedo = MultiplyColor(material.baseColor, SampleTexture(texture, surfaceUv));
    const float roughness = ResolveMaterialRoughness(material, texture, surfaceUv);

    if (material.shadingModel == ri::scene::ShadingModel::Unlit) {
        return ClampColor(albedo + material.emissiveColor);
    }

    const float ambientOcclusion =
        depth == 0 ? ComputeAmbientOcclusion(accel, nodes, order, position, normal, options, pixelX, pixelY) : 1.0f;
    ri::math::Vec3 color = HemisphereSkyLight(options, normal, sun) * ambientOcclusion;
    float shadowAccumulator = 0.0f;
    int shadowSamples = 0;

    for (const ResolvedLight& light : lights) {
        ri::math::Vec3 toLight{0.0f, 0.0f, 0.0f};
        float attenuation = 1.0f;
        float shadowDistance = camera.farClip;

        float lightAngularRadius = 0.0f;
        if (light.type == ri::scene::LightType::Directional) {
            toLight = ri::math::Normalize(light.direction * -1.0f);
            shadowDistance = camera.farClip;
            lightAngularRadius = options.rayTracingSunRadius;
        } else {
            const ri::math::Vec3 lightOffset = light.position - position;
            const float distance = ri::math::Length(lightOffset);
            if (distance <= 0.0001f) {
                continue;
            }
            toLight = lightOffset / distance;
            shadowDistance = distance - 0.01f;
            if (light.range > 0.0f) {
                attenuation = Clamp01(1.0f - (distance / light.range));
                attenuation *= attenuation;
            }
            if (light.type == ri::scene::LightType::Spot) {
                const float cone = ri::math::Dot(ri::math::Normalize(light.direction) * -1.0f, toLight);
                const float coneCutoff = std::cos(ri::math::DegreesToRadians(light.spotAngleDegrees * 0.5f));
                if (cone <= coneCutoff) {
                    attenuation = 0.0f;
                } else {
                    attenuation *= std::pow(Clamp01((cone - coneCutoff) / std::max(1.0f - coneCutoff, 0.001f)), 2.0f);
                }
            }
        }

        const int shadowRays = std::max(1, options.rayTracingShadowRays);
        float visibility = 0.0f;
        for (int sampleIndex = 0; sampleIndex < shadowRays; ++sampleIndex) {
            visibility += ShadowVisibility(accel,
                                           nodes,
                                           order,
                                           position,
                                           toLight,
                                           shadowDistance,
                                           lightAngularRadius,
                                           pixelX,
                                           pixelY,
                                           sampleIndex);
        }
        visibility /= static_cast<float>(shadowRays);
        shadowAccumulator += visibility;
        shadowSamples += 1;

        const float diffuse = std::max(0.0f, ri::math::Dot(normal, toLight)) * light.intensity * attenuation;
        color = color + MultiplyColor(albedo, light.color * (diffuse * visibility));

        const ri::math::Vec3 halfVector = ri::math::Normalize(toLight + viewDirection);
        const float specPower = std::lerp(96.0f, 12.0f, roughness);
        const float specular = std::pow(std::max(0.0f, ri::math::Dot(normal, halfVector)), specPower)
            * attenuation * (1.0f - roughness) * 0.35f;
        color = color + (light.color * (specular * visibility));
    }

    if (shadowSamples > 0) {
        const float contactShadow = 1.0f - (shadowAccumulator / static_cast<float>(shadowSamples));
        color = color * (1.0f - contactShadow * 0.08f);
    }

    color = ClampColor(color + material.emissiveColor);

    if (options.rayTracingReflections && depth < options.rayTracingMaxBounces) {
        const float fresnel = Clamp01(0.04f + (1.0f - 0.04f) * std::pow(1.0f - std::max(0.0f, ri::math::Dot(normal, viewDirection)), 5.0f));
        const float metallic = std::clamp(material.metallic, 0.0f, 1.0f);
        const float reflectWeight =
            (metallic * fresnel) + ((1.0f - metallic) * fresnel * 0.16f * (1.0f - roughness));
        if (reflectWeight > 0.02f) {
            ri::math::Vec3 reflectDirection =
                incomingRay.direction + normal * (2.0f * ri::math::Dot(normal, incomingRay.direction * -1.0f));
            reflectDirection = ri::math::Normalize(reflectDirection);
            const ri::spatial::Ray reflectRay{.origin = position + normal * 0.002f, .direction = reflectDirection};
            const RayHit reflectHit =
                TraceScene(accel, nodes, order, reflectRay, 0.001f, camera.farClip, triangleIndex);
            ri::math::Vec3 reflected = reflectHit.hit
                ? ShadeHit(scene,
                           accel,
                           nodes,
                           order,
                           reflectHit,
                           reflectRay,
                           camera,
                           lights,
                           sun,
                           options,
                           textureCache,
                           textureRoot,
                           pixelX,
                           pixelY,
                           depth + 1)
                : SkyColor(options, reflectDirection, sun);
            color = ClampColor(ri::math::Lerp(color, reflected, reflectWeight * (1.0f - roughness * 0.75f)));
        }
    }

    const float fogFactor = ComputeScenePreviewFogFactor(options, hit.distance);
    const ri::math::Vec3 fogTint = ResolveScenePreviewFogTint(options, fogFactor);
    color = ClampColor(ri::math::Lerp(color, fogTint, fogFactor * fogFactor * options.fogStrength));
    return color;
}

TracePrimaryResult TracePrimaryRay(const ri::scene::Scene& scene,
                                   const ScenePreviewRayTraceScene& accel,
                                   const std::vector<ScenePreviewRayTraceBvhNode>& nodes,
                                   const std::vector<int>& order,
                                   const ri::spatial::Ray& ray,
                                   const CameraBasis& camera,
                                   const std::vector<ResolvedLight>& lights,
                                   const SunSkyContext& sun,
                                   const ScenePreviewOptions& options,
                                   const TextureCache& textureCache,
                                   const fs::path& textureRoot,
                                   const int pixelX,
                                   const int pixelY) {
    const RayHit hit = TraceScene(accel, nodes, order, ray, camera.nearClip, camera.farClip);
    if (!hit.hit) {
        return TracePrimaryResult{
            .color = SkyColor(options, ray.direction, sun),
            .depth = camera.farClip,
        };
    }
    return TracePrimaryResult{
        .color = ShadeHit(scene,
                          accel,
                          nodes,
                          order,
                          hit,
                          ray,
                          camera,
                          lights,
                          sun,
                          options,
                          textureCache,
                          textureRoot,
                          pixelX,
                          pixelY,
                          0),
        .depth = hit.distance,
    };
}

void TracePixelRow(const int y,
                   const int traceWidth,
                   const int traceHeight,
                   const CameraBasis& camera,
                   const ri::scene::Scene& scene,
                   const ScenePreviewRayTraceScene& accel,
                   const std::vector<ScenePreviewRayTraceBvhNode>& bvh,
                   const std::vector<int>& triangleOrder,
                   const std::vector<ResolvedLight>& lights,
                   const SunSkyContext& sun,
                   const ScenePreviewOptions& options,
                   const TextureCache& textureCache,
                   const fs::path& textureRoot,
                   SoftwareImage& traceImage,
                   std::vector<float>& traceDepth) {
    const int samplesPerPixel = std::clamp(options.rayTracingSamplesPerPixel, 1, 4);
    const float screenY = 1.0f - ((static_cast<float>(y) + 0.5f) / static_cast<float>(traceHeight)) * 2.0f;
    for (int x = 0; x < traceWidth; ++x) {
        ri::math::Vec3 accumulated{0.0f, 0.0f, 0.0f};
        float nearestDepth = camera.farClip;
        for (int sampleIndex = 0; sampleIndex < samplesPerPixel; ++sampleIndex) {
            const float jitterX = samplesPerPixel > 1 ? (Hash01(x, y, sampleIndex * 2 + 1) - 0.5f) : 0.0f;
            const float jitterY = samplesPerPixel > 1 ? (Hash01(x, y, sampleIndex * 2 + 2) - 0.5f) : 0.0f;
            const float screenX =
                ((static_cast<float>(x) + 0.5f + jitterX) / static_cast<float>(traceWidth)) * 2.0f - 1.0f;
            const float sampleScreenY = screenY + (jitterY / static_cast<float>(traceHeight)) * 2.0f;
            const ri::math::Vec3 direction = ri::math::Normalize(
                camera.forward
                + (camera.right * (screenX * camera.aspectRatio / camera.focalLength))
                + (camera.up * (sampleScreenY / camera.focalLength)));
            const ri::spatial::Ray ray{.origin = camera.position, .direction = direction};
            const TracePrimaryResult sample = TracePrimaryRay(scene,
                                                              accel,
                                                              bvh,
                                                              triangleOrder,
                                                              ray,
                                                              camera,
                                                              lights,
                                                              sun,
                                                              options,
                                                              textureCache,
                                                              textureRoot,
                                                              x,
                                                              y);
            accumulated = accumulated + sample.color;
            nearestDepth = std::min(nearestDepth, sample.depth);
        }
        const ri::math::Vec3 color = ClampColor(accumulated * (1.0f / static_cast<float>(samplesPerPixel)));
        const std::size_t offset = static_cast<std::size_t>((y * traceWidth + x) * 3);
        traceImage.pixels[offset + 0] = ToByte(color.x);
        traceImage.pixels[offset + 1] = ToByte(color.y);
        traceImage.pixels[offset + 2] = ToByte(color.z);
        traceDepth[static_cast<std::size_t>(y * traceWidth + x)] = nearestDepth;
    }
}

void UpscaleDepthBuffer(const std::vector<float>& lowRes,
                        const int lowWidth,
                        const int lowHeight,
                        std::vector<float>& outDepth,
                        const int outWidth,
                        const int outHeight) {
    outDepth.assign(static_cast<std::size_t>(outWidth * outHeight), std::numeric_limits<float>::max());
    for (int y = 0; y < outHeight; ++y) {
        const int sy = std::clamp(
            static_cast<int>((static_cast<float>(y) + 0.5f) / static_cast<float>(outHeight) * static_cast<float>(lowHeight)),
            0,
            lowHeight - 1);
        for (int x = 0; x < outWidth; ++x) {
            const int sx = std::clamp(
                static_cast<int>((static_cast<float>(x) + 0.5f) / static_cast<float>(outWidth) * static_cast<float>(lowWidth)),
                0,
                lowWidth - 1);
            outDepth[static_cast<std::size_t>(y * outWidth + x)] =
                lowRes[static_cast<std::size_t>(sy * lowWidth + sx)];
        }
    }
}

void UpscaleImage(const SoftwareImage& lowRes, SoftwareImage& outImage) {
    for (int y = 0; y < outImage.height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(outImage.height);
        const float sy = v * static_cast<float>(lowRes.height) - 0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, lowRes.height - 1);
        const int y1 = std::min(lowRes.height - 1, y0 + 1);
        const float ty = sy - static_cast<float>(y0);
        for (int x = 0; x < outImage.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(outImage.width);
            const float sx = u * static_cast<float>(lowRes.width) - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(sx)), 0, lowRes.width - 1);
            const int x1 = std::min(lowRes.width - 1, x0 + 1);
            const float tx = sx - static_cast<float>(x0);
            const auto sample = [&](const int sx0, const int sy0) {
                const std::size_t offset = static_cast<std::size_t>((sy0 * lowRes.width + sx0) * 3);
                return ri::math::Vec3{
                    static_cast<float>(lowRes.pixels[offset + 0]) / 255.0f,
                    static_cast<float>(lowRes.pixels[offset + 1]) / 255.0f,
                    static_cast<float>(lowRes.pixels[offset + 2]) / 255.0f,
                };
            };
            const ri::math::Vec3 c00 = sample(x0, y0);
            const ri::math::Vec3 c10 = sample(x1, y0);
            const ri::math::Vec3 c01 = sample(x0, y1);
            const ri::math::Vec3 c11 = sample(x1, y1);
            const ri::math::Vec3 cx0 = ri::math::Lerp(c00, c10, tx);
            const ri::math::Vec3 cx1 = ri::math::Lerp(c01, c11, tx);
            const ri::math::Vec3 color = ClampColor(ri::math::Lerp(cx0, cx1, ty));
            const std::size_t outOffset = static_cast<std::size_t>((y * outImage.width + x) * 3);
            outImage.pixels[outOffset + 0] = ToByte(color.x);
            outImage.pixels[outOffset + 1] = ToByte(color.y);
            outImage.pixels[outOffset + 2] = ToByte(color.z);
        }
    }
}

fs::path ResolveTextureRoot(const ScenePreviewOptions& options) {
    if (options.textureRoot.has_value() && !options.textureRoot->empty()) {
        const fs::path path = *options.textureRoot;
        if (ri::content::IsEngineTextureLibraryDirectory(path) || fs::is_directory(path)) {
            return path;
        }
    }
    const fs::path def = DefaultEngineTextureRoot();
    return def.empty() ? fs::path{} : def;
}

} // namespace

void RenderScenePreviewRayTraceInto(const ri::scene::Scene& scene,
                                    int cameraNodeHandle,
                                    const ScenePreviewOptions& rawOptions,
                                    SoftwareImage& outImage,
                                    ScenePreviewCache* cache) {
    ScenePreviewOptions options = rawOptions;
    options.width = std::clamp(options.width, 64, 2048);
    options.height = std::clamp(options.height, 64, 2048);
    options.orderedDither = false;
    options.rayTracingResolutionScale =
        std::clamp(FiniteOr(options.rayTracingResolutionScale, 0.68f), 0.25f, 1.0f);
    options.rayTracingMaxBounces = std::clamp(options.rayTracingMaxBounces, 0, 4);
    options.rayTracingShadowRays = std::clamp(options.rayTracingShadowRays, 1, 16);
    options.rayTracingSunRadius = std::clamp(FiniteOr(options.rayTracingSunRadius, 0.045f), 0.0f, 0.5f);
    options.rayTracingAmbientOcclusionRays = std::clamp(options.rayTracingAmbientOcclusionRays, 1, 8);
    options.rayTracingAmbientOcclusionRadius =
        std::clamp(FiniteOr(options.rayTracingAmbientOcclusionRadius, 0.55f), 0.05f, 1000.0f);
    options.rayTracingAmbientOcclusionStrength =
        std::clamp(FiniteOr(options.rayTracingAmbientOcclusionStrength, 0.42f), 0.0f, 1.0f);
    options.rayTracingParallelRowsThreshold = std::clamp(options.rayTracingParallelRowsThreshold, 32, 2048);

    outImage.width = options.width;
    outImage.height = options.height;
    outImage.pixels.resize(static_cast<std::size_t>(options.width * options.height * 3), 0);
    FillGradientBackground(outImage, options);

    if (!IsValidCameraNodeHandle(scene, cameraNodeHandle)) {
        return;
    }

    ScenePreviewRayTraceScene localAccel{};
    ScenePreviewRayTraceScene& accel = cache != nullptr ? cache->rayTraceScene : localAccel;
    BuildRayTraceScene(scene, options, accel);
    if (accel.triV0.empty()) {
        return;
    }

    std::vector<ScenePreviewRayTraceBvhNode> localBvh{};
    std::vector<int> localOrder{};
    std::vector<ScenePreviewRayTraceBvhNode>& bvh = cache != nullptr ? cache->rayTraceBvh : localBvh;
    std::vector<int>& triangleOrder = cache != nullptr ? cache->rayTraceBvhOrder : localOrder;
    EnsureRayTraceBvh(accel, cache, localBvh, localOrder);

    const CameraBasis camera = BuildCameraBasis(scene, cameraNodeHandle, options);
    const std::vector<ResolvedLight> lights = ResolveLights(scene);
    const SunSkyContext sun = BuildSunSkyContext(lights);
    TextureCache localTextureCache{};
    TextureCache& textureCache = cache != nullptr ? cache->textures : localTextureCache;
    const fs::path textureRoot = ResolveTextureRoot(options);
    PreloadRayTraceTextures(textureCache, textureRoot, scene, accel, options);
    const TextureCache& frozenTextureCache = textureCache;

    const float scale = options.rayTracingResolutionScale;
    const int traceWidth = std::max(16, static_cast<int>(std::lround(static_cast<float>(options.width) * scale)));
    const int traceHeight = std::max(16, static_cast<int>(std::lround(static_cast<float>(options.height) * scale)));

    SoftwareImage traceImage{};
    traceImage.width = traceWidth;
    traceImage.height = traceHeight;
    traceImage.pixels.resize(static_cast<std::size_t>(traceWidth * traceHeight * 3), 0);
    std::vector<float> traceDepth(static_cast<std::size_t>(traceWidth * traceHeight),
                                  std::numeric_limits<float>::max());

    const int parallelThreshold = std::max(32, options.rayTracingParallelRowsThreshold);
    const unsigned int hardwareThreads = std::max(1U, std::thread::hardware_concurrency());
    const int threadCount = traceHeight >= parallelThreshold
        ? static_cast<int>(std::min(hardwareThreads, static_cast<unsigned int>(8)))
        : 1;

    if (threadCount <= 1) {
        for (int y = 0; y < traceHeight; ++y) {
            TracePixelRow(y,
                          traceWidth,
                          traceHeight,
                          camera,
                          scene,
                          accel,
                          bvh,
                          triangleOrder,
                          lights,
                          sun,
                          options,
                          frozenTextureCache,
                          textureRoot,
                          traceImage,
                          traceDepth);
        }
    } else {
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threadCount));
        for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
            workers.emplace_back([&, threadIndex]() {
                const int rowStart = (traceHeight * threadIndex) / threadCount;
                const int rowEnd = (traceHeight * (threadIndex + 1)) / threadCount;
                for (int y = rowStart; y < rowEnd; ++y) {
                    TracePixelRow(y,
                                  traceWidth,
                                  traceHeight,
                                  camera,
                                  scene,
                                  accel,
                                  bvh,
                                  triangleOrder,
                                  lights,
                                  sun,
                                  options,
                                  frozenTextureCache,
                                  textureRoot,
                                  traceImage,
                                  traceDepth);
                }
            });
        }
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    std::vector<float> localDepthBuffer{};
    std::vector<float>& depthBuffer = cache != nullptr ? cache->depthBuffer : localDepthBuffer;
    if (scale >= 0.999f) {
        outImage.pixels = std::move(traceImage.pixels);
        depthBuffer = std::move(traceDepth);
        return;
    }
    UpscaleImage(traceImage, outImage);
    UpscaleDepthBuffer(traceDepth, traceWidth, traceHeight, depthBuffer, outImage.width, outImage.height);
}

} // namespace ri::render::software
