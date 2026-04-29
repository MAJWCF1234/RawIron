#pragma once

#include "VisualShellUiColors.h"

#include <windows.h>

#include <string>

int RectWidth(const RECT& rect);
int RectHeight(const RECT& rect);

void FillRectColor(HDC dc, const RECT& rect, COLORREF color);
/// Vertical bands approximating brushed sheet metal (highlight top, shadow bottom).
void FillRectVerticalGradient(HDC dc, const RECT& rect, COLORREF top, COLORREF bottom);
void DrawPanelDropShadow(HDC dc, const RECT& panelRect, int layers = 6);
void DrawEdgeLines(HDC dc, const RECT& rect, COLORREF topLeft, COLORREF bottomRight);
void InflateRectCopy(RECT& rect, int amount);
void DrawBeveledPanel(HDC dc, const RECT& rect, bool sunken = false);
void DrawDesktopBackground(HDC dc, const RECT& rect);
void DrawTextLine(HDC dc, const RECT& rect, const std::string& text, COLORREF color, HFONT font, UINT format);
void DrawTextLineW(HDC dc, const RECT& rect, const std::wstring& text, COLORREF color, HFONT font, UINT format);

void DrawTag(HDC dc,
             const RECT& rect,
             const std::string& text,
             COLORREF fillColor,
             COLORREF textColor,
             HFONT font,
             bool sunken = false);

HFONT CreateUiFont(int height, int weight, const wchar_t* faceName = L"MS Sans Serif");
RECT InsetRectBy(const RECT& rect, int horizontal, int vertical);
