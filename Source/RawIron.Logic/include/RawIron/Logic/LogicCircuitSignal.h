#pragma once

#include "RawIron/Logic/LogicTypes.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace ri::logic {

/// Mental model helpers for spatial, block-style logic (toy circuit boards, sandbox wiring)—the graph does not cap your ranges:
/// - **Trace** → `logic_relay` forwards pulses and optional \ref LogicContext::analogSignal.
/// - **Repeater / clock** → `logic_timer`, `logic_pulse`.
/// - **Memory** → `logic_latch`.
/// - **Measure** → `logic_compare`.
/// - **Accumulator** → `logic_counter`.
/// - **AND plane** → `logic_merge` with `MergeMode::All`.
/// - **Fan-out** → `logic_split`.
/// - **Bus** → `logic_channel`.

struct LogicCircuitNodeProbe {
    std::string id;
    std::string kind;
    bool powered = false;
    /// Debug / HUD hint (same units as \ref LogicContext::analogSignal where applicable; often 0 vs 1 for binary nodes).
    double signalStrength = 0.0;
    std::string detail;
};

[[nodiscard]] inline std::optional<double> TryGetAnalogSignal(const LogicContext& ctx) {
    if (!ctx.analogSignal.has_value()) {
        return std::nullopt;
    }
    return ctx.analogSignal;
}

inline void SetAnalogSignal(LogicContext& ctx, double value) {
    ctx.analogSignal = value;
}

/// Subtract `delta` from the current analog signal (floors at 0). No-op if unset (treats missing as 0).
inline void ApplyAnalogAttenuation(LogicContext& ctx, double delta) {
    if (!std::isfinite(delta) || delta <= 0.0) {
        return;
    }
    const double base = ctx.analogSignal.value_or(0.0);
    if (!std::isfinite(base)) {
        ctx.analogSignal = 0.0;
        return;
    }
    const double next = std::max(0.0, base - delta);
    ctx.analogSignal = next;
}

/// Optional: map `value` into `steps` discrete display buckets (e.g. multi-segment LED bar). Purely presentational.
[[nodiscard]] inline int QuantizeAnalogToSteps(double value, int steps, double fullScale) {
    if (steps < 1 || !std::isfinite(value) || !std::isfinite(fullScale) || fullScale <= 0.0) {
        return 0;
    }
    if (value <= 0.0) {
        return 0;
    }
    if (value >= fullScale) {
        return steps;
    }
    return static_cast<int>((value / fullScale) * static_cast<double>(steps));
}

} // namespace ri::logic
