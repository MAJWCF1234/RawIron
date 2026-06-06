#include "ProceduralMaterialMaps.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ri::render::vulkan {

namespace {

[[nodiscard]] int WrapIndex(const int value, const int size, const bool wrap) {
    if (size <= 1) {
        return 0;
    }
    if (wrap) {
        int wrapped = value % size;
        if (wrapped < 0) {
            wrapped += size;
        }
        return wrapped;
    }
    return std::clamp(value, 0, size - 1);
}

[[nodiscard]] float SrgbToLinear(const float channel) {
    if (channel <= 0.04045f) {
        return channel / 12.92f;
    }
    return std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

/// Alpha (0..1) of a pixel, used to keep cutout cards from gaining ghost relief.
[[nodiscard]] float SampleAlpha(const RgbaImage& image, const int x, const int y, const bool wrap) {
    const int sx = WrapIndex(x, image.width, wrap);
    const int sy = WrapIndex(y, image.height, wrap);
    const std::size_t offset = (static_cast<std::size_t>(sy) * static_cast<std::size_t>(image.width)
                                + static_cast<std::size_t>(sx))
                               * 4U;
    return static_cast<float>(image.rgba[offset + 3]) / 255.0f;
}

/// Perceptual luminance from *linearised* sRGB, weighted by alpha so transparent
/// regions contribute no height. The standard 709 coefficients are only correct
/// in linear space, so the channels are decoded first.
[[nodiscard]] float SampleLinearLuminance(const RgbaImage& image, const int x, const int y, const bool wrap) {
    const int sx = WrapIndex(x, image.width, wrap);
    const int sy = WrapIndex(y, image.height, wrap);
    const std::size_t offset = (static_cast<std::size_t>(sy) * static_cast<std::size_t>(image.width)
                                + static_cast<std::size_t>(sx))
                               * 4U;
    const float r = SrgbToLinear(static_cast<float>(image.rgba[offset + 0]) / 255.0f);
    const float g = SrgbToLinear(static_cast<float>(image.rgba[offset + 1]) / 255.0f);
    const float b = SrgbToLinear(static_cast<float>(image.rgba[offset + 2]) / 255.0f);
    const float a = static_cast<float>(image.rgba[offset + 3]) / 255.0f;
    return ((0.2126f * r) + (0.7152f * g) + (0.0722f * b)) * a;
}

[[nodiscard]] std::uint8_t ToByte(const float value01) {
    const float scaled = std::clamp(value01, 0.0f, 1.0f) * 255.0f;
    return static_cast<std::uint8_t>(scaled + 0.5f);
}

/// Builds a (optionally smoothed) height field from the albedo linear luminance.
[[nodiscard]] std::vector<float> BuildHeightField(const RgbaImage& albedo, const float blur, const bool wrap) {
    const int width = albedo.width;
    const int height = albedo.height;
    std::vector<float> heights(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0.0f);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            heights[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                SampleLinearLuminance(albedo, x, y, wrap);
        }
    }
    const float blurAmount = std::clamp(blur, 0.0f, 1.0f);
    if (blurAmount <= 1e-4f || width < 3 || height < 3) {
        return heights;
    }

    std::vector<float> smoothed = heights;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int sx = WrapIndex(x + dx, width, wrap);
                    const int sy = WrapIndex(y + dy, height, wrap);
                    sum += heights[static_cast<std::size_t>(sy) * static_cast<std::size_t>(width)
                                   + static_cast<std::size_t>(sx)];
                }
            }
            const float average = sum / 9.0f;
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
            smoothed[index] = heights[index] + (average - heights[index]) * blurAmount;
        }
    }
    return smoothed;
}

[[nodiscard]] float HeightAt(const std::vector<float>& heights,
                             const int width,
                             const int height,
                             const int x,
                             const int y,
                             const bool wrap) {
    const int sx = WrapIndex(x, width, wrap);
    const int sy = WrapIndex(y, height, wrap);
    return heights[static_cast<std::size_t>(sy) * static_cast<std::size_t>(width) + static_cast<std::size_t>(sx)];
}

} // namespace

