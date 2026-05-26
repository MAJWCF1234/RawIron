#include "RawIron/Scene/StructuralPrimitivePresets.h"

namespace ri::scene {

ri::structural::StructuralPrimitiveOptions ShapeFromStructuralPreset(const StructuralPrimitivePreset& preset) {
    ri::structural::StructuralPrimitiveOptions shape{};
    shape.radialSegments = preset.radialSegments;
    shape.sides = preset.sides;
    shape.hemisphereSegments = preset.hemisphereSegments;
    shape.thickness = preset.thickness;
    shape.topRadius = preset.topRadius;
    shape.bottomRadius = preset.bottomRadius;
    shape.length = preset.length;
    shape.spanDegrees = preset.spanDegrees;
    shape.ridgeRatio = preset.ridgeRatio;
    shape.archStyle = preset.archStyle;
    if (preset.structuralType == std::string_view("stairs") || preset.structuralType == std::string_view("spiral_stairs")) {
        shape.steps = preset.steps > 0 ? preset.steps : 8;
    }
    if (preset.structuralType == std::string_view("rounded_box")) {
        shape.bevelRadius = preset.bevelRadius;
        shape.bevelSegments = preset.bevelSegments;
    }
    return shape;
}

std::optional<StructuralPrimitivePreset> FindStructuralPreset(const std::string_view labelOrType) {
    for (const StructuralPrimitivePreset& preset : kStructuralPrimitivePresets) {
        if (labelOrType == preset.label || labelOrType == preset.structuralType) {
            return preset;
        }
    }
    return std::nullopt;
}

} // namespace ri::scene
