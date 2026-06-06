#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxWorld.h"

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/SceneSubtreeColliders.h"
#include "RawIron/Scene/StructuralPrimitiveBundle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ri::games::multiplayersandbox {

namespace {

namespace fs = std::filesystem;

using namespace ri::scene;

constexpr const char* kLrtPackageRelativePath = "Assets\\Packages\\LRT - Texture Pack - RT28.8 - 128x";

std::string ToPackageTexturePath(const fs::path& packageRoot, const std::string_view relativePath) {
    return (packageRoot / fs::path(relativePath)).lexically_normal().string();
}

bool PackageTextureExists(const fs::path& packageRoot, const std::string_view relativePath) {
    if (relativePath.empty()) {
        return false;
    }
    return fs::exists((packageRoot / fs::path(relativePath)).lexically_normal());
}

bool ContainsToken(const std::string_view text, const std::string_view token) {
    return text.find(token) != std::string_view::npos;
}

bool IsCtmCopperLookupMaterial(const std::string_view albedoRelativePath,
                               const std::string_view specRelativePath) {
    return ContainsToken(albedoRelativePath, "ctm/RT_all_") && ContainsToken(albedoRelativePath, "copper") &&
           ContainsToken(specRelativePath, "_s.png");
}

RuntimeMaterialParams MakePackMaterial(const fs::path& packageRoot,
                                       const std::string_view label,
                                       const std::string_view albedoRelativePath,
                                       const std::string_view normalRelativePath,
                                       const std::string_view specRelativePath,
                                       const MaterialStyle style,
                                       const MaterialWorkflow workflow,
                                       const ri::math::Vec3& baseColor,
                                       const float metallic,
                                       const float roughness,
                                       const ri::math::Vec2& tiling) {
    RuntimeMaterialParams material{};
    material.materialName = std::string("sandbox_lrt_") + std::string(label);
    material.shadingModel = ShadingModel::Lit;
    material.materialStyle = style;
    material.materialWorkflow = workflow;
    material.baseColor = baseColor;

    const bool metalLookupMaterial = IsCtmCopperLookupMaterial(albedoRelativePath, specRelativePath) &&
                                     PackageTextureExists(packageRoot, "ctm/metalcolormap.png") &&
                                     PackageTextureExists(packageRoot, albedoRelativePath);
    if (metalLookupMaterial) {
        material.materialStyle = MaterialStyle::Standard;
        material.materialWorkflow = MaterialWorkflow::SpecGloss;
        material.baseColorTexture = ToPackageTexturePath(packageRoot, "ctm/metalcolormap.png");
        material.detailTexture = ToPackageTexturePath(packageRoot, albedoRelativePath);
        material.baseColor = ri::math::Vec3{1.0f, 1.0f, 1.0f};
        material.metallic = 0.24f;
        material.roughness = std::min(roughness, 0.30f);
    } else if (PackageTextureExists(packageRoot, albedoRelativePath)) {
        material.baseColorTexture = ToPackageTexturePath(packageRoot, albedoRelativePath);
        material.detailTexture = material.baseColorTexture;
    }

    if (normalRelativePath != albedoRelativePath && PackageTextureExists(packageRoot, normalRelativePath)) {
        material.normalTexture = ToPackageTexturePath(packageRoot, normalRelativePath);
    }
    if (specRelativePath != albedoRelativePath && PackageTextureExists(packageRoot, specRelativePath)) {
        material.ormTexture = ToPackageTexturePath(packageRoot, specRelativePath);
    }
    material.textureTiling = tiling;
    if (!metalLookupMaterial) {
        material.metallic = metallic;
        material.roughness = roughness;
    }
    return material;
}

std::vector<StructuralGalleryMaterialPreset> BuildLrtGalleryMaterials(const fs::path& packageRoot) {
    std::vector<StructuralGalleryMaterialPreset> rows;
    rows.reserve(7U);

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "lrt_smooth_stone",
        .material = MakePackMaterial(packageRoot,
                                     "smooth_stone",
                                     "tile/RT_smooth_stone.png",
                                     "tile/RT_smooth_stone_n.png",
                                     "tile/RT_smooth_stone_s.png",
                                     MaterialStyle::Standard,
                                     MaterialWorkflow::SpecGloss,
                                     ri::math::Vec3{0.96f, 0.95f, 0.93f},
                                     0.02f,
                                     0.80f,
                                     ri::math::Vec2{1.8f, 1.8f}),
    });

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "lrt_tuff_bricks",
        .material = MakePackMaterial(packageRoot,
                                     "tuff_bricks",
                                     "tile/RT_tuff_bricks.png",
                                     "tile/RT_tuff_bricks_n.png",
                                     "tile/RT_tuff_bricks_s.png",
                                     MaterialStyle::Layered,
                                     MaterialWorkflow::SpecGloss,
                                     ri::math::Vec3{0.90f, 0.89f, 0.86f},
                                     0.04f,
                                     0.72f,
                                     ri::math::Vec2{1.7f, 1.7f}),
    });

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "lrt_stainless_steel",
        .material = MakePackMaterial(packageRoot,
                                     "stainless_steel",
                                     "tile/RT_stainless_steel.png",
                                     "tile/RT_stainless_steel.png",
                                     "tile/RT_stainless_steel_s.png",
                                     MaterialStyle::MixedMedia,
                                     MaterialWorkflow::SpecGloss,
                                     ri::math::Vec3{0.92f, 0.94f, 0.97f},
                                     0.88f,
                                     0.20f,
                                     ri::math::Vec2{2.1f, 2.1f}),
    });

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "lrt_chiseled_copper",
        .material = MakePackMaterial(packageRoot,
                                     "chiseled_copper",
                                     "ctm/RT_all_chiseled_copper_1.png",
                                     "ctm/RT_all_chiseled_copper_1_n.png",
                                     "ctm/RT_all_chiseled_copper_1_s.png",
                                     MaterialStyle::Standard,
                                     MaterialWorkflow::SpecGloss,
                                     ri::math::Vec3{1.0f, 1.0f, 1.0f},
                                     1.0f,
                                     0.24f,
                                     ri::math::Vec2{1.0f, 1.0f}),
    });

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "lrt_copper_grate",
        .material = MakePackMaterial(packageRoot,
                                     "copper_grate",
                                     "ctm/RT_all_copper_grate_1.png",
                                     "ctm/RT_all_copper_grate_1_n.png",
                                     "ctm/RT_all_copper_grate_1_s.png",
                                     MaterialStyle::Standard,
                                     MaterialWorkflow::SpecGloss,
                                     ri::math::Vec3{1.0f, 1.0f, 1.0f},
                                     1.0f,
                                     0.28f,
                                     ri::math::Vec2{1.0f, 1.0f}),
    });

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "lrt_white_glaze",
        .material = MakePackMaterial(packageRoot,
                                     "white_glaze",
                                     "tile/RT_white_glazed_terracotta.png",
                                     "tile/RT_white_glazed_terracotta_n.png",
                                     "tile/RT_white_glazed_terracotta_s.png",
                                     MaterialStyle::Retro,
                                     MaterialWorkflow::SpecGloss,
                                     ri::math::Vec3{0.98f, 0.98f, 0.98f},
                                     0.06f,
                                     0.42f,
                                     ri::math::Vec2{1.6f, 1.6f}),
    });

    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "lrt_white_glass",
        .material = MakePackMaterial(packageRoot,
                                     "white_glass",
                                     "tile/RT_white_stained_glass.png",
                                     "tile/RT_white_stained_glass_n.png",
                                     "tile/RT_white_stained_glass_s.png",
                                     MaterialStyle::Crystal,
                                     MaterialWorkflow::SpecGloss,
                                     ri::math::Vec3{0.92f, 0.97f, 1.0f},
                                     0.04f,
                                     0.12f,
                                     ri::math::Vec2{1.9f, 1.9f}),
    });
    rows.back().material.transparent = true;
    rows.back().material.opacity = 0.56f;
    rows.back().material.doubleSided = true;

    // Realistic JAVA pack (rt2_*) additions. These ship authored albedo + normal (_n)
    // + spec/gloss (_s) triples, so the renderer binds real PBR maps with zero
    // procedural generation -- the authored content replaces generated assets outright.
    struct Rt2GalleryEntry {
        std::string_view label;
        std::string_view name;       // bare block name (no extension)
        MaterialStyle style;
        MaterialWorkflow workflow;
        ri::math::Vec3 baseColor;
        float metallic;
        float roughness;
        ri::math::Vec2 tiling;
    };
    const std::array<Rt2GalleryEntry, 8U> rt2Entries{{
        {"rt2_oak_planks", "rt2_oak_planks", MaterialStyle::Layered, MaterialWorkflow::SpecGloss,
         ri::math::Vec3{0.86f, 0.70f, 0.46f}, 0.02f, 0.74f, ri::math::Vec2{1.4f, 1.4f}},
        {"rt2_gold_block", "rt2_gold_block", MaterialStyle::MixedMedia, MaterialWorkflow::SpecGloss,
         ri::math::Vec3{1.0f, 0.84f, 0.36f}, 0.92f, 0.22f, ri::math::Vec2{1.0f, 1.0f}},
        {"rt2_copper_block", "rt2_copper_block", MaterialStyle::MixedMedia, MaterialWorkflow::SpecGloss,
         ri::math::Vec3{0.94f, 0.58f, 0.42f}, 0.88f, 0.30f, ri::math::Vec2{1.0f, 1.0f}},
        {"rt2_diamond_block", "rt2_diamond_block", MaterialStyle::Crystal, MaterialWorkflow::SpecGloss,
         ri::math::Vec3{0.62f, 0.96f, 0.96f}, 0.10f, 0.18f, ri::math::Vec2{1.0f, 1.0f}},
        {"rt2_amethyst_block", "rt2_amethyst_block", MaterialStyle::Crystal, MaterialWorkflow::SpecGloss,
         ri::math::Vec3{0.74f, 0.55f, 0.93f}, 0.06f, 0.24f, ri::math::Vec2{1.0f, 1.0f}},
        {"rt2_bricks", "rt2_bricks", MaterialStyle::Layered, MaterialWorkflow::SpecGloss,
         ri::math::Vec3{0.78f, 0.46f, 0.38f}, 0.03f, 0.78f, ri::math::Vec2{1.6f, 1.6f}},
        {"rt2_deepslate_tiles", "rt2_deepslate_tiles", MaterialStyle::Standard, MaterialWorkflow::SpecGloss,
         ri::math::Vec3{0.55f, 0.56f, 0.60f}, 0.05f, 0.66f, ri::math::Vec2{1.5f, 1.5f}},
        {"rt2_prismarine_bricks", "rt2_prismarine_bricks", MaterialStyle::Standard, MaterialWorkflow::SpecGloss,
         ri::math::Vec3{0.55f, 0.82f, 0.78f}, 0.08f, 0.52f, ri::math::Vec2{1.4f, 1.4f}},
    }};
    for (const Rt2GalleryEntry& entry : rt2Entries) {
        const std::string albedoRel = std::string("tile/") + std::string(entry.name) + ".png";
        const std::string normalRel = std::string("tile/") + std::string(entry.name) + "_n.png";
        const std::string specRel = std::string("tile/") + std::string(entry.name) + "_s.png";
        rows.push_back(StructuralGalleryMaterialPreset{
            .label = std::string(entry.label),
            .material = MakePackMaterial(packageRoot, entry.label, albedoRel, normalRel, specRel, entry.style,
                                         entry.workflow, entry.baseColor, entry.metallic, entry.roughness,
                                         entry.tiling),
        });
    }

    // Cross-reference showcase: reference ONLY the albedo and let the renderer auto-discover
    // the authored "_n" sibling and synthesise a high-quality ORM from that normal map. This
    // is the MetalRough path where generation is replaced/augmented by authored cross-data.
    rows.push_back(StructuralGalleryMaterialPreset{
        .label = "rt2_sandstone_xref",
        .material = MakePackMaterial(packageRoot,
                                     "rt2_sandstone_xref",
                                     "tile/rt2_sandstone.png",
                                     "tile/rt2_sandstone.png", // == albedo -> no authored normal wired
                                     "tile/rt2_sandstone.png", // == albedo -> no authored spec wired
                                     MaterialStyle::Standard,
                                     MaterialWorkflow::MetalRough,
                                     ri::math::Vec3{0.92f, 0.84f, 0.62f},
                                     0.02f,
                                     0.80f,
                                     ri::math::Vec2{1.5f, 1.5f}),
    });

    return rows;
}

