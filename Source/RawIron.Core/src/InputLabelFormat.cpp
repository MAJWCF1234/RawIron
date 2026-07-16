#include "RawIron/Core/InputLabelFormat.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace ri::core {
namespace {

std::string ToLower(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

std::string ToTitleCase(std::string value) {
    std::string formatted;
    formatted.reserve(value.size());
    bool uppercaseNext = true;
    for (char ch : value) {
        if (ch == '_' || ch == '-') {
            if (!formatted.empty() && formatted.back() != ' ') {
                formatted.push_back(' ');
            }
            uppercaseNext = true;
            continue;
        }

        if (uppercaseNext) {
            formatted.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            uppercaseNext = false;
        } else {
            formatted.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    if (!formatted.empty() && formatted.back() == ' ') {
        formatted.pop_back();
    }
    return formatted;
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

    const std::string lowered = ToLower(normalized);

    if (lowered == "mouse1" || lowered == "mouseleft") {
        return "MouseLeft";
    }
    if (lowered == "mouse2" || lowered == "mouseright") {
        return "MouseRight";
    }
    if (lowered == "mouse3" || lowered == "mousemiddle") {
        return "MouseMiddle";
    }
    if (lowered == "mouse4" || lowered == "mouseback") {
        return "MouseBack";
    }
    if (lowered == "mouse5" || lowered == "mouseforward") {
        return "MouseForward";
    }
    if (lowered == "space" || lowered == "spacebar") {
        return "Space";
    }
    if (lowered == "esc" || lowered == "escape") {
        return "Escape";
    }
    if (lowered == "enter" || lowered == "return") {
        return "Enter";
    }
    if (lowered == "ctrl" || lowered == "control" || lowered == "ctrlleft" || lowered == "controlleft") {
        return "ControlLeft";
    }
    if (lowered == "ctrlright" || lowered == "controlright") {
        return "ControlRight";
    }
    if (lowered == "shift" || lowered == "shiftleft") {
        return "ShiftLeft";
    }
    if (lowered == "shiftright") {
        return "ShiftRight";
    }
    if (lowered == "alt" || lowered == "altleft") {
        return "AltLeft";
    }
    if (lowered == "altright") {
        return "AltRight";
    }

    static constexpr std::pair<std::string_view, std::string_view> kCanonicalCodes[] = {
        {"tab", "Tab"}, {"backspace", "Backspace"}, {"delete", "Delete"},
        {"insert", "Insert"}, {"home", "Home"}, {"end", "End"},
        {"arrowup", "ArrowUp"}, {"arrowdown", "ArrowDown"},
        {"arrowleft", "ArrowLeft"}, {"arrowright", "ArrowRight"},
        {"pageup", "PageUp"}, {"pagedown", "PageDown"},
        {"capslock", "CapsLock"}, {"numlock", "NumLock"}, {"scrolllock", "ScrollLock"},
        {"metaleft", "MetaLeft"}, {"metaright", "MetaRight"},
    };
    for (const auto& [candidate, canonical] : kCanonicalCodes) {
        if (lowered == candidate) {
            return std::string(canonical);
        }
    }

    if (lowered.size() == 4U && lowered.rfind("key", 0) == 0U
        && std::isalpha(static_cast<unsigned char>(lowered[3])) != 0) {
        return "Key" + std::string(1U, static_cast<char>(std::toupper(static_cast<unsigned char>(lowered[3]))));
    }
    if (lowered.size() == 6U && lowered.rfind("digit", 0) == 0U
        && std::isdigit(static_cast<unsigned char>(lowered[5])) != 0) {
        return "Digit" + std::string(1U, lowered[5]);
    }
    if (lowered.size() >= 2U && lowered.size() <= 3U && lowered[0] == 'f') {
        int functionIndex = 0;
        bool digitsOnly = true;
        for (std::size_t index = 1; index < lowered.size(); ++index) {
            digitsOnly = digitsOnly && std::isdigit(static_cast<unsigned char>(lowered[index])) != 0;
            functionIndex = functionIndex * 10 + (lowered[index] - '0');
        }
        if (digitsOnly && functionIndex >= 1 && functionIndex <= 24) {
            return "F" + std::to_string(functionIndex);
        }
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
    if (normalizedInput == "MouseBack") {
        return "Mouse 4";
    }
    if (normalizedInput == "MouseForward") {
        return "Mouse 5";
    }
    if (normalizedInput.rfind("Digit", 0) == 0U && normalizedInput.size() == 6U
        && std::isdigit(static_cast<unsigned char>(normalizedInput[5])) != 0) {
        return std::string(normalizedInput.substr(5U));
    }
    if (normalizedInput.rfind("Numpad", 0) == 0U && normalizedInput.size() >= 7U) {
        return "Num " + std::string(normalizedInput.substr(6U));
    }
    if (normalizedInput.rfind("Key", 0) == 0U && normalizedInput.size() == 4U
        && std::isalpha(static_cast<unsigned char>(normalizedInput[3])) != 0) {
        return std::string(normalizedInput.substr(3U));
    }
    if (normalizedInput.rfind("F", 0) == 0U && normalizedInput.size() >= 2U && normalizedInput.size() <= 3U) {
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
