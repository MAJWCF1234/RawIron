#include "RawIron/Core/KeyboardFocus.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ri::core {

#if defined(_WIN32)
namespace {

[[nodiscard]] bool WindowOwnsForeground(const HWND host, const HWND overlay) noexcept {
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        return false;
    }
    if (foreground == host || (overlay != nullptr && foreground == overlay)) {
        return true;
    }
    // A host may own further popups of its own (overlays, dialogs, tool windows). Keyboard input
    // still belongs to it when one of those holds the foreground.
    return GetAncestor(foreground, GA_ROOTOWNER) == GetAncestor(host, GA_ROOTOWNER);
}

} // namespace
#endif

void KeyboardFocusGate::Update(void* hostWindowHandle, void* overlayWindowHandle) noexcept {
#if defined(_WIN32)
    const HWND host = static_cast<HWND>(hostWindowHandle);
    const bool nowFocused =
        host != nullptr && WindowOwnsForeground(host, static_cast<HWND>(overlayWindowHandle));
#else
    (void)hostWindowHandle;
    (void)overlayWindowHandle;
    const bool nowFocused = false;
#endif
    justRegainedFocus_ = nowFocused && !focused_;
    focused_ = nowFocused;
}

bool KeyboardFocusGate::IsKeyDown(const int virtualKey) const noexcept {
    if (!focused_) {
        return false;
    }
#if defined(_WIN32)
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
#else
    (void)virtualKey;
    return false;
#endif
}

bool KeyboardFocusGate::IsKeyDownSettled(const int virtualKey) const noexcept {
    return !justRegainedFocus_ && IsKeyDown(virtualKey);
}

bool KeyboardFocusGate::ConsumeKeyPress(const int virtualKey) const noexcept {
#if defined(_WIN32)
    const bool pressedSinceLastQuery = (GetAsyncKeyState(virtualKey) & 0x0001) != 0;
    return pressedSinceLastQuery && focused_ && !justRegainedFocus_;
#else
    (void)virtualKey;
    return false;
#endif
}

} // namespace ri::core
