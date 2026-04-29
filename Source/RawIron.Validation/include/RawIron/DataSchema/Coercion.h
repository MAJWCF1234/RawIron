#pragma once

#include "RawIron/DataSchema/ValidationReport.h"

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace ri::validate {

[[nodiscard]] inline SafeParseResult<double> TryCoerceDouble(std::string_view text, bool allowEmpty = false) {
    SafeParseResult<double> out{};
    if (text.empty()) {
        if (allowEmpty) {
            out.value = std::nullopt;
            return out;
        }
        out.report.Add(IssueCode::CoercionRejected, "", "expected number, got empty string");
        return out;
    }
    std::string buffer(text);
    char* endPtr = nullptr;
    const double parsed = std::strtod(buffer.c_str(), &endPtr);
    if (endPtr == buffer.c_str() || *endPtr != '\0') {
        out.report.Add(IssueCode::CoercionRejected, "", "string is not a finite decimal number");
        return out;
    }
    if (!std::isfinite(parsed)) {
        out.report.Add(IssueCode::ConstraintViolation, "", "number out of finite range");
        return out;
    }
    out.value = parsed;
    return out;
}

[[nodiscard]] inline SafeParseResult<std::int32_t> TryCoerceInt32(std::string_view text, bool allowEmpty = false) {
    SafeParseResult<std::int32_t> out{};
    if (text.empty()) {
        if (allowEmpty) {
            out.value = std::nullopt;
            return out;
        }
        out.report.Add(IssueCode::CoercionRejected, "", "expected integer, got empty string");
        return out;
    }
    std::string buffer(text);
    char* endPtr = nullptr;
    const long long parsed = std::strtoll(buffer.c_str(), &endPtr, 10);
    if (endPtr == buffer.c_str() || *endPtr != '\0') {
        out.report.Add(IssueCode::CoercionRejected, "", "string is not a base-10 integer");
        return out;
    }
    if (parsed < static_cast<long long>(std::numeric_limits<std::int32_t>::min())
        || parsed > static_cast<long long>(std::numeric_limits<std::int32_t>::max())) {
        out.report.Add(IssueCode::ConstraintViolation, "", "integer overflow for int32");
        return out;
    }
    out.value = static_cast<std::int32_t>(parsed);
    return out;
}

[[nodiscard]] inline SafeParseResult<bool> TryCoerceBool(std::string_view text, bool allowEmpty = false) {
    SafeParseResult<bool> out{};
    if (text.empty()) {
        if (allowEmpty) {
            out.value = std::nullopt;
            return out;
        }
        out.report.Add(IssueCode::CoercionRejected, "", "expected boolean, got empty string");
        return out;
    }
    std::string lower(text.size(), '\0');
    std::transform(text.begin(), text.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower == "true" || lower == "1" || lower == "yes") {
        out.value = true;
        return out;
    }
    if (lower == "false" || lower == "0" || lower == "no") {
        out.value = false;
        return out;
    }
    out.report.Add(IssueCode::CoercionRejected, "", "string is not a recognized boolean");
    return out;
}

} // namespace ri::validate
