#include "RawIron/Core/CommandLine.h"

#include <charconv>

namespace ri::core {
namespace {

std::optional<int> ParseInt(std::string_view text) {
    int parsed = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return parsed;
}

} // namespace

CommandLine::CommandLine(int argc, char** argv) {
    if (argc < 0) {
        argc = 0;
    }
    if (argc > 0 && argv == nullptr) {
        argc = 0;
    }
    args_.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        if (argv[index] == nullptr) {
            continue;
        }
        args_.emplace_back(argv[index]);
    }
}

bool CommandLine::HasFlag(std::string_view flag) const {
    for (const std::string& arg : args_) {
        if (arg == flag) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> CommandLine::GetValue(std::string_view option) const {
    if (option.empty()) {
        return std::nullopt;
    }
    const std::string prefix = std::string(option) + "=";
    for (std::size_t index = 0; index < args_.size(); ++index) {
        const std::string& arg = args_[index];
        if (arg == option) {
            if ((index + 1U) < args_.size()) {
                return args_[index + 1U];
            }
            return std::nullopt;
        }

        if (arg.rfind(prefix, 0) == 0) {
            return arg.substr(prefix.size());
        }
    }
    return std::nullopt;
}

std::optional<int> CommandLine::TryGetInt(std::string_view option) const {
    const std::optional<std::string> value = GetValue(option);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return ParseInt(*value);
}

int CommandLine::GetIntOr(std::string_view option, int fallback) const {
    const std::optional<int> value = TryGetInt(option);
    if (!value.has_value()) {
        return fallback;
    }
    return *value;
}

const std::vector<std::string>& CommandLine::Args() const noexcept {
    return args_;
}

} // namespace ri::core
