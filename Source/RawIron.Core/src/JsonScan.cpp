#include "RawIron/Core/Detail/JsonScan.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace ri::core::detail {

namespace fs = std::filesystem;

namespace {

[[nodiscard]] bool IsJsonValueTerminator(std::string_view text, std::size_t index) {
    index = SkipWhitespace(text, index);
    return index >= text.size() || text[index] == ',' || text[index] == '}' || text[index] == ']';
}

[[nodiscard]] int HexDigitValue(const char character) noexcept {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return 10 + character - 'a';
    if (character >= 'A' && character <= 'F') return 10 + character - 'A';
    return -1;
}

[[nodiscard]] bool ParseHexCodeUnit(std::string_view text, const std::size_t index, std::uint32_t& value) noexcept {
    if (index > text.size() || text.size() - index < 4U) {
        return false;
    }
    value = 0U;
    for (std::size_t offset = 0; offset < 4U; ++offset) {
        const int digit = HexDigitValue(text[index + offset]);
        if (digit < 0) {
            return false;
        }
        value = (value << 4U) | static_cast<std::uint32_t>(digit);
    }
    return true;
}

void AppendUtf8(std::string& output, const std::uint32_t codePoint) {
    if (codePoint <= 0x7FU) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    } else if (codePoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
}

} // namespace

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
                case 'u': {
                    std::uint32_t codePoint = 0U;
                    if (!ParseHexCodeUnit(text, index, codePoint)) {
                        return std::nullopt;
                    }
                    index += 4U;
                    if (codePoint >= 0xD800U && codePoint <= 0xDBFFU) {
                        if (index > text.size() || text.size() - index < 6U
                            || text[index] != '\\' || text[index + 1U] != 'u') {
                            return std::nullopt;
                        }
                        std::uint32_t lowSurrogate = 0U;
                        if (!ParseHexCodeUnit(text, index + 2U, lowSurrogate)
                            || lowSurrogate < 0xDC00U || lowSurrogate > 0xDFFFU) {
                            return std::nullopt;
                        }
                        index += 6U;
                        codePoint = 0x10000U + ((codePoint - 0xD800U) << 10U) + (lowSurrogate - 0xDC00U);
                    } else if (codePoint >= 0xDC00U && codePoint <= 0xDFFFU) {
                        return std::nullopt;
                    }
                    AppendUtf8(value, codePoint);
                    break;
                }
                default:
                    return std::nullopt;
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

        if (static_cast<unsigned char>(character) < 0x20U) {
            return std::nullopt;
        }

        value.push_back(character);
    }

    return std::nullopt;
}

std::optional<std::size_t> FindJsonKey(std::string_view text, std::string_view key) {
    std::size_t index = 0;
    while (index < text.size()) {
        index = SkipWhitespace(text, index);
        if (index >= text.size()) {
            break;
        }
        if (text[index] != '"') {
            ++index;
            continue;
        }

        std::size_t consumed = index;
        const std::optional<std::string> parsedKey = ParseQuotedString(text, index, &consumed);
        if (!parsedKey.has_value()) {
            ++index;
            continue;
        }
        const std::size_t cursor = SkipWhitespace(text, consumed);
        if (*parsedKey == key && cursor < text.size() && text[cursor] == ':') {
            return cursor + 1U;
        }
        index = consumed > index ? consumed : index + 1U;
    }
    return std::nullopt;
}

std::optional<std::string> ExtractJsonString(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    const std::size_t start = SkipWhitespace(text, *valueIndex);
    std::size_t consumed = start;
    const std::optional<std::string> value = ParseQuotedString(text, start, &consumed);
    if (!value.has_value() || !IsJsonValueTerminator(text, consumed)) {
        return std::nullopt;
    }
    return value;
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

    cursor = SkipWhitespace(text, cursor);
    if (cursor < text.size() && text[cursor] == ']') {
        return values;
    }
    while (cursor < text.size()) {
        cursor = SkipWhitespace(text, cursor);
        if (cursor >= text.size() || text[cursor] == ']') {
            return {};
        }

        std::size_t consumed = cursor;
        const std::optional<std::string> item = ParseQuotedString(text, cursor, &consumed);
        if (!item.has_value()) {
            return {};
        }
        values.push_back(*item);
        cursor = SkipWhitespace(text, consumed);
        if (cursor < text.size() && text[cursor] == ']') {
            return values;
        }
        if (cursor < text.size() && text[cursor] == ',') {
            ++cursor;
            const std::size_t next = SkipWhitespace(text, cursor);
            if (next >= text.size() || text[next] == ']') {
                return {};
            }
            continue;
        }
        return {};
    }

    return {};
}

std::optional<bool> ExtractJsonBool(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    std::size_t cursor = SkipWhitespace(text, *valueIndex);
    if (cursor + 4U <= text.size() && text.substr(cursor, 4) == "true" && IsJsonValueTerminator(text, cursor + 4U)) {
        return true;
    }
    if (cursor + 5U <= text.size() && text.substr(cursor, 5) == "false" && IsJsonValueTerminator(text, cursor + 5U)) {
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

    constexpr std::int64_t kMaxPositive = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    constexpr std::int64_t kMaxNegativeMagnitude = kMaxPositive + 1LL;
    const std::int64_t limit = negative ? kMaxNegativeMagnitude : kMaxPositive;
    std::int64_t value = 0;
    while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0) {
        value = value * 10 + static_cast<std::int64_t>(text[cursor] - '0');
        ++cursor;
        if (value > limit) {
            return std::nullopt;
        }
    }
    if (!IsJsonValueTerminator(text, cursor)) {
        return std::nullopt;
    }
    if (negative && value == kMaxNegativeMagnitude) {
        return std::numeric_limits<std::int32_t>::min();
    }
    const std::int32_t narrowed = static_cast<std::int32_t>(negative ? -value : value);
    return narrowed;
}

namespace {

[[nodiscard]] bool ScanJsonNumberToken(std::string_view text, std::size_t start, std::size_t* consumedOut, double* valueOut) {
    std::size_t index = start;
    if (index < text.size() && text[index] == '-') {
        ++index;
    }

    if (index >= text.size() || !std::isdigit(static_cast<unsigned char>(text[index]))) {
        return false;
    }
    if (text[index] == '0') {
        ++index;
    } else {
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
    }

    if (index < text.size() && text[index] == '.') {
        ++index;
        const std::size_t fractionalStart = index;
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
        if (index == fractionalStart) {
            return false;
        }
    }
    if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
        ++index;
        if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
            ++index;
        }
        const std::size_t exponentStart = index;
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index])) != 0) {
            ++index;
        }
        if (index == exponentStart) {
            return false;
        }
    }

    if (!IsJsonValueTerminator(text, index)) {
        return false;
    }

    std::string buffer(text.substr(start, index - start));
    char* parseEnd = nullptr;
    const double parsed = std::strtod(buffer.c_str(), &parseEnd);
    if (parseEnd == buffer.c_str() || parseEnd != buffer.c_str() + buffer.size() || !std::isfinite(parsed)) {
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
    if (!IsJsonValueTerminator(text, cursor)) {
        return std::nullopt;
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
