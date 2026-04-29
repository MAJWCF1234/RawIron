#pragma once

#if defined(_WIN32)
#include <windows.h>

class ShellState;

/// Shows the workshop window and runs the Win32 message loop.
[[nodiscard]] int RunVisualShellDesktopUi(ShellState& shell, HINSTANCE instance);
#endif
