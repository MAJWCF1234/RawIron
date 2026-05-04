#pragma once

#include "RawIron/Scene/SceneKit.h"
#include "ShellWorkshop.h"

#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

fs::path ResolveExecutablePath();
fs::path FindBuildRoot(const fs::path& executablePath);
fs::path FindSourceRoot(const fs::path& start);
fs::path PreviewOutputPath(const fs::path& sourceRoot);

struct Action {
    enum class Kind {
        RunCaptured,
        LaunchDetached,
        OpenFolder,
    };

    std::string title;
    std::string detail;
    Kind kind = Kind::RunCaptured;
    fs::path target;
    std::vector<std::string> args;
    bool injectCurrentExample = false;
};
std::vector<Action> BuildActions(const fs::path& buildRoot, const fs::path& sourceRoot);

fs::path BuildTreeBinaryPath(const fs::path& buildRoot, const fs::path& relativeDir, const std::string& filename);
/// Prefer the primary CMake output path; if missing, try other configs and flat layout (MSVC / mixed trees).
fs::path ResolveBuiltBinaryPath(const fs::path& buildRoot, const fs::path& relativeDir, const std::string& filename);
std::optional<std::size_t> FindActionIndex(const std::vector<Action>& actions, std::string_view substring);
bool TargetReady(const Action& action);
std::string ActionReadyLabel(const Action& action);
std::wstring Widen(const std::string& value);
std::string Narrow(const fs::path& path);
std::string QuoteArgument(const std::string& value);
std::string TrimLine(std::string value);
std::string BuildCommandLine(const fs::path& target, const std::vector<std::string>& args);
std::vector<std::string> ReadProcessOutput(const fs::path& target, const std::vector<std::string>& args, int& exitCode);
#if defined(_WIN32)
bool LaunchDetachedProcess(const fs::path& target, const std::vector<std::string>& args);
bool OpenFolder(const fs::path& target);
#else
bool LaunchDetachedProcess(const fs::path& target, const std::vector<std::string>& args);
bool OpenFolder(const fs::path& target);
#endif

class ShellState {
public:
    ShellState(fs::path buildRoot, fs::path sourceRoot)
        : buildRoot_(std::move(buildRoot)),
          sourceRoot_(std::move(sourceRoot)),
          actions_(BuildActions(buildRoot_, sourceRoot_)) {
        sceneKitOptions_.assetRoot = sourceRoot_ / "Assets" / "Source";
        AppendLog("Shell initialized.");
#if defined(_WIN32)
        {
            const fs::path riProbe =
                ResolveBuiltBinaryPath(buildRoot_, fs::path("Tools") / "ri_tool", "ri_tool.exe");
            if (!fs::exists(riProbe)) {
                AppendLog("Tip: ri_tool.exe not present in this build dir. Example: cmake --build <build-dir> "
                          "--target ri_tool --config RelWithDebInfo.");
            }
        }
#endif
        sceneKitExamples_ = ri::scene::RunSceneKitMilestoneChecks(sceneKitOptions_);
        if (sceneKitExamples_.empty()) {
            AppendLog("Scene Kit examples unavailable. Falling back to the lit cube preview.");
        } else {
            AppendLog("Scene Kit preview index loaded: " + std::to_string(sceneKitExamples_.size()) + " scenes.");
        }
        gameProjects_ = ri::shell::EnumerateGameProjects(sourceRoot_);
        ri::shell::LoadRecentSessionPaths(sourceRoot_, recentSessions_);
    }

