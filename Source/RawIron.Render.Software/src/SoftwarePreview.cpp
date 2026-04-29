#include "RawIron/Render/SoftwarePreview.h"

#include "RawIron/Math/Mat4.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace ri::render::software {

namespace {

struct Vertex {
    ri::math::Vec3 position{};
};

struct Triangle {
    int a = 0;
    int b = 0;
    int c = 0;
};

struct Quad {
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
};

struct ShadedTriangle {
    ri::math::Vec3 a{};
    ri::math::Vec3 b{};
    ri::math::Vec3 c{};
    ri::math::Vec3 baseColor{};
    float specularStrength = 0.0f;
    float shininess = 16.0f;
};

struct ScreenVertex {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
};

constexpr std::array<Vertex, 8> kCubeVertices = {{
    {{-0.5f, -0.5f, -0.5f}},
    {{ 0.5f, -0.5f, -0.5f}},
    {{ 0.5f,  0.5f, -0.5f}},
    {{-0.5f,  0.5f, -0.5f}},
    {{-0.5f, -0.5f,  0.5f}},
    {{ 0.5f, -0.5f,  0.5f}},
    {{ 0.5f,  0.5f,  0.5f}},
    {{-0.5f,  0.5f,  0.5f}},
}};

constexpr std::array<Quad, 6> kCubeFaces = {{
    {4, 5, 6, 7},
    {1, 0, 3, 2},
    {0, 4, 7, 3},
    {5, 1, 2, 6},
    {3, 7, 6, 2},
    {0, 1, 5, 4},
}};

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

std::uint8_t ToByte(float value) {
    return static_cast<std::uint8_t>(std::lround(Clamp01(value) * 255.0f));
}

ri::math::Vec3 ClampColor(const ri::math::Vec3& color) {
    return ri::math::Vec3{
        Clamp01(color.x),
        Clamp01(color.y),
        Clamp01(color.z),
    };
}

ri::math::Vec3 MultiplyColor(const ri::math::Vec3& lhs, const ri::math::Vec3& rhs) {
    return ClampColor(ri::math::Vec3{
        lhs.x * rhs.x,
        lhs.y * rhs.y,
        lhs.z * rhs.z,
    });
}

void SetPixel(SoftwareImage& image, int x, int y, const ri::math::Vec3& color) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        return;
    }

    const std::size_t offset = static_cast<std::size_t>((y * image.width + x) * 3);
    const ri::math::Vec3 clamped = ClampColor(color);
    image.pixels[offset + 0] = ToByte(clamped.x);
    image.pixels[offset + 1] = ToByte(clamped.y);
    image.pixels[offset + 2] = ToByte(clamped.z);
}

void FillGradientBackground(SoftwareImage& image, const CubePreviewOptions& options) {
    for (int y = 0; y < image.height; ++y) {
        const float t = image.height > 1 ? static_cast<float>(y) / static_cast<float>(image.height - 1) : 0.0f;
        const ri::math::Vec3 rowColor = ri::math::Lerp(options.clearTop, options.clearBottom, t);
        for (int x = 0; x < image.width; ++x) {
            SetPixel(image, x, y, rowColor);
        }
    }
}

void DrawShadow(SoftwareImage& image) {
    const float centerX = static_cast<float>(image.width) * 0.5f;
    const float centerY = static_cast<float>(image.height) * 0.73f;
    const float radiusX = static_cast<float>(image.width) * 0.18f;
    const float radiusY = static_cast<float>(image.height) * 0.06f;

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const float nx = (static_cast<float>(x) - centerX) / radiusX;
            const float ny = (static_cast<float>(y) - centerY) / radiusY;
            const float distanceSquared = nx * nx + ny * ny;
            if (distanceSquared >= 1.0f) {
                continue;
            }

            const float alpha = (1.0f - distanceSquared) * 0.18f;
            const std::size_t offset = static_cast<std::size_t>((y * image.width + x) * 3);
            for (int channel = 0; channel < 3; ++channel) {
                const float source = static_cast<float>(image.pixels[offset + channel]) / 255.0f;
                const float mixed = source * (1.0f - alpha);
                image.pixels[offset + channel] = ToByte(mixed);
            }
        }
    }
}

