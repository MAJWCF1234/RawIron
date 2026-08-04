#pragma once

/// Device-independent interaction layer for JSON UI manifests: turns edge-triggered input into
/// manifest actions, owns option selection and the VN auto-advance timer, and reports what changed
/// so presenters can repaint only when needed.
///
/// Hosts translate their own devices (Win32 messages, ImGui IO, gamepad) into `UiFlowInput` and let
/// the controller decide what happens, instead of each game re-deriving the rules.

#include "RawIron/Ui/UiFlowSession.h"
#include "RawIron/Ui/UiPresentation.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace ri::ui {

/// One update's worth of edge-triggered input. All flags are "happened since last update".
struct UiFlowInput {
    float deltaSeconds = 0.0f;
    bool advanceSpace = false;
    bool advanceEnter = false;
    /// Pointer click that did not land on an option row.
    bool advanceClick = false;
    bool advanceWheel = false;
    bool selectNext = false;
    bool selectPrevious = false;
    bool back = false;
    /// 1..9 to activate an option directly, 0 for none.
    int digitPressed = 0;
    /// Option index a pointer activated, or -1.
    int activatedOptionIndex = -1;
    /// Option index a pointer is hovering, or -1.
    int hoveredOptionIndex = -1;
};

struct UiFlowUpdateResult {
    bool actionApplied = false;
    bool selectionChanged = false;
    bool screenChanged = false;
    /// `back` was requested with nothing left to pop: the host should close the UI.
    bool exitRequested = false;
    /// The session has no current screen (empty or broken manifest).
    bool screenMissing = false;
    /// Something visible changed; presenters should repaint.
    bool needsRepaint = false;
};

class UiFlowController {
public:
    using EmitHandler = std::function<void(std::string_view actionId)>;

    /// Clears selection, timers, and the cached screen so the next update republishes.
    void Reset();

    /// Applies `input` to `session`, at most one navigation action per call, and refreshes the
    /// cached presentation. `onEmit` receives manifest `emit` action ids.
    UiFlowUpdateResult Update(UiFlowSession& session, const UiFlowInput& input, const EmitHandler& onEmit);

    /// Cached flattened screen from the most recent `Update`.
    [[nodiscard]] const UiPresentedScreen& Presented() const noexcept { return presented_; }

    [[nodiscard]] std::size_t SelectedOption() const noexcept { return selectedOption_; }

    /// Returns true when the selection actually moved.
    bool SetSelectedOption(std::size_t index);

    /// Label of the selected option, or empty when the screen has none.
    [[nodiscard]] std::string_view SelectedOptionLabel() const noexcept;

    [[nodiscard]] float ScreenElapsedSeconds() const noexcept { return screenElapsedSeconds_; }

    /// Id of the screen currently presented (empty when none).
    [[nodiscard]] std::string_view ScreenId() const noexcept { return screenId_; }

private:
    void RefreshPresentation(const UiFlowSession& session);
    bool ActivateOption(UiFlowSession& session, std::size_t optionIndex, const EmitHandler& onEmit);

    UiPresentedScreen presented_{};
    std::size_t selectedOption_ = 0U;
    float screenElapsedSeconds_ = 0.0f;
    bool advanceTimerConsumed_ = false;
    std::string screenId_{};
    std::uint64_t navigationRevision_ = 0U;
    bool hasPresented_ = false;
};

} // namespace ri::ui
