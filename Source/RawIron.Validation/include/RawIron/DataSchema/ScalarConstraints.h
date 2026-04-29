#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <cstdint>
#include <string>

namespace ri::validate {

[[nodiscard]] ValidationReport ValidateDoubleInRange(double value,
                                                     double minInclusive,
                                                     double maxInclusive,
                                                     std::string path);

[[nodiscard]] ValidationReport ValidateInt32InRange(std::int32_t value,
                                                    std::int32_t minInclusive,
                                                    std::int32_t maxInclusive,
                                                    std::string path);

} // namespace ri::validate
