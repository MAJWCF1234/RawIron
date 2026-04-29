#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::runtime {

struct ExperiencePresetPatch {
    std::string gameId;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<std::string> windowTitle;
    std::optional<std::filesystem::path> defaultHeadlessOutputRelativePath;
};

[[nodiscard]] std::optional<ExperiencePresetPatch> ResolveExperiencePreset(std::string_view presetId);
[[nodiscard]] std::vector<std::string_view> SupportedExperiencePresets();

} // namespace ri::runtime
