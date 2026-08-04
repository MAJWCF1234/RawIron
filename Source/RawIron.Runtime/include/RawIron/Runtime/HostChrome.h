#pragma once

#include "RawIron/Runtime/HostInputService.h"

namespace ri::runtime {

/// Standalone host chrome chords. Defaults match the engine demos; games that need different
/// behaviour (e.g. Escape opens pause instead of quitting) pass a policy or ignore quitRequested.
struct HostChromePolicy {
    int quitKey = 0x1B;              // VK_ESCAPE
    int diagnosticsKey = 'U';
    bool requireCtrlForDiagnostics = true;
    bool requireShiftForDiagnostics = true;
    /// When false, Escape is never reported as quit (LiminalHall routes it to UI itself).
    bool quitOnEscape = true;
};

struct HostChromeActions {
    bool quitRequested = false;
    bool diagnosticsToggled = false;
};

/// Poll shared standalone chrome from a mounted HostInputService.
/// Differentiation of chords can later move into scripts/ui.riscript; until then the policy is the knob.
[[nodiscard]] HostChromeActions PollHostChrome(const HostInputService& input,
                                               const HostChromePolicy& policy = {});

} // namespace ri::runtime
