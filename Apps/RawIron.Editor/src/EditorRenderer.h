#pragma once

#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ri::editor {

#if defined(_WIN32)
class EditorRenderer {
public:
    [[nodiscard]] static std::wstring Utf8ToWide(std::string_view utf8);
    [[nodiscard]] static std::string WideToUtf8(std::wstring_view wide);
    [[nodiscard]] static std::wstring Widen(const std::string& value);

    static void FillRectColor(HDC dc, const RECT& rect, COLORREF color);
    static void DrawPanelFrame(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow);
    static void DrawInsetFrame(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow);
    static void DrawTextLine(HDC dc, const RECT& rect, const std::string& text, COLORREF color, HFONT font, UINT format);
    static void DrawToolbarButton(HDC dc, const RECT& rect, const std::string& label, bool active, HFONT smallFont);
    static void DrawPanelHeader(HDC dc,
                                const RECT& panelRect,
                                const std::string& title,
                                HFONT headerFont,
                                HFONT smallFont,
                                const std::string& meta = {});
};
#endif

} // namespace ri::editor
