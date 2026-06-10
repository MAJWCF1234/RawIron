#include "RawIron/Content/ScriptScalars.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>

namespace ri::content {
namespace {

std::string Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

[[nodiscard]] std::optional<std::pair<std::string, float>> TryParseScalarLine(const std::string& line) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return std::nullopt;
    }
    const std::size_t equals = trimmed.find('=');
    if (equals == std::string::npos) {
        return std::nullopt;
    }
    const std::string key = Trim(trimmed.substr(0, equals));
    const std::string valueText = Trim(trimmed.substr(equals + 1U));
    if (key.empty() || valueText.empty()) {
        return std::nullopt;
    }
    try {
        std::size_t consumed = 0;
        const float value = std::stof(valueText, &consumed);
        if (consumed == valueText.size() && std::isfinite(value)) {
            return std::pair<std::string, float>{key, value};
        }
    } catch (...) {
    }
    return std::nullopt;
}

[[nodiscard]] std::string FormatScalarValue(const float value) {
    std::ostringstream stream{};
    stream.setf(std::ios::fmtflags(0), std::ios::floatfield);
    stream.precision(6);
    stream << value;
    std::string text = stream.str();
    const std::size_t dot = text.find('.');
    if (dot != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    return text.empty() ? "0" : text;
}

} // namespace

ScriptScalarMap LoadScriptScalarsFromText(const std::string_view text) {
    ScriptScalarMap values;
    std::istringstream stream{std::string(text)};
    std::string line;
    while (std::getline(stream, line)) {
        if (const std::optional<std::pair<std::string, float>> parsed = TryParseScalarLine(line); parsed.has_value()) {
            values[parsed->first] = parsed->second;
        }
    }
    return values;
}

void MergeScriptScalarMaps(ScriptScalarMap& destination, const ScriptScalarMap& overrides) {
    for (const auto& [key, value] : overrides) {
        destination[key] = value;
    }
}

bool PatchScriptScalarsFile(const std::filesystem::path& path,
                            const ScriptScalarMap& patches,
                            std::string* errorOut) {
    if (patches.empty()) {
        return true;
    }

    std::vector<std::string> lines;
    {
        std::ifstream input(path);
        if (input.is_open()) {
            std::string line;
            while (std::getline(input, line)) {
                lines.push_back(line);
            }
        }
    }

    std::set<std::string, std::less<>> remainingKeys{};
    for (const auto& [key, _] : patches) {
        remainingKeys.insert(key);
    }

    for (std::string& line : lines) {
        if (const std::optional<std::pair<std::string, float>> parsed = TryParseScalarLine(line); parsed.has_value()) {
            const auto patchIt = patches.find(parsed->first);
            if (patchIt != patches.end()) {
                line = parsed->first + "=" + FormatScalarValue(patchIt->second);
                remainingKeys.erase(parsed->first);
            }
        }
    }

    if (!remainingKeys.empty()) {
        if (!lines.empty() && !lines.back().empty()) {
            lines.emplace_back();
        }
        lines.emplace_back("# Patched by RawIron");
        for (const std::string& key : remainingKeys) {
            const auto patchIt = patches.find(key);
            if (patchIt != patches.end()) {
                lines.push_back(key + "=" + FormatScalarValue(patchIt->second));
            }
        }
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        output << lines[index];
        if (index + 1U < lines.size()) {
            output << '\n';
        }
    }
    const std::string body = output.str();
    const std::string finalBody = body.empty() ? std::string{} : body + "\n";

    std::ofstream writer(path, std::ios::binary | std::ios::trunc);
    if (!writer.is_open()) {
        if (errorOut != nullptr) {
            *errorOut = "Failed to open " + path.string() + " for writing.";
        }
        return false;
    }
    writer << finalBody;
    return writer.good();
}

ScriptScalarMap LoadScriptScalars(const std::filesystem::path& path) {
    ScriptScalarMap values;
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return values;
    }

    std::string line;
    while (std::getline(stream, line)) {
        if (const std::optional<std::pair<std::string, float>> parsed = TryParseScalarLine(line); parsed.has_value()) {
            values[parsed->first] = parsed->second;
        }
    }

    return values;
}

float ScriptScalarOr(const ScriptScalarMap& values, std::string_view key, float fallback) {
    const auto it = values.find(std::string(key));
    return it == values.end() ? fallback : it->second;
}

float ScriptScalarOrClamped(const ScriptScalarMap& values,
                            std::string_view key,
                            float fallback,
                            float minValue,
                            float maxValue) {
    return std::clamp(ScriptScalarOr(values, key, fallback), minValue, maxValue);
}

int ScriptScalarOrInt(const ScriptScalarMap& values, std::string_view key, int fallback) {
    const float asFloat = ScriptScalarOr(values, key, static_cast<float>(fallback));
    if (!std::isfinite(asFloat)) {
        return fallback;
    }
    const float rounded = std::round(asFloat);
    if (rounded < static_cast<float>(std::numeric_limits<int>::min())
        || rounded > static_cast<float>(std::numeric_limits<int>::max())) {
        return fallback;
    }
    return static_cast<int>(rounded);
}

int ScriptScalarOrIntClamped(const ScriptScalarMap& values,
                             std::string_view key,
                             int fallback,
                             int minValue,
                             int maxValue) {
    return std::clamp(ScriptScalarOrInt(values, key, fallback), minValue, maxValue);
}

bool ScriptScalarOrBool(const ScriptScalarMap& values, std::string_view key, bool fallback) {
    const float value = ScriptScalarOr(values, key, fallback ? 1.0f : 0.0f);
    if (!std::isfinite(value)) {
        return fallback;
    }
    return value >= 0.5f;
}

bool ScriptScalarValidationReport::ok() const {
    for (const ScriptScalarValidationIssue& issue : issues) {
        if (issue.severity == ScriptScalarValidationSeverity::Error) {
            return false;
        }
    }
    return true;
}

ScriptScalarValidationReport ValidateScriptScalars(
    const ScriptScalarMap& values,
    const ScriptScalarSchema& schema) {
    ScriptScalarValidationReport report{};
    std::set<std::string, std::less<>> knownKeys{};
    for (const ScriptScalarRule& rule : schema.rules) {
        knownKeys.insert(rule.key);
        const auto it = values.find(rule.key);
        if (it == values.end()) {
            if (rule.required) {
                report.issues.push_back(ScriptScalarValidationIssue{
                    .severity = ScriptScalarValidationSeverity::Error,
                    .key = rule.key,
                    .message = "missing required key",
                });
            }
            continue;
        }
        const float value = it->second;
        if (rule.minValue.has_value() && value < *rule.minValue) {
            report.issues.push_back(ScriptScalarValidationIssue{
                .severity = ScriptScalarValidationSeverity::Error,
                .key = rule.key,
                .message = "value below minimum",
            });
        }
        if (rule.maxValue.has_value() && value > *rule.maxValue) {
            report.issues.push_back(ScriptScalarValidationIssue{
                .severity = ScriptScalarValidationSeverity::Error,
                .key = rule.key,
                .message = "value above maximum",
            });
        }
    }
    if (!schema.allowUnknownKeys) {
        for (const auto& [key, _] : values) {
            if (!knownKeys.contains(key)) {
                report.issues.push_back(ScriptScalarValidationIssue{
                    .severity = ScriptScalarValidationSeverity::Error,
                    .key = key,
                    .message = "unknown key",
                });
            }
        }
    }
    return report;
}

} // namespace ri::content
