#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <functional>
#include <initializer_list>
#include <vector>

namespace ri::validate {

using RefinementFn = std::function<void(ValidationReport&)>;

/// Runs pure cross-field checks after structural validation; each refinement appends to the same report.
void RunRefinements(std::initializer_list<RefinementFn> refinements, ValidationReport& report);

} // namespace ri::validate
