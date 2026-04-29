#pragma once

#include "RawIron/Math/Vec3.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace ri::render::software {

struct SoftwareImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

struct CubePreviewOptions {
    int width = 512;
    int height = 512;
    ri::math::Vec3 clearTop{0.08f, 0.10f, 0.14f};
    ri::math::Vec3 clearBottom{0.19f, 0.23f, 0.30f};
    ri::math::Vec3 cubeColor{0.86f, 0.55f, 0.24f};
    ri::math::Vec3 stageColor{0.19f, 0.25f, 0.33f};
    ri::math::Vec3 fogColor{0.17f, 0.20f, 0.25f};
    float yawDegrees = 36.0f;
    float pitchDegrees = -28.0f;
    float rollDegrees = 12.0f;
    float fieldOfViewDegrees = 50.0f;
};

[[nodiscard]] SoftwareImage RenderShadedCubePreview(const CubePreviewOptions& options = {});
bool SaveBmp(const SoftwareImage& image, std::string_view outputPath);

} // namespace ri::render::software
