#pragma once

#include "RawIron/Runtime/RuntimeEventBus.h"
#include "RawIron/World/TextOverlayState.h"

#include <memory>
#include <optional>

namespace ri::audio {
class AudioManager;
class ManagedSound;
}

namespace ri::world {

/// Bridges semantic runtime events to `TextOverlayState` updates.
class TextOverlayEventBridge {
public:
    TextOverlayEventBridge() = default;
    ~TextOverlayEventBridge();

    void Attach(ri::runtime::RuntimeEventBus& eventBus, TextOverlayState& overlayState);
    void Attach(ri::runtime::RuntimeEventBus& eventBus,
                TextOverlayState& overlayState,
                ri::audio::AudioManager& audioManager);
    void Detach();

private:
    ri::runtime::RuntimeEventBus* eventBus_ = nullptr;
    TextOverlayState* overlayState_ = nullptr;
    ri::audio::AudioManager* audioManager_ = nullptr;
    std::shared_ptr<ri::audio::ManagedSound> activeVoice_{};

    std::optional<ri::runtime::RuntimeEventBus::ListenerId> messageListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> subtitleListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> levelToastListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> objectiveListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> stateChangedListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> loadingListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> levelLoadedListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> voiceLineListener_;
    std::optional<ri::runtime::RuntimeEventBus::ListenerId> voiceStopListener_;
};

} // namespace ri::world
