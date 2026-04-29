#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <initializer_list>
#include <string>
#include <string_view>

namespace ri::validate {

[[nodiscard]] ValidationReport ValidateAllowedString(std::string_view value,
                                                     std::initializer_list<std::string_view> allowed,
                                                     std::string path);

} // namespace ri::validate
