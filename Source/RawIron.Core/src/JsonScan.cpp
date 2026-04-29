#include "RawIron/Core/Detail/JsonScan.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace ri::core::detail {

namespace fs = std::filesystem;

std::string ReadTextFile(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (stream.bad()) {
        return {};
    }
    return buffer.str();
}

bool WriteTextFile(const fs::path& path, std::string_view utf8) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }
    stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    stream.flush();
    return stream.good();
}

std::size_t SkipWhitespace(std::string_view text, std::size_t index) {
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) {
        ++index;
    }
    return index;
}

std::optional<std::string> ParseQuotedString(std::string_view text, std::size_t index, std::size_t* consumed) {
    if (index >= text.size() || text[index] != '"') {
        return std::nullopt;
    }

    std::string value;
    ++index;
    bool escaping = false;
    while (index < text.size()) {
        const char character = text[index++];
        if (escaping) {
            switch (character) {
                case '"':
                case '\\':
                case '/':
                    value.push_back(character);
                    break;
                case 'b':
                    value.push_back('\b');
                    break;
                case 'f':
                    value.push_back('\f');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                default:
                    value.push_back(character);
                    break;
            }
            escaping = false;
            continue;
        }

        if (character == '\\') {
            escaping = true;
            continue;
        }

        if (character == '"') {
            if (consumed != nullptr) {
                *consumed = index;
            }
            return value;
        }

        value.push_back(character);
    }

    return std::nullopt;
}

std::optional<std::size_t> FindJsonKey(std::string_view text, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    std::size_t index = text.find(needle);
    while (index != std::string_view::npos) {
        const std::size_t cursor = SkipWhitespace(text, index + needle.size());
        if (cursor < text.size() && text[cursor] == ':') {
            return cursor + 1U;
        }
        index = text.find(needle, index + 1U);
    }
    return std::nullopt;
}

std::optional<std::string> ExtractJsonString(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    return ParseQuotedString(text, SkipWhitespace(text, *valueIndex));
}

std::vector<std::string> ExtractJsonStringArray(std::string_view text, std::string_view key) {
    std::vector<std::string> values;
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return values;
    }

    std::size_t cursor = SkipWhitespace(text, *valueIndex);
    if (cursor >= text.size() || text[cursor] != '[') {
        return values;
    }
    ++cursor;

    while (cursor < text.size()) {
        cursor = SkipWhitespace(text, cursor);
        if (cursor >= text.size()) {
            break;
        }
        if (text[cursor] == ']') {
            break;
        }

        std::size_t consumed = cursor;
        const std::optional<std::string> item = ParseQuotedString(text, cursor, &consumed);
        if (!item.has_value()) {
            break;
        }
        values.push_back(*item);
        cursor = SkipWhitespace(text, consumed);
        if (cursor < text.size() && text[cursor] == ',') {
            ++cursor;
        }
    }

    return values;
}

std::optional<bool> ExtractJsonBool(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    std::size_t cursor = SkipWhitespace(text, *valueIndex);
    if (cursor + 4U <= text.size() && text.substr(cursor, 4) == "true") {
        return true;
    }
    if (cursor + 5U <= text.size() && text.substr(cursor, 5) == "false") {
        return false;
    }
    return std::nullopt;
}

std::optional<std::int32_t> ExtractJsonInt(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    std::size_t cursor = SkipWhitespace(text, *valueIndex);
    bool negative = false;
    if (cursor < text.size() && text[cursor] == '-') {
        negative = true;
        ++cursor;
    }
    if (cursor >= text.size() || !std::isdigit(static_cast<unsigned char>(text[cursor]))) {
        return std::nullopt;
    }
    std::int64_t value = 0;
    while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0) {
        value = value * 10 + static_cast<std::int64_t>(text[cursor] - '0');
        ++cursor;
        if (value > 0x7FFFFFFFLL) {
            return std::nullopt;
        }
    }
    const std::int32_t narrowed = static_cast<std::int32_t>(negative ? -value : value);
    return narrowed;
}

