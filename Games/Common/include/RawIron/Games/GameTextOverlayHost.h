#pragma once

#include "RawIron/World/TextOverlayEventBridge.h"
#include "RawIron/World/TextOverlayState.h"

namespace ri::audio {
class AudioManager;
}

namespace ri::runtime {
class RuntimeEventBus;
}

namespace ri::games {

/// Standalone game HUD host: bridges runtime text events into on-screen GDI overlays.
struct GameTextOverlayHost {
    ri::world::TextOverlayState state;
    ri::world::TextOverlayEventBridge bridge;
};

void WireGameTextOverlay(ri::runtime::RuntimeEventBus* eventBus,
                         GameTextOverlayHost& host,
                         ri::audio::AudioManager* audioManager = nullptr);

void TickGameTextOverlay(GameTextOverlayHost& host, float deltaSeconds);

#if defined(_WIN32)
void DrawGameTextOverlay(void* hwnd, const GameTextOverlayHost& host);
#endif

} // namespace ri::games
