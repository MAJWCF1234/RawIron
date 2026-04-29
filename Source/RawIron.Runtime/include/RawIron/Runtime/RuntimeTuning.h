#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <unordered_map>
#include <vector>

namespace ri::runtime {

// Numeric bounds for mechanics/debug tuning surfaced in the legacy web shell. The prototype kept
// this table in `index.js`; RawIron centralizes clamp + defaults for tools, automation, and future
// native UI.

struct RuntimeTuningLimits {
    double min = 0.0;
    double max = 0.0;
    double defaultValue = 0.0;
};

[[nodiscard]] const RuntimeTuningLimits* FindRuntimeTuningLimits(std::string_view key) noexcept;
[[nodiscard]] std::span<const std::string_view> RuntimeTuningKeys() noexcept;
[[nodiscard]] std::unordered_map<std::string, double> BuildDefaultRuntimeTuningRecord();
[[nodiscard]] std::unordered_map<std::string, double> BuildRuntimeTuningSnapshot(
    const std::unordered_map<std::string, double>& overrides) noexcept;
[[nodiscard]] std::string FormatRuntimeTuningReport(const std::unordered_map<std::string, double>& values);

/// Unknown keys yield std::nullopt. Missing or non-finite input yields the authored default.
[[nodiscard]] std::optional<double> SanitizeRuntimeTuningValue(std::string_view key,
                                                               std::optional<double> raw) noexcept;

/// Clamps every *known* key in-place (unknown keys are left unchanged). Non-finite numbers on
/// known keys become that key's default. Use when ingesting a full or partial tuning record
/// (e.g. JSON blob) on the engine side.
void SanitizeRuntimeTuningRecord(std::unordered_map<std::string, double>& values) noexcept;

} // namespace ri::runtime
