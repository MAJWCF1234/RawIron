#include "RawIron/Content/Value.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

namespace ri::content {
namespace {

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::string Lower(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char character : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return lowered;
}

std::optional<double> ParseFiniteDouble(std::string_view text) noexcept {
    const std::string token = Trim(text);
    if (token.empty()) {
        return std::nullopt;
    }

    char* parseEnd = nullptr;
    const double parsed = std::strtod(token.c_str(), &parseEnd);
    if (parseEnd == token.c_str() || parseEnd == nullptr || *parseEnd != '\0' || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

} // namespace

const Value* Value::FindPath(std::string_view dottedPath) const noexcept {
    if (dottedPath.empty()) {
        return this;
    }
    const Value* cursor = this;
    std::size_t offset = 0;
    while (cursor != nullptr && offset < dottedPath.size()) {
        const std::size_t separator = dottedPath.find('.', offset);
        const std::size_t length =
            separator == std::string_view::npos ? (dottedPath.size() - offset) : (separator - offset);
        if (length == 0U) {
            return nullptr;
        }
        cursor = cursor->Find(dottedPath.substr(offset, length));
        if (separator == std::string_view::npos) {
            break;
        }
        offset = separator + 1U;
    }
    return cursor;
}

Value* Value::FindPath(std::string_view dottedPath) noexcept {
    return const_cast<Value*>(std::as_const(*this).FindPath(dottedPath));
}

std::optional<double> Value::CoerceNumber() const noexcept {
    if (const double* number = TryGetNumber(); number != nullptr && std::isfinite(*number)) {
        return *number;
    }
    if (const bool* boolean = TryGetBoolean(); boolean != nullptr) {
        return *boolean ? 1.0 : 0.0;
    }
    if (const std::string* text = TryGetString(); text != nullptr) {
        return ParseFiniteDouble(*text);
    }
    return std::nullopt;
}

std::optional<bool> Value::CoerceBoolean() const noexcept {
    if (const bool* boolean = TryGetBoolean(); boolean != nullptr) {
        return *boolean;
    }
    if (const double* number = TryGetNumber(); number != nullptr && std::isfinite(*number)) {
        return *number != 0.0;
    }
    if (const std::string* text = TryGetString(); text != nullptr) {
        const std::string lowered = Lower(Trim(*text));
        if (lowered == "true" || lowered == "yes" || lowered == "on" || lowered == "1") {
            return true;
        }
        if (lowered == "false" || lowered == "no" || lowered == "off" || lowered == "0") {
            return false;
        }
    }
    return std::nullopt;
}

Value Value::ParseLooseScalar(std::string_view text) {
    const std::string token = Trim(text);
    const std::string lowered = Lower(token);
    if (lowered == "null") {
        return Value{};
    }
    if (lowered == "true") {
        return Value{true};
    }
    if (lowered == "false") {
        return Value{false};
    }
    if (const std::optional<double> number = ParseFiniteDouble(token); number.has_value()) {
        return Value{*number};
    }
    return Value{token};
}

} // namespace ri::content
