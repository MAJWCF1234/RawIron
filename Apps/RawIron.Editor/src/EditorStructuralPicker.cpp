#include "EditorStructuralPicker.h"

#include "EditorAuthoringCatalog.h"
#include "EditorRenderer.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Structural/StructuralPrimitives.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace ri::editor {

namespace {

using ri::scene::AddLightNode;
using ri::scene::AddOrbitCamera;
using ri::scene::AddStructuralBrushNode;
using ri::scene::kStructuralPrimitivePresets;
using ri::scene::LightNodeOptions;
using ri::scene::LightType;
using ri::scene::OrbitCameraOptions;
using ri::scene::OrbitCameraState;
using ri::scene::ProjectionType;
using ri::scene::SetOrbitCameraState;
using ri::scene::ShapeFromStructuralPreset;
using ri::scene::StarterScene;
using ri::scene::StructuralBrushSpawnOptions;

[[nodiscard]] ri::math::Vec3 ThumbnailScaleForPreset(const ri::scene::StructuralPrimitivePreset& preset) {
    const std::string_view label = preset.label;
    if (label.find("terrain") != std::string_view::npos || label.find("heightmap") != std::string_view::npos
        || label.find("displacement") != std::string_view::npos) {
        return ri::math::Vec3{1.4f, 0.35f, 1.4f};
    }
    if (label.find("water") != std::string_view::npos || label.find("plane") != std::string_view::npos) {
        return ri::math::Vec3{1.6f, 0.08f, 1.6f};
    }
    if (label.find("cable") != std::string_view::npos || label.find("catenary") != std::string_view::npos
        || label.find("spline") != std::string_view::npos) {
        return ri::math::Vec3{1.2f, 0.35f, 1.2f};
    }
    if (label.find("stairs") != std::string_view::npos || label.find("catwalk") != std::string_view::npos
        || label.find("landing") != std::string_view::npos || label.find("handrail") != std::string_view::npos) {
        return ri::math::Vec3{1.1f, 0.9f, 1.1f};
    }
    if (label.find("corridor") != std::string_view::npos || label.find("vault") != std::string_view::npos
        || label.find("colonnade") != std::string_view::npos) {
        return ri::math::Vec3{1.35f, 0.85f, 1.35f};
    }
    if (label.find("sphere") != std::string_view::npos || label.find("geodesic") != std::string_view::npos) {
        return ri::math::Vec3{0.95f, 0.95f, 0.95f};
    }
    if (label.find("arch") != std::string_view::npos || label.find("pipe") != std::string_view::npos) {
        return ri::math::Vec3{1.15f, 1.15f, 1.15f};
    }
    return ri::math::Vec3{1.0f, 1.0f, 1.0f};
}

[[nodiscard]] ri::math::Vec3 RotateThumbnailPoint(const ri::math::Vec3& point) {
    constexpr float kYawRadians = 38.0f * 3.14159265358979323846f / 180.0f;
    constexpr float kPitchRadians = -21.0f * 3.14159265358979323846f / 180.0f;
    const float yawCos = std::cos(kYawRadians);
    const float yawSin = std::sin(kYawRadians);
    const float pitchCos = std::cos(kPitchRadians);
    const float pitchSin = std::sin(kPitchRadians);

    const float yawX = point.x * yawCos + point.z * yawSin;
    const float yawZ = -point.x * yawSin + point.z * yawCos;
    const float pitchY = point.y * pitchCos - yawZ * pitchSin;
    return ri::math::Vec3{yawX, pitchY, 0.0f};
}

[[nodiscard]] std::uint64_t QuantizePositionKey(const ri::math::Vec3& point) {
    const std::int32_t x = static_cast<std::int32_t>(std::lround(point.x * 8192.0f));
    const std::int32_t y = static_cast<std::int32_t>(std::lround(point.y * 8192.0f));
    const std::int32_t z = static_cast<std::int32_t>(std::lround(point.z * 8192.0f));
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 42)
         | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 21)
         | static_cast<std::uint64_t>(static_cast<std::uint32_t>(z));
}

[[nodiscard]] std::uint64_t MakeEdgeKey(const std::uint64_t a, const std::uint64_t b) {
    return a < b ? ((a << 32) | b) : ((b << 32) | a);
}

