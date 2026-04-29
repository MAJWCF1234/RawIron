#pragma once

#include "RawIron/World/PresentationState.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace ri::world {

enum class TextOverlayChannel : std::uint8_t {
    MessageBox,
    SubtitleLine,
    LevelNameToast,
};

struct TextOverlayChannelState {
    bool visible = false;
    std::string text;
    PresentationSeverity severity = PresentationSeverity::Normal;
    double durationMs = 0.0;
    double remainingMs = 0.0;
    std::uint64_t revision = 0;
};

struct TextOverlayObjectiveState {
    std::string text;
    std::string hint;
    bool flashing = false;
    double flashRemainingMs = 0.0;
    std::uint64_t revision = 0;
};

struct TextOverlayBlockerState {
    bool loadingVisible = false;
    bool startMenuVisible = false;
    bool pauseVisible = false;
    bool gameOverVisible = false;
    bool fadeVisible = false;
    bool debugTerminalVisible = false;
    std::string loadingStatus;
    double loadingProgress01 = 0.0;
};

struct TextOverlaySnapshot {
    bool hudVisible = false;
    TextOverlayChannelState messageBox;
    TextOverlayChannelState subtitleLine;
    TextOverlayChannelState levelNameToast;
    TextOverlayObjectiveState objectiveReadout;
    TextOverlayBlockerState blockers;
};

class TextOverlayState {
public:
    struct TextOverlayChannelHash {
        std::size_t operator()(TextOverlayChannel channel) const noexcept {
            return static_cast<std::size_t>(channel);
        }
    };
    using DismissTimerMap = std::unordered_map<TextOverlayChannel, double, TextOverlayChannelHash>;

    void ShowMessage(std::string text,
                     double durationMs = 4000.0,
                     PresentationSeverity severity = PresentationSeverity::Normal);
    void ShowSubtitle(std::string text, double durationMs = 5000.0);
    void ShowLevelNameToast(std::string text, double durationMs = 2500.0);
    void UpdateObjective(std::string text, const ObjectiveUpdateOptions& options = {});

    void SetLoadingVisible(bool visible, std::string status = {});
    void SetLoadingProgress(double progress01, std::string status = {});
    void SetStartMenuVisible(bool visible);
    void SetPauseVisible(bool visible);
    void SetGameOverVisible(bool visible);
    void SetFadeVisible(bool visible);
    void SetDebugTerminalVisible(bool visible);

    void ClearTransientChannels();
    void Advance(double elapsedMs);

    [[nodiscard]] TextOverlaySnapshot Snapshot() const;
    [[nodiscard]] const DismissTimerMap& DismissTimers() const;

private:
    void ActivateTimedChannel(TextOverlayChannel channel,
                              std::string text,
                              double durationMs,
                              PresentationSeverity severity);
    void SetChannelVisibility(TextOverlayChannel channel, bool visible);
    [[nodiscard]] TextOverlayChannelState& MutableChannel(TextOverlayChannel channel);
    [[nodiscard]] const TextOverlayChannelState& GetChannel(TextOverlayChannel channel) const;
    [[nodiscard]] bool AnyHudTextVisible() const;

    TextOverlayChannelState messageBox_{};
    TextOverlayChannelState subtitleLine_{};
    TextOverlayChannelState levelNameToast_{};
    TextOverlayObjectiveState objectiveReadout_{};
    TextOverlayBlockerState blockers_{};
    DismissTimerMap hudDismissTimers_{};
    std::uint64_t nextRevision_ = 1U;
};

} // namespace ri::world
