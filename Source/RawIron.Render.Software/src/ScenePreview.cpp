#include "RawIron/Render/ScenePreview.h"

#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/PhotoModeCamera.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ri::render::software {

namespace {

namespace fs = std::filesystem;

struct ScreenVertex {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    ri::math::Vec2 uv{0.0f, 0.0f};
};

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

struct ClipVertex {
    ri::math::Vec3 p{};
    ri::math::Vec2 uv{};
};

struct TextureSample {
    ri::math::Vec3 color{1.0f, 1.0f, 1.0f};
    float alpha = 1.0f;
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

struct MeshCullBounds {
    ri::math::Vec3 center{};
    float radius = 0.0f;
    bool valid = false;
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

/// UV corners per face matching `kCubeFaces` quad winding (v grows down in image space after sample).
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

float DitherNudge(int x, int y) {
    // Integer hash noise avoids visible 4x4/8x8 screen-door grid artifacts.
    std::uint32_t n = static_cast<std::uint32_t>(x) * 1973U
        + static_cast<std::uint32_t>(y) * 9277U
        + 0x7f4a7c15U;
    n ^= (n << 13);
    n ^= (n >> 17);
    n ^= (n << 5);
    const float unit = static_cast<float>(n & 1023U) / 1023.0f;
    return unit - 0.5f;
}

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

std::uint8_t ToByte(float value) {
    const float normalized = Clamp01(value);
    return static_cast<std::uint8_t>(normalized * 255.0f + 0.5f);
}

ri::math::Vec3 ClampColor(const ri::math::Vec3& color) {
    return ri::math::Vec3{
        Clamp01(color.x),
        Clamp01(color.y),
        Clamp01(color.z),
    };
}

ri::math::Vec3 MultiplyColor(const ri::math::Vec3& lhs, const ri::math::Vec3& rhs) {
    return ClampColor(ri::math::Vec3{
        lhs.x * rhs.x,
        lhs.y * rhs.y,
        lhs.z * rhs.z,
    });
}

void SetPixel(SoftwareImage& image, int x, int y, const ri::math::Vec3& color, bool dither) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        return;
    }

    ri::math::Vec3 out = ClampColor(color);
    if (dither) {
        const float nudge = DitherNudge(x, y);
        constexpr float step = 1.0f / 255.0f;
        out = ClampColor(out + ri::math::Vec3{nudge * step, nudge * step, nudge * step});
    }

    const std::size_t offset = static_cast<std::size_t>((y * image.width + x) * 3);
    image.pixels[offset + 0] = ToByte(out.x);
    image.pixels[offset + 1] = ToByte(out.y);
    image.pixels[offset + 2] = ToByte(out.z);
}

void FillGradientBackground(SoftwareImage& image, const ScenePreviewOptions& options) {
    for (int y = 0; y < image.height; ++y) {
        const float t = image.height > 1 ? static_cast<float>(y) / static_cast<float>(image.height - 1) : 0.0f;
        const ri::math::Vec3 rowColor = ri::math::Lerp(options.clearTop, options.clearBottom, t);
        for (int x = 0; x < image.width; ++x) {
            SetPixel(image, x, y, rowColor, options.orderedDither);
        }
    }
}

float EdgeFunction(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

ScreenVertex ProjectPoint(const ri::math::Vec3& position,
                          const ri::math::Vec2& uv,
                          int width,
                          int height,
                          float focalLength,
                          float aspectRatio) {
    const float ndcX = (position.x / position.z) * (focalLength / aspectRatio);
    const float ndcY = (position.y / position.z) * focalLength;

    return ScreenVertex{
        .x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width - 1),
        .y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height - 1),
        .depth = position.z,
        .uv = uv,
    };
}

MeshCullBounds BuildMeshCullBounds(const ri::scene::Mesh& mesh) {
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
    case ri::scene::PrimitiveType::Cube:
        return MeshCullBounds{.center = {}, .radius = 0.8660254f, .valid = true};
    case ri::scene::PrimitiveType::Plane:
        return MeshCullBounds{.center = {}, .radius = 0.7071068f, .valid = true};
    case ri::scene::PrimitiveType::Sphere:
        return MeshCullBounds{.center = {}, .radius = 1.0f, .valid = true};
    case ri::scene::PrimitiveType::Custom:
    default:
        return MeshCullBounds{};
    }
}

bool IsHiddenPreviewNode(const ri::scene::Scene& scene, int nodeHandle, const ScenePreviewOptions& options) {
    if (options.hiddenNodeHandles.empty() || nodeHandle == ri::scene::kInvalidHandle) {
        return false;
    }

    int current = nodeHandle;
    while (current != ri::scene::kInvalidHandle) {
        if (std::find(options.hiddenNodeHandles.begin(), options.hiddenNodeHandles.end(), current)
            != options.hiddenNodeHandles.end()) {
            return true;
        }
        current = scene.GetNode(current).parent;
    }
    return false;
}

