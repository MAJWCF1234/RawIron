#include "RawIron/Scene/SceneStructuralTraceFeed.h"

#include "RawIron/Scene/TraceMeshRefinement.h"

namespace ri::scene {
namespace {

[[nodiscard]] int FindNodeHandleByColliderName(const Scene& scene, const std::string_view colliderId) {
    if (colliderId.empty()) {
        return -1;
    }
    for (int handle = 0; handle < static_cast<int>(scene.NodeCount()); ++handle) {
        if (scene.GetNode(handle).name == colliderId) {
            return handle;
        }
    }
    return -1;
}

} // namespace

ri::trace::StructuralTraceRefiner MakeStructuralMeshTraceRefiner(const Scene& scene,
                                                                  const MeshTraceRefinementOptions& meshOptions) {
    return [scenePtr = &scene, meshOptions](const ri::trace::TraceHit& coarse,
                                            const ri::math::Vec3& rayOriginWorld,
                                            const ri::math::Vec3& rayDirectionUnitWorld)
               -> std::optional<ri::trace::TraceHit> {
        const int nodeHandle = FindNodeHandleByColliderName(*scenePtr, coarse.id);
        if (nodeHandle < 0) {
            return std::nullopt;
        }
        const Ray worldRay{
            .origin = rayOriginWorld,
            .direction = rayDirectionUnitWorld,
        };
        return RefineTraceRayHitWithMeshTriangles(coarse, *scenePtr, nodeHandle, worldRay, meshOptions);
    };
}

} // namespace ri::scene
