#pragma once

#include <windows.h>

namespace ri::vshell::colors {

/// Source/VGUI-inspired stamped metal: dark zinc, amber affordances, muted status green.
constexpr COLORREF kBackground = RGB(14, 15, 17);
constexpr COLORREF kDesktop = RGB(24, 26, 28);
constexpr COLORREF kDesktopGradientBottom = RGB(13, 14, 16);
constexpr COLORREF kPanel = RGB(49, 50, 51);
constexpr COLORREF kInset = RGB(25, 27, 29);
constexpr COLORREF kMetalFaceTop = RGB(78, 80, 82);
constexpr COLORREF kMetalFaceBottom = RGB(34, 36, 39);
constexpr COLORREF kMetalSunkenTop = RGB(18, 20, 22);
constexpr COLORREF kMetalSunkenBottom = RGB(35, 37, 39);
constexpr COLORREF kTitleBar = RGB(39, 41, 44);
constexpr COLORREF kTitleBarText = RGB(238, 233, 217);
constexpr COLORREF kText = RGB(224, 222, 211);
constexpr COLORREF kMuted = RGB(151, 149, 137);
constexpr COLORREF kSelection = RGB(112, 76, 39);
constexpr COLORREF kSelectionText = RGB(255, 242, 210);
constexpr COLORREF kReady = RGB(118, 167, 118);
constexpr COLORREF kLog = RGB(197, 196, 184);
constexpr COLORREF kMissing = RGB(212, 132, 96);
constexpr COLORREF kHighlight = RGB(156, 154, 142);
constexpr COLORREF kLightShadow = RGB(91, 92, 89);
constexpr COLORREF kShadow = RGB(33, 35, 37);
constexpr COLORREF kDarkShadow = RGB(5, 7, 9);
constexpr COLORREF kAccentEmber = RGB(229, 143, 51);
constexpr COLORREF kSourceTitleBarBg = RGB(51, 53, 56);
constexpr COLORREF kDesktopIconWell = RGB(28, 32, 36);
constexpr COLORREF kDesktopShortcutPaperTop = RGB(70, 72, 73);
constexpr COLORREF kDesktopShortcutPaperBottom = RGB(38, 40, 42);
constexpr COLORREF kMetalClear = RGB(12, 13, 15);
constexpr COLORREF kTaskbarFaceTop = RGB(49, 51, 53);
constexpr COLORREF kTaskbarFaceBottom = RGB(23, 25, 28);
constexpr COLORREF kTaskbarRim = RGB(174, 122, 53);
constexpr COLORREF kOrbFaceTop = RGB(105, 96, 80);
constexpr COLORREF kOrbFaceBottom = RGB(48, 43, 36);
constexpr COLORREF kGameCardTop = RGB(45, 47, 49);
constexpr COLORREF kGameCardBottom = RGB(24, 26, 29);
constexpr COLORREF kTagOpenFolder = RGB(37, 40, 42);
constexpr COLORREF kTagEditor = RGB(81, 62, 40);
constexpr COLORREF kStartMenuFillTop = RGB(43, 45, 47);
constexpr COLORREF kStartMenuFillBottom = RGB(20, 22, 24);
constexpr COLORREF kStartMenuRow = RGB(36, 38, 40);
constexpr COLORREF kToolbarSpacerText = RGB(176, 164, 135);

} // namespace ri::vshell::colors