std::vector<ri::math::Vec3> BuildMeshVertexNormals(const ri::scene::Mesh& mesh) {
    if (mesh.positions.empty()) {
        return {};
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

        if (ia < 0 || ib < 0 || ic < 0 ||
            ia >= static_cast<int>(mesh.positions.size()) ||
            ib >= static_cast<int>(mesh.positions.size()) ||
            ic >= static_cast<int>(mesh.positions.size())) {
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

    for (std::size_t index = 0; index < normals.size(); ++index) {
        if (ri::math::LengthSquared(normals[index]) <= 1e-8f) {
            normals[index] = ri::math::Vec3{0.0f, 1.0f, 0.0f};
            continue;
        }
        normals[index] = ri::math::Normalize(normals[index]);
    }
    return normals;
}

CameraBasis BuildCameraBasis(const ri::scene::Scene& scene, int cameraNodeHandle, const ScenePreviewOptions& options) {
    CameraBasis basis{};
    const float aspectRatio =
        static_cast<float>(options.width) / static_cast<float>(std::max(options.height, 1));
    basis.aspectRatio = aspectRatio;

    const ri::scene::Node& cameraNode = scene.GetNode(cameraNodeHandle);
    const ri::scene::Camera& camera = scene.GetCamera(cameraNode.camera);
    const float fieldOfViewDegrees =
        ri::scene::ResolvePhotoModeFieldOfViewDegrees(camera.fieldOfViewDegrees, options.photoMode, aspectRatio);
    const ri::math::Mat4 world = scene.ComputeWorldMatrix(cameraNodeHandle);
    basis.position = ri::math::ExtractTranslation(world);
    basis.right = ri::math::ExtractRight(world);
    basis.up = ri::math::ExtractUp(world);
    basis.forward = ri::math::ExtractForward(world);
    basis.focalLength = 1.0f / std::tan(ri::math::DegreesToRadians(fieldOfViewDegrees * 0.5f));
    basis.nearClip = std::max(0.01f, camera.nearClip);
    basis.farClip = std::max(basis.nearClip + 0.01f, camera.farClip);
    return basis;
}

ri::math::Vec3 ToCameraSpace(const CameraBasis& camera, const ri::math::Vec3& worldPoint) {
    const ri::math::Vec3 offset = worldPoint - camera.position;
    return ri::math::Vec3{
        ri::math::Dot(offset, camera.right),
        ri::math::Dot(offset, camera.up),
        ri::math::Dot(offset, camera.forward),
    };
}

bool IsSphereVisibleInCamera(const CameraBasis& camera, const ri::math::Vec3& viewCenter, float radius) {
    const float safeRadius = std::max(0.0f, radius);
    if (viewCenter.z + safeRadius <= camera.nearClip) {
        return false;
    }
    if (viewCenter.z - safeRadius >= camera.farClip) {
        return false;
    }

    const float tanHalfFovY = 1.0f / std::max(camera.focalLength, 1e-4f);
    const float tanHalfFovX = tanHalfFovY * std::max(camera.aspectRatio, 0.001f);
    const float clampedDepth = std::max(viewCenter.z, camera.nearClip);
    const float halfHeight = clampedDepth * tanHalfFovY;
    const float halfWidth = clampedDepth * tanHalfFovX;
    if (std::fabs(viewCenter.x) - safeRadius > halfWidth) {
        return false;
    }
    if (std::fabs(viewCenter.y) - safeRadius > halfHeight) {
        return false;
    }
    return true;
}

float ComputeFarHorizonFactor(float distance, const ScenePreviewOptions& options) {
    const float start = std::max(0.0f, options.farHorizonStartDistance);
    const float end = std::max(start + 0.001f, options.farHorizonEndDistance);
    return Clamp01((distance - start) / (end - start));
}

std::uint32_t ComputeFarHorizonStride(float distance,
                                      const ScenePreviewOptions& options,
                                      std::uint32_t maxStride) {
    const std::uint32_t clampedMaxStride = std::max(1U, maxStride);
    if (clampedMaxStride <= 1U || distance <= options.farHorizonStartDistance) {
        return 1U;
    }
    const float factor = ComputeFarHorizonFactor(distance, options);
    const float strideFloat = 1.0f + factor * static_cast<float>(clampedMaxStride - 1U);
    return std::clamp(static_cast<std::uint32_t>(std::lround(strideFloat)), 1U, clampedMaxStride);
}

std::vector<ResolvedLight> ResolveLights(const ri::scene::Scene& scene) {
    std::vector<ResolvedLight> lights;
    for (const int lightNodeHandle : ri::scene::CollectLightNodes(scene)) {
        const ri::scene::Node& node = scene.GetNode(lightNodeHandle);
        const ri::scene::Light& light = scene.GetLight(node.light);
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(lightNodeHandle);
        lights.push_back(ResolvedLight{
            .type = light.type,
            .position = ri::math::ExtractTranslation(world),
            .direction = ri::math::ExtractForward(world),
            .color = light.color,
            .intensity = light.intensity,
            .range = light.range,
            .spotAngleDegrees = light.spotAngleDegrees,
        });
    }
    return lights;
}

ri::math::Vec3 ShadeFace(const ri::math::Vec3& baseColor,
                         ri::scene::ShadingModel shadingModel,
                         const ri::math::Vec3& worldNormal,
                         const ri::math::Vec3& worldCenter,
                         const CameraBasis& camera,
                         const std::vector<ResolvedLight>& lights,
                         const ScenePreviewOptions& options) {
    if (shadingModel == ri::scene::ShadingModel::Unlit) {
        return ClampColor(baseColor);
    }

    const ri::math::Vec3 normal = ri::math::Normalize(worldNormal);
    const ri::math::Vec3 viewDirection = ri::math::Normalize(camera.position - worldCenter);
    ri::math::Vec3 lightAccumulator = options.ambientLight;
    ri::math::Vec3 specularAccumulator{0.0f, 0.0f, 0.0f};

    for (const ResolvedLight& light : lights) {
        ri::math::Vec3 toLight{0.0f, 0.0f, 0.0f};
        float attenuation = 1.0f;

        if (light.type == ri::scene::LightType::Directional) {
            toLight = ri::math::Normalize(light.direction * -1.0f);
        } else {
            const ri::math::Vec3 lightOffset = light.position - worldCenter;
            const float distance = ri::math::Length(lightOffset);
            if (distance <= 0.0001f) {
                continue;
            }

            toLight = lightOffset / distance;
            if (light.range > 0.0f) {
                attenuation = Clamp01(1.0f - (distance / light.range));
                attenuation *= attenuation;
            }

            if (light.type == ri::scene::LightType::Spot) {
                const ri::math::Vec3 spotForward = ri::math::Normalize(light.direction);
                const float cone = ri::math::Dot(spotForward * -1.0f, toLight);
                const float coneCutoff = std::cos(ri::math::DegreesToRadians(light.spotAngleDegrees * 0.5f));
                if (cone <= coneCutoff) {
                    attenuation = 0.0f;
                } else {
                    attenuation *= std::pow(Clamp01((cone - coneCutoff) / std::max(1.0f - coneCutoff, 0.001f)), 2.0f);
                }
            }
        }

        const float diffuse = std::max(0.0f, ri::math::Dot(normal, toLight)) * light.intensity * attenuation;
        lightAccumulator = lightAccumulator + (light.color * diffuse);

        const ri::math::Vec3 halfVector = ri::math::Normalize(toLight + viewDirection);
        const float specular =
            std::pow(std::max(0.0f, ri::math::Dot(normal, halfVector)), 18.0f) * attenuation * 0.12f;
        specularAccumulator = specularAccumulator + (light.color * specular);
    }

    return ClampColor(MultiplyColor(baseColor, lightAccumulator) + specularAccumulator);
}

TextureSample SampleTexturePixelWrapped(const RgbaImage& tex, int x, int y) {
    x = x % tex.width;
    y = y % tex.height;
    if (x < 0) {
        x += tex.width;
    }
    if (y < 0) {
        y += tex.height;
    }
    const std::size_t o =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(tex.width) + static_cast<std::size_t>(x)) * 4U;
    return TextureSample{
        .color = ri::math::Vec3{
            static_cast<float>(tex.rgba[o + 0U]) / 255.0f,
            static_cast<float>(tex.rgba[o + 1U]) / 255.0f,
            static_cast<float>(tex.rgba[o + 2U]) / 255.0f,
        },
        .alpha = static_cast<float>(tex.rgba[o + 3U]) / 255.0f,
    };
}

ri::math::Vec2 ApplyTextureAtlasFrame(const ri::scene::Material& material,
                                      const ScenePreviewOptions& options,
                                      float u,
                                      float v) {
    const int columns = std::max(1, material.baseColorTextureAtlasColumns);
    const int rows = std::max(1, material.baseColorTextureAtlasRows);
    const int frameCapacity = columns * rows;
    const int frameCount = material.baseColorTextureAtlasFrameCount > 0
        ? std::min(material.baseColorTextureAtlasFrameCount, frameCapacity)
        : frameCapacity;
    if (columns <= 1 && rows <= 1) {
        return ri::math::Vec2{u, v};
    }

    int frame = 0;
    if (frameCount > 1 && material.baseColorTextureAtlasFramesPerSecond > 0.0f) {
        const double cursor =
            std::floor(std::max(0.0, options.animationTimeSeconds) * material.baseColorTextureAtlasFramesPerSecond);
        frame = static_cast<int>(static_cast<long long>(cursor) % static_cast<long long>(frameCount));
    }
    const int column = frame % columns;
    const int row = frame / columns;
    const float localU = u - std::floor(u);
    const float localV = v - std::floor(v);
    return ri::math::Vec2{
        (static_cast<float>(column) + localU) / static_cast<float>(columns),
        (static_cast<float>(row) + localV) / static_cast<float>(rows),
    };
}

TextureSample SampleTextureRepeat(const RgbaImage& tex,
                                  const ri::scene::Material& material,
                                  const ScenePreviewOptions& options,
                                  float u,
                                  float v,
                                  bool pointSample) {
    const ri::math::Vec2 atlasUv = ApplyTextureAtlasFrame(material, options, u, v);
    u = atlasUv.x;
    v = atlasUv.y;
    u = u - std::floor(u);
    v = v - std::floor(v);
    const float fx = u * static_cast<float>(tex.width - 1);
    const float fy = v * static_cast<float>(tex.height - 1);
    if (pointSample) {
        const int x = static_cast<int>(std::floor(fx + 0.5f));
        const int y = static_cast<int>(std::floor(fy + 0.5f));
        return SampleTexturePixelWrapped(tex, x, y);
    }

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    const TextureSample c00 = SampleTexturePixelWrapped(tex, x0, y0);
    const TextureSample c10 = SampleTexturePixelWrapped(tex, x1, y0);
    const TextureSample c01 = SampleTexturePixelWrapped(tex, x0, y1);
    const TextureSample c11 = SampleTexturePixelWrapped(tex, x1, y1);
    const ri::math::Vec3 cx0 = ri::math::Lerp(c00.color, c10.color, tx);
    const ri::math::Vec3 cx1 = ri::math::Lerp(c01.color, c11.color, tx);
    const float ax0 = c00.alpha + ((c10.alpha - c00.alpha) * tx);
    const float ax1 = c01.alpha + ((c11.alpha - c01.alpha) * tx);
    return TextureSample{
        .color = ClampColor(ri::math::Lerp(cx0, cx1, ty)),
        .alpha = Clamp01(ax0 + ((ax1 - ax0) * ty)),
    };
}

void RasterizeTriangleProjected(SoftwareImage& image,
                                std::vector<float>& depthBuffer,
                                const ScenePreviewOptions& options,
                                const ScreenVertex& screenA,
                                const ScreenVertex& screenB,
                                const ScreenVertex& screenC,
                                const ri::scene::Material& material,
                                const ri::math::Vec3& modulate,
                                const RgbaImage* texture,
                                const CameraBasis& camera) {
    (void)camera;
    const float area = EdgeFunction(screenA.x, screenA.y, screenB.x, screenB.y, screenC.x, screenC.y);
    if (std::fabs(area) <= 0.0001f) {
        return;
    }
    const float invArea = 1.0f / area;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({screenA.x, screenB.x, screenC.x}))));
    const int maxX = std::min(options.width - 1, static_cast<int>(std::ceil(std::max({screenA.x, screenB.x, screenC.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({screenA.y, screenB.y, screenC.y}))));
    const int maxY = std::min(options.height - 1, static_cast<int>(std::ceil(std::max({screenA.y, screenB.y, screenC.y}))));

    const float za = screenA.depth;
    const float zb = screenB.depth;
    const float zc = screenC.depth;
    const float invZa = 1.0f / std::max(za, 1e-4f);
    const float invZb = 1.0f / std::max(zb, 1e-4f);
    const float invZc = 1.0f / std::max(zc, 1e-4f);
    const float w0StepX = (screenC.y - screenB.y) * invArea;
    const float w1StepX = (screenA.y - screenC.y) * invArea;
    const float w2StepX = (screenB.y - screenA.y) * invArea;
    const float w0StepY = (screenB.x - screenC.x) * invArea;
    const float w1StepY = (screenC.x - screenA.x) * invArea;
    const float w2StepY = (screenA.x - screenB.x) * invArea;
    const float originX = static_cast<float>(minX) + 0.5f;
    const float originY = static_cast<float>(minY) + 0.5f;
    const float rowW0Start = EdgeFunction(screenB.x, screenB.y, screenC.x, screenC.y, originX, originY) * invArea;
    const float rowW1Start = EdgeFunction(screenC.x, screenC.y, screenA.x, screenA.y, originX, originY) * invArea;
    const float rowW2Start = EdgeFunction(screenA.x, screenA.y, screenB.x, screenB.y, originX, originY) * invArea;

    float rowW0 = rowW0Start;
    float rowW1 = rowW1Start;
    float rowW2 = rowW2Start;
    for (int y = minY; y <= maxY; ++y) {
        float w0 = rowW0;
        float w1 = rowW1;
        float w2 = rowW2;
        for (int x = minX; x <= maxX; ++x) {
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                w0 += w0StepX;
                w1 += w1StepX;
                w2 += w2StepX;
                continue;
            }

            const float wInvZ = w0 * invZa + w1 * invZb + w2 * invZc;
            if (wInvZ <= 1e-8f) {
                w0 += w0StepX;
                w1 += w1StepX;
                w2 += w2StepX;
                continue;
            }
            // Eye-space z must use the same 1/z interpolation as UVs; linear z in screen space causes
            // diagonal moiré / wrong occlusion when textures are perspective-correct.
            const float depth = 1.0f / wInvZ;
            const std::size_t pixelIndex = static_cast<std::size_t>(y * options.width + x);
            if (depth >= depthBuffer[pixelIndex]) {
                w0 += w0StepX;
                w1 += w1StepX;
                w2 += w2StepX;
                continue;
            }

            depthBuffer[pixelIndex] = depth;

            float u = 0.0f;
            float v = 0.0f;
            if (options.affineTextureMapping || texture == nullptr) {
                u = w0 * screenA.uv.x + w1 * screenB.uv.x + w2 * screenC.uv.x;
                v = w0 * screenA.uv.y + w1 * screenB.uv.y + w2 * screenC.uv.y;
            } else {
                u = (w0 * screenA.uv.x * invZa + w1 * screenB.uv.x * invZb + w2 * screenC.uv.x * invZc) / wInvZ;
                v = (w0 * screenA.uv.y * invZa + w1 * screenB.uv.y * invZb + w2 * screenC.uv.y * invZc) / wInvZ;
            }

            TextureSample sample{};
            if (texture != nullptr && texture->Valid()) {
                const bool samplePoint = options.pointSampleTextures
                    || (options.adaptiveTextureSampling && depth >= options.adaptivePointSampleStartDepth);
                sample = SampleTextureRepeat(*texture, material, options, u, v, samplePoint);
            }
            const ri::math::Vec3 shaded = MultiplyColor(modulate, sample.color);
            const float fogFactor = Clamp01((depth - 3.0f) / 14.0f);
            ri::math::Vec3 out = ClampColor(shaded + material.emissiveColor);
            out = ClampColor(ri::math::Lerp(out, options.fogColor, fogFactor * fogFactor * 0.72f));
            const bool texturedSample = texture != nullptr && texture->Valid();
            if (options.orderedDither && !texturedSample) {
                const float nudge = DitherNudge(x, y);
                constexpr float step = 1.0f / 255.0f;
                out = ClampColor(out + ri::math::Vec3{nudge * step, nudge * step, nudge * step});
            }
            const std::size_t colorOffset = pixelIndex * 3U;
            const float alpha = Clamp01(material.opacity * sample.alpha);
            if (material.transparent || alpha < 0.999f) {
                const ri::math::Vec3 destination{
                    static_cast<float>(image.pixels[colorOffset + 0U]) / 255.0f,
                    static_cast<float>(image.pixels[colorOffset + 1U]) / 255.0f,
                    static_cast<float>(image.pixels[colorOffset + 2U]) / 255.0f,
                };
                out = ClampColor(ri::math::Lerp(destination, out, alpha));
            }
            image.pixels[colorOffset + 0U] = ToByte(out.x);
            image.pixels[colorOffset + 1U] = ToByte(out.y);
            image.pixels[colorOffset + 2U] = ToByte(out.z);

            w0 += w0StepX;
            w1 += w1StepX;
            w2 += w2StepX;
        }
        rowW0 += w0StepY;
        rowW1 += w1StepY;
        rowW2 += w2StepY;
    }
}

