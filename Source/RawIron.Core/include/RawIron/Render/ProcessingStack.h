#pragma once

#include "RawIron/Render/ShaderConfig.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ri::render {

enum class ProcessingStackBackend { Base, SerenityColor, Post };
std::vector<std::string> DiscoverProcessingStacks(const std::filesystem::path& libraryRoot);

struct ProcessingStack {
    std::string name = "base";
    ProcessingStackBackend backend = ProcessingStackBackend::Base;
    // Native Serenity uniform ABI.
    // [0] exposure multiplier, saturation, crosstalk, tone mapper
    // [1] upper/lower curve, dither, sharpening
    // [2-4] shadow / mid / highlight grade RGB + multiplier
    // [5] grading, curves, ACES approximation, sharpening enabled
    // [6] texel size xy + frame time from the renderer; w = stage flags
    //     (1 AUTO_EXPOSURE, 2 TAA, 4 VOLUMETRIC_CLOUDS, 8 Fake_purkinje, 16 TOGGLE_VL_FOG)
    // [7] BLOOM_STRENGTH, BLOOMY_FOG, Purkinje_strength, TAA BLEND_FACTOR
    // [8] Exposure_Speed, CLOUD_SHADOW_STRENGTH, WATER_REFRACTION, BLOOM_THRESHOLD
    std::array<std::array<float, 4>, 9> serenity{{
        {1, 0, 0, 0},
        {0, 0, 1, .35f},
        {1, 1, 1, 1},
        {1, 1, 1, 1},
        {1, 1, 1, 1},
        {0, 0, 0, 1},
        {0, 0, 0, 31},
        {1, 1.5f, 1, .12f},
        {1, 1, 1, 1}
    }};
    std::array<float, 4> exposureTuning{{1, 0, 0, 0}}; // Manual_exposure_value; reserved
    std::array<float, 4> purkinjeColor{{.4f, .7f, 1, 5}}; // source RGB and multiplier
    ShaderPresentationConfig post{};
};

// active.cfg selects a sibling <name>/stack.cfg. Reloads are transactional:
// invalid edits keep the last valid stack, and changing selection never merges
// the previous stack's values into the new one. Poll from the render thread.
class ProcessingStackReloader {
public:
    explicit ProcessingStackReloader(std::filesystem::path selectionFile = {}, std::string overrideName = {});
    bool Poll(double monotonicSeconds, std::string* error = nullptr);
    void Select(std::string name);
    const ProcessingStack& Current() const { return current_; }
    std::uint64_t Revision() const { return revision_; }
private:
    std::filesystem::path selectionFile_;
    std::string overrideName_;
    std::string previousContent_;
    std::string previousError_;
    ProcessingStack current_{};
    double nextPoll_ = 0;
    std::uint64_t revision_ = 0;
};

} // namespace ri::render
