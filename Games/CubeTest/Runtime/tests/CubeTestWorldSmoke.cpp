#include "RawIron/Games/CubeTest/CubeTestWorld.h"

#include "RawIron/Content/RipakArchive.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/Scene.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
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
    ri::games::cubetest::AnimateCubeTestWorld(world, 1.0);
    ok &= Require(world.scene.GetNode(world.cubeNode).localTransform.rotationDegrees.y > initialYaw + 30.0f,
                  "streaming cube should spin continuously");

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
