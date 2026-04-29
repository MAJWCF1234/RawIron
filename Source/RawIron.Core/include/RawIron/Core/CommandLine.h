#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::core {

class CommandLine {
public:
    CommandLine(int argc, char** argv);

    [[nodiscard]] bool HasFlag(std::string_view flag) const;
    [[nodiscard]] std::optional<std::string> GetValue(std::string_view option) const;
    [[nodiscard]] std::optional<int> TryGetInt(std::string_view option) const;
    [[nodiscard]] int GetIntOr(std::string_view option, int fallback) const;
    [[nodiscard]] const std::vector<std::string>& Args() const noexcept;

private:
    std::vector<std::string> args_;
};

} // namespace ri::core