void AddCatalogLight(Scene& scene,
                     const int parent,
                     const std::string& name,
                     const ri::math::Vec3& position,
                     const ri::math::Vec3& color,
                     const float intensity,
                     const float range) {
    LightNodeOptions options{};
    options.nodeName = name;
    options.parent = parent;
    options.transform.position = position;
    options.light = Light{
        .name = name,
        .type = LightType::Point,
        .color = color,
        .intensity = intensity,
        .range = range,
    };
    (void)AddLightNode(scene, options);
}

int AddCatalogPrimitive(Scene& scene,
                        const int parent,
                        const std::string& nodeName,
                        const PrimitiveType primitive,
                        const ri::math::Vec3& position,
                        const ri::math::Vec3& scale,
                        const ri::math::Vec3& color,
                        const std::string& materialName,
                        const std::string& texture,
                        const ri::math::Vec2& tiling,
                        const ri::math::Vec3& rotation = {}) {
    PrimitiveNodeOptions options{};
    options.nodeName = nodeName;
    options.parent = parent;
    options.primitive = primitive;
    options.materialName = materialName;
    options.transform.position = position;
    options.transform.scale = scale;
    options.transform.rotationDegrees = rotation;
    options.baseColor = color;
    options.baseColorTexture = texture;
    options.textureTiling = tiling;
    options.shadingModel = ShadingModel::Lit;
    options.materialStyle = MaterialStyle::Layered;
    options.roughness = 0.88f;
    options.metallic = 0.02f;
    return AddPrimitiveNode(scene, options);
}