    ~ShellState() {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    const std::vector<Action>& Actions() const noexcept {
        return actions_;
    }

    std::size_t SelectedExampleIndex() const noexcept {
        return selectedExampleIndex_;
    }

    std::size_t ExampleCount() const noexcept {
        return sceneKitExamples_.size();
    }

    const ri::scene::SceneKitMilestoneResult* CurrentExample() const noexcept {
        if (sceneKitExamples_.empty()) {
            return nullptr;
        }
        return &sceneKitExamples_.at(selectedExampleIndex_);
    }

    fs::path BuildRoot() const noexcept {
        return buildRoot_;
    }

    fs::path SourceRoot() const noexcept {
        return sourceRoot_;
    }

    std::size_t SelectedIndex() const noexcept {
        return selectedIndex_;
    }

    void SetSelectedIndex(std::size_t index) noexcept {
        if (!actions_.empty()) {
            selectedIndex_ = index % actions_.size();
        }
    }

    const std::vector<ri::shell::GameProject>& GameProjects() const noexcept {
        return gameProjects_;
    }

    const std::vector<std::string>& RecentSessions() const noexcept {
        return recentSessions_;
    }

    void ReloadRecentSessions() {
        ri::shell::LoadRecentSessionPaths(sourceRoot_, recentSessions_);
    }

#if defined(_WIN32)
    bool LaunchEditorForGame(const ri::shell::GameProject& game, void* windowHandle) {
        const std::string exeSuffix = ".exe";
        const fs::path editor =
            ResolveBuiltBinaryPath(buildRoot_, fs::path("Apps") / "RawIron.Editor", "RawIron.Editor" + exeSuffix);
        if (!fs::exists(editor)) {
            AppendLog("Editor binary missing.");
            RequestRedraw(windowHandle);
            return false;
        }
        std::vector<std::string> args = {
            "--editor-ui",
            "--game=" + game.id,
        };
        ri::shell::TouchRecentPath(sourceRoot_, game.root);
        ReloadRecentSessions();
        if (LaunchDetachedProcess(editor, args)) {
            AppendLog("Launched editor for " + game.name + " (" + game.id + ").");
            RequestRedraw(windowHandle);
            return true;
        }
        AppendLog("Could not launch editor.");
        RequestRedraw(windowHandle);
        return false;
    }

    bool OpenGameFolder(const ri::shell::GameProject& game, void* windowHandle) {
        if (OpenFolder(game.root)) {
            AppendLog("Opened folder: " + game.id);
            RequestRedraw(windowHandle);
            return true;
        }
        AppendLog("Could not open game folder.");
        RequestRedraw(windowHandle);
        return false;
    }
#else
    bool LaunchEditorForGame(const ri::shell::GameProject&, void*) {
        return false;
    }
    bool OpenGameFolder(const ri::shell::GameProject&, void*) {
        return false;
    }
#endif

    void MoveSelection(int delta) {
        if (actions_.empty()) {
            return;
        }

        const int count = static_cast<int>(actions_.size());
        int next = static_cast<int>(selectedIndex_) + delta;
        while (next < 0) {
            next += count;
        }
        selectedIndex_ = static_cast<std::size_t>(next % count);
    }

    void CycleExample(int delta) {
        if (sceneKitExamples_.empty()) {
            return;
        }

        const int count = static_cast<int>(sceneKitExamples_.size());
        int next = static_cast<int>(selectedExampleIndex_) + delta;
        while (next < 0) {
            next += count;
        }
        selectedExampleIndex_ = static_cast<std::size_t>(next % count);
    }

    bool Busy() const noexcept {
        return busy_.load();
    }

    std::deque<std::string> SnapshotLogs() const {
        std::scoped_lock lock(logMutex_);
        return logs_;
    }

    void AppendLog(const std::string& line) {
        std::scoped_lock lock(logMutex_);
        logs_.push_back(line);
        while (logs_.size() > 24) {
            logs_.pop_front();
        }
    }

    void LaunchSelected(void* windowHandle) {
        if (Busy()) {
            AppendLog("Shell is busy. Wait for the current action to finish.");
            RequestRedraw(windowHandle);
            return;
        }

        if (worker_.joinable()) {
            worker_.join();
        }

        const std::size_t actionIndex = selectedIndex_;
        busy_.store(true);
        worker_ = std::thread([this, actionIndex, windowHandle]() {
            ExecuteAction(actions_.at(actionIndex), windowHandle);
            busy_.store(false);
            RequestRedraw(windowHandle);
        });
    }

private:
    static void RequestRedraw(void* windowHandle) {
#if defined(_WIN32)
        if (windowHandle != nullptr) {
            InvalidateRect(static_cast<HWND>(windowHandle), nullptr, FALSE);
        }
#else
        (void)windowHandle;
#endif
    }

    void ExecuteAction(const Action& action, void* windowHandle) {
        AppendLog("> " + action.title);
        RequestRedraw(windowHandle);

        std::vector<std::string> effectiveArgs = action.args;
        if (action.injectCurrentExample) {
            if (const auto* example = CurrentExample(); example != nullptr) {
                effectiveArgs.push_back("--example");
                effectiveArgs.push_back(example->slug);
            }
        }

        if (action.kind == Action::Kind::OpenFolder) {
            if (OpenFolder(action.target)) {
                AppendLog("Opened: " + action.target.string());
            } else {
                AppendLog("Could not open folder.");
            }
            return;
        }

        if (action.kind == Action::Kind::LaunchDetached) {
            if (LaunchDetachedProcess(action.target, effectiveArgs)) {
                AppendLog("Launched: " + action.target.string());
            } else {
                AppendLog("Could not launch target.");
            }
            return;
        }

        if (!TargetReady(action)) {
            AppendLog("Target missing: " + action.target.string());
            return;
        }

        int exitCode = 0;
        const std::vector<std::string> lines = ReadProcessOutput(action.target, effectiveArgs, exitCode);
        if (lines.empty()) {
            AppendLog("No output.");
        } else {
            for (const std::string& line : lines) {
                AppendLog(line);
                RequestRedraw(windowHandle);
            }
        }
        AppendLog("Exit code: " + std::to_string(exitCode));
    }

    fs::path buildRoot_;
    fs::path sourceRoot_;
    std::vector<Action> actions_;
    ri::scene::SceneKitMilestoneOptions sceneKitOptions_{};
    std::vector<ri::scene::SceneKitMilestoneResult> sceneKitExamples_;
    std::size_t selectedIndex_ = 0;
    std::size_t selectedExampleIndex_ = 0;
    std::atomic<bool> busy_ = false;
    mutable std::mutex logMutex_;
    std::deque<std::string> logs_;
    std::thread worker_;
    std::vector<ri::shell::GameProject> gameProjects_;
    std::vector<std::string> recentSessions_;
};

void PrintHeadlessSummary(const ShellState& shell);

