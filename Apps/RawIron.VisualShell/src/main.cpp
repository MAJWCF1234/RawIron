#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Log.h"

#include "VisualShellTypes.h"

#if defined(_WIN32)
#include "VisualShellDesktop.h"
#include <windows.h>
#endif

#include <filesystem>
#include <exception>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    ri::core::InitializeCrashDiagnostics();
    try {
        const ri::core::CommandLine commandLine(argc, argv);
        const fs::path executablePath = ResolveExecutablePath();
        const fs::path buildRoot = FindBuildRoot(executablePath);
        const fs::path sourceRoot =
            FindSourceRoot(buildRoot.empty() ? executablePath.parent_path() : buildRoot.parent_path());
        ShellState shell(buildRoot, sourceRoot);

        if (commandLine.HasFlag("--headless")) {
            PrintHeadlessSummary(shell);
            return 0;
        }

#if defined(_WIN32)
        if (HWND console = GetConsoleWindow(); console != nullptr) {
            ShowWindow(console, SW_HIDE);
        }

        shell.AppendLog("Visual shell ready.");
        shell.AppendLog("Use Up/Down for actions, Left/Right for Scene Kit examples, and Enter to run tools.");
        return RunVisualShellDesktopUi(shell, GetModuleHandleW(nullptr));
#else
        PrintHeadlessSummary(shell);
        return 0;
#endif
    } catch (const std::exception&) {
        ri::core::LogCurrentExceptionWithStackTrace("VisualShell Failure");
        return 1;
    }
}
