#include "RawIron/Render/VulkanFrameScheduling.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

#define RI_REQUIRE(condition)                                                              \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            std::fprintf(stderr, "REQUIRE failed at line %d: %s\n", __LINE__, #condition); \
            return false;                                                                  \
        }                                                                                  \
    } while (false)

using ri::render::vulkan::ShouldRenderVulkanNativeSceneFrame;

bool TestContinuousModeAlwaysRenders() {
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(0U, false, false, 0U));
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(0U, false, true, 0U));
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(73U, false, true, 73U));
    return true;
}

bool TestEqualNonzeroSequenceMaySkip() {
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(9U, true, false, 9U));
    RI_REQUIRE(!ShouldRenderVulkanNativeSceneFrame(9U, true, true, 9U));
    RI_REQUIRE(!ShouldRenderVulkanNativeSceneFrame(0U, true, true, 0U));
    return true;
}

bool TestChangedNonzeroSequenceRenders() {
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(10U, true, true, 9U));
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(4U, true, true, 10U));
    return true;
}

bool TestSequenceWrapAndDistinctDecreaseRender() {
    constexpr std::uint64_t kMaximum = std::numeric_limits<std::uint64_t>::max();
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(kMaximum, true, true, kMaximum - 1U));
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(0U, true, true, kMaximum));
    RI_REQUIRE(ShouldRenderVulkanNativeSceneFrame(1U, true, true, 500U));
    RI_REQUIRE(!ShouldRenderVulkanNativeSceneFrame(1U, true, true, 1U));
    return true;
}

} // namespace

int main() {
    if (!TestContinuousModeAlwaysRenders()
        || !TestEqualNonzeroSequenceMaySkip()
        || !TestChangedNonzeroSequenceRenders()
        || !TestSequenceWrapAndDistinctDecreaseRender()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
