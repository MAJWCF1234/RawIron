#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace ri::validate {

[[nodiscard]] ValidationReport ValidateStringLength(std::string_view text,
                                                    std::size_t minChars,
                                                    std::size_t maxChars,
                                                    std::string path);

/// ASCII letters, digits, underscore; must start with letter or `_`.
[[nodiscard]] ValidationReport ValidateAsciiIdentifier(std::string_view text, std::string path);

/// Fixed subset: `YYYY-MM-DDThh:mm:ssZ` or same with fractional seconds and `Z`.
[[nodiscard]] ValidationReport ValidateIso8601UtcTimestampString(std::string_view text, std::string path);

} // namespace ri::validate
