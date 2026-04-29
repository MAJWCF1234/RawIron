#include "RawIron/World/TextOverlayState.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ri::world {
namespace {

double SanitizeDuration(double durationMs, double fallbackMs) {
    if (!std::isfinite(durationMs)) {
        return fallbackMs;
    }
    return std::clamp(durationMs, 0.0, 180000.0);
}

double SanitizeProgress(double progress01) {
    if (!std::isfinite(progress01)) {
        return 0.0;
    }
    return std::clamp(progress01, 0.0, 1.0);
}

} // namespace

void TextOverlayState::ShowMessage(std::string text,
                                   double durationMs,
                                   PresentationSeverity severity) {
    ActivateTimedChannel(TextOverlayChannel::MessageBox, std::move(text), durationMs, severity);
}

void TextOverlayState::ShowSubtitle(std::string text, double durationMs) {
    ActivateTimedChannel(TextOverlayChannel::SubtitleLine,
                         std::move(text),
                         durationMs,
                         PresentationSeverity::Normal);
}

void TextOverlayState::ShowLevelNameToast(std::string text, double durationMs) {
    ActivateTimedChannel(TextOverlayChannel::LevelNameToast,
                         std::move(text),
                         durationMs,
                         PresentationSeverity::Normal);
}

void TextOverlayState::UpdateObjective(std::string text, const ObjectiveUpdateOptions& options) {
    objectiveReadout_.text = std::move(text);
    objectiveReadout_.hint = options.hint;
    objectiveReadout_.flashRemainingMs = options.flash ? 1500.0 : 0.0;
    objectiveReadout_.flashing = objectiveReadout_.flashRemainingMs > 0.0;
    objectiveReadout_.revision = nextRevision_++;

    if (options.announce && !objectiveReadout_.text.empty()) {
        ShowMessage("NEW OBJECTIVE: " + objectiveReadout_.text, 4000.0, PresentationSeverity::Normal);
    } else if (!options.hint.empty()) {
        ShowMessage(options.hint, options.hintDurationMs, PresentationSeverity::Normal);
    }
}

void TextOverlayState::SetLoadingVisible(bool visible, std::string status) {
    blockers_.loadingVisible = visible;
    if (!status.empty() || !visible) {
        blockers_.loadingStatus = std::move(status);
    }
}

void TextOverlayState::SetLoadingProgress(double progress01, std::string status) {
    blockers_.loadingProgress01 = SanitizeProgress(progress01);
    if (!status.empty()) {
        blockers_.loadingStatus = std::move(status);
    }
}

void TextOverlayState::SetStartMenuVisible(bool visible) {
    blockers_.startMenuVisible = visible;
}

void TextOverlayState::SetPauseVisible(bool visible) {
    blockers_.pauseVisible = visible;
}

void TextOverlayState::SetGameOverVisible(bool visible) {
    blockers_.gameOverVisible = visible;
}

void TextOverlayState::SetFadeVisible(bool visible) {
    blockers_.fadeVisible = visible;
}

void TextOverlayState::SetDebugTerminalVisible(bool visible) {
    blockers_.debugTerminalVisible = visible;
}

void TextOverlayState::ClearTransientChannels() {
    messageBox_ = TextOverlayChannelState{};
    subtitleLine_ = TextOverlayChannelState{};
    levelNameToast_ = TextOverlayChannelState{};
    hudDismissTimers_.clear();
    objectiveReadout_.flashing = false;
    objectiveReadout_.flashRemainingMs = 0.0;
}

void TextOverlayState::Advance(double elapsedMs) {
    if (std::isfinite(elapsedMs) && elapsedMs > 0.0) {
        for (auto it = hudDismissTimers_.begin(); it != hudDismissTimers_.end();) {
            it->second = std::max(0.0, it->second - elapsedMs);
            TextOverlayChannelState& channel = MutableChannel(it->first);
            channel.remainingMs = it->second;
            if (it->second <= 0.0) {
                SetChannelVisibility(it->first, false);
                channel.text.clear();
                channel.remainingMs = 0.0;
                channel.durationMs = 0.0;
                it = hudDismissTimers_.erase(it);
            } else {
                ++it;
            }
        }
        objectiveReadout_.flashRemainingMs = std::max(0.0, objectiveReadout_.flashRemainingMs - elapsedMs);
        objectiveReadout_.flashing = objectiveReadout_.flashRemainingMs > 0.0;
    }
}

TextOverlaySnapshot TextOverlayState::Snapshot() const {
    return TextOverlaySnapshot{
        .hudVisible = AnyHudTextVisible(),
        .messageBox = messageBox_,
        .subtitleLine = subtitleLine_,
        .levelNameToast = levelNameToast_,
        .objectiveReadout = objectiveReadout_,
        .blockers = blockers_,
    };
}

const TextOverlayState::DismissTimerMap& TextOverlayState::DismissTimers() const {
    return hudDismissTimers_;
}

void TextOverlayState::ActivateTimedChannel(TextOverlayChannel channel,
                                            std::string text,
                                            double durationMs,
                                            PresentationSeverity severity) {
    const double safeDuration = SanitizeDuration(durationMs, 4000.0);
    TextOverlayChannelState& target = MutableChannel(channel);
    target.visible = !text.empty();
    target.text = std::move(text);
    target.severity = severity;
    target.durationMs = safeDuration;
    target.remainingMs = safeDuration;
    target.revision = nextRevision_++;
    hudDismissTimers_[channel] = safeDuration;
}

void TextOverlayState::SetChannelVisibility(TextOverlayChannel channel, bool visible) {
    MutableChannel(channel).visible = visible;
}

TextOverlayChannelState& TextOverlayState::MutableChannel(TextOverlayChannel channel) {
    switch (channel) {
        case TextOverlayChannel::MessageBox:
            return messageBox_;
        case TextOverlayChannel::SubtitleLine:
            return subtitleLine_;
        case TextOverlayChannel::LevelNameToast:
            return levelNameToast_;
    }
    return messageBox_;
}

const TextOverlayChannelState& TextOverlayState::GetChannel(TextOverlayChannel channel) const {
    switch (channel) {
        case TextOverlayChannel::MessageBox:
            return messageBox_;
        case TextOverlayChannel::SubtitleLine:
            return subtitleLine_;
        case TextOverlayChannel::LevelNameToast:
            return levelNameToast_;
    }
    return messageBox_;
}

bool TextOverlayState::AnyHudTextVisible() const {
    return GetChannel(TextOverlayChannel::MessageBox).visible
        || GetChannel(TextOverlayChannel::SubtitleLine).visible
        || GetChannel(TextOverlayChannel::LevelNameToast).visible
        || !objectiveReadout_.text.empty();
}

} // namespace ri::world
