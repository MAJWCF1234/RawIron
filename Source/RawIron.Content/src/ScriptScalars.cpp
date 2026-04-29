#include "RawIron/Content/ScriptScalars.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
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

} // namespace

ScriptScalarMap LoadScriptScalars(const std::filesystem::path& path) {
    ScriptScalarMap values;
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return values;
    }

    std::string line;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, equals));
        const std::string valueText = Trim(line.substr(equals + 1U));
        if (key.empty() || valueText.empty()) {
            continue;
        }

        try {
            const float value = std::stof(valueText);
            if (std::isfinite(value)) {
                values[key] = value;
            }
        } catch (...) {
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

} // namespace ri::content
