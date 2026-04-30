#include "VisualShellTypes.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif

namespace fs = std::filesystem;
#if defined(RAWIRON_CTEST_COMMAND)
constexpr const char* kCtestCommand = RAWIRON_CTEST_COMMAND;
#else
constexpr const char* kCtestCommand = "ctest";
#endif

#if defined(RAWIRON_BUILD_CONFIGURATION)
constexpr const char* kBuildConfiguration = RAWIRON_BUILD_CONFIGURATION;
#else
constexpr const char* kBuildConfiguration = "";
#endif

#if defined(RAWIRON_IS_MULTI_CONFIG)
constexpr bool kIsMultiConfig = RAWIRON_IS_MULTI_CONFIG != 0;
#else
constexpr bool kIsMultiConfig = false;
#endif
std::wstring Widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int nbytes = static_cast<int>(value.size());
    int wideChars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), nbytes, nullptr, 0);
    if (wideChars <= 0) {
        wideChars = MultiByteToWideChar(CP_UTF8, 0, value.data(), nbytes, nullptr, 0);
    }
    if (wideChars <= 0) {
        std::wstring legacy;
        legacy.reserve(static_cast<std::size_t>(value.size()));
        for (unsigned char ch : value) {
            legacy.push_back(static_cast<wchar_t>(ch));
        }
        return legacy;
    }
    std::wstring out(static_cast<std::size_t>(wideChars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), nbytes, out.data(), wideChars);
    return out;
}

std::string Narrow(const fs::path& path) {
    return path.string();
}

std::string QuoteArgument(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) {
        return value;
    }

    std::string quoted = "\"";
    for (const char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += '"';
    return quoted;
}

