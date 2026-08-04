#pragma once

/// Focus gate for windows that poll the keyboard.
///
/// Win32 `GetAsyncKeyState` reports the desktop-wide keyboard, not the calling window's, so code
/// that polls it directly keeps reacting to keys typed into other applications: pause menus open by
/// themselves, the player walks while you type in a browser, music toggles from a background app,
/// and Escape pressed anywhere can close the window. Route every polled key read through this.

namespace ri::core {

class KeyboardFocusGate {
public:
    /// Call once per frame, before reading any key. `overlayWindowHandle` is optional and covers
    /// hosts whose UI overlay is a separate window that can take focus.
    void Update(void* hostWindowHandle, void* overlayWindowHandle = nullptr) noexcept;

    [[nodiscard]] bool Focused() const noexcept { return focused_; }

    /// True only on the first frame after focus returns. A key still held from before the window
    /// went to the background must not read as a fresh press the moment focus comes back, so edge
    /// detectors latch state on this frame instead of firing.
    [[nodiscard]] bool JustRegainedFocus() const noexcept { return justRegainedFocus_; }

    /// Held state for `virtualKey`, or false whenever the window does not own keyboard input.
    [[nodiscard]] bool IsKeyDown(int virtualKey) const noexcept;

    /// Held state for `virtualKey` that also reads false on the frame focus returns. Use for keys
    /// whose edge is derived by the caller and would otherwise fire on refocus.
    [[nodiscard]] bool IsKeyDownSettled(int virtualKey) const noexcept;

    /// Edge read of the "pressed since the last query" bit. Call it every frame: the query is
    /// issued even while unfocused so presses made in another application are drained rather than
    /// queuing up and firing the instant the window regains focus.
    [[nodiscard]] bool ConsumeKeyPress(int virtualKey) const noexcept;

private:
    bool focused_ = false;
    bool justRegainedFocus_ = false;
};

} // namespace ri::core
