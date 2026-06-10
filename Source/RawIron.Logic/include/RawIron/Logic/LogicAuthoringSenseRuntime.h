#pragma once

#include "RawIron/Logic/LogicAuthoringEditorIO.h"
#include "RawIron/Logic/LogicGraph.h"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ri::logic {

struct LogicAuthoringSenseRuntimeState {
    std::unordered_map<std::string, bool> edgeBoolByKey{};
    std::unordered_map<std::string, std::uint64_t> lastEmitMsByKey{};
    /// `sense_zone` Arm/Clr and `sense_overlap` En/Rst (default enabled when unset).
    std::unordered_map<std::string, bool> enabledByLogicNodeId{};
    /// `sense_tag` Tag input (defaults to `player` when unset).
    std::unordered_map<std::string, std::string> tagFilterByLogicNodeId{};
    /// `sense_scalar` Key input (defaults to `distance` when unset).
    std::unordered_map<std::string, std::string> scalarKeyByLogicNodeId{};
    /// Set when Poll input is wired or pulsed; gates auto-interval emission until Poll fires.
    std::unordered_map<std::string, bool> pollModeByLogicNodeId{};
    /// One-shot Poll request consumed on the next in-range tick.
    std::unordered_map<std::string, bool> pollPendingByLogicNodeId{};
};

struct LogicAuthoringSenseProbeRecord {
    std::string logicNodeId;
    std::string kitId;
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
};

struct LogicSenseRaycastRequest {
    std::array<float, 3> origin{0.0f, 0.0f, 0.0f};
    std::array<float, 3> direction{0.0f, 0.0f, 1.0f};
    float maxDistance = 18.0f;
};

struct LogicSenseRaycastHit {
    float distance = 0.0f;
};

struct LogicAuthoringSenseRuntimeOptions {
    std::function<std::optional<LogicSenseRaycastHit>(const LogicSenseRaycastRequest&)> raycast{};
    std::string_view probeInstigatorTag = "player";
    float lineOfSightClearance = 0.25f;
};

void BindLogicSenseInputDispatchHandler(
    LogicGraph& graph,
    LogicAuthoringSenseRuntimeState& state,
    const std::unordered_map<std::string, std::string>& kitIdByLogicNodeId);

void TickLogicAuthoringSenseNodes(LogicGraph& graph,
                                  const std::vector<LogicAuthoringSenseProbeRecord>& probes,
                                  const std::array<float, 3>& probeWorldPosition,
                                  LogicAuthoringSenseRuntimeState& state,
                                  const LogicAuthoringSenseRuntimeOptions* options = nullptr);

void TickLogicAuthoringSenseNodes(LogicGraph& graph,
                                  const LogicAuthoringEditorFile& file,
                                  const std::array<float, 3>& probeWorldPosition,
                                  LogicAuthoringSenseRuntimeState& state,
                                  const LogicAuthoringSenseRuntimeOptions* options = nullptr);

} // namespace ri::logic
