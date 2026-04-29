#include "VisualShellLauncherArt.h"
#include "VisualShellUiDraw.h"

#include <algorithm>

static COLORREF LauncherAccentStripe(int iconIndex, bool ready) {
    static const COLORREF kAccents[] = {
        RGB(205, 143, 66),
        RGB(226, 128, 44),
        RGB(126, 150, 170),
        RGB(116, 164, 105),
        RGB(194, 142, 62),
        RGB(155, 158, 152),
        RGB(185, 130, 54),
        RGB(126, 158, 152),
        RGB(176, 166, 140),
        RGB(221, 120, 58),
        RGB(142, 134, 156),
        RGB(210, 156, 72),
    };
    COLORREF acc =
        (iconIndex >= 0 && iconIndex < 12) ? kAccents[iconIndex] : RGB(100, 180, 130);
    if (!ready) {
        acc =
            RGB(GetRValue(acc) * 3 / 5 + 25, GetGValue(acc) * 3 / 5 + 25, GetBValue(acc) * 3 / 5 + 28);
    }
    return acc;
}

static COLORREF DimColor(COLORREF color, int num = 3, int denom = 5) {
    return RGB(GetRValue(color) * num / denom,
               GetGValue(color) * num / denom,
               GetBValue(color) * num / denom);
}

static void DrawRivet(HDC dc, int x, int y, COLORREF accent, bool ready) {
    const COLORREF dark = ready ? RGB(12, 14, 16) : RGB(18, 19, 20);
    RECT pit{x - 1, y - 1, x + 2, y + 2};
    FillRectColor(dc, pit, dark);
    RECT glint{x, y - 1, x + 1, y};
    FillRectColor(dc, glint, ready ? DimColor(accent, 4, 5) : RGB(82, 80, 72));
}

static void DrawGlyphBars(HDC dc, int x, int bottom, int barW, int gap, const int* heights, int count) {
    for (int i = 0; i < count; ++i) {
        Rectangle(dc,
                  x + i * (barW + gap),
                  bottom - heights[i],
                  x + i * (barW + gap) + barW,
                  bottom);
    }
}

void FillLauncherIconWell(HDC dc, const RECT& well, int iconIndex, bool ready) {
    const COLORREF accent = LauncherAccentStripe(iconIndex, ready);
    FillRectVerticalGradient(dc, well, ready ? RGB(54, 56, 56) : RGB(38, 39, 39),
                             ready ? RGB(18, 20, 21) : RGB(23, 24, 25));
    DrawEdgeLines(dc, well, ready ? RGB(145, 139, 122) : RGB(78, 78, 72), RGB(5, 6, 7));

    RECT stripe{well.left, well.top, well.right, well.top + 2};
    FillRectColor(dc, stripe, accent);

    RECT tray{well.left + 5, well.top + 6, well.right - 5, well.bottom - 5};
    FillRectVerticalGradient(dc, tray, ready ? RGB(34, 37, 38) : RGB(29, 31, 32),
                             ready ? RGB(19, 21, 23) : RGB(22, 23, 24));
    DrawEdgeLines(dc, tray, RGB(82, 82, 76), RGB(7, 9, 10));

    RECT leftRail{tray.left + 1, tray.top + 2, tray.left + 3, tray.bottom - 2};
    FillRectColor(dc, leftRail, DimColor(accent, ready ? 4 : 3, 5));

    HPEN grain = CreatePen(PS_SOLID, 1, ready ? RGB(43, 45, 44) : RGB(34, 35, 35));
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, grain));
    for (int y = tray.top + 5; y < tray.bottom - 3; y += 6) {
        MoveToEx(dc, tray.left + 6, y, nullptr);
        LineTo(dc, tray.right - 5, y);
    }
    SelectObject(dc, oldPen);
    DeleteObject(grain);

    DrawRivet(dc, well.left + 5, well.top + 5, accent, ready);
    DrawRivet(dc, well.right - 6, well.top + 5, accent, ready);
    DrawRivet(dc, well.left + 5, well.bottom - 6, accent, ready);
    DrawRivet(dc, well.right - 6, well.bottom - 6, accent, ready);
}

