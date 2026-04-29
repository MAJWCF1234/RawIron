#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::validate {

enum class IssueCode : std::uint16_t {
    Ok = 0,
    TypeMismatch = 1,
    MissingField = 2,
    UnknownKey = 3,
    ConstraintViolation = 4,
    InvalidEnum = 5,
    InvalidReference = 6,
    VersionMismatch = 7,
    CoercionRejected = 8,
    NoUnionMatch = 9,
};

[[nodiscard]] const char* IssueCodeLabel(IssueCode code) noexcept;

struct ValidationIssue {
    std::string path;
    IssueCode code = IssueCode::Ok;
    std::string message;
    std::string contextKey;
    std::string contextValue;
    /// Optional machine-readable tags for tools (e.g. Zod-style `expected` / `received`).
    std::string expectedType;
    std::string actualType;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;

    [[nodiscard]] bool Ok() const { return issues.empty(); }

    void Add(IssueCode code, std::string path, std::string message);
    void AddWithContext(IssueCode code,
                        std::string path,
                        std::string message,
                        std::string contextKey,
                        std::string contextValue);
    void AddTypeMismatch(std::string path,
                         std::string expectedType,
                         std::string actualType,
                         std::string message = {});
    void Merge(const ValidationReport& other);
    void Merge(ValidationReport&& other);
    /// Prefix each imported issue path (e.g. child at `/components/Health` under parent `/entities/3`).
    void AbsorbPrefixed(std::string_view pathPrefix, const ValidationReport& child);
    void Clear() { issues.clear(); }
    /// Single-line summary for logs; stable enough for grep (includes codes and paths).
    [[nodiscard]] std::string SummaryLine() const;
    /// One JSON array of objects `{code,path,message,contextKey?,contextValue?}` for tools / telemetry.
    [[nodiscard]] std::string IssuesToJsonArray() const;
};

[[nodiscard]] ValidationReport MergeReports(std::initializer_list<const ValidationReport*> reports);

template <typename T>
struct SafeParseResult {
    std::optional<T> value;
    ValidationReport report;

    [[nodiscard]] explicit operator bool() const { return value.has_value(); }
};

} // namespace ri::validate
