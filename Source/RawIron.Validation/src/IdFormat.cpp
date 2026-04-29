#include "RawIron/DataSchema/IdFormat.h"

#include <cctype>
#include <string>

namespace ri::validate {

namespace {

bool IsHexDigit(char ch) {
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

} // namespace

ValidationReport ValidateHexString(std::string_view text, std::size_t numNibbles, std::string path) {
    ValidationReport report;
    if (numNibbles == 0U) {
        if (text.empty()) {
            return report;
        }
        report.Add(IssueCode::ConstraintViolation, std::move(path), "expected empty string for zero-length hex");
        return report;
    }
    if (numNibbles % 2U != 0U) {
        report.Add(IssueCode::ConstraintViolation, path, "hex length must be an even nibble count");
        return report;
    }
    if (text.size() != numNibbles) {
        report.AddWithContext(IssueCode::ConstraintViolation,
                              std::move(path),
                              "hex string length mismatch",
                              "length",
                              std::to_string(text.size()));
        return report;
    }
    for (char ch : text) {
        if (!IsHexDigit(ch)) {
            report.Add(IssueCode::ConstraintViolation, path, "non-hexadecimal character in hash");
            return report;
        }
    }
    return report;
}

ValidationReport ValidateUuidString(std::string_view text, std::string path) {
    ValidationReport report;
    if (text.size() != 36U) {
        report.Add(IssueCode::ConstraintViolation, std::move(path), "UUID string must be 36 characters");
        return report;
    }
    constexpr std::size_t kHyphenPositions[4U] = {8U, 13U, 18U, 23U};
    for (std::size_t i = 0; i < 36U; ++i) {
        const bool isHyphen =
            (i == kHyphenPositions[0] || i == kHyphenPositions[1] || i == kHyphenPositions[2]
             || i == kHyphenPositions[3]);
        if (isHyphen) {
            if (text[i] != '-') {
                report.Add(IssueCode::ConstraintViolation, path, "UUID hyphens must be at fixed positions");
                return report;
            }
        } else if (!IsHexDigit(text[i])) {
            report.Add(IssueCode::ConstraintViolation, path, "UUID contains non-hex digit");
            return report;
        }
    }
    return report;
}

} // namespace ri::validate
