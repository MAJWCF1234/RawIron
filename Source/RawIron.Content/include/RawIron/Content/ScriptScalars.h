#pragma once

#include <filesystem>
#include <optional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

using ScriptScalarMap = std::map<std::string, float, std::less<>>;

/// Loads `key=value` float scalars from a simple script file.
/// - Empty lines and `#` comments are ignored.
/// - Invalid or non-finite values are skipped.
[[nodiscard]] ScriptScalarMap LoadScriptScalars(const std::filesystem::path& path);

/// Parses `key=value` float lines from in-memory script text (comments and blanks ignored).
[[nodiscard]] ScriptScalarMap LoadScriptScalarsFromText(std::string_view text);

/// Overwrites keys present in `overrides`; leaves other destination entries unchanged.
void MergeScriptScalarMaps(ScriptScalarMap& destination, const ScriptScalarMap& overrides);

/// Updates only the keys listed in `patches`, preserving comments and unrelated keys in the file.
[[nodiscard]] bool PatchScriptScalarsFile(const std::filesystem::path& path,
                                          const ScriptScalarMap& patches,
                                          std::string* errorOut = nullptr);

[[nodiscard]] float ScriptScalarOr(const ScriptScalarMap& values, std::string_view key, float fallback);

[[nodiscard]] float ScriptScalarOrClamped(const ScriptScalarMap& values,
                                          std::string_view key,
                                          float fallback,
                                          float minValue,
                                          float maxValue);

[[nodiscard]] int ScriptScalarOrInt(const ScriptScalarMap& values, std::string_view key, int fallback);

[[nodiscard]] int ScriptScalarOrIntClamped(const ScriptScalarMap& values,
                                           std::string_view key,
                                           int fallback,
                                           int minValue,
                                           int maxValue);

[[nodiscard]] bool ScriptScalarOrBool(const ScriptScalarMap& values, std::string_view key, bool fallback);

enum class ScriptScalarValidationSeverity {
    Warning,
    Error,
};

struct ScriptScalarRule {
    std::string key;
    bool required = false;
    std::optional<float> minValue{};
    std::optional<float> maxValue{};
};

struct ScriptScalarSchema {
    std::string name;
    bool allowUnknownKeys = true;
    std::vector<ScriptScalarRule> rules{};
};

struct ScriptScalarValidationIssue {
    ScriptScalarValidationSeverity severity = ScriptScalarValidationSeverity::Warning;
    std::string key;
    std::string message;
};

struct ScriptScalarValidationReport {
    std::vector<ScriptScalarValidationIssue> issues{};

    [[nodiscard]] bool ok() const;
};

[[nodiscard]] ScriptScalarValidationReport ValidateScriptScalars(
    const ScriptScalarMap& values,
    const ScriptScalarSchema& schema);

} // namespace ri::content
