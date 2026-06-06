#include "EditorRenderer.h"

#include <algorithm>

namespace ri::editor {

#if defined(_WIN32)
std::wstring EditorRenderer::Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int count =
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), count);
    return wide;
}

std::string EditorRenderer::WideToUtf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int count =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        wide.data(),
                        static_cast<int>(wide.size()),
                        utf8.data(),
                        count,
                        nullptr,
                        nullptr);
    return utf8;
}

std::wstring EditorRenderer::Widen(const std::string& value) {
    return Utf8ToWide(value);
}

void EditorRenderer::FillRectColor(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void EditorRenderer::DrawPanelFrame(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow) {
    FillRectColor(dc, rect, fill);
    HPEN highlightPen = CreatePen(PS_SOLID, 1, highlight);
    HPEN shadowPen = CreatePen(PS_SOLID, 1, shadow);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, highlightPen));

    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.left, rect.top);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, shadowPen);
    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.right - 1, rect.bottom - 1);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, oldPen);
    DeleteObject(highlightPen);
    DeleteObject(shadowPen);

    const RECT inner{
        rect.left + 1,
        rect.top + 1,
        std::max(rect.left + 1, rect.right - 1),
        std::max(rect.top + 1, rect.bottom - 1),
    };
    if (inner.right > inner.left && inner.bottom > inner.top) {
        HPEN innerPen = CreatePen(PS_SOLID, 1, RGB(28, 33, 40));
        oldPen = static_cast<HPEN>(SelectObject(dc, innerPen));
        MoveToEx(dc, inner.left, inner.bottom - 1, nullptr);
        LineTo(dc, inner.right - 1, inner.bottom - 1);
        LineTo(dc, inner.right - 1, inner.top);
        SelectObject(dc, oldPen);
        DeleteObject(innerPen);
    }
}

void EditorRenderer::DrawInsetFrame(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow) {
    FillRectColor(dc, rect, fill);
    HPEN shadowPen = CreatePen(PS_SOLID, 1, shadow);
    HPEN highlightPen = CreatePen(PS_SOLID, 1, highlight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, shadowPen));

    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.left, rect.top);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, highlightPen);
    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.right - 1, rect.bottom - 1);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, oldPen);
    DeleteObject(shadowPen);
    DeleteObject(highlightPen);
}

void EditorRenderer::DrawTextLine(HDC dc, const RECT& rect, const std::string& text, COLORREF color, HFONT font, UINT format) {
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    const std::wstring wide = Widen(text);
    RECT mutableRect = rect;
    DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &mutableRect, format | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

void EditorRenderer::DrawToolbarButton(HDC dc, const RECT& rect, const std::string& label, bool active, HFONT smallFont) {
    const COLORREF fill = active ? RGB(128, 96, 42) : RGB(58, 65, 76);
    const COLORREF highlight = active ? RGB(255, 227, 162) : RGB(150, 160, 174);
    const COLORREF shadow = active ? RGB(60, 42, 18) : RGB(24, 28, 34);
    DrawPanelFrame(dc, rect, fill, highlight, shadow);

    const RECT accent{
        rect.left + 1,
        rect.top + 1,
        rect.right - 1,
        std::min(rect.bottom - 1, rect.top + 4),
    };
    if (accent.bottom > accent.top) {
        FillRectColor(dc, accent, active ? RGB(255, 196, 96) : RGB(86, 98, 114));
    }

    const RECT baseGlow{
        rect.left + 2,
        std::max(rect.top + 2, rect.bottom - 5),
        rect.right - 2,
        rect.bottom - 2,
    };
    if (baseGlow.bottom > baseGlow.top) {
        FillRectColor(dc, baseGlow, active ? RGB(96, 70, 28) : RGB(48, 54, 64));
    }

    DrawTextLine(dc,
                 rect,
                 label,
                 active ? RGB(255, 247, 214) : RGB(235, 239, 244),
                 smallFont,
                 DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

void EditorRenderer::DrawPanelHeader(HDC dc,
                                     const RECT& panelRect,
                                     const std::string& title,
                                     HFONT headerFont,
                                     HFONT smallFont,
                                     const std::string& meta) {
    RECT header{panelRect.left + 2, panelRect.top + 2, panelRect.right - 2, panelRect.top + 30};
    FillRectColor(dc, header, RGB(46, 53, 63));
    const RECT accent{header.left, header.top, header.right, std::min(header.bottom, header.top + 4)};
    FillRectColor(dc, accent, RGB(204, 145, 60));
    const RECT lowerAccent{header.left, header.bottom - 2, header.right, header.bottom};
    FillRectColor(dc, lowerAccent, RGB(34, 38, 46));
    DrawTextLine(dc,
                 RECT{header.left + 10, header.top + 4, header.right - 120, header.bottom - 4},
                 title,
                 RGB(246, 247, 243),
                 headerFont,
                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    if (!meta.empty()) {
        DrawTextLine(dc,
                     RECT{header.left + 120, header.top + 4, header.right - 10, header.bottom - 4},
                     meta,
                     RGB(214, 220, 228),
                     smallFont,
                     DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }
}
#endif

} // namespace ri::editor
