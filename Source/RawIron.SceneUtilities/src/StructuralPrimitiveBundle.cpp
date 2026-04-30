#include "RawIron/Scene/StructuralPrimitiveBundle.h"

#include "RawIron/Scene/StructuralBrush.h"

namespace ri::scene {

StructuralPrimitiveBundleResult SpawnStructuralPrimitiveBundle(Scene& scene,
                                                               const StructuralPrimitiveBundleParams& params) {
    StructuralPrimitiveBundleResult result{};

    const std::string structuralType = ResolveStructuralPrimitiveTypeToken(
        params.primitiveTypeField ? std::optional<std::string_view>(*params.primitiveTypeField) : std::nullopt,
        params.typeAliasField ? std::optional<std::string_view>(*params.typeAliasField) : std::nullopt,
        "box");

    StructuralBrushSpawnOptions brush{};
    brush.nodeName = params.nodeName;
    brush.structuralType = structuralType;
    brush.shape = params.shape;
    brush.parent = params.parent;
    brush.transform = params.transform;
    brush.materialName = params.material.materialName;
    brush.shadingModel = params.material.shadingModel;
    brush.baseColor = params.material.baseColor;
    brush.baseColorTexture = params.material.baseColorTexture;
    brush.textureTiling = params.material.textureTiling;

    result.node = AddStructuralBrushNode(scene, brush);
    if (result.node == kInvalidHandle) {
        return result;
    }
    const Node& node = scene.GetNode(result.node);
    result.mesh = node.mesh;
    result.material = node.material;
    return result;
}

} // namespace ri::scene
