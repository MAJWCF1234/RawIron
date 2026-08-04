#include "RawIron/Ui/UiFlowController.h"
#include "RawIron/Ui/UiFlowSession.h"
#include "RawIron/Ui/UiJsonIO.h"
#include "RawIron/Ui/UiLayout.h"
#include "RawIron/Ui/UiPresentation.h"
#include "RawIron/Ui/UiText.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition "\n"; \
            return EXIT_FAILURE; \
        } \
    } while (false)

int main() {
    using namespace ri::ui;

    UiManifest parsed;
    std::string error;
    CHECK(!TryParseUiManifestFromJson(
        R"({"schemaVersion":2,"screens":[{"id":"main"}]})", parsed, &error));
    CHECK(error.find("schemaVersion") != std::string::npos && parsed.screens.empty());
    CHECK(!TryParseUiManifestFromJson(
        R"({"startScreen":"missing","screens":[{"id":"main"}]})", parsed, &error));
    CHECK(error.find("startScreen") != std::string::npos);
    CHECK(!TryParseUiManifestFromJson(
        R"({"screens":[{"id":"main"},{"id":"main"}]})", parsed, &error));
    CHECK(error.find("duplicate UI screen") != std::string::npos);
    CHECK(!TryParseUiManifestFromJson(
        R"({"variables":[{"id":"route","value":"a"},{"id":"route","value":"b"}],"screens":[{"id":"main"}]})",
        parsed,
        &error));
    CHECK(error.find("duplicate UI variable") != std::string::npos);
    CHECK(TryParseUiManifestFromJson(
        R"({"startScreen":"main","variables":[{"id":"route","value":"a"}],"screens":[{"id":"main","background":{"tint":[2,-1,.5,1]},"advance":{"delaySeconds":999999,"action":{"type":"navigate","target":"next"}},"blocks":[{"type":"spacer","height":-5},{"type":"image","heightHint":9999}]},{"id":"next"}]})",
        parsed,
        &error));
    CHECK(error.empty());
    CHECK(parsed.screens.front().backgroundRgba[0] == 1.0f);
    CHECK(parsed.screens.front().backgroundRgba[1] == 0.0f);
    CHECK(parsed.screens.front().advanceAfterSeconds == 86400.0f);
    CHECK(parsed.screens.front().blocks[0].spacerHeight == 0.0f);
    CHECK(parsed.screens.front().blocks[1].imageHeightHint == 2000.0f);

    UiFlowSession session;
    session.Reset(parsed);
    CHECK(session.CurrentScreen() != nullptr && session.CurrentScreen()->id == "main");
    CHECK(session.GetVariableValueView("route") == "a");
    const std::uint64_t resetRevision = session.NavigationRevision();
    for (int index = 0; index < 512; ++index) {
        CHECK(session.NavigateTo("next"));
    }
    CHECK(session.Stack().size() == 128U);
    CHECK(session.Stack().front() == "main" && session.Stack().back() == "next");
    CHECK(session.NavigationRevision() == resetRevision + 512U);
    CHECK(!session.NavigateTo("missing"));

    for (int index = 0; index < 1000; ++index) {
        session.MaybeAppendHistory(
            "line-" + std::to_string(index),
            UiHistoryLine{.text = "line"});
    }
    CHECK(session.History().size() == 160U);

    const UiImageUvRect wide = ComputeCoverImageUv(1920.0f, 1080.0f, 800.0f, 800.0f);
    CHECK(wide.u0 > 0.0f && wide.u1 < 1.0f && wide.v0 == 0.0f && wide.v1 == 1.0f);
    const UiImageUvRect tall = ComputeCoverImageUv(800.0f, 1200.0f, 1600.0f, 900.0f);
    CHECK(tall.v0 > 0.0f && tall.v1 < 1.0f && tall.u0 == 0.0f && tall.u1 == 1.0f);
    const UiStageSize compact = ComputeResponsiveUiStageSize(320.0f, 240.0f);
    CHECK(compact.width <= 296.0f && compact.height <= 204.0f);
    const UiStageSize large = ComputeResponsiveUiStageSize(1920.0f, 1080.0f);
    CHECK(large.width == 900.0f && large.height <= 760.0f);

    // Centered panel bounds must survive viewports smaller than the preferred panel: the previous
    // presenter fed a preferred minimum straight to std::clamp, which is undefined once the window
    // shrinks past it, and produced panels wider than the window with negative origins.
    const UiPanelBounds tiny = ComputeCenteredPanelBounds(200, 120, 420, 320);
    CHECK(tiny.left >= 0 && tiny.top >= 0);
    CHECK(tiny.left + tiny.width <= 200 && tiny.top + tiny.height <= 120);
    const UiPanelBounds degenerate = ComputeCenteredPanelBounds(0, 0, 420, 320);
    CHECK(degenerate.width == 0 && degenerate.height == 0 && degenerate.left == 0 && degenerate.top == 0);
    const UiPanelBounds roomy = ComputeCenteredPanelBounds(1920, 1080, 720, 500);
    CHECK(roomy.width == 720 && roomy.height == 500 && roomy.left == 600 && roomy.top == 290);

    // `${var}` interpolation is shared by every presenter.
    UiManifest storyManifest;
    CHECK(TryParseUiManifestFromJson(
        R"({"startScreen":"line","variables":[{"id":"who","value":"Mina"}],"screens":[{"id":"line","title":"Chapter ${who}",
            "advance":{"onClick":true,"action":{"type":"navigate","target":"choice"}},
            "blocks":[{"type":"say","speaker":"${who}","text":"Hello ${who}"},{"type":"narration","text":"The hall waits."},
                      {"type":"separator"},{"type":"spacer","height":18},
                      {"type":"label","text":"hidden","visibleWhen":{"var":"who","equals":"nobody"}}]},
           {"id":"choice","blocks":[{"type":"choices","choices":[{"label":"Stay as ${who}","action":{"type":"emit","id":"stay"}},
                                                                 {"label":"Leave","action":{"type":"back"}}]}]}]})",
        storyManifest,
        &error));
    CHECK(error.empty());

    UiFlowSession story;
    story.Reset(storyManifest);
    CHECK(ResolveStoreText(story, "hi ${who} ${missing} ${unterminated") == "hi Mina  ${unterminated");

    // Say / narration / separator / spacer must all reach the presenter: the in-game overlay used to
    // drop everything except headings and paragraphs, so VN screens rendered empty.
    const UiPresentedScreen line = PresentScreen(story);
    CHECK(line.title == "Chapter Mina");
    CHECK(line.advanceAvailable && line.options.empty());
    // say -> speaker + body, then narration, separator, spacer; the `visibleWhen` label stays hidden.
    CHECK(line.rows.size() == 5U);
    CHECK(line.rows[0].style == UiTextStyle::Speaker && line.rows[0].text == "Mina");
    CHECK(line.rows[1].style == UiTextStyle::Body && line.rows[1].text == "Hello Mina");
    CHECK(line.rows[2].style == UiTextStyle::Narration);
    CHECK(line.rows[3].style == UiTextStyle::Separator);
    CHECK(line.rows[4].style == UiTextStyle::Spacer && line.rows[4].requestedHeightPixels == 18.0f);

    // A crude fixed-height measurer is enough to prove the layout contract.
    const UiTextMeasureFn measure = [](const UiPresentedRow& row, const int wrapWidth) {
        return wrapWidth > 0 && !row.text.empty() ? 24 : 0;
    };
    const UiOverlayLayout roomyLayout =
        ComputeOverlayLayout(line, UiOverlayLayoutInput{.clientWidth = 1600, .clientHeight = 900, .dpi = 96}, measure);
    CHECK(roomyLayout.renderable);
    CHECK(roomyLayout.panel.left >= 0 && roomyLayout.panel.top >= 0);
    CHECK(roomyLayout.panel.left + roomyLayout.panel.width <= 1600);
    CHECK(roomyLayout.panel.top + roomyLayout.panel.height <= 900);
    CHECK(roomyLayout.rows.size() == line.rows.size() && !roomyLayout.rowsTruncated);
    CHECK(roomyLayout.contentLeft < roomyLayout.contentRight);
    // Rows stay above the hint legend and inside the panel.
    for (const UiOverlayRowPlacement& placement : roomyLayout.rows) {
        CHECK(placement.top >= roomyLayout.panel.top);
        CHECK(placement.top + placement.height <= roomyLayout.hintTop);
    }
    // A viewport too small for a legible panel reports itself instead of emitting bad geometry.
    const UiOverlayLayout unusable =
        ComputeOverlayLayout(line, UiOverlayLayoutInput{.clientWidth = 120, .clientHeight = 90, .dpi = 96}, measure);
    CHECK(!unusable.renderable);

    // Option rows are reserved before prose flows, so options can never be pushed off-panel, and any
    // row that could not be drawn must not be left clickable.
    CHECK(story.NavigateTo("choice"));
    const UiPresentedScreen choice = PresentScreen(story);
    CHECK(choice.options.size() == 2U && choice.options[0].label == "Stay as Mina");
    const UiOverlayLayout choiceLayout =
        ComputeOverlayLayout(choice, UiOverlayLayoutInput{.clientWidth = 1280, .clientHeight = 720, .dpi = 96}, measure);
    CHECK(choiceLayout.renderable && choiceLayout.optionRects.size() == 2U);
    for (const UiPanelBounds& rect : choiceLayout.optionRects) {
        CHECK(rect.width > 0 && rect.height > 0);
        CHECK(rect.top >= choiceLayout.panel.top);
        CHECK(rect.top + rect.height <= choiceLayout.hintTop);
        CHECK(rect.left + rect.width <= choiceLayout.panel.left + choiceLayout.panel.width);
    }
    const UiOverlayLayout dpiLayout =
        ComputeOverlayLayout(choice, UiOverlayLayoutInput{.clientWidth = 1280, .clientHeight = 720, .dpi = 192}, measure);
    CHECK(dpiLayout.unit > choiceLayout.unit);
    CHECK(UiFontPixelSize(UiTextStyle::Body, 192) == 2 * UiFontPixelSize(UiTextStyle::Body, 96));

    // Interaction: hover selects, digits activate, clicking empty space advances but never fires the
    // highlighted option, and `back` at the root asks the host to close instead of silently sticking.
    UiFlowSession interactive;
    interactive.Reset(storyManifest);
    UiFlowController controller;
    std::string emitted;
    const UiFlowController::EmitHandler onEmit = [&emitted](std::string_view actionId) {
        emitted = std::string(actionId);
    };

    UiFlowUpdateResult result = controller.Update(interactive, UiFlowInput{}, onEmit);
    CHECK(result.screenChanged && !result.actionApplied && controller.ScreenId() == "line");

    UiFlowInput clickEmpty{};
    clickEmpty.advanceClick = true;
    result = controller.Update(interactive, clickEmpty, onEmit);
    CHECK(result.actionApplied && result.screenChanged && controller.ScreenId() == "choice");
    CHECK(controller.Presented().options.size() == 2U);

    UiFlowInput hover{};
    hover.hoveredOptionIndex = 1;
    result = controller.Update(interactive, hover, onEmit);
    CHECK(result.selectionChanged && controller.SelectedOption() == 1U);
    CHECK(controller.SelectedOptionLabel() == "Leave");

    // Highlighted option is "Leave" (a back action): a click on empty space must not trigger it.
    UiFlowInput strayClick{};
    strayClick.advanceClick = true;
    strayClick.hoveredOptionIndex = 1;
    result = controller.Update(interactive, strayClick, onEmit);
    CHECK(!result.actionApplied && controller.ScreenId() == "choice");

    UiFlowInput digit{};
    digit.digitPressed = 1;
    result = controller.Update(interactive, digit, onEmit);
    CHECK(result.actionApplied && emitted == "stay");

    UiFlowInput back{};
    back.back = true;
    result = controller.Update(interactive, back, onEmit);
    CHECK(result.actionApplied && controller.ScreenId() == "line");
    result = controller.Update(interactive, back, onEmit);
    CHECK(result.exitRequested && !result.actionApplied);

    // Non-finite frame deltas must not poison the auto-advance timer.
    UiFlowInput badDelta{};
    badDelta.deltaSeconds = std::numeric_limits<float>::quiet_NaN();
    result = controller.Update(interactive, badDelta, onEmit);
    CHECK(std::isfinite(controller.ScreenElapsedSeconds()));

    return EXIT_SUCCESS;
}
