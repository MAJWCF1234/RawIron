#pragma once

#include <algorithm>

namespace ri::ui {

struct UiImageUvRect {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

struct UiStageSize {
    float width = 0.0f;
    float height = 0.0f;
};

/// Center-crops an image to cover a display region while preserving its aspect ratio.
[[nodiscard]] inline UiImageUvRect ComputeCoverImageUv(float imageWidth,
                                                       float imageHeight,
                                                       float displayWidth,
                                                       float displayHeight) noexcept {
    if (!(imageWidth > 0.0f) || !(imageHeight > 0.0f)
        || !(displayWidth > 0.0f) || !(displayHeight > 0.0f)) {
        return {};
    }
    const float imageAspect = imageWidth / imageHeight;
    const float displayAspect = displayWidth / displayHeight;
    if (imageAspect > displayAspect) {
        const float visible = std::clamp(displayAspect / imageAspect, 0.0f, 1.0f);
        const float margin = (1.0f - visible) * 0.5f;
        return {.u0 = margin, .v0 = 0.0f, .u1 = 1.0f - margin, .v1 = 1.0f};
    }
    const float visible = std::clamp(imageAspect / displayAspect, 0.0f, 1.0f);
    const float margin = (1.0f - visible) * 0.5f;
    return {.u0 = 0.0f, .v0 = margin, .u1 = 1.0f, .v1 = 1.0f - margin};
}

/// Responsive menu/VN stage sizing that never exceeds the available window.
[[nodiscard]] inline UiStageSize ComputeResponsiveUiStageSize(float availableWidth,
                                                               float availableHeight) noexcept {
    availableWidth = std::max(1.0f, availableWidth);
    availableHeight = std::max(1.0f, availableHeight);
    const float widthLimit = std::max(1.0f, availableWidth - 24.0f);
    const float heightLimit = std::max(1.0f, availableHeight - 36.0f);
    const float preferredWidth = std::clamp(availableWidth - 96.0f, 280.0f, 900.0f);
    const float preferredHeight = std::clamp(availableHeight * 0.78f, 220.0f, 760.0f);
    return {
        .width = std::min(preferredWidth, widthLimit),
        .height = std::min(preferredHeight, heightLimit),
    };
}

} // namespace ri::ui