template <typename DistanceFn>
int ClipPolygonToPlane(const ClipVertex* input,
                       int inputCount,
                       ClipVertex* output,
                       DistanceFn distanceFn) {
    if (inputCount <= 0) {
        return 0;
    }

    int outputCount = 0;
    for (int i = 0; i < inputCount; ++i) {
        const ClipVertex& current = input[static_cast<std::size_t>(i)];
        const ClipVertex& previous = input[static_cast<std::size_t>((i + inputCount - 1) % inputCount)];
        const float dCurrent = distanceFn(current.p);
        const float dPrevious = distanceFn(previous.p);
        const bool currentInside = dCurrent <= 0.0f;
        const bool previousInside = dPrevious <= 0.0f;

        if (currentInside != previousInside) {
            const float denom = dPrevious - dCurrent;
            const float t = std::fabs(denom) > 0.000001f ? (dPrevious / denom) : 0.0f;
            if (outputCount < 12) {
                ClipVertex edge{};
                edge.p = previous.p + ((current.p - previous.p) * t);
                edge.uv = previous.uv + ((current.uv - previous.uv) * t);
                output[static_cast<std::size_t>(outputCount++)] = edge;
            }
        }
        if (currentInside && outputCount < 12) {
            output[static_cast<std::size_t>(outputCount++)] = current;
        }
    }
    return outputCount;
}

