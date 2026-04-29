#include "RawIron/DataSchema/Allowlist.h"
#include "RawIron/DataSchema/CollectionConstraints.h"
#include "RawIron/DataSchema/DocumentHeader.h"
#include "RawIron/DataSchema/Migration.h"
#include "RawIron/DataSchema/ObjectShape.h"
#include "RawIron/DataSchema/PathNormalize.h"
#include "RawIron/DataSchema/ReferenceIntegrity.h"
#include "RawIron/DataSchema/Refinement.h"
#include "RawIron/DataSchema/ScalarConstraints.h"
#include "RawIron/DataSchema/StringConstraints.h"
#include "RawIron/DataSchema/StringPattern.h"
#include "RawIron/DataSchema/ValidationReport.h"
#include "RawIron/DataSchema/SchemaRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <utility>

namespace ri::validate {

const char* IssueCodeLabel(IssueCode code) noexcept {
    switch (code) {
    case IssueCode::Ok:
        return "ok";
    case IssueCode::TypeMismatch:
        return "type_mismatch";
    case IssueCode::MissingField:
        return "missing_field";
    case IssueCode::UnknownKey:
        return "unknown_key";
    case IssueCode::ConstraintViolation:
        return "constraint_violation";
    case IssueCode::InvalidEnum:
        return "invalid_enum";
    case IssueCode::InvalidReference:
        return "invalid_reference";
    case IssueCode::VersionMismatch:
        return "version_mismatch";
    case IssueCode::CoercionRejected:
        return "coercion_rejected";
    case IssueCode::NoUnionMatch:
        return "no_union_match";
    }
    return "unknown";
}

void ValidationReport::Add(IssueCode code, std::string path, std::string message) {
    issues.push_back(ValidationIssue{
        .path = std::move(path),
        .code = code,
        .message = std::move(message),
        .contextKey = {},
        .contextValue = {},
        .expectedType = {},
        .actualType = {},
    });
}

void ValidationReport::AddWithContext(IssueCode code,
                                      std::string path,
                                      std::string message,
                                      std::string contextKey,
                                      std::string contextValue) {
    issues.push_back(ValidationIssue{
        .path = std::move(path),
        .code = code,
        .message = std::move(message),
        .contextKey = std::move(contextKey),
        .contextValue = std::move(contextValue),
        .expectedType = {},
        .actualType = {},
    });
}

void ValidationReport::AddTypeMismatch(std::string path,
                                       std::string expectedType,
                                       std::string actualType,
                                       std::string message) {
    if (message.empty()) {
        message = "type mismatch";
    }
    issues.push_back(ValidationIssue{
        .path = std::move(path),
        .code = IssueCode::TypeMismatch,
        .message = std::move(message),
        .contextKey = {},
        .contextValue = {},
        .expectedType = std::move(expectedType),
        .actualType = std::move(actualType),
    });
}

void ValidationReport::Merge(const ValidationReport& other) {
    issues.insert(issues.end(), other.issues.begin(), other.issues.end());
}

void ValidationReport::Merge(ValidationReport&& other) {
    if (issues.empty()) {
        issues = std::move(other.issues);
        return;
    }
    issues.reserve(issues.size() + other.issues.size());
    for (ValidationIssue& issue : other.issues) {
        issues.push_back(std::move(issue));
    }
    other.issues.clear();
}

void ValidationReport::AbsorbPrefixed(std::string_view pathPrefix, const ValidationReport& child) {
    for (const ValidationIssue& issue : child.issues) {
        std::string pathOut;
        if (pathPrefix.empty()) {
            pathOut = issue.path;
        } else if (issue.path.empty()) {
            pathOut = std::string(pathPrefix);
        } else {
            pathOut.assign(pathPrefix);
            if (pathOut.back() != '/' && issue.path.front() != '/') {
                pathOut.push_back('/');
            }
            pathOut += issue.path;
        }
        issues.push_back(ValidationIssue{
            .path = std::move(pathOut),
            .code = issue.code,
            .message = issue.message,
            .contextKey = issue.contextKey,
            .contextValue = issue.contextValue,
            .expectedType = issue.expectedType,
            .actualType = issue.actualType,
        });
    }
}

std::string ValidationReport::SummaryLine() const {
    if (issues.empty()) {
        return std::string("validation_ok");
    }
    std::ostringstream line;
    for (std::size_t i = 0; i < issues.size(); ++i) {
        if (i > 0) {
            line << "; ";
        }
        const ValidationIssue& issue = issues[i];
        line << IssueCodeLabel(issue.code) << '@' << issue.path << ": " << issue.message;
    }
    return line.str();
}

namespace {

std::string JsonEscape(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 8U);
    for (unsigned char uc : text) {
        const char ch = static_cast<char>(uc);
        switch (ch) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (uc < 0x20U) {
                static const char* kHex = "0123456789abcdef";
                out += "\\u00";
                out.push_back(kHex[(uc >> 4U) & 0x0FU]);
                out.push_back(kHex[uc & 0x0FU]);
            } else {
                out.push_back(ch);
            }
            break;
        }
    }
    return out;
}

} // namespace

