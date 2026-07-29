#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace ri::core {

/// Non-owning tightly packed 8-bit image view used by renderer regression tests and capture tools.
struct ImageView8 {
    int width = 0;
    int height = 0;
    std::size_t channelCount = 0;
    std::span<const std::uint8_t> pixels{};
};

struct ImageComparisonOptions {
    /// A pixel is an outlier when any compared channel differs by more than this amount.
    std::uint8_t perChannelTolerance = 2U;
    double maximumMeanAbsoluteError = 1.0;
    double maximumRootMeanSquareError = 2.0;
    /// Fraction of pixels allowed to exceed `perChannelTolerance`.
    double maximumOutlierFraction = 0.001;
    /// Ignore channel four for RGBA/BGRA captures. RGB ordering must still match between inputs.
    bool ignoreAlpha = true;
};

struct ImageComparisonResult {
    bool comparable = false;
    bool matched = false;
    std::size_t comparedPixelCount = 0;
    std::size_t comparedChannelCount = 0;
    std::size_t outlierPixelCount = 0;
    std::uint8_t maximumChannelError = 0U;
    double meanAbsoluteError = 0.0;
    double rootMeanSquareError = 0.0;
    double outlierFraction = 0.0;
    std::string diagnostic;
};

/// Compares equal-sized tightly packed images without assuming a renderer or file format.
/// Malformed views and invalid thresholds produce a non-comparable result rather than reading
/// outside either pixel buffer.
[[nodiscard]] ImageComparisonResult CompareImages(
    const ImageView8& expected,
    const ImageView8& actual,
    const ImageComparisonOptions& options = {});

} // namespace ri::core
