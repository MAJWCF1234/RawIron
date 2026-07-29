#include "RawIron/Core/ImageComparison.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/Helpers.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

ri::render::software::SoftwareImage RenderLayeredCubes(const bool createNearFirst) {
    ri::scene::Scene scene{"TransparencyOrder"};
    ri::scene::OrbitCameraHandles camera = ri::scene::AddOrbitCamera(
        scene,
        ri::scene::OrbitCameraOptions{
            .orbit = {
                .target = {0.0f, 0.0f, 0.5f},
                .distance = 6.0f,
                .yawDegrees = 180.0f,
                .pitchDegrees = 0.0f,
            },
        });

    const auto addCube = [&](const char* name, const float z, const ri::math::Vec3 color) {
        ri::scene::AddPrimitiveNode(
            scene,
            ri::scene::PrimitiveNodeOptions{
                .nodeName = name,
                .primitive = ri::scene::PrimitiveType::Cube,
                .transform = {
                    .position = {0.0f, 0.0f, z},
                    .scale = {2.5f, 2.5f, 0.5f},
                },
                .shadingModel = ri::scene::ShadingModel::Unlit,
                .baseColor = color,
                .opacity = 0.5f,
                .transparent = true,
            });
    };

    if (createNearFirst) {
        addCube("NearRed", 0.0f, {1.0f, 0.05f, 0.05f});
        addCube("FarBlue", 1.0f, {0.05f, 0.05f, 1.0f});
    } else {
        addCube("FarBlue", 1.0f, {0.05f, 0.05f, 1.0f});
        addCube("NearRed", 0.0f, {1.0f, 0.05f, 0.05f});
    }

    ri::render::software::ScenePreviewOptions options{};
    options.width = 96;
    options.height = 96;
    options.clearTop = {0.0f, 0.0f, 0.0f};
    options.clearBottom = {0.0f, 0.0f, 0.0f};
    options.fogStrength = 0.0f;
    options.previewContrast = 1.0f;
    options.previewSaturation = 1.0f;
    return ri::render::software::RenderScenePreview(scene, camera.cameraNode, options);
}

bool ChannelGradeMatchesReference() {
    ri::scene::Scene scene{"ChannelGrade"};
    const ri::scene::OrbitCameraHandles camera = ri::scene::AddOrbitCamera(scene, {});
    ri::render::software::ScenePreviewOptions options{};
    options.width = 64;
    options.height = 64;
    options.clearTop = {0.37f, 0.42f, 0.81f};
    options.clearBottom = options.clearTop;
    options.orderedDither = false;
    options.previewExposure = 1.08f;
    options.previewContrast = 1.08f;
    options.previewSaturation = 1.0f;
    const ri::render::software::SoftwareImage image =
        ri::render::software::RenderScenePreview(scene, camera.cameraNode, options);
    if (image.pixels.size() < 3U) {
        return false;
    }

    const auto reference = [&](const float input) {
        const std::uint8_t encoded = static_cast<std::uint8_t>(
            std::clamp(input, 0.0f, 1.0f) * 255.0f + 0.5f);
        float channel = std::clamp(static_cast<float>(encoded) / 255.0f * options.previewExposure, 0.0f, 1.0f);
        channel = std::clamp((channel - 0.5f) * options.previewContrast + 0.5f, 0.0f, 1.0f);
        return static_cast<std::uint8_t>(std::lround(channel * 255.0f));
    };
    return image.pixels[0U] == reference(options.clearTop.x)
        && image.pixels[1U] == reference(options.clearTop.y)
        && image.pixels[2U] == reference(options.clearTop.z);
}

} // namespace

int main() {
    if (!ChannelGradeMatchesReference()) {
        return EXIT_FAILURE;
    }
    const ri::render::software::SoftwareImage nearFirst = RenderLayeredCubes(true);
    const ri::render::software::SoftwareImage farFirst = RenderLayeredCubes(false);
    const ri::core::ImageComparisonResult orderComparison = ri::core::CompareImages(
        {
            .width = nearFirst.width,
            .height = nearFirst.height,
            .channelCount = 3U,
            .pixels = nearFirst.pixels,
        },
        {
            .width = farFirst.width,
            .height = farFirst.height,
            .channelCount = 3U,
            .pixels = farFirst.pixels,
        },
        {
            .perChannelTolerance = 0U,
            .maximumMeanAbsoluteError = 0.0,
            .maximumRootMeanSquareError = 0.0,
            .maximumOutlierFraction = 0.0,
        });
    if (!orderComparison.comparable || !orderComparison.matched || nearFirst.pixels.empty()) {
        return EXIT_FAILURE;
    }

    const std::size_t center = static_cast<std::size_t>(
        ((nearFirst.height / 2) * nearFirst.width + (nearFirst.width / 2)) * 3);
    if (center + 2U >= nearFirst.pixels.size()) {
        return EXIT_FAILURE;
    }
    const auto red = nearFirst.pixels[center + 0U];
    const auto blue = nearFirst.pixels[center + 2U];
    if (red < 40U || blue < 20U || std::max(red, blue) < 80U) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
