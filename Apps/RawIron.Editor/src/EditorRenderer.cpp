#include "EditorRenderer.h"

#include <algorithm>
#include <cstring>
#include <vector>

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
        HPEN innerPen = CreatePen(PS_SOLID, 1, EditorUiTheme::kWellShadow);
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

void EditorRenderer::DrawToolbarButton(HDC dc,
                                       const RECT& rect,
                                       const std::string& label,
                                       const bool active,
                                       HFONT smallFont,
                                       const EditorToolbarStyle style) {
    COLORREF fill = EditorUiTheme::kBtnDarkFill;
    COLORREF highlight = EditorUiTheme::kBtnDarkHi;
    COLORREF shadow = EditorUiTheme::kBtnDarkShadow;
    COLORREF text = EditorUiTheme::kBtnDarkText;
    if (active) {
        fill = EditorUiTheme::kBtnDarkActiveFill;
        highlight = EditorUiTheme::kBtnDarkActiveShadow;
        shadow = EditorUiTheme::kBtnDarkActiveHi;
        text = EditorUiTheme::kBtnDarkActiveText;
    }
    switch (style) {
        case EditorToolbarStyle::Light:
            fill = active ? EditorUiTheme::kBtnLightActiveFill : EditorUiTheme::kBtnLightFill;
            highlight = active ? EditorUiTheme::kBtnLightActiveShadow : EditorUiTheme::kBtnLightHi;
            shadow = active ? EditorUiTheme::kBtnLightActiveHi : EditorUiTheme::kBtnLightShadow;
            text = EditorUiTheme::kBtnLightText;
            break;
        case EditorToolbarStyle::Creator:
            fill = active ? EditorUiTheme::kBtnCreatorActiveFill : EditorUiTheme::kBtnCreatorFill;
            highlight = active ? EditorUiTheme::kBtnCreatorActiveShadow : EditorUiTheme::kBtnCreatorHi;
            shadow = active ? EditorUiTheme::kBtnCreatorActiveHi : EditorUiTheme::kBtnCreatorShadow;
            text = active ? EditorUiTheme::kBtnCreatorActiveText : EditorUiTheme::kBtnCreatorText;
            break;
        case EditorToolbarStyle::Dark:
            break;
    }

    if (active) {
        DrawInsetFrame(dc, rect, fill, shadow, highlight);
    } else {
        DrawPanelFrame(dc, rect, fill, highlight, shadow);
    }

    if (style == EditorToolbarStyle::Creator && !active) {
        const RECT accent{rect.left + 1, rect.top + 1, rect.right - 1, std::min(rect.bottom - 1, rect.top + 3)};
        if (accent.bottom > accent.top) {
            FillRectColor(dc, accent, EditorUiTheme::kCreatorCardAccent);
        }
    }

    DrawTextLine(dc, rect, label, text, smallFont, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

void EditorRenderer::DrawPanelHeader(HDC dc,
                                     const RECT& panelRect,
                                     const std::string& title,
                                     HFONT headerFont,
                                     HFONT smallFont,
                                     const std::string& meta,
                                     const bool showCollapseToggle,
                                     const bool collapsed,
                                     RECT* collapseToggleRectOut) {
    RECT header{panelRect.left + 2, panelRect.top + 2, panelRect.right - 2, panelRect.top + 30};
    DrawPanelFrame(dc, header, EditorUiTheme::kHeaderFill, EditorUiTheme::kHeaderAccent, EditorUiTheme::kHeaderLower);
    const RECT accent{header.left + 1, header.top + 1, header.right - 1, std::min(header.bottom, header.top + 3)};
    FillRectColor(dc, accent, EditorUiTheme::kHeaderAccent);

    const int toggleReserve = showCollapseToggle ? 34 : 0;
    if (showCollapseToggle && collapseToggleRectOut != nullptr) {
        *collapseToggleRectOut = RECT{header.right - 30, header.top + 4, header.right - 6, header.bottom - 4};
        DrawToolbarButton(dc, *collapseToggleRectOut, collapsed ? "»" : "«", false, smallFont, EditorToolbarStyle::Dark);
    }

    DrawTextLine(dc,
                 RECT{header.left + 10, header.top + 4, header.right - 120 - toggleReserve, header.bottom - 4},
                 title,
                 EditorUiTheme::kHeaderText,
                 headerFont,
                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    if (!meta.empty()) {
        DrawTextLine(dc,
                     RECT{header.left + 120, header.top + 4, header.right - 10 - toggleReserve, header.bottom - 4},
                     meta,
                     EditorUiTheme::kHeaderMeta,
                     smallFont,
                     DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

RECT EditorRenderer::InsetRect(const RECT& rect, const int amount) {
    return RECT{
        rect.left + amount,
        rect.top + amount,
        std::max(rect.left + amount, rect.right - amount),
        std::max(rect.top + amount, rect.bottom - amount),
    };
}

void EditorRenderer::BlitSoftwareImage(HDC dc,
                                       const RECT& target,
                                       const ri::render::software::SoftwareImage& image) {
    const int targetWidth = std::max(0, static_cast<int>(target.right - target.left));
    const int targetHeight = std::max(0, static_cast<int>(target.bottom - target.top));
    if (targetWidth <= 0 || targetHeight <= 0) {
        return;
    }
    if (image.pixels.empty() || image.width <= 0 || image.height <= 0) {
        FillRectColor(dc, target, EditorUiTheme::kViewportWellFill);
        return;
    }

    const int rowBytes = image.width * 3;
    const int dibStride = ((rowBytes + 3) / 4) * 4;
    const std::uint8_t* bits = image.pixels.data();
    std::vector<std::uint8_t> paddedRows;
    if (dibStride != rowBytes) {
        paddedRows.resize(static_cast<std::size_t>(dibStride * image.height));
        for (int y = 0; y < image.height; ++y) {
            std::memcpy(paddedRows.data() + static_cast<std::size_t>(y * dibStride),
                        image.pixels.data() + static_cast<std::size_t>(y * rowBytes),
                        static_cast<std::size_t>(rowBytes));
        }
        bits = paddedRows.data();
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = image.width;
    bitmapInfo.bmiHeader.biHeight = -image.height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 24;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(dc,
                  target.left,
                  target.top,
                  targetWidth,
                  targetHeight,
                  0,
                  0,
                  image.width,
                  image.height,
                  bits,
                  &bitmapInfo,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}
#endif

} // namespace ri::editor