void RasterizeTriangleClipped(SoftwareImage& image,
                              std::vector<float>& depthBuffer,
                              const ScenePreviewOptions& options,
                              const CameraBasis& camera,
                              const ClipVertex& va,
                              const ClipVertex& vb,
                              const ClipVertex& vc,
                              const ri::scene::Material& material,
                              const ri::math::Vec3& modulate,
                              const RgbaImage* texture) {
    constexpr float kNearDepth = 0.05f;
    std::array<ClipVertex, 12> clipped{};
    std::array<ClipVertex, 12> scratch{};
    clipped[0] = va;
    clipped[1] = vb;
    clipped[2] = vc;
    int clippedCount = 3;

    const float kx = camera.aspectRatio / camera.focalLength;
    const float ky = 1.0f / camera.focalLength;
    int outCount = ClipPolygonToPlane(
        clipped.data(), clippedCount, scratch.data(), [kNearDepth](const ri::math::Vec3& v) {
            return kNearDepth - v.z;
        });
    if (outCount < 3) {
        return;
    }
    clippedCount = ClipPolygonToPlane(
        scratch.data(), outCount, clipped.data(), [kx](const ri::math::Vec3& v) {
            return v.x - (v.z * kx);
        });
    if (clippedCount < 3) {
        return;
    }
    outCount = ClipPolygonToPlane(
        clipped.data(), clippedCount, scratch.data(), [kx](const ri::math::Vec3& v) {
            return (-v.x) - (v.z * kx);
        });
    if (outCount < 3) {
        return;
    }
    clippedCount = ClipPolygonToPlane(
        scratch.data(), outCount, clipped.data(), [ky](const ri::math::Vec3& v) {
            return v.y - (v.z * ky);
        });
    if (clippedCount < 3) {
        return;
    }
    outCount = ClipPolygonToPlane(
        clipped.data(), clippedCount, scratch.data(), [ky](const ri::math::Vec3& v) {
            return (-v.y) - (v.z * ky);
        });

    if (outCount < 3) {
        return;
    }

    for (int i = 1; i < outCount - 1; ++i) {
        const ClipVertex& a = scratch[0];
        const ClipVertex& b = scratch[static_cast<std::size_t>(i)];
        const ClipVertex& c = scratch[static_cast<std::size_t>(i + 1)];
        const ScreenVertex sa = ProjectPoint(a.p, a.uv, options.width, options.height, camera.focalLength, camera.aspectRatio);
        const ScreenVertex sb = ProjectPoint(b.p, b.uv, options.width, options.height, camera.focalLength, camera.aspectRatio);
        const ScreenVertex sc = ProjectPoint(c.p, c.uv, options.width, options.height, camera.focalLength, camera.aspectRatio);
        RasterizeTriangleProjected(image, depthBuffer, options, sa, sb, sc, material, modulate, texture, camera);
    }
}

