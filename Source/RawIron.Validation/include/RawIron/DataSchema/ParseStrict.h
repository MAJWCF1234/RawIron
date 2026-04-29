#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <stdexcept>
#include <string>
#include <string_view>

namespace ri::validate {

/// Strict parse: unwrap value or throw with report summary (for sim / hot paths).
template <typename T>
T UnwrapOrThrow(SafeParseResult<T> result, std::string_view contextLabel = {}) {
    if (result.value.has_value()) {
        return std::move(*result.value);
    }
    std::string message;
    if (!contextLabel.empty()) {
        message.assign(contextLabel);
        message += ": ";
    }
    message += result.report.SummaryLine();
    throw std::runtime_error(message);
}

} // namespace ri::validate
