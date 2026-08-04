#include "RawIron/Ui/UiFlowController.h"

#include <algorithm>
#include <cmath>

namespace ri::ui {

void UiFlowController::Reset() {
    presented_ = UiPresentedScreen{};
    selectedOption_ = 0U;
    screenElapsedSeconds_ = 0.0f;
    advanceTimerConsumed_ = false;
    screenId_.clear();
    navigationRevision_ = 0U;
    hasPresented_ = false;
}

void UiFlowController::RefreshPresentation(const UiFlowSession& session) {
    presented_ = PresentScreen(session);
    if (presented_.options.empty()) {
        selectedOption_ = 0U;
    } else {
        selectedOption_ = std::min(selectedOption_, presented_.options.size() - 1U);
    }
}

bool UiFlowController::SetSelectedOption(const std::size_t index) {
    if (presented_.options.empty()) {
        const bool changed = selectedOption_ != 0U;
        selectedOption_ = 0U;
        return changed;
    }
    const std::size_t clamped = std::min(index, presented_.options.size() - 1U);
    if (clamped == selectedOption_) {
        return false;
    }
    selectedOption_ = clamped;
    return true;
}

std::string_view UiFlowController::SelectedOptionLabel() const noexcept {
    if (selectedOption_ >= presented_.options.size()) {
        return {};
    }
    return presented_.options[selectedOption_].label;
}

bool UiFlowController::ActivateOption(UiFlowSession& session,
                                      const std::size_t optionIndex,
                                      const EmitHandler& onEmit) {
    if (optionIndex >= presented_.options.size()) {
        return false;
    }
    return session.ApplyAction(presented_.options[optionIndex].action, onEmit);
}

UiFlowUpdateResult UiFlowController::Update(UiFlowSession& session,
                                            const UiFlowInput& input,
                                            const EmitHandler& onEmit) {
    UiFlowUpdateResult result{};

    RefreshPresentation(session);
    if (presented_.screen == nullptr) {
        result.screenMissing = true;
        screenId_.clear();
        hasPresented_ = false;
        return result;
    }

    // A screen change can come from navigation or from a host-side session reset, so compare both
    // the revision counter and the resolved id.
    const std::uint64_t revision = session.NavigationRevision();
    if (!hasPresented_ || revision != navigationRevision_ || screenId_ != presented_.screen->id) {
        screenId_ = presented_.screen->id;
        navigationRevision_ = revision;
        hasPresented_ = true;
        selectedOption_ = 0U;
        screenElapsedSeconds_ = 0.0f;
        advanceTimerConsumed_ = false;
        result.screenChanged = true;
        result.needsRepaint = true;
    }

    const float delta = std::isfinite(input.deltaSeconds) ? std::max(0.0f, input.deltaSeconds) : 0.0f;
    screenElapsedSeconds_ += delta;

    if (input.hoveredOptionIndex >= 0 && SetSelectedOption(static_cast<std::size_t>(input.hoveredOptionIndex))) {
        result.selectionChanged = true;
        result.needsRepaint = true;
    }

    const auto finish = [&](const bool applied) {
        if (applied) {
            result.actionApplied = true;
            result.needsRepaint = true;
            const std::uint64_t appliedRevision = session.NavigationRevision();
            RefreshPresentation(session);
            if (presented_.screen == nullptr) {
                result.screenMissing = true;
                screenId_.clear();
                hasPresented_ = false;
                return result;
            }
            if (appliedRevision != navigationRevision_ || screenId_ != presented_.screen->id) {
                screenId_ = presented_.screen->id;
                navigationRevision_ = appliedRevision;
                selectedOption_ = 0U;
                screenElapsedSeconds_ = 0.0f;
                advanceTimerConsumed_ = false;
                result.screenChanged = true;
            }
        }
        return result;
    };

    if (input.back) {
        if (session.GoBack()) {
            return finish(true);
        }
        result.exitRequested = true;
        return result;
    }

    if (input.digitPressed >= 1 && input.digitPressed <= 9) {
        const std::size_t optionIndex = static_cast<std::size_t>(input.digitPressed - 1);
        if (optionIndex < presented_.options.size()) {
            if (SetSelectedOption(optionIndex)) {
                result.selectionChanged = true;
                result.needsRepaint = true;
            }
            return finish(ActivateOption(session, optionIndex, onEmit));
        }
        return result;
    }

    if (input.activatedOptionIndex >= 0) {
        const std::size_t optionIndex = static_cast<std::size_t>(input.activatedOptionIndex);
        if (optionIndex < presented_.options.size()) {
            if (SetSelectedOption(optionIndex)) {
                result.selectionChanged = true;
                result.needsRepaint = true;
            }
            return finish(ActivateOption(session, optionIndex, onEmit));
        }
        return result;
    }

    if (!presented_.options.empty() && (input.selectNext || input.selectPrevious)) {
        const std::size_t count = presented_.options.size();
        const std::size_t next = input.selectNext
            ? (selectedOption_ + 1U) % count
            : (selectedOption_ + count - 1U) % count;
        if (SetSelectedOption(next)) {
            result.selectionChanged = true;
            result.needsRepaint = true;
        }
        return result;
    }

    // Keyboard confirm activates the highlighted option. A click on empty panel space deliberately
    // does not: clicking nowhere near a row must never fire "Quit".
    if (!presented_.options.empty() && (input.advanceSpace || input.advanceEnter)) {
        return finish(ActivateOption(session, selectedOption_, onEmit));
    }

    if (presented_.advanceAvailable) {
        const UiScreen& screen = *presented_.screen;
        bool advance = (screen.advanceOnSpace && input.advanceSpace)
            || (screen.advanceOnEnter && input.advanceEnter)
            || (screen.advanceOnClick && input.advanceClick)
            || (screen.advanceOnMouseWheel && input.advanceWheel);
        if (!advance && !advanceTimerConsumed_ && screen.advanceAfterSeconds > 0.0f
            && screenElapsedSeconds_ >= screen.advanceAfterSeconds) {
            advanceTimerConsumed_ = true;
            advance = true;
        }
        if (advance) {
            return finish(session.ApplyAction(screen.advanceAction, onEmit));
        }
    }
    return result;
}

} // namespace ri::ui
