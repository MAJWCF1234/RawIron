#pragma once

#include "RawIron/Runtime/RuntimeTuning.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::world {

struct DeveloperConsoleLine {
    std::string text;
    bool isError = false;
};

class DeveloperConsoleState {
public:
    using CommandHandler = std::function<bool(std::string_view args, std::string& output, bool& isError)>;

    explicit DeveloperConsoleState(std::size_t maxScrollback = 256U, std::size_t maxHistory = 64U);

    void SetOpen(bool open) noexcept;
    void ToggleOpen() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept;

    void SubmitCommand(std::string line);
    void RegisterCommand(std::string prefix, CommandHandler handler);
    void RegisterScript(std::string name, std::string contents);
    [[nodiscard]] std::vector<std::string> Autocomplete(std::string_view prefix) const;
    [[nodiscard]] std::optional<std::string> HistoryUp();
    [[nodiscard]] std::optional<std::string> HistoryDown();

    [[nodiscard]] const std::vector<DeveloperConsoleLine>& Scrollback() const noexcept;
    [[nodiscard]] const std::vector<std::string>& CommandHistory() const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, double>& TuningValues() const noexcept;
    [[nodiscard]] std::string ExportTuningState() const;
    [[nodiscard]] bool ImportTuningState(std::string_view serialized, std::string* error = nullptr);

private:
    struct ConVar {
        std::string name;
        double value = 0.0;
        double defaultValue = 0.0;
        double minValue = 0.0;
        double maxValue = 0.0;
        bool archive = false;
        bool cheat = false;
        bool readOnly = false;
        std::string help;
    };

    bool TryRunBuiltIn(std::string_view command, std::string_view args, std::string& output, bool& isError);
    bool TryHandleConVarSet(std::string_view name, std::string_view rawValue, std::string& output, bool& isError);
    void ExecuteCommandLine(std::string_view line);
    void PushLine(std::string text, bool isError = false);
    void PushHistory(std::string command);
    [[nodiscard]] std::string BuildTuningQueryString() const;

    bool isOpen_ = false;
    std::size_t maxScrollback_ = 256U;
    std::size_t maxHistory_ = 64U;
    std::vector<DeveloperConsoleLine> scrollback_;
    std::vector<std::string> history_;
    std::unordered_map<std::string, CommandHandler> commands_;
    std::unordered_map<std::string, std::string> commandHelp_;
    std::unordered_map<std::string, ConVar> convars_;
    std::unordered_map<std::string, std::string> aliases_;
    std::unordered_map<std::string, std::string> scripts_;
    std::unordered_map<std::string, std::string> tuningPresets_;
    std::unordered_map<std::string, double> tuningValues_;
    std::unordered_map<std::string, double> defaultTuningValues_;
    std::optional<std::size_t> historyCursor_{};
};

} // namespace ri::world
