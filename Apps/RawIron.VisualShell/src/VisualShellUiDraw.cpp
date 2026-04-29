#include "VisualShellUiDraw.h"
#include "VisualShellTypes.h"

#include <algorithm>

namespace pal = ri::vshell::colors;

int RectWidth(const RECT& rect) {
    return static_cast<int>(rect.right - rect.left);
}

int RectHeight(const RECT& rect) {
    return static_cast<int>(rect.bottom - rect.top);
}

void FillRectColor(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

namespace {

BYTE LerpChannel(BYTE from, BYTE to, int num, int denom) {
    if (denom <= 0) {
        return from;
    }
    return static_cast<BYTE>(from + (static_cast<int>(to) - from) * num / denom);
}

COLORREF BlendColor(COLORREF from, COLORREF to, int num, int denom) {
    return RGB(
        LerpChannel(GetRValue(from), GetRValue(to), num, denom),
        LerpChannel(GetGValue(from), GetGValue(to), num, denom),
        LerpChannel(GetBValue(from), GetBValue(to), num, denom));
}

} // namespace

void FillRectVerticalGradient(HDC dc, const RECT& rect, COLORREF top, COLORREF bottom) {
    const int h = RectHeight(rect);
    const int w = RectWidth(rect);
    if (w <= 0 || h <= 0) {
        return;
    }

    const int bands = std::clamp(h / 3, 6, 48);
    const BYTE tr = GetRValue(top);
    const BYTE tg = GetGValue(top);
    const BYTE tb = GetBValue(top);
    const BYTE br = GetRValue(bottom);
    const BYTE bg = GetGValue(bottom);
    const BYTE bb = GetBValue(bottom);

    const int denom = bands - 1;
    for (int i = 0; i < bands; ++i) {
        RECT row{
            rect.left,
            rect.top + (i * h) / bands,
            rect.right,
            rect.top + ((i + 1) * h) / bands,
        };
        const int num = denom <= 0 ? 0 : i;
        const int d = denom <= 0 ? 1 : denom;
        const COLORREF mid = RGB(
            LerpChannel(tr, br, num, d),
            LerpChannel(tg, bg, num, d),
            LerpChannel(tb, bb, num, d));
        FillRectColor(dc, row, mid);
    }
}

void DrawPanelDropShadow(HDC dc, const RECT& panelRect, int layers) {
    for (int i = layers; i >= 1; --i) {
        RECT shadow = panelRect;
        OffsetRect(&shadow, i, i);
        const int g = 3 + i * 4;
        FillRectColor(dc, shadow, RGB(g, g + 1, g + 2));
    }
}

void DrawEdgeLines(HDC dc, const RECT& rect, COLORREF topLeft, COLORREF bottomRight) {
    HPEN lightPen = CreatePen(PS_SOLID, 1, topLeft);
    HPEN darkPen = CreatePen(PS_SOLID, 1, bottomRight);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, lightPen));

    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.left, rect.top);
    LineTo(dc, rect.right - 1, rect.top);

    SelectObject(dc, darkPen);
    MoveToEx(dc, rect.left, rect.bottom - 1, nullptr);
    LineTo(dc, rect.right - 1, rect.bottom - 1);
    LineTo(dc, rect.right - 1, rect.top - 1);

    SelectObject(dc, oldPen);
    DeleteObject(lightPen);
    DeleteObject(darkPen);
}

void InflateRectCopy(RECT& rect, int amount) {
    rect.left += amount;
    rect.top += amount;
    rect.right -= amount;
    rect.bottom -= amount;
}

