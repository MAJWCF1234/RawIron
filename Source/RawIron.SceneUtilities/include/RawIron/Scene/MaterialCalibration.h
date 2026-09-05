#pragma once

#include "RawIron/Scene/Scene.h"

#include <string>

namespace ri::scene {

// Hosts supply authored paths or mounted package IDs; this builder does no file I/O.
struct MaterialCalibrationTextures {
    std::string srgbAlbedo;
    std::string linearNormal;
    std::string linearNormalDirectX;
};

struct MaterialCalibrationScene {
    Scene scene{"Material Calibration"};
    int root = kInvalidHandle;
    int floor = kInvalidHandle;
    int cameraRig = kInvalidHandle;
    int camera = kInvalidHandle;
};

// Fixed, static fixtures for color, texture decoding, normals, PBR, blend and depth.
// Throws invalid_argument for empty texture IDs instead of creating an untextured control.
[[nodiscard]] MaterialCalibrationScene BuildMaterialCalibrationScene(
    const MaterialCalibrationTextures& textures);

// Neutral pairs: OpenGL, converted DirectX, deliberately unconverted DirectX.
// Second row mirrors U to expose tangent-frame handedness. Caller owns placement.
[[nodiscard]] int AddNormalMappingComparisonPanels(Scene& scene,
    const MaterialCalibrationTextures& textures, int parent = kInvalidHandle);

[[nodiscard]] MaterialCalibrationScene BuildNormalMappingComparisonScene(
    const MaterialCalibrationTextures& textures);

} // namespace ri::scene
