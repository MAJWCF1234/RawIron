#include "RawIron/Ui/UiOverlayWin32.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Keep the min/max macros out of the way of std::min / std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace ri::ui {
namespace {

constexpr const wchar_t* kOverlayWindowClassName = L"RawIronUiOverlayWindow";

/// GDI needs UTF-16; manifests are UTF-8. Widening byte-by-byte mangles every non-ASCII glyph
/// (curly quotes, accents, dashes), which is most of a narrative manifest.
[[nodiscard]] std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int sourceLength = static_cast<int>(std::min<std::size_t>(text.size(), 0x7FFFFFF0U));
    const int required = MultiByteToWideChar(CP_UTF8, 0, text.data(), sourceLength, nullptr, 0);
    if (required <= 0) {
        return std::wstring(text.begin(), text.end());
    }
    std::wstring wide(static_cast<std::size_t>(required), L'\0');
    const int written = MultiByteToWideChar(CP_UTF8, 0, text.data(), sourceLength, wide.data(), required);
    wide.resize(static_cast<std::size_t>(std::max(0, written)));
    return wide;
}

[[nodiscard]] UINT AlignToDrawTextFlags(const UiTextAlign align) noexcept {
    switch (align) {
    case UiTextAlign::Center:
        return DT_CENTER;
    case UiTextAlign::Right:
        return DT_RIGHT;
    case UiTextAlign::Left:
    default:
        return DT_LEFT;
    }
}

[[nodiscard]] COLORREF ColorForStyle(const UiTextStyle style) noexcept {
    switch (style) {
    case UiTextStyle::Title:
    case UiTextStyle::Heading:
        return RGB(236, 241, 250);
    case UiTextStyle::Speaker:
        return RGB(255, 214, 138);
    case UiTextStyle::Narration:
        return RGB(186, 194, 212);
    case UiTextStyle::Note:
    case UiTextStyle::Hint:
        return RGB(152, 160, 180);
    case UiTextStyle::Body:
    case UiTextStyle::Option:
    case UiTextStyle::Separator:
    case UiTextStyle::Spacer:
    default:
        return RGB(222, 228, 242);
    }
}

/// Fonts for every style, created once per paint and released together.
class OverlayFontSet {
public:
    explicit OverlayFontSet(const int dpi) {
        fonts_[Index(UiTextStyle::Title)] = Create(UiFontPixelSize(UiTextStyle::Title, dpi), FW_SEMIBOLD, false);
        fonts_[Index(UiTextStyle::Heading)] = Create(UiFontPixelSize(UiTextStyle::Heading, dpi), FW_SEMIBOLD, false);
        fonts_[Index(UiTextStyle::Body)] = Create(UiFontPixelSize(UiTextStyle::Body, dpi), FW_NORMAL, false);
        fonts_[Index(UiTextStyle::Speaker)] = Create(UiFontPixelSize(UiTextStyle::Speaker, dpi), FW_BOLD, false);
        fonts_[Index(UiTextStyle::Narration)] = Create(UiFontPixelSize(UiTextStyle::Narration, dpi), FW_NORMAL, true);
        fonts_[Index(UiTextStyle::Note)] = Create(UiFontPixelSize(UiTextStyle::Note, dpi), FW_NORMAL, false);
        fonts_[Index(UiTextStyle::Option)] = Create(UiFontPixelSize(UiTextStyle::Option, dpi), FW_NORMAL, false);
        fonts_[Index(UiTextStyle::Hint)] = Create(UiFontPixelSize(UiTextStyle::Hint, dpi), FW_NORMAL, false);
    }

    ~OverlayFontSet() {
        for (HFONT font : fonts_) {
            if (font != nullptr) {
                DeleteObject(font);
            }
        }
    }

    OverlayFontSet(const OverlayFontSet&) = delete;
    OverlayFontSet& operator=(const OverlayFontSet&) = delete;

    [[nodiscard]] HFONT For(const UiTextStyle style) const {
        HFONT font = fonts_[Index(style)];
        return font != nullptr ? font : fonts_[Index(UiTextStyle::Body)];
    }

private:
    [[nodiscard]] static std::size_t Index(const UiTextStyle style) noexcept {
        const std::size_t raw = static_cast<std::size_t>(style);
        return raw < kStyleCount ? raw : static_cast<std::size_t>(UiTextStyle::Body);
    }

