#pragma once

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

/// Shared palette: dark workshop shell, restrained copper cues, dense authoring panels.
struct EditorUiTheme {
    static constexpr COLORREF kDarkSteel = RGB(29, 32, 37); // #1D2025
    static constexpr COLORREF kCopper = RGB(176, 111, 43);  // #B06F2B
    static constexpr COLORREF kWindowBg = kDarkSteel;

    static constexpr COLORREF kPanelRaisedFill = kDarkSteel;
    static constexpr COLORREF kPanelRaisedHi = RGB(66, 71, 80);
    static constexpr COLORREF kPanelRaisedShadow = RGB(18, 20, 24);
    static constexpr COLORREF kSplitter = RGB(22, 25, 29);
    static constexpr COLORREF kStatusBarFill = kDarkSteel;
    static constexpr COLORREF kStatusBarHi = RGB(58, 63, 72);
    static constexpr COLORREF kStatusBarShadow = RGB(17, 19, 23);

    static constexpr COLORREF kWellFill = RGB(31, 35, 41);
    static constexpr COLORREF kWellHi = RGB(62, 68, 77);
    static constexpr COLORREF kWellShadow = RGB(14, 16, 19);
    static constexpr COLORREF kViewportWellFill = RGB(20, 23, 27);

    static constexpr COLORREF kToolStripFill = kDarkSteel;
    static constexpr COLORREF kToolStripHi = RGB(63, 69, 78);
    static constexpr COLORREF kToolStripShadow = RGB(17, 19, 23);
    static constexpr COLORREF kToolStripAccent = RGB(80, 89, 102);

    static constexpr COLORREF kTopStripe = kCopper;
    static constexpr COLORREF kProjectBandFill = kDarkSteel;
    static constexpr COLORREF kProjectBandHi = RGB(70, 76, 86);
    static constexpr COLORREF kProjectBandShadow = RGB(20, 22, 26);
    static constexpr COLORREF kProjectBandAccent = kCopper;

    static constexpr COLORREF kHeaderFill = kDarkSteel;
    static constexpr COLORREF kHeaderAccent = kCopper;
    static constexpr COLORREF kHeaderLower = RGB(20, 23, 27);
    static constexpr COLORREF kHeaderText = RGB(236, 238, 237);
    static constexpr COLORREF kHeaderMeta = RGB(153, 160, 170);

    static constexpr COLORREF kTextOnDark = RGB(218, 222, 226);
    static constexpr COLORREF kTextOnLight = RGB(28, 30, 34);
    static constexpr COLORREF kTextOnPanel = RGB(220, 224, 227);
    static constexpr COLORREF kTextOnPanelMuted = RGB(145, 152, 162);
    static constexpr COLORREF kTextMuted = RGB(130, 138, 149);
    static constexpr COLORREF kTextGold = RGB(232, 190, 100);
    static constexpr COLORREF kTextStatusGold = RGB(226, 170, 62);

    static constexpr COLORREF kBtnLightFill = RGB(54, 59, 68);
    static constexpr COLORREF kBtnLightHi = RGB(82, 89, 100);
    static constexpr COLORREF kBtnLightShadow = RGB(24, 27, 32);
    static constexpr COLORREF kBtnLightActiveFill = RGB(42, 47, 55);
    static constexpr COLORREF kBtnLightActiveHi = RGB(68, 75, 85);
    static constexpr COLORREF kBtnLightActiveShadow = RGB(18, 20, 24);
    static constexpr COLORREF kBtnLightText = kTextOnPanel;
    static constexpr COLORREF kBtnLightActiveText = kTextOnPanel;

    static constexpr COLORREF kBtnDarkFill = RGB(49, 54, 63);
    static constexpr COLORREF kBtnDarkHi = RGB(76, 83, 94);
    static constexpr COLORREF kBtnDarkShadow = RGB(20, 23, 27);
    static constexpr COLORREF kBtnDarkActiveFill = RGB(108, 76, 37);
    static constexpr COLORREF kBtnDarkActiveHi = RGB(191, 139, 67);
    static constexpr COLORREF kBtnDarkActiveShadow = RGB(39, 27, 15);
    static constexpr COLORREF kBtnDarkText = RGB(232, 234, 238);
    static constexpr COLORREF kBtnDarkActiveText = RGB(255, 248, 224);

    static constexpr COLORREF kBtnCreatorFill = RGB(66, 57, 46);
    static constexpr COLORREF kBtnCreatorHi = RGB(117, 96, 69);
    static constexpr COLORREF kBtnCreatorShadow = RGB(25, 21, 17);
    static constexpr COLORREF kBtnCreatorActiveFill = RGB(105, 73, 38);
    static constexpr COLORREF kBtnCreatorActiveHi = RGB(190, 137, 66);
    static constexpr COLORREF kBtnCreatorActiveShadow = RGB(39, 27, 15);
    static constexpr COLORREF kBtnCreatorText = RGB(248, 236, 216);
    static constexpr COLORREF kBtnCreatorActiveText = RGB(255, 248, 232);

    static constexpr COLORREF kSelSceneFill = RGB(124, 78, 36);
    static constexpr COLORREF kSelSceneText = RGB(255, 248, 224);
    static constexpr COLORREF kSelResourceFill = RGB(72, 96, 136);
    static constexpr COLORREF kSelResourceText = RGB(232, 240, 255);
    static constexpr COLORREF kSearchActiveFill = RGB(48, 64, 96);
    static constexpr COLORREF kSearchIdleFill = RGB(56, 58, 64);

    static constexpr COLORREF kOrthoCellFill = RGB(52, 54, 58);
    static constexpr COLORREF kOrthoTitle = RGB(232, 236, 244);
    static constexpr COLORREF kOrthoGridA = RGB(80, 82, 88);
    static constexpr COLORREF kOrthoGridB = RGB(64, 66, 72);
    static constexpr COLORREF kOrthoCrosshair = RGB(255, 255, 96);
    static constexpr COLORREF kOrthoSelBox = RGB(255, 255, 0);

    static constexpr COLORREF kCreatorCardFill = RGB(48, 45, 40);
    static constexpr COLORREF kCreatorCardHi = RGB(82, 76, 67);
    static constexpr COLORREF kCreatorCardShadow = RGB(20, 18, 15);
    static constexpr COLORREF kCreatorCardAccent = kCopper;
    static constexpr COLORREF kCreatorSection = RGB(213, 184, 132);
    static constexpr COLORREF kCreatorBody = RGB(166, 162, 153);

    static constexpr COLORREF kMenuBarFill = RGB(34, 38, 44);
    static constexpr COLORREF kMenuBarHi = RGB(57, 63, 72);
    static constexpr COLORREF kMenuBarShadow = RGB(15, 17, 20);
    static constexpr COLORREF kMenuBarText = RGB(202, 207, 213);
    static constexpr COLORREF kPerspBorder = kCopper;
    static constexpr COLORREF kPerspTitle = RGB(232, 214, 166);
    static constexpr COLORREF kQuadDivider = RGB(13, 15, 18);
    static constexpr COLORREF kTooltipFill = RGB(24, 27, 32);
};

enum class EditorToolbarStyle {
    Light,
    Dark,
    Creator,
};

enum class EditorToolMode {
    Select,
    Create,
    Camera,
};

} // namespace ri::editor
