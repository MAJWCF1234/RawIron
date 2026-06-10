#include "RawIron/Games/GameTextOverlayHost.h"

#include "RawIron/Games/TextOverlayStandaloneDraw.h"
#include "RawIron/Runtime/RuntimeEventBus.h"

namespace ri::games {

void WireGameTextOverlay(ri::runtime::RuntimeEventBus* const eventBus,
                         GameTextOverlayHost& host,
                         ri::audio::AudioManager* const audioManager) {
    host.bridge.Detach();
    if (eventBus == nullptr) {
        return;
    }
    if (audioManager != nullptr) {
        host.bridge.Attach(*eventBus, host.state, *audioManager);
    } else {
        host.bridge.Attach(*eventBus, host.state);
    }
}

void TickGameTextOverlay(GameTextOverlayHost& host, const float deltaSeconds) {
    host.state.Advance(static_cast<double>(deltaSeconds) * 1000.0);
}

#if defined(_WIN32)
void DrawGameTextOverlay(void* const hwnd, const GameTextOverlayHost& host) {
    DrawTextOverlaySnapshot(hwnd, host.state.Snapshot());
}
#endif

} // namespace ri::games
