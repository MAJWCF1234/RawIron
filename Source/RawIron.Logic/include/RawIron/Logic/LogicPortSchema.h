#pragma once

#include "RawIron/Logic/LogicTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace ri::logic {

struct LogicPortDescriptor {
    std::string name;
    bool carriesNumericParameter = false;
};

struct LogicNodePortSchema {
    std::string kind;
    std::vector<LogicPortDescriptor> inputs;
    std::vector<LogicPortDescriptor> outputs;
};

/// Stable node kind string for editor labels/socket palettes.
[[nodiscard]] std::string_view GetLogicNodeKindName(const LogicNodeDefinition& definition);

/// Contract ports for a logic node kind (empty when unknown).
[[nodiscard]] LogicNodePortSchema GetLogicNodePortSchema(std::string_view kind);

/// Convenience overload for authored node definitions.
[[nodiscard]] LogicNodePortSchema GetLogicNodePortSchema(const LogicNodeDefinition& definition);

/// Port contract for map-layer world actor kinds (trigger_volume, spawner, door, etc.).
[[nodiscard]] LogicNodePortSchema GetWorldActorPortSchema(std::string_view actorKind);

} // namespace ri::logic
