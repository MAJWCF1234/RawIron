#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::core::detail {

[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& path);

[[nodiscard]] bool WriteTextFile(const std::filesystem::path& path, std::string_view utf8);

[[nodiscard]] std::size_t SkipWhitespace(std::string_view text, std::size_t index);

[[nodiscard]] std::optional<std::string> ParseQuotedString(std::string_view text,
                                                          std::size_t index,
                                                          std::size_t* consumed = nullptr);

[[nodiscard]] std::optional<std::size_t> FindJsonKey(std::string_view text, std::string_view key);

[[nodiscard]] std::optional<std::string> ExtractJsonString(std::string_view text, std::string_view key);

[[nodiscard]] std::vector<std::string> ExtractJsonStringArray(std::string_view text, std::string_view key);

[[nodiscard]] std::optional<bool> ExtractJsonBool(std::string_view text, std::string_view key);

[[nodiscard]] std::optional<std::int32_t> ExtractJsonInt(std::string_view text, std::string_view key);

[[nodiscard]] std::optional<std::uint64_t> ExtractJsonUInt64(std::string_view text, std::string_view key);

[[nodiscard]] std::optional<double> ExtractJsonDouble(std::string_view text, std::string_view key);

/// Locates `"key": { ... }` and returns a view of the full JSON object including braces.
[[nodiscard]] std::optional<std::string_view> ExtractJsonObject(std::string_view text, std::string_view key);

/// After `openBraceIndex` pointing at `{`, returns the index one past the matching `}` (or nullopt).
[[nodiscard]] std::optional<std::size_t> FindMatchingBrace(std::string_view text, std::size_t openBraceIndex);

/// Parses a top-level JSON array whose elements are JSON objects `{...}`.
[[nodiscard]] std::vector<std::string_view> SplitJsonArrayObjects(std::string_view text,
                                                                 std::string_view key);

[[nodiscard]] std::string EscapeJsonString(std::string_view utf8);

} // namespace ri::core::detail
