#include "RawIron/Render/PreviewTexture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <fstream>
#include <vector>

namespace ri::render::software {

RgbaImage LoadRgbaImageFile(const std::filesystem::path& path) {
    RgbaImage out{};
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        return out;
    }

    const std::streamsize size = stream.tellg();
    if (size <= 0) {
        return out;
    }

    stream.seekg(0, std::ios::beg);
    std::vector<stbi_uc> buffer(static_cast<std::size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return out;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        buffer.data(),
        static_cast<int>(buffer.size()),
        &width,
        &height,
        &channels,
        4);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        stbi_image_free(pixels);
        return out;
    }

    out.width = width;
    out.height = height;
    out.rgba.assign(pixels, pixels + static_cast<std::size_t>(width * height * 4));
    stbi_image_free(pixels);
    return out;
}

} // namespace ri::render::software