using TextureCache = std::unordered_map<std::string, RgbaImage>;

const RgbaImage* ResolveTexture(TextureCache& cache,
                                const fs::path& textureRoot,
                                const ScenePreviewOptions& options,
                                const ri::scene::Material& material) {
    std::string textureName = material.baseColorTexture;
    if (!material.baseColorTextureFrames.empty()) {
        std::size_t frameIndex = 0;
        if (material.baseColorTextureFramesPerSecond > 0.0f && material.baseColorTextureFrames.size() > 1U) {
            const double frameCursor =
                std::floor(std::max(0.0, options.animationTimeSeconds) * material.baseColorTextureFramesPerSecond);
            frameIndex = static_cast<std::size_t>(
                static_cast<long long>(frameCursor) % static_cast<long long>(material.baseColorTextureFrames.size()));
        }
        textureName = material.baseColorTextureFrames[frameIndex];
    }

    if (textureName.empty()) {
        return nullptr;
    }
    const fs::path path = textureRoot / textureName;
    const std::string key = path.generic_string();
    const auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second.Valid() ? &it->second : nullptr;
    }

    RgbaImage loaded = LoadRgbaImageFile(path);
    const auto inserted = cache.emplace(key, std::move(loaded));
    return inserted.first->second.Valid() ? &inserted.first->second : nullptr;
}

