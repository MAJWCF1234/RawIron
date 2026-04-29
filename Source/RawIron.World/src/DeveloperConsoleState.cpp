#include "RawIron/World/DeveloperConsoleState.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <sstream>
#include <utility>
#include <vector>

namespace ri::world {
namespace {

std::string Trim(std::string text) {
    auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    const auto begin = std::find_if(text.begin(), text.end(), notSpace);
    const auto end = std::find_if(text.rbegin(), text.rend(), notSpace).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

std::pair<std::string, std::string> SplitFirstToken(std::string_view input) {
    const std::size_t firstSpace = input.find_first_of(" \t");
    if (firstSpace == std::string_view::npos) {
        return {std::string(input), {}};
    }
    const std::size_t restStart = input.find_first_not_of(" \t", firstSpace);
    return {
        std::string(input.substr(0, firstSpace)),
        restStart == std::string_view::npos ? std::string{} : std::string(input.substr(restStart)),
    };
}

std::vector<std::string> SplitCommandChain(std::string_view input) {
    std::vector<std::string> commands;
    std::string current;
    bool inQuotes = false;
    for (char c : input) {
        if (c == '"') {
            inQuotes = !inQuotes;
            current.push_back(c);
            continue;
        }
        if (c == ';' && !inQuotes) {
            std::string trimmed = Trim(current);
            if (!trimmed.empty()) {
                commands.push_back(std::move(trimmed));
            }
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    std::string trimmed = Trim(current);
    if (!trimmed.empty()) {
        commands.push_back(std::move(trimmed));
    }
    return commands;
}

bool TryParseDouble(std::string_view value, double& out) {
    const std::string asString(value);
    const char* first = asString.data();
    const char* last = asString.data() + asString.size();
    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
}

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string JoinStrings(const std::vector<std::string>& parts, std::string_view separator) {
    if (parts.empty()) {
        return {};
    }
    std::string joined;
    std::size_t total = 0;
    for (const std::string& part : parts) {
        total += part.size();
    }
    total += separator.size() * (parts.size() - 1U);
    joined.reserve(total);
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index > 0) {
            joined.append(separator);
        }
        joined.append(parts[index]);
    }
    return joined;
}

bool StartsWithCaseInsensitive(std::string_view value, std::string_view prefix) {
    if (prefix.size() > value.size()) {
        return false;
    }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i]))
            != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

std::vector<std::string_view> Split(std::string_view text, char delimiter) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t next = text.find(delimiter, start);
        if (next == std::string_view::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, next - start));
        start = next + 1U;
    }
    return parts;
}

} // namespace

DeveloperConsoleState::DeveloperConsoleState(std::size_t maxScrollback, std::size_t maxHistory)
    : maxScrollback_(std::max<std::size_t>(1U, maxScrollback))
    , maxHistory_(std::max<std::size_t>(1U, maxHistory))
    , tuningValues_(ri::runtime::BuildDefaultRuntimeTuningRecord())
    , defaultTuningValues_(tuningValues_) {
    commandHelp_["help"] = "help [command] - list commands or show command help";
    commandHelp_["clear"] = "clear - clear console scrollback";
    commandHelp_["echo"] = "echo <text> - print text";
    commandHelp_["history"] = "history - print recent commands";
    commandHelp_["find"] = "find <token> - search commands and cvars";
    commandHelp_["cvarlist"] = "cvarlist - list tuning cvars";
    commandHelp_["con_enable"] = "con_enable <0|1> - disable/enable console";
    commandHelp_["developer"] = "developer <0|1> - alias for con_enable";
    commandHelp_["tune"] = "tune list|get|set|reset|query|save|load|apply|presets";
    commandHelp_["set"] = "set <cvar> <value> - set cvar value";
    commandHelp_["get"] = "get <cvar> - print cvar value";
    commandHelp_["toggle"] = "toggle <cvar> - toggle 0/1 cvar";
    commandHelp_["incrementvar"] = "incrementvar <cvar> <min> <max> <delta>";
    commandHelp_["alias"] = "alias <name> <command chain>";
    commandHelp_["unalias"] = "unalias <name> - remove alias";
    commandHelp_["exec"] = "exec <script_name>";
    commandHelp_["scriptlist"] = "scriptlist - list registered scripts";
    commandHelp_["cmdlist"] = "cmdlist - list built-in and custom commands";
    commandHelp_["status"] = "status - print console runtime status";

    for (const std::string_view key : ri::runtime::RuntimeTuningKeys()) {
        const ri::runtime::RuntimeTuningLimits* limits = ri::runtime::FindRuntimeTuningLimits(key);
        if (limits == nullptr) {
            continue;
        }
        const auto found = tuningValues_.find(std::string(key));
        if (found == tuningValues_.end()) {
            continue;
        }
        ConVar cvar;
        cvar.name = std::string(key);
        cvar.value = found->second;
        cvar.defaultValue = limits->defaultValue;
        cvar.minValue = limits->min;
        cvar.maxValue = limits->max;
        cvar.archive = true;
        cvar.cheat = false;
        cvar.readOnly = false;
        cvar.help = "runtime tuning cvar";
        convars_.emplace(cvar.name, std::move(cvar));
    }
}