std::uint32_t ProceduralGeneratorBuildId() {
    // Manual, stable identity for the generation math. Bump the low bytes whenever the
    // algorithms below change so on-disk caches invalidate exactly once per real change
    // -- never on an unrelated recompile (which the old __DATE__/__TIME__ hash did, and
    // which forced needless regeneration/reupload of every procedural map).
    return 0x00010004U; // Material hydration generator v1.4 (alpha-aware normal-ORM, midpoint contrast).
}

RgbaImage GenerateNormalMapFromAlbedo(const RgbaImage& albedo, const ProceduralNormalMapOptions& options) {
    if (!albedo.Valid()) {
        return {};
    }

    const int width = albedo.width;
    const int height = albedo.height;
    std::vector<float> heights = BuildHeightField(albedo, options.blur, options.wrap);
    const float strength = std::max(options.strength, 0.0f);

    // Apply optional height inversion / bias / contrast before the Sobel pass. Contrast is
    // applied about the 0.5 midpoint so heightScale==1 / bias==0 is a true no-op and a
    // mid-gray stays mid-gray (the old form pushed 0.5 up to a plateau).
    if (options.invertHeight || options.heightBias != 0.0f || options.heightScale != 1.0f) {
        for (float& value : heights) {
            const float oriented = options.invertHeight ? (1.0f - value) : value;
            const float transformed = ((oriented - 0.5f) * options.heightScale) + 0.5f + options.heightBias;
            value = std::clamp(transformed, 0.0f, 1.0f);
        }
    }

    RgbaImage normal{};
    normal.width = width;
    normal.height = height;
    normal.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0U);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offsetEarly =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
            if (SampleAlpha(albedo, x, y, options.wrap) <= 0.01f) {
                normal.rgba[offsetEarly + 0] = 128U;
                normal.rgba[offsetEarly + 1] = 128U;
                normal.rgba[offsetEarly + 2] = 255U;
                normal.rgba[offsetEarly + 3] = 0U;
                continue;
            }
            // Sobel kernel over the height field.
            const float tl = HeightAt(heights, width, height, x - 1, y - 1, options.wrap);
            const float tc = HeightAt(heights, width, height, x, y - 1, options.wrap);
            const float tr = HeightAt(heights, width, height, x + 1, y - 1, options.wrap);
            const float ml = HeightAt(heights, width, height, x - 1, y, options.wrap);
            const float mr = HeightAt(heights, width, height, x + 1, y, options.wrap);
            const float bl = HeightAt(heights, width, height, x - 1, y + 1, options.wrap);
            const float bc = HeightAt(heights, width, height, x, y + 1, options.wrap);
            const float br = HeightAt(heights, width, height, x + 1, y + 1, options.wrap);

            const float dx = (tr + 2.0f * mr + br) - (tl + 2.0f * ml + bl);
            const float dy = (bl + 2.0f * bc + br) - (tl + 2.0f * tc + tr);

            float nx = -dx * strength;
            float ny = -dy * strength;
            float nz = 1.0f;
            const float length = std::sqrt((nx * nx) + (ny * ny) + (nz * nz));
            if (length > 1e-6f) {
                nx /= length;
                ny /= length;
                nz /= length;
            } else {
                nx = 0.0f;
                ny = 0.0f;
                nz = 1.0f;
            }

            const std::size_t offset =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
            normal.rgba[offset + 0] = ToByte(nx * 0.5f + 0.5f);
            normal.rgba[offset + 1] = ToByte(ny * 0.5f + 0.5f);
            normal.rgba[offset + 2] = ToByte(nz * 0.5f + 0.5f);
            normal.rgba[offset + 3] = 255U;
        }
    }
    return normal;
}