ri::math::Vec3 ApplyFog(const ri::math::Vec3& color, float depth, const CubePreviewOptions& options) {
    const float fogFactor = Clamp01((depth - 2.8f) / 4.8f);
    const float weightedFog = fogFactor * fogFactor * 0.55f;
    return ClampColor(ri::math::Lerp(color, options.fogColor, weightedFog));
}

ri::math::Vec3 ShadeFace(const ri::math::Vec3& baseColor,
                         const ri::math::Vec3& normal,
                         const ri::math::Vec3& faceCenter,
                         float specularStrength,
                         float shininess,
                         const CubePreviewOptions& options) {
    const ri::math::Vec3 viewDirection = ri::math::Normalize(faceCenter * -1.0f);
    const ri::math::Vec3 keyLightDirection = ri::math::Normalize(ri::math::Vec3{0.45f, 0.75f, -1.00f});
    const ri::math::Vec3 fillLightDirection = ri::math::Normalize(ri::math::Vec3{-0.85f, 0.25f, -0.35f});
    const ri::math::Vec3 ambientLight{0.14f, 0.15f, 0.18f};
    const ri::math::Vec3 keyLightColor{1.00f, 0.91f, 0.78f};
    const ri::math::Vec3 fillLightColor{0.30f, 0.42f, 0.62f};
    const ri::math::Vec3 rimLightColor{0.52f, 0.76f, 1.00f};
    const ri::math::Vec3 specularColor{1.00f, 0.96f, 0.88f};

    const float key = std::max(0.0f, ri::math::Dot(normal, keyLightDirection));
    const float fill = std::max(0.0f, ri::math::Dot(normal, fillLightDirection));
    const float sky = Clamp01((normal.y + 1.0f) * 0.5f);
    const float groundBounce = Clamp01(-normal.y) * 0.08f;
    const float fresnel = std::pow(Clamp01(1.0f - std::max(0.0f, ri::math::Dot(normal, viewDirection))), 3.5f);
    const ri::math::Vec3 halfVector = ri::math::Normalize(keyLightDirection + viewDirection);
    const float specular = std::pow(std::max(0.0f, ri::math::Dot(normal, halfVector)), shininess) * specularStrength;

    ri::math::Vec3 lightAccumulator = ambientLight;
    lightAccumulator = lightAccumulator + (keyLightColor * (key * 0.78f));
    lightAccumulator = lightAccumulator + (fillLightColor * (fill * 0.30f));
    lightAccumulator = lightAccumulator + (ri::math::Vec3{0.10f, 0.12f, 0.14f} * (sky * 0.22f));
    lightAccumulator = lightAccumulator + (options.stageColor * groundBounce);

    ri::math::Vec3 shaded = MultiplyColor(baseColor, lightAccumulator);
    shaded = shaded + (rimLightColor * (fresnel * 0.22f));
    shaded = shaded + (specularColor * specular);
    return ClampColor(shaded);
}

