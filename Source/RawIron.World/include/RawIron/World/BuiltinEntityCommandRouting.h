#pragma once

#include "RawIron/Logic/LogicTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ri::logic {
class LogicGraph;
}

namespace ri::world {

class RuntimeEnvironmentService;

struct BuiltinEntityCommand {
    /// Well-known: `set_hidden`, `set_visible`, `set_light_intensity`, `logic_input` (forwards to world-actor logic).
    std::string verb;
    std::string targetId;
    std::unordered_map<std::string, std::string> args;
};

struct BuiltinEntityCommandResult {
    bool ok = false;
    std::string message;
};

/// Small built-in command surface for level scripts and tools. Safe when targets are missing: returns `ok == false` and
/// a diagnostic \ref BuiltinEntityCommandResult::message. Optional \p logLines accumulates a trace for host UIs.
[[nodiscard]] BuiltinEntityCommandResult DispatchBuiltinEntityCommand(RuntimeEnvironmentService& environment,
                                                                      ri::logic::LogicGraph& graph,
                                                                      const BuiltinEntityCommand& command,
                                                                      std::vector<std::string>* logLines = nullptr);

[[nodiscard]] std::string EntityPresentationHiddenFlagKey(std::string_view targetId);

} // namespace ri::world
