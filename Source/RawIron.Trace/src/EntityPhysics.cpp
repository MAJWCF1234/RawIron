#include "RawIron/Trace/EntityPhysics.h"

namespace ri::trace {

ObjectPhysicsBatchResult StepKinematicEntityBatch(
    const TraceScene& scene,
    std::vector<KinematicEntitySlot>& entities,
    float deltaSeconds,
    const KinematicPhysicsOptions& options,
    const KinematicVolumeModifiers& modifiers,
    const KinematicConstraintState& constraints,
    const ObjectPhysicsBatchOptions& batchOptions) {
    std::vector<KinematicObjectSlot> scratch{};
    scratch.reserve(entities.size());
    for (const KinematicEntitySlot& entity : entities) {
        scratch.push_back(static_cast<const KinematicObjectSlot&>(entity));
    }

    ObjectPhysicsBatchResult result =
        StepKinematicObjectBatch(scene, scratch, deltaSeconds, options, modifiers, constraints, batchOptions);

    for (std::size_t index = 0; index < entities.size(); ++index) {
        static_cast<KinematicObjectSlot&>(entities[index]) = scratch[index];
    }

    return result;
}

} // namespace ri::trace
