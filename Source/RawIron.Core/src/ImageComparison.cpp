#include "RawIron/Core/ImageComparison.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace ri::core {
namespace {

[[nodiscard]] bool TryGetRequiredByteCount(
    const ImageView8& image,
    std::size_t* pixelCount,
    std::size_t* byteCount) {
    if (pixelCount == nullptr || byteCount == nullptr
        || image.width <= 0 || image.height <= 0
        || image.channelCount == 0U || image.channelCount > 4U) {
        return false;
    }
    const std::size_t width = static_cast<std::size_t>(image.width);
    const std::size_t height = static_cast<std::size_t>(image.height);
    if (width > (std::numeric_limits<std::size_t>::max)() / height) {
        return false;
    }
    *pixelCount = width * height;
    if (*pixelCount > (std::numeric_limits<std::size_t>::max)() / image.channelCount) {
        return false;
    }
    *byteCount = *pixelCount * image.channelCount;
    return image.pixels.size() == *byteCount;
}

[[nodiscard]] bool ValidOptions(const ImageComparisonOptions& options) {
    return std::isfinite(options.maximumMeanAbsoluteError)
        && std::isfinite(options.maximumRootMeanSquareError)
        && std::isfinite(options.maximumOutlierFraction)
        && options.maximumMeanAbsoluteError >= 0.0
        && options.maximumRootMeanSquareError >= 0.0
        && options.maximumOutlierFraction >= 0.0
        && options.maximumOutlierFraction <= 1.0;
}

} // namespace

ImageComparisonResult CompareImages(
    const ImageView8& expected,
    const ImageView8& actual,
    const ImageComparisonOptions& options) {
    ImageComparisonResult result{};
    if (!ValidOptions(options)) {
        result.diagnostic = "Image comparison thresholds are invalid.";
        return result;
    }
    if (expected.width != actual.width || expected.height != actual.height
        || expected.channelCount != actual.channelCount) {
        result.diagnostic = "Image dimensions or channel counts do not match.";
        return result;
    }

    std::size_t expectedPixels = 0U;
    std::size_t expectedBytes = 0U;
    std::size_t actualPixels = 0U;
    std::size_t actualBytes = 0U;
    if (!TryGetRequiredByteCount(expected, &expectedPixels, &expectedBytes)
        || !TryGetRequiredByteCount(actual, &actualPixels, &actualBytes)
        || expectedPixels != actualPixels || expectedBytes != actualBytes) {
        result.diagnostic = "Image pixel storage is malformed or overflows its dimensions.";
        return result;
    }

    const std::size_t channelsToCompare =
        options.ignoreAlpha && expected.channelCount == 4U ? 3U : expected.channelCount;
    const std::size_t sampleCount = expectedPixels * channelsToCompare;
    long double absoluteErrorSum = 0.0L;
    long double squaredErrorSum = 0.0L;
    for (std::size_t pixel = 0U; pixel < expectedPixels; ++pixel) {
        const std::size_t offset = pixel * expected.channelCount;
        bool outlier = false;
        for (std::size_t channel = 0U; channel < channelsToCompare; ++channel) {
            const int difference = std::abs(
                static_cast<int>(expected.pixels[offset + channel])
                - static_cast<int>(actual.pixels[offset + channel]));
            result.maximumChannelError = std::max(
                result.maximumChannelError,
                static_cast<std::uint8_t>(difference));
            absoluteErrorSum += static_cast<long double>(difference);
            squaredErrorSum += static_cast<long double>(difference * difference);
            outlier = outlier || difference > static_cast<int>(options.perChannelTolerance);
        }
        if (outlier) {
            ++result.outlierPixelCount;
        }
    }

    result.comparable = true;
    result.comparedPixelCount = expectedPixels;
    result.comparedChannelCount = channelsToCompare;
    result.meanAbsoluteError = static_cast<double>(
        absoluteErrorSum / static_cast<long double>(sampleCount));
    result.rootMeanSquareError = std::sqrt(static_cast<double>(
        squaredErrorSum / static_cast<long double>(sampleCount)));
    result.outlierFraction = static_cast<double>(result.outlierPixelCount)
        / static_cast<double>(expectedPixels);
    result.matched = result.meanAbsoluteError <= options.maximumMeanAbsoluteError
        && result.rootMeanSquareError <= options.maximumRootMeanSquareError
        && result.outlierFraction <= options.maximumOutlierFraction;

    std::ostringstream diagnostic;
    diagnostic << (result.matched ? "Images match" : "Images differ")
        << ": MAE=" << std::fixed << std::setprecision(3) << result.meanAbsoluteError
        << ", RMSE=" << result.rootMeanSquareError
        << ", max=" << static_cast<unsigned int>(result.maximumChannelError)
        << ", outliers=" << result.outlierPixelCount << '/' << result.comparedPixelCount << '.';
    result.diagnostic = diagnostic.str();
    return result;
}

} // namespace ri::core