// Authoring-only description of an LRT surface. The engine's Vulkan renderer
// (RawIron.Render.Vulkan) consumes the resolved albedo/normal/spec slots; the game
// never implements any rendering itself.
struct LrtSurfaceSpec {
    std::string tileBaseName;
    MaterialStyle style = MaterialStyle::Standard;
    float metallic = 0.02f;
    float roughness = 0.80f;
};

// Spawns a primitive that uses the full LRT PBR texture set (albedo + normal + spec/gloss)
// with the SpecGloss workflow, exactly like the showcase inspection cube. When the LRT pack
// is unavailable it falls back to the legacy single-texture authoring so the scene still loads.
int AddLrtSurfacePrimitive(Scene& scene,
                           const fs::path& packageRoot,
                           const int parent,
                           const std::string& nodeName,
                           const PrimitiveType primitive,
                           const ri::math::Vec3& position,
                           const ri::math::Vec3& scale,
                           const ri::math::Vec3& color,
                           const std::string& materialName,
                           const LrtSurfaceSpec& surface,
                           const std::string& fallbackTexture,
                           const ri::math::Vec2& tiling,
                           const ri::math::Vec3& rotation = {}) {
    PrimitiveNodeOptions options{};
    options.nodeName = nodeName;
    options.parent = parent;
    options.primitive = primitive;
    options.materialName = materialName;
    options.transform.position = position;
    options.transform.scale = scale;
    options.transform.rotationDegrees = rotation;
    options.baseColor = color;
    options.textureTiling = tiling;
    options.shadingModel = ShadingModel::Lit;

    const std::string albedoRelative = "tile/" + surface.tileBaseName + ".png";
    if (PackageTextureExists(packageRoot, albedoRelative)) {
        options.materialStyle = surface.style;
        options.materialWorkflow = MaterialWorkflow::SpecGloss;
        options.baseColorTexture = ToPackageTexturePath(packageRoot, albedoRelative);
        options.detailTexture = options.baseColorTexture;
        const std::string normalRelative = "tile/" + surface.tileBaseName + "_n.png";
        if (PackageTextureExists(packageRoot, normalRelative)) {
            options.normalTexture = ToPackageTexturePath(packageRoot, normalRelative);
        }
        const std::string specRelative = "tile/" + surface.tileBaseName + "_s.png";
        if (PackageTextureExists(packageRoot, specRelative)) {
            options.ormTexture = ToPackageTexturePath(packageRoot, specRelative);
        }
        options.metallic = surface.metallic;
        options.roughness = surface.roughness;
    } else {
        options.materialStyle = MaterialStyle::Layered;
        options.baseColorTexture = fallbackTexture;
        options.metallic = 0.02f;
        options.roughness = 0.88f;
    }
    return AddPrimitiveNode(scene, options);
}

