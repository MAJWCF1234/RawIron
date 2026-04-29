#pragma once

#include "RawIron/Runtime/RuntimeEventBus.h"

#include <string_view>

namespace ri::runtime::entity_io {

/// Runtime event channel used by \ref HelperActivityTracker and debug overlays (`HelperActivityState::lastEntityIoEvent`).
inline constexpr std::string_view kEventType = "entityIo";

/// Stable field keys consumed by \ref ri::world::HelperActivityTracker when summarizing activity strings.
inline constexpr std::string_view kFieldKind = "kind";
inline constexpr std::string_view kFieldSourceId = "sourceId";
inline constexpr std::string_view kFieldTargetId = "targetId";
inline constexpr std::string_view kFieldInputName = "inputName";
inline constexpr std::string_view kFieldOutputName = "outputName";
inline constexpr std::string_view kFieldTimerId = "timerId";
inline constexpr std::string_view kFieldInstigatorId = "instigatorId";
inline constexpr std::string_view kFieldParameter = "parameter";
inline constexpr std::string_view kFieldAnalogSignal = "analogSignal";

/// `kind` discriminant written by \ref ri::world::EmitEntityIoForLogicOutput / \ref ri::world::EmitEntityIoForLogicInput.
inline constexpr std::string_view kKindOutput = "output";
inline constexpr std::string_view kKindInput = "input";

void EmitEntityIo(RuntimeEventBus& bus, RuntimeEventFields fields);

} // namespace ri::runtime::entity_io
