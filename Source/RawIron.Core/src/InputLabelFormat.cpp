#include "RawIron/Core/InputLabelFormat.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace ri::core {
namespace {

bool EqualsInsensitive(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(lhs[index])) !=
            std::tolower(static_cast<unsigned char>(rhs[index]))) {
            return false;
        }
    }
    return true;
}

void NormalizeWhitespace(std::string& value) {
    std::replace_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }, '_');
}

std::string ToTitleCase(std::string value) {
    bool uppercaseNext = true;
    for (char& ch : value) {
        if (ch == '_' || ch == '-') {
            ch = ' ';
            uppercaseNext = true;
            continue;
        }

        if (uppercaseNext) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            uppercaseNext = false;
        } else {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }
    return value;
}

} // namespace

std::string NormalizeKeyboardInputId(std::string_view inputId) {
    std::string normalized(inputId);
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), normalized.end());

    if (normalized.empty()) {
        return {};
    }

    NormalizeWhitespace(normalized);

    if (EqualsInsensitive(normalized, "mouse1") || EqualsInsensitive(normalized, "mouseleft")) {
        return "MouseLeft";
    }
    if (EqualsInsensitive(normalized, "mouse2") || EqualsInsensitive(normalized, "mouseright")) {
        return "MouseRight";
    }
    if (EqualsInsensitive(normalized, "mouse3") || EqualsInsensitive(normalized, "mousemiddle")) {
        return "MouseMiddle";
    }
    if (EqualsInsensitive(normalized, "space") || EqualsInsensitive(normalized, "spacebar")) {
        return "Space";
    }
    if (EqualsInsensitive(normalized, "esc")) {
        return "Escape";
    }
    if (EqualsInsensitive(normalized, "ctrl") || EqualsInsensitive(normalized, "control")) {
        return "ControlLeft";
    }
    if (EqualsInsensitive(normalized, "shift")) {
        return "ShiftLeft";
    }
    if (EqualsInsensitive(normalized, "alt")) {
        return "AltLeft";
    }

    return normalized;
}

std::string FormatNormalizedKeyboardLabel(std::string_view normalizedInput) {
    if (normalizedInput.empty()) {
        return {};
    }

    if (normalizedInput == "Space") {
        return "Space";
    }
    if (normalizedInput == "Tab") {
        return "Tab";
    }
    if (normalizedInput == "Escape") {
        return "Esc";
    }
    if (normalizedInput == "ArrowUp") {
        return "Up Arrow";
    }
    if (normalizedInput == "ArrowDown") {
        return "Down Arrow";
    }
    if (normalizedInput == "ArrowLeft") {
        return "Left Arrow";
    }
    if (normalizedInput == "ArrowRight") {
        return "Right Arrow";
    }
    if (normalizedInput == "PageDown") {
        return "Page Down";
    }
    if (normalizedInput == "PageUp") {
        return "Page Up";
    }
    if (normalizedInput == "CapsLock") {
        return "Caps Lock";
    }
    if (normalizedInput == "NumLock") {
        return "Num Lock";
    }
    if (normalizedInput == "ScrollLock") {
        return "Scroll Lock";
    }
    if (normalizedInput == "ShiftLeft" || normalizedInput == "ShiftRight") {
        return "Shift";
    }
    if (normalizedInput == "ControlLeft" || normalizedInput == "ControlRight") {
        return "Ctrl";
    }
    if (normalizedInput == "AltLeft" || normalizedInput == "AltRight") {
        return "Alt";
    }
    if (normalizedInput == "MouseLeft") {
        return "Mouse 1";
    }
    if (normalizedInput == "MouseRight") {
        return "Mouse 2";
    }
    if (normalizedInput == "MouseMiddle") {
        return "Mouse 3";
    }
    if (normalizedInput.rfind("Digit", 0) == 0U && normalizedInput.size() == 6U) {
        return std::string(normalizedInput.substr(5U));
    }
    if (normalizedInput.rfind("Numpad", 0) == 0U && normalizedInput.size() >= 7U) {
        return "Num " + std::string(normalizedInput.substr(6U));
    }
    if (normalizedInput.rfind("Key", 0) == 0U && normalizedInput.size() == 4U) {
        return std::string(normalizedInput.substr(3U));
    }
    if (normalizedInput.rfind("F", 0) == 0U && normalizedInput.size() <= 3U) {
        const bool functionKey = std::all_of(normalizedInput.begin() + 1, normalizedInput.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        });
        if (functionKey) {
            return std::string(normalizedInput);
        }
    }

    return ToTitleCase(std::string(normalizedInput));
}

std::string FormatInputLabelFromInputId(std::string_view inputId) {
    const std::string normalizedInput = NormalizeKeyboardInputId(inputId);
    return FormatNormalizedKeyboardLabel(normalizedInput);
}

std::string KeyCodeToLabel(std::string_view keyCode) {
    return FormatInputLabelFromInputId(keyCode);
}

} // namespace ri::core