RgbaImage GenerateOrmMapFromAlbedo(const RgbaImage& albedo, const ProceduralOrmMapOptions& options) {
    if (!albedo.Valid()) {
        return {};
    }

    const int width = albedo.width;
    const int height = albedo.height;
    const bool wrap = options.wrap;
    const std::vector<float> heights = BuildHeightField(albedo, 0.0f, wrap);

    const float aoStrength = std::clamp(options.aoStrength, 0.0f, 1.0f);
    const float roughnessDetail = std::clamp(options.roughnessDetail, 0.0f, 1.0f);
    const float baseRoughness = std::clamp(options.baseRoughness, 0.0f, 1.0f);
    const float baseMetallic = std::clamp(options.baseMetallic, 0.0f, 1.0f);

    RgbaImage orm{};
    orm.width = width;
    orm.height = height;
    orm.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0U);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
            if (SampleAlpha(albedo, x, y, wrap) <= 0.01f) {
                orm.rgba[offset + 0] = 255U;
                orm.rgba[offset + 1] = ToByte(baseRoughness);
                orm.rgba[offset + 2] = ToByte(baseMetallic);
                orm.rgba[offset + 3] = 0U;
                continue;
            }

            const float local = HeightAt(heights, width, height, x, y, wrap);

            // Neighbourhood average drives both the cavity (occlusion) and the local
            // micro-contrast (roughness) terms, so roughness reflects real surface
            // detail rather than distance from a global mean.
            float neighbourSum = 0.0f;
            float varianceSum = 0.0f;
            int neighbourCount = 0;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    neighbourSum += HeightAt(heights, width, height, x + dx, y + dy, wrap);
                    ++neighbourCount;
                }
            }
            const float neighbourAvg = neighbourCount > 0 ? (neighbourSum / static_cast<float>(neighbourCount)) : local;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    const float delta = HeightAt(heights, width, height, x + dx, y + dy, wrap) - neighbourAvg;
                    varianceSum += delta * delta;
                }
            }
            const float localDetail = std::clamp(std::sqrt(varianceSum / 25.0f) * 4.0f, 0.0f, 1.0f);

            const float cavity = std::clamp(neighbourAvg - local, 0.0f, 1.0f);
            const float ao = std::clamp(1.0f - cavity * aoStrength * 1.6f, 0.0f, 1.0f);
            const float roughness = std::clamp(baseRoughness + (localDetail - 0.5f) * roughnessDetail, 0.04f, 1.0f);

            orm.rgba[offset + 0] = ToByte(ao);
            orm.rgba[offset + 1] = ToByte(roughness);
            orm.rgba[offset + 2] = ToByte(baseMetallic);
            orm.rgba[offset + 3] = 255U;
        }
    }
    return orm;
}

namespace {

/// Decodes a tangent-space normal sample (RGB in 0..255) into a unit-ish vector.
struct DecodedNormal {
    float x;
    float y;
    float z;
};

[[nodiscard]] DecodedNormal DecodeNormal(const RgbaImage& image, const int x, const int y, const bool wrap) {
    const int sx = WrapIndex(x, image.width, wrap);
    const int sy = WrapIndex(y, image.height, wrap);
    const std::size_t offset = (static_cast<std::size_t>(sy) * static_cast<std::size_t>(image.width)
                                + static_cast<std::size_t>(sx))
                               * 4U;
    DecodedNormal n{
        static_cast<float>(image.rgba[offset + 0]) / 255.0f * 2.0f - 1.0f,
        static_cast<float>(image.rgba[offset + 1]) / 255.0f * 2.0f - 1.0f,
        static_cast<float>(image.rgba[offset + 2]) / 255.0f * 2.0f - 1.0f,
    };
    const float lengthSq = (n.x * n.x) + (n.y * n.y) + (n.z * n.z);
    if (lengthSq > 1e-6f) {
        const float invLength = 1.0f / std::sqrt(lengthSq);
        n.x *= invLength;
        n.y *= invLength;
        n.z *= invLength;
    } else {
        n = DecodedNormal{0.0f, 0.0f, 1.0f};
    }
    return n;
}

} // namespace

