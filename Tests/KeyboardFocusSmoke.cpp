#include "RawIron/Core/KeyboardFocus.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition "\n"; \
            return EXIT_FAILURE; \
        } \
    } while (false)

namespace {

#if defined(_WIN32)
constexpr int kSampleKey = VK_ESCAPE;
constexpr int kOtherKey = VK_SPACE;
#else
constexpr int kSampleKey = 0x1B;
constexpr int kOtherKey = 0x20;
#endif

/// Every read must be inert so that a host with no window - headless runs, benchmark harnesses,
/// tools - can never pick up keystrokes aimed at whatever application actually has focus.
[[nodiscard]] bool ReadsInert(const ri::core::KeyboardFocusGate& gate) {
    return !gate.Focused() && !gate.IsKeyDown(kSampleKey) && !gate.IsKeyDownSettled(kSampleKey)
        && !gate.ConsumeKeyPress(kSampleKey) && !gate.IsKeyDown(kOtherKey);
}

} // namespace

int main() {
    ri::core::KeyboardFocusGate gate;
    CHECK(ReadsInert(gate));
    CHECK(!gate.JustRegainedFocus());

    gate.Update(nullptr);
    CHECK(ReadsInert(gate));
    CHECK(!gate.JustRegainedFocus());

    // A null host stays unfocused no matter how many frames elapse, and an overlay handle alone is
    // never enough to grant input.
    for (int frame = 0; frame < 8; ++frame) {
        gate.Update(nullptr, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xDEADBEEFu)));
        CHECK(ReadsInert(gate));
        CHECK(!gate.JustRegainedFocus());
    }

#if defined(_WIN32)
    // A real but non-foreground window is also inert. The message-only window below can never be
    // activated, which is exactly the "game is in the background" case that used to let keystrokes
    // from other applications drive menus and movement.
    const HWND background = CreateWindowExW(
        0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    CHECK(background != nullptr);
    for (int frame = 0; frame < 8; ++frame) {
        gate.Update(background);
        CHECK(ReadsInert(gate));
        CHECK(!gate.JustRegainedFocus());
    }
    DestroyWindow(background);
#endif

    std::cout << "KeyboardFocusSmoke OK\n";
    return EXIT_SUCCESS;
}
