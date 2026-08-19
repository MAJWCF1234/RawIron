#include "RawIron/Content/PluginPackage.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <process.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

int Fail(const std::string& message) {
    std::cerr << "PluginPackageBuildSmoke: " << message << '\n';
    return EXIT_FAILURE;
}

std::string ReadText(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

#if defined(_WIN32)
std::wstring QuoteWindowsArgument(const fs::path& path) {
    std::wstring value = path.native();
    std::wstring result = L"\"";
    for (const wchar_t ch : value) {
        if (ch == L'"') {
            result += L"\\\"";
        } else {
            result.push_back(ch);
        }
    }
    result += L"\"";
    return result;
}
#endif

struct ProcessResult {
    bool launched = false;
    int exitCode = -1;
    std::string output;
};

ProcessResult RunTool(const fs::path& tool, const std::vector<fs::path>& arguments, const fs::path& log) {
#if defined(_WIN32)
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    const HANDLE logHandle = CreateFileW(
        log.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, &security,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logHandle == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::wstring command = QuoteWindowsArgument(tool);
    for (const fs::path& argument : arguments) {
        command += L' ';
        command += QuoteWindowsArgument(argument);
    }
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = logHandle;
    startup.hStdError = logHandle;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const BOOL launched = CreateProcessW(
        tool.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startup, &process);
    if (launched == FALSE) {
        CloseHandle(logHandle);
        return {};
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0U;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(logHandle);
    return {true, static_cast<int>(exitCode), ReadText(log)};
#else
    const pid_t child = fork();
    if (child < 0) {
        return {};
    }
    if (child == 0) {
        const int descriptor = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (descriptor < 0) {
            _exit(126);
        }
        dup2(descriptor, STDOUT_FILENO);
        dup2(descriptor, STDERR_FILENO);
        close(descriptor);
        std::vector<std::string> texts;
        texts.reserve(arguments.size() + 1U);
        texts.push_back(tool.string());
        for (const fs::path& argument : arguments) {
            texts.push_back(argument.string());
        }
        std::vector<char*> pointers;
        pointers.reserve(texts.size() + 1U);
        for (std::string& text : texts) {
            pointers.push_back(text.data());
        }
        pointers.push_back(nullptr);
        execv(tool.c_str(), pointers.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) {
        return {};
    }
    return {WEXITSTATUS(status) != 127, WEXITSTATUS(status), ReadText(log)};
#endif
}

} // namespace

int main(const int argc, const char* const* argv) {
    if (argc != 3) {
        return Fail("expected <ri_tool> <workspace-root>");
    }
    const fs::path toolPath(argv[1]);
    const fs::path workspace(argv[2]);
    if (!fs::is_regular_file(toolPath)) {
        return Fail("ri_tool executable does not exist");
    }

    const fs::path templatePackage = workspace / "Plugins" / "Templates" / "PublicPluginPackage";
    const ri::content::PluginPackageArchivePlan plan =
        ri::content::PlanPluginPackageArchive(templatePackage);
    if (!plan.valid || plan.descriptor.id != "example_public_plugin") {
        return Fail("template plugin package archive plan is invalid");
    }
    bool sawDescriptor = false;
    bool sawHooks = false;
    bool sawReadme = false;
    for (const std::string& entry : plan.relativeEntries) {
        sawDescriptor = sawDescriptor || entry == "package.riplugin.json";
        sawHooks = sawHooks || entry == "plugins/hooks.riplugin";
        sawReadme = sawReadme || entry == "README.md";
    }
    if (!sawDescriptor || !sawHooks || !sawReadme) {
        return Fail("template archive plan is missing required public entries");
    }

#if defined(_WIN32)
    const auto processId = static_cast<std::uint64_t>(_getpid());
#else
    const auto processId = static_cast<std::uint64_t>(getpid());
#endif
    const auto uniqueTick = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path fixtureRoot = fs::temp_directory_path()
        / ("RawIronPluginPackageBuildSmoke." + std::to_string(processId) + "."
           + std::to_string(uniqueTick));
    std::error_code error;
    if (!fs::create_directory(fixtureRoot, error) || error) {
        return Fail("could not create fixture root");
    }

    const fs::path stagingRoot = fixtureRoot / "staged";
    std::vector<std::string> stageIssues;
    if (!ri::content::StagePluginPackageArchive(plan, stagingRoot, stageIssues)) {
        fs::remove_all(fixtureRoot, error);
        return Fail("staging template package failed");
    }
    if (!fs::is_regular_file(stagingRoot / "package.riplugin.json")
        || !fs::is_regular_file(stagingRoot / "plugins" / "hooks.riplugin")) {
        fs::remove_all(fixtureRoot, error);
        return Fail("staged package is missing expected files");
    }

    const fs::path archivePath = fixtureRoot / "example_public_plugin.ripak";
    const fs::path buildLog = fixtureRoot / "build.log";
    const ProcessResult build = RunTool(
        toolPath,
        {
            fs::path("--plugin-package-build"),
            templatePackage,
            fs::path("--output"),
            archivePath,
        },
        buildLog);
    if (!build.launched || build.exitCode != 0
        || build.output.find("RawIron plugin package built") == std::string::npos
        || !fs::is_regular_file(archivePath)) {
        fs::remove_all(fixtureRoot, error);
        return Fail("plugin-package-build failed: " + build.output);
    }

    const fs::path validateLog = fixtureRoot / "validate.log";
    const ProcessResult validate = RunTool(
        toolPath,
        {fs::path("--plugin-package-validate"), archivePath},
        validateLog);
    if (!validate.launched || validate.exitCode != 0
        || validate.output.find("Plugin package validation: pass") == std::string::npos) {
        fs::remove_all(fixtureRoot, error);
        return Fail("built .ripak failed plugin-package-validate: " + validate.output);
    }

    fs::remove_all(fixtureRoot, error);
    return EXIT_SUCCESS;
}
