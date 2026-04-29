#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <string>
#include <string_view>
#include <vector>

namespace ri::validate {

/// Fails if any string appears more than once (first duplicate index reported).
[[nodiscard]] ValidationReport ValidateDistinctStrings(std::string_view pathPrefix,
                                                       const std::vector<std::string_view>& values);

} // namespace ri::validate
