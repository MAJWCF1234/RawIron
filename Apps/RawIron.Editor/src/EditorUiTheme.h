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

/// Shared palette: dark utility shell, copper accents, dense authoring panels.
struct EditorUiTheme {
    static constexpr COLORREF kWindowBg = RGB(52, 54, 58);

    static constexpr COLORREF kPanelRaisedFill = RGB(66, 68, 74);
    static constexpr COLORREF kPanelRaisedHi = RGB(88, 90, 98);
    static constexpr COLORREF kPanelRaisedShadow = RGB(38, 40, 44);
    static constexpr COLORREF kSplitter = RGB(44, 46, 50);
    static constexpr COLORREF kStatusBarFill = RGB(62, 64, 70);
    static constexpr COLORREF kStatusBarHi = RGB(84, 86, 92);
    static constexpr COLORREF kStatusBarShadow = RGB(36, 38, 42);

    static constexpr COLORREF kWellFill = RGB(48, 50, 54);
    static constexpr COLORREF kWellHi = RGB(96, 98, 104);
    static constexpr COLORREF kWellShadow = RGB(24, 26, 30);
    static constexpr COLORREF kViewportWellFill = RGB(36, 38, 42);

    static constexpr COLORREF kToolStripFill = RGB(58, 60, 66);
    static constexpr COLORREF kToolStripHi = RGB(96, 100, 108);
    static constexpr COLORREF kToolStripShadow = RGB(32, 34, 38);
    static constexpr COLORREF kToolStripAccent = RGB(112, 120, 140);

    static constexpr COLORREF kTopStripe = RGB(184, 120, 48);
    static constexpr COLORREF kProjectBandFill = RGB(74, 76, 82);
    static constexpr COLORREF kProjectBandHi = RGB(104, 106, 112);
    static constexpr COLORREF kProjectBandShadow = RGB(44, 46, 50);
    static constexpr COLORREF kProjectBandAccent = RGB(208, 144, 72);

    static constexpr COLORREF kHeaderFill = RGB(56, 60, 68);
    static constexpr COLORREF kHeaderAccent = RGB(200, 136, 72);
    static constexpr COLORREF kHeaderLower = RGB(40, 44, 52);
    static constexpr COLORREF kHeaderText = RGB(248, 248, 240);
    static constexpr COLORREF kHeaderMeta = RGB(176, 180, 188);

    static constexpr COLORREF kTextOnDark = RGB(224, 226, 230);
    static constexpr COLORREF kTextOnLight = RGB(28, 30, 34);
    static constexpr COLORREF kTextOnPanel = RGB(220, 222, 228);
    static constexpr COLORREF kTextOnPanelMuted = RGB(152, 156, 164);
    static constexpr COLORREF kTextMuted = RGB(140, 144, 152);
    static constexpr COLORREF kTextGold = RGB(255, 220, 112);
    static constexpr COLORREF kTextStatusGold = RGB(255, 200, 64);

    static constexpr COLORREF kBtnLightFill = RGB(88, 90, 96);
    static constexpr COLORREF kBtnLightHi = RGB(128, 130, 138);
    static constexpr COLORREF kBtnLightShadow = RGB(48, 50, 54);
    static constexpr COLORREF kBtnLightActiveFill = RGB(72, 74, 80);
    static constexpr COLORREF kBtnLightActiveHi = RGB(104, 106, 112);
    static constexpr COLORREF kBtnLightActiveShadow = RGB(36, 38, 42);
    static constexpr COLORREF kBtnLightText = kTextOnPanel;
    static constexpr COLORREF kBtnLightActiveText = kTextOnPanel;

    static constexpr COLORREF kBtnDarkFill = RGB(72, 74, 80);
    static constexpr COLORREF kBtnDarkHi = RGB(112, 114, 122);
    static constexpr COLORREF kBtnDarkShadow = RGB(36, 38, 42);
    static constexpr COLORREF kBtnDarkActiveFill = RGB(128, 96, 48);
    static constexpr COLORREF kBtnDarkActiveHi = RGB(224, 192, 128);
    static constexpr COLORREF kBtnDarkActiveShadow = RGB(48, 32, 16);
    static constexpr COLORREF kBtnDarkText = RGB(232, 234, 238);
    static constexpr COLORREF kBtnDarkActiveText = RGB(255, 248, 224);

    static constexpr COLORREF kBtnCreatorFill = RGB(88, 76, 60);
    static constexpr COLORREF kBtnCreatorHi = RGB(168, 144, 112);
    static constexpr COLORREF kBtnCreatorShadow = RGB(40, 32, 24);
    static constexpr COLORREF kBtnCreatorActiveFill = RGB(128, 96, 64);
    static constexpr COLORREF kBtnCreatorActiveHi = RGB(240, 200, 144);
    static constexpr COLORREF kBtnCreatorActiveShadow = RGB(56, 40, 24);
    static constexpr COLORREF kBtnCreatorText = RGB(248, 236, 216);
    static constexpr COLORREF kBtnCreatorActiveText = RGB(255, 248, 232);

    static constexpr COLORREF kSelSceneFill = RGB(160, 96, 48);
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

    static constexpr COLORREF kCreatorCardFill = RGB(64, 62, 56);
    static constexpr COLORREF kCreatorCardHi = RGB(120, 116, 104);
    static constexpr COLORREF kCreatorCardShadow = RGB(32, 30, 26);
    static constexpr COLORREF kCreatorCardAccent = RGB(200, 136, 72);
    static constexpr COLORREF kCreatorSection = RGB(220, 196, 152);
    static constexpr COLORREF kCreatorBody = RGB(176, 172, 160);

    static constexpr COLORREF kMenuBarFill = RGB(58, 60, 66);
    static constexpr COLORREF kMenuBarHi = RGB(88, 90, 98);
    static constexpr COLORREF kMenuBarShadow = RGB(32, 34, 38);
    static constexpr COLORREF kMenuBarText = RGB(212, 214, 220);
    static constexpr COLORREF kPerspBorder = RGB(200, 160, 64);
    static constexpr COLORREF kPerspTitle = RGB(255, 248, 208);
    static constexpr COLORREF kQuadDivider = RGB(24, 26, 30);
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