void DeveloperConsoleState::SetOpen(const bool open) noexcept {
    isOpen_ = open;
}

void DeveloperConsoleState::ToggleOpen() noexcept {
    isOpen_ = !isOpen_;
}

bool DeveloperConsoleState::IsOpen() const noexcept {
    return isOpen_;
}

void DeveloperConsoleState::SubmitCommand(std::string line) {
    const std::string trimmedLine = Trim(std::move(line));
    if (trimmedLine.empty()) {
        return;
    }
    PushHistory(trimmedLine);
    historyCursor_.reset();
    PushLine("] " + trimmedLine, false);
    for (const std::string& commandLine : SplitCommandChain(trimmedLine)) {
        ExecuteCommandLine(commandLine);
    }
}

void DeveloperConsoleState::RegisterCommand(std::string prefix, CommandHandler handler) {
    if (!prefix.empty() && handler) {
        const std::string key = prefix;
        commands_[key] = std::move(handler);
        commandHelp_.emplace(key, "custom command");
    }
}

void DeveloperConsoleState::RegisterScript(std::string name, std::string contents) {
    if (!name.empty() && !contents.empty()) {
        scripts_[std::move(name)] = std::move(contents);
    }
}

std::vector<std::string> DeveloperConsoleState::Autocomplete(std::string_view prefix) const {
    std::vector<std::string> suggestions;
    std::unordered_map<std::string, bool> seen;
    const std::string prefixLower = ToLower(std::string(prefix));
    auto maybeAdd = [&](std::string candidate) {
        if (!StartsWithCaseInsensitive(candidate, prefixLower)) {
            return;
        }
        if (seen.emplace(candidate, true).second) {
            suggestions.push_back(std::move(candidate));
        }
    };
    for (const auto& [command, _] : commandHelp_) {
        maybeAdd(command);
    }
    for (const auto& [alias, _] : aliases_) {
        maybeAdd(alias);
    }
    for (const auto& [script, _] : scripts_) {
        maybeAdd(script);
    }
    for (const auto& [command, _] : commands_) {
        maybeAdd(command);
    }
    for (const auto& [convar, _] : convars_) {
        maybeAdd(convar);
    }
    for (const std::string_view key : ri::runtime::RuntimeTuningKeys()) {
        maybeAdd(std::string(key));
    }
    std::sort(suggestions.begin(), suggestions.end());
    return suggestions;
}

std::optional<std::string> DeveloperConsoleState::HistoryUp() {
    if (history_.empty()) {
        return std::nullopt;
    }
    if (!historyCursor_.has_value()) {
        historyCursor_ = history_.size() - 1U;
    } else if (*historyCursor_ > 0U) {
        *historyCursor_ -= 1U;
    }
    return history_.at(*historyCursor_);
}

std::optional<std::string> DeveloperConsoleState::HistoryDown() {
    if (!historyCursor_.has_value() || history_.empty()) {
        return std::nullopt;
    }
    if (*historyCursor_ + 1U >= history_.size()) {
        historyCursor_.reset();
        return std::string{};
    }
    *historyCursor_ += 1U;
    return history_.at(*historyCursor_);
}

const std::vector<DeveloperConsoleLine>& DeveloperConsoleState::Scrollback() const noexcept {
    return scrollback_;
}

const std::vector<std::string>& DeveloperConsoleState::CommandHistory() const noexcept {
    return history_;
}

const std::unordered_map<std::string, double>& DeveloperConsoleState::TuningValues() const noexcept {
    return tuningValues_;
}

