#include "RawIron/XR/HardwareSceneBuilder.h"
#include "RawIron/Render/PreviewTexture.h"
#include "RawIron/Math/Mat4.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace ri::xr {
constexpr std::array<ri::math::Vec3, 8> kCubeVertices{{
    {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
    {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
    {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
    {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}}};
constexpr std::array<std::array<int, 4>, 6> kCubeFaces{{
    {4, 5, 6, 7}, {1, 0, 3, 2}, {0, 4, 7, 3},
    {5, 1, 2, 6}, {3, 7, 6, 2}, {0, 1, 5, 4}}};
constexpr std::array<ri::math::Vec3, 4> kPlaneVertices{{
    {-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, -0.5f},
    {0.5f, 0.0f, 0.5f}, {-0.5f, 0.0f, 0.5f}}};

// Desktop and PCVR consume the same Material paths. The OpenXR host packs those paths into one
// immutable atlas at session startup; normal maps use a distinct key because they need linear
// decode rather than albedo's sRGB decode.
[[nodiscard]] std::string NormalAtlasKey(const std::string& texturePath) {
    return "normal:" + texturePath;
}

HardwareTextureAtlas BuildHardwareTextureAtlas(const ri::scene::Scene& scene) {
    HardwareTextureAtlas atlas{};
    int cellIndex = 0;
    const auto copyTexture = [&](const std::string& texturePath, const bool normalMap) {
        const std::string atlasKey = normalMap ? NormalAtlasKey(texturePath) : texturePath;
        if (texturePath.empty() || atlas.rects.contains(atlasKey)) return;
        if (cellIndex >= HardwareTextureAtlas::kGrid * HardwareTextureAtlas::kGrid) {
            atlas.errors.push_back("XR atlas capacity exceeded: " + texturePath); return;
        }
        const ri::render::software::RgbaImage image =
            ri::render::software::LoadRgbaImageFile(std::filesystem::path(texturePath));
        if (!image.Valid()) { atlas.errors.push_back("XR texture decode failed: " + texturePath); return; }
        const int cellX = (cellIndex % HardwareTextureAtlas::kGrid) * HardwareTextureAtlas::kCellSize;
        const int cellY = (cellIndex / HardwareTextureAtlas::kGrid) * HardwareTextureAtlas::kCellSize;
        for (int y = 0; y < HardwareTextureAtlas::kCellSize; ++y) {
            const int sourceY = (std::min)(
                y * image.height / HardwareTextureAtlas::kCellSize, image.height - 1);
            for (int x = 0; x < HardwareTextureAtlas::kCellSize; ++x) {
                const int sourceX = (std::min)(
                    x * image.width / HardwareTextureAtlas::kCellSize, image.width - 1);
                const std::size_t sourceOffset = static_cast<std::size_t>((sourceY * image.width + sourceX) * 4);
                const std::size_t destinationOffset = static_cast<std::size_t>(
                    ((cellY + y) * HardwareTextureAtlas::kSize + cellX + x) * 4);
                if (!normalMap) {
                    std::copy_n(image.rgba.data() + sourceOffset, 4, atlas.rgba.data() + destinationOffset);
                } else {
                    // The atlas image is sRGB. Encode the normally-linear tangent vector first,
                    // so sampling returns its original 0..1 vector components in the shader.
                    for (int channel = 0; channel < 3; ++channel) {
                        const float linear = static_cast<float>(image.rgba[sourceOffset + channel]) / 255.0f;
                        const float srgb = linear <= 0.0031308f
                            ? linear * 12.92f
                            : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
                        atlas.rgba[destinationOffset + channel] = static_cast<std::uint8_t>(
                            std::clamp(std::lround(srgb * 255.0f), 0L, 255L));
                    }
                    atlas.rgba[destinationOffset + 3U] = image.rgba[sourceOffset + 3U];
                }
            }
        }
        const float inverseSize = 1.0f / static_cast<float>(HardwareTextureAtlas::kSize);
        atlas.rects.emplace(atlasKey, std::array<float, 4>{
            (static_cast<float>(cellX) + 0.5f) * inverseSize,
            (static_cast<float>(cellY) + 0.5f) * inverseSize,
            (static_cast<float>(cellX + HardwareTextureAtlas::kCellSize) - 0.5f) * inverseSize,
            (static_cast<float>(cellY + HardwareTextureAtlas::kCellSize) - 0.5f) * inverseSize});
        ++cellIndex;
        ++atlas.loadedTextures;
    };
    for (std::size_t materialIndex = 0; materialIndex < scene.MaterialCount(); ++materialIndex) {
        const ri::scene::Material& material = scene.GetMaterial(static_cast<int>(materialIndex));
        copyTexture(material.baseColorTexture, false);
        copyTexture(material.normalTexture, true);
    }
    return atlas;
}

void AppendHardwareTriangle(std::vector<ri::xr::HardwareSceneVertex>& output,
                            const ri::math::Mat4& world,
                            const ri::math::Vec3& a,
                            const ri::math::Vec3& b,
                            const ri::math::Vec3& c,
                            const ri::math::Vec3& baseColor,
                            const std::array<ri::math::Vec2, 3>& texCoords,
                            const std::array<float, 4>& atlasRect,
                            const std::array<float, 4>& normalAtlasRect,
                            const ri::scene::Material& material,
                            const std::array<ri::math::Vec3, 3>& localNormals = {}) {
    const ri::math::Vec3 positions[]{
        ri::math::TransformPoint(world, a),
        ri::math::TransformPoint(world, b),
        ri::math::TransformPoint(world, c)};
    ri::math::Vec3 normal = ri::math::Cross(positions[1] - positions[0], positions[2] - positions[0]);
    if (ri::math::LengthSquared(normal) > 1.0e-10f) normal = ri::math::Normalize(normal);
    for (std::size_t index = 0; index < 3U; ++index) {
        const ri::math::Vec3& position = positions[index];
        ri::math::Vec3 vertexNormal = localNormals[index];
        if (ri::math::LengthSquared(vertexNormal) > 1.0e-10f) {
            vertexNormal = ri::math::TransformNormal(world, vertexNormal);
            vertexNormal = ri::math::LengthSquared(vertexNormal) > 1.0e-10f
                ? ri::math::Normalize(vertexNormal)
                : normal;
        } else {
            vertexNormal = normal;
        }
        output.push_back({
            {position.x, position.y, position.z},
            {vertexNormal.x, vertexNormal.y, vertexNormal.z},
            {baseColor.x, baseColor.y, baseColor.z},
            {texCoords[index].x, texCoords[index].y},
            {atlasRect[0], atlasRect[1], atlasRect[2], atlasRect[3]},
            {normalAtlasRect[0], normalAtlasRect[1], normalAtlasRect[2], normalAtlasRect[3]},
            {material.metallic, material.roughness, material.normalScale.x, material.normalScale.y}});
    }
}

void AppendHardwareMesh(std::vector<ri::xr::HardwareSceneVertex>& output,
                        const ri::scene::Mesh& mesh,
                        const ri::scene::Material& material,
                        const ri::math::Mat4& world,
                        const HardwareTextureAtlas& atlas) {
    const ri::math::Vec3 color{
        std::clamp(material.baseColor.x + material.emissiveColor.x, 0.0f, 1.0f),
        std::clamp(material.baseColor.y + material.emissiveColor.y, 0.0f, 1.0f),
        std::clamp(material.baseColor.z + material.emissiveColor.z, 0.0f, 1.0f)};
    const auto rectIt = atlas.rects.find(material.baseColorTexture);
    const std::array<float, 4> atlasRect = rectIt != atlas.rects.end()
        ? rectIt->second
        : std::array<float, 4>{};
    const auto normalRectIt = atlas.rects.find(NormalAtlasKey(material.normalTexture));
    const std::array<float, 4> normalAtlasRect = normalRectIt != atlas.rects.end()
        ? normalRectIt->second
        : std::array<float, 4>{};
    if (!mesh.positions.empty()
        && mesh.geometryMode != ri::scene::MeshGeometryMode::CameraFacingSpriteQuads) {
        const bool indexed = mesh.indices.size() >= 3U;
        const std::size_t triangleCount = indexed ? mesh.indices.size() / 3U : mesh.positions.size() / 3U;
        for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
            const int ia = indexed ? mesh.indices[triangle * 3U] : static_cast<int>(triangle * 3U);
            const int ib = indexed ? mesh.indices[triangle * 3U + 1U] : static_cast<int>(triangle * 3U + 1U);
            const int ic = indexed ? mesh.indices[triangle * 3U + 2U] : static_cast<int>(triangle * 3U + 2U);
            if (ia < 0 || ib < 0 || ic < 0
                || static_cast<std::size_t>(ia) >= mesh.positions.size()
                || static_cast<std::size_t>(ib) >= mesh.positions.size()
                || static_cast<std::size_t>(ic) >= mesh.positions.size()) continue;
            const auto uv = [&](const int index) {
                if (mesh.texCoords.size() == mesh.positions.size()) {
                    const ri::math::Vec2 authored = mesh.texCoords[static_cast<std::size_t>(index)];
                    return ri::math::Vec2{
                        authored.x * material.textureTiling.x,
                        authored.y * material.textureTiling.y};
                }
                return ri::math::Vec2{};
            };
            AppendHardwareTriangle(
                output,
                world,
                mesh.positions[ia],
                mesh.positions[ib],
                mesh.positions[ic],
                color,
                {uv(ia), uv(ib), uv(ic)},
                atlasRect,
                normalAtlasRect,
                material,
                {mesh.normals.size() == mesh.positions.size() ? mesh.normals[static_cast<std::size_t>(ia)] : ri::math::Vec3{},
                 mesh.normals.size() == mesh.positions.size() ? mesh.normals[static_cast<std::size_t>(ib)] : ri::math::Vec3{},
                 mesh.normals.size() == mesh.positions.size() ? mesh.normals[static_cast<std::size_t>(ic)] : ri::math::Vec3{}});
        }
        return;
    }
    if (mesh.primitive == ri::scene::PrimitiveType::Cube) {
        for (const auto& face : kCubeFaces) {
            AppendHardwareTriangle(
                output, world, kCubeVertices[face[0]], kCubeVertices[face[1]], kCubeVertices[face[2]], color,
                {{{0.0f, 0.0f}, {material.textureTiling.x, 0.0f}, material.textureTiling}}, atlasRect, normalAtlasRect, material);
            AppendHardwareTriangle(
                output, world, kCubeVertices[face[0]], kCubeVertices[face[2]], kCubeVertices[face[3]], color,
                {{{0.0f, 0.0f}, material.textureTiling, {0.0f, material.textureTiling.y}}}, atlasRect, normalAtlasRect, material);
        }
    } else if (mesh.primitive == ri::scene::PrimitiveType::Plane) {
        AppendHardwareTriangle(
            output, world, kPlaneVertices[0], kPlaneVertices[1], kPlaneVertices[2], color,
            {{{0.0f, 0.0f}, {material.textureTiling.x, 0.0f}, material.textureTiling}}, atlasRect, normalAtlasRect, material);
        AppendHardwareTriangle(
            output, world, kPlaneVertices[0], kPlaneVertices[2], kPlaneVertices[3], color,
            {{{0.0f, 0.0f}, material.textureTiling, {0.0f, material.textureTiling.y}}}, atlasRect, normalAtlasRect, material);
    }
}

std::vector<ri::xr::HardwareSceneVertex> BuildHardwareScene(
    const ri::scene::Scene& scene,
    const HardwareTextureAtlas& atlas,
    const std::vector<int>& excludedNodes) {
    std::vector<ri::xr::HardwareSceneVertex> vertices{};
    vertices.reserve(scene.GetRenderableNodeHandles().size() * 36U);
    for (const int nodeHandle : scene.GetRenderableNodeHandles()) {
        if (std::ranges::find(excludedNodes, nodeHandle) != excludedNodes.end()) continue;
        const ri::scene::Node& node = scene.GetNode(nodeHandle);
        if (node.mesh < 0 || node.material < 0) continue;
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
        AppendHardwareMesh(
            vertices,
            scene.GetMesh(node.mesh),
            scene.GetMaterial(node.material),
            world,
            atlas);
    }
    for (std::size_t batchIndex = 0; batchIndex < scene.MeshInstanceBatchCount(); ++batchIndex) {
        const ri::scene::MeshInstanceBatch& batch = scene.GetMeshInstanceBatch(static_cast<int>(batchIndex));
        if (batch.mesh < 0 || batch.material < 0) continue;
        const ri::math::Mat4 parentWorld = batch.parent >= 0
            ? scene.ComputeWorldMatrix(batch.parent)
            : ri::math::IdentityMatrix();
        for (const ri::scene::Transform& transform : batch.transforms) {
            const ri::math::Mat4 world = ri::math::Multiply(parentWorld, transform.LocalMatrix());
                AppendHardwareMesh(
                vertices,
                scene.GetMesh(batch.mesh),
                scene.GetMaterial(batch.material),
                world,
                atlas);
        }
    }
    return vertices;
}

} // namespace ri::xr