std::string TrimLine(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

fs::path ResolveExecutablePath() {
#if defined(_WIN32)
    std::array<char, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    return fs::path(std::string(buffer.data(), length));
#else
    return fs::current_path() / "RawIron.VisualShell";
#endif
}

fs::path FindBuildRoot(const fs::path& executablePath) {
    fs::path current = executablePath.parent_path();
    while (!current.empty()) {
        if (fs::exists(current / "CMakeCache.txt")) {
            return current;
        }
        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

fs::path FindSourceRoot(const fs::path& start) {
    fs::path current = start;
    while (!current.empty()) {
        if (fs::exists(current / "Source") &&
            fs::exists(current / "Documentation") &&
            fs::exists(current / "CMakeLists.txt")) {
            return current;
        }

        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return {};
}

fs::path PreviewOutputPath(const fs::path& sourceRoot) {
    if (!sourceRoot.empty()) {
        return sourceRoot / "Saved" / "Previews" / "visual_shell_cube.bmp";
    }
    return fs::current_path() / "visual_shell_cube.bmp";
}

fs::path BuildTreeBinaryPath(const fs::path& buildRoot, const fs::path& relativeDir, const std::string& filename) {
    fs::path path = buildRoot / relativeDir;
    if (kIsMultiConfig && std::string_view(kBuildConfiguration).size() > 0) {
        path /= kBuildConfiguration;
    }
    path /= filename;
    return path;
}

fs::path ResolveBuiltBinaryPath(const fs::path& buildRoot, const fs::path& relativeDir, const std::string& filename) {
    const fs::path primary = BuildTreeBinaryPath(buildRoot, relativeDir, filename);
    if (fs::exists(primary)) {
        return primary;
    }
#if defined(_WIN32)
    static constexpr const char* kMsConfigs[] = {"RelWithDebInfo", "Release", "Debug", "MinSizeRel"};
    for (const char* cfg : kMsConfigs) {
        const fs::path candidate = buildRoot / relativeDir / cfg / filename;
        if (fs::exists(candidate)) {
            return candidate;
        }
    }
#endif
    const fs::path flat = buildRoot / relativeDir / filename;
    if (fs::exists(flat)) {
        return flat;
    }
    return primary;
}
std::vector<Action> BuildActions(const fs::path& buildRoot, const fs::path& sourceRoot) {
#if defined(_WIN32)
    const std::string exeSuffix = ".exe";
#else
    const std::string exeSuffix;
#endif

    std::vector<Action> actions;
    actions.push_back(Action{
        "Preview Window",
        "Open the selected Scene Kit preview window.",
        Action::Kind::LaunchDetached,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Apps") / "RawIron.Preview", "RawIron.Preview" + exeSuffix),
        {},
        true,
    });
    actions.push_back(Action{
        "Level Editor",
        "Launch the native RawIron editor shell.",
        Action::Kind::LaunchDetached,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Apps") / "RawIron.Editor", "RawIron.Editor" + exeSuffix),
        {
            "--editor-ui",
        },
    });
    actions.push_back(Action{
        "Standalone Player",
        "Launch the native RawIron player shell.",
        Action::Kind::LaunchDetached,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Apps") / "RawIron.Player", "RawIron.Player" + exeSuffix),
        {},
    });
    actions.push_back(Action{
        "Liminal Hall Game",
        "Launch the Liminal Hall test game (Vulkan, 1600x900, workspace root set).",
        Action::Kind::LaunchDetached,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Games") / "LiminalHall" / "App", "RawIron.LiminalGame" + exeSuffix),
        {
            "--renderer=vulkan",
            "--width=1600",
            "--height=900",
            "--workspace-root",
            Narrow(sourceRoot),
        },
    });
    actions.push_back(Action{
        "Liminal Vulkan FPS Sample",
        "Present 300 frames at 1600x900 and append average Vulkan FPS lines to this log.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Games") / "LiminalHall" / "App", "RawIron.LiminalGame" + exeSuffix),
        {
            "--renderer=vulkan",
            "--width=1600",
            "--height=900",
            "--benchmark-frames=300",
            "--workspace-root",
            Narrow(sourceRoot),
        },
    });
    actions.push_back(Action{
        "Save Preview Snapshot",
        "Render the selected Scene Kit example into Saved/Previews.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Apps") / "RawIron.Preview", "RawIron.Preview" + exeSuffix),
        {
            "--headless",
            "--output",
            Narrow(PreviewOutputPath(sourceRoot)),
            "--width",
            "768",
            "--height",
            "768",
        },
        true,
    });
    actions.push_back(Action{
        "Vulkan Diagnostics",
        "Check the Vulkan loader and current runtime status.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Tools") / "ri_tool", "ri_tool" + exeSuffix),
        {
            "--vulkan-diagnostics",
        },
    });
    actions.push_back(Action{
        "Scene Kit Checks",
        "Run the ten Scene Kit example checks and render previews.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Tools") / "ri_tool", "ri_tool" + exeSuffix),
        {
            "--scenekit-checks",
            "--root",
            Narrow(sourceRoot),
        },
    });
    actions.push_back(Action{
        "Scene Kit Targets",
        "List the tracked RawIron Scene Kit parity examples.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Tools") / "ri_tool", "ri_tool" + exeSuffix),
        {
            "--scenekit-targets",
        },
    });
    actions.push_back(Action{
        "Workspace Check",
        "Validate the RawIron workspace configuration and structure.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Tools") / "ri_tool", "ri_tool" + exeSuffix),
        {
            "--workspace",
        },
    });
    actions.push_back(Action{
        "Asset Formats",
        "List supported asset formats and conversion capabilities.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Tools") / "ri_tool", "ri_tool" + exeSuffix),
        {
            "--formats",
        },
    });
    actions.push_back(Action{
        "Sample Scene Dump",
        "Output the current sample scene structure for inspection.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Tools") / "ri_tool", "ri_tool" + exeSuffix),
        {
            "--sample-scene",
        },
    });
    actions.push_back(Action{
        "Core Tests",
        "Run RawIron.Core native tests directly.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Tests") / "RawIron.Core.Tests", "RawIron.Core.Tests" + exeSuffix),
        {},
    });
    actions.push_back(Action{
        "Engine Import Tests",
        "Run the prototype-port import test suite.",
        Action::Kind::RunCaptured,
        ResolveBuiltBinaryPath(buildRoot, fs::path("Tests") / "RawIron.EngineImport.Tests",
                            "RawIron.EngineImport.Tests" + exeSuffix),
        {},
    });
    actions.push_back(Action{
        "Full Test Sweep",
        "Run ctest across the whole current build tree.",
        Action::Kind::RunCaptured,
        fs::path(kCtestCommand),
        {
            "--test-dir",
            Narrow(buildRoot),
            "--output-on-failure",
        },
    });
    if (kIsMultiConfig && std::string_view(kBuildConfiguration).size() > 0) {
        actions.back().args.push_back("-C");
        actions.back().args.push_back(kBuildConfiguration);
    }
    actions.push_back(Action{
        "Open Documentation",
        "Open the Obsidian documentation vault folder.",
        Action::Kind::OpenFolder,
        sourceRoot / "Documentation",
        {},
    });
    actions.push_back(Action{
        "Open Previews Folder",
        "Open the preview output folder.",
        Action::Kind::OpenFolder,
        PreviewOutputPath(sourceRoot).parent_path(),
        {},
    });
    return actions;
}
std::optional<std::size_t> FindActionIndex(const std::vector<Action>& actions, std::string_view substring) {
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (actions[i].title.find(substring) != std::string::npos) {
            return i;
        }
    }
    return std::nullopt;
}