float EdgeFunction(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

ScreenVertex ProjectPoint(const ri::math::Vec3& position, int width, int height, float focalLength, float aspectRatio) {
    const float ndcX = (position.x / position.z) * (focalLength / aspectRatio);
    const float ndcY = (position.y / position.z) * focalLength;

    return ScreenVertex{
        .x = (ndcX * 0.5f + 0.5f) * static_cast<float>(width - 1),
        .y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(height - 1),
        .depth = position.z,
    };
}

void RasterizeTriangle(SoftwareImage& image,
                       std::vector<float>& depthBuffer,
                       const CubePreviewOptions& options,
                       const ShadedTriangle& triangle,
                       float focalLength,
                       float aspectRatio,
                       const ri::math::Vec3* overrideColor = nullptr) {
    if (triangle.a.z <= 0.05f || triangle.b.z <= 0.05f || triangle.c.z <= 0.05f) {
        return;
    }

    const ri::math::Vec3 faceNormal = ri::math::Normalize(ri::math::Cross(triangle.b - triangle.a, triangle.c - triangle.a));
    const ri::math::Vec3 faceCenter = (triangle.a + triangle.b + triangle.c) / 3.0f;
    if (ri::math::Dot(faceNormal, faceCenter) >= 0.0f) {
        return;
    }

    const ScreenVertex screenA = ProjectPoint(triangle.a, options.width, options.height, focalLength, aspectRatio);
    const ScreenVertex screenB = ProjectPoint(triangle.b, options.width, options.height, focalLength, aspectRatio);
    const ScreenVertex screenC = ProjectPoint(triangle.c, options.width, options.height, focalLength, aspectRatio);

    const float area = EdgeFunction(screenA.x, screenA.y, screenB.x, screenB.y, screenC.x, screenC.y);
    if (std::fabs(area) <= 0.0001f) {
        return;
    }

    const ri::math::Vec3 litColor = overrideColor == nullptr
        ? ShadeFace(
              triangle.baseColor,
              faceNormal,
              faceCenter,
              triangle.specularStrength,
              triangle.shininess,
              options)
        : *overrideColor;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({screenA.x, screenB.x, screenC.x}))));
    const int maxX = std::min(options.width - 1, static_cast<int>(std::ceil(std::max({screenA.x, screenB.x, screenC.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({screenA.y, screenB.y, screenC.y}))));
    const int maxY = std::min(options.height - 1, static_cast<int>(std::ceil(std::max({screenA.y, screenB.y, screenC.y}))));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;

            const float w0 = EdgeFunction(screenB.x, screenB.y, screenC.x, screenC.y, px, py) / area;
            const float w1 = EdgeFunction(screenC.x, screenC.y, screenA.x, screenA.y, px, py) / area;
            const float w2 = EdgeFunction(screenA.x, screenA.y, screenB.x, screenB.y, px, py) / area;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }

            const float depth = w0 * screenA.depth + w1 * screenB.depth + w2 * screenC.depth;
            const std::size_t pixelIndex = static_cast<std::size_t>(y * options.width + x);
            if (depth >= depthBuffer[pixelIndex]) {
                continue;
            }

            depthBuffer[pixelIndex] = depth;
            SetPixel(image, x, y, ApplyFog(litColor, depth, options));
        }
    }
}

void RasterizeQuad(SoftwareImage& image,
                   std::vector<float>& depthBuffer,
                   const CubePreviewOptions& options,
                   const ri::math::Vec3& a,
                   const ri::math::Vec3& b,
                   const ri::math::Vec3& c,
                   const ri::math::Vec3& d,
                   const ri::math::Vec3& baseColor,
                   float specularStrength,
                   float shininess,
                   float focalLength,
                   float aspectRatio) {
    const ri::math::Vec3 faceNormal = ri::math::Normalize(ri::math::Cross(b - a, c - a));
    const ri::math::Vec3 faceCenter = (a + b + c + d) / 4.0f;
    const ri::math::Vec3 litColor = ShadeFace(baseColor, faceNormal, faceCenter, specularStrength, shininess, options);

    RasterizeTriangle(
        image,
        depthBuffer,
        options,
        ShadedTriangle{a, b, c, baseColor, specularStrength, shininess},
        focalLength,
        aspectRatio,
        &litColor);
    RasterizeTriangle(
        image,
        depthBuffer,
        options,
        ShadedTriangle{a, c, d, baseColor, specularStrength, shininess},
        focalLength,
        aspectRatio,
        &litColor);
}

void WriteU16(std::ofstream& stream, std::uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
    };
    stream.write(bytes, sizeof(bytes));
}

void WriteU32(std::ofstream& stream, std::uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff),
    };
    stream.write(bytes, sizeof(bytes));
}

} // namespace

