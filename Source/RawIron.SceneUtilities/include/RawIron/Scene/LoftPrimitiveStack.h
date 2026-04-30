#pragma once

#include "RawIron/Math/Vec2.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Scene/Components.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace ri::scene {

[[nodiscard]] std::vector<ri::math::Vec2> GetPrimitiveProfile2dPreset(std::string_view presetName);

[[nodiscard]] std::vector<ri::math::Vec2> SanitizePrimitiveProfile2dLoop(
    const std::vector<ri::math::Vec2>& authoredPoints,
    std::string_view fallbackPreset = "square",
    std::size_t maxPoints = 64,
    float maxAbsCoordinate = 1.0f);

[[nodiscard]] std::vector<ri::math::Vec3> GetPrimitiveProfilePreset(std::string_view presetName);

[[nodiscard]] std::vector<ri::math::Vec3> ResamplePrimitiveProfileLoop(const std::vector<ri::math::Vec3>& profileLoop,
                                                                       std::size_t sampleCount);

[[nodiscard]] float GetLoftInterpolationValue(float t);

[[nodiscard]] Mesh BuildLoftPrimitiveGeometryFromProfiles(const std::vector<ri::math::Vec3>& startProfileLoop,
                                                          const std::vector<ri::math::Vec3>& endProfileLoop,
                                                          std::size_t depthSegments,
                                                          bool capEnds);

} // namespace ri::scene
