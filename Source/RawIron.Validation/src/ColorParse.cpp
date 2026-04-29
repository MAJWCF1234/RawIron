#include "RawIron/DataSchema/ColorParse.h"

#include <cctype>

namespace ri::validate {

namespace {

std::optional<int> HexNibble(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return std::nullopt;
}

std::optional<int> ParseHexPair(std::string_view text, std::size_t i) {
    if (i + 1U >= text.size()) {
        return std::nullopt;
    }
    const std::optional<int> hi = HexNibble(text[i]);
    const std::optional<int> lo = HexNibble(text[i + 1U]);
    if (!hi.has_value() || !lo.has_value()) {
        return std::nullopt;
    }
    return (*hi << 4) | *lo;
}

} // namespace

SafeParseResult<std::array<std::uint8_t, 4U>> TryParseColorRgba8(std::string_view text) {
    SafeParseResult<std::array<std::uint8_t, 4U>> out{};
    if (text.empty() || text.front() != '#') {
        out.report.Add(IssueCode::CoercionRejected, "/color", "expected #rrggbb-style color");
        return out;
    }
    text.remove_prefix(1U);
    std::array<std::uint8_t, 4U> rgba{};

    if (text.size() == 3U) {
        for (std::size_t i = 0; i < 3U; ++i) {
            const std::optional<int> n = HexNibble(text[i]);
            if (!n.has_value()) {
                out.report.Add(IssueCode::CoercionRejected, "/color", "invalid hex digit in #rgb");
                return out;
            }
            const int v = *n * 17;
            rgba[i] = static_cast<std::uint8_t>(v);
        }
        rgba[3U] = 255U;
        out.value = rgba;
        return out;
    }
    if (text.size() == 6U) {
        for (std::size_t c = 0; c < 3U; ++c) {
            const std::optional<int> v = ParseHexPair(text, c * 2U);
            if (!v.has_value() || *v < 0 || *v > 255) {
                out.report.Add(IssueCode::CoercionRejected, "/color", "invalid #rrggbb color");
                return out;
            }
            rgba[c] = static_cast<std::uint8_t>(*v);
        }
        rgba[3U] = 255U;
        out.value = rgba;
        return out;
    }
    if (text.size() == 8U) {
        for (std::size_t c = 0; c < 4U; ++c) {
            const std::optional<int> v = ParseHexPair(text, c * 2U);
            if (!v.has_value() || *v < 0 || *v > 255) {
                out.report.Add(IssueCode::CoercionRejected, "/color", "invalid #rrggbbaa color");
                return out;
            }
            rgba[c] = static_cast<std::uint8_t>(*v);
        }
        out.value = rgba;
        return out;
    }
    out.report.Add(IssueCode::CoercionRejected, "/color", "color must be #rgb, #rrggbb, or #rrggbbaa");
    return out;
}

} // namespace ri::validate
