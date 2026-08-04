#include "RawIron/Runtime/HostChrome.h"

namespace ri::runtime {

HostChromeActions PollHostChrome(const HostInputService& input, const HostChromePolicy& policy) {
    HostChromeActions actions{};
    if (policy.quitOnEscape && input.ConsumeKeyPress(policy.quitKey)) {
        actions.quitRequested = true;
    }
    const bool diagnosticsPressed = input.ConsumeKeyPress(policy.diagnosticsKey);
    const bool ctrlOk = !policy.requireCtrlForDiagnostics || input.IsKeyDown(0x11);   // VK_CONTROL
    const bool shiftOk = !policy.requireShiftForDiagnostics || input.IsKeyDown(0x10); // VK_SHIFT
    if (diagnosticsPressed && ctrlOk && shiftOk) {
        actions.diagnosticsToggled = true;
    }
    return actions;
}

} // namespace ri::runtime