std::string ValidationReport::IssuesToJsonArray() const {
    std::ostringstream json;
    json << '[';
    for (std::size_t i = 0; i < issues.size(); ++i) {
        if (i > 0) {
            json << ',';
        }
        const ValidationIssue& issue = issues[i];
        json << "{\"code\":\"" << IssueCodeLabel(issue.code) << "\",\"path\":\""
             << JsonEscape(issue.path) << "\",\"message\":\"" << JsonEscape(issue.message) << "\"";
        if (!issue.contextKey.empty()) {
            json << ",\"contextKey\":\"" << JsonEscape(issue.contextKey) << "\",\"contextValue\":\""
                 << JsonEscape(issue.contextValue) << "\"";
        }
        if (!issue.expectedType.empty()) {
            json << ",\"expectedType\":\"" << JsonEscape(issue.expectedType) << "\"";
        }
        if (!issue.actualType.empty()) {
            json << ",\"actualType\":\"" << JsonEscape(issue.actualType) << "\"";
        }
        json << '}';
    }
    json << ']';
    return json.str();
}

ValidationReport MergeReports(std::initializer_list<const ValidationReport*> reports) {
    ValidationReport merged;
    for (const ValidationReport* report : reports) {
        if (report != nullptr && !report->issues.empty()) {
            merged.Merge(*report);
        }
    }
    return merged;
}

std::string NormalizePathSeparators(std::string_view path, char separator) {
    std::string out;
    out.reserve(path.size());
    for (char ch : path) {
        if (ch == '\\' || ch == '/') {
            out.push_back(separator);
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

ValidationReport ValidateStringLength(std::string_view text,
                                    std::size_t minChars,
                                    std::size_t maxChars,
                                    std::string path) {
    ValidationReport report;
    if (minChars > maxChars) {
        report.Add(IssueCode::ConstraintViolation, std::move(path), "invalid string length bounds");
        return report;
    }
    if (text.size() < minChars || text.size() > maxChars) {
        report.AddWithContext(IssueCode::ConstraintViolation,
                              std::move(path),
                              "string length out of range",
                              "length",
                              std::to_string(text.size()));
    }
    return report;
}

ValidationReport ValidateAsciiIdentifier(std::string_view text, std::string path) {
    ValidationReport report;
    if (text.empty()) {
        report.Add(IssueCode::ConstraintViolation, path, "identifier must be non-empty");
        return report;
    }
    const unsigned char first = static_cast<unsigned char>(text.front());
    if (std::isalpha(first) == 0 && first != '_') {
        report.Add(IssueCode::ConstraintViolation, path, "identifier has invalid first character");
        return report;
    }
    for (std::size_t i = 1; i < text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (std::isalnum(ch) == 0 && ch != '_') {
            report.Add(IssueCode::ConstraintViolation, path, "identifier contains invalid character");
            return report;
        }
    }
    return report;
}

ValidationReport ValidateIso8601UtcTimestampString(std::string_view text, std::string path) {
    return ValidateRegexMatch(text,
                              R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?Z$)",
                              std::move(path));
}

ValidationReport ValidateDoubleInRange(double value,
                                       double minInclusive,
                                       double maxInclusive,
                                       std::string path) {
    ValidationReport report;
    if (minInclusive > maxInclusive) {
        report.Add(IssueCode::ConstraintViolation, std::move(path), "invalid numeric range bounds");
        return report;
    }
    if (!std::isfinite(value) || value < minInclusive || value > maxInclusive) {
        report.Add(IssueCode::ConstraintViolation, std::move(path), "double out of allowed range");
    }
    return report;
}

ValidationReport ValidateInt32InRange(std::int32_t value,
                                      std::int32_t minInclusive,
                                      std::int32_t maxInclusive,
                                      std::string path) {
    ValidationReport report;
    if (minInclusive > maxInclusive) {
        report.Add(IssueCode::ConstraintViolation, std::move(path), "invalid integer range bounds");
        return report;
    }
    if (value < minInclusive || value > maxInclusive) {
        report.Add(IssueCode::ConstraintViolation, std::move(path), "integer out of allowed range");
    }
    return report;
}

ValidationReport ValidateAllowedString(std::string_view value,
                                       std::initializer_list<std::string_view> allowed,
                                       std::string path) {
    ValidationReport report;
    for (std::string_view candidate : allowed) {
        if (candidate == value) {
            return report;
        }
    }
    report.Add(IssueCode::InvalidEnum, std::move(path), "value not in allowed enumeration");
    return report;
}

ValidationReport ValidateCollectionSize(std::size_t count,
                                        std::size_t minCount,
                                        std::size_t maxCount,
                                        std::string path) {
    ValidationReport report;
    if (minCount > maxCount) {
        report.Add(IssueCode::ConstraintViolation, std::move(path), "invalid collection size bounds");
        return report;
    }
    if (count < minCount || count > maxCount) {
        report.AddWithContext(IssueCode::ConstraintViolation,
                              std::move(path),
                              "collection size out of range",
                              "count",
                              std::to_string(count));
    }
    return report;
}

void RunRefinements(std::initializer_list<RefinementFn> refinements, ValidationReport& report) {
    for (const RefinementFn& fn : refinements) {
        if (fn) {
            fn(report);
        }
    }
}

ValidationReport ValidateIdsInTable(std::string_view pathPrefix,
                                    const std::vector<std::string>& ids,
                                    const std::unordered_set<std::string>& allowed) {
    ValidationReport report;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (allowed.find(ids[i]) != allowed.end()) {
            continue;
        }
        std::string path;
        path.reserve(pathPrefix.size() + 24U);
        path.append(pathPrefix);
        path.push_back('[');
        path += std::to_string(i);
        path.push_back(']');
        report.Add(IssueCode::InvalidReference, std::move(path), "id not in document table");
    }
    return report;
}

} // namespace ri::validate