bool TargetReady(const Action& action) {
    if (action.kind == Action::Kind::RunCaptured || action.kind == Action::Kind::LaunchDetached) {
        if (action.target.filename() == fs::path(kCtestCommand).filename()) {
            return true;
        }
    }
    return !action.target.empty() && fs::exists(action.target);
}

std::string BuildCommandLine(const fs::path& target, const std::vector<std::string>& args) {
    std::string command = QuoteArgument(Narrow(target));
    for (const std::string& arg : args) {
        command += " ";
        command += QuoteArgument(arg);
    }
    command += " 2>&1";
    return command;
}

std::vector<std::string> ReadProcessOutput(const fs::path& target, const std::vector<std::string>& args, int& exitCode) {
    std::vector<std::string> lines;
    const std::string command = BuildCommandLine(target, args);

#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        exitCode = -1;
        lines.push_back("Unable to start process.");
        return lines;
    }

    std::array<char, 512> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        const std::string line = TrimLine(buffer.data());
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

#if defined(_WIN32)
    exitCode = _pclose(pipe);
#else
    exitCode = pclose(pipe);
#endif
    return lines;
}

#if defined(_WIN32)
bool LaunchDetachedProcess(const fs::path& target, const std::vector<std::string>& args) {
    if (!fs::exists(target)) {
        return false;
    }

    std::string command = QuoteArgument(Narrow(target));
    for (const std::string& arg : args) {
        command += " ";
        command += QuoteArgument(arg);
    }

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    std::vector<char> writable(command.begin(), command.end());
    writable.push_back('\0');

    const BOOL created = CreateProcessA(
        nullptr,
        writable.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);
    if (!created) {
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

bool OpenFolder(const fs::path& target) {
    if (target.empty()) {
        return false;
    }

    fs::create_directories(target);
    const std::wstring wideTarget = target.wstring();
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", wideTarget.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}
#else
bool LaunchDetachedProcess(const fs::path&, const std::vector<std::string>&) {
    return false;
}

bool OpenFolder(const fs::path&) {
    return false;
}
#endif

std::string ActionReadyLabel(const Action& action) {
    return action.title + (TargetReady(action) ? " [ready]" : " [missing]");
}

void PrintHeadlessSummary(const ShellState& shell) {
    std::cout << "[Shell Ready]\n";
    std::cout << "RawIron Visual Shell\n";
    std::cout << "Build root: " << shell.BuildRoot().string() << "\n";
    std::cout << "Source root: " << shell.SourceRoot().string() << "\n";
    int index = 1;
    for (const Action& action : shell.Actions()) {
        std::cout << index << ". " << ActionReadyLabel(action) << "\n";
        ++index;
    }
}