void SetWireframePixel(ri::render::software::SoftwareImage& image,
                       const int x,
                       const int y,
                       const ri::math::Vec3& color) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        return;
    }
    const std::size_t offset = static_cast<std::size_t>((y * image.width + x) * 3);
    image.pixels[offset + 0] = static_cast<std::uint8_t>(std::clamp(color.x * 255.0f, 0.0f, 255.0f));
    image.pixels[offset + 1] = static_cast<std::uint8_t>(std::clamp(color.y * 255.0f, 0.0f, 255.0f));
    image.pixels[offset + 2] = static_cast<std::uint8_t>(std::clamp(color.z * 255.0f, 0.0f, 255.0f));
}

void DrawWireframeLine(ri::render::software::SoftwareImage& image,
                       int x0,
                       int y0,
                       int x1,
                       int y1,
                       const ri::math::Vec3& color) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        SetWireframePixel(image, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int error2 = error * 2;
        if (error2 >= dy) {
            error += dy;
            x0 += sx;
        }
        if (error2 <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

[[nodiscard]] ri::render::software::SoftwareImage RenderWireframeMeshThumbnail(
    const ri::structural::CompiledMesh& mesh,
    const ri::math::Vec3& wireColor,
    const ri::math::Vec3& scale,
    const ri::math::Vec3& offset,
    const int width,
    const int height) {
    ri::render::software::SoftwareImage image{};
    image.width = width;
    image.height = height;
    image.pixels.assign(static_cast<std::size_t>(width * height * 3), 0);

    const ri::math::Vec3 clearTop = wireColor * 0.12f + ri::math::Vec3{0.04f, 0.05f, 0.07f};
    const ri::math::Vec3 clearBottom = wireColor * 0.18f + ri::math::Vec3{0.07f, 0.08f, 0.11f};
    for (int y = 0; y < height; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(std::max(1, height - 1));
        const ri::math::Vec3 background = clearTop * (1.0f - t) + clearBottom * t;
        for (int x = 0; x < width; ++x) {
            SetWireframePixel(image, x, y, background);
        }
    }

    if (mesh.positions.size() < 3 || mesh.triangleCount == 0) {
        return image;
    }

    std::vector<ri::math::Vec2> projected;
    projected.reserve(mesh.positions.size());
    float minX = 1.0e9f;
    float maxX = -1.0e9f;
    float minY = 1.0e9f;
    float maxY = -1.0e9f;
    for (const ri::math::Vec3& position : mesh.positions) {
        const ri::math::Vec3 world{
            position.x * scale.x + offset.x,
            position.y * scale.y + offset.y,
            position.z * scale.z + offset.z,
        };
        const ri::math::Vec3 rotated = RotateThumbnailPoint(world);
        projected.push_back(ri::math::Vec2{rotated.x, -rotated.y});
        minX = std::min(minX, projected.back().x);
        maxX = std::max(maxX, projected.back().x);
        minY = std::min(minY, projected.back().y);
        maxY = std::max(maxY, projected.back().y);
    }

    const float spanX = std::max(0.001f, maxX - minX);
    const float spanY = std::max(0.001f, maxY - minY);
    constexpr float kMargin = 6.0f;
    const float fitScale =
        std::min((static_cast<float>(width) - kMargin * 2.0f) / spanX,
                 (static_cast<float>(height) - kMargin * 2.0f) / spanY);
    const float centerX = (minX + maxX) * 0.5f;
    const float centerY = (minY + maxY) * 0.5f;
    const float screenCenterX = static_cast<float>(width) * 0.5f;
    const float screenCenterY = static_cast<float>(height) * 0.5f;

    auto toScreen = [&](const ri::math::Vec2& point) -> ri::math::Vec2 {
        return ri::math::Vec2{
            screenCenterX + (point.x - centerX) * fitScale,
            screenCenterY + (point.y - centerY) * fitScale,
        };
    };

    std::unordered_set<std::uint64_t> edges;
    edges.reserve(mesh.triangleCount * 2);
    const std::size_t triangleCount = mesh.triangleCount;
    for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const std::size_t base = triangle * 3;
        if (base + 2 >= mesh.positions.size()) {
            break;
        }
        const std::uint64_t keys[3] = {
            QuantizePositionKey(mesh.positions[base + 0]),
            QuantizePositionKey(mesh.positions[base + 1]),
            QuantizePositionKey(mesh.positions[base + 2]),
        };
        for (int edge = 0; edge < 3; ++edge) {
            const std::uint64_t edgeKey = MakeEdgeKey(keys[edge], keys[(edge + 1) % 3]);
            if (!edges.insert(edgeKey).second) {
                continue;
            }
            const ri::math::Vec2 a = toScreen(projected[base + static_cast<std::size_t>(edge)]);
            const ri::math::Vec2 b = toScreen(projected[base + static_cast<std::size_t>((edge + 1) % 3)]);
            DrawWireframeLine(image,
                              static_cast<int>(std::lround(a.x)),
                              static_cast<int>(std::lround(a.y)),
                              static_cast<int>(std::lround(b.x)),
                              static_cast<int>(std::lround(b.y)),
                              wireColor);
        }
    }

    return image;
}

[[nodiscard]] ri::render::software::SoftwareImage RenderGuideWireframeThumbnail(
    const ri::scene::StructuralPrimitivePreset& preset,
    const int width,
    const int height) {
    const ri::structural::CompiledMesh mesh =
        ri::structural::BuildPrimitiveMesh(preset.structuralType, ShapeFromStructuralPreset(preset));
    return RenderWireframeMeshThumbnail(mesh,
                                        GuideStructuralWireColor(preset.structuralType),
                                        ThumbnailScaleForPreset(preset),
                                        ri::math::Vec3{0.0f, 0.45f, 0.0f},
                                        width,
                                        height);
}

[[nodiscard]] ri::render::software::SoftwareImage RenderBoxWireframeThumbnail(
    const ri::math::Vec3& wireColor,
    const ri::math::Vec3& scale,
    const int width,
    const int height) {
    const ri::structural::CompiledMesh mesh = ri::structural::BuildPrimitiveMesh("box", {});
    return RenderWireframeMeshThumbnail(mesh, wireColor, scale, ri::math::Vec3{0.0f, 0.0f, 0.0f}, width, height);
}

[[nodiscard]] StarterScene BuildPresetThumbnailScene(const ri::scene::StructuralPrimitivePreset& preset) {
    StarterScene starterScene{ri::scene::Scene("StructuralThumb"), {}};
    ri::scene::Scene& scene = starterScene.scene;
    starterScene.handles.root = scene.CreateNode("World");

    LightNodeOptions sun{};
    sun.nodeName = "Sun";
    sun.parent = starterScene.handles.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-42.0f, 34.0f, 0.0f};
    sun.light = ri::scene::Light{
        .name = "Sun",
        .type = LightType::Directional,
        .color = ri::math::Vec3{0.96f, 0.94f, 0.88f},
        .intensity = 3.6f,
    };
    starterScene.handles.sun = AddLightNode(scene, sun);

    OrbitCameraOptions orbitCamera{};
    orbitCamera.parent = starterScene.handles.root;
    orbitCamera.camera = ri::scene::Camera{
        .name = "ThumbCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 52.0f,
        .nearClip = 0.05f,
        .farClip = 64.0f,
    };
    orbitCamera.orbit = OrbitCameraState{
        .target = ri::math::Vec3{0.0f, 0.45f, 0.0f},
        .distance = 2.85f,
        .yawDegrees = 38.0f,
        .pitchDegrees = -21.0f,
    };
    starterScene.handles.orbitCamera = AddOrbitCamera(scene, orbitCamera);

    StructuralBrushSpawnOptions brush{};
    brush.structuralType = preset.structuralType;
    brush.shape = ShapeFromStructuralPreset(preset);
    brush.parent = starterScene.handles.root;
    brush.nodeName = "PreviewPrimitive";
    brush.transform.position = ri::math::Vec3{0.0f, 0.45f, 0.0f};
    brush.transform.scale = ThumbnailScaleForPreset(preset);
    brush.materialName = "picker_preview";
    brush.baseColor = ri::math::Vec3{0.64f, 0.68f, 0.74f};
    brush.baseColorTexture = ri::scene::DefaultStructuralBrushAlbedoTexture();
    brush.baseColor = ri::scene::DefaultStructuralBrushBaseColor();
    brush.textureTiling = ri::math::Vec2{1.5f, 1.5f};
    brush.roughness = 0.82f;
    (void)AddStructuralBrushNode(scene, brush);

    SetOrbitCameraState(scene, starterScene.handles.orbitCamera, orbitCamera.orbit);
    return starterScene;
}

