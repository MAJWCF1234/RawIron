#include "RawIron/DataSchema/StringPattern.h"

#include <regex>
#include <string>

namespace ri::validate {

ValidationReport ValidateRegexMatch(std::string_view text,
                                    const std::string& pattern,
                                    std::string valuePath,
                                    std::string patternPath) {
    ValidationReport report;
    std::regex re;
    try {
        re.assign(pattern, std::regex_constants::ECMAScript);
    } catch (const std::regex_error&) {
        report.Add(IssueCode::ConstraintViolation, std::move(patternPath), "invalid regex pattern");
        return report;
    }
    const std::string textOwned(text);
    if (!std::regex_match(textOwned, re)) {
        report.Add(IssueCode::ConstraintViolation, std::move(valuePath), "value does not match pattern");
    }
    return report;
}

ValidationReport ValidateEachObjectKeyMatchesPattern(const std::string_view objectPath,
                                                     const std::vector<std::string_view>& keys,
                                                     const std::string& pattern,
                                                     std::string patternPath) {
    ValidationReport report;
    std::regex re;
    try {
        re.assign(pattern, std::regex_constants::ECMAScript);
    } catch (const std::regex_error&) {
        report.Add(IssueCode::ConstraintViolation, std::move(patternPath), "invalid regex pattern");
        return report;
    }
    std::string base(objectPath);
    if (base.empty()) {
        base = "/";
    }
    for (const std::string_view key : keys) {
        if (!std::regex_match(std::string(key), re)) {
            std::string pathOut = base;
            if (pathOut.back() != '/') {
                pathOut.push_back('/');
            }
            pathOut.append("@key");
            report.AddWithContext(IssueCode::ConstraintViolation,
                                  std::move(pathOut),
                                  "object key does not match pattern",
                                  "key",
                                  std::string(key));
        }
    }
    return report;
}

} // namespace ri::validate
