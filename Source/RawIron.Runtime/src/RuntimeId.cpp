#include "RawIron/Runtime/RuntimeId.h"

// Proto shell: `engine/runtimeIdShim.js` (keep alphabet, suffix length, and prefix rules aligned).

#include <algorithm>
#include <cctype>
#include <random>
#include <string>

namespace ri::runtime {
namespace {

std::mt19937& RuntimeIdRng() {
    thread_local std::mt19937 generator(std::random_device{}());
    return generator;
}

} // namespace

std::string SanitizeRuntimeIdPrefix(std::string_view prefix) {
    std::string trimmed(prefix);

    const auto first = std::find_if_not(trimmed.begin(), trimmed.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(trimmed.rbegin(), trimmed.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    trimmed = first < last ? std::string(first, last) : std::string{};

    std::string sanitized;
    sanitized.reserve(trimmed.size());
    bool previousWasHyphen = false;
    for (const unsigned char c : trimmed) {
        const bool valid = std::isalnum(c) != 0 || c == '_' || c == '-';
        if (valid) {
            sanitized.push_back(static_cast<char>(c));
            previousWasHyphen = false;
            continue;
        }
        if (!previousWasHyphen) {
            sanitized.push_back('-');
            previousWasHyphen = true;
        }
    }

    while (!sanitized.empty() && sanitized.front() == '-') {
        sanitized.erase(sanitized.begin());
    }
    while (!sanitized.empty() && sanitized.back() == '-') {
        sanitized.pop_back();
    }

    return sanitized.empty() ? std::string("rt") : sanitized;
}

std::string CreateRuntimeId(std::string_view prefix) {
    static constexpr char kAlphabet[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static constexpr std::size_t kAlphabetSize = sizeof(kAlphabet) - 1;
    static constexpr std::size_t kSuffixLength = 10;

    std::uniform_int_distribution<std::size_t> distribution(0, kAlphabetSize - 1);

    std::string value = SanitizeRuntimeIdPrefix(prefix);
    value.push_back('_');
    for (std::size_t index = 0; index < kSuffixLength; ++index) {
        value.push_back(kAlphabet[distribution(RuntimeIdRng())]);
    }
    return value;
}

} // namespace ri::runtime
