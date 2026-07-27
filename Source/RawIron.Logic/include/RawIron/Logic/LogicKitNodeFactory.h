#pragma once

#include "RawIron/Logic/LogicTypes.h"

#include <optional>
#include <string>
#include <string_view>

namespace ri::logic {

/// Result of attempting to instantiate executable logic from a LogicKit manifest `id`.
struct LogicKitNodeFactoryResult {
    LogicNodeDefinition definition{};
    bool executable = true;
    std::string canonicalKitId;
};

/// Maps LogicKit `id` strings (and legacy aliases) to runtime \ref LogicNodeDefinition nodes.
/// Returns empty when the kit id is unknown. Supported authoring sense kits are
/// executed by LogicAuthoringSenseRuntime rather than being silently discarded.
[[nodiscard]] std::optional<LogicKitNodeFactoryResult> CreateLogicNodeFromKitId(std::string_view kitId,
                                                                                std::string nodeInstanceId);

[[nodiscard]] bool LogicKitIdIsExecutable(std::string_view kitId);

[[nodiscard]] std::string_view LogicKitIdCanonical(std::string_view kitId);

} // namespace ri::logic
