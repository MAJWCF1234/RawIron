#include "RawIron/Runtime/RuntimeTuning.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string_view>

namespace ri::runtime {
namespace {

struct Rule {
    std::string_view key;
    RuntimeTuningLimits limits;
};

// Mirrors proto `engine/runtimeTuningShim.js` / shell mechanics bounds (keep in sync).
// Stamina-related keys are optional: only games that set `MovementControllerOptions::simulateStamina`
// (and wire drain/regen into options) need to honor them.
constexpr Rule kRules[] = {
    {.key = "sensitivity", .limits = {.min = 0.2, .max = 3.0, .defaultValue = 1.0}},
    {.key = "staminaDrain", .limits = {.min = 1.0, .max = 60.0, .defaultValue = 25.0}},
    {.key = "staminaRegen", .limits = {.min = 1.0, .max = 60.0, .defaultValue = 15.0}},
    {.key = "walkSpeed", .limits = {.min = 0.5, .max = 12.0, .defaultValue = 5.0}},
    {.key = "sprintSpeed", .limits = {.min = 0.5, .max = 16.0, .defaultValue = 8.0}},
    {.key = "gravity", .limits = {.min = 1.0, .max = 60.0, .defaultValue = 32.0}},
    {.key = "jumpForce", .limits = {.min = 0.5, .max = 20.0, .defaultValue = 9.6}},
    {.key = "fallGravityMultiplier", .limits = {.min = 0.5, .max = 4.0, .defaultValue = 1.4}},
    {.key = "lowJumpGravityMultiplier", .limits = {.min = 0.5, .max = 4.0, .defaultValue = 1.18}},
    {.key = "maxFallSpeed", .limits = {.min = 1.0, .max = 50.0, .defaultValue = 28.0}},
};

constexpr std::string_view kKeys[] = {
    "sensitivity",
    "staminaDrain",
    "staminaRegen",
    "walkSpeed",
    "sprintSpeed",
    "gravity",
    "jumpForce",
    "fallGravityMultiplier",
    "lowJumpGravityMultiplier",
    "maxFallSpeed",
};

} // namespace

const RuntimeTuningLimits* FindRuntimeTuningLimits(std::string_view key) noexcept {
    for (const Rule& rule : kRules) {
        if (rule.key == key) {
            return &rule.limits;
        }
    }
    return nullptr;
}

std::span<const std::string_view> RuntimeTuningKeys() noexcept {
    return std::span<const std::string_view>(kKeys, std::size(kKeys));
}

std::unordered_map<std::string, double> BuildDefaultRuntimeTuningRecord() {
    std::unordered_map<std::string, double> defaults;
    defaults.reserve(std::size(kRules));
    for (const Rule& rule : kRules) {
        defaults.emplace(std::string(rule.key), rule.limits.defaultValue);
    }
    return defaults;
}

std::unordered_map<std::string, double> BuildRuntimeTuningSnapshot(
    const std::unordered_map<std::string, double>& overrides) noexcept {
    std::unordered_map<std::string, double> snapshot = BuildDefaultRuntimeTuningRecord();
    for (const auto& [key, value] : overrides) {
        const RuntimeTuningLimits* limits = FindRuntimeTuningLimits(key);
        if (limits == nullptr) {
            continue;
        }
        if (!std::isfinite(value)) {
            snapshot[key] = limits->defaultValue;
        } else {
            snapshot[key] = std::clamp(value, limits->min, limits->max);
        }
    }
    return snapshot;
}

std::string FormatRuntimeTuningReport(const std::unordered_map<std::string, double>& values) {
    std::ostringstream out;
    out << "runtime_tuning\n";
    for (std::string_view key : RuntimeTuningKeys()) {
        const RuntimeTuningLimits* limits = FindRuntimeTuningLimits(key);
        if (limits == nullptr) {
            continue;
        }
        const auto it = values.find(std::string(key));
        const double value = it == values.end() ? limits->defaultValue : it->second;
        out << key << "=" << value << " (min=" << limits->min << ", max=" << limits->max
            << ", default=" << limits->defaultValue << ")\n";
    }
    return out.str();
}

std::optional<double> SanitizeRuntimeTuningValue(std::string_view key, std::optional<double> raw) noexcept {
    const RuntimeTuningLimits* limits = FindRuntimeTuningLimits(key);
    if (limits == nullptr) {
        return std::nullopt;
    }

    if (!raw.has_value() || !std::isfinite(*raw)) {
        return limits->defaultValue;
    }

    return std::clamp(*raw, limits->min, limits->max);
}

void SanitizeRuntimeTuningRecord(std::unordered_map<std::string, double>& values) noexcept {
    for (auto& entry : values) {
        const RuntimeTuningLimits* limits = FindRuntimeTuningLimits(entry.first);
        if (limits == nullptr) {
            continue;
        }
        if (!std::isfinite(entry.second)) {
            entry.second = limits->defaultValue;
        } else {
            entry.second = std::clamp(entry.second, limits->min, limits->max);
        }
    }
}

} // namespace ri::runtime
