#pragma once

#include <cstddef>
#include <windows.h>

constexpr int kLauncherIconShell = 12;

void FillLauncherIconWell(HDC dc, const RECT& well, int iconIndex, bool ready);
void DrawLauncherGlyph(HDC dc, const RECT& well, int iconIndex, bool ready);
void FillGameThumbnailWell(HDC dc, const RECT& thumb, std::size_t gameIndex);
void DrawGamePackageGlyph(HDC dc, const RECT& thumb, std::size_t gameIndex);