namespace {

[[nodiscard]] bool ScanJsonNumberToken(std::string_view text, std::size_t start, std::size_t* consumedOut, double* valueOut) {
    std::size_t index = start;
    if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
        ++index;
    }
    while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
        ++index;
    }
    if (index < text.size() && text[index] == '.') {
        ++index;
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
    }
    if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
        ++index;
        if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
            ++index;
        }
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
    }

    if (index == start) {
        return false;
    }

    std::string buffer(text.substr(start, index - start));
    char* parseEnd = nullptr;
    const double parsed = std::strtod(buffer.c_str(), &parseEnd);
    if (parseEnd == buffer.c_str()) {
        return false;
    }
    *valueOut = parsed;
    *consumedOut = index;
    return true;
}

} // namespace

std::optional<double> ExtractJsonDouble(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    std::size_t cursor = SkipWhitespace(text, *valueIndex);
    std::size_t consumed = 0;
    double value = 0.0;
    if (!ScanJsonNumberToken(text, cursor, &consumed, &value)) {
        return std::nullopt;
    }
    (void)consumed;
    return value;
}

std::optional<std::size_t> FindMatchingBrace(std::string_view text, std::size_t openBraceIndex) {
    if (openBraceIndex >= text.size() || text[openBraceIndex] != '{') {
        return std::nullopt;
    }
    std::size_t depth = 1;
    std::size_t index = openBraceIndex + 1;
    bool inString = false;
    bool escaping = false;
    while (index < text.size() && depth > 0) {
        const char character = text[index++];
        if (inString) {
            if (escaping) {
                escaping = false;
                continue;
            }
            if (character == '\\') {
                escaping = true;
                continue;
            }
            if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '{') {
            ++depth;
            continue;
        }
        if (character == '}') {
            --depth;
            if (depth == 0) {
                return index;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string_view> ExtractJsonObject(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    const std::size_t cursor = SkipWhitespace(text, *valueIndex);
    if (cursor >= text.size() || text[cursor] != '{') {
        return std::nullopt;
    }
    const std::optional<std::size_t> endExclusive = FindMatchingBrace(text, cursor);
    if (!endExclusive.has_value()) {
        return std::nullopt;
    }
    return text.substr(cursor, *endExclusive - cursor);
}

std::optional<std::uint64_t> ExtractJsonUInt64(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    std::size_t cursor = SkipWhitespace(text, *valueIndex);
    if (cursor >= text.size() || !std::isdigit(static_cast<unsigned char>(text[cursor]))) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0) {
        const int digit = text[cursor] - '0';
        ++cursor;
        if (value > (std::numeric_limits<std::uint64_t>::max() - static_cast<unsigned>(digit)) / 10ULL) {
            return std::nullopt;
        }
        value = value * 10ULL + static_cast<std::uint64_t>(digit);
    }
    return value;
}

std::vector<std::string_view> SplitJsonArrayObjects(std::string_view text, std::string_view key) {
    std::vector<std::string_view> objects;
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return objects;
    }
    std::size_t cursor = SkipWhitespace(text, *valueIndex);
    if (cursor >= text.size() || text[cursor] != '[') {
        return objects;
    }
    ++cursor;

    while (cursor < text.size()) {
        cursor = SkipWhitespace(text, cursor);
        if (cursor >= text.size()) {
            break;
        }
        if (text[cursor] == ']') {
            break;
        }
        if (text[cursor] != '{') {
            break;
        }
        const std::optional<std::size_t> endExclusive = FindMatchingBrace(text, cursor);
        if (!endExclusive.has_value()) {
            break;
        }
        objects.push_back(text.substr(cursor, *endExclusive - cursor));
        cursor = SkipWhitespace(text, *endExclusive);
        if (cursor < text.size() && text[cursor] == ',') {
            ++cursor;
        }
    }

    return objects;
}

std::string EscapeJsonString(std::string_view utf8) {
    std::string out;
    out.reserve(utf8.size() + 8);
    for (const char character : utf8) {
        switch (character) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
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
                if (static_cast<unsigned char>(character) < 0x20U) {
                    out.push_back(character);
                } else {
                    out.push_back(character);
                }
                break;
        }
    }
    return out;
}

} // namespace ri::core::detail