void AddAccentBeacon(Scene& scene,
                     const int parent,
                     const std::string& nodePrefix,
                     const ri::math::Vec3& position,
                     const ri::math::Vec3& baseScale,
                     const ri::math::Vec3& baseColor,
                     const std::string& baseTexture,
                     const ri::math::Vec3& orbScale,
                     const ri::math::Vec3& orbColor,
                     const std::string& orbTexture,
                     const ri::math::Vec3& lightColor,
                     const float lightIntensity,
                     const float lightRange) {
    PrimitiveNodeOptions base{};
    base.nodeName = nodePrefix + "_base";
    base.parent = parent;
    base.primitive = PrimitiveType::Cube;
    base.materialName = nodePrefix + "_base_material";
    base.transform.position = position + ri::math::Vec3{0.0f, -((baseScale.y * 0.5f) - 0.08f), 0.0f};
    base.transform.scale = baseScale;
    base.baseColor = baseColor;
    base.baseColorTexture = baseTexture;
    base.textureTiling = ri::math::Vec2{1.0f, 1.0f};
    base.shadingModel = ShadingModel::Lit;
    base.materialStyle = MaterialStyle::MixedMedia;
    base.materialWorkflow = MaterialWorkflow::SpecGloss;
    base.metallic = 0.18f;
    base.roughness = 0.58f;
    base.emissiveColor = baseColor * 0.012f;
    (void)AddPrimitiveNode(scene, base);

    PrimitiveNodeOptions shell{};
    shell.nodeName = nodePrefix + "_shell";
    shell.parent = parent;
    shell.primitive = PrimitiveType::Sphere;
    shell.materialName = nodePrefix + "_shell_material";
    shell.transform.position = position + ri::math::Vec3{0.0f, baseScale.y * 0.55f, 0.0f};
    shell.transform.scale = orbScale * 1.22f;
    shell.baseColor = orbColor;
    shell.baseColorTexture = "glass.png";
    shell.textureTiling = ri::math::Vec2{1.0f, 1.0f};
    shell.shadingModel = ShadingModel::Lit;
    shell.materialStyle = MaterialStyle::Crystal;
    shell.materialWorkflow = MaterialWorkflow::SpecGloss;
    shell.roughness = 0.12f;
    shell.opacity = 0.22f;
    shell.transparent = true;
    shell.doubleSided = true;
    shell.emissiveColor = orbColor * 0.04f;
    (void)AddPrimitiveNode(scene, shell);

    PrimitiveNodeOptions orb{};
    orb.nodeName = nodePrefix + "_orb";
    orb.parent = parent;
    orb.primitive = PrimitiveType::Cube;
    orb.materialName = nodePrefix + "_orb_material";
    orb.transform.position = position + ri::math::Vec3{0.0f, baseScale.y * 0.55f, 0.0f};
    orb.transform.scale = orbScale;
    orb.baseColor = orbColor;
    orb.baseColorTexture = orbTexture;
    orb.textureTiling = ri::math::Vec2{1.0f, 1.0f};
    orb.shadingModel = ShadingModel::Lit;
    orb.materialStyle = MaterialStyle::Crystal;
    orb.materialWorkflow = MaterialWorkflow::SpecGloss;
    orb.metallic = 0.02f;
    orb.roughness = 0.15f;
    orb.emissiveColor = lightColor * 0.72f;
    orb.additiveBlend = true;
    orb.opacity = 1.0f;
    (void)AddPrimitiveNode(scene, orb);
    AddCatalogLight(scene,
                    parent,
                    nodePrefix + "_light",
                    position + ri::math::Vec3{0.0f, baseScale.y * 0.68f, 0.0f},
                    lightColor,
                    lightIntensity,
                    lightRange);
}

int AddMaterialInspectionRig(Scene& scene,
                             const int parent,
                             const ri::math::Vec3& position,
                             const fs::path& packageRoot,
                             int* outRigHandle) {
    const int rig = scene.CreateNode("SandboxMaterialInspectionRig", parent);
    scene.GetNode(rig).localTransform.position = position;
    if (outRigHandle != nullptr) {
        *outRigHandle = rig;
    }

    (void)AddLrtSurfacePrimitive(scene,
                                 packageRoot,
                                 rig,
                                 "SandboxMaterialInspectionPedestal",
                                 PrimitiveType::Cube,
                                 ri::math::Vec3{0.0f, -0.95f, 0.0f},
                                 ri::math::Vec3{3.2f, 0.35f, 3.2f},
                                 ri::math::Vec3{0.62f, 0.63f, 0.66f},
                                 "sandbox_material_inspection_pedestal",
                                 LrtSurfaceSpec{"RT_deepslate_tiles", MaterialStyle::Standard, 0.05f, 0.62f},
                                 "smooth_stone.png",
                                 ri::math::Vec2{2.0f, 2.0f});

    (void)AddLrtSurfacePrimitive(scene,
                                 packageRoot,
                                 rig,
                                 "SandboxMaterialInspectionBackdrop",
                                 PrimitiveType::Cube,
                                 ri::math::Vec3{0.0f, 2.3f, 8.4f},
                                 ri::math::Vec3{14.0f, 7.8f, 0.45f},
                                 ri::math::Vec3{0.40f, 0.42f, 0.48f},
                                 "sandbox_material_inspection_backdrop",
                                 LrtSurfaceSpec{"RT_polished_blackstone_bricks", MaterialStyle::Layered, 0.04f, 0.74f},
                                 "polished_blackstone_bricks.png",
                                 ri::math::Vec2{3.0f, 2.0f});

    PrimitiveNodeOptions cubeOptions{};
    cubeOptions.nodeName = "SandboxMaterialInspectionCube";
    cubeOptions.parent = rig;
    cubeOptions.primitive = PrimitiveType::Cube;
    cubeOptions.materialName = "sandbox_lrt_inspection_chiseled_copper";
    cubeOptions.transform.position = ri::math::Vec3{0.0f, 1.05f, 0.0f};
    cubeOptions.transform.rotationDegrees = ri::math::Vec3{8.0f, 18.0f, 0.0f};
    cubeOptions.transform.scale = ri::math::Vec3{2.1f, 2.1f, 2.1f};
    cubeOptions.baseColor = ri::math::Vec3{1.0f, 1.0f, 1.0f};
    cubeOptions.baseColorTexture = ToPackageTexturePath(packageRoot, "ctm/metalcolormap.png");
    cubeOptions.normalTexture = ToPackageTexturePath(packageRoot, "ctm/RT_all_chiseled_copper_1_n.png");
    cubeOptions.ormTexture = ToPackageTexturePath(packageRoot, "ctm/RT_all_chiseled_copper_1_s.png");
    cubeOptions.detailTexture = ToPackageTexturePath(packageRoot, "ctm/RT_all_chiseled_copper_1.png");
    cubeOptions.textureTiling = ri::math::Vec2{1.0f, 1.0f};
    cubeOptions.shadingModel = ShadingModel::Lit;
    cubeOptions.materialStyle = MaterialStyle::Standard;
    cubeOptions.materialWorkflow = MaterialWorkflow::SpecGloss;
    cubeOptions.metallic = 0.24f;
    cubeOptions.roughness = 0.24f;
    const int cube = AddPrimitiveNode(scene, cubeOptions);

    (void)AddCatalogLight(scene,
                          rig,
                          "SandboxMaterialInspectionKeyLight",
                          ri::math::Vec3{-1.4f, 3.2f, -2.2f},
                          ri::math::Vec3{1.0f, 0.96f, 0.90f},
                          11.0f,
                          16.0f);
    (void)AddCatalogLight(scene,
                          rig,
                          "SandboxMaterialInspectionRimLight",
                          ri::math::Vec3{2.5f, 2.1f, 1.6f},
                          ri::math::Vec3{0.76f, 0.88f, 1.0f},
                          6.0f,
                          13.0f);

    return cube;
}

} // namespace

