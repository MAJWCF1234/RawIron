#include "RawIron/Scene/PhysicsPropSpawn.h"

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/SceneUtils.h"

namespace ri::scene {

PhysicsPropSpawnResult SpawnPhysicsPropFromAuthoring(Scene& scene,
                                                     const PhysicsPropAuthoring& authoring,
                                                     LevelObjectRegistry* registry) {
    PhysicsPropSpawnResult result{};

    RuntimePrimitiveParams params = authoring.visual;
    params.primitive =
        ResolveScenePrimitiveTypeFromAuthoring(authoring.primitiveTypeField ? std::optional<std::string_view>(*authoring.primitiveTypeField)
                                                                          : std::nullopt,
                                               authoring.typeAliasField ? std::optional<std::string_view>(*authoring.typeAliasField)
                                                                        : std::nullopt,
                                               authoring.primitiveFallback);
    params.primitiveKind.clear();

    result.visualNode = InstantiateRuntimePrimitive(scene, params, registry);

    if (authoring.attachPointLight && result.visualNode != kInvalidHandle) {
        LightNodeOptions light{};
        light.nodeName = params.nodeName + "_Light";
        light.parent = result.visualNode;
        light.light.name = light.nodeName;
        light.light.type = LightType::Point;
        light.light.color = authoring.lightColor;
        light.light.intensity = authoring.lightIntensity;
        light.light.range = authoring.lightRange;
        result.lightNode = AddLightNode(scene, light);
    }

    if (authoring.spawnTraceCollider && result.visualNode != kInvalidHandle) {
        if (const std::optional<ri::spatial::Aabb> bounds = TryComputeMeshNodeWorldAabb(scene, result.visualNode)) {
            ri::trace::TraceCollider collider{};
            collider.id = params.registryId.empty() ? scene.GetNode(result.visualNode).name : params.registryId;
            if (collider.id.empty()) {
                collider.id = "physics_prop_" + std::to_string(result.visualNode);
            }
            collider.bounds = *bounds;
            collider.structural = authoring.traceStructural;
            collider.dynamic = authoring.traceDynamic;
            collider.simulationTags = authoring.simulationTags;
            collider.simulationFlags = authoring.simulationFlags;
            result.traceCollider = std::move(collider);
        }
    }

    return result;
}

} // namespace ri::scene
