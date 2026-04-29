#pragma once

#include "RawIron/Trace/ObjectPhysics.h"

#include <string>
#include <vector>

namespace ri::trace {

/// Like \ref KinematicObjectSlot but carries an optional stable gameplay id for routing logic or telemetry.
struct KinematicEntitySlot : KinematicObjectSlot {
    std::string entityId{};
};

inline void WakeKinematicEntity(KinematicEntitySlot& slot) noexcept {
    WakeKinematicObject(slot);
}

/// Same integration path as \ref StepKinematicObjectBatch; copies results back into entity slots (preserves `entityId`).
[[nodiscard]] ObjectPhysicsBatchResult StepKinematicEntityBatch(
    const TraceScene& scene,
    std::vector<KinematicEntitySlot>& entities,
    float deltaSeconds,
    const KinematicPhysicsOptions& options,
    const KinematicVolumeModifiers& modifiers = {},
    const KinematicConstraintState& constraints = {},
    const ObjectPhysicsBatchOptions& batchOptions = {});

} // namespace ri::trace
