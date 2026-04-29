#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ri::validate {

/// Each id must appear in `allowed` (document-local table). Emits \ref IssueCode::InvalidReference per miss.
[[nodiscard]] ValidationReport ValidateIdsInTable(std::string_view pathPrefix,
                                                  const std::vector<std::string>& ids,
                                                  const std::unordered_set<std::string>& allowed);

} // namespace ri::validate