void DrawLauncherGlyph(HDC dc, const RECT& well, int iconIndex, bool ready) {
    const COLORREF ink = ready ? RGB(235, 226, 203) : RGB(122, 124, 118);
    const COLORREF accent = LauncherAccentStripe(iconIndex, ready);
    HPEN pen = CreatePen(PS_SOLID, 2, ink);
    HPEN finePen = CreatePen(PS_SOLID, 1, ready ? RGB(176, 166, 142) : RGB(88, 90, 86));
    HPEN accentPen = CreatePen(PS_SOLID, 2, accent);
    HPEN shadowPen = CreatePen(PS_SOLID, 3, RGB(6, 7, 8));
    HBRUSH accentBrush = CreateSolidBrush(DimColor(accent, ready ? 4 : 3, 5));
    HBRUSH darkBrush = CreateSolidBrush(RGB(13, 15, 16));
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(dc, GetStockObject(NULL_BRUSH)));

    const int L = well.left;
    const int T = well.top;
    const int R = well.right;
    const int B = well.bottom;
    const int cx = (L + R) / 2;
    const int cy = (T + B) / 2;
    const int w = (std::max)(8, RectWidth(well));
    const int h = (std::max)(8, RectHeight(well));
    const int u = (std::max)(1, w / 12);

    switch (iconIndex) {
        case 0: {
            SelectObject(dc, shadowPen);
            MoveToEx(dc, cx - w / 4 + 1, cy + h / 5 + 1, nullptr);
            LineTo(dc, cx + w / 8 + 1, cy - h / 5 + 1);
            SelectObject(dc, pen);
            MoveToEx(dc, cx - w / 4, cy + h / 5, nullptr);
            LineTo(dc, cx + w / 8, cy - h / 5);
            SelectObject(dc, accentPen);
            RoundRect(dc, cx + w / 12, cy - h / 3, cx + w / 3, cy - h / 12, 3, 3);
            SelectObject(dc, finePen);
            Rectangle(dc, cx - w / 12, cy - h / 16, cx + w / 10, cy + h / 10);
            break;
        }
        case 1: {
            POINT flame[] = {
                {cx, cy + h / 5},
                {cx - w / 7, cy - h / 12},
                {cx - w / 12, cy - h / 4},
                {cx + w / 20, cy - h / 10},
                {cx + w / 8, cy - h / 4},
                {cx + w / 7, cy - h / 16},
            };
            SelectObject(dc, accentPen);
            Polygon(dc, flame, 6);
            SelectObject(dc, pen);
            MoveToEx(dc, cx - w / 4, cy + h / 4, nullptr);
            LineTo(dc, cx + w / 4, cy + h / 4);
            MoveToEx(dc, cx - w / 7, cy + h / 6, nullptr);
            LineTo(dc, cx + w / 7, cy + h / 6);
            break;
        }
        case 2: {
            SelectObject(dc, finePen);
            Rectangle(dc, cx - w / 4, cy - h / 5, cx - w / 20, cy + h / 20);
            Rectangle(dc, cx + w / 20, cy - h / 12, cx + w / 4, cy + h / 5);
            SelectObject(dc, accentPen);
            MoveToEx(dc, cx - w / 20, cy - h / 16, nullptr);
            LineTo(dc, cx + w / 20, cy);
            MoveToEx(dc, cx - w / 4, cy + h / 4, nullptr);
            LineTo(dc, cx + w / 4, cy + h / 4);
            break;
        }
        case 3: {
            POINT shield[] = {
                {cx, T + h / 8},
                {R - w / 6, cy - h / 10},
                {R - w / 6, cy + h / 8},
                {cx, B - h / 6},
                {L + w / 6, cy + h / 8},
                {L + w / 6, cy - h / 10},
            };
            SelectObject(dc, accentPen);
            Polygon(dc, shield, 6);
            SelectObject(dc, pen);
            MoveToEx(dc, cx - w / 10, cy, nullptr);
            LineTo(dc, cx + w / 10, cy);
            MoveToEx(dc, cx, cy - h / 12, nullptr);
            LineTo(dc, cx, cy + h / 12);
            break;
        }
        case 4: {
            SelectObject(dc, pen);
            RoundRect(dc, cx - w / 3, cy - h / 6, cx + w / 3, cy + h / 5, 6, 6);
            MoveToEx(dc, L + w / 5, cy, nullptr);
            LineTo(dc, L + w / 3, cy);
            MoveToEx(dc, L + w / 4, cy - h / 12, nullptr);
            LineTo(dc, L + w / 4, cy + h / 12);
            SelectObject(dc, accentBrush);
            Ellipse(dc, R - w / 4, cy - h / 10, R - w / 6, cy);
            Ellipse(dc, cx + w / 9, cy, cx + w / 5, cy + h / 12);
            SelectObject(dc, GetStockObject(NULL_BRUSH));
            break;
        }
        case 5: {
            POINT folder[] = {
                {L + w / 8, cy - h / 10},
                {L + w / 5, T + h / 7},
                {cx - w / 15, T + h / 7},
                {cx + w / 20, cy - h / 10},
                {R - w / 8, T + h / 7},
                {R - w / 8, B - h / 7},
                {L + w / 8, B - h / 7},
            };
            SelectObject(dc, pen);
            Polygon(dc, folder, 7);
            SelectObject(dc, accentPen);
            MoveToEx(dc, L + w / 8, cy - h / 18, nullptr);
            LineTo(dc, R - w / 8, cy - h / 18);
            break;
        }
        case 6: {
            SelectObject(dc, pen);
            Rectangle(dc, cx - w / 5, cy - h / 8, cx + w / 7, cy + h / 7);
            MoveToEx(dc, cx + w / 7, cy - h / 16, nullptr);
            LineTo(dc, cx + w / 4, cy - h / 16);
            MoveToEx(dc, cx + w / 7, cy + h / 16, nullptr);
            LineTo(dc, cx + w / 4, cy + h / 16);
            SelectObject(dc, accentPen);
            MoveToEx(dc, cx - w / 5, cy, nullptr);
            LineTo(dc, cx - w / 3, cy);
            MoveToEx(dc, cx - w / 3, cy, nullptr);
            LineTo(dc, cx - w / 3, cy - h / 7);
            break;
        }
        case 7: {
            SelectObject(dc, pen);
            Rectangle(dc, cx - w / 4, cy - h / 5, cx + w / 4, cy + h / 6);
            SelectObject(dc, accentPen);
            MoveToEx(dc, cx - w / 7, cy - h / 14, nullptr);
            LineTo(dc, cx - w / 20, cy);
            LineTo(dc, cx - w / 7, cy + h / 14);
            SelectObject(dc, finePen);
            MoveToEx(dc, cx, cy + h / 12, nullptr);
            LineTo(dc, cx + w / 7, cy + h / 12);
            break;
        }
        case 8: {
            SelectObject(dc, pen);
            Ellipse(dc, cx - w / 5, cy - h / 5, cx + w / 5, cy + h / 5);
            MoveToEx(dc, cx, cy - h / 4, nullptr);
            LineTo(dc, cx, cy + h / 4);
            MoveToEx(dc, cx - w / 4, cy, nullptr);
            LineTo(dc, cx + w / 4, cy);
            MoveToEx(dc, cx - w / 5, cy - h / 5, nullptr);
            LineTo(dc, cx + w / 5, cy + h / 5);
            MoveToEx(dc, cx + w / 5, cy - h / 5, nullptr);
            LineTo(dc, cx - w / 5, cy + h / 5);
            SelectObject(dc, accentPen);
            Ellipse(dc, cx - u, cy - u, cx + u, cy + u);
            break;
        }
        case 9: {
            SelectObject(dc, pen);
            MoveToEx(dc, cx - w / 5, cy + h / 5, nullptr);
            LineTo(dc, cx + w / 5, cy - h / 5);
            MoveToEx(dc, cx - w / 6, cy - h / 5, nullptr);
            LineTo(dc, cx + w / 6, cy + h / 5);
            SelectObject(dc, accentPen);
            Rectangle(dc, cx + w / 7, cy - h / 4, cx + w / 4, cy - h / 8);
            break;
        }
        case 10: {
            SelectObject(dc, pen);
            Rectangle(dc, cx - w / 4, cy - h / 5, cx + w / 5, cy + h / 5);
            SelectObject(dc, finePen);
            for (int i = 0; i < 3; ++i) {
                const int px = cx - w / 6 + i * w / 7;
                MoveToEx(dc, px, cy + h / 5, nullptr);
                LineTo(dc, px, cy + h / 3);
            }
            SelectObject(dc, accentPen);
            MoveToEx(dc, cx + w / 5, cy - h / 8, nullptr);
            LineTo(dc, cx + w / 3, cy - h / 8);
            MoveToEx(dc, cx + w / 5, cy + h / 8, nullptr);
            LineTo(dc, cx + w / 3, cy + h / 8);
            break;
        }
        case 11: {
            const int bw = (std::max)(2, w / 15);
            const int bh = (std::max)(4, h / 8);
            const int bx0 = cx - w / 5;
            const int heights[] = {bh, bh + h / 10, bh + h / 5};
            SelectObject(dc, pen);
            DrawGlyphBars(dc, bx0, cy + h / 4, bw * 2, bw, heights, 3);
            SelectObject(dc, accentPen);
            Arc(dc, cx - w / 4, cy - h / 4, cx + w / 4, cy + h / 4,
                cx - w / 5, cy, cx + w / 5, cy - h / 6);
            break;
        }
        case kLauncherIconShell: {
            SelectObject(dc, pen);
            MoveToEx(dc, L + w / 7, cy - h / 10, nullptr);
            LineTo(dc, L + w / 7, cy + h / 8);
            MoveToEx(dc, L + w / 6, cy - h / 12, nullptr);
            LineTo(dc, R - w / 6, cy - h / 12);
            SelectObject(dc, accentPen);
            MoveToEx(dc, L + w / 5, cy + h / 6, nullptr);
            LineTo(dc, R - w / 6, cy + h / 6);
            break;
        }
        default: {
            Ellipse(dc, cx - w / 8, cy - h / 8, cx + w / 8, cy + h / 8);
            break;
        }
    }

    SelectObject(dc, oldBr);
    SelectObject(dc, oldPen);
    DeleteObject(darkBrush);
    DeleteObject(accentBrush);
    DeleteObject(shadowPen);
    DeleteObject(accentPen);
    DeleteObject(finePen);
    DeleteObject(pen);
}

