#pragma once

#include "RawIron/Trace/LocomotionTuning.h"

#include <string>
#include <unordered_map>

namespace ri::trace {

/// Builds locomotion scalars from a sanitized runtime tuning record (defaults → authoring overrides).
/// Unknown keys are ignored; missing keys keep `DefaultLocomotionTuning()` entries for locomotion fields.
[[nodiscard]] LocomotionTuning LocomotionTuningFromRuntimeRecord(
    const std::unordered_map<std::string, double>& record) noexcept;

/// Symmetric export of locomotion scalars for snapshots, replays, HUDs, or merging back into
/// `ri::runtime::BuildRuntimeTuningSnapshot` payloads (subset keys only).
[[nodiscard]] std::unordered_map<std::string, double> LocomotionTuningToRuntimeRecordSubset(
    const LocomotionTuning& tuning) noexcept;

} // namespace ri::trace
