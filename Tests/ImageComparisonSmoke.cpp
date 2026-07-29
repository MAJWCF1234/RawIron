#include "RawIron/Core/ImageComparison.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {

ri::core::ImageView8 View(
    const std::vector<std::uint8_t>& pixels,
    const int width,
    const int height,
    const std::size_t channels) {
    return {
        .width = width,
        .height = height,
        .channelCount = channels,
        .pixels = pixels,
    };
}

} // namespace

int main() {
    const std::vector<std::uint8_t> reference{
        10U, 20U, 30U, 255U,
        40U, 50U, 60U, 255U,
        70U, 80U, 90U, 255U,
        100U, 110U, 120U, 255U,
    };
    std::vector<std::uint8_t> close = reference;
    close[0] += 2U;
    close[7] = 0U; // Alpha is ignored by default.
    const ri::core::ImageComparisonResult tolerated =
        ri::core::CompareImages(View(reference, 2, 2, 4U), View(close, 2, 2, 4U));
    if (!tolerated.comparable || !tolerated.matched
        || tolerated.outlierPixelCount != 0U || tolerated.maximumChannelError != 2U) {
        return EXIT_FAILURE;
    }

    std::vector<std::uint8_t> oneOutlier = reference;
    oneOutlier[4] = 100U;
    const ri::core::ImageComparisonResult allowedOutlier = ri::core::CompareImages(
        View(reference, 2, 2, 4U),
        View(oneOutlier, 2, 2, 4U),
        {
            .perChannelTolerance = 2U,
            .maximumMeanAbsoluteError = 6.0,
            .maximumRootMeanSquareError = 18.0,
            .maximumOutlierFraction = 0.25,
        });
    if (!allowedOutlier.matched || allowedOutlier.outlierPixelCount != 1U
        || std::abs(allowedOutlier.outlierFraction - 0.25) > 0.0001) {
        return EXIT_FAILURE;
    }

    std::vector<std::uint8_t> widespread(reference.size(), 255U);
    const ri::core::ImageComparisonResult rejected =
        ri::core::CompareImages(View(reference, 2, 2, 4U), View(widespread, 2, 2, 4U));
    if (!rejected.comparable || rejected.matched || rejected.outlierPixelCount != 4U
        || rejected.diagnostic.find("Images differ") == std::string::npos) {
        return EXIT_FAILURE;
    }

    const std::vector<std::uint8_t> malformed{1U, 2U, 3U};
    if (ri::core::CompareImages(View(malformed, 2, 2, 3U), View(malformed, 2, 2, 3U)).comparable
        || ri::core::CompareImages(View(reference, 2, 2, 4U), View(reference, 4, 1, 4U)).comparable
        || ri::core::CompareImages(
               View(reference, 2, 2, 4U),
               View(reference, 2, 2, 4U),
               {.maximumMeanAbsoluteError = std::numeric_limits<double>::quiet_NaN()}).comparable) {
        return EXIT_FAILURE;
    }

    const ri::core::ImageComparisonResult alphaCompared = ri::core::CompareImages(
        View(reference, 2, 2, 4U),
        View(close, 2, 2, 4U),
        {
            .perChannelTolerance = 2U,
            .maximumMeanAbsoluteError = 1.0,
            .maximumRootMeanSquareError = 2.0,
            .maximumOutlierFraction = 0.0,
            .ignoreAlpha = false,
        });
    return alphaCompared.comparable && !alphaCompared.matched
        && alphaCompared.outlierPixelCount == 1U
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
