#pragma once

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ri::test {

[[nodiscard]] inline const char* ReadEnvironment(const char* key) noexcept {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    const char* value = std::getenv(key);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    return value;
}

struct TestCase {
    const char* name = nullptr;
    void (*fn)() = nullptr;
};

struct HarnessConfig {
    bool list_only = false;
    bool help_only = false;
    bool verbose = false;
    bool timing = false;
    /// Empty means run every case; otherwise substring match on registered test names.
    std::string filter_substring{};
};

[[nodiscard]] inline HarnessConfig ParseTestHarnessArgv(int argc, char** argv) {
    HarnessConfig cfg{};
    for (int index = 1; index < argc; ++index) {
        std::string_view arg = argv[index];
        if (arg == "--help" || arg == "-h" || arg == "-?") {
            cfg.help_only = true;
            continue;
        }
        if (arg == "--list") {
            cfg.list_only = true;
            continue;
        }
        if (arg == "--verbose" || arg == "-v") {
            cfg.verbose = true;
            continue;
        }
        if (arg == "--timing" || arg == "-t") {
            cfg.timing = true;
            continue;
        }
        constexpr std::string_view filter_prefix = "--filter=";
        if (arg.size() >= filter_prefix.size() && arg.compare(0, filter_prefix.size(), filter_prefix) == 0) {
            cfg.filter_substring = std::string(arg.substr(filter_prefix.size()));
            continue;
        }
        throw std::runtime_error(
            std::string("Unknown argument: ")
            + std::string(arg)
            + " (try --help)");
    }

    if (cfg.filter_substring.empty()) {
        if (const char* env = ReadEnvironment("RAWIRON_TEST_FILTER"); env != nullptr && env[0] != '\0') {
            cfg.filter_substring = env;
        }
    }

    if (!cfg.verbose && ReadEnvironment("RAWIRON_TEST_VERBOSE") != nullptr) {
        cfg.verbose = true;
    }
    if (!cfg.timing && ReadEnvironment("RAWIRON_TEST_TIMING") != nullptr) {
        cfg.timing = true;
    }

    return cfg;
}

inline void PrintHarnessHelp(std::string_view executable_label) {
    std::cout << "Usage: " << executable_label << " [options]\n"
              << "Runs the registered RawIron native test suite.\n"
              << "\n"
              << "Options:\n"
              << "  --list               Print test names (honors filter) and exit 0.\n"
              << "  --filter=<substring> Run only tests whose registered name contains the substring.\n"
              << "  --verbose, -v        Print each test name before it runs.\n"
              << "  --timing, -t         Print elapsed milliseconds after each executed test.\n"
              << "  --help, -h           Show this help.\n"
              << "\n"
              << "Environment:\n"
              << "  RAWIRON_TEST_FILTER   Same effect as --filter=\n"
              << "  RAWIRON_TEST_VERBOSE  Same effect as --verbose\n"
              << "  RAWIRON_TEST_TIMING   Same effect as --timing\n"
              << "\n"
              << "Exit codes: 0 ok, 1 assertion/runtime failure, 2 filter matched no tests.\n";
}

[[nodiscard]] inline bool NameMatchesFilter(std::string_view test_name, std::string_view filter_substring) {
    if (filter_substring.empty()) {
        return true;
    }
    return test_name.find(filter_substring) != std::string_view::npos;
}

[[nodiscard]] inline int RunTestHarness(std::string_view executable_label,
                                        std::span<const TestCase> cases,
                                        int argc,
                                        char** argv) {
    try {
        const HarnessConfig cfg = ParseTestHarnessArgv(argc, argv);

        if (cfg.help_only) {
            PrintHarnessHelp(executable_label);
            return 0;
        }

        if (cfg.list_only) {
            std::size_t printed = 0;
            for (const TestCase& entry : cases) {
                if (!entry.name || !entry.fn) {
                    continue;
                }
                if (!NameMatchesFilter(entry.name, cfg.filter_substring)) {
                    continue;
                }
                std::cout << entry.name << '\n';
                ++printed;
            }
            if (!cfg.filter_substring.empty() && printed == 0U) {
                std::cerr << executable_label << ": --list: no tests matched filter.\n";
                return 2;
            }
            return 0;
        }

        std::size_t planned = 0;
        for (const TestCase& entry : cases) {
            if (!entry.name || !entry.fn) {
                continue;
            }
            if (NameMatchesFilter(entry.name, cfg.filter_substring)) {
                ++planned;
            }
        }

        if (planned == 0U && !cfg.filter_substring.empty()) {
            std::cerr << executable_label << ": no tests matched filter \"" << cfg.filter_substring << "\".\n";
            return 2;
        }

        using Clock = std::chrono::steady_clock;

        for (const TestCase& entry : cases) {
            if (!entry.name || !entry.fn) {
                continue;
            }
            if (!NameMatchesFilter(entry.name, cfg.filter_substring)) {
                continue;
            }

            if (cfg.verbose || cfg.timing) {
                std::cout << "[" << executable_label << "] running " << entry.name << '\n';
            }

            const Clock::time_point start = Clock::now();
            entry.fn();
            if (cfg.timing) {
                const auto ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
                std::cout << "[" << executable_label << "] " << entry.name << " finished in " << ms << " ms\n";
            }
        }

        std::cout << executable_label << " passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << executable_label << " failed: " << exception.what() << '\n';
        return 1;
    }
}

} // namespace ri::test
