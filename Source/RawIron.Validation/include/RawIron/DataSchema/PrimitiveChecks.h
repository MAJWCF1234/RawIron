#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <cmath>
#include <string>

namespace ri::validate {

[[nodiscard]] inline bool IsFiniteNumber(double value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] inline ValidationReport RequireFiniteDouble(double value, std::string path) {
    ValidationReport report;
    if (!std::isfinite(value)) {
        report.Add(IssueCode::ConstraintViolation, std::move(path), "value must be finite");
    }
    return report;
}

} // namespace ri::validate
