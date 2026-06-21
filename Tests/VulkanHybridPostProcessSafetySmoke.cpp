#include "RawIron/Render/VulkanScenePreviewBridge.h"

#include <array>
#include <cstdlib>

int main() {
    std::array<float, 4> tuning{{0.0f, 0.0f, 0.0f, 1.0f}};
    ri::render::vulkan::ApplyHybridHdrPresentationSafety(tuning, 2);
    if (tuning[0] != 0.0f) {
        return EXIT_FAILURE;
    }
    if (tuning[3] > 0.58f) {
        return EXIT_FAILURE;
    }

    std::array<float, 4> authoredCas{{0.12f, 0.0f, 0.0f, 0.0f}};
    ri::render::vulkan::ApplyHybridHdrPresentationSafety(authoredCas, 2);
    if (authoredCas[0] < 0.12f || authoredCas[3] > 0.58f) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
