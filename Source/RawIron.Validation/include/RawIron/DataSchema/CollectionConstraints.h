#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <cstddef>
#include <string>

namespace ri::validate {

[[nodiscard]] ValidationReport ValidateCollectionSize(std::size_t count,
                                                      std::size_t minCount,
                                                      std::size_t maxCount,
                                                      std::string path);

} // namespace ri::validate
