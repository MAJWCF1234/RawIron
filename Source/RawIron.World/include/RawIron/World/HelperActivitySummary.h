#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace ri::world {

/// Collapses ASCII whitespace runs (space, tab, CR, LF) to a single space, trims ends, returns `"none"`
/// when empty, then truncates with a trailing `~` when longer than `maxLength` (matches mechanics
/// debug / instrumentation labels in the web shell — see `engine/helperActivityShim.js`).
[[nodiscard]] inline std::string SummarizeHelperActivity(std::string_view value,
                                                         std::size_t maxLength = 24) {
    std::string normalized;
    normalized.reserve(value.size());
    bool previousWhitespace = false;

    for (char character : value) {
        const bool whitespace =
            character == ' ' || character == '\t' || character == '\r' || character == '\n';
        if (whitespace) {
            if (!normalized.empty() && !previousWhitespace) {
                normalized.push_back(' ');
            }
            previousWhitespace = true;
            continue;
        }
        normalized.push_back(character);
        previousWhitespace = false;
    }

    while (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
    }
    if (normalized.empty()) {
        return "none";
    }
    if (normalized.size() > maxLength) {
        const std::size_t prefixLength = maxLength > 0 ? std::max<std::size_t>(1, maxLength - 1) : 0;
        return normalized.substr(0, prefixLength) + '~';
    }
    return normalized;
}

} // namespace ri::world