void DrawBeveledPanel(HDC dc, const RECT& rect, bool sunken) {
    if (sunken) {
        FillRectVerticalGradient(dc, rect, pal::kMetalSunkenTop, pal::kMetalSunkenBottom);
    } else {
        FillRectVerticalGradient(dc, rect, pal::kMetalFaceTop, pal::kMetalFaceBottom);
    }

    HPEN grainPen = CreatePen(
        PS_SOLID,
        1,
        sunken ? BlendColor(pal::kMetalSunkenBottom, pal::kDarkShadow, 1, 2)
               : BlendColor(pal::kMetalFaceTop, pal::kMetalFaceBottom, 3, 5));
    HPEN oldGrain = static_cast<HPEN>(SelectObject(dc, grainPen));
    for (int y = rect.top + 5; y < rect.bottom - 3; y += 7) {
        MoveToEx(dc, rect.left + 3, y, nullptr);
        LineTo(dc, rect.right - 3, y);
    }
    SelectObject(dc, oldGrain);
    DeleteObject(grainPen);

    const COLORREF outerTopLeft = sunken ? pal::kDarkShadow : pal::kHighlight;
    const COLORREF outerBottomRight = sunken ? pal::kHighlight : pal::kDarkShadow;
    DrawEdgeLines(dc, rect, outerTopLeft, outerBottomRight);

    RECT inner = rect;
    InflateRectCopy(inner, 1);
    const COLORREF innerTopLeft = sunken ? pal::kShadow : pal::kLightShadow;
    const COLORREF innerBottomRight = sunken ? pal::kLightShadow : pal::kShadow;
    DrawEdgeLines(dc, inner, innerTopLeft, innerBottomRight);
}

void DrawDesktopBackground(HDC dc, const RECT& rect) {
    FillRectColor(dc, rect, pal::kBackground);

    RECT desktop = rect;
    desktop.left += 10;
    desktop.top += 10;
    desktop.right -= 10;
    desktop.bottom -= 10;
    FillRectVerticalGradient(dc, desktop, pal::kDesktop, pal::kDesktopGradientBottom);

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(148, 154, 168));
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    for (int y = desktop.top; y < desktop.bottom; y += 4) {
        MoveToEx(dc, desktop.left, y, nullptr);
        LineTo(dc, desktop.right, y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawTextLine(HDC dc, const RECT& rect, const std::string& text, COLORREF color, HFONT font, UINT format) {
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    const std::wstring wide = Widen(text);
    RECT mutableRect = rect;
    DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &mutableRect, format);
    SelectObject(dc, oldFont);
}

void DrawTextLineW(HDC dc, const RECT& rect, const std::wstring& text, COLORREF color, HFONT font, UINT format) {
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    RECT mutableRect = rect;
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &mutableRect, format);
    SelectObject(dc, oldFont);
}

void DrawTag(HDC dc,
             const RECT& rect,
             const std::string& text,
             COLORREF fillColor,
             COLORREF textColor,
             HFONT font,
             bool sunken) {
    FillRectVerticalGradient(dc,
                             rect,
                             sunken ? BlendColor(fillColor, pal::kDarkShadow, 1, 3)
                                    : BlendColor(fillColor, pal::kHighlight, 1, 3),
                             sunken ? BlendColor(fillColor, pal::kLightShadow, 1, 5)
                                    : BlendColor(fillColor, pal::kDarkShadow, 1, 3));
    DrawEdgeLines(dc, rect, sunken ? pal::kShadow : pal::kHighlight, sunken ? pal::kHighlight : pal::kDarkShadow);
    RECT inner = rect;
    InflateRectCopy(inner, 1);
    DrawEdgeLines(dc, inner, sunken ? pal::kDarkShadow : pal::kLightShadow, sunken ? pal::kLightShadow : pal::kShadow);
    if (RectWidth(rect) > 12 && RectHeight(rect) > 10) {
        RECT groove{rect.left + 3, rect.top + 3, rect.left + 5, rect.bottom - 3};
        FillRectColor(dc, groove, sunken ? pal::kDarkShadow : pal::kAccentEmber);
    }
    DrawTextLine(dc, rect, text, textColor, font, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

HFONT CreateUiFont(int height, int weight, const wchar_t* faceName) {
    return CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        faceName);
}

RECT InsetRectBy(const RECT& rect, int horizontal, int vertical) {
    return RECT{
        rect.left + horizontal,
        rect.top + vertical,
        rect.right - horizontal,
        rect.bottom - vertical,
    };
}