void DrawPlaceholderThumb(HDC dc, const RECT& rect, const std::string& label, HFONT font) {
    EditorRenderer::DrawInsetFrame(dc, rect, RGB(36, 40, 48), RGB(108, 114, 124), RGB(16, 18, 22));
    EditorRenderer::DrawTextLine(dc,
                                 rect,
                                 label,
                                 RGB(188, 194, 204),
                                 font,
                                 DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

} // namespace

[[nodiscard]] bool MatchesStructuralType(std::string_view type, std::string_view candidate) {
    return type == candidate;
}

bool IsGuideStructuralPreset(const std::string_view structuralType) {
    return MatchesStructuralType(structuralType, "plane")
        || MatchesStructuralType(structuralType, "spline_sweep")
        || MatchesStructuralType(structuralType, "revolve")
        || MatchesStructuralType(structuralType, "loft_primitive")
        || MatchesStructuralType(structuralType, "spline_ribbon")
        || MatchesStructuralType(structuralType, "catenary_primitive")
        || MatchesStructuralType(structuralType, "cable_primitive")
        || MatchesStructuralType(structuralType, "thick_polygon_primitive")
        || MatchesStructuralType(structuralType, "trim_sheet_sweep")
        || MatchesStructuralType(structuralType, "water_surface_primitive")
        || MatchesStructuralType(structuralType, "terrain_quad")
        || MatchesStructuralType(structuralType, "heightmap_patch")
        || MatchesStructuralType(structuralType, "displacement")
        || MatchesStructuralType(structuralType, "voronoi_fracture")
        || MatchesStructuralType(structuralType, "metaball_cluster")
        || MatchesStructuralType(structuralType, "lsystem_branch")
        || MatchesStructuralType(structuralType, "lattice_volume");
}

ri::math::Vec3 GuideStructuralWireColor(const std::string_view structuralType) {
    if (MatchesStructuralType(structuralType, "plane")
        || MatchesStructuralType(structuralType, "water_surface_primitive")) {
        return ri::math::Vec3{0.42f, 0.72f, 1.0f};
    }
    if (MatchesStructuralType(structuralType, "terrain_quad")
        || MatchesStructuralType(structuralType, "heightmap_patch")
        || MatchesStructuralType(structuralType, "displacement")) {
        return ri::math::Vec3{0.48f, 0.88f, 0.42f};
    }
    if (MatchesStructuralType(structuralType, "voronoi_fracture")
        || MatchesStructuralType(structuralType, "metaball_cluster")
        || MatchesStructuralType(structuralType, "lsystem_branch")) {
        return ri::math::Vec3{0.92f, 0.55f, 0.95f};
    }
    if (MatchesStructuralType(structuralType, "lattice_volume")) {
        return ri::math::Vec3{0.95f, 0.82f, 0.28f};
    }
    return ri::math::Vec3{0.35f, 0.92f, 0.88f};
}

std::size_t StructuralPresetCount() {
    return kStructuralPrimitivePresets.size();
}

const ri::scene::StructuralPrimitivePreset& StructuralPresetAt(const std::size_t index) {
    return kStructuralPrimitivePresets[index % kStructuralPrimitivePresets.size()];
}

std::string StructuralPresetDisplayLabel(const std::size_t index) {
    return std::string(StructuralPresetAt(index).label);
}

std::size_t ActiveCatalogPresetCount(const AuthoringCatalogSection section) {
    if (section == AuthoringCatalogSection::Structural) {
        return kStructuralPrimitivePresets.size();
    }
    return AuthoringCatalogPresetCount(section);
}

std::string ActiveCatalogPresetLabel(const AuthoringCatalogSection section, const std::size_t index) {
    if (section == AuthoringCatalogSection::Structural) {
        return StructuralPresetDisplayLabel(index);
    }
    return AuthoringCatalogPresetLabel(section, index);
}

ri::math::Vec3 ActiveCatalogWireColor(const AuthoringCatalogSection section, const std::size_t index) {
    if (section == AuthoringCatalogSection::Structural) {
        const ri::scene::StructuralPrimitivePreset& preset = StructuralPresetAt(index);
        if (IsGuideStructuralPreset(preset.structuralType)) {
            return GuideStructuralWireColor(preset.structuralType);
        }
        return ri::math::Vec3{0.64f, 0.68f, 0.74f};
    }
    const AuthoringCatalogPreset& preset = AuthoringCatalogPresetAt(section, index);
    return AuthoringCatalogWireColor(section, preset.typeId);
}

bool ActiveCatalogUsesWireframe(const AuthoringCatalogSection section, const std::size_t index) {
    if (section == AuthoringCatalogSection::Structural) {
        return IsGuideStructuralPreset(StructuralPresetAt(index).structuralType);
    }
    (void)index;
    return true;
}

bool StructuralThumbnailCache::Has(const AuthoringCatalogSection section, const std::size_t presetIndex) const {
    switch (section) {
        case AuthoringCatalogSection::Volumes:
            return presetIndex < volumeReady_.size() && volumeReady_[presetIndex];
        case AuthoringCatalogSection::Logic:
            return presetIndex < logicReady_.size() && logicReady_[presetIndex];
        case AuthoringCatalogSection::Structural:
            return presetIndex < structuralReady_.size() && structuralReady_[presetIndex];
    }
    return false;
}

const ri::render::software::SoftwareImage& StructuralThumbnailCache::Get(const AuthoringCatalogSection section,
                                                                        const std::size_t presetIndex) {
    static const ri::render::software::SoftwareImage kEmpty{};
    switch (section) {
        case AuthoringCatalogSection::Volumes:
            if (presetIndex >= volumeImages_.size() || !volumeReady_[presetIndex]) {
                return kEmpty;
            }
            return volumeImages_[presetIndex];
        case AuthoringCatalogSection::Logic:
            if (presetIndex >= logicImages_.size() || !logicReady_[presetIndex]) {
                return kEmpty;
            }
            return logicImages_[presetIndex];
        case AuthoringCatalogSection::Structural:
            if (presetIndex >= structuralImages_.size() || !structuralReady_[presetIndex]) {
                return kEmpty;
            }
            return structuralImages_[presetIndex];
    }
    return kEmpty;
}

void StructuralThumbnailCache::Clear() {
    structuralImages_.clear();
    structuralReady_.clear();
    volumeImages_.clear();
    volumeReady_.clear();
    logicImages_.clear();
    logicReady_.clear();
}

void StructuralThumbnailCache::Ensure(const AuthoringCatalogSection section,
                                      const std::size_t presetIndex,
                                      const std::filesystem::path& textureRoot) {
    const std::size_t count = ActiveCatalogPresetCount(section);
    std::vector<ri::render::software::SoftwareImage>* images = &structuralImages_;
    std::vector<bool>* ready = &structuralReady_;
    if (section == AuthoringCatalogSection::Volumes) {
        images = &volumeImages_;
        ready = &volumeReady_;
    } else if (section == AuthoringCatalogSection::Logic) {
        images = &logicImages_;
        ready = &logicReady_;
    }

    if (images->size() != count) {
        images->assign(count, ri::render::software::SoftwareImage{});
        ready->assign(count, false);
    }
    if (presetIndex >= count || (*ready)[presetIndex]) {
        return;
    }

    if (section == AuthoringCatalogSection::Structural) {
        const ri::scene::StructuralPrimitivePreset& preset = kStructuralPrimitivePresets[presetIndex];
        if (IsGuideStructuralPreset(preset.structuralType)) {
            (*images)[presetIndex] =
                RenderGuideWireframeThumbnail(preset, kStructuralPickerCellSize, kStructuralPickerCellSize);
            (*ready)[presetIndex] = !(*images)[presetIndex].pixels.empty();
            return;
        }

        StarterScene thumbScene = BuildPresetThumbnailScene(preset);
        ri::render::software::ScenePreviewOptions options{};
        options.width = kStructuralPickerCellSize;
        options.height = kStructuralPickerCellSize;
        options.clearTop = ri::math::Vec3{0.06f, 0.07f, 0.10f};
        options.clearBottom = ri::math::Vec3{0.12f, 0.13f, 0.17f};
        options.ambientLight = ri::math::Vec3{0.16f, 0.17f, 0.20f};
        options.orderedDither = false;
        options.renderer = ri::render::software::ScenePreviewRenderer::Raster;
        if (!textureRoot.empty()) {
            options.textureRoot = textureRoot;
        }

        (*images)[presetIndex] =
            ri::render::software::RenderScenePreview(thumbScene.scene,
                                                     thumbScene.handles.orbitCamera.cameraNode,
                                                     options);
        (*ready)[presetIndex] = !(*images)[presetIndex].pixels.empty();
        return;
    }

    const AuthoringCatalogPreset& preset = AuthoringCatalogPresetAt(section, presetIndex);
    const ri::math::Vec3 wireColor = AuthoringCatalogWireColor(section, preset.typeId);
    const ri::math::Vec3 thumbScale =
        section == AuthoringCatalogSection::Logic ? ri::math::Vec3{0.35f, 0.35f, 0.35f}
                                                  : ri::math::Vec3{1.0f, 1.0f, 1.0f};
    (*images)[presetIndex] =
        RenderBoxWireframeThumbnail(wireColor, thumbScale, kStructuralPickerCellSize, kStructuralPickerCellSize);
    (*ready)[presetIndex] = !(*images)[presetIndex].pixels.empty();
}

void StructuralThumbnailCache::Prewarm(const AuthoringCatalogSection section,
                                       const std::size_t presetIndex,
                                       const std::filesystem::path& textureRoot) {
    Ensure(section, presetIndex, textureRoot);
}

void StructuralThumbnailCache::PrewarmVisible(const AuthoringCatalogSection section,
                                              const std::vector<std::size_t>& presetIndices,
                                              const std::filesystem::path& textureRoot,
                                              const int budget) {
    int remaining = std::max(1, budget);
    for (const std::size_t index : presetIndices) {
        if (remaining <= 0) {
            break;
        }
        if (!Has(section, index)) {
            Ensure(section, index, textureRoot);
            --remaining;
        }
    }
}

#if defined(_WIN32)

StructuralPickerLayout ComputeStructuralPickerLayout(const RECT& viewportInner,
                                                     const AuthoringCatalogSection section,
                                                     const int scrollTopRow) {
    StructuralPickerLayout layout{};
    const int panelHeight = ComputeStructuralPickerPanelHeight();
    layout.panelRect = RECT{
        viewportInner.left + 6,
        viewportInner.bottom - panelHeight - 4,
        viewportInner.right - 6,
        viewportInner.bottom - 4,
    };

    const int tabTop = layout.panelRect.top + kStructuralPickerHeaderHeight + 2;
    layout.meshTabBtn = RECT{layout.panelRect.left + 8, tabTop, layout.panelRect.left + 58, tabTop + kStructuralPickerTabHeight};
    layout.volumeTabBtn = RECT{layout.panelRect.left + 62, tabTop, layout.panelRect.left + 118, tabTop + kStructuralPickerTabHeight};
    layout.logicTabBtn = RECT{layout.panelRect.left + 122, tabTop, layout.panelRect.left + 170, tabTop + kStructuralPickerTabHeight};

    layout.contentRect = RECT{
        layout.panelRect.left + 8,
        tabTop + kStructuralPickerTabHeight + 2,
        layout.panelRect.right - 8,
        tabTop + kStructuralPickerTabHeight + 2 + StructuralPickerGridHeight(),
    };

    const std::size_t presetCount = ActiveCatalogPresetCount(section);
    const int contentWidth = std::max(1, static_cast<int>(layout.contentRect.right - layout.contentRect.left));
    layout.columns = std::max(4, contentWidth / (kStructuralPickerCellSize + 6));
    const int rowStride = kStructuralPickerCellSize + kStructuralPickerLabelHeight + 4;
    layout.visibleRows = kStructuralPickerVisibleRows;
    layout.totalRows = static_cast<int>((presetCount + static_cast<std::size_t>(layout.columns) - 1)
                                        / static_cast<std::size_t>(layout.columns));
    layout.scrollTopRow = std::clamp(scrollTopRow, 0, std::max(0, layout.totalRows - layout.visibleRows));

    const int footerTop = layout.panelRect.bottom - kStructuralPickerFooterHeight - 4;
    layout.prevPageBtn = RECT{layout.panelRect.left + 8, footerTop, layout.panelRect.left + 50, footerTop + 18};
    layout.nextPageBtn = RECT{layout.panelRect.left + 54, footerTop, layout.panelRect.left + 96, footerTop + 18};
    layout.placeBtn = RECT{layout.panelRect.right - 108, footerTop, layout.panelRect.right - 8, footerTop + 18};
    layout.collapseToggleBtn =
        RECT{layout.panelRect.right - 58, layout.panelRect.top + 2, layout.panelRect.right - 8, layout.panelRect.top + 18};

    int x = layout.contentRect.left;
    int y = layout.contentRect.top;
    int column = 0;
    const std::size_t firstIndex = static_cast<std::size_t>(layout.scrollTopRow * layout.columns);
    for (std::size_t offset = 0; offset < presetCount; ++offset) {
        const std::size_t presetIndex = firstIndex + offset;
        if (presetIndex >= presetCount) {
            break;
        }
        const int cellTop = y;
        const int cellBottom = cellTop + kStructuralPickerCellSize;
        if (cellBottom > layout.contentRect.bottom) {
            break;
        }
        StructuralPickerCell cell{};
        cell.presetIndex = presetIndex;
        cell.thumbRect = RECT{x, cellTop, x + kStructuralPickerCellSize, cellBottom};
        cell.labelRect =
            RECT{x, cellBottom + 2, x + kStructuralPickerCellSize, cellBottom + 2 + kStructuralPickerLabelHeight};
        layout.cells.push_back(cell);

        ++column;
        if (column >= layout.columns) {
            column = 0;
            x = layout.contentRect.left;
            y += rowStride;
        } else {
            x += kStructuralPickerCellSize + 6;
        }
    }
    return layout;
}

RECT ComputeStructuralPickerCollapsedBarRect(const RECT& viewportInner) {
    return RECT{
        viewportInner.left + 6,
        viewportInner.bottom - kStructuralPickerCollapsedBarHeight - 4,
        viewportInner.right - 6,
        viewportInner.bottom - 4,
    };
}

StructuralPickerHit HitTestStructuralPickerCollapsedBar(const RECT& barRect, const POINT& point) {
    const RECT expandBtn{barRect.right - 108, barRect.top + 4, barRect.right - 8, barRect.bottom - 4};
    if (PtInRect(&expandBtn, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::ToggleExpand};
    }
    if (PtInRect(&barRect, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::ToggleExpand};
    }
    return {};
}

void RenderStructuralPickerCollapsedBar(HDC dc,
                                        const RECT& barRect,
                                        const StructuralPickerTheme& theme,
                                        const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    EditorRenderer::DrawInsetFrame(dc, barRect, RGB(48, 54, 64), RGB(196, 168, 96), RGB(14, 16, 20));
    EditorRenderer::DrawTextLine(
        dc,
        RECT{barRect.left + 10, barRect.top + 4, barRect.right - 120, barRect.bottom - 4},
        "Authoring catalog hidden  |  Ctrl+2 or Expand to show meshes / volumes / logic",
        RGB(214, 210, 196),
        theme.smallFont,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    const RECT expandBtn{barRect.right - 108, barRect.top + 4, barRect.right - 8, barRect.bottom - 4};
    drawToolbarButton(dc, expandBtn, "Expand Catalog", false);
}

int CountStructuralPickerRows(const AuthoringCatalogSection section, const RECT& contentRect, const int columns) {
    const int safeColumns = std::max(1, columns);
    (void)contentRect;
    const std::size_t presetCount = ActiveCatalogPresetCount(section);
    return static_cast<int>((presetCount + static_cast<std::size_t>(safeColumns) - 1)
                            / static_cast<std::size_t>(safeColumns));
}

StructuralPickerHit HitTestStructuralPicker(const StructuralPickerLayout& layout, const POINT& point) {
    for (const StructuralPickerCell& cell : layout.cells) {
        RECT hitRect = cell.thumbRect;
        hitRect.bottom = cell.labelRect.bottom;
        if (PtInRect(&hitRect, point) != FALSE) {
            return {.kind = StructuralPickerHitKind::Preset, .presetIndex = cell.presetIndex};
        }
    }
    if (PtInRect(&layout.meshTabBtn, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::SectionStructural};
    }
    if (PtInRect(&layout.volumeTabBtn, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::SectionVolumes};
    }
    if (PtInRect(&layout.logicTabBtn, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::SectionLogic};
    }
    if (PtInRect(&layout.prevPageBtn, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::PrevPage};
    }
    if (PtInRect(&layout.nextPageBtn, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::NextPage};
    }
    if (PtInRect(&layout.placeBtn, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::Place};
    }
    if (PtInRect(&layout.collapseToggleBtn, point) != FALSE) {
        return {.kind = StructuralPickerHitKind::ToggleExpand};
    }
    return {};
}

void RenderStructuralPickerOverlay(HDC dc,
                                   const StructuralPickerLayout& layout,
                                   const StructuralPickerModel& model,
                                   StructuralThumbnailCache& thumbnails,
                                   const std::filesystem::path& textureRoot,
                                   const StructuralPickerTheme& theme,
                                   const std::function<void(HDC, const RECT&, const std::string&, bool)>& drawToolbarButton) {
    EditorRenderer::DrawInsetFrame(
        dc, layout.panelRect, RGB(48, 54, 64), RGB(196, 168, 96), RGB(14, 16, 20));
    EditorRenderer::FillRectColor(
        dc,
        RECT{layout.panelRect.left + 1, layout.panelRect.top + 1, layout.panelRect.right - 1, layout.panelRect.top + 5},
        RGB(214, 156, 72));

    EditorRenderer::DrawTextLine(
        dc,
        RECT{layout.panelRect.left + 10, layout.panelRect.top + 3, layout.panelRect.right - 160, layout.panelRect.top + 18},
        "Authoring Catalog  |  click preset  |  Place to spawn  |  wheel to page",
        RGB(248, 244, 228),
        theme.headerFont,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    drawToolbarButton(dc, layout.collapseToggleBtn, "Hide", false);

    drawToolbarButton(dc,
                      layout.meshTabBtn,
                      std::string(AuthoringCatalogSectionLabel(AuthoringCatalogSection::Structural)),
                      model.section == AuthoringCatalogSection::Structural);
    drawToolbarButton(dc,
                      layout.volumeTabBtn,
                      std::string(AuthoringCatalogSectionLabel(AuthoringCatalogSection::Volumes)),
                      model.section == AuthoringCatalogSection::Volumes);
    drawToolbarButton(dc,
                      layout.logicTabBtn,
                      std::string(AuthoringCatalogSectionLabel(AuthoringCatalogSection::Logic)),
                      model.section == AuthoringCatalogSection::Logic);

    std::vector<std::size_t> visibleIndices;
    visibleIndices.reserve(layout.cells.size());
    for (const StructuralPickerCell& cell : layout.cells) {
        visibleIndices.push_back(cell.presetIndex);
    }
    thumbnails.PrewarmVisible(model.section, visibleIndices, textureRoot, 2);

    for (const StructuralPickerCell& cell : layout.cells) {
        const bool selected = cell.presetIndex == model.selectedPresetIndex;
        const bool hovered = cell.presetIndex == model.hoveredPresetIndex;
        const bool wirePreset = ActiveCatalogUsesWireframe(model.section, cell.presetIndex);
        const ri::math::Vec3 wireColor = ActiveCatalogWireColor(model.section, cell.presetIndex);
        COLORREF frameFill = selected ? RGB(92, 72, 36) : (hovered ? RGB(58, 66, 78) : RGB(34, 38, 46));
        COLORREF frameEdge = selected ? RGB(255, 214, 120) : RGB(112, 118, 128);
        if (wirePreset) {
            const auto channel = [](const float value) -> int {
                return static_cast<int>(std::clamp(value * 255.0f, 0.0f, 255.0f));
            };
            frameFill = selected ? RGB(channel(wireColor.x * 0.55f),
                                       channel(wireColor.y * 0.55f),
                                       channel(wireColor.z * 0.55f))
                                 : RGB(28, 32, 38);
            frameEdge = RGB(channel(wireColor.x), channel(wireColor.y), channel(wireColor.z));
        }
        EditorRenderer::DrawInsetFrame(dc, cell.thumbRect, frameFill, frameEdge, RGB(12, 14, 18));

        const ri::render::software::SoftwareImage& image = thumbnails.Get(model.section, cell.presetIndex);
        if (image.pixels.empty()) {
            DrawPlaceholderThumb(dc, cell.thumbRect, "...", theme.smallFont);
        } else {
            const RECT inner{
                cell.thumbRect.left + 2,
                cell.thumbRect.top + 2,
                cell.thumbRect.right - 2,
                cell.thumbRect.bottom - 2,
            };
            EditorRenderer::BlitSoftwareImage(dc, inner, image);
        }

        EditorRenderer::DrawTextLine(dc,
                                     cell.labelRect,
                                     ActiveCatalogPresetLabel(model.section, cell.presetIndex),
                                     selected ? RGB(255, 236, 170) : RGB(196, 202, 210),
                                     theme.smallFont,
                                     DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    drawToolbarButton(dc, layout.prevPageBtn, "Prev", false);
    drawToolbarButton(dc, layout.nextPageBtn, "Next", false);
    const std::string placeLabel =
        std::string("Place ") + ActiveCatalogPresetLabel(model.section, model.selectedPresetIndex);
    drawToolbarButton(dc, layout.placeBtn, placeLabel, true);

    const std::string footer =
        model.statusLine.empty()
            ? ("Showing row " + std::to_string(layout.scrollTopRow + 1) + "/" + std::to_string(layout.totalRows)
               + "  |  " + std::to_string(ActiveCatalogPresetCount(model.section)) + " "
               + std::string(AuthoringCatalogSectionLabel(model.section)))
            : model.statusLine;
    EditorRenderer::DrawTextLine(
        dc,
        RECT{layout.panelRect.left + 120, layout.placeBtn.top, layout.placeBtn.left - 8, layout.placeBtn.bottom},
        footer,
        RGB(176, 182, 192),
        theme.smallFont,
        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

#endif

} // namespace ri::editor