fs::path ResolveTextureRoot(const ScenePreviewOptions& options) {
    if (options.textureRoot.has_value() && !options.textureRoot->empty()) {
        const fs::path p = *options.textureRoot;
        if (ri::content::IsEngineTextureLibraryDirectory(p) || fs::is_directory(p)) {
            return p;
        }
    }
    const fs::path def = DefaultEngineTextureRoot();
    // Default path already comes from content resolution (PNG library probe); avoid a second
    // `is_directory` gate that can spuriously fail on some Windows path / junction setups.
    if (!def.empty()) {
        return def;
    }
    return {};
}

void DrawPrimitiveNode(SoftwareImage& image,
                       std::vector<float>& depthBuffer,
                       const ScenePreviewOptions& options,
                       const CameraBasis& camera,
                       const std::vector<ResolvedLight>& lights,
                       TextureCache& textureCache,
                       const fs::path& textureRoot,
                       const ri::scene::Mesh& mesh,
                       const ri::scene::Material& material,
                       const ri::math::Mat4& world) {

    const RgbaImage* texture = nullptr;
    if (!textureRoot.empty()) {
        texture = ResolveTexture(textureCache, textureRoot, options, material);
    }

    const ri::math::Vec2 tiling = material.textureTiling;
    const std::vector<ri::math::Vec3> smoothedNormals =
        (mesh.primitive == ri::scene::PrimitiveType::Custom || mesh.primitive == ri::scene::PrimitiveType::Sphere)
        ? BuildMeshVertexNormals(mesh)
        : std::vector<ri::math::Vec3>{};

    const auto drawQuadWorld = [&](const std::array<ri::math::Vec3, 4>& localVertices,
                                   const std::array<ri::math::Vec2, 4>& localUv) {
        std::array<ri::math::Vec3, 4> worldVertices{};
        std::array<ClipVertex, 4> clipVerts{};
        for (std::size_t index = 0; index < localVertices.size(); ++index) {
            worldVertices[index] = ri::math::TransformPoint(world, localVertices[index]);
            clipVerts[index].p = ToCameraSpace(camera, worldVertices[index]);
            clipVerts[index].uv = ri::math::Vec2{
                localUv[index].x * tiling.x,
                localUv[index].y * tiling.y,
            };
        }

        const ri::math::Vec3 worldNormal = ri::math::Normalize(
            ri::math::Cross(worldVertices[1] - worldVertices[0], worldVertices[2] - worldVertices[0]));
        const ri::math::Vec3 worldCenter =
            (worldVertices[0] + worldVertices[1] + worldVertices[2] + worldVertices[3]) / 4.0f;
        const ri::math::Vec3 modulate = ShadeFace(
            material.baseColor,
            material.shadingModel,
            worldNormal,
            worldCenter,
            camera,
            lights,
            options);

        RasterizeTriangleClipped(
            image, depthBuffer, options, camera, clipVerts[0], clipVerts[1], clipVerts[2], material, modulate, texture);
        RasterizeTriangleClipped(
            image, depthBuffer, options, camera, clipVerts[0], clipVerts[2], clipVerts[3], material, modulate, texture);
    };

    const auto drawTriangleWorld = [&](const ri::math::Vec3& localA,
                                       const ri::math::Vec3& localB,
                                       const ri::math::Vec3& localC,
                                       const ri::math::Vec2& uvA,
                                       const ri::math::Vec2& uvB,
                                       const ri::math::Vec2& uvC,
                                       const ri::math::Vec3* localNormalA = nullptr,
                                       const ri::math::Vec3* localNormalB = nullptr,
                                       const ri::math::Vec3* localNormalC = nullptr) {
        const ri::math::Vec3 worldA = ri::math::TransformPoint(world, localA);
        const ri::math::Vec3 worldB = ri::math::TransformPoint(world, localB);
        const ri::math::Vec3 worldC = ri::math::TransformPoint(world, localC);
        ClipVertex ca{};
        ca.p = ToCameraSpace(camera, worldA);
        ca.uv = ri::math::Vec2{uvA.x * tiling.x, uvA.y * tiling.y};
        ClipVertex cb{};
        cb.p = ToCameraSpace(camera, worldB);
        cb.uv = ri::math::Vec2{uvB.x * tiling.x, uvB.y * tiling.y};
        ClipVertex cc{};
        cc.p = ToCameraSpace(camera, worldC);
        cc.uv = ri::math::Vec2{uvC.x * tiling.x, uvC.y * tiling.y};

        ri::math::Vec3 worldNormal = ri::math::Cross(worldB - worldA, worldC - worldA);
        if (localNormalA != nullptr && localNormalB != nullptr && localNormalC != nullptr) {
            const ri::math::Vec3 worldNormalA = ri::math::Normalize(ri::math::TransformVector(world, *localNormalA));
            const ri::math::Vec3 worldNormalB = ri::math::Normalize(ri::math::TransformVector(world, *localNormalB));
            const ri::math::Vec3 worldNormalC = ri::math::Normalize(ri::math::TransformVector(world, *localNormalC));
            const ri::math::Vec3 blendedNormal = worldNormalA + worldNormalB + worldNormalC;
            if (ri::math::LengthSquared(blendedNormal) > 1e-8f) {
                worldNormal = ri::math::Normalize(blendedNormal);
            }
        }
        const ri::math::Vec3 worldCenter = (worldA + worldB + worldC) / 3.0f;
        const ri::math::Vec3 modulate = ShadeFace(
            material.baseColor,
            material.shadingModel,
            worldNormal,
            worldCenter,
            camera,
            lights,
            options);
        RasterizeTriangleClipped(image, depthBuffer, options, camera, ca, cb, cc, material, modulate, texture);
    };

    switch (mesh.primitive) {
        case ri::scene::PrimitiveType::Cube:
            for (std::size_t face = 0; face < kCubeFaces.size(); ++face) {
                const std::array<int, 4>& faceIdx = kCubeFaces[face];
                std::array<ri::math::Vec3, 4> lv{};
                std::array<ri::math::Vec2, 4> luv{};
                for (int k = 0; k < 4; ++k) {
                    lv[static_cast<std::size_t>(k)] = kCubeVertices[static_cast<std::size_t>(faceIdx[static_cast<std::size_t>(k)])];
                    luv[static_cast<std::size_t>(k)] = kCubeFaceCornerUv[face][static_cast<std::size_t>(k)];
                }
                drawQuadWorld(lv, luv);
            }
            break;
        case ri::scene::PrimitiveType::Plane: {
            std::array<ri::math::Vec2, 4> luv{};
            for (std::size_t i = 0; i < 4; ++i) {
                const ri::math::Vec3& v = kPlaneVertices[i];
                luv[i] = ri::math::Vec2{v.x + 0.5f, v.z + 0.5f};
            }
            drawQuadWorld(kPlaneVertices, luv);
            break;
        }
        case ri::scene::PrimitiveType::Custom: {
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

                if (ia < 0 || ib < 0 || ic < 0 ||
                    ia >= static_cast<int>(mesh.positions.size()) ||
                    ib >= static_cast<int>(mesh.positions.size()) ||
                    ic >= static_cast<int>(mesh.positions.size())) {
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

                drawTriangleWorld(
                    mesh.positions[static_cast<std::size_t>(ia)],
                    mesh.positions[static_cast<std::size_t>(ib)],
                    mesh.positions[static_cast<std::size_t>(ic)],
                    uva,
                    uvb,
                    uvc,
                    smoothedNormals.empty() ? nullptr : &smoothedNormals[static_cast<std::size_t>(ia)],
                    smoothedNormals.empty() ? nullptr : &smoothedNormals[static_cast<std::size_t>(ib)],
                    smoothedNormals.empty() ? nullptr : &smoothedNormals[static_cast<std::size_t>(ic)]);
            }
            break;
        }
        case ri::scene::PrimitiveType::Sphere: {
            if (mesh.positions.empty()) {
                break;
            }
            const bool hasUv = mesh.texCoords.size() == mesh.positions.size();
            const int triangleCount = static_cast<int>(mesh.indices.size() / 3U);
            for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
                const int ia = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 0)];
                const int ib = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 1)];
                const int ic = mesh.indices[static_cast<std::size_t>(triangleIndex * 3 + 2)];
                if (ia < 0 || ib < 0 || ic < 0 ||
                    ia >= static_cast<int>(mesh.positions.size()) ||
                    ib >= static_cast<int>(mesh.positions.size()) ||
                    ic >= static_cast<int>(mesh.positions.size())) {
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

                drawTriangleWorld(
                    mesh.positions[static_cast<std::size_t>(ia)],
                    mesh.positions[static_cast<std::size_t>(ib)],
                    mesh.positions[static_cast<std::size_t>(ic)],
                    uva,
                    uvb,
                    uvc,
                    smoothedNormals.empty() ? nullptr : &smoothedNormals[static_cast<std::size_t>(ia)],
                    smoothedNormals.empty() ? nullptr : &smoothedNormals[static_cast<std::size_t>(ib)],
                    smoothedNormals.empty() ? nullptr : &smoothedNormals[static_cast<std::size_t>(ic)]);
            }
            break;
        }
    }
}

} // namespace

