#pragma once

/// Reusable in-game menu / visual-novel overlay for Windows hosts.
///
/// Any game can put a JSON UI manifest on screen by binding a `UiFlowSession` + `UiFlowController`
/// to this overlay and calling `SyncToHost` each frame; the engine owns window creation, DPI-aware
/// layout, painting, and pointer hit-testing. Hosts remain responsible only for policy: which
/// manifest is active, and what `emit` action ids mean.

#include "RawIron/Ui/UiFlowController.h"
#include "RawIron/Ui/UiPresentation.h"

#include <vector>

namespace ri::ui {

/// Pointer activity accumulated by the overlay window since the last drain.
struct UiOverlayPointerInput {
    /// A click landed on the overlay but not on an option row (VN click-to-advance).
    bool clickedEmptySpace = false;
    /// Option index a click activated, or -1.
    int activatedOptionIndex = -1;
    /// Option index the pointer moved onto since the last drain, or -1 when it has not changed
    /// rows. Reported as an edge so a mouse resting on a row cannot fight keyboard navigation.
    int hoveredOptionIndex = -1;
    bool wheelMoved = false;
};

class UiOverlayWindowWin32 {
public:
    UiOverlayWindowWin32() = default;
    ~UiOverlayWindowWin32();
    UiOverlayWindowWin32(const UiOverlayWindowWin32&) = delete;
    UiOverlayWindowWin32& operator=(const UiOverlayWindowWin32&) = delete;
    UiOverlayWindowWin32(UiOverlayWindowWin32&&) = delete;
    UiOverlayWindowWin32& operator=(UiOverlayWindowWin32&&) = delete;

    /// Binds what the overlay paints; pass nullptr for both to detach.
    void Bind(const UiFlowSession* session, UiFlowController* controller);

    /// Creates the overlay on first use, pins it over the host window's client area, and shows it.
    /// Hides itself while the host is minimized, and only floats above other applications while the
    /// host has focus. Repaints only when the geometry moved, the bound content changed, or
    /// `RequestRepaint` was called, so an idle menu costs nothing. Returns false when the overlay
    /// is unavailable.
    bool SyncToHost(void* hostWindowHandle);

    void Hide();
    void Destroy();

    void RequestRepaint() noexcept { needsRepaint_ = true; }

    [[nodiscard]] bool Visible() const noexcept { return visible_; }
    [[nodiscard]] void* WindowHandle() const noexcept { return windowHandle_; }

    /// Returns pointer input gathered from overlay mouse messages and clears it.
    UiOverlayPointerInput DrainPointerInput() noexcept;

    /// Internal: routed from the overlay window procedure. Returns true when the message was
    /// consumed. Declared public only so the window procedure can reach it.
    bool HandleWindowMessage(unsigned int message, unsigned long long wParam, long long lParam);

private:
    void Paint();
    /// Returns the option index at overlay client coordinates, or -1.
    [[nodiscard]] int HitTestOption(int clientX, int clientY) const;

    const UiFlowSession* session_ = nullptr;
    UiFlowController* controller_ = nullptr;
    void* windowHandle_ = nullptr;
    bool visible_ = false;
    bool needsRepaint_ = true;
    /// Overlay currently floats above other applications (only while the host has focus).
    bool topmost_ = true;
    int hostLeft_ = 0;
    int hostTop_ = 0;
    int hostWidth_ = 0;
    int hostHeight_ = 0;
    /// Option rects from the most recent paint, in overlay client coordinates. Zero-width entries
    /// were not drawable and are therefore not clickable.
    std::vector<UiPanelBounds> optionRects_{};
    UiOverlayPointerInput pointerInput_{};
    /// Hover row already handed to the host, so an unmoved pointer is not re-reported.
    int reportedHoverOptionIndex_ = -1;
};

} // namespace ri::ui
