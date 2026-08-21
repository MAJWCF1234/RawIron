#include "RawIron/Games/CubeTest/CubeTestWorld.h"
#include "RawIron/Games/CubeTest/CubeTestAuthority.h"

#include "RawIron/Content/RipakArchive.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/Scene.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>

namespace {

bool Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

std::uint64_t HashPixels(const std::vector<std::uint8_t>& pixels) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const std::uint8_t pixel : pixels) {
        hash = (hash ^ pixel) * 1099511628211ULL;
    }
    return hash;
}

bool MatricesNear(const ri::math::Mat4& lhs, const ri::math::Mat4& rhs) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (std::abs(lhs.m[row][column] - rhs.m[row][column]) > 1.0e-6f) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    ri::games::cubetest::CubeTestWorld world =
        ri::games::cubetest::BuildCubeTestWorld("Cube Test Smoke");

    bool ok = true;
    ok &= Require(world.rootNode != ri::scene::kInvalidHandle, "world should expose a valid root node");
    ok &= Require(world.platformNode != ri::scene::kInvalidHandle, "world should expose a valid platform node");
    ok &= Require(world.cubeNode != ri::scene::kInvalidHandle, "world should expose a valid cube node");
    ok &= Require(world.playerCameraNode != ri::scene::kInvalidHandle, "world should expose a valid camera node");
    ok &= Require(!world.colliders.empty(), "world should include at least one structural collider");
    ok &= Require(world.colliders.size() >= 6U, "capability gallery should expose collider-backed test areas");
    ok &= Require(world.portals.size() == 12U, "capability gallery should expose six bidirectional portal links");
    ok &= Require(world.interactionRoomRoot != ri::scene::kInvalidHandle,
                  "gallery should expose the native XR interaction room");
    ok &= Require(world.interactionProps.size() == 24U
                      && world.interactionPropNodes.size() == world.interactionProps.size(),
                  "XR interaction room should bind every native prop state to a scene node");
    ok &= Require(world.projectileRoomRoot != ri::scene::kInvalidHandle,
                  "gallery should expose the native pooled-projectile room");
    ok &= Require(world.projectileProps.size() == 50U
                      && world.projectilePropNodes.size() == world.projectileProps.size(),
                  "projectile room should bind targets and every fixed pool slot to scene nodes");
    ok &= Require(world.teleportRoomRoot != ri::scene::kInvalidHandle,
                  "gallery should expose the trace-validated teleport room");
    const ri::world::InteractivePropEmissionResult fired =
        ri::games::cubetest::EmitCubeTestProjectile(
            world, {125.1f, 1.5f, 0.0f}, {1.0f, 0.0f, 0.0f});
    ok &= Require(fired.propIndex >= 18 && world.projectileProps[fired.propIndex].active,
                  "shared room API should activate a pooled projectile");
    ri::games::cubetest::CubeTestWorld authorityClientWorld =
        ri::games::cubetest::BuildCubeTestWorld("Cube Test Authority Client");
    ri::games::cubetest::CubeTestAuthorityBridge authorityHost(&world);
    ri::games::cubetest::CubeTestAuthorityBridge authorityClient(&authorityClientWorld);
    const std::vector<std::uint8_t> authorityProjectile =
        ri::games::cubetest::CubeTestAuthorityBridge::BuildProjectileCommand(
            {125.2f, 1.4f, 0.0f}, {1.0f, 0.0f, 0.0f});
    std::string authorityError;
    const std::size_t activeBeforeAuthorityCommand = static_cast<std::size_t>(std::count_if(
        world.projectileProps.begin(), world.projectileProps.end(),
        [](const ri::world::InteractivePropState& prop) { return prop.active; }));
    ok &= Require(authorityHost.HandleCommand(7U, 0U, authorityProjectile, &authorityError),
                  "authority bridge should accept a bounded projectile command");
    const std::size_t activeAfterAuthorityCommand = static_cast<std::size_t>(std::count_if(
        world.projectileProps.begin(), world.projectileProps.end(),
        [](const ri::world::InteractivePropState& prop) { return prop.active; }));
    ok &= Require(activeAfterAuthorityCommand > activeBeforeAuthorityCommand,
                  "authority command should mutate only the host physics pool");
    const auto authoritySnapshot = authorityHost.CaptureSnapshot(42U);
    ok &= Require(authoritySnapshot.has_value()
                      && authorityClient.ApplySnapshot(*authoritySnapshot, &authorityError),
                  "desktop and XR authority bridge should round-trip both dynamic pools");
    ok &= Require(authorityClientWorld.projectileProps.size() == world.projectileProps.size()
                      && authorityClientWorld.projectileProps[18].active == world.projectileProps[18].active
                      && std::abs(authorityClientWorld.projectileProps[18].position.x
                                  - world.projectileProps[18].position.x) < 1.0e-6f,
                  "authority snapshot should reproduce the host projectile state exactly");
    ok &= Require(world.spriteNode != ri::scene::kInvalidHandle, "sprite room should expose a native sprite cloud");
    ok &= Require(world.spriteFrameMeshes.size() == 32U, "sprite room should prebuild smooth frames for four layouts");
    const int initialSpriteMesh = world.scene.GetNode(world.spriteNode).mesh;
    const ri::scene::Mesh& spriteMesh = world.scene.GetMesh(initialSpriteMesh);
    ok &= Require(spriteMesh.positions.size() == 512U * 4U,
                  "sprite cloud should pack all 512 reference sprites into one mesh draw");
    ok &= Require(spriteMesh.geometryMode == ri::scene::MeshGeometryMode::CameraFacingSpriteQuads,
                  "sprite cloud should use the engine camera-facing mesh contract");
    ok &= Require(spriteMesh.billboardOffsets.size() == spriteMesh.positions.size(),
                  "sprite cloud should provide an explicit billboard-offset stream");
    ok &= Require(world.exporterInstanceBatch != ri::scene::kInvalidHandle,
                  "export room should expose a hierarchy and instance-batch sample");
    ok &= Require(world.shaderBallNode != ri::scene::kInvalidHandle,
                  "export room should import the exact Three.js ShaderBall glTF fixture");
    ok &= Require(world.coffeeModelNode != ri::scene::kInvalidHandle,
                  "export room should import the exact meshopt-compressed coffee fixture");
    const std::filesystem::path referenceModelRoot = std::filesystem::path(__FILE__).parent_path()
        / ".." / ".." / "assets" / "reference" / "threejs-r185" / "models" / "gltf";
    const ri::scene::ModelSourceValidationReport shaderBallReport =
        ri::scene::ValidateModelSource(referenceModelRoot / "ShaderBall.glb");
    ok &= Require(shaderBallReport.valid,
                  "Raw Iron should import the quantized Three.js ShaderBall fixture through its public model API");
    const ri::scene::ModelSourceValidationReport coffeeReport =
        ri::scene::ValidateModelSource(referenceModelRoot / "coffeemat.glb");
    if (!coffeeReport.valid) {
        std::cerr << "coffeemat import: " << coffeeReport.summary << '\n';
    }
    ok &= Require(coffeeReport.valid,
                  "Raw Iron should decode the meshopt-compressed Three.js coffee fixture through its public model API");
    ok &= Require(std::filesystem::path(
                      world.scene.GetMaterial(world.scene.GetNode(world.spriteNode).material).baseColorTexture)
                      .filename() == "sprite.png",
                  "sprite room should use the exact Three.js sprite fixture");
    int normalFixtureCount = 0;
    for (const ri::scene::Node& node : world.scene.Nodes()) {
        if (node.name != "CubeTest_OpenGLNormalPanel" && node.name != "CubeTest_InvertedNormalPanel") {
            continue;
        }
        const ri::scene::Material& panelMaterial = world.scene.GetMaterial(node.material);
        ok &= Require(std::filesystem::is_regular_file(panelMaterial.normalTexture),
                      "normal-map panels should resolve exact Three.js texture fixtures");
        if (node.name == "CubeTest_OpenGLNormalPanel") {
            ok &= Require(panelMaterial.normalScale.y > 0.0f,
                          "OpenGL normal fixture should preserve its green channel");
        } else {
            ok &= Require(panelMaterial.normalScale.y < 0.0f,
                          "DirectX normal fixture should invert its green channel in the material contract");
        }
        ++normalFixtureCount;
    }
    ok &= Require(normalFixtureCount == 2, "normal-map room should expose both convention fixtures");

    const ri::scene::Node& cube = world.scene.GetNode(world.cubeNode);
    ok &= Require(cube.material != ri::scene::kInvalidHandle, "cube should have a material");

    const ri::scene::Material& material = world.scene.GetMaterial(cube.material);
    ok &= Require(!material.baseColorTexture.empty(), "cube material should have an M-mesh base color map");
    ok &= Require(!material.normalTexture.empty(), "cube material should have an M-mesh normal map");
    ok &= Require(!material.ormTexture.empty(), "cube material should have an M-mesh packed spec/ORM map");
    ok &= Require(!material.detailTexture.empty(), "cube material should have a detail map for material stress testing");
    ok &= Require(cube.localTransform.scale.x < 1.5f && cube.localTransform.scale.y < 1.5f,
                  "streaming cube should remain a small focused sample");
    for (const int sampleNode : {
             world.cubeNode,
             world.goldSampleNode,
             world.copperSampleNode,
             world.ironSampleNode,
             world.crystalSampleNode,
         }) {
        const ri::scene::Node& sample = world.scene.GetNode(sampleNode);
        const ri::scene::Material& sampleMaterial = world.scene.GetMaterial(sample.material);
        ok &= Require(std::filesystem::is_regular_file(sampleMaterial.baseColorTexture),
                      "Cube Test albedo map should resolve to an existing package file");
        ok &= Require(std::filesystem::is_regular_file(sampleMaterial.normalTexture),
                      "Cube Test normal map should resolve to an existing package file");
        ok &= Require(std::filesystem::is_regular_file(sampleMaterial.ormTexture),
                      "Cube Test spec/ORM map should resolve to an existing package file");
    }

    ri::games::cubetest::ConfigureCookedTextureCube(
        world, ri::games::cubetest::CubeTestCookedTextureSequence());
    const ri::scene::Material& cookedMaterial = world.scene.GetMaterial(cube.material);
    ok &= Require(cookedMaterial.baseColorTextureFrames.size() >= 5U,
                  "streaming cube should expose several cooked texture frames");
    ok &= Require(cookedMaterial.normalTexture.empty() && cookedMaterial.ormTexture.empty(),
                  "cooked albedo demo should use safe fallback maps instead of loose sidecars");
    const float initialYaw = cube.localTransform.rotationDegrees.y;
    const ri::math::Mat4 initialCameraWorld = world.scene.ComputeWorldMatrix(world.playerCameraNode);
    ri::games::cubetest::AnimateCubeTestWorld(world, 1.0);
    ok &= Require(world.scene.GetNode(world.cubeNode).localTransform.rotationDegrees.y > initialYaw + 30.0f,
                  "streaming cube should spin continuously");
    ri::games::cubetest::AnimateCubeTestWorld(world, 6.5);
    ok &= Require(world.scene.GetNode(world.spriteNode).mesh != initialSpriteMesh,
                  "sprite room should transition between native layouts");
    ok &= Require(MatricesNear(initialCameraWorld, world.scene.ComputeWorldMatrix(world.playerCameraNode)),
                  "world animation must never orbit or rotate the player camera");

    ri::render::software::ScenePreviewOptions previewOptions{};
    previewOptions.width = 320;
    previewOptions.height = 180;
    previewOptions.textureRoot = ri::render::software::DefaultEngineTextureRoot();
    previewOptions.clearTop = {0.58f, 0.66f, 0.72f};
    previewOptions.clearBottom = {0.32f, 0.35f, 0.36f};
    previewOptions.fogStartDepth = 18.0f;
    previewOptions.fogEndDepth = 80.0f;
    previewOptions.fogStrength = 0.28f;
    std::unordered_set<std::uint64_t> frameHashes;
    for (int frameIndex = 0; frameIndex < 8; ++frameIndex) {
        ri::games::cubetest::CubeTestWorld frameWorld =
            ri::games::cubetest::BuildCubeTestWorld("Cube Test Render Safety");
        const double seconds = static_cast<double>(frameIndex) / 12.0;
        ri::games::cubetest::AnimateCubeTestWorldJiggle(frameWorld, seconds);
        previewOptions.animationTimeSeconds = seconds;
        const ri::render::software::SoftwareImage frame = ri::render::software::RenderScenePreview(
            frameWorld.scene, frameWorld.playerCameraNode, previewOptions);
        ok &= Require(frame.width == previewOptions.width && frame.height == previewOptions.height,
                      "jiggle preview should preserve its requested dimensions");
        ok &= Require(frame.pixels.size()
                          == static_cast<std::size_t>(previewOptions.width * previewOptions.height * 3),
                      "jiggle preview should return a complete RGB frame");
        std::size_t nearBlackPixels = 0U;
        for (std::size_t offset = 0; offset + 2U < frame.pixels.size(); offset += 3U) {
            if (frame.pixels[offset] < 4U && frame.pixels[offset + 1U] < 4U
                && frame.pixels[offset + 2U] < 4U) {
                ++nearBlackPixels;
            }
        }
        ok &= Require(nearBlackPixels < static_cast<std::size_t>(frame.width * frame.height) / 100U,
                      "jiggle preview should not generate screen-filling black raster artifacts");
        frameHashes.insert(HashPixels(frame.pixels));
    }
    ok &= Require(frameHashes.size() >= 4U,
                  "jiggle preview sequence should contain multiple distinct rendered frames");

    if (argc == 2) {
        auto pack = std::make_shared<ri::content::CookedTexturePack>(
            ri::content::CookedTexturePack::Open(argv[1], "indexes/RAWIRONX32.index.json"));
        ri::games::cubetest::CubeTestWorld cookedWorld =
            ri::games::cubetest::BuildCubeTestWorld("Cube Test Cooked Streaming");
        ri::games::cubetest::ConfigureCookedTextureCube(
            cookedWorld, ri::games::cubetest::CubeTestCookedTextureSequence());
        ri::render::software::ScenePreviewOptions cookedOptions = previewOptions;
        cookedOptions.cookedTexturePack = pack;
        ri::render::software::ScenePreviewCache cookedCache{};
        std::unordered_set<std::uint64_t> cookedHashes;
        for (const double seconds : {0.0, 1.4, 2.8}) {
            cookedOptions.animationTimeSeconds = seconds;
            const ri::render::software::SoftwareImage frame = ri::render::software::RenderScenePreview(
                cookedWorld.scene, cookedWorld.playerCameraNode, cookedOptions, &cookedCache);
            cookedHashes.insert(HashPixels(frame.pixels));
        }
        ok &= Require(cookedHashes.size() == 3U,
                      "three cooked texture intervals should produce three distinct cube frames");
        ok &= Require(cookedCache.textures.size() >= 3U,
                      "software renderer should cache each requested cooked texture without extraction");
    } else if (argc != 1) {
        ok &= Require(false, "expected optional RAWIRONX32.ripak path only");
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
