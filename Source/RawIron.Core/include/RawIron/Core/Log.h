#pragma once

#include <string_view>

namespace ri::core {

void LogInfo(std::string_view message);
void LogSection(std::string_view title);

} // namespace ri::core
