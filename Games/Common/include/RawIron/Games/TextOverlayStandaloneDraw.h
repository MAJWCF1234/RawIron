#pragma once

#include "RawIron/World/TextOverlayState.h"

namespace ri::games {

#if defined(_WIN32)
/// Draws active `TextOverlaySnapshot` channels over a standalone Vulkan HWND.
void DrawTextOverlaySnapshot(void* hwnd, const ri::world::TextOverlaySnapshot& snapshot);
#endif

} // namespace ri::games