    [[nodiscard]] static HFONT Create(const int height, const int weight, const bool italic) {
        return CreateFontW(
            height, 0, 0, 0, weight, italic ? TRUE : FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
            L"Segoe UI");
    }

    static constexpr std::size_t kStyleCount = 10U;
    std::array<HFONT, kStyleCount> fonts_{};
};

LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* overlay = reinterpret_cast<UiOverlayWindowWin32*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (overlay != nullptr
        && overlay->HandleWindowMessage(
            message, static_cast<unsigned long long>(wParam), static_cast<long long>(lParam))) {
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void EnsureOverlayWindowClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = OverlayWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    // IDC_ARROW expands through the ANSI resource macro in this TU; the wide loader needs the cast.
    windowClass.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    windowClass.lpszClassName = kOverlayWindowClassName;
    RegisterClassExW(&windowClass);
    registered = true;
}

} // namespace

UiOverlayWindowWin32::~UiOverlayWindowWin32() {
    Destroy();
}

void UiOverlayWindowWin32::Bind(const UiFlowSession* session, UiFlowController* controller) {
    // Hosts rebind every frame: only an actual change may discard pending pointer input, otherwise
    // clicks gathered since the last drain would be thrown away.
    if (session_ == session && controller_ == controller) {
        return;
    }
    session_ = session;
    controller_ = controller;
    optionRects_.clear();
    pointerInput_ = UiOverlayPointerInput{};
    reportedHoverOptionIndex_ = -1;
    needsRepaint_ = true;
}

