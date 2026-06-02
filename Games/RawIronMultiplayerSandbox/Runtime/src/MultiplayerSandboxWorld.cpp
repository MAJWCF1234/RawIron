#include "RawIron/Games/MultiplayerSandbox/MultiplayerSandboxWorld.h"

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/StructuralPrimitiveBundle.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::games::multiplayersandbox {

namespace {

namespace fs = std::filesystem;

using namespace ri::scene;

constexpr const char* kLrtPackageRelativePath = "Assets\\Packages\\LRT - Texture Pack - RT28.8 - 128x";

std::string ToPackageTexturePath(const fs::path& packageRoot, const std::string_view relativePath) {
    return (packageRoot / fs::path(relativePath)).lexically_normal().string();
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
    material.baseColorTexture = ToPackageTexturePath(packageRoot, albedoRelativePath);
    material.normalTexture = ToPackageTexturePath(packageRoot, normalRelativePath);
    material.detailTexture = material.baseColorTexture;
    if (!specRelativePath.empty()) {
        material.ormTexture = ToPackageTexturePath(packageRoot, specRelativePath);
    }
    material.textureTiling = tiling;
    material.metallic = metallic;
    material.roughness = roughness;
    return material;
}

std::vector<StructuralGalleryMaterialPreset> BuildLrtGalleryMaterials(const fs::path& packageRoot) {
    std::vector<StructuralGalleryMaterialPreset> rows;
    rows.reserve(6U);

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
        .label = "lrt_cut_copper",
        .material = MakePackMaterial(packageRoot,
                                     "cut_copper",
                                     "ctm/RT_all_cut_copper_1.png",
                                     "ctm/RT_all_cut_copper_1_n.png",
                                     "ctm/RT_all_cut_copper_1_s.png",
                                     MaterialStyle::MixedMedia,
                                     MaterialWorkflow::SpecGloss,
                                     ri::math::Vec3{0.92f, 0.76f, 0.66f},
                                     0.70f,
                                     0.30f,
                                     ri::math::Vec2{2.0f, 2.0f}),
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
        .intensity = 1.55f,
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
    const fs::path lrtPackageRoot = gameRoot.parent_path().parent_path() / kLrtPackageRelativePath;
    if (fs::exists(lrtPackageRoot)) {
        galleryOptions.materialRows = BuildLrtGalleryMaterials(lrtPackageRoot);
    }

    const StructuralPrimitiveGalleryResult gallery = SpawnStructuralPrimitiveGallery(scene, galleryOptions);
    world.catalogRoot = gallery.root;
    world.catalogExtents = gallery.boundsExtents;
    world.handles.floor = gallery.platform;

    const ri::math::Vec3 overlookCenter{
        -gallery.boundsExtents.x + 36.0f,
        3.9f,
        -gallery.boundsExtents.z + 13.5f,
    };
    const ri::math::Vec3 overlookScale{26.0f, 0.8f, 16.0f};
    (void)AddCatalogPrimitive(scene,
                              world.handles.root,
                              "SandboxCatalogOverlookDeck",
                              PrimitiveType::Cube,
                              overlookCenter,
                              overlookScale,
                              ri::math::Vec3{0.70f, 0.73f, 0.76f},
                              "sandbox_overlook_deck",
                              "smooth_stone.png",
                              ri::math::Vec2{6.0f, 4.0f});
    (void)AddCatalogPrimitive(scene,
                              world.handles.root,
                              "SandboxCatalogOverlookRamp",
                              PrimitiveType::Cube,
                              overlookCenter + ri::math::Vec3{-15.0f, -1.95f, 0.0f},
                              ri::math::Vec3{10.0f, 1.6f, 9.0f},
                              ri::math::Vec3{0.58f, 0.60f, 0.63f},
                              "sandbox_overlook_ramp",
                              "smooth_stone.png",
                              ri::math::Vec2{3.0f, 2.0f},
                              ri::math::Vec3{0.0f, 0.0f, 0.0f});
    (void)AddCatalogPrimitive(scene,
                              world.handles.root,
                              "SandboxCatalogOverlookRailLeft",
                              PrimitiveType::Cube,
                              overlookCenter + ri::math::Vec3{0.0f, 1.35f, -7.7f},
                              ri::math::Vec3{26.0f, 0.22f, 0.24f},
                              ri::math::Vec3{0.22f, 0.25f, 0.30f},
                              "sandbox_overlook_rail_left",
                              "iron_block.png",
                              ri::math::Vec2{6.0f, 1.0f});
    (void)AddCatalogPrimitive(scene,
                              world.handles.root,
                              "SandboxCatalogOverlookRailRight",
                              PrimitiveType::Cube,
                              overlookCenter + ri::math::Vec3{0.0f, 1.35f, 7.7f},
                              ri::math::Vec3{26.0f, 0.22f, 0.24f},
                              ri::math::Vec3{0.22f, 0.25f, 0.30f},
                              "sandbox_overlook_rail_right",
                              "iron_block.png",
                              ri::math::Vec2{6.0f, 1.0f});
    (void)AddCatalogPrimitive(scene,
                              world.handles.root,
                              "SandboxCatalogOverlookRailFront",
                              PrimitiveType::Cube,
                              overlookCenter + ri::math::Vec3{12.8f, 1.35f, 0.0f},
                              ri::math::Vec3{0.24f, 0.22f, 15.4f},
                              ri::math::Vec3{0.22f, 0.25f, 0.30f},
                              "sandbox_overlook_rail_front",
                              "iron_block.png",
                              ri::math::Vec2{1.0f, 4.0f});

    AddCatalogLight(scene,
                    world.handles.root,
                    "SandboxCatalogKeyLight",
                    ri::math::Vec3{-72.0f, 24.0f, -26.0f},
                    ri::math::Vec3{0.94f, 0.96f, 1.0f},
                    26.0f,
                    150.0f);
    AddCatalogLight(scene,
                    world.handles.root,
                    "SandboxCatalogFillLight",
                    ri::math::Vec3{68.0f, 16.0f, 28.0f},
                    ri::math::Vec3{0.78f, 0.92f, 1.0f},
                    15.0f,
                    130.0f);
    AddCatalogLight(scene,
                    world.handles.root,
                    "SandboxCatalogRowReadLight",
                    ri::math::Vec3{0.0f, 9.5f, 0.0f},
                    ri::math::Vec3{1.0f, 0.90f, 0.76f},
                    10.0f,
                    72.0f);
    AddCatalogLight(scene,
                    world.handles.root,
                    "SandboxCatalogOverlookLight",
                    overlookCenter + ri::math::Vec3{-6.0f, 6.5f, -2.5f},
                    ri::math::Vec3{1.0f, 0.97f, 0.92f},
                    13.0f,
                    48.0f);

    world.colliders.push_back(ri::trace::TraceCollider{
        .id = "multiplayer-sandbox-primitive-catalog-platform",
        .bounds =
            ri::spatial::Aabb{
                .min = ri::math::Vec3{-gallery.boundsExtents.x, -0.86f, -gallery.boundsExtents.z},
                .max = ri::math::Vec3{gallery.boundsExtents.x, 0.08f, gallery.boundsExtents.z},
            },
        .structural = true,
    });
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
    (void)world;
    (void)elapsedSeconds;
}

} // namespace ri::games::multiplayersandbox
