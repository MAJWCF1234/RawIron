#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ri::render::software {

struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba{};

    [[nodiscard]] bool Valid() const noexcept {
        return width > 0 && height > 0 && rgba.size() == static_cast<std::size_t>(width * height * 4);
    }
};

[[nodiscard]] RgbaImage LoadRgbaImageFile(const std::filesystem::path& path);

} // namespace ri::render::software
