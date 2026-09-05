#include "RawIron/Scene/StructuralPrimitiveBundle.h"

#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"

#include <cctype>
#include <string_view>
#include <vector>

namespace ri::scene {
namespace {

std::string SanitizeLabel(const std::string_view label) {
    std::string clean;
    clean.reserve(label.size());
    for (const char character : label) {
        const unsigned char value = static_cast<unsigned char>(character);
        clean.push_back(std::isalnum(value) != 0 ? static_cast<char>(std::tolower(value)) : '_');
    }
    return clean.empty() ? "unnamed" : clean;
}

RuntimeMaterialParams MakeGalleryMaterial(const std::string_view label,
                                          const std::string_view baseTexture,
                                          const ri::math::Vec3& baseColor,
                                          const MaterialStyle style,
                                          const float metallic,
                                          const float roughness,
                                          const ri::math::Vec2& tiling) {
    RuntimeMaterialParams material{};
    material.materialName = std::string("struct_gallery_") + SanitizeLabel(label);
    material.shadingModel = ShadingModel::Lit;
    material.materialStyle = style;
    material.materialWorkflow = MaterialWorkflow::MetalRough;
    material.baseColor = baseColor;
    material.baseColorTexture = std::string(baseTexture);
    material.textureTiling = tiling;
    material.metallic = metallic;
    material.roughness = roughness;
    return material;
}

ri::math::Vec3 DisplayScaleForPreset(const std::string_view label, const ri::math::Vec3& baseScale) {
    const std::string clean = SanitizeLabel(label);
    if (clean.find("plane") != std::string::npos || clean.find("water") != std::string::npos ||
        clean.find("terrain") != std::string::npos || clean.find("heightmap") != std::string::npos ||
        clean.find("displacement") != std::string::npos) {
        return ri::math::Vec3{baseScale.x * 1.25f, baseScale.y * 0.35f, baseScale.z * 1.25f};
    }
    if (clean.find("wall") != std::string::npos || clean.find("frame") != std::string::npos ||
        clean.find("buttress") != std::string::npos || clean.find("parapet") != std::string::npos ||
        clean.find("pilaster") != std::string::npos || clean.find("lintel") != std::string::npos) {
        return ri::math::Vec3{baseScale.x * 1.10f, baseScale.y * 1.35f, baseScale.z * 0.85f};
    }
    if (clean.find("corridor") != std::string::npos || clean.find("vault") != std::string::npos ||
        clean.find("colonnade") != std::string::npos) {
        return ri::math::Vec3{baseScale.x * 1.40f, baseScale.y * 0.90f, baseScale.z * 1.40f};
    }
    if (clean.find("stairs") != std::string::npos || clean.find("ramp") != std::string::npos ||
        clean.find("catwalk") != std::string::npos) {
        return ri::math::Vec3{baseScale.x * 1.35f, baseScale.y * 0.95f, baseScale.z * 1.15f};
    }
    if (clean.find("column") != std::string::npos || clean.find("capsule_tall") != std::string::npos ||
        clean.find("lsystem") != std::string::npos) {
        return ri::math::Vec3{baseScale.x * 0.85f, baseScale.y * 1.55f, baseScale.z * 0.85f};
    }
    if (clean.find("lattice") != std::string::npos || clean.find("cable") != std::string::npos ||
        clean.find("catenary") != std::string::npos || clean.find("spline") != std::string::npos) {
        return ri::math::Vec3{baseScale.x * 1.20f, baseScale.y * 1.10f, baseScale.z * 1.20f};
    }
    return baseScale;
}

void ApplyNonDefaultOverrides(ri::structural::StructuralPrimitiveOptions& target,
                              const ri::structural::StructuralPrimitiveOptions& overrides) {
    const ri::structural::StructuralPrimitiveOptions defaults{};
    if (overrides.radialSegments != defaults.radialSegments) target.radialSegments = overrides.radialSegments;
    if (overrides.sides != defaults.sides) target.sides = overrides.sides;
    if (overrides.detail != defaults.detail) target.detail = overrides.detail;
    if (overrides.steps != defaults.steps) target.steps = overrides.steps;
    if (overrides.cellsX != defaults.cellsX) target.cellsX = overrides.cellsX;
    if (overrides.cellsY != defaults.cellsY) target.cellsY = overrides.cellsY;
    if (overrides.cellsZ != defaults.cellsZ) target.cellsZ = overrides.cellsZ;
    if (overrides.hemisphereSegments != defaults.hemisphereSegments) target.hemisphereSegments = overrides.hemisphereSegments;
    if (overrides.thickness != defaults.thickness) target.thickness = overrides.thickness;
    if (overrides.depth != defaults.depth) target.depth = overrides.depth;
    if (overrides.strutRadius != defaults.strutRadius) target.strutRadius = overrides.strutRadius;
    if (overrides.topRadius != defaults.topRadius) target.topRadius = overrides.topRadius;
    if (overrides.bottomRadius != defaults.bottomRadius) target.bottomRadius = overrides.bottomRadius;
    if (overrides.length != defaults.length) target.length = overrides.length;
    if (overrides.exponentX != defaults.exponentX) target.exponentX = overrides.exponentX;
    if (overrides.exponentY != defaults.exponentY) target.exponentY = overrides.exponentY;
    if (overrides.exponentZ != defaults.exponentZ) target.exponentZ = overrides.exponentZ;
    if (overrides.spanDegrees != defaults.spanDegrees) target.spanDegrees = overrides.spanDegrees;
    if (overrides.sweepDegrees != defaults.sweepDegrees) target.sweepDegrees = overrides.sweepDegrees;
    if (overrides.startDegrees != defaults.startDegrees) target.startDegrees = overrides.startDegrees;
    if (overrides.ridgeRatio != defaults.ridgeRatio) target.ridgeRatio = overrides.ridgeRatio;
    if (overrides.bevelRadius != defaults.bevelRadius) target.bevelRadius = overrides.bevelRadius;
    if (overrides.bevelSegments != defaults.bevelSegments) target.bevelSegments = overrides.bevelSegments;
    if (overrides.centerColumn != defaults.centerColumn) target.centerColumn = overrides.centerColumn;
    if (overrides.closedProfile != defaults.closedProfile) target.closedProfile = overrides.closedProfile;
    if (overrides.closedPath != defaults.closedPath) target.closedPath = overrides.closedPath;
    if (overrides.capEnds != defaults.capEnds) target.capEnds = overrides.capEnds;
    if (overrides.pathSegments != defaults.pathSegments) target.pathSegments = overrides.pathSegments;
    if (overrides.archStyle != defaults.archStyle) target.archStyle = overrides.archStyle;
    if (overrides.latticeStyle != defaults.latticeStyle) target.latticeStyle = overrides.latticeStyle;
    if (!overrides.points.empty()) target.points = overrides.points;
    if (!overrides.vertices.empty()) target.vertices = overrides.vertices;
    if (!overrides.heightfieldSamples.empty()) target.heightfieldSamples = overrides.heightfieldSamples;
}

} // namespace

StructuralPrimitiveBundleResult SpawnStructuralPrimitiveBundle(Scene& scene,
                                                               const StructuralPrimitiveBundleParams& params) {
    StructuralPrimitiveBundleResult result{};

    ri::structural::StructuralPrimitiveOptions resolvedShape = params.shape;
    std::string structuralType{};
    if (params.presetField.has_value()) {
        if (const std::optional<StructuralPrimitivePreset> preset = FindStructuralPreset(*params.presetField)) {
            structuralType = std::string(preset->structuralType);
            resolvedShape = ShapeFromStructuralPreset(*preset);
            ApplyNonDefaultOverrides(resolvedShape, params.shape);
        } else {
            structuralType = *params.presetField;
            resolvedShape = params.shape;
        }
    } else {
        structuralType = ResolveStructuralPrimitiveTypeToken(
            params.primitiveTypeField ? std::optional<std::string_view>(*params.primitiveTypeField) : std::nullopt,
            params.typeAliasField ? std::optional<std::string_view>(*params.typeAliasField) : std::nullopt,
            "box");
        resolvedShape = params.shape;
    }

    StructuralBrushSpawnOptions brush{};
    brush.nodeName = params.nodeName;
    brush.structuralType = structuralType;
    brush.shape = resolvedShape;
    brush.parent = params.parent;
    brush.transform = params.transform;
    brush.materialName = params.material.materialName;
    brush.shadingModel = params.material.shadingModel;
    brush.materialStyle = params.material.materialStyle;
    brush.materialWorkflow = params.material.materialWorkflow;
    brush.baseColor = params.material.baseColor;
    brush.baseColorTexture = params.material.baseColorTexture;
    brush.normalTexture = params.material.normalTexture;
    brush.ormTexture = params.material.ormTexture;
    brush.roughnessTexture = params.material.roughnessTexture;
    brush.metallicTexture = params.material.metallicTexture;
    brush.emissiveTexture = params.material.emissiveTexture;
    brush.opacityTexture = params.material.opacityTexture;
    brush.occlusionTexture = params.material.occlusionTexture;
    brush.detailTexture = params.material.detailTexture;
    brush.textureTiling = params.material.textureTiling;
    brush.emissiveColor = params.material.emissiveColor;
    brush.metallic = params.material.metallic;
    brush.roughness = params.material.roughness;
    brush.opacity = params.material.opacity;
    brush.alphaCutoff = params.material.alphaCutoff;
    brush.doubleSided = params.material.doubleSided || structuralType == "mobius";
    brush.transparent = params.material.transparent;
    brush.additiveBlend = params.material.additiveBlend;

    result.node = AddStructuralBrushNode(scene, brush);
    if (result.node == kInvalidHandle) {
        return result;
    }
    const Node& node = scene.GetNode(result.node);
    result.mesh = node.mesh;
    result.material = node.material;
    return result;
}

StructuralPrimitiveAssemblyResult SpawnStructuralPrimitiveAssembly(Scene& scene,
                                                                   const StructuralPrimitiveAssemblyParams& params) {
    StructuralPrimitiveAssemblyOptions assembly{};
    assembly.rootNodeName = params.rootNodeName;
    assembly.parent = params.parent;
    assembly.transform.position = params.transform.position;
    assembly.transform.rotationDegrees = params.transform.rotationDegrees;
    assembly.transform.scale = params.transform.scale;
    assembly.nodes = params.nodes;
    assembly.compileOptions = params.compileOptions;

    StructuralBrushSpawnOptions& brush = assembly.material;
    brush.materialName = params.material.materialName;
    brush.shadingModel = params.material.shadingModel;
    brush.materialStyle = params.material.materialStyle;
    brush.materialWorkflow = params.material.materialWorkflow;
    brush.baseColor = params.material.baseColor;
    brush.baseColorTexture = params.material.baseColorTexture;
    brush.normalTexture = params.material.normalTexture;
    brush.ormTexture = params.material.ormTexture;
    brush.roughnessTexture = params.material.roughnessTexture;
    brush.metallicTexture = params.material.metallicTexture;
    brush.emissiveTexture = params.material.emissiveTexture;
    brush.opacityTexture = params.material.opacityTexture;
    brush.occlusionTexture = params.material.occlusionTexture;
    brush.detailTexture = params.material.detailTexture;
    brush.textureTiling = params.material.textureTiling;
    brush.emissiveColor = params.material.emissiveColor;
    brush.metallic = params.material.metallic;
    brush.roughness = params.material.roughness;
    brush.opacity = params.material.opacity;
    brush.alphaCutoff = params.material.alphaCutoff;
    brush.doubleSided = params.material.doubleSided;
    brush.transparent = params.material.transparent;
    brush.additiveBlend = params.material.additiveBlend;

    return AddStructuralPrimitiveAssembly(scene, assembly);
}

std::vector<ri::structural::StructuralNode> BuildSandboxStructuralHallNodes() {
    std::vector<ri::structural::StructuralNode> nodes;
    nodes.reserve(8U);

    ri::structural::StructuralNode shell = MakeStructuralPrimitiveSolid(
        "sandbox_hall_shell",
        "hollow_box",
        ri::math::Vec3{0.0f, 3.0f, 0.0f},
        ri::math::Vec3{14.0f, 5.5f, 10.0f});
    shell.name = "SandboxHallShell";
    shell.thickness = 0.18f;
    nodes.push_back(std::move(shell));

    nodes.push_back(MakeStructuralPrimitiveSubtract(
        "sandbox_hall_door_cut",
        "box",
        {"sandbox_hall_shell"},
        ri::math::Vec3{0.0f, 1.6f, 5.35f},
        ri::math::Vec3{2.4f, 3.2f, 0.7f}));

    nodes.push_back(MakeStructuralPrimitiveSubtract(
        "sandbox_hall_window_cut",
        "box",
        {"sandbox_hall_shell"},
        ri::math::Vec3{-4.8f, 3.6f, 5.35f},
        ri::math::Vec3{2.2f, 1.6f, 0.6f}));

    nodes.push_back(MakeStructuralPrimitiveSubtract(
        "sandbox_hall_sky_cut",
        "box",
        {"sandbox_hall_shell"},
        ri::math::Vec3{0.0f, 9.5f, 0.0f},
        ri::math::Vec3{18.0f, 8.0f, 14.0f}));

    ri::structural::StructuralNode perforated = MakeStructuralPrimitiveGraphNode(
        "sandbox_hall_perforated",
        "perforated_wall",
        ri::math::Vec3{3.8f, 2.4f, 0.0f},
        ri::math::Vec3{0.35f, 4.6f, 6.2f});
    perforated.name = "SandboxHallPerforatedPartition";
    perforated.cellsX = 6;
    perforated.cellsY = 4;
    perforated.thickness = 0.1f;
    // The perforated divider is visual detail. Its coarse AABB must not become an invisible
    // blocking wall in movement/ballistics traces; the structural feed will retain it for render
    // ownership while filtering it from the blocking trace channel.
    perforated.detailOnly = true;
    nodes.push_back(std::move(perforated));

    nodes.push_back(MakeStructuralPrimitiveGraphNode(
        "sandbox_hall_ramp",
        "ramp",
        ri::math::Vec3{-6.2f, 0.35f, -1.5f},
        ri::math::Vec3{4.2f, 0.75f, 5.8f}));

    nodes.push_back(MakeStructuralPrimitiveGraphNode(
        "sandbox_hall_arch",
        "arch",
        ri::math::Vec3{0.0f, 0.55f, -5.6f},
        ri::math::Vec3{5.5f, 4.8f, 1.2f},
        ri::structural::StructuralPrimitiveOptions{.thickness = 0.18f, .spanDegrees = 180.0f, .archStyle = "round"}));

    return nodes;
}

StructuralPrimitiveAssemblyResult SpawnSandboxStructuralHall(Scene& scene,
                                                               const int parent,
                                                               const ri::math::Vec3 worldOrigin,
                                                               const std::string_view rootName) {
    StructuralPrimitiveAssemblyParams params{};
    params.parent = parent;
    params.transform.position = worldOrigin;
    params.rootNodeName = std::string(rootName);
    params.nodes = BuildSandboxStructuralHallNodes();
    params.material.baseColor = ri::math::Vec3{0.78f, 0.80f, 0.84f};
    params.material.baseColorTexture = "smooth_stone.png";
    params.material.materialStyle = MaterialStyle::Layered;
    params.material.detailTexture = "concrete_silver_olo32.png";
    params.material.textureTiling = ri::math::Vec2{2.5f, 2.5f};
    params.material.roughness = 0.90f;
    params.material.materialName = std::string(rootName) + "_mat";
    return SpawnStructuralPrimitiveAssembly(scene, params);
}

std::vector<StructuralGalleryMaterialPreset> BuildDefaultStructuralGalleryMaterials() {
    std::vector<StructuralGalleryMaterialPreset> rows;
    rows.reserve(7U);

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "concrete_standard",
        .material = MakeGalleryMaterial("concrete_standard",
                                        "smooth_stone.png",
                                        ri::math::Vec3{0.86f, 0.84f, 0.78f},
                                        MaterialStyle::Standard,
                                        0.0f,
                                        0.86f,
                                        ri::math::Vec2{2.0f, 2.0f}),
    });

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "raw_iron_metal",
        .material = MakeGalleryMaterial("raw_iron_metal",
                                        "raw_iron_block.png",
                                        ri::math::Vec3{0.95f, 0.72f, 0.58f},
                                        MaterialStyle::Layered,
                                        0.68f,
                                        0.42f,
                                        ri::math::Vec2{2.4f, 2.4f}),
    });
    rows.back().material.detailTexture = "iron_block.png";

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "oxidized_copper",
        .material = MakeGalleryMaterial("oxidized_copper",
                                        "oxidized_cut_copper.png",
                                        ri::math::Vec3{0.42f, 0.86f, 0.78f},
                                        MaterialStyle::MixedMedia,
                                        0.52f,
                                        0.36f,
                                        ri::math::Vec2{2.2f, 2.2f}),
    });
    rows.back().material.detailTexture = "weathered_cut_copper.png";

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "blackstone_retro",
        .material = MakeGalleryMaterial("blackstone_retro",
                                        "polished_blackstone_bricks.png",
                                        ri::math::Vec3{0.54f, 0.50f, 0.58f},
                                        MaterialStyle::Retro,
                                        0.1f,
                                        0.92f,
                                        ri::math::Vec2{2.0f, 2.0f}),
    });

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "amethyst_crystal",
        .material = MakeGalleryMaterial("amethyst_crystal",
                                        "amethyst_block.png",
                                        ri::math::Vec3{0.84f, 0.64f, 1.0f},
                                        MaterialStyle::Crystal,
                                        0.05f,
                                        0.24f,
                                        ri::math::Vec2{2.0f, 2.0f}),
    });
    rows.back().material.detailTexture = "amethyst_cluster.png";
    rows.back().material.transparent = true;
    rows.back().material.opacity = 0.82f;
    rows.back().material.doubleSided = true;

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "lantern_emissive",
        .material = MakeGalleryMaterial("lantern_emissive",
                                        "sea_lantern.png",
                                        ri::math::Vec3{0.92f, 1.0f, 0.96f},
                                        MaterialStyle::MixedMedia,
                                        0.0f,
                                        0.32f,
                                        ri::math::Vec2{2.0f, 2.0f}),
    });
    rows.back().material.emissiveTexture = "sea_lantern.png";
    rows.back().material.emissiveColor = ri::math::Vec3{0.35f, 0.75f, 0.95f};

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "sculk_mixed",
        .material = MakeGalleryMaterial("sculk_mixed",
                                        "sculk.png",
                                        ri::math::Vec3{0.32f, 0.76f, 0.82f},
                                        MaterialStyle::MixedMedia,
                                        0.0f,
                                        0.72f,
                                        ri::math::Vec2{2.1f, 2.1f}),
    });
    rows.back().material.detailTexture = "sculk_vein.png";
    rows.back().material.emissiveTexture = "sculk_sensor_top.png";
    rows.back().material.emissiveColor = ri::math::Vec3{0.08f, 0.55f, 0.72f};

    return rows;
}

