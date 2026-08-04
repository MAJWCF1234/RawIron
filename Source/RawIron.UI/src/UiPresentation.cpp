#include "RawIron/Ui/UiPresentation.h"

#include "RawIron/Ui/UiText.h"

#include <algorithm>

namespace ri::ui {
namespace {

[[nodiscard]] UiTextAlign ParseAlign(std::string_view align, const UiTextAlign fallback) noexcept {
    if (align == "left") {
        return UiTextAlign::Left;
    }
    if (align == "center") {
        return UiTextAlign::Center;
    }
    if (align == "right") {
        return UiTextAlign::Right;
    }
    return fallback;
}

[[nodiscard]] int ScaleToDpi(const int pixels, const int dpi) noexcept {
    const int safeDpi = std::max(96, dpi);
    return static_cast<int>((static_cast<std::int64_t>(pixels) * safeDpi) / 96);
}

} // namespace

UiPresentedScreen PresentScreen(const UiFlowSession& session) {
    UiPresentedScreen presented{};
    presented.screen = session.CurrentScreen();
    if (presented.screen == nullptr) {
        return presented;
    }
    presented.title = ResolveStoreText(session, presented.screen->title);
    presented.advanceAvailable = presented.screen->advanceAction.kind != UiActionKind::None;

    for (const UiBlock& block : presented.screen->blocks) {
        if (!session.IsBlockVisible(block)) {
            continue;
        }
        switch (block.kind) {
        case UiBlockKind::Heading: {
            std::string text = ResolveStoreText(session, block.text);
            if (text.empty()) {
                break;
            }
            presented.rows.push_back(UiPresentedRow{
                .text = std::move(text),
                .style = UiTextStyle::Heading,
                .align = ParseAlign(block.align, UiTextAlign::Center),
                .spacingAboveUnits = 2,
            });
            break;
        }
        case UiBlockKind::Paragraph:
        case UiBlockKind::Label: {
            std::string text = ResolveStoreText(session, block.text);
            if (text.empty()) {
                break;
            }
            const UiTextAlign fallback =
                block.kind == UiBlockKind::Label ? UiTextAlign::Center : UiTextAlign::Left;
            presented.rows.push_back(UiPresentedRow{
                .text = std::move(text),
                .style = UiTextStyle::Body,
                .align = ParseAlign(block.align, fallback),
            });
            break;
        }
        case UiBlockKind::Say: {
            std::string speaker = ResolveStoreText(session, block.speaker);
            if (!speaker.empty()) {
                presented.rows.push_back(UiPresentedRow{
                    .text = std::move(speaker),
                    .style = UiTextStyle::Speaker,
                    .align = UiTextAlign::Left,
                    .spacingAboveUnits = 1,
                });
            }
            std::string text = ResolveStoreText(session, block.text);
            if (!text.empty()) {
                presented.rows.push_back(UiPresentedRow{
                    .text = std::move(text),
                    .style = UiTextStyle::Body,
                    .align = ParseAlign(block.align, UiTextAlign::Left),
                });
            }
            break;
        }
        case UiBlockKind::Narration: {
            std::string text = ResolveStoreText(session, block.text);
            if (text.empty()) {
                break;
            }
            presented.rows.push_back(UiPresentedRow{
                .text = std::move(text),
                .style = UiTextStyle::Narration,
                .align = ParseAlign(block.align, UiTextAlign::Left),
                .spacingAboveUnits = 1,
            });
            break;
        }
        case UiBlockKind::HistoryNote: {
            if (block.historyBacklogOnly) {
                break;
            }
            std::string text = ResolveStoreText(session, block.text);
            if (text.empty()) {
                break;
            }
            presented.rows.push_back(UiPresentedRow{
                .text = std::move(text),
                .style = UiTextStyle::Note,
                .align = ParseAlign(block.align, UiTextAlign::Center),
                .spacingAboveUnits = 1,
            });
            break;
        }
        case UiBlockKind::Separator:
            presented.rows.push_back(UiPresentedRow{
                .style = UiTextStyle::Separator,
                .spacingAboveUnits = 1,
            });
            break;
        case UiBlockKind::Spacer:
            presented.rows.push_back(UiPresentedRow{
                .style = UiTextStyle::Spacer,
                .requestedHeightPixels = std::clamp(block.spacerHeight, 0.0f, 96.0f),
            });
            break;
        case UiBlockKind::Button:
            presented.options.push_back(UiPresentedOption{
                .label = ResolveStoreText(session, block.label),
                .action = block.action,
            });
            break;
        case UiBlockKind::Choices:
            for (const UiChoiceItem& choice : block.choices) {
                if (!session.IsChoiceVisible(choice)) {
                    continue;
                }
                presented.options.push_back(UiPresentedOption{
                    .label = ResolveStoreText(session, choice.label),
                    .action = choice.action,
                });
            }
            break;
        case UiBlockKind::Image:
        default:
            break;
        }
    }
    return presented;
}

int UiFontPixelSize(const UiTextStyle style, const int dpi) noexcept {
    switch (style) {
    case UiTextStyle::Title:
        return ScaleToDpi(38, dpi);
    case UiTextStyle::Heading:
        return ScaleToDpi(26, dpi);
    case UiTextStyle::Option:
        return ScaleToDpi(23, dpi);
    case UiTextStyle::Note:
    case UiTextStyle::Hint:
        return ScaleToDpi(16, dpi);
    case UiTextStyle::Body:
    case UiTextStyle::Speaker:
    case UiTextStyle::Narration:
    case UiTextStyle::Separator:
    case UiTextStyle::Spacer:
    default:
        return ScaleToDpi(20, dpi);
    }
}

UiOverlayLayout ComputeOverlayLayout(const UiPresentedScreen& screen,
                                    const UiOverlayLayoutInput& input,
                                    const UiTextMeasureFn& measure) {
    UiOverlayLayout layout{};
    layout.optionRects.assign(screen.options.size(), UiPanelBounds{});

    const int clientWidth = std::max(0, input.clientWidth);
    const int clientHeight = std::max(0, input.clientHeight);
    const int dpi = std::max(96, input.dpi);
    const auto scaled = [dpi](const int pixels) { return ScaleToDpi(pixels, dpi); };

    layout.unit = std::max(1, scaled(6));
    const int paddingX = scaled(24);
    const int paddingY = scaled(20);

    // Preference is clamped before it ever meets the client size: passing a minimum straight to
    // std::clamp against a shrinking window is undefined behaviour.
    const int preferredWidth = std::min(std::max(clientWidth - scaled(160), scaled(420)), scaled(760));
    const int panelWidth = std::min(preferredWidth, std::max(0, clientWidth - scaled(16)));
    // A panel below this needs room for a title line, one row, and the input legend; anything
    // smaller is reported as unrenderable so presenters draw a plain backdrop instead of a
    // shredded panel.
    const int minimumUsableWidth = (paddingX * 2) + scaled(40);
    const int minimumUsableHeight = (paddingY * 2) + scaled(22) + scaled(60);
    if (panelWidth <= minimumUsableWidth || clientHeight < minimumUsableHeight || !measure) {
        return layout;
    }

    const int contentWidth = panelWidth - (paddingX * 2);
    const int titleHeight = screen.title.empty()
        ? 0
        : measure(UiPresentedRow{.text = screen.title, .style = UiTextStyle::Title, .align = UiTextAlign::Center},
                  contentWidth);

    std::vector<int> rowHeights(screen.rows.size(), 0);
    int contentHeight = 0;
    for (std::size_t rowIndex = 0; rowIndex < screen.rows.size(); ++rowIndex) {
        const UiPresentedRow& row = screen.rows[rowIndex];
        int height = 0;
        switch (row.style) {
        case UiTextStyle::Separator:
            height = std::max(1, scaled(1));
            break;
        case UiTextStyle::Spacer:
            height = scaled(static_cast<int>(row.requestedHeightPixels));
            break;
        default:
            height = measure(row, contentWidth);
            break;
        }
        rowHeights[rowIndex] = height;
        contentHeight += (row.spacingAboveUnits * layout.unit) + height + layout.unit;
    }

    const int optionRowHeight = scaled(40);
    const int optionRowGap = std::max(1, scaled(4));
    const int optionCount = static_cast<int>(screen.options.size());
    const int optionsHeight =
        optionCount > 0 ? (optionCount * (optionRowHeight + optionRowGap)) + layout.unit : 0;
    const int hintHeight = scaled(22);

    const int desiredHeight =
        paddingY + titleHeight + scaled(14) + contentHeight + optionsHeight + hintHeight + paddingY;
    layout.panel = ComputeCenteredPanelBounds(
        clientWidth, clientHeight, panelWidth, std::max(desiredHeight, scaled(160)));
    layout.renderable =
        layout.panel.width > minimumUsableWidth && layout.panel.height >= minimumUsableHeight;
    if (!layout.renderable) {
        return layout;
    }

    layout.contentLeft = layout.panel.left + paddingX;
    layout.contentRight = layout.panel.left + layout.panel.width - paddingX;
    layout.titleTop = layout.panel.top + paddingY;
    layout.titleHeight = titleHeight;

    layout.hintHeight = hintHeight;
    layout.hintTop = layout.panel.top + layout.panel.height - paddingY - hintHeight;

    // Options are reserved bottom-up so prose can never displace them.
    const int optionsTop = layout.hintTop - optionsHeight;
    int cursorY = layout.titleTop + titleHeight + scaled(14);
    const int contentBottom = std::max(cursorY, optionsTop - layout.unit);

    layout.rows.reserve(screen.rows.size());
    for (std::size_t rowIndex = 0; rowIndex < screen.rows.size(); ++rowIndex) {
        const UiPresentedRow& row = screen.rows[rowIndex];
        const int rowTop = cursorY + (row.spacingAboveUnits * layout.unit);
        const int height = rowHeights[rowIndex];
        if (rowTop + height > contentBottom) {
            layout.rowsTruncated = true;
            break;
        }
        layout.rows.push_back(UiOverlayRowPlacement{.rowIndex = rowIndex, .top = rowTop, .height = height});
        cursorY = rowTop + height + layout.unit;
    }

    const int optionWidth = layout.contentRight - layout.contentLeft;
    int optionY = optionsTop;
    for (std::size_t optionIndex = 0; optionIndex < screen.options.size(); ++optionIndex) {
        const int top = optionY;
        optionY += optionRowHeight + optionRowGap;
        if (top < layout.panel.top || top + optionRowHeight > layout.hintTop || optionWidth <= 0) {
            continue;
        }
        layout.optionRects[optionIndex] = UiPanelBounds{
            .left = layout.contentLeft,
            .top = top,
            .width = optionWidth,
            .height = optionRowHeight,
        };
    }
    return layout;
}

std::string_view UiOverlayHintText(const UiPresentedScreen& screen) noexcept {
    if (!screen.options.empty()) {
        return "Click, Enter, or 1-9  |  Tab: next  |  Esc: back";
    }
    if (screen.advanceAvailable) {
        return "Click or press Space to continue  |  Esc: back";
    }
    return "Esc: back";
}

} // namespace ri::ui
