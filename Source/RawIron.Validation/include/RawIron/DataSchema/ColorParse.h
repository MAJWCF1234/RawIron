#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace ri::validate {

/// Parses `#rgb`, `#rrggbb`, or `#rrggbbaa` (hex digits, case-insensitive). Alpha defaults to 255 when omitted.
[[nodiscard]] SafeParseResult<std::array<std::uint8_t, 4U>> TryParseColorRgba8(std::string_view text);

} // namespace ri::validate