bool UiOverlayWindowWin32::SyncToHost(void* hostWindowHandle) {
    HWND hostHwnd = static_cast<HWND>(hostWindowHandle);
    if (hostHwnd == nullptr || session_ == nullptr) {
        Hide();
        return false;
    }

    EnsureOverlayWindowClass();
    HWND overlayHwnd = static_cast<HWND>(windowHandle_);
    if (overlayHwnd == nullptr) {
        overlayHwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kOverlayWindowClassName,
            L"RawIron UI Overlay",
            WS_POPUP,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr);
        if (overlayHwnd == nullptr) {
            return false;
        }
        SetWindowLongPtrW(overlayHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        windowHandle_ = overlayHwnd;
        needsRepaint_ = true;
    }

    RECT hostClient{};
    if (GetClientRect(hostHwnd, &hostClient) == 0) {
        return false;
    }
    const int width = hostClient.right - hostClient.left;
    const int height = hostClient.bottom - hostClient.top;
    // A minimized host has an off-screen origin and a degenerate client area. Following it would
    // park a topmost panel over whatever the player alt-tabbed to.
    if (IsIconic(hostHwnd) != 0 || width <= 0 || height <= 0) {
        Hide();
        return true;
    }
    POINT hostOrigin{hostClient.left, hostClient.top};
    ClientToScreen(hostHwnd, &hostOrigin);

    // Stay above other applications only while the game itself is in front; otherwise sit directly
    // on top of the host window so the overlay tracks it without hijacking the desktop.
    const HWND foreground = GetForegroundWindow();
    const bool shouldBeTopmost = foreground == hostHwnd || foreground == overlayHwnd;

    const bool geometryChanged = !visible_ || hostOrigin.x != hostLeft_ || hostOrigin.y != hostTop_
        || width != hostWidth_ || height != hostHeight_ || shouldBeTopmost != topmost_;
    if (geometryChanged) {
        hostLeft_ = hostOrigin.x;
        hostTop_ = hostOrigin.y;
        hostWidth_ = width;
        hostHeight_ = height;
        topmost_ = shouldBeTopmost;
        if (shouldBeTopmost) {
            SetWindowPos(
                overlayHwnd, HWND_TOPMOST, hostOrigin.x, hostOrigin.y, width, height,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
        } else {
            // Dropping the topmost style needs HWND_NOTOPMOST explicitly; a plain sibling insert
            // would leave WS_EX_TOPMOST set and keep the panel floating over other apps.
            SetWindowPos(
                overlayHwnd, HWND_NOTOPMOST, hostOrigin.x, hostOrigin.y, width, height,
                SWP_NOACTIVATE | SWP_SHOWWINDOW);
            SetWindowPos(
                overlayHwnd, hostHwnd, 0, 0, 0, 0,
                SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
        }
        visible_ = true;
        needsRepaint_ = true;
    }

    if (needsRepaint_) {
        needsRepaint_ = false;
        InvalidateRect(overlayHwnd, nullptr, FALSE);
        UpdateWindow(overlayHwnd);
    }
    return true;
}

void UiOverlayWindowWin32::Hide() {
    if (windowHandle_ != nullptr && visible_) {
        ShowWindow(static_cast<HWND>(windowHandle_), SW_HIDE);
    }
    visible_ = false;
    optionRects_.clear();
    pointerInput_ = UiOverlayPointerInput{};
    reportedHoverOptionIndex_ = -1;
}

void UiOverlayWindowWin32::Destroy() {
    if (windowHandle_ != nullptr) {
        SetWindowLongPtrW(static_cast<HWND>(windowHandle_), GWLP_USERDATA, 0);
        DestroyWindow(static_cast<HWND>(windowHandle_));
        windowHandle_ = nullptr;
    }
    visible_ = false;
    optionRects_.clear();
    pointerInput_ = UiOverlayPointerInput{};
    reportedHoverOptionIndex_ = -1;
}

UiOverlayPointerInput UiOverlayWindowWin32::DrainPointerInput() noexcept {
    UiOverlayPointerInput drained = pointerInput_;
    // Hover is reported once per row change. Republishing it every frame let a stationary pointer
    // reassert its row on every update, so Tab/arrow navigation snapped back under the cursor.
    if (pointerInput_.hoveredOptionIndex == reportedHoverOptionIndex_) {
        drained.hoveredOptionIndex = -1;
    } else {
        reportedHoverOptionIndex_ = pointerInput_.hoveredOptionIndex;
    }
    pointerInput_.clickedEmptySpace = false;
    pointerInput_.activatedOptionIndex = -1;
    pointerInput_.wheelMoved = false;
    return drained;
}

int UiOverlayWindowWin32::HitTestOption(const int clientX, const int clientY) const {
    for (std::size_t optionIndex = 0; optionIndex < optionRects_.size(); ++optionIndex) {
        const UiPanelBounds& rect = optionRects_[optionIndex];
        if (rect.width <= 0 || rect.height <= 0) {
            continue;
        }
        if (clientX < rect.left || clientX >= rect.left + rect.width) {
            continue;
        }
        if (clientY < rect.top || clientY >= rect.top + rect.height) {
            continue;
        }
        return static_cast<int>(optionIndex);
    }
    return -1;
}

bool UiOverlayWindowWin32::HandleWindowMessage(const unsigned int message,
                                               const unsigned long long wParam,
                                               const long long lParam) {
    (void)wParam;
    switch (message) {
    case WM_ERASEBKGND:
        return true;
    case WM_PAINT:
        Paint();
        return true;
    case WM_LBUTTONDOWN: {
        const int clickX = static_cast<int>(static_cast<short>(LOWORD(static_cast<DWORD>(lParam))));
        const int clickY = static_cast<int>(static_cast<short>(HIWORD(static_cast<DWORD>(lParam))));
        const int optionIndex = HitTestOption(clickX, clickY);
        if (optionIndex >= 0) {
            pointerInput_.activatedOptionIndex = optionIndex;
        } else {
            // Clicks that miss every row still advance VN screens; swallowing them was why
            // click-to-continue never worked while the overlay was up.
            pointerInput_.clickedEmptySpace = true;
        }
        return true;
    }
    case WM_MOUSEMOVE: {
        const int moveX = static_cast<int>(static_cast<short>(LOWORD(static_cast<DWORD>(lParam))));
        const int moveY = static_cast<int>(static_cast<short>(HIWORD(static_cast<DWORD>(lParam))));
        const int hovered = HitTestOption(moveX, moveY);
        if (hovered != pointerInput_.hoveredOptionIndex) {
            pointerInput_.hoveredOptionIndex = hovered;
            needsRepaint_ = true;
        }
        return true;
    }
    case WM_MOUSEWHEEL:
        pointerInput_.wheelMoved = true;
        return true;
    default:
        break;
    }
    return false;
}

void UiOverlayWindowWin32::Paint() {
    HWND overlayHwnd = static_cast<HWND>(windowHandle_);
    if (overlayHwnd == nullptr) {
        return;
    }

    PAINTSTRUCT paintStruct{};
    HDC windowDc = BeginPaint(overlayHwnd, &paintStruct);
    if (windowDc == nullptr) {
        return;
    }

    RECT clientRect{};
    GetClientRect(overlayHwnd, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        optionRects_.clear();
        EndPaint(overlayHwnd, &paintStruct);
        return;
    }

    // The overlay sits over a live game view and must not flicker: compose off-screen, blit once.
    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP backBuffer = nullptr;
    HGDIOBJ previousBitmap = nullptr;
    HDC dc = windowDc;
    if (memoryDc != nullptr) {
        backBuffer = CreateCompatibleBitmap(windowDc, clientWidth, clientHeight);
        if (backBuffer != nullptr) {
            previousBitmap = SelectObject(memoryDc, backBuffer);
            dc = memoryDc;
        } else {
            DeleteDC(memoryDc);
            memoryDc = nullptr;
        }
    }

    const auto present = [&]() {
        if (dc == memoryDc) {
            BitBlt(windowDc, 0, 0, clientWidth, clientHeight, memoryDc, 0, 0, SRCCOPY);
        }
        if (memoryDc != nullptr) {
            SelectObject(memoryDc, previousBitmap);
            DeleteObject(backBuffer);
            DeleteDC(memoryDc);
        }
        EndPaint(overlayHwnd, &paintStruct);
    };

    HBRUSH backdropBrush = CreateSolidBrush(RGB(6, 8, 16));
    FillRect(dc, &clientRect, backdropBrush);
    DeleteObject(backdropBrush);

    if (session_ == nullptr) {
        optionRects_.clear();
        present();
        return;
    }

    const UiPresentedScreen* cached =
        controller_ != nullptr && controller_->Presented().screen != nullptr ? &controller_->Presented() : nullptr;
    const UiPresentedScreen fallback = cached == nullptr ? PresentScreen(*session_) : UiPresentedScreen{};
    const UiPresentedScreen& presented = cached != nullptr ? *cached : fallback;

    const int savedDcState = SaveDC(dc);
    SetBkMode(dc, TRANSPARENT);

    const int dpi = std::max(96, GetDeviceCaps(windowDc, LOGPIXELSY));
    const OverlayFontSet fonts(dpi);

    const UiTextMeasureFn measure = [&dc, &fonts](const UiPresentedRow& row, const int wrapWidth) -> int {
        if (row.text.empty() || wrapWidth <= 0) {
            return 0;
        }
        const std::wstring wide = Utf8ToWide(row.text);
        SelectObject(dc, fonts.For(row.style));
        RECT calcRect{0, 0, wrapWidth, 0};
        DrawTextW(
            dc,
            wide.c_str(),
            static_cast<int>(wide.size()),
            &calcRect,
            AlignToDrawTextFlags(row.align) | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        return calcRect.bottom - calcRect.top;
    };

    const UiOverlayLayout layout = ComputeOverlayLayout(
        presented,
        UiOverlayLayoutInput{.clientWidth = clientWidth, .clientHeight = clientHeight, .dpi = dpi},
        measure);

    optionRects_ = layout.optionRects;
    if (!layout.renderable) {
        RestoreDC(dc, savedDcState);
        present();
        return;
    }

    RECT panelRect{
        layout.panel.left,
        layout.panel.top,
        layout.panel.left + layout.panel.width,
        layout.panel.top + layout.panel.height};
    HBRUSH panelBrush = CreateSolidBrush(RGB(18, 22, 34));
    FillRect(dc, &panelRect, panelBrush);
    DeleteObject(panelBrush);
    FrameRect(dc, &panelRect, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

    // Nothing may bleed outside the panel, however small the window gets.
    IntersectClipRect(dc, panelRect.left, panelRect.top, panelRect.right, panelRect.bottom);

    if (layout.titleHeight > 0 && !presented.title.empty()) {
        const std::wstring title = Utf8ToWide(presented.title);
        SelectObject(dc, fonts.For(UiTextStyle::Title));
        SetTextColor(dc, ColorForStyle(UiTextStyle::Title));
        RECT titleRect{layout.contentLeft, layout.titleTop, layout.contentRight, layout.titleTop + layout.titleHeight};
        DrawTextW(dc, title.c_str(), static_cast<int>(title.size()), &titleRect, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    }

    for (const UiOverlayRowPlacement& placement : layout.rows) {
        if (placement.rowIndex >= presented.rows.size()) {
            continue;
        }
        const UiPresentedRow& row = presented.rows[placement.rowIndex];
        if (row.style == UiTextStyle::Spacer) {
            continue;
        }
        if (row.style == UiTextStyle::Separator) {
            RECT separatorRect{
                layout.contentLeft, placement.top, layout.contentRight, placement.top + placement.height};
            FillRect(dc, &separatorRect, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
            continue;
        }
        if (row.text.empty()) {
            continue;
        }
        const std::wstring text = Utf8ToWide(row.text);
        SelectObject(dc, fonts.For(row.style));
        SetTextColor(dc, ColorForStyle(row.style));
        RECT rowRect{layout.contentLeft, placement.top, layout.contentRight, placement.top + placement.height};
        DrawTextW(
            dc,
            text.c_str(),
            static_cast<int>(text.size()),
            &rowRect,
            AlignToDrawTextFlags(row.align) | DT_WORDBREAK | DT_NOPREFIX);
    }

    if (layout.rowsTruncated) {
        SelectObject(dc, fonts.For(UiTextStyle::Note));
        SetTextColor(dc, ColorForStyle(UiTextStyle::Note));
        const int markerTop = std::max(layout.titleTop, layout.hintTop - layout.hintHeight);
        RECT moreRect{layout.contentLeft, markerTop, layout.contentRight, markerTop + layout.hintHeight};
        DrawTextW(dc, L"...", -1, &moreRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    }

    const std::size_t selectedOption = controller_ != nullptr ? controller_->SelectedOption() : 0U;
    SelectObject(dc, fonts.For(UiTextStyle::Option));
    for (std::size_t optionIndex = 0; optionIndex < layout.optionRects.size(); ++optionIndex) {
        const UiPanelBounds& bounds = layout.optionRects[optionIndex];
        if (bounds.width <= 0 || bounds.height <= 0 || optionIndex >= presented.options.size()) {
            continue;
        }
        RECT optionRect{bounds.left, bounds.top, bounds.left + bounds.width, bounds.top + bounds.height};
        if (optionIndex == selectedOption) {
            HBRUSH selectedBrush = CreateSolidBrush(RGB(36, 58, 96));
            FillRect(dc, &optionRect, selectedBrush);
            DeleteObject(selectedBrush);
            FrameRect(dc, &optionRect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            SetTextColor(dc, RGB(255, 255, 255));
        } else {
            SetTextColor(dc, RGB(214, 221, 236));
        }
        const std::wstring label =
            std::to_wstring(optionIndex + 1U) + L".  " + Utf8ToWide(presented.options[optionIndex].label);
        RECT labelRect{
            optionRect.left + (layout.unit * 2), optionRect.top, optionRect.right - layout.unit, optionRect.bottom};
        DrawTextW(
            dc,
            label.c_str(),
            static_cast<int>(label.size()),
            &labelRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    const std::wstring hint = Utf8ToWide(UiOverlayHintText(presented));
    SelectObject(dc, fonts.For(UiTextStyle::Hint));
    SetTextColor(dc, ColorForStyle(UiTextStyle::Hint));
    RECT hintRect{layout.contentLeft, layout.hintTop, layout.contentRight, layout.hintTop + layout.hintHeight};
    DrawTextW(
        dc,
        hint.c_str(),
        static_cast<int>(hint.size()),
        &hintRect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RestoreDC(dc, savedDcState);
    present();
}

} // namespace ri::ui

#endif // _WIN32
