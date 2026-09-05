#pragma once

#include "RawIron/XR/OpenXrRuntime.h"
#include "RawIron/Scene/Scene.h"
#include <array>
#include <unordered_map>

namespace ri::xr {
struct HardwareTextureAtlas {
    static constexpr int kSize = 2048;
    static constexpr int kGrid = 8;
    static constexpr int kCellSize = kSize / kGrid;
    std::vector<std::uint8_t> rgba = std::vector<std::uint8_t>(kSize*kSize*4, 255U);
    std::unordered_map<std::string, std::array<float,4>> rects{};
    std::size_t loadedTextures = 0;
    std::vector<std::string> errors{};
};
HardwareTextureAtlas BuildHardwareTextureAtlas(const ri::scene::Scene& scene);
void AppendHardwareMesh(std::vector<HardwareSceneVertex>& output, const ri::scene::Mesh& mesh,
    const ri::scene::Material& material, const ri::math::Mat4& world, const HardwareTextureAtlas& atlas);
// All authored rooms are resident; exclusions are the dynamic nodes supplied per frame.
std::vector<HardwareSceneVertex> BuildHardwareScene(const ri::scene::Scene& scene,
    const HardwareTextureAtlas& atlas, const std::vector<int>& excludedNodes = {});
} // namespace ri::xr
