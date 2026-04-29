#pragma once

#include "RawIron/World/PresentationState.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ri::world {

enum class LevelFlowPresentationMode {
    Disabled,
    Minimal,
    Verbose,
};

struct LevelFlowPresentationPolicy {
    LevelFlowPresentationMode mode = LevelFlowPresentationMode::Verbose;
    bool showLoadStatus = true;
    bool showLevelToast = true;
    bool showStoryIntro = true;
    bool showCheckpointRestore = true;
    std::size_t historyLimit = 16U;
};

struct LevelFlowLoadRequest {
    std::string levelId;
    std::string levelLabel;
    std::string storyIntro;
    bool firstVisit = true;
    bool restoredFromCheckpoint = false;
};

struct LevelFlowHistoryEntry {
    std::string category;
    std::string text;
    std::uint64_t revision = 0;
};

class LevelFlowPresentationState {
public:
    explicit LevelFlowPresentationState(const LevelFlowPresentationPolicy& policy = {});

    void SetPolicy(const LevelFlowPresentationPolicy& policy);
    [[nodiscard]] const LevelFlowPresentationPolicy& Policy() const;

    void BeginLoad(std::string levelLabel);
    void CompleteLoad(const LevelFlowLoadRequest& request);
    void Advance(double elapsedMs);
    void ClearTransientState();

    [[nodiscard]] bool IsLoading() const;
    [[nodiscard]] const std::string& LoadingStatus() const;
    [[nodiscard]] const std::string& LocationLabel() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveToast() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveStoryIntro() const;
    [[nodiscard]] const std::optional<TimedPresentationEntry>& ActiveCheckpointNotice() const;
    [[nodiscard]] const std::vector<LevelFlowHistoryEntry>& History() const;

private:
    void ActivateTimed(std::optional<TimedPresentationEntry>& target, std::string text, double durationMs);
    void PushHistory(std::string category, std::string text);

    LevelFlowPresentationPolicy policy_{};
    bool isLoading_ = false;
    std::string loadingStatus_{};
    std::string locationLabel_{};
    std::optional<TimedPresentationEntry> activeToast_{};
    std::optional<TimedPresentationEntry> activeStoryIntro_{};
    std::optional<TimedPresentationEntry> activeCheckpointNotice_{};
    std::vector<LevelFlowHistoryEntry> history_{};
    std::uint64_t nextRevision_ = 1U;
};

} // namespace ri::world