bool DeveloperConsoleState::TryRunBuiltIn(std::string_view command,
                                          std::string_view args,
                                          std::string& output,
                                          bool& isError) {
    if (command == "set") {
        const auto [name, rawValue] = SplitFirstToken(args);
        if (name.empty() || rawValue.empty()) {
            output = "Usage: set <cvar> <value>";
            isError = true;
            return true;
        }
        return TryHandleConVarSet(name, rawValue, output, isError);
    }
    if (command == "get") {
        const std::string name = Trim(std::string(args));
        const auto found = convars_.find(name);
        if (found == convars_.end()) {
            output = "Unknown cvar: " + name;
            isError = true;
            return true;
        }
        output = name + "=" + std::to_string(found->second.value);
        return true;
    }
    if (command == "toggle") {
        const std::string name = Trim(std::string(args));
        const auto found = convars_.find(name);
        if (found == convars_.end()) {
            output = "Unknown cvar: " + name;
            isError = true;
            return true;
        }
        const double epsilon = 1.0e-6;
        const bool atMin = std::abs(found->second.value - found->second.minValue) <= epsilon;
        const double newValue = atMin ? found->second.maxValue : found->second.minValue;
        return TryHandleConVarSet(name, std::to_string(newValue), output, isError);
    }
    if (command == "incrementvar") {
        const auto [name, rest] = SplitFirstToken(args);
        const auto [minText, rest2] = SplitFirstToken(rest);
        const auto [maxText, deltaText] = SplitFirstToken(rest2);
        double minValue = 0.0;
        double maxValue = 0.0;
        double delta = 0.0;
        if (name.empty() || minText.empty() || maxText.empty() || deltaText.empty()
            || !TryParseDouble(minText, minValue)
            || !TryParseDouble(maxText, maxValue)
            || !TryParseDouble(deltaText, delta)) {
            output = "Usage: incrementvar <cvar> <min> <max> <delta>";
            isError = true;
            return true;
        }
        const auto found = convars_.find(name);
        if (found == convars_.end()) {
            output = "Unknown cvar: " + name;
            isError = true;
            return true;
        }
        const double next = std::clamp(found->second.value + delta, std::min(minValue, maxValue), std::max(minValue, maxValue));
        return TryHandleConVarSet(name, std::to_string(next), output, isError);
    }
    if (command == "alias") {
        const std::string trimmedArgs = Trim(std::string(args));
        if (trimmedArgs.empty()) {
            if (aliases_.empty()) {
                output = "No aliases registered.";
                return true;
            }
            std::vector<std::string> names;
            names.reserve(aliases_.size());
            for (const auto& [name, _] : aliases_) {
                names.push_back(name);
            }
            std::sort(names.begin(), names.end());
            std::ostringstream stream;
            stream << "Aliases:";
            for (const std::string& name : names) {
                const auto found = aliases_.find(name);
                stream << " " << name << "=\"" << found->second << "\"";
            }
            output = stream.str();
            return true;
        }

        const auto [name, value] = SplitFirstToken(trimmedArgs);
        if (name.empty() || value.empty()) {
            output = "Usage: alias <name> <command chain>";
            isError = true;
            return true;
        }
        aliases_[name] = value;
        output = "alias " + name + "=\"" + value + "\"";
        return true;
    }
    if (command == "unalias") {
        const std::string name = Trim(std::string(args));
        if (name.empty()) {
            output = "Usage: unalias <name>";
            isError = true;
            return true;
        }
        const std::size_t removed = aliases_.erase(name);
        if (removed == 0U) {
            output = "Unknown alias: " + name;
            isError = true;
            return true;
        }
        output = "Removed alias: " + name;
        return true;
    }
    if (command == "exec") {
        const std::string scriptName = Trim(std::string(args));
        const auto found = scripts_.find(scriptName);
        if (scriptName.empty() || found == scripts_.end()) {
            output = "Unknown script: " + scriptName;
            isError = true;
            return true;
        }
        PushLine("exec: " + scriptName, false);
        for (const std::string& line : SplitCommandChain(found->second)) {
            ExecuteCommandLine(line);
        }
        output = "exec complete: " + scriptName;
        return true;
    }
    if (command == "scriptlist") {
        if (scripts_.empty()) {
            output = "No scripts registered.";
            return true;
        }
        std::vector<std::string> names;
        names.reserve(scripts_.size());
        for (const auto& [name, _] : scripts_) {
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        output = "Scripts: " + JoinStrings(names, " ");
        return true;
    }
    if (command == "cmdlist") {
        std::vector<std::string> names;
        names.reserve(commandHelp_.size() + commands_.size());
        for (const auto& [name, _] : commandHelp_) {
            names.push_back(name);
        }
        for (const auto& [name, _] : commands_) {
            if (std::find(names.begin(), names.end(), name) == names.end()) {
                names.push_back(name);
            }
        }
        std::sort(names.begin(), names.end());
        output = "Commands: " + JoinStrings(names, " ");
        return true;
    }
    if (command == "status") {
        std::ostringstream stream;
        stream << "console:"
               << " open=" << (isOpen_ ? "1" : "0")
               << " lines=" << scrollback_.size()
               << " history=" << history_.size()
               << " aliases=" << aliases_.size()
               << " scripts=" << scripts_.size()
               << " cvars=" << convars_.size();
        output = stream.str();
        return true;
    }
    if (command == "help") {
        const std::string topic = Trim(std::string(args));
        if (!topic.empty()) {
            const auto found = commandHelp_.find(topic);
            if (found != commandHelp_.end()) {
                output = found->second;
                return true;
            }
            if (ri::runtime::FindRuntimeTuningLimits(topic) != nullptr) {
                const ri::runtime::RuntimeTuningLimits* limits = ri::runtime::FindRuntimeTuningLimits(topic);
                output = topic + " - cvar range [" + std::to_string(limits->min) + ", " + std::to_string(limits->max)
                    + "] default " + std::to_string(limits->defaultValue);
                return true;
            }
            output = "No help for: " + topic;
            isError = true;
            return true;
        }
        std::vector<std::string> names;
        names.reserve(commandHelp_.size());
        for (const auto& [name, _] : commandHelp_) {
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        std::ostringstream stream;
        stream << "Commands:";
        for (const std::string& name : names) {
            stream << " " << name;
        }
        output = stream.str();
        return true;
    }
    if (command == "history") {
        std::ostringstream stream;
        stream << "History:";
        for (std::size_t index = 0; index < history_.size(); ++index) {
            stream << " [" << index << "]" << history_[index];
        }
        output = stream.str();
        return true;
    }
    if (command == "clear") {
        scrollback_.clear();
        output = "Console cleared.";
        return true;
    }
    if (command == "echo") {
        output = std::string(args);
        return true;
    }
    if (command == "find") {
        const std::string token = ToLower(Trim(std::string(args)));
        if (token.empty()) {
            output = "Usage: find <token>";
            isError = true;
            return true;
        }
        std::vector<std::string> matches;
        for (const auto& [name, _] : commandHelp_) {
            if (ToLower(name).find(token) != std::string::npos) {
                matches.push_back(name);
            }
        }
        for (const std::string_view key : ri::runtime::RuntimeTuningKeys()) {
            const std::string keyString(key);
            if (ToLower(keyString).find(token) != std::string::npos) {
                matches.push_back(keyString);
            }
        }
        for (const auto& [name, _] : aliases_) {
            if (ToLower(name).find(token) != std::string::npos) {
                matches.push_back(name);
            }
        }
        for (const auto& [name, _] : scripts_) {
            if (ToLower(name).find(token) != std::string::npos) {
                matches.push_back(name);
            }
        }
        std::sort(matches.begin(), matches.end());
        matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
        std::ostringstream stream;
        stream << "find(" << token << "):";
        for (const std::string& entry : matches) {
            stream << " " << entry;
        }
        output = stream.str();
        return true;
    }
    if (command == "cvarlist") {
        std::ostringstream stream;
        stream << "cvars:";
        std::vector<std::string> names;
        names.reserve(convars_.size());
        for (const auto& [name, _] : convars_) {
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        for (const std::string& key : names) {
            const auto found = convars_.find(key);
            const std::string flags = std::string(found->second.archive ? "a" : "")
                + (found->second.cheat ? "c" : "")
                + (found->second.readOnly ? "r" : "");
            stream << " " << key << "=" << found->second.value;
            if (!flags.empty()) {
                stream << "[" << flags << "]";
            }
        }
        output = stream.str();
        return true;
    }
    if (command == "con_enable" || command == "developer") {
        const std::string value = Trim(std::string(args));
        if (value.empty()) {
            output = std::string(command) + "=" + (isOpen_ ? "1" : "0");
            return true;
        }
        if (value == "0") {
            isOpen_ = false;
            output = std::string(command) + "=0";
            return true;
        }
        if (value == "1") {
            isOpen_ = true;
            output = std::string(command) + "=1";
            return true;
        }
        output = "Usage: " + std::string(command) + " <0|1>";
        isError = true;
        return true;
    }
    if (command != "tune") {
        return false;
    }

    const auto [subcommand, rest] = SplitFirstToken(args);
    if (subcommand.empty() || subcommand == "list") {
        std::ostringstream stream;
        stream << "Tuning values:";
        for (const std::string_view key : ri::runtime::RuntimeTuningKeys()) {
            const auto found = tuningValues_.find(std::string(key));
            if (found != tuningValues_.end()) {
                stream << " " << key << "=" << found->second;
            }
        }
        output = stream.str();
        return true;
    }
    if (subcommand == "get") {
        const std::string key = Trim(rest);
        const auto found = convars_.find(key);
        if (found == convars_.end()) {
            output = "Unknown tuning key: " + key;
            isError = true;
            return true;
        }
        output = key + "=" + std::to_string(found->second.value);
        return true;
    }
    if (subcommand == "set") {
        const auto [key, rawValue] = SplitFirstToken(rest);
        if (key.empty() || rawValue.empty()) {
            output = "Usage: tune set <key> <value>";
            isError = true;
            return true;
        }
        return TryHandleConVarSet(key, rawValue, output, isError);
    }
    if (subcommand == "reset") {
        const std::string key = Trim(rest);
        if (key.empty() || key == "all") {
            tuningValues_ = defaultTuningValues_;
            output = "Tuning reset to defaults.";
            return true;
        }
        const auto found = defaultTuningValues_.find(key);
        if (found == defaultTuningValues_.end()) {
            output = "Unknown tuning key: " + key;
            isError = true;
            return true;
        }
        tuningValues_[key] = found->second;
        auto cvarFound = convars_.find(key);
        if (cvarFound != convars_.end()) {
            cvarFound->second.value = found->second;
        }
        output = key + "=" + std::to_string(found->second);
        return true;
    }
    if (subcommand == "query") {
        output = BuildTuningQueryString();
        return true;
    }
    if (subcommand == "save") {
        const std::string presetName = Trim(rest);
        if (presetName.empty()) {
            output = "Usage: tune save <preset_name>";
            isError = true;
            return true;
        }
        tuningPresets_[presetName] = ExportTuningState();
        output = "Saved tuning preset: " + presetName;
        return true;
    }
    if (subcommand == "load") {
        const std::string presetName = Trim(rest);
        if (presetName.empty()) {
            output = "Usage: tune load <preset_name>";
            isError = true;
            return true;
        }
        const auto found = tuningPresets_.find(presetName);
        if (found == tuningPresets_.end()) {
            output = "Unknown tuning preset: " + presetName;
            isError = true;
            return true;
        }
        std::string importError;
        if (!ImportTuningState(found->second, &importError)) {
            output = "Failed to load preset: " + importError;
            isError = true;
            return true;
        }
        output = "Loaded tuning preset: " + presetName;
        return true;
    }
    if (subcommand == "apply") {
        const std::string serialized = Trim(rest);
        if (serialized.empty()) {
            output = "Usage: tune apply <query_or_pairs>";
            isError = true;
            return true;
        }
        std::string importError;
        if (!ImportTuningState(serialized, &importError)) {
            output = "Failed to apply tuning: " + importError;
            isError = true;
            return true;
        }
        output = "Applied tuning state.";
        return true;
    }
    if (subcommand == "presets") {
        std::vector<std::string> names;
        names.reserve(tuningPresets_.size());
        for (const auto& [name, _] : tuningPresets_) {
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        std::ostringstream stream;
        stream << "Tuning presets:";
        for (const std::string& name : names) {
            stream << " " << name;
        }
        output = stream.str();
        return true;
    }

    output = "Unknown tune command: " + std::string(subcommand);
    isError = true;
    return true;
}

bool DeveloperConsoleState::TryHandleConVarSet(std::string_view name,
                                               std::string_view rawValue,
                                               std::string& output,
                                               bool& isError) {
    auto found = convars_.find(std::string(name));
    if (found == convars_.end()) {
        output = "Unknown cvar: " + std::string(name);
        isError = true;
        return true;
    }
    if (found->second.readOnly) {
        output = "Cvar is read-only: " + std::string(name);
        isError = true;
        return true;
    }
    double parsed = 0.0;
    if (!TryParseDouble(rawValue, parsed)) {
        output = "Invalid numeric value for cvar: " + std::string(name);
        isError = true;
        return true;
    }
    const double clamped = std::clamp(parsed, found->second.minValue, found->second.maxValue);
    found->second.value = clamped;
    tuningValues_[found->first] = clamped;
    output = found->first + "=" + std::to_string(clamped);
    return true;
}

void DeveloperConsoleState::ExecuteCommandLine(std::string_view line) {
    const std::string trimmed = Trim(std::string(line));
    if (trimmed.empty()) {
        return;
    }
    const auto [command, args] = SplitFirstToken(trimmed);
    const auto alias = aliases_.find(command);
    if (alias != aliases_.end()) {
        for (const std::string& expanded : SplitCommandChain(alias->second + (args.empty() ? "" : (" " + args)))) {
            ExecuteCommandLine(expanded);
        }
        return;
    }

    std::string output;
    bool isError = false;
    bool handled = TryRunBuiltIn(command, args, output, isError);
    if (!handled) {
        const auto it = commands_.find(command);
        if (it != commands_.end() && it->second) {
            handled = it->second(args, output, isError);
        }
    }
    if (!handled) {
        PushLine("Unknown command: " + command, true);
        return;
    }
    if (!output.empty()) {
        PushLine(output, isError);
    }
}

void DeveloperConsoleState::PushLine(std::string text, const bool isError) {
    scrollback_.push_back(DeveloperConsoleLine{
        .text = std::move(text),
        .isError = isError,
    });
    if (scrollback_.size() > maxScrollback_) {
        scrollback_.erase(scrollback_.begin(),
                          scrollback_.begin()
                              + static_cast<std::ptrdiff_t>(scrollback_.size() - maxScrollback_));
    }
}

void DeveloperConsoleState::PushHistory(std::string command) {
    if (!history_.empty() && history_.back() == command) {
        return;
    }
    history_.push_back(std::move(command));
    if (history_.size() > maxHistory_) {
        history_.erase(history_.begin(),
                       history_.begin() + static_cast<std::ptrdiff_t>(history_.size() - maxHistory_));
    }
}

std::string DeveloperConsoleState::BuildTuningQueryString() const {
    std::ostringstream query;
    bool first = true;
    for (const std::string_view key : ri::runtime::RuntimeTuningKeys()) {
        const auto current = tuningValues_.find(std::string(key));
        const auto defaults = defaultTuningValues_.find(std::string(key));
        if (current == tuningValues_.end() || defaults == defaultTuningValues_.end()) {
            continue;
        }
        if (std::abs(current->second - defaults->second) <= 1.0e-6) {
            continue;
        }
        query << (first ? "?" : "&") << key << "=" << current->second;
        first = false;
    }
    if (first) {
        return "?";
    }
    return query.str();
}

std::string DeveloperConsoleState::ExportTuningState() const {
    return BuildTuningQueryString();
}

bool DeveloperConsoleState::ImportTuningState(std::string_view serialized, std::string* error) {
    const std::string_view source = !serialized.empty() && serialized.front() == '?'
        ? serialized.substr(1)
        : serialized;
    if (source.empty()) {
        return true;
    }

    for (const std::string_view pair : Split(source, '&')) {
        if (pair.empty()) {
            continue;
        }
        const std::size_t equals = pair.find('=');
        if (equals == std::string_view::npos) {
            if (error != nullptr) {
                *error = "Missing '=' in pair: " + std::string(pair);
            }
            return false;
        }
        const std::string key = Trim(std::string(pair.substr(0, equals)));
        const std::string rawValue = Trim(std::string(pair.substr(equals + 1U)));
        if (key.empty() || rawValue.empty()) {
            if (error != nullptr) {
                *error = "Invalid key/value pair.";
            }
            return false;
        }

        std::string output;
        bool isError = false;
        if (!TryHandleConVarSet(key, rawValue, output, isError) || isError) {
            if (error != nullptr) {
                *error = output.empty() ? "Unknown import error." : output;
            }
            return false;
        }
    }
    return true;
}

} // namespace ri::world