RgbaImage GenerateOrmMapFromNormal(const RgbaImage& normal, const ProceduralOrmMapOptions& options) {
    if (!normal.Valid()) {
        return {};
    }

    const int width = normal.width;
    const int height = normal.height;
    const bool wrap = options.wrap;
    const float aoStrength = std::clamp(options.aoStrength, 0.0f, 1.0f);
    const float roughnessDetail = std::clamp(options.roughnessDetail, 0.0f, 1.0f);
    const float baseRoughness = std::clamp(options.baseRoughness, 0.0f, 1.0f);
    const float baseMetallic = std::clamp(options.baseMetallic, 0.0f, 1.0f);

    RgbaImage orm{};
    orm.width = width;
    orm.height = height;
    orm.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0U);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;

            // Respect coverage from the source normal map: transparent texels (including
            // those a generated normal marks with alpha 0 for cutouts) must not hydrate
            // invisible areas with relief-driven occlusion/roughness.
            if (normal.rgba[offset + 3] <= 2U) {
                orm.rgba[offset + 0] = 255U;
                orm.rgba[offset + 1] = ToByte(baseRoughness);
                orm.rgba[offset + 2] = ToByte(baseMetallic);
                orm.rgba[offset + 3] = 0U;
                continue;
            }

            const DecodedNormal center = DecodeNormal(normal, x, y, wrap);

            // Occlusion from local curvature: a pixel whose normal points away from the
            // averaged neighbourhood sits inside a concavity and should self-occlude.
            float sumX = 0.0f;
            float sumY = 0.0f;
            float sumZ = 0.0f;
            int neighbourCount = 0;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    // Authored normal maps may carry garbage RGB under transparent texels;
                    // treat a transparent neighbour as the centre normal so cutout edges do
                    // not fabricate curvature/occlusion from that hidden data.
                    const DecodedNormal sample = SampleAlpha(normal, x + dx, y + dy, wrap) <= 0.01f
                                                     ? center
                                                     : DecodeNormal(normal, x + dx, y + dy, wrap);
                    sumX += sample.x;
                    sumY += sample.y;
                    sumZ += sample.z;
                    ++neighbourCount;
                }
            }
            const float invCount = neighbourCount > 0 ? (1.0f / static_cast<float>(neighbourCount)) : 0.0f;
            const float avgX = sumX * invCount;
            const float avgY = sumY * invCount;
            const float avgZ = sumZ * invCount;
            // Deviation between the centre normal and the neighbourhood mean. High where
            // the surface bends (edges, grooves); near zero on flat areas.
            const float devX = center.x - avgX;
            const float devY = center.y - avgY;
            const float devZ = center.z - avgZ;
            const float curvature = std::sqrt((devX * devX) + (devY * devY) + (devZ * devZ));

            const float cavity = std::clamp(curvature * 2.2f, 0.0f, 1.0f);
            const float ao = std::clamp(1.0f - cavity * aoStrength * 1.4f, 0.0f, 1.0f);

            // Slope (how far the normal tilts from straight up) plus curvature detail make
            // bevels and rough relief read as higher roughness than flat faces.
            const float slope = std::clamp(1.0f - center.z, 0.0f, 1.0f);
            const float detail = std::clamp((slope * 0.65f) + (curvature * 1.6f), 0.0f, 1.0f);
            const float roughness = std::clamp(baseRoughness + (detail - 0.5f) * roughnessDetail, 0.04f, 1.0f);

            orm.rgba[offset + 0] = ToByte(ao);
            orm.rgba[offset + 1] = ToByte(roughness);
            orm.rgba[offset + 2] = ToByte(baseMetallic);
            orm.rgba[offset + 3] = 255U;
        }
    }
    return orm;
}

RgbaImage GenerateNeutralNormalMap(const int width, const int height) {
    RgbaImage normal{};
    if (width <= 0 || height <= 0) {
        return normal;
    }
    normal.width = width;
    normal.height = height;
    normal.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0U);
    for (std::size_t pixel = 0; pixel < normal.rgba.size(); pixel += 4U) {
        normal.rgba[pixel + 0] = 128U;
        normal.rgba[pixel + 1] = 128U;
        normal.rgba[pixel + 2] = 255U;
        normal.rgba[pixel + 3] = 255U;
    }
    return normal;
}

RgbaImage GenerateNeutralOrmMap(const int width, const int height, const float roughness, const float metallic) {
    RgbaImage orm{};
    if (width <= 0 || height <= 0) {
        return orm;
    }
    orm.width = width;
    orm.height = height;
    orm.rgba.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0U);
    // Clamp away mirror-smooth roughness: a neutral fallback must never read as a perfect
    // reflector, which produces fireflies/aliasing under specular lighting.
    const std::uint8_t roughnessByte = ToByte(std::clamp(roughness, 0.04f, 1.0f));
    const std::uint8_t metallicByte = ToByte(std::clamp(metallic, 0.0f, 1.0f));
    for (std::size_t pixel = 0; pixel < orm.rgba.size(); pixel += 4U) {
        orm.rgba[pixel + 0] = 255U;
        orm.rgba[pixel + 1] = roughnessByte;
        orm.rgba[pixel + 2] = metallicByte;
        orm.rgba[pixel + 3] = 255U;
    }
    return orm;
}

} // namespace ri::render::vulkan
