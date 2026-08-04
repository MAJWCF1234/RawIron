#pragma once

/// Presenter-agnostic presentation layer for JSON UI manifests.
///
/// Games and tools never re-implement screen flattening, wrapping, or panel layout: they call
/// `PresentScreen` to turn the live `UiFlowSession` into resolved rows + options, then
/// `ComputeOverlayLayout` to place them for a given viewport. Nothing here touches a graphics API,
/// so it is unit-testable headless and shared by every backend (GDI overlay, ImGui, future Vulkan).

#include "RawIron/Ui/UiFlowSession.h"
#include "RawIron/Ui/UiLayout.h"
#include "RawIron/Ui/UiManifest.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::ui {

enum class UiTextStyle : std::uint8_t {
    /// Screen title banner.
    Title = 0,
    Heading = 1,
    Body = 2,
    /// `say` speaker name.
    Speaker = 3,
    Narration = 4,
    /// `historyNote` / de-emphasized footnote.
    Note = 5,
    Separator = 6,
    Spacer = 7,
    /// Interactive option row.
    Option = 8,
    /// Bottom-of-panel input legend.
    Hint = 9,
};

enum class UiTextAlign : std::uint8_t {
    Left = 0,
    Center = 1,
    Right = 2,
};

/// One drawable row of a screen with `${var}` interpolation already applied.
struct UiPresentedRow {
    std::string text{};
    UiTextStyle style = UiTextStyle::Body;
    UiTextAlign align = UiTextAlign::Left;
    /// Extra leading gap, in multiples of the layout base unit.
    int spacingAboveUnits = 0;
    /// `Spacer` rows only: requested height in manifest (96 dpi) pixels.
    float requestedHeightPixels = 0.0f;
};

/// An activatable row (manifest `button`, or one entry of a `choices` block).
struct UiPresentedOption {
    std::string label{};
    UiAction action{};
};

/// Everything a presenter needs about the session's current screen.
struct UiPresentedScreen {
    const UiScreen* screen = nullptr;
    std::string title{};
    std::vector<UiPresentedRow> rows{};
    std::vector<UiPresentedOption> options{};
    /// True when the screen defines an advance action (VN click/space-to-continue).
    bool advanceAvailable = false;
};

/// Flattens the session's current screen: resolves `${var}`, drops blocks hidden by `visibleWhen`,
/// separates interactive options from drawable rows.
[[nodiscard]] UiPresentedScreen PresentScreen(const UiFlowSession& session);

/// Recommended font size in physical pixels for a style at the given DPI.
[[nodiscard]] int UiFontPixelSize(UiTextStyle style, int dpi) noexcept;

struct UiOverlayLayoutInput {
    int clientWidth = 0;
    int clientHeight = 0;
    int dpi = 96;
};

/// A row placed inside the panel. `rowIndex` indexes `UiPresentedScreen::rows`.
struct UiOverlayRowPlacement {
    std::size_t rowIndex = 0U;
    int top = 0;
    int height = 0;
};

struct UiOverlayLayout {
    /// False when the viewport cannot host a legible panel; presenters should draw only a backdrop.
    bool renderable = false;
    UiPanelBounds panel{};
    int contentLeft = 0;
    int contentRight = 0;
    int titleTop = 0;
    int titleHeight = 0;
    std::vector<UiOverlayRowPlacement> rows{};
    /// True when at least one row did not fit and was dropped (presenters draw a continuation mark).
    bool rowsTruncated = false;
    /// Parallel to `UiPresentedScreen::options`. Zero-width entries did not fit and must be treated
    /// as neither drawable nor clickable, so a row is never invisibly hot.
    std::vector<UiPanelBounds> optionRects{};
    int hintTop = 0;
    int hintHeight = 0;
    /// Base spacing unit in physical pixels.
    int unit = 6;
};

/// Measures the wrapped height in physical pixels of `row` constrained to `wrapWidth`.
using UiTextMeasureFn = std::function<int(const UiPresentedRow& row, int wrapWidth)>;

/// Places the title, rows, option list, and hint legend inside a centered panel. Never returns
/// geometry outside the client area, and reserves the option list before flowing text so options
/// cannot be pushed off-panel by long prose.
[[nodiscard]] UiOverlayLayout ComputeOverlayLayout(const UiPresentedScreen& screen,
                                                   const UiOverlayLayoutInput& input,
                                                   const UiTextMeasureFn& measure);

/// Input legend for the current screen ("click to continue" vs. option navigation).
[[nodiscard]] std::string_view UiOverlayHintText(const UiPresentedScreen& screen) noexcept;

} // namespace ri::ui
