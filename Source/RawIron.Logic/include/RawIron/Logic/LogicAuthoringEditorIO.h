#pragma once

#include "RawIron/Logic/LogicAuthoring.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ri::logic {

/// One placed logic block from `logic_authoring.ri_logic` (editor persistence format v1).
struct LogicAuthoringEditorNodeRecord {
    std::string logicNodeId;
    std::string kitId;
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    bool executable = true;
};

struct LogicAuthoringEditorWireRecord {
    std::string wireId;
    std::string sourceLogicId;
    std::string outputName;
    std::string targetLogicId;
    std::string inputName;
};

struct LogicAuthoringEditorTriggerRecord {
    std::string triggerId;
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
};

struct LogicAuthoringEditorFile {
    bool creatorVisible = true;
    bool playerHidden = false;
    std::vector<LogicAuthoringEditorNodeRecord> nodes;
    std::vector<LogicAuthoringEditorTriggerRecord> triggers;
    std::vector<LogicAuthoringEditorWireRecord> wires;
};

[[nodiscard]] std::optional<LogicAuthoringEditorFile> LoadLogicAuthoringEditorFile(
    const std::filesystem::path& path);

[[nodiscard]] LogicAuthoringGraph BuildLogicAuthoringGraphFromEditorFile(
    const LogicAuthoringEditorFile& file);

[[nodiscard]] LogicAuthoringCompileOptions BuildCompileOptionsFromEditorFile(
    const LogicAuthoringEditorFile& file);

} // namespace ri::logic
