#include "RawIron/World/LevelFlowPresentationState.h"

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

void AdvanceTimedEntry(std::optional<TimedPresentationEntry>& entry, double elapsedMs) {
    if (!entry.has_value()) {
        return;
    }
    if (!std::isfinite(elapsedMs) || elapsedMs <= 0.0) {
        return;
    }
    entry->remainingMs = std::max(0.0, entry->remainingMs - elapsedMs);
    if (entry->remainingMs <= 0.0) {
        entry.reset();
    }
}

std::string NormalizeLabel(std::string label, std::string fallback) {
    return label.empty() ? std::move(fallback) : std::move(label);
}

} // namespace

LevelFlowPresentationState::LevelFlowPresentationState(const LevelFlowPresentationPolicy& policy) {
    SetPolicy(policy);
}

void LevelFlowPresentationState::SetPolicy(const LevelFlowPresentationPolicy& policy) {
    policy_ = policy;
    policy_.historyLimit = std::max<std::size_t>(1U, policy_.historyLimit);
    if (policy_.mode == LevelFlowPresentationMode::Disabled) {
        ClearTransientState();
        history_.clear();
        loadingStatus_.clear();
    }
}

const LevelFlowPresentationPolicy& LevelFlowPresentationState::Policy() const {
    return policy_;
}

void LevelFlowPresentationState::BeginLoad(std::string levelLabel) {
    isLoading_ = true;
    if (policy_.mode == LevelFlowPresentationMode::Disabled || !policy_.showLoadStatus) {
        loadingStatus_.clear();
        return;
    }
    loadingStatus_ = "Loading " + NormalizeLabel(std::move(levelLabel), "level") + "...";
    PushHistory("load", loadingStatus_);
}

void LevelFlowPresentationState::CompleteLoad(const LevelFlowLoadRequest& request) {
    isLoading_ = false;
    locationLabel_ = NormalizeLabel(request.levelLabel, NormalizeLabel(request.levelId, "Unknown Echo"));

    if (policy_.mode == LevelFlowPresentationMode::Disabled) {
        loadingStatus_.clear();
        return;
    }

    if (policy_.showLoadStatus) {
        loadingStatus_ = "Ready.";
        PushHistory("ready", loadingStatus_);
    } else {
        loadingStatus_.clear();
    }

    if (policy_.showLevelToast) {
        ActivateTimed(activeToast_,
                      policy_.mode == LevelFlowPresentationMode::Minimal
                          ? "Entered " + locationLabel_
                          : locationLabel_,
                      2500.0);
        PushHistory("toast", activeToast_->text);
    }

    if (policy_.showCheckpointRestore && request.restoredFromCheckpoint) {
        ActivateTimed(activeCheckpointNotice_, "Checkpoint restored.", 2200.0);
        PushHistory("checkpoint", activeCheckpointNotice_->text);
    }

    if (policy_.mode == LevelFlowPresentationMode::Verbose
        && policy_.showStoryIntro
        && request.firstVisit
        && !request.storyIntro.empty()) {
        ActivateTimed(activeStoryIntro_, request.storyIntro, 9000.0);
        PushHistory("intro", activeStoryIntro_->text);
    }
}

void LevelFlowPresentationState::Advance(double elapsedMs) {
    AdvanceTimedEntry(activeToast_, elapsedMs);
    AdvanceTimedEntry(activeStoryIntro_, elapsedMs);
    AdvanceTimedEntry(activeCheckpointNotice_, elapsedMs);
}

void LevelFlowPresentationState::ClearTransientState() {
    activeToast_.reset();
    activeStoryIntro_.reset();
    activeCheckpointNotice_.reset();
}

bool LevelFlowPresentationState::IsLoading() const {
    return isLoading_;
}

const std::string& LevelFlowPresentationState::LoadingStatus() const {
    return loadingStatus_;
}

const std::string& LevelFlowPresentationState::LocationLabel() const {
    return locationLabel_;
}

const std::optional<TimedPresentationEntry>& LevelFlowPresentationState::ActiveToast() const {
    return activeToast_;
}

const std::optional<TimedPresentationEntry>& LevelFlowPresentationState::ActiveStoryIntro() const {
    return activeStoryIntro_;
}

const std::optional<TimedPresentationEntry>& LevelFlowPresentationState::ActiveCheckpointNotice() const {
    return activeCheckpointNotice_;
}

const std::vector<LevelFlowHistoryEntry>& LevelFlowPresentationState::History() const {
    return history_;
}

void LevelFlowPresentationState::ActivateTimed(std::optional<TimedPresentationEntry>& target,
                                               std::string text,
                                               double durationMs) {
    const double safeDuration = SanitizeDuration(durationMs, 2500.0);
    target = TimedPresentationEntry{
        .text = std::move(text),
        .durationMs = safeDuration,
        .remainingMs = safeDuration,
        .severity = PresentationSeverity::Normal,
    };
}

void LevelFlowPresentationState::PushHistory(std::string category, std::string text) {
    history_.push_back(LevelFlowHistoryEntry{
        .category = std::move(category),
        .text = std::move(text),
        .revision = nextRevision_++,
    });
    if (history_.size() > policy_.historyLimit) {
        history_.erase(history_.begin(),
                       history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - policy_.historyLimit));
    }
}

} // namespace ri::world
