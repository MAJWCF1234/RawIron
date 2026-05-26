#pragma once

#include "RawIron/Logic/LogicAuthoring.h"
#include "RawIron/Logic/LogicPortSchema.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ri::logic::detail {

struct PortResolution {
    bool recognized = false;
    bool normalized = false;
    std::string canonical;
};

[[nodiscard]] std::string NodeDefinitionId(const LogicNodeDefinition& definition);

void AddIssue(std::vector<LogicAuthoringCompileIssue>& issues,
              LogicAuthoringIssueSeverity severity,
              std::string code,
              std::string message,
              std::string subjectId = {});

[[nodiscard]] PortResolution ResolvePortName(
    const LogicNodePortSchema& schema, std::string_view rawPortName, bool inputPort);

[[nodiscard]] std::uint32_t ClampRouteDelayMs(std::uint32_t authoredDelayMs);

[[nodiscard]] LogicNodeDefinition NormalizeNodeDefinition(const LogicNodeDefinition& definition,
                                                          std::vector<LogicAuthoringCompileIssue>& issues);

} // namespace ri::logic::detail
