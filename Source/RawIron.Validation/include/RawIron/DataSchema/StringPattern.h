#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <string>
#include <string_view>
#include <vector>

namespace ri::validate {

/// ECMA-style regex match on `text`. Invalid `pattern` produces a constraint issue on `patternPath`.
[[nodiscard]] ValidationReport ValidateRegexMatch(std::string_view text,
                                                  const std::string& pattern,
                                                  std::string valuePath,
                                                  std::string patternPath = "/schema/pattern");

/// Runs `regex_match` for each key (e.g. map/object property names). Issues use `objectPath + "/@key"` with context `key`.
[[nodiscard]] ValidationReport ValidateEachObjectKeyMatchesPattern(std::string_view objectPath,
                                                                  const std::vector<std::string_view>& keys,
                                                                  const std::string& pattern,
                                                                  std::string patternPath = "/schema/pattern");

} // namespace ri::validate
