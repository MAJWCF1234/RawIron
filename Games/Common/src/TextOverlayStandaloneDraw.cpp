#include "RawIron/Games/TextOverlayStandaloneDraw.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <string>

namespace ri::games {
namespace {

void DrawCaptionPanel(HDC deviceContext,
                      const RECT& bounds,
                      const std::wstring& text,
                      COLORREF textColor,
                      COLORREF backdropColor,
                      UINT align) {
    if (text.empty()) {
        return;
    }
    HBRUSH backdrop = CreateSolidBrush(backdropColor);
    FillRect(deviceContext, &bounds, backdrop);
    DeleteObject(backdrop);

    SetBkMode(deviceContext, TRANSPARENT);
    SetTextColor(deviceContext, textColor);
    RECT textRect = bounds;
    textRect.left += 10;
    textRect.right -= 10;
    textRect.top += 6;
    textRect.bottom -= 6;
    DrawTextW(deviceContext, text.c_str(), static_cast<int>(text.size()), &textRect, align | DT_WORDBREAK);
}

std::wstring ToWide(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

} // namespace

void DrawTextOverlaySnapshot(void* const hwndVoid, const ri::world::TextOverlaySnapshot& snapshot) {
    HWND hwnd = static_cast<HWND>(hwndVoid);
    if (hwnd == nullptr || !snapshot.hudVisible) {
        return;
    }

    HDC deviceContext = GetDC(hwnd);
    if (deviceContext == nullptr) {
        return;
    }

    RECT client{};
    if (!GetClientRect(hwnd, &client)) {
        ReleaseDC(hwnd, deviceContext);
        return;
    }

    const int clientWidth = static_cast<int>(client.right - client.left);
    const int clientHeight = static_cast<int>(client.bottom - client.top);
    const int panelWidth = (std::min)(clientWidth - 48, 720);

    if (snapshot.levelNameToast.visible && !snapshot.levelNameToast.text.empty()) {
        const int panelHeight = 44;
        const RECT panel{
            client.left + (clientWidth - panelWidth) / 2,
            client.top + 24,
            client.left + (clientWidth + panelWidth) / 2,
            client.top + 24 + panelHeight,
        };
        DrawCaptionPanel(
            deviceContext,
            panel,
            ToWide(snapshot.levelNameToast.text),
            RGB(255, 244, 214),
            RGB(24, 20, 14),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (!snapshot.objectiveReadout.text.empty()) {
        const int panelHeight = 52;
        const RECT panel{
            client.left + 16,
            client.top + 16,
            client.left + 16 + (std::min)(panelWidth, 420),
            client.top + 16 + panelHeight,
        };
        std::wstring objective = L"Objective: " + ToWide(snapshot.objectiveReadout.text);
        if (!snapshot.objectiveReadout.hint.empty()) {
            objective += L"\n" + ToWide(snapshot.objectiveReadout.hint);
        }
        DrawCaptionPanel(
            deviceContext,
            panel,
            objective,
            snapshot.objectiveReadout.flashing ? RGB(255, 220, 120) : RGB(214, 230, 248),
            RGB(12, 18, 28),
            DT_LEFT);
    }

    int bottomCursor = client.bottom - 24;
    if (snapshot.messageBox.visible && !snapshot.messageBox.text.empty()) {
        const int panelHeight = 56;
        bottomCursor -= panelHeight;
        const RECT panel{
            client.left + (clientWidth - panelWidth) / 2,
            bottomCursor,
            client.left + (clientWidth + panelWidth) / 2,
            bottomCursor + panelHeight,
        };
        const COLORREF textColor = snapshot.messageBox.severity == ri::world::PresentationSeverity::Critical
            ? RGB(255, 168, 148)
            : RGB(232, 238, 246);
        DrawCaptionPanel(deviceContext, panel, ToWide(snapshot.messageBox.text), textColor, RGB(16, 18, 24), DT_CENTER);
        bottomCursor -= 8;
    }

    if (snapshot.subtitleLine.visible && !snapshot.subtitleLine.text.empty()) {
        const int panelHeight = 48;
        bottomCursor -= panelHeight;
        const RECT panel{
            client.left + (clientWidth - panelWidth) / 2,
            bottomCursor,
            client.left + (clientWidth + panelWidth) / 2,
            bottomCursor + panelHeight,
        };
        DrawCaptionPanel(
            deviceContext,
            panel,
            ToWide(snapshot.subtitleLine.text),
            RGB(248, 248, 252),
            RGB(8, 8, 12),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (snapshot.blockers.loadingVisible) {
        const int panelHeight = 40;
        const RECT panel{
            client.left + (clientWidth - panelWidth) / 2,
            client.top + clientHeight / 2 - panelHeight / 2,
            client.left + (clientWidth + panelWidth) / 2,
            client.top + clientHeight / 2 + panelHeight / 2,
        };
        std::wstring status = snapshot.blockers.loadingStatus.empty() ? L"Loading..." : ToWide(snapshot.blockers.loadingStatus);
        DrawCaptionPanel(deviceContext, panel, status, RGB(220, 228, 236), RGB(10, 12, 16), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    ReleaseDC(hwnd, deviceContext);
}

} // namespace ri::games
#endif
