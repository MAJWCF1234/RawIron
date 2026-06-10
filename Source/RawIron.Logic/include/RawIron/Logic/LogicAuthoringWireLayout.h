#pragma once

#include "RawIron/Logic/LogicAuthoringEditorIO.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::logic {

struct LogicEditorPortLayout {
    std::unordered_map<std::string, std::unordered_map<std::string, std::array<float, 3>>> inputPortsByActor{};
    std::unordered_map<std::string, std::unordered_map<std::string, std::array<float, 3>>> outputPortsByActor{};
};

[[nodiscard]] LogicEditorPortLayout BuildLogicEditorPortLayout(const LogicAuthoringEditorFile& file);

[[nodiscard]] std::optional<std::array<float, 3>> ResolveLogicEditorWireEndpoint(
    const LogicEditorPortLayout& layout,
    const LogicAuthoringEditorFile& file,
    std::string_view actorId,
    std::string_view portName,
    bool input);

[[nodiscard]] std::vector<std::array<float, 3>> BuildLogicWireBezierBeadPositions(
    const std::array<float, 3>& from,
    const std::array<float, 3>& to);

} // namespace ri::logic
