#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace ri::validate {

/// `numNibbles` hex digits (must be even). Empty text fails unless `numNibbles == 0`.
[[nodiscard]] ValidationReport ValidateHexString(std::string_view text,
                                                   std::size_t numNibbles,
                                                   std::string path);

/// RFC 4122 string form: 8-4-4-4-12 lowercase or uppercase hex with hyphens.
[[nodiscard]] ValidationReport ValidateUuidString(std::string_view text, std::string path);

} // namespace ri::validate