SoftwareImage RenderShadedCubePreview(const CubePreviewOptions& rawOptions) {
    CubePreviewOptions options = rawOptions;
    options.width = std::clamp(options.width, 64, 2048);
    options.height = std::clamp(options.height, 64, 2048);
    options.fieldOfViewDegrees = std::clamp(options.fieldOfViewDegrees, 20.0f, 120.0f);

    SoftwareImage image{};
    image.width = options.width;
    image.height = options.height;
    image.pixels.resize(static_cast<std::size_t>(options.width * options.height * 3), 0);

    FillGradientBackground(image, options);
    DrawShadow(image);

    const float aspectRatio = static_cast<float>(options.width) / static_cast<float>(options.height);
    const float focalLength = 1.0f / std::tan(ri::math::DegreesToRadians(options.fieldOfViewDegrees * 0.5f));
    const ri::math::Mat4 world = ri::math::TRS(
        ri::math::Vec3{0.0f, 0.10f, 3.6f},
        ri::math::Vec3{options.pitchDegrees, options.yawDegrees, options.rollDegrees},
        ri::math::Vec3{1.65f, 1.65f, 1.65f});

    std::vector<float> depthBuffer(static_cast<std::size_t>(options.width * options.height),
                                   std::numeric_limits<float>::max());

    RasterizeQuad(
        image,
        depthBuffer,
        options,
        ri::math::Vec3{-3.4f, -1.35f, 2.2f},
        ri::math::Vec3{-4.2f, -1.35f, 8.6f},
        ri::math::Vec3{ 4.2f, -1.35f, 8.6f},
        ri::math::Vec3{ 3.4f, -1.35f, 2.2f},
        options.stageColor,
        0.04f,
        10.0f,
        focalLength,
        aspectRatio);

    for (const Quad& face : kCubeFaces) {
        RasterizeQuad(
            image,
            depthBuffer,
            options,
            ri::math::TransformPoint(world, kCubeVertices[static_cast<std::size_t>(face.a)].position),
            ri::math::TransformPoint(world, kCubeVertices[static_cast<std::size_t>(face.b)].position),
            ri::math::TransformPoint(world, kCubeVertices[static_cast<std::size_t>(face.c)].position),
            ri::math::TransformPoint(world, kCubeVertices[static_cast<std::size_t>(face.d)].position),
            options.cubeColor,
            0.24f,
            28.0f,
            focalLength,
            aspectRatio);
    }

    return image;
}

bool SaveBmp(const SoftwareImage& image, std::string_view outputPath) {
    if (image.width <= 0 || image.height <= 0 ||
        image.pixels.size() != static_cast<std::size_t>(image.width * image.height * 3)) {
        return false;
    }

    std::ofstream stream(std::string(outputPath), std::ios::binary);
    if (!stream) {
        return false;
    }

    const std::uint32_t rowStride = static_cast<std::uint32_t>(image.width * 3);
    const std::uint32_t rowPadding = (4U - (rowStride % 4U)) % 4U;
    const std::uint32_t pixelDataSize = (rowStride + rowPadding) * static_cast<std::uint32_t>(image.height);
    const std::uint32_t fileSize = 14U + 40U + pixelDataSize;

    stream.put('B');
    stream.put('M');
    WriteU32(stream, fileSize);
    WriteU16(stream, 0);
    WriteU16(stream, 0);
    WriteU32(stream, 54U);

    WriteU32(stream, 40U);
    WriteU32(stream, static_cast<std::uint32_t>(image.width));
    WriteU32(stream, static_cast<std::uint32_t>(image.height));
    WriteU16(stream, 1U);
    WriteU16(stream, 24U);
    WriteU32(stream, 0U);
    WriteU32(stream, pixelDataSize);
    WriteU32(stream, 2835U);
    WriteU32(stream, 2835U);
    WriteU32(stream, 0U);
    WriteU32(stream, 0U);

    const std::array<char, 3> padding = {0, 0, 0};
    for (int y = image.height - 1; y >= 0; --y) {
        for (int x = 0; x < image.width; ++x) {
            const std::size_t offset = static_cast<std::size_t>((y * image.width + x) * 3);
            stream.put(static_cast<char>(image.pixels[offset + 2]));
            stream.put(static_cast<char>(image.pixels[offset + 1]));
            stream.put(static_cast<char>(image.pixels[offset + 0]));
        }
        stream.write(padding.data(), static_cast<std::streamsize>(rowPadding));
    }

    return stream.good();
}

} // namespace ri::render::software