namespace ri::data::schema {

ri::validate::SafeParseResult<DocumentHeader> ParseDocumentHeader(std::string_view kind,
                                                                  std::string_view versionDotSeparated) {
    ri::validate::SafeParseResult<DocumentHeader> result{};
    const auto parseUint32Token = [](std::string_view token, std::uint32_t& value) {
        if (token.empty()) {
            return false;
        }
        for (const char ch : token) {
            if (ch < '0' || ch > '9') {
                return false;
            }
        }
        const std::string owned(token);
        const unsigned long parsed = std::strtoul(owned.c_str(), nullptr, 10);
        value = static_cast<std::uint32_t>(
            std::min(parsed, static_cast<unsigned long>(std::numeric_limits<std::uint32_t>::max())));
        return true;
    };
    if (kind.empty()) {
        result.report.Add(ri::validate::IssueCode::VersionMismatch, "/kind", "document kind must not be empty");
        return result;
    }
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    if (!versionDotSeparated.empty()) {
        const std::size_t dot = versionDotSeparated.find('.');
        const std::string_view majorPart =
            dot == std::string_view::npos ? versionDotSeparated : versionDotSeparated.substr(0, dot);
        if (!parseUint32Token(majorPart, major)) {
            result.report.Add(ri::validate::IssueCode::VersionMismatch,
                              "/schemaVersion",
                              "invalid major version token");
            return result;
        }
        if (dot != std::string_view::npos) {
            const std::string_view minorPart = versionDotSeparated.substr(dot + 1U);
            if (!parseUint32Token(minorPart, minor)) {
                result.report.Add(ri::validate::IssueCode::VersionMismatch,
                                  "/schemaVersion",
                                  "invalid minor version token");
                return result;
            }
        }
    }
    result.value = DocumentHeader{std::string(kind), major, minor};
    return result;
}

void SchemaRegistry::Register(SchemaId id) {
    if (id.kind.empty()) {
        return;
    }
    entries_.try_emplace(std::move(id), std::string{});
}

void SchemaRegistry::Register(std::string_view kind, std::uint32_t versionMajor, std::uint32_t versionMinor) {
    Register(SchemaId{std::string(kind), versionMajor, versionMinor});
}

void SchemaRegistry::RegisterTagged(SchemaId id, std::string_view dispatchTag) {
    if (id.kind.empty()) {
        return;
    }
    entries_.insert_or_assign(std::move(id), std::string(dispatchTag));
}

void SchemaRegistry::RegisterTagged(std::string_view kind,
                                    std::uint32_t versionMajor,
                                    std::uint32_t versionMinor,
                                    std::string_view dispatchTag) {
    RegisterTagged(SchemaId{std::string(kind), versionMajor, versionMinor}, dispatchTag);
}

bool SchemaRegistry::Contains(const SchemaId& id) const {
    return entries_.find(id) != entries_.end();
}

bool SchemaRegistry::Contains(std::string_view kind, std::uint32_t versionMajor, std::uint32_t versionMinor) const {
    return Contains(SchemaId{std::string(kind), versionMajor, versionMinor});
}

std::optional<std::string_view> SchemaRegistry::TagFor(const SchemaId& id) const {
    const auto it = entries_.find(id);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return std::string_view(it->second);
}

void SchemaRegistry::Clear() {
    entries_.clear();
}

ri::validate::ValidationReport ValidateObjectShape(const ObjectShape& shape,
                                                   const std::unordered_set<std::string>& presentKeys) {
    ri::validate::ValidationReport report;
    for (const std::string& key : shape.requiredKeys) {
        if (presentKeys.find(key) == presentKeys.end()) {
            report.Add(ri::validate::IssueCode::MissingField, '/' + key, "required key absent");
        }
    }
    if (shape.unknownPolicy == UnknownKeyPolicy::AllowExtra
        || shape.unknownPolicy == UnknownKeyPolicy::Strip) {
        return report;
    }
    if (shape.unknownPolicy == UnknownKeyPolicy::Forbid
        || shape.unknownPolicy == UnknownKeyPolicy::PassthroughBag) {
        std::unordered_set<std::string> allowed;
        allowed.reserve(shape.requiredKeys.size() + shape.optionalKeys.size() + 1U);
        for (const std::string& key : shape.requiredKeys) {
            allowed.insert(key);
        }
        for (const std::string& key : shape.optionalKeys) {
            allowed.insert(key);
        }
        if (shape.unknownPolicy == UnknownKeyPolicy::PassthroughBag && !shape.extensionsBagKey.empty()) {
            allowed.insert(shape.extensionsBagKey);
        }
        for (const std::string& key : presentKeys) {
            if (allowed.find(key) == allowed.end()) {
                report.Add(ri::validate::IssueCode::UnknownKey, '/' + key, "unknown key");
            }
        }
    }
    return report;
}

void MigrationRegistry::AddEdge(MigrationEdge edge) {
    edges_.push_back(std::move(edge));
}

bool MigrationRegistry::CanReach(SchemaId from, SchemaId to) const {
    if (from == to) {
        return true;
    }
    std::set<SchemaId, SchemaIdLess> visited;
    std::queue<SchemaId> queue;
    queue.push(from);
    while (!queue.empty()) {
        SchemaId cur = queue.front();
        queue.pop();
        if (cur == to) {
            return true;
        }
        if (!visited.insert(cur).second) {
            continue;
        }
        for (const MigrationEdge& edge : edges_) {
            if (edge.from == cur) {
                queue.push(edge.to);
            }
        }
    }
    return false;
}

std::optional<std::vector<SchemaId>> MigrationRegistry::ShortestPath(SchemaId from, SchemaId to) const {
    if (from == to) {
        return std::vector<SchemaId>{from};
    }
    std::set<SchemaId, SchemaIdLess> visited;
    std::map<SchemaId, SchemaId, SchemaIdLess> parent;
    std::queue<SchemaId> queue;
    visited.insert(from);
    queue.push(from);
    while (!queue.empty()) {
        const SchemaId cur = queue.front();
        queue.pop();
        for (const MigrationEdge& edge : edges_) {
            if (edge.from != cur) {
                continue;
            }
            if (!visited.insert(edge.to).second) {
                continue;
            }
            parent[edge.to] = cur;
            if (edge.to == to) {
                std::vector<SchemaId> chain;
                for (SchemaId at = to;;) {
                    chain.push_back(at);
                    if (at == from) {
                        break;
                    }
                    const auto it = parent.find(at);
                    if (it == parent.end()) {
                        return std::nullopt;
                    }
                    at = it->second;
                }
                std::reverse(chain.begin(), chain.end());
                return chain;
            }
            queue.push(edge.to);
        }
    }
    return std::nullopt;
}

void MigrationRegistry::Clear() {
    edges_.clear();
}

} // namespace ri::data::schema
