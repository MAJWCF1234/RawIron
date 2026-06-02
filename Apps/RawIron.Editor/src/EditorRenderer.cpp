#include "EditorRenderer.h"

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
    DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &mutableRect, format);
    SelectObject(dc, oldFont);
}

void EditorRenderer::DrawToolbarButton(HDC dc, const RECT& rect, const std::string& label, bool active, HFONT smallFont) {
    DrawPanelFrame(
        dc,
        rect,
        active ? RGB(96, 104, 120) : RGB(74, 80, 90),
        active ? RGB(228, 234, 245) : RGB(182, 188, 198),
        active ? RGB(32, 36, 44) : RGB(40, 44, 52));
    DrawTextLine(dc,
                 rect,
                 label,
                 active ? RGB(255, 244, 195) : RGB(232, 236, 242),
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
    FillRectColor(dc, header, RGB(86, 92, 104));
    DrawTextLine(dc,
                 RECT{header.left + 10, header.top + 4, header.right - 120, header.bottom - 4},
                 title,
                 RGB(244, 244, 242),
                 headerFont,
                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    if (!meta.empty()) {
        DrawTextLine(dc,
                     RECT{header.left + 120, header.top + 4, header.right - 10, header.bottom - 4},
                     meta,
                     RGB(208, 214, 224),
                     smallFont,
                     DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }
}
#endif

} // namespace ri::editor