void FillGameThumbnailWell(HDC dc, const RECT& thumb, std::size_t gameIndex) {
    static const COLORREF kBases[] = {
        RGB(56, 45, 34),
        RGB(42, 50, 58),
        RGB(43, 56, 43),
        RGB(58, 48, 40),
        RGB(42, 54, 56),
    };
    COLORREF base = kBases[gameIndex % 5];
    const int h = RectHeight(thumb);
    const int h2 = (std::max)(1, h / 2);
    RECT top{thumb.left, thumb.top, thumb.right, thumb.top + h2};
    RECT bot{thumb.left, thumb.top + h2, thumb.right, thumb.bottom};
    FillRectColor(dc, top, RGB(GetRValue(base) + 14, GetGValue(base) + 14, GetBValue(base) + 14));
    FillRectColor(dc, bot, base);
    RECT st{thumb.left, thumb.top, thumb.right, thumb.top + 2};
    FillRectColor(dc, st, RGB(211, 135, 55));
}

void DrawGamePackageGlyph(HDC dc, const RECT& thumb, std::size_t gameIndex) {
    (void)gameIndex;
    const COLORREF ink = RGB(224, 214, 190);
    HPEN pen = CreatePen(PS_SOLID, 2, ink);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(dc, GetStockObject(NULL_BRUSH)));

    const int cx = (thumb.left + thumb.right) / 2;
    const int cy = (thumb.top + thumb.bottom) / 2;
    const int rw = RectWidth(thumb) / 3;
    const int rh = RectHeight(thumb) / 3;
    Ellipse(dc, cx - rw, cy - rh, cx + rw, cy + rh);
    Ellipse(dc, cx - rw / 3, cy - rh / 3, cx + rw / 3, cy + rh / 3);
    const int px = cx + rw / 3;
    const int py = cy + rh / 4;
    MoveToEx(dc, px - 4, py + 2, nullptr);
    LineTo(dc, px + 8, py + 2);
    LineTo(dc, px + 4, py - 6);
    LineTo(dc, px - 4, py + 2);

    SelectObject(dc, oldBr);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}
