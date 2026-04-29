#pragma once

#include <string>
#include <string_view>

namespace ri::core {

/// Normalizes user-authored or legacy input tokens to canonical IDs (same rules as `ActionBindings`
/// historically used internally). Proto: `engine/keyboardLabelShim.js` (`normalizeKeyboardInputId`).
[[nodiscard]] std::string NormalizeKeyboardInputId(std::string_view inputId);

/// Pretty label for a **canonical** input id (after `NormalizeKeyboardInputId`). Handles Web
/// `KeyboardEvent.code` patterns (`KeyE`, `Digit1`, …), modifiers, mouse buttons, and function keys.
[[nodiscard]] std::string FormatNormalizedKeyboardLabel(std::string_view normalizedInputId);

/// Normalize then format — preferred entry for tooling and UI. Mirrors the contract of
/// `ActionBindings::FormatInputLabel`.
[[nodiscard]] std::string FormatInputLabelFromInputId(std::string_view inputId);
/// Compatibility helper for keycode-oriented callsites (same behavior as `FormatInputLabelFromInputId`).
[[nodiscard]] std::string KeyCodeToLabel(std::string_view keyCode);

} // namespace ri::core
