#include "RawIron/Scene/RuntimeMeshFactory.h"

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/PrimitiveTypeCanonical.h"
#include "RawIron/Scene/LevelObjectRegistry.h"
#include "RawIron/Scene/Scene.h"

#include <string>

namespace ri::scene {

PrimitiveType ParsePrimitiveKindLoose(const std::string_view token, const PrimitiveType fallback) {
    return ResolveScenePrimitiveTypeFromAuthoring(token, fallback);
}

int InstantiateRuntimePrimitive(Scene& scene,
                                const RuntimePrimitiveParams& params,
                                LevelObjectRegistry* registry) {
    const PrimitiveType resolvedPrimitive =
        params.primitiveKind.empty() ? params.primitive : ParsePrimitiveKindLoose(params.primitiveKind, params.primitive);

    PrimitiveNodeOptions options{};
    options.nodeName = params.nodeName;
    options.parent = params.parent;
    options.primitive = resolvedPrimitive;
    options.transform = params.transform;
    options.materialName = params.material.materialName;
    options.shadingModel = params.material.shadingModel;
    options.baseColor = params.material.baseColor;
    options.baseColorTexture = params.material.baseColorTexture;
    options.baseColorTextureFrames = params.material.baseColorTextureFrames;
    options.baseColorTextureFramesPerSecond = params.material.baseColorTextureFramesPerSecond;
    options.textureTiling = params.material.textureTiling;
    options.emissiveColor = params.material.emissiveColor;
    options.metallic = params.material.metallic;
    options.roughness = params.material.roughness;
    options.opacity = params.material.opacity;
    options.alphaCutoff = params.material.alphaCutoff;
    options.doubleSided = params.material.doubleSided;
    options.transparent = params.material.transparent;

    const int node = AddPrimitiveNode(scene, options);
    if (registry != nullptr && !params.registryId.empty()) {
        registry->Register(params.registryId, node);
    }
    return node;
}

} // namespace ri::scene
