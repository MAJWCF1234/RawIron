#pragma once

#include <string>
#include <string_view>

namespace ri::runtime {

std::string SanitizeRuntimeIdPrefix(std::string_view prefix);
std::string CreateRuntimeId(std::string_view prefix = "rt");

} // namespace ri::runtime