StructuralPrimitiveGalleryResult SpawnStructuralPrimitiveGallery(Scene& scene,
                                                                 const StructuralPrimitiveGalleryOptions& options) {
    StructuralPrimitiveGalleryResult result{};
    result.rows = static_cast<int>(options.materialRows.empty() ? BuildDefaultStructuralGalleryMaterials().size()
                                                                : options.materialRows.size());
    result.columns = static_cast<int>(kStructuralPrimitivePresets.size());
    if (result.rows <= 0 || result.columns <= 0) {
        return result;
    }

    const std::vector<StructuralGalleryMaterialPreset> defaultRows =
        options.materialRows.empty() ? BuildDefaultStructuralGalleryMaterials() : std::vector<StructuralGalleryMaterialPreset>{};
    const std::vector<StructuralGalleryMaterialPreset>& materialRows =
        options.materialRows.empty() ? defaultRows : options.materialRows;

    result.root = scene.CreateNode(options.nodeNamePrefix, options.parent);
    if (result.root == kInvalidHandle) {
        return result;
    }
    scene.GetNode(result.root).localTransform = options.transform;

    const float spanX = static_cast<float>(result.columns - 1) * options.cellSpacing.x;
    const float spanZ = static_cast<float>(result.rows - 1) * options.cellSpacing.y;
    const float width = spanX + options.platformMargin.x * 2.0f + options.itemScale.x * 2.0f;
    const float depth = spanZ + options.platformMargin.z * 2.0f + options.itemScale.z * 2.0f;
    result.boundsCenter = options.transform.position;
    result.boundsExtents = ri::math::Vec3{width * 0.5f, options.itemScale.y * 1.2f, depth * 0.5f};

    if (options.includePlatform) {
        const float tileWorldSize = std::max(options.platformTileWorldSize, 0.25f);
        RuntimeMaterialParams platformMaterial =
            MakeGalleryMaterial("catalog_platform",
                                "smooth_stone.png",
                                ri::math::Vec3{0.72f, 0.72f, 0.68f},
                                MaterialStyle::Layered,
                                0.0f,
                                0.92f,
                                ri::math::Vec2{width / tileWorldSize, depth / tileWorldSize});
        platformMaterial.detailTexture = "concrete_silver_olo32.png";

        StructuralPrimitiveBundleParams platform{};
        platform.presetField = "box";
        platform.nodeName = options.nodeNamePrefix + "_platform";
        platform.parent = result.root;
        platform.transform.position = ri::math::Vec3{0.0f, -options.platformThickness * 0.5f, 0.0f};
        platform.transform.scale = ri::math::Vec3{width, options.platformThickness, depth};
        platform.material = platformMaterial;
        result.platform = SpawnStructuralPrimitiveBundle(scene, platform).node;
    }

    const float startX = -spanX * 0.5f;
    const float startZ = -spanZ * 0.5f;
    // Platform top sits at y=0 in gallery root space (center at -thickness/2, half-height thickness/2).
    // Seat catalog items on that surface with a tiny lift to avoid depth fighting with the ground face.
    constexpr float kPlatformTopY = 0.0f;
    constexpr float kPlatformContactEpsilon = 0.004f;
    for (std::size_t rowIndex = 0; rowIndex < materialRows.size(); ++rowIndex) {
        const StructuralGalleryMaterialPreset& row = materialRows[rowIndex];
        for (std::size_t columnIndex = 0; columnIndex < kStructuralPrimitivePresets.size(); ++columnIndex) {
            const StructuralPrimitivePreset& preset = kStructuralPrimitivePresets[columnIndex];
            const ri::math::Vec3 scale = DisplayScaleForPreset(preset.label, options.itemScale);

            StructuralPrimitiveBundleParams item{};
            item.presetField = preset.label;
            item.nodeName = options.nodeNamePrefix + "_" + SanitizeLabel(row.label) + "_" + SanitizeLabel(preset.label);
            item.parent = result.root;
            item.transform.position = ri::math::Vec3{
                startX + static_cast<float>(columnIndex) * options.cellSpacing.x,
                kPlatformTopY + (scale.y * 0.5f) + kPlatformContactEpsilon,
                startZ + static_cast<float>(rowIndex) * options.cellSpacing.y,
            };
            item.transform.scale = scale;
            item.material = row.material;
            item.material.materialName = item.nodeName + "_mat";

            const StructuralPrimitiveBundleResult spawned = SpawnStructuralPrimitiveBundle(scene, item);
            if (spawned.node == kInvalidHandle) {
                ++result.failed;
            } else {
                ++result.spawned;
            }
        }
    }

    return result;
}

} // namespace ri::scene
