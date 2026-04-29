#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace ri::content {

using ScriptScalarMap = std::map<std::string, float, std::less<>>;

/// Loads `key=value` float scalars from a simple script file.
/// - Empty lines and `#` comments are ignored.
/// - Invalid or non-finite values are skipped.
[[nodiscard]] ScriptScalarMap LoadScriptScalars(const std::filesystem::path& path);

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

} // namespace ri::content
