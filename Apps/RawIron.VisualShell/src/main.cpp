#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Runtime/RuntimeCore.h"

#include "VisualShellTypes.h"

#if defined(_WIN32)
#include "VisualShellDesktop.h"
#include <windows.h>
#endif

#include <filesystem>
#include <exception>
#include <iostream>
#include <utility>

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
        ri::runtime::RuntimePaths paths = ri::runtime::DetectRuntimePaths(sourceRoot.empty() ? fs::current_path() : sourceRoot);
        ri::runtime::RuntimeCore runtime(
            ri::runtime::RuntimeIdentity{
                .id = "rawiron.visualshell",
                .displayName = "RawIron.VisualShell",
                .mode = "tool",
                .instanceId = {},
            },
            std::move(paths));
        if (!runtime.Startup(commandLine)) {
            return 1;
        }
        (void)runtime.Frame(ri::core::FrameContext{
            .frameIndex = 0,
            .deltaSeconds = 0.0,
            .elapsedSeconds = 0.0,
            .realtimeSeconds = 0.0,
            .realDeltaSeconds = 0.0,
        });

        if (commandLine.HasFlag("--headless")) {
            PrintHeadlessSummary(shell);
            runtime.Shutdown();
            return 0;
        }

#if defined(_WIN32)
        if (HWND console = GetConsoleWindow(); console != nullptr) {
            ShowWindow(console, SW_HIDE);
        }

        shell.AppendLog("Visual shell ready.");
        shell.AppendLog("Use Up/Down for actions, Left/Right for Scene Kit examples, and Enter to run tools.");
        const int result = RunVisualShellDesktopUi(shell, GetModuleHandleW(nullptr));
        runtime.Shutdown();
        return result;
#else
        PrintHeadlessSummary(shell);
        runtime.Shutdown();
        return 0;
#endif
    } catch (const std::exception&) {
        ri::core::LogCurrentExceptionWithStackTrace("VisualShell Failure");
        return 1;
    }
}
