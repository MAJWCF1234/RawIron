#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace ri::world::headless {

// Generic headless verification for any subsystem that exposes a readable snapshot after each
// deterministic step. There is no rendering, audio, or platform I/O — only state transitions
// you can observe and compare to a authored "success chart".
//
// How to use:
// 1) Define Snapshot — any type that fully captures what you need to assert (structs, codes,
//    flags, scalars). Prefer the same typed fields your production API already returns.
// 2) Provide readSnapshot() — must reflect committed state after apply() (e.g. copy from agent,
//    last result bundle, or aggregate of several getters).
// 3) Build a chart: each row is apply (mutation / tick / input) + match(snapshot, failure).
//    Rows run in order; the first failing row stops the run.
// 4) Pair with numeric outcome bands (HeadlessVerification.h) when you want stable serials for
//    telemetry or diff tools alongside typed assertions.
//
template<typename Snapshot>
struct HeadlessSuccessChartRow {
    std::function<void()> apply{};
    std::function<bool(const Snapshot& snapshot, std::string& failureMessage)> match{};
};

template<typename Snapshot>
[[nodiscard]] bool RunHeadlessSuccessChart(const std::function<Snapshot()>& readSnapshot,
                                           std::span<const HeadlessSuccessChartRow<Snapshot>> chart,
                                           std::string& failureMessage) {
    for (std::size_t rowIndex = 0; rowIndex < chart.size(); ++rowIndex) {
        const HeadlessSuccessChartRow<Snapshot>& row = chart[rowIndex];
        if (row.apply) {
            row.apply();
        }
        if (!row.match) {
            failureMessage = "row " + std::to_string(rowIndex) + ": missing match predicate";
            return false;
        }
        const Snapshot snapshot = readSnapshot();
        if (!row.match(snapshot, failureMessage)) {
            failureMessage = "headless chart row " + std::to_string(rowIndex) + ": " + failureMessage;
            return false;
        }
    }
    return true;
}

template<typename Snapshot>
[[nodiscard]] bool RunHeadlessSuccessChart(const std::function<Snapshot()>& readSnapshot,
                                           const std::vector<HeadlessSuccessChartRow<Snapshot>>& chart,
                                           std::string& failureMessage) {
    return RunHeadlessSuccessChart(readSnapshot, std::span<const HeadlessSuccessChartRow<Snapshot>>(chart.data(), chart.size()), failureMessage);
}

} // namespace ri::world::headless
