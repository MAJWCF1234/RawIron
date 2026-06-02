#include "EditorPlaytestLauncher.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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
#endif

namespace ri::editor {

namespace fs = std::filesystem;

#if defined(_WIN32)
namespace {

std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    const int written = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), needed);
    if (written != needed) {
        return {};
    }
    return wide;
}

std::string PathToUtf8(const fs::path& path) {
    const std::u8string u8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

fs::path ResolveEditorModulePath() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return fs::path(buffer);
}

fs::path FindBuildRoot(const fs::path& executablePath) {
    fs::path current = executablePath.parent_path();
    while (!current.empty()) {
        std::error_code ec{};
        if (fs::exists(current / "CMakeCache.txt", ec)) {
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

std::optional<std::string> TryResolveCurrentBuildConfiguration(const fs::path& editorExe) {
    const fs::path parent = editorExe.parent_path();
    const fs::path owner = parent.parent_path();
    if (owner.filename() == "RawIron.Editor") {
        const std::string configuration = parent.filename().string();
        if (!configuration.empty()) {
            return configuration;
        }
    }
    return std::nullopt;
}

std::string QuoteCommandLineArgument(std::string_view value) {
    if (value.find_first_of(" \t\"") == std::string_view::npos) {
        return std::string(value);
    }
    std::string quoted;
    quoted.reserve(value.size() + 8U);
    quoted.push_back('"');
    for (const char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('"');
    return quoted;
}

std::wstring BuildShellExecuteParameterString(const std::vector<std::string>& args) {
    std::string joined;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index > 0U) {
            joined.push_back(' ');
        }
        joined += QuoteCommandLineArgument(args[index]);
    }
    return Utf8ToWide(joined);
}

fs::path ResolveBundledGameExecutable(const ri::content::GameManifest& manifest) {
    const fs::path editorExe = ResolveEditorModulePath();
    if (editorExe.empty()) {
        return {};
    }
    const fs::path buildRoot = FindBuildRoot(editorExe);
    if (buildRoot.empty()) {
        return {};
    }

    const std::optional<std::string> configuration = TryResolveCurrentBuildConfiguration(editorExe);
    std::vector<fs::path> candidates;
    const std::string executableName = manifest.entry + ".exe";
    const fs::path gameFolderName = manifest.rootPath.filename();

    const auto appendCandidate = [&candidates](const fs::path& candidate) {
        if (!candidate.empty()) {
            candidates.push_back(candidate);
        }
    };
    const auto appendConfigCandidate = [&appendCandidate, &configuration](fs::path base) {
        if (configuration.has_value()) {
            appendCandidate(base / *configuration);
        }
        appendCandidate(std::move(base));
    };

    appendConfigCandidate(buildRoot / "Games" / gameFolderName / "App" / executableName);
    appendConfigCandidate(buildRoot / "Apps" / manifest.entry / executableName);

    for (const fs::path& candidate : candidates) {
        std::error_code ec{};
        if (!candidate.empty() && fs::exists(candidate, ec)) {
            return fs::weakly_canonical(candidate, ec);
        }
    }
    return {};
}

} // namespace
#endif

PlaytestLaunchResult LaunchPlaytestForManifest(void* nativeWindowHandle,
                                               const ri::content::GameManifest& manifest,
                                               const fs::path& workspaceRoot) {
    PlaytestLaunchResult result{};
#if defined(_WIN32)
    const fs::path gameExe = ResolveBundledGameExecutable(manifest);
    if (gameExe.empty()) {
        result.message = "Play: could not find built executable for " + manifest.entry + ".";
        return result;
    }

    std::vector<std::string> args{};
    args.push_back("--game-root");
    args.push_back(PathToUtf8(manifest.rootPath));
    args.push_back("--workspace-root");
    args.push_back(PathToUtf8(workspaceRoot));

    const std::wstring exePath = gameExe.wstring();
    const std::wstring parameters = BuildShellExecuteParameterString(args);
    const std::wstring cwd = workspaceRoot.wstring();
    const HINSTANCE shellResult = ShellExecuteW(static_cast<HWND>(nativeWindowHandle),
                                                L"open",
                                                exePath.c_str(),
                                                parameters.empty() ? nullptr : parameters.c_str(),
                                                cwd.empty() ? nullptr : cwd.c_str(),
                                                SW_SHOWNORMAL);
    result.launched = reinterpret_cast<INT_PTR>(shellResult) > 32;
    result.message = result.launched
        ? "Play: launched " + manifest.name + "."
        : "Play: could not start " + manifest.entry + ".";
#else
    (void)nativeWindowHandle;
    (void)manifest;
    (void)workspaceRoot;
    result.message = "Play: supported on Windows builds.";
#endif
    return result;
}

} // namespace ri::editor
