#pragma once

#include <string>
#include <string_view>

namespace ri::validate {

/// Deterministic interchange paths: normalize separators to `separator` (default '/').
[[nodiscard]] std::string NormalizePathSeparators(std::string_view path, char separator = '/');

} // namespace ri::validate
