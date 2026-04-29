#include "RawIron/World/TextOverlayEventBridge.h"
#include "RawIron/World/TextOverlayEvents.h"
#include "RawIron/Audio/AudioManager.h"

#include <algorithm>
#include <cctype>

namespace ri::world {
namespace {

std::string GetFieldOrDefault(const ri::runtime::RuntimeEvent& event,
                              std::string_view key,
                              std::string_view fallback = {}) {
    const auto found = event.fields.find(std::string(key));
    if (found != event.fields.end() && !found->second.empty()) {
        return found->second;
    }
    return std::string(fallback);
}

bool ParseBoolField(const ri::runtime::RuntimeEvent& event, std::string_view key, bool fallback = false) {
    const auto found = event.fields.find(std::string(key));
    if (found == event.fields.end()) {
        return fallback;
    }
    std::string value = found->second;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

double ParseDoubleField(const ri::runtime::RuntimeEvent& event, std::string_view key, double fallback = 0.0) {
    const auto found = event.fields.find(std::string(key));
    if (found == event.fields.end() || found->second.empty()) {
        return fallback;
    }
    try {
        return std::stod(found->second);
    } catch (...) {
        return fallback;
    }
}

std::string BuildSubtitleText(const ri::runtime::RuntimeEvent& event) {
    const std::string subtitle = GetFieldOrDefault(event, "subtitleText", GetFieldOrDefault(event, "text"));
    const std::string speaker = GetFieldOrDefault(event, "speaker");
    if (speaker.empty() || subtitle.empty()) {
        return subtitle;
    }
    return speaker + ": " + subtitle;
}

PresentationSeverity ParseSeverity(const ri::runtime::RuntimeEvent& event) {
    std::string severity = GetFieldOrDefault(event, "severity");
    std::transform(severity.begin(), severity.end(), severity.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return severity == "critical" ? PresentationSeverity::Critical : PresentationSeverity::Normal;
}

void RemoveListener(ri::runtime::RuntimeEventBus* bus,
                    std::string_view type,
                    std::optional<ri::runtime::RuntimeEventBus::ListenerId>& listenerId) {
    if (bus != nullptr && listenerId.has_value()) {
        bus->Off(type, *listenerId);
        listenerId.reset();
    }
}

} // namespace

TextOverlayEventBridge::~TextOverlayEventBridge() {
    Detach();
}

void TextOverlayEventBridge::Attach(ri::runtime::RuntimeEventBus& eventBus, TextOverlayState& overlayState) {
    Detach();
    eventBus_ = &eventBus;
    overlayState_ = &overlayState;
    audioManager_ = nullptr;

    messageListener_ = eventBus.On(text_overlay_events::kEventMessage, [this](const ri::runtime::RuntimeEvent& event) {
        if (overlayState_ == nullptr) {
            return;
        }
        overlayState_->ShowMessage(GetFieldOrDefault(event, "text", "message"),
                                   ParseDoubleField(event, "durationMs", 4000.0),
                                   ParseSeverity(event));
    });

    subtitleListener_ = eventBus.On(text_overlay_events::kEventSubtitle, [this](const ri::runtime::RuntimeEvent& event) {
        if (overlayState_ == nullptr) {
            return;
        }
        overlayState_->ShowSubtitle(GetFieldOrDefault(event, "text"),
                                    ParseDoubleField(event, "durationMs", 5000.0));
    });

    levelToastListener_ = eventBus.On(text_overlay_events::kEventLevelToast, [this](const ri::runtime::RuntimeEvent& event) {
        if (overlayState_ == nullptr) {
            return;
        }
        overlayState_->ShowLevelNameToast(GetFieldOrDefault(event, "text"),
                                          ParseDoubleField(event, "durationMs", 2500.0));
    });

    objectiveListener_ = eventBus.On(text_overlay_events::kEventObjectiveChanged, [this](const ri::runtime::RuntimeEvent& event) {
        if (overlayState_ == nullptr) {
            return;
        }
        overlayState_->UpdateObjective(GetFieldOrDefault(event, "text"), {
            .announce = ParseBoolField(event, "announce", true),
            .flash = ParseBoolField(event, "flash", true),
            .hint = GetFieldOrDefault(event, "hint"),
            .hintDurationMs = ParseDoubleField(event, "hintDurationMs", 7000.0),
        });
    });

    stateChangedListener_ = eventBus.On("stateChanged", [this](const ri::runtime::RuntimeEvent& event) {
        if (overlayState_ == nullptr) {
            return;
        }
        const std::string key = GetFieldOrDefault(event, "key");
        const bool value = ParseBoolField(event, "value", false);
        if (key == "isPaused") {
            overlayState_->SetPauseVisible(value);
        } else if (key == "isGameOver") {
            overlayState_->SetGameOverVisible(value);
        } else if (key == "debugTerminalOpen") {
            overlayState_->SetDebugTerminalVisible(value);
        } else if (key == "startMenuVisible") {
            overlayState_->SetStartMenuVisible(value);
        } else if (key == "fadeVisible") {
            overlayState_->SetFadeVisible(value);
        }
    });

    loadingListener_ = eventBus.On(text_overlay_events::kEventLoadingProgress, [this](const ri::runtime::RuntimeEvent& event) {
        if (overlayState_ == nullptr) {
            return;
        }
        const bool visible = ParseBoolField(event, "visible", true);
        overlayState_->SetLoadingVisible(visible, GetFieldOrDefault(event, "status"));
        overlayState_->SetLoadingProgress(ParseDoubleField(event, "progress01", 0.0),
                                          GetFieldOrDefault(event, "status"));
    });

    levelLoadedListener_ = eventBus.On("levelLoaded", [this](const ri::runtime::RuntimeEvent& event) {
        if (overlayState_ == nullptr) {
            return;
        }
        overlayState_->SetLoadingVisible(false);
        const std::string label = GetFieldOrDefault(event, "levelName", GetFieldOrDefault(event, "levelFilename"));
        if (!label.empty()) {
            overlayState_->ShowLevelNameToast(label, 2500.0);
        }
    });

    voiceLineListener_ = eventBus.On(text_overlay_events::kEventVoiceLine, [this](const ri::runtime::RuntimeEvent& event) {
        if (overlayState_ == nullptr) {
            return;
        }
        const std::string subtitle = BuildSubtitleText(event);
        double durationMs = ParseDoubleField(event, "subtitleDurationMs", 0.0);

        if (audioManager_ != nullptr) {
            const std::string audioPath = GetFieldOrDefault(event, "audioPath");
            const double volume = ParseDoubleField(event, "volume", 1.0);
            if (!audioPath.empty()) {
                activeVoice_ = audioManager_->PlayVoice(audioPath, volume);
                if (activeVoice_ != nullptr) {
                    const double soundDuration = activeVoice_->GetDuration();
                    if (durationMs <= 0.0 && soundDuration > 0.0) {
                        durationMs = soundDuration * 1000.0;
                    }
                }
            }
        }
        if (durationMs <= 0.0) {
            durationMs = 5000.0;
        }

        if (!subtitle.empty()) {
            overlayState_->ShowSubtitle(subtitle, durationMs);
        }
    });

    voiceStopListener_ = eventBus.On(text_overlay_events::kEventVoiceStop, [this](const ri::runtime::RuntimeEvent&) {
        if (audioManager_ != nullptr && activeVoice_ != nullptr) {
            audioManager_->StopManagedSound(activeVoice_, true);
        }
        activeVoice_.reset();
    });
}

void TextOverlayEventBridge::Attach(ri::runtime::RuntimeEventBus& eventBus,
                                    TextOverlayState& overlayState,
                                    ri::audio::AudioManager& audioManager) {
    Attach(eventBus, overlayState);
    audioManager_ = &audioManager;
}

void TextOverlayEventBridge::Detach() {
    RemoveListener(eventBus_, text_overlay_events::kEventMessage, messageListener_);
    RemoveListener(eventBus_, text_overlay_events::kEventSubtitle, subtitleListener_);
    RemoveListener(eventBus_, text_overlay_events::kEventLevelToast, levelToastListener_);
    RemoveListener(eventBus_, text_overlay_events::kEventObjectiveChanged, objectiveListener_);
    RemoveListener(eventBus_, "stateChanged", stateChangedListener_);
    RemoveListener(eventBus_, text_overlay_events::kEventLoadingProgress, loadingListener_);
    RemoveListener(eventBus_, "levelLoaded", levelLoadedListener_);
    RemoveListener(eventBus_, text_overlay_events::kEventVoiceLine, voiceLineListener_);
    RemoveListener(eventBus_, text_overlay_events::kEventVoiceStop, voiceStopListener_);
    if (audioManager_ != nullptr && activeVoice_ != nullptr) {
        audioManager_->StopManagedSound(activeVoice_, true);
    }
    activeVoice_.reset();
    audioManager_ = nullptr;
    overlayState_ = nullptr;
    eventBus_ = nullptr;
}

} // namespace ri::world
