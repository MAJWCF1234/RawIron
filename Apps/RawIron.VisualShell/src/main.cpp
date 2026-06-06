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
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace {

std::vector<std::string> BuildShellProjectCreateArgs(const ri::core::CommandLine& commandLine,
                                                     const fs::path& sourceRoot) {
    const std::optional<std::string> projectId = commandLine.GetValue("--new-project");
    if (!projectId.has_value() || projectId->empty()) {
        throw std::runtime_error("--new-project requires a project id.");
    }

    std::vector<std::string> args = {
        "--create-project",
        *projectId,
    };

    const auto appendOptionIfPresent = [&](std::string_view option) {
        if (const std::optional<std::string> value = commandLine.GetValue(option);
            value.has_value() && !value->empty()) {
            args.emplace_back(option);
            args.push_back(*value);
        }
    };

    appendOptionIfPresent("--name");
    appendOptionIfPresent("--author");
    appendOptionIfPresent("--type");
    appendOptionIfPresent("--version");
    appendOptionIfPresent("--description");
    appendOptionIfPresent("--game-root");

    args.emplace_back("--root");
    args.push_back(sourceRoot.empty() ? fs::current_path().string() : sourceRoot.string());
    return args;
}

} // namespace

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

        if (commandLine.HasFlag("--list-actions")) {
            PrintActionCatalog(shell, std::cout);
            runtime.Shutdown();
            return 0;
        }

        if (commandLine.HasFlag("--list-projects")) {
            const int result = RunRiToolThroughShell(buildRoot,
                                                     {
                                                         "--list-projects",
                                                         "--root",
                                                         sourceRoot.empty() ? fs::current_path().string() : sourceRoot.string(),
                                                     },
                                                     std::cout);
            runtime.Shutdown();
            return result;
        }

        if (const std::optional<std::string> game = commandLine.GetValue("--describe-project");
            game.has_value()) {
            const int result = RunRiToolThroughShell(buildRoot,
                                                     {
                                                         "--describe-project",
                                                         "--game",
                                                         *game,
                                                         "--root",
                                                         sourceRoot.empty() ? fs::current_path().string() : sourceRoot.string(),
                                                     },
                                                     std::cout);
            runtime.Shutdown();
            return result;
        }

        if (const std::optional<std::string> game = commandLine.GetValue("--list-project-resources");
            game.has_value()) {
            std::vector<std::string> args = {
                "--list-project-resources",
                "--game",
                *game,
                "--root",
                sourceRoot.empty() ? fs::current_path().string() : sourceRoot.string(),
            };
            if (const std::optional<std::string> category = commandLine.GetValue("--resource-category");
                category.has_value() && !category->empty()) {
                args.emplace_back("--resource-category");
                args.push_back(*category);
            }
            const int result = RunRiToolThroughShell(buildRoot, args, std::cout);
            runtime.Shutdown();
            return result;
        }

        if (const std::optional<std::string> game = commandLine.GetValue("--doctor-project");
            game.has_value()) {
            const int result = RunRiToolThroughShell(buildRoot,
                                                     {
                                                         "--doctor-project",
                                                         "--game",
                                                         *game,
                                                         "--root",
                                                         sourceRoot.empty() ? fs::current_path().string() : sourceRoot.string(),
                                                     },
                                                     std::cout);
            runtime.Shutdown();
            return result;
        }

        if (const std::optional<std::string> game = commandLine.GetValue("--probe-open-editor");
            game.has_value()) {
            const ri::shell::GameProject* project = FindGameProjectById(shell, *game);
            if (project == nullptr) {
                std::cerr << "Unknown game project id: " << *game << "\n";
                runtime.Shutdown();
                return 2;
            }
            std::cout << "[Shell Probe]\n";
            std::cout << "Mode: open-editor\n";
            std::cout << "Project id: " << project->id << "\n";
            std::cout << "Project root: " << project->root.string() << "\n";
            std::cout << "Editor target: " << shell.ResolveEditorBinaryPath().string() << "\n";
            std::cout << "Ready: " << (shell.CanLaunchEditorForGame(*project) ? "yes" : "no") << "\n";
            runtime.Shutdown();
            return shell.CanLaunchEditorForGame(*project) ? 0 : 1;
        }

        if (const std::optional<std::string> game = commandLine.GetValue("--probe-open-folder");
            game.has_value()) {
            const ri::shell::GameProject* project = FindGameProjectById(shell, *game);
            if (project == nullptr) {
                std::cerr << "Unknown game project id: " << *game << "\n";
                runtime.Shutdown();
                return 2;
            }
            std::cout << "[Shell Probe]\n";
            std::cout << "Mode: open-folder\n";
            std::cout << "Project id: " << project->id << "\n";
            std::cout << "Project root: " << project->root.string() << "\n";
            std::cout << "Ready: " << (shell.CanOpenGameFolder(*project) ? "yes" : "no") << "\n";
            runtime.Shutdown();
            return shell.CanOpenGameFolder(*project) ? 0 : 1;
        }

        if (const std::optional<std::string> game = commandLine.GetValue("--open-editor");
            game.has_value()) {
            const ri::shell::GameProject* project = FindGameProjectById(shell, *game);
            if (project == nullptr) {
                std::cerr << "Unknown game project id: " << *game << "\n";
                runtime.Shutdown();
                return 2;
            }
#if defined(_WIN32)
            const bool ok = shell.LaunchEditorForGame(*project, nullptr);
            runtime.Shutdown();
            return ok ? 0 : 1;
#else
            std::cerr << "--open-editor is only supported on Windows desktop builds.\n";
            runtime.Shutdown();
            return 1;
#endif
        }

        if (const std::optional<std::string> game = commandLine.GetValue("--open-folder");
            game.has_value()) {
            const ri::shell::GameProject* project = FindGameProjectById(shell, *game);
            if (project == nullptr) {
                std::cerr << "Unknown game project id: " << *game << "\n";
                runtime.Shutdown();
                return 2;
            }
#if defined(_WIN32)
            const bool ok = shell.OpenGameFolder(*project, nullptr);
            runtime.Shutdown();
            return ok ? 0 : 1;
#else
            std::cerr << "--open-folder is only supported on Windows desktop builds.\n";
            runtime.Shutdown();
            return 1;
#endif
        }

        if (commandLine.HasFlag("--open-console")) {
#if defined(_WIN32)
            const bool ok = shell.OpenConsole(nullptr);
            runtime.Shutdown();
            return ok ? 0 : 1;
#else
            std::cerr << "--open-console is only supported on Windows desktop builds.\n";
            runtime.Shutdown();
            return 1;
#endif
        }

        if (commandLine.GetValue("--new-project").has_value()) {
            const int result = RunRiToolThroughShell(buildRoot,
                                                     BuildShellProjectCreateArgs(commandLine, sourceRoot),
                                                     std::cout);
            runtime.Shutdown();
            return result;
        }

        if (commandLine.HasFlag("--ri-tool")) {
            std::vector<std::string> forwardedArgs;
            const std::vector<std::string>& args = commandLine.Args();
            bool forward = false;
            for (std::size_t index = 1; index < args.size(); ++index) {
                if (forward) {
                    forwardedArgs.push_back(args[index]);
                    continue;
                }
                if (args[index] == "--ri-tool") {
                    forward = true;
                }
            }
            const int result = RunRiToolThroughShell(buildRoot, forwardedArgs, std::cout);
            runtime.Shutdown();
            return result;
        }

        if (const std::optional<std::string> actionSelector = commandLine.GetValue("--run-action");
            actionSelector.has_value()) {
            const std::optional<std::size_t> actionIndex = ResolveActionSelector(shell.Actions(), *actionSelector);
            if (!actionIndex.has_value()) {
                std::cerr << "Unknown shell action selector: " << *actionSelector << "\n";
                runtime.Shutdown();
                return 2;
            }
            const int result = RunHeadlessAction(shell, *actionIndex, std::cout);
            runtime.Shutdown();
            return result;
        }

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