fs::path DefaultEngineTextureRoot() {
    return ri::content::ResolveEngineTexturesDirectory({});
}

void RenderScenePreviewInto(const ri::scene::Scene& scene,
                            int cameraNodeHandle,
                            const ScenePreviewOptions& rawOptions,
                            SoftwareImage& outImage,
                            ScenePreviewCache* cache) {
    ScenePreviewOptions options = rawOptions;
    options.width = std::clamp(options.width, 64, 2048);
    options.height = std::clamp(options.height, 64, 2048);

    outImage.width = options.width;
    outImage.height = options.height;
    outImage.pixels.resize(static_cast<std::size_t>(options.width * options.height * 3), 0);
    FillGradientBackground(outImage, options);

    if (cameraNodeHandle == ri::scene::kInvalidHandle) {
        return;
    }

    const CameraBasis camera = BuildCameraBasis(scene, cameraNodeHandle, options);
    const std::vector<ResolvedLight> lights = ResolveLights(scene);
    std::vector<float> localDepthBuffer{};
    std::vector<float>& depthBuffer = cache != nullptr ? cache->depthBuffer : localDepthBuffer;
    depthBuffer.assign(static_cast<std::size_t>(options.width * options.height), std::numeric_limits<float>::max());
    TextureCache localTextureCache{};
    TextureCache& textureCache = cache != nullptr ? cache->textures : localTextureCache;
    const fs::path textureRoot = ResolveTextureRoot(options);
    std::vector<std::optional<MeshCullBounds>> meshCullCache(scene.MeshCount());

    for (const int nodeHandle : scene.GetRenderableNodeHandles()) {
        if (IsHiddenPreviewNode(scene, nodeHandle, options)) {
            continue;
        }
        const ri::scene::Node& node = scene.GetNode(nodeHandle);
        if (node.mesh == ri::scene::kInvalidHandle || node.material == ri::scene::kInvalidHandle) {
            continue;
        }
        const ri::scene::Mesh& mesh = scene.GetMesh(node.mesh);
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
        if (!meshCullCache[node.mesh].has_value()) {
            meshCullCache[node.mesh] = BuildMeshCullBounds(mesh);
        }
        const MeshCullBounds& cull = *meshCullCache[node.mesh];
        if (cull.valid) {
            const ri::math::Vec3 worldCullCenter = ri::math::TransformPoint(world, cull.center);
            const ri::math::Vec3 viewCullCenter = ToCameraSpace(camera, worldCullCenter);
            const ri::math::Vec3 worldScale = ri::math::ExtractScale(world);
            const float worldRadius = cull.radius
                * std::max({std::fabs(worldScale.x), std::fabs(worldScale.y), std::fabs(worldScale.z), 0.0001f});
            if (!IsSphereVisibleInCamera(camera, viewCullCenter, worldRadius)) {
                continue;
            }
            if (options.enableFarHorizon) {
                const float distance = std::max(viewCullCenter.z - worldRadius, 0.0f);
                if (distance > options.farHorizonMaxDistance) {
                    continue;
                }
                const std::uint32_t stride = ComputeFarHorizonStride(
                    distance,
                    options,
                    options.farHorizonMaxNodeStride);
                if (stride > 1U && (static_cast<std::uint32_t>(nodeHandle) % stride) != 0U) {
                    continue;
                }
            }
        }
        DrawPrimitiveNode(outImage,
                          depthBuffer,
                          options,
                          camera,
                          lights,
                          textureCache,
                          textureRoot,
                          mesh,
                          scene.GetMaterial(node.material),
                          world);
    }

    for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
        const ri::scene::MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
        if (IsHiddenPreviewNode(scene, batch.parent, options)) {
            continue;
        }
        if (batch.mesh == ri::scene::kInvalidHandle || batch.material == ri::scene::kInvalidHandle) {
            continue;
        }
        const ri::scene::Mesh& mesh = scene.GetMesh(batch.mesh);
        const ri::scene::Material& material = scene.GetMaterial(batch.material);
        const ri::math::Mat4 parentWorld = batch.parent != ri::scene::kInvalidHandle
            ? scene.ComputeWorldMatrix(batch.parent)
            : ri::math::IdentityMatrix();
        if (!meshCullCache[batch.mesh].has_value()) {
            meshCullCache[batch.mesh] = BuildMeshCullBounds(mesh);
        }
        const MeshCullBounds& cull = *meshCullCache[batch.mesh];
        for (std::size_t transformIndex = 0; transformIndex < batch.transforms.size(); ++transformIndex) {
            const ri::scene::Transform& transform = batch.transforms[transformIndex];
            const ri::math::Mat4 world = ri::math::Multiply(parentWorld, transform.LocalMatrix());
            if (cull.valid) {
                const ri::math::Vec3 worldCullCenter = ri::math::TransformPoint(world, cull.center);
                const ri::math::Vec3 viewCullCenter = ToCameraSpace(camera, worldCullCenter);
                const ri::math::Vec3 worldScale = ri::math::ExtractScale(world);
                const float worldRadius = cull.radius
                    * std::max({std::fabs(worldScale.x), std::fabs(worldScale.y), std::fabs(worldScale.z), 0.0001f});
                if (!IsSphereVisibleInCamera(camera, viewCullCenter, worldRadius)) {
                    continue;
                }
                if (options.enableFarHorizon) {
                    const float distance = std::max(viewCullCenter.z - worldRadius, 0.0f);
                    if (distance > options.farHorizonMaxDistance) {
                        continue;
                    }
                    const std::uint32_t stride = ComputeFarHorizonStride(
                        distance,
                        options,
                        options.farHorizonMaxInstanceStride);
                    if (stride > 1U
                        && (static_cast<std::uint32_t>(batchIndex) + static_cast<std::uint32_t>(transformIndex)) % stride
                            != 0U) {
                        continue;
                    }
                }
            }
            DrawPrimitiveNode(outImage,
                              depthBuffer,
                              options,
                              camera,
                              lights,
                              textureCache,
                              textureRoot,
                              mesh,
                              material,
                              world);
        }
    }
}

SoftwareImage RenderScenePreview(const ri::scene::Scene& scene,
                                 int cameraNodeHandle,
                                 const ScenePreviewOptions& rawOptions,
                                 ScenePreviewCache* cache) {
    SoftwareImage image{};
    RenderScenePreviewInto(scene, cameraNodeHandle, rawOptions, image, cache);
    return image;
}

} // namespace ri::render::software