World BuildWorld(const std::string_view sceneName, const fs::path& gameRoot) {
    World world{};
    world.scene = ri::scene::Scene(std::string(sceneName));
    Scene& scene = world.scene;

    world.handles.root = scene.CreateNode("MultiplayerSandboxExperienceRoot");

    LightNodeOptions sun{};
    sun.nodeName = "SandboxCatalogSun";
    sun.parent = world.handles.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-58.0f, 38.0f, 0.0f};
    sun.light = Light{
        .name = "SandboxCatalogSun",
        .type = LightType::Directional,
        .color = ri::math::Vec3{0.78f, 0.84f, 0.92f},
        .intensity = 1.25f,
    };
    world.handles.sun = AddLightNode(scene, sun);

    OrbitCameraOptions orbitCamera{};
    orbitCamera.parent = world.handles.root;
    orbitCamera.camera = Camera{
        .name = "SandboxCatalogOrbitCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 86.0f,
        .nearClip = 0.05f,
        .farClip = 1200.0f,
    };
    orbitCamera.orbit = OrbitCameraState{
        .target = ri::math::Vec3{0.0f, 2.5f, 0.0f},
        .distance = 86.0f,
        .yawDegrees = 34.0f,
        .pitchDegrees = -20.0f,
    };
    world.handles.orbitCamera = AddOrbitCamera(scene, orbitCamera);

    StructuralPrimitiveGalleryOptions galleryOptions{};
    galleryOptions.parent = world.handles.root;
    galleryOptions.nodeNamePrefix = "MultiplayerSandboxPrimitiveCatalog";
    galleryOptions.cellSpacing = ri::math::Vec2{4.9f, 5.8f};
    galleryOptions.itemScale = ri::math::Vec3{2.05f, 2.05f, 2.05f};
    galleryOptions.platformMargin = ri::math::Vec3{9.0f, 0.0f, 8.0f};
    galleryOptions.platformThickness = 0.8f;
    galleryOptions.platformTileWorldSize = 2.5f;
    const fs::path lrtPackageRoot = gameRoot.parent_path().parent_path() / kLrtPackageRelativePath;
    if (fs::exists(lrtPackageRoot)) {
        galleryOptions.materialRows = BuildLrtGalleryMaterials(lrtPackageRoot);
    }

    const StructuralPrimitiveGalleryResult gallery = SpawnStructuralPrimitiveGallery(scene, galleryOptions);
    world.catalogRoot = gallery.root;
    world.catalogExtents = gallery.boundsExtents;
    world.handles.floor = gallery.platform;

    if (gallery.platform != kInvalidHandle) {
        SubtreeColliderBuildOptions platformColliders{};
        platformColliders.idPrefix = "sandbox_catalog_platform";
        platformColliders.structural = true;
        (void)AppendTraceCollidersForSubtree(scene, gallery.platform, platformColliders, world.colliders);
    }

    const ri::math::Vec3 overlookCenter{
        -gallery.boundsExtents.x + 36.0f,
        3.9f,
        -gallery.boundsExtents.z + 13.5f,
    };
    const ri::math::Vec3 overlookScale{26.0f, 0.8f, 16.0f};
    const float overlookTileWorldSize = 1.5f;
    (void)AddLrtSurfacePrimitive(scene,
                                 lrtPackageRoot,
                                 world.handles.root,
                                 "SandboxCatalogOverlookDeck",
                                 PrimitiveType::Cube,
                                 overlookCenter,
                                 overlookScale,
                                 ri::math::Vec3{0.86f, 0.88f, 0.90f},
                                 "sandbox_overlook_deck",
                                 LrtSurfaceSpec{"RT_smooth_stone", MaterialStyle::Standard, 0.03f, 0.74f},
                                 "smooth_stone.png",
                                 ri::math::Vec2{overlookScale.x / overlookTileWorldSize,
                                                overlookScale.z / overlookTileWorldSize});
    (void)AddLrtSurfacePrimitive(scene,
                                 lrtPackageRoot,
                                 world.handles.root,
                                 "SandboxCatalogOverlookRamp",
                                 PrimitiveType::Cube,
                                 overlookCenter + ri::math::Vec3{-15.0f, -1.95f, 0.0f},
                                 ri::math::Vec3{10.0f, 1.6f, 9.0f},
                                 ri::math::Vec3{0.78f, 0.80f, 0.83f},
                                 "sandbox_overlook_ramp",
                                 LrtSurfaceSpec{"RT_smooth_stone", MaterialStyle::Standard, 0.03f, 0.78f},
                                 "smooth_stone.png",
                                 ri::math::Vec2{3.0f, 2.0f},
                                 ri::math::Vec3{0.0f, 0.0f, 0.0f});
    const LrtSurfaceSpec railSurface{"RT_iron_block", MaterialStyle::MixedMedia, 0.82f, 0.30f};
    const ri::math::Vec3 railColor{0.78f, 0.80f, 0.84f};
    (void)AddLrtSurfacePrimitive(scene,
                                 lrtPackageRoot,
                                 world.handles.root,
                                 "SandboxCatalogOverlookRailLeft",
                                 PrimitiveType::Cube,
                                 overlookCenter + ri::math::Vec3{0.0f, 1.35f, -7.7f},
                                 ri::math::Vec3{26.0f, 0.22f, 0.24f},
                                 railColor,
                                 "sandbox_overlook_rail_left",
                                 railSurface,
                                 "iron_block.png",
                                 ri::math::Vec2{6.0f, 1.0f});
    (void)AddLrtSurfacePrimitive(scene,
                                 lrtPackageRoot,
                                 world.handles.root,
                                 "SandboxCatalogOverlookRailRight",
                                 PrimitiveType::Cube,
                                 overlookCenter + ri::math::Vec3{0.0f, 1.35f, 7.7f},
                                 ri::math::Vec3{26.0f, 0.22f, 0.24f},
                                 railColor,
                                 "sandbox_overlook_rail_right",
                                 railSurface,
                                 "iron_block.png",
                                 ri::math::Vec2{6.0f, 1.0f});
    (void)AddLrtSurfacePrimitive(scene,
                                 lrtPackageRoot,
                                 world.handles.root,
                                 "SandboxCatalogOverlookRailFront",
                                 PrimitiveType::Cube,
                                 overlookCenter + ri::math::Vec3{12.8f, 1.35f, 0.0f},
                                 ri::math::Vec3{0.24f, 0.22f, 15.4f},
                                 railColor,
                                 "sandbox_overlook_rail_front",
                                 railSurface,
                                 "iron_block.png",
                                 ri::math::Vec2{1.0f, 4.0f});

    AddCatalogLight(scene,
                    world.handles.root,
                    "SandboxCatalogKeyLight",
                    ri::math::Vec3{-72.0f, 24.0f, -26.0f},
                    ri::math::Vec3{0.94f, 0.96f, 1.0f},
                    20.0f,
                    150.0f);
    AddCatalogLight(scene,
                    world.handles.root,
                    "SandboxCatalogFillLight",
                    ri::math::Vec3{68.0f, 16.0f, 28.0f},
                    ri::math::Vec3{0.78f, 0.92f, 1.0f},
                    12.0f,
                    130.0f);
    AddCatalogLight(scene,
                    world.handles.root,
                    "SandboxCatalogRowReadLight",
                    ri::math::Vec3{0.0f, 9.5f, 0.0f},
                    ri::math::Vec3{1.0f, 0.90f, 0.76f},
                    8.0f,
                    72.0f);
    AddCatalogLight(scene,
                    world.handles.root,
                    "SandboxCatalogOverlookLight",
                    overlookCenter + ri::math::Vec3{-6.0f, 6.5f, -2.5f},
                    ri::math::Vec3{1.0f, 0.97f, 0.92f},
                    10.0f,
                    48.0f);

    const bool enableShowcaseDecor = false;
    if (enableShowcaseDecor) {
        AddAccentBeacon(scene,
                        world.handles.root,
                        "SandboxAccentBeaconNorthWest",
                        overlookCenter + ri::math::Vec3{-10.2f, 3.1f, -5.1f},
                        ri::math::Vec3{0.82f, 2.0f, 0.82f},
                        ri::math::Vec3{0.40f, 0.30f, 0.18f},
                        "cut_copper.png",
                        ri::math::Vec3{1.10f, 1.10f, 1.10f},
                        ri::math::Vec3{0.98f, 0.90f, 0.76f},
                        "glowstone.png",
                        ri::math::Vec3{1.0f, 0.82f, 0.62f},
                        14.0f,
                        42.0f);
        AddAccentBeacon(scene,
                        world.handles.root,
                        "SandboxAccentBeaconNorthEast",
                        overlookCenter + ri::math::Vec3{10.2f, 3.1f, -5.1f},
                        ri::math::Vec3{0.82f, 2.0f, 0.82f},
                        ri::math::Vec3{0.18f, 0.26f, 0.36f},
                        "sea_lantern.png",
                        ri::math::Vec3{1.10f, 1.10f, 1.10f},
                        ri::math::Vec3{0.90f, 0.96f, 1.00f},
                        "glass.png",
                        ri::math::Vec3{0.78f, 0.92f, 1.0f},
                        14.0f,
                        42.0f);
        AddAccentBeacon(scene,
                        world.handles.root,
                        "SandboxAccentBeaconSouthWest",
                        overlookCenter + ri::math::Vec3{-10.2f, 3.1f, 5.1f},
                        ri::math::Vec3{0.82f, 2.0f, 0.82f},
                        ri::math::Vec3{0.46f, 0.34f, 0.20f},
                        "gold_block.png",
                        ri::math::Vec3{1.10f, 1.10f, 1.10f},
                        ri::math::Vec3{1.0f, 0.92f, 0.78f},
                        "red_stained_glass.png",
                        ri::math::Vec3{1.0f, 0.82f, 0.66f},
                        12.0f,
                        36.0f);
        AddAccentBeacon(scene,
                        world.handles.root,
                        "SandboxAccentBeaconSouthEast",
                        overlookCenter + ri::math::Vec3{10.2f, 3.1f, 5.1f},
                        ri::math::Vec3{0.82f, 2.0f, 0.82f},
                        ri::math::Vec3{0.36f, 0.26f, 0.44f},
                        "amethyst_block.png",
                        ri::math::Vec3{1.10f, 1.10f, 1.10f},
                        ri::math::Vec3{0.92f, 0.86f, 1.0f},
                        "purple_stained_glass.png",
                        ri::math::Vec3{0.82f, 0.70f, 1.0f},
                        13.0f,
                        38.0f);
        AddAccentBeacon(scene,
                        world.handles.root,
                        "SandboxHeroBeacon",
                        overlookCenter + ri::math::Vec3{2.4f, 4.1f, 0.0f},
                        ri::math::Vec3{1.00f, 2.4f, 1.00f},
                        ri::math::Vec3{0.30f, 0.22f, 0.14f},
                        "raw_gold_block.png",
                        ri::math::Vec3{1.20f, 1.20f, 1.20f},
                        ri::math::Vec3{1.0f, 0.94f, 0.76f},
                        "sea_lantern.png",
                        ri::math::Vec3{1.0f, 0.90f, 0.62f},
                        16.0f,
                        50.0f);

        const float showcaseFrameHalfWidth = gallery.boundsExtents.x + 10.5f;
        const float showcaseFrameHalfDepth = gallery.boundsExtents.z + 9.5f;
        const float showcaseFrameHeight = 15.5f;
        const float showcaseColumnHeight = showcaseFrameHeight - 1.0f;
        const float showcaseBeamY = showcaseFrameHeight - 0.45f;

        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseColumnNorthWest",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{-showcaseFrameHalfWidth, showcaseColumnHeight * 0.5f, -showcaseFrameHalfDepth},
                                  ri::math::Vec3{0.72f, showcaseColumnHeight, 0.72f},
                                  ri::math::Vec3{0.18f, 0.20f, 0.24f},
                                  "sandbox_showcase_column_nw",
                                  "iron_block.png",
                                  ri::math::Vec2{1.0f, 6.0f});
        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseColumnNorthEast",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{showcaseFrameHalfWidth, showcaseColumnHeight * 0.5f, -showcaseFrameHalfDepth},
                                  ri::math::Vec3{0.72f, showcaseColumnHeight, 0.72f},
                                  ri::math::Vec3{0.18f, 0.20f, 0.24f},
                                  "sandbox_showcase_column_ne",
                                  "iron_block.png",
                                  ri::math::Vec2{1.0f, 6.0f});
        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseColumnSouthWest",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{-showcaseFrameHalfWidth, showcaseColumnHeight * 0.5f, showcaseFrameHalfDepth},
                                  ri::math::Vec3{0.72f, showcaseColumnHeight, 0.72f},
                                  ri::math::Vec3{0.18f, 0.20f, 0.24f},
                                  "sandbox_showcase_column_sw",
                                  "iron_block.png",
                                  ri::math::Vec2{1.0f, 6.0f});
        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseColumnSouthEast",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{showcaseFrameHalfWidth, showcaseColumnHeight * 0.5f, showcaseFrameHalfDepth},
                                  ri::math::Vec3{0.72f, showcaseColumnHeight, 0.72f},
                                  ri::math::Vec3{0.18f, 0.20f, 0.24f},
                                  "sandbox_showcase_column_se",
                                  "iron_block.png",
                                  ri::math::Vec2{1.0f, 6.0f});

        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseBeamNorth",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{0.0f, showcaseBeamY, -showcaseFrameHalfDepth},
                                  ri::math::Vec3{showcaseFrameHalfWidth * 2.0f + 1.4f, 0.42f, 0.68f},
                                  ri::math::Vec3{0.62f, 0.66f, 0.71f},
                                  "sandbox_showcase_beam_north",
                                  "smooth_stone.png",
                                  ri::math::Vec2{8.0f, 1.0f});
        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseBeamSouth",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{0.0f, showcaseBeamY, showcaseFrameHalfDepth},
                                  ri::math::Vec3{showcaseFrameHalfWidth * 2.0f + 1.4f, 0.42f, 0.68f},
                                  ri::math::Vec3{0.62f, 0.66f, 0.71f},
                                  "sandbox_showcase_beam_south",
                                  "smooth_stone.png",
                                  ri::math::Vec2{8.0f, 1.0f});
        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseBeamWest",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{-showcaseFrameHalfWidth, showcaseBeamY, 0.0f},
                                  ri::math::Vec3{0.68f, 0.42f, showcaseFrameHalfDepth * 2.0f + 1.4f},
                                  ri::math::Vec3{0.62f, 0.66f, 0.71f},
                                  "sandbox_showcase_beam_west",
                                  "smooth_stone.png",
                                  ri::math::Vec2{1.0f, 8.0f});
        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseBeamEast",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{showcaseFrameHalfWidth, showcaseBeamY, 0.0f},
                                  ri::math::Vec3{0.68f, 0.42f, showcaseFrameHalfDepth * 2.0f + 1.4f},
                                  ri::math::Vec3{0.62f, 0.66f, 0.71f},
                                  "sandbox_showcase_beam_east",
                                  "smooth_stone.png",
                                  ri::math::Vec2{1.0f, 8.0f});
        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseScrimNorth",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{0.0f, showcaseColumnHeight * 0.5f, -showcaseFrameHalfDepth - 0.55f},
                                  ri::math::Vec3{showcaseFrameHalfWidth * 1.8f, showcaseColumnHeight, 0.36f},
                                  ri::math::Vec3{0.32f, 0.34f, 0.38f},
                                  "sandbox_showcase_scrim_north",
                                  "polished_blackstone_bricks.png",
                                  ri::math::Vec2{6.0f, 4.0f});
        (void)AddCatalogPrimitive(scene,
                                  world.handles.root,
                                  "SandboxShowcaseScrimSouth",
                                  PrimitiveType::Cube,
                                  ri::math::Vec3{0.0f, showcaseColumnHeight * 0.5f, showcaseFrameHalfDepth + 0.55f},
                                  ri::math::Vec3{showcaseFrameHalfWidth * 1.8f, showcaseColumnHeight, 0.36f},
                                  ri::math::Vec3{0.32f, 0.34f, 0.38f},
                                  "sandbox_showcase_scrim_south",
                                  "polished_blackstone_bricks.png",
                                  ri::math::Vec2{6.0f, 4.0f});

        AddCatalogLight(scene,
                        world.handles.root,
                        "SandboxShowcaseWarmKey",
                        ri::math::Vec3{-showcaseFrameHalfWidth * 0.70f, showcaseFrameHeight - 1.8f, -showcaseFrameHalfDepth * 0.65f},
                        ri::math::Vec3{1.0f, 0.86f, 0.72f},
                        12.0f,
                        62.0f);
        AddCatalogLight(scene,
                        world.handles.root,
                        "SandboxShowcaseCoolFill",
                        ri::math::Vec3{showcaseFrameHalfWidth * 0.58f, showcaseFrameHeight - 2.2f, showcaseFrameHalfDepth * 0.55f},
                        ri::math::Vec3{0.78f, 0.90f, 1.0f},
                        10.0f,
                        58.0f);
    }

    const ri::math::Vec3 playerEye{
        overlookCenter.x - 7.5f,
        overlookCenter.y + 1.62f,
        overlookCenter.z - 1.2f,
    };

    const ri::math::Vec3 brushHallOrigin{
        playerEye.x + 22.0f,
        overlookCenter.y - 3.5f,
        playerEye.z + 14.0f,
    };
    const StructuralPrimitiveAssemblyResult structuralHall =
        SpawnSandboxStructuralHall(scene, world.handles.root, brushHallOrigin, "SandboxStructuralHall");
    world.brushHallRoot = structuralHall.root;
    if (structuralHall.root != kInvalidHandle) {
        SubtreeColliderBuildOptions hallColliders{};
        hallColliders.idPrefix = "sandbox_structural_hall";
        hallColliders.structural = true;
        (void)AppendTraceCollidersForSubtree(scene, structuralHall.root, hallColliders, world.colliders);
    }

    const float initialYawRadians = 68.0f * 0.017453292519943295f;
    const ri::math::Vec3 playerForward{std::sin(initialYawRadians), 0.0f, std::cos(initialYawRadians)};
    world.inspectionCube = AddMaterialInspectionRig(
        scene,
        world.handles.root,
        playerEye + (playerForward * 7.8f) + ri::math::Vec3{0.0f, -0.9f, 0.0f},
        lrtPackageRoot,
        &world.inspectionRig);

    world.colliders.push_back(ri::trace::TraceCollider{
        .id = "multiplayer-sandbox-overlook-deck",
        .bounds =
            ri::spatial::Aabb{
                .min = ri::math::Vec3{
                    overlookCenter.x - (overlookScale.x * 0.5f),
                    overlookCenter.y - (overlookScale.y * 0.5f),
                    overlookCenter.z - (overlookScale.z * 0.5f),
                },
                .max = ri::math::Vec3{
                    overlookCenter.x + (overlookScale.x * 0.5f),
                    overlookCenter.y + (overlookScale.y * 0.5f),
                    overlookCenter.z + (overlookScale.z * 0.5f),
                },
            },
        .structural = true,
    });

    world.playerRig = scene.CreateNode("SandboxPlayerRig", world.handles.root);
    ri::scene::Node& playerRig = scene.GetNode(world.playerRig);
    playerRig.localTransform.position =
        ri::math::Vec3{overlookCenter.x - 7.5f, overlookCenter.y + 1.62f, overlookCenter.z - 1.2f};
    playerRig.localTransform.rotationDegrees = ri::math::Vec3{0.0f, 68.0f, 0.0f};

    world.playerCameraNode = scene.CreateNode("SandboxPlayerCameraNode", world.playerRig);
    const int playerCamera = scene.AddCamera(Camera{
        .name = "SandboxPlayerCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 84.0f,
        .nearClip = 0.05f,
        .farClip = 1200.0f,
    });
    scene.AttachCamera(world.playerCameraNode, playerCamera);
    scene.GetNode(world.playerCameraNode).localTransform.rotationDegrees = ri::math::Vec3{-12.0f, 0.0f, 0.0f};

    world.handles.crate = world.playerRig;
    world.handles.beacon = world.playerCameraNode;
    return world;
}

void AnimateWorld(World& world, const double elapsedSeconds) {
    if (world.inspectionCube != ri::scene::kInvalidHandle) {
        ri::scene::Node& cube = world.scene.GetNode(world.inspectionCube);
        const float t = static_cast<float>(elapsedSeconds);
        cube.localTransform.rotationDegrees = ri::math::Vec3{
            10.0f + std::sin(t * 0.65f) * 3.0f,
            std::fmod(t * 13.5f, 360.0f),
            2.5f + std::sin(t * 0.33f) * 1.0f,
        };
    }
}

} // namespace ri::games::multiplayersandbox
