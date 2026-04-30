#include "RawIron/Structural/StructuralCompiler.h"
#include "RawIron/Structural/StructuralPrimitives.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <functional>
#include <map>
#include <set>

namespace ri::structural {
namespace {

std::uint64_t HashCombine(const std::uint64_t seed, const std::uint64_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
}

template <typename T>
std::uint64_t HashValue(const T& value) {
    return static_cast<std::uint64_t>(std::hash<T>{}(value));
}

ConvexSolid CloneSolid(const ConvexSolid& solid) {
    return solid;
}

struct SolidFragment {
    ConvexSolid solid;
    std::optional<Bounds> bounds;
};

struct PreparedBooleanSolid {
    ConvexSolid solid;
    std::optional<Bounds> bounds;
    std::vector<Plane> planes;
};

struct StructuralCutterVolume {
    StructuralNode node;
    bool intersect = false;
    std::set<std::string> targetIds;
    std::vector<PreparedBooleanSolid> solids;
};

std::vector<SolidFragment> BuildFragments(const std::vector<ConvexSolid>& solids) {
    std::vector<SolidFragment> fragments;
    fragments.reserve(solids.size());
    for (const ConvexSolid& solid : solids) {
        fragments.push_back(SolidFragment{
            .solid = solid,
            .bounds = ComputeSolidBounds(solid),
        });
    }
    return fragments;
}

std::vector<PreparedBooleanSolid> PrepareBooleanSolids(const std::vector<ConvexSolid>& solids) {
    std::vector<PreparedBooleanSolid> prepared;
    prepared.reserve(solids.size());
    for (const ConvexSolid& solid : solids) {
        std::vector<Plane> planes;
        planes.reserve(solid.polygons.size());
        for (const ConvexPolygon& polygon : solid.polygons) {
            planes.push_back(polygon.plane);
        }
        prepared.push_back(PreparedBooleanSolid{
            .solid = solid,
            .bounds = ComputeSolidBounds(solid),
            .planes = std::move(planes),
        });
    }
    return prepared;
}

bool BoundsIntersect(const std::optional<Bounds>& lhs, const std::optional<Bounds>& rhs) {
    if (!lhs.has_value() || !rhs.has_value()) {
        return true;
    }
    return !(lhs->max.x < rhs->min.x
          || lhs->min.x > rhs->max.x
          || lhs->max.y < rhs->min.y
          || lhs->min.y > rhs->max.y
          || lhs->max.z < rhs->min.z
          || lhs->min.z > rhs->max.z);
}

std::vector<StructuralBooleanTarget> CollectSupportedBooleanTargets(const std::set<std::string>& targetIds,
                                                                    const std::vector<StructuralBooleanTarget>& targets) {
    std::vector<StructuralBooleanTarget> supported;
    if (targetIds.size() < 2U) {
        return supported;
    }
    for (const StructuralBooleanTarget& target : targets) {
        if (!SupportsBooleanAdditiveTarget(target)) {
            continue;
        }
        if (targetIds.contains(target.node.id)) {
            supported.push_back(target);
        }
    }
    if (supported.size() < 2U) {
        supported.clear();
    }
    return supported;
}

std::vector<std::string> CollectSupportedTargetIds(const std::vector<StructuralBooleanTarget>& targets) {
    std::vector<std::string> ids;
    ids.reserve(targets.size());
    for (const StructuralBooleanTarget& target : targets) {
        if (!target.node.id.empty()) {
            ids.push_back(target.node.id);
        }
    }
    return ids;
}

float ClampFloat(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

int ClampInt(int value, int minValue, int maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

StructuralPrimitiveOptions BuildPrimitiveOptionsFromNode(const StructuralNode& node) {
    StructuralPrimitiveOptions options{};
    if (node.radialSegments > 0) {
        options.radialSegments = node.radialSegments;
    }
    if (node.sides > 0) {
        options.sides = node.sides;
    }
    if (node.detail > 0) {
        options.detail = node.detail;
    }
    if (node.steps > 0) {
        options.steps = node.steps;
    } else if (node.segments > 0) {
        options.steps = node.segments;
    }
    if (node.cellsX > 0) {
        options.cellsX = node.cellsX;
    }
    if (node.cellsY > 0) {
        options.cellsY = node.cellsY;
    }
    if (node.cellsZ > 0) {
        options.cellsZ = node.cellsZ;
    }
    if (node.hemisphereSegments > 0) {
        options.hemisphereSegments = node.hemisphereSegments;
    }
    if (node.thickness > 0.0f) {
        options.thickness = node.thickness;
    }
    if (node.depth > 0.0f) {
        options.depth = node.depth;
    }
    if (node.strutRadius > 0.0f) {
        options.strutRadius = node.strutRadius;
    }
    if (node.topRadius > 0.0f) {
        options.topRadius = node.topRadius;
    }
    if (node.bottomRadius > 0.0f) {
        options.bottomRadius = node.bottomRadius;
    }
    if (node.length > 0.0f) {
        options.length = node.length;
    }
    if (node.exponentX > 0.0f) {
        options.exponentX = node.exponentX;
    }
    if (node.exponentY > 0.0f) {
        options.exponentY = node.exponentY;
    }
    if (node.exponentZ > 0.0f) {
        options.exponentZ = node.exponentZ;
    }
    if (node.spanDegrees > 0.0f) {
        options.spanDegrees = node.spanDegrees;
    }
    if (node.sweepDegrees > 0.0f) {
        options.sweepDegrees = node.sweepDegrees;
    }
    if (node.startDegrees != 0.0f) {
        options.startDegrees = node.startDegrees;
    }
    if (node.ridgeRatio > 0.0f) {
        options.ridgeRatio = node.ridgeRatio;
    }
    options.centerColumn = node.centerColumn;
    if (!node.archStyle.empty()) {
        options.archStyle = node.archStyle;
    }
    if (!node.latticeStyle.empty()) {
        options.latticeStyle = node.latticeStyle;
    }
    if (!node.points.empty()) {
        options.points = node.points;
    }
    if (!node.vertices.empty()) {
        options.vertices = node.vertices;
    }
    return options;
}

std::string ResolveStructuralPrimitiveType(const StructuralNode& node) {
    if (!node.primitiveType.empty()) {
        return node.primitiveType;
    }
    return node.type;
}

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string ResolveStructuralOpType(const StructuralNode& node) {
    return ToLowerCopy(node.opType);
}

bool IsCompileOnlyStructuralNodeType(std::string_view type) {
    return type == "bevel_modifier_primitive"
        || type == "structural_detail_modifier"
        || type == "non_manifold_reconciler"
        || type == "convex_hull_aggregate";
}

bool IsDeferredStructuralTargetOperationNode(const StructuralNode& node) {
    return node.type == "terrain_hole_cutout"
        || node.type == "spline_mesh_deformer"
        || node.type == "spline_decal_ribbon"
        || node.type == "spline_ribbon"
        || node.type == "surface_scatter_volume"
        || node.type == "scatter_surface_primitive"
        || node.type == "shrinkwrap_modifier_primitive"
        || node.type == "auto_fillet_boolean_primitive"
        || node.type == "sdf_organic_blend_primitive"
        || node.type == "sdf_blend_node"
        || node.type == "sdf_intersection_node"
        || node.type == "automatic_convex_subdivision_modifier";
}

std::string NormalizeDeferredStructuralTargetOperationType(const StructuralNode& node) {
    if (node.type == "scatter_surface_primitive") {
        return "surface_scatter_volume";
    }
    if (node.type == "spline_ribbon") {
        return "spline_decal_ribbon";
    }
    if (node.type == "sdf_organic_blend_primitive" || node.type == "sdf_blend_node") {
        return "sdf_blend_node";
    }
    if (node.type == "auto_fillet_boolean_primitive") {
        return "fillet_boolean_modifier";
    }
    return node.type;
}

bool IsBooleanOperatorNode(const StructuralNode& node) {
    return node.type == "boolean_union"
        || (node.type == "boolean_intersection" && node.primitiveType.empty())
        || (node.type == "boolean_difference" && node.primitiveType.empty());
}

bool IsStructuralCutterNode(const StructuralNode& node) {
    const std::string opType = ResolveStructuralOpType(node);
    if (opType == "subtractive" || opType == "intersect") {
        return true;
    }
    if (node.type == "boolean_subtractor") {
        return true;
    }
    if (node.type == "boolean_intersection" && !node.primitiveType.empty()) {
        return true;
    }
    if (node.type == "door_window_cutout") {
        return true;
    }
    return false;
}

std::vector<std::pair<ri::math::Vec3, ri::math::Vec3>> BuildHollowBoxPrimitiveBoxExtents(float thickness) {
    const float wallThickness = ClampFloat(thickness > 0.0f ? thickness : 0.12f, 0.04f, 0.45f);
    std::vector<std::pair<ri::math::Vec3, ri::math::Vec3>> extents;
    extents.reserve(6);

    const auto pushBox = [&](const ri::math::Vec3& min, const ri::math::Vec3& max) {
        extents.emplace_back(min, max);
    };

    pushBox({-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f + wallThickness, 0.5f});
    pushBox({-0.5f, 0.5f - wallThickness, -0.5f}, {0.5f, 0.5f, 0.5f});

    const float innerMinY = -0.5f + wallThickness;
    const float innerMaxY = 0.5f - wallThickness;
    const float innerMinZ = -0.5f + wallThickness;
    const float innerMaxZ = 0.5f - wallThickness;
    const float innerMinX = -0.5f + wallThickness;
    const float innerMaxX = 0.5f - wallThickness;

    pushBox({-0.5f, innerMinY, innerMinZ}, {-0.5f + wallThickness, innerMaxY, innerMaxZ});
    pushBox({0.5f - wallThickness, innerMinY, innerMinZ}, {0.5f, innerMaxY, innerMaxZ});
    pushBox({innerMinX, innerMinY, -0.5f}, {innerMaxX, innerMaxY, -0.5f + wallThickness});
    pushBox({innerMinX, innerMinY, 0.5f - wallThickness}, {innerMaxX, innerMaxY, 0.5f});

    return extents;
}

std::optional<StructuralBooleanTarget> CreateBooleanTargetFromNode(const StructuralNode& node) {
    if (!SupportsBooleanAdditiveTargetNode(node)) {
        return std::nullopt;
    }

    std::vector<ConvexSolid> solids = BuildBooleanAdditiveSolidsFromNode(node);
    if (solids.empty()) {
        return std::nullopt;
    }

    return StructuralBooleanTarget{
        .node = node,
        .solids = std::move(solids),
    };
}

std::optional<StructuralCutterVolume> CreateStructuralCutterVolume(const StructuralNode& node) {
    StructuralNode sourceNode = node;
    const std::string opType = ResolveStructuralOpType(node);

    bool intersect = false;
    if (node.type == "boolean_subtractor") {
        sourceNode.type = ResolveStructuralPrimitiveType(node);
        sourceNode.primitiveType.clear();
    } else if (node.type == "boolean_intersection" && !node.primitiveType.empty()) {
        sourceNode.type = node.primitiveType;
        sourceNode.primitiveType.clear();
        intersect = true;
    } else if (node.type == "door_window_cutout") {
        sourceNode.type = "box";
        sourceNode.primitiveType.clear();
    }

    if (opType == "intersect") {
        intersect = true;
    } else if (opType == "subtractive") {
        intersect = false;
    }

    const std::vector<ConvexSolid> solids = BuildBooleanAdditiveSolidsFromNode(sourceNode);
    if (solids.empty()) {
        return std::nullopt;
    }

    StructuralCutterVolume volume{};
    volume.node = node;
    volume.intersect = intersect;
    volume.targetIds = std::set<std::string>(node.targetIds.begin(), node.targetIds.end());
    volume.solids = PrepareBooleanSolids(solids);
    return volume;
}

StructuralDeferredTargetOperation CreateDeferredTargetOperation(const StructuralNode& node) {
    StructuralDeferredTargetOperation operation{};
    operation.node = node;
    operation.normalizedType = NormalizeDeferredStructuralTargetOperationType(node);
    operation.targetIds = !node.targetIds.empty() ? node.targetIds : node.childNodeList;
    return operation;
}

void AppendCompiledNodes(std::vector<CompiledGeometryNode>& destination,
                         const std::vector<CompiledGeometryNode>& source) {
    destination.insert(destination.end(), source.begin(), source.end());
}

void ApplySuppressedTargetIds(std::vector<StructuralBooleanTarget>& remainingTargets,
                              std::vector<std::string>& suppressedTargetIds,
                              const std::vector<std::string>& consumedTargetIds) {
    if (consumedTargetIds.empty()) {
        return;
    }

    suppressedTargetIds.insert(suppressedTargetIds.end(), consumedTargetIds.begin(), consumedTargetIds.end());
    const std::set<std::string> suppressedSet(suppressedTargetIds.begin(), suppressedTargetIds.end());
    remainingTargets.erase(std::remove_if(remainingTargets.begin(),
                                          remainingTargets.end(),
                                          [&](const StructuralBooleanTarget& target) {
                                              return !target.node.id.empty() && suppressedSet.contains(target.node.id);
                                          }),
                           remainingTargets.end());
}

CompiledMesh TransformCompiledMesh(const CompiledMesh& mesh, const ri::math::Mat4& matrix) {
    CompiledMesh transformed{};
    transformed.positions.reserve(mesh.positions.size());
    transformed.normals.reserve(mesh.normals.size());
    transformed.triangleCount = mesh.triangleCount;
    for (const ri::math::Vec3& position : mesh.positions) {
        const ri::math::Vec3 transformedPosition = ri::math::TransformPoint(matrix, position);
        transformed.positions.push_back(transformedPosition);
        if (!transformed.hasBounds) {
            transformed.hasBounds = true;
            transformed.boundsMin = transformedPosition;
            transformed.boundsMax = transformedPosition;
        } else {
            transformed.boundsMin.x = std::min(transformed.boundsMin.x, transformedPosition.x);
            transformed.boundsMin.y = std::min(transformed.boundsMin.y, transformedPosition.y);
            transformed.boundsMin.z = std::min(transformed.boundsMin.z, transformedPosition.z);
            transformed.boundsMax.x = std::max(transformed.boundsMax.x, transformedPosition.x);
            transformed.boundsMax.y = std::max(transformed.boundsMax.y, transformedPosition.y);
            transformed.boundsMax.z = std::max(transformed.boundsMax.z, transformedPosition.z);
        }
    }
    for (const ri::math::Vec3& normal : mesh.normals) {
        transformed.normals.push_back(ri::math::Normalize(ri::math::TransformVector(matrix, normal)));
    }
    return transformed;
}

ri::math::Vec3 ExtractRotationXYZDegrees(const ri::math::Mat4& matrix) {
    const ri::math::Vec3 scale = ri::math::ExtractScale(matrix);
    const float safeScaleX = std::fabs(scale.x) > 1e-6f ? scale.x : 1.0f;
    const float safeScaleY = std::fabs(scale.y) > 1e-6f ? scale.y : 1.0f;
    const float safeScaleZ = std::fabs(scale.z) > 1e-6f ? scale.z : 1.0f;

    const float r00 = matrix.m[0][0] / safeScaleX;
    const float r10 = matrix.m[1][0] / safeScaleX;
    const float r20 = matrix.m[2][0] / safeScaleX;
    const float r21 = matrix.m[2][1] / safeScaleY;
    const float r22 = matrix.m[2][2] / safeScaleZ;
    const float r11 = matrix.m[1][1] / safeScaleY;
    const float r12 = matrix.m[1][2] / safeScaleZ;

    float xRadians = 0.0f;
    float yRadians = 0.0f;
    float zRadians = 0.0f;

    const float clampedR20 = ClampFloat(r20, -1.0f, 1.0f);
    if (std::fabs(clampedR20) < 0.9999f) {
        yRadians = std::asin(-clampedR20);
        xRadians = std::atan2(r21, r22);
        zRadians = std::atan2(r10, r00);
    } else {
        yRadians = clampedR20 <= -1.0f ? (ri::math::kPi * 0.5f) : (-ri::math::kPi * 0.5f);
        xRadians = std::atan2(-r12, r11);
        zRadians = 0.0f;
    }

    constexpr float kRadiansToDegrees = 180.0f / ri::math::kPi;
    return ri::math::Vec3{
        xRadians * kRadiansToDegrees,
        yRadians * kRadiansToDegrees,
        zRadians * kRadiansToDegrees,
    };
}

} // namespace

std::optional<Bounds> ComputeSolidBounds(const ConvexSolid& solid) {
    Bounds bounds{};
    bool hasVertex = false;
    for (const ConvexPolygon& polygon : solid.polygons) {
        for (const ri::math::Vec3& vertex : polygon.vertices) {
            if (!hasVertex) {
                bounds.min = vertex;
                bounds.max = vertex;
                hasVertex = true;
                continue;
            }
            bounds.min.x = std::min(bounds.min.x, vertex.x);
            bounds.min.y = std::min(bounds.min.y, vertex.y);
            bounds.min.z = std::min(bounds.min.z, vertex.z);
            bounds.max.x = std::max(bounds.max.x, vertex.x);
            bounds.max.y = std::max(bounds.max.y, vertex.y);
            bounds.max.z = std::max(bounds.max.z, vertex.z);
        }
    }
    if (!hasVertex) {
        return std::nullopt;
    }
    return bounds;
}

ConvexSolid TransformSolid(const ConvexSolid& solid, const ri::math::Mat4& matrix) {
    ConvexSolid transformed{};
    transformed.polygons.reserve(solid.polygons.size());

    for (const ConvexPolygon& polygon : solid.polygons) {
        ConvexPolygon transformedPolygon{};
        transformedPolygon.vertices.reserve(polygon.vertices.size());
        for (const ri::math::Vec3& vertex : polygon.vertices) {
            transformedPolygon.vertices.push_back(ri::math::TransformPoint(matrix, vertex));
        }
        transformedPolygon.plane = ComputePolygonPlane(transformedPolygon.vertices).value_or(Plane{});
        transformed.polygons.push_back(std::move(transformedPolygon));
    }

    return transformed;
}

ConvexSolid CreateWorldSpaceBoxSolid(const ri::math::Mat4& matrix, const ri::math::Vec3& min, const ri::math::Vec3& max) {
    return TransformSolid(CreateAxisAlignedBoxSolid(min, max), matrix);
}

ri::math::Mat4 GetNodeTransformMatrix(const StructuralNode& node) {
    return ri::math::TRS(node.position, node.rotation, node.scale);
}

ri::math::Mat4 GetOffsetStepMatrix(const StructuralNode& node) {
    return ri::math::TRS(node.offsetStepPosition, node.offsetStepRotation, node.offsetStepScale);
}

ri::math::Vec3 MirrorAxisVector(std::string_view axis, float origin, const ri::math::Vec3& position) {
    ri::math::Vec3 mirrored = position;
    if (axis == "y") {
        mirrored.y = (origin * 2.0f) - mirrored.y;
    } else if (axis == "z") {
        mirrored.z = (origin * 2.0f) - mirrored.z;
    } else {
        mirrored.x = (origin * 2.0f) - mirrored.x;
    }
    return mirrored;
}

ri::math::Vec3 MirrorAxisRotation(std::string_view axis, const ri::math::Vec3& rotationDegrees) {
    if (axis == "y") {
        return ri::math::Vec3{-rotationDegrees.x, rotationDegrees.y, -rotationDegrees.z};
    }
    if (axis == "z") {
        return ri::math::Vec3{-rotationDegrees.x, -rotationDegrees.y, rotationDegrees.z};
    }
    return ri::math::Vec3{rotationDegrees.x, -rotationDegrees.y, -rotationDegrees.z};
}

StructuralNode CreateMirroredStructuralNode(const StructuralNode& baseNode,
                                            const StructuralNode& mirrorNode,
                                            std::size_t index) {
    const std::string_view axis = mirrorNode.mirrorAxis.empty() ? std::string_view("x") : std::string_view(mirrorNode.mirrorAxis);
    const float origin = axis == "y"
        ? mirrorNode.position.y
        : (axis == "z" ? mirrorNode.position.z : mirrorNode.position.x);

    StructuralNode mirrored = baseNode;
    mirrored.id = !baseNode.id.empty()
        ? baseNode.id + "_mirrored_" + std::to_string(index + 1U)
        : (!mirrorNode.id.empty() ? mirrorNode.id + "_" + std::to_string(index + 1U) : "mirror_" + std::to_string(index + 1U));
    mirrored.position = MirrorAxisVector(axis, origin, baseNode.position);
    mirrored.rotation = MirrorAxisRotation(axis, baseNode.rotation);
    mirrored.mirroredFrom = baseNode.id;
    return mirrored;
}

std::vector<StructuralNode> ExpandSymmetryMirrorNodes(const std::vector<StructuralNode>& nodes) {
    if (nodes.empty()) {
        return {};
    }

    std::vector<StructuralNode> expanded = nodes;
    for (const StructuralNode& node : nodes) {
        if (node.type != "symmetry_mirror_plane") {
            continue;
        }
        const std::vector<std::string> targetIds = GetBooleanOperatorTargetIds(node);
        if (targetIds.empty()) {
            continue;
        }
        std::size_t mirrorIndex = 0;
        for (const std::string& targetId : targetIds) {
            auto found = std::find_if(nodes.begin(), nodes.end(), [&](const StructuralNode& candidate) {
                return candidate.id == targetId;
            });
            if (found != nodes.end()) {
                expanded.push_back(CreateMirroredStructuralNode(*found, node, mirrorIndex));
                mirrorIndex += 1U;
            }
        }
    }
    return expanded;
}

StructuralNode CreateTransformedArrayPrimitiveNode(const StructuralNode& baseNode,
                                                   const ri::math::Mat4& transformMatrix,
                                                   const StructuralNode& arrayNode,
                                                   std::size_t index) {
    StructuralNode transformed = baseNode;
    transformed.type = !baseNode.primitiveType.empty() ? baseNode.primitiveType : baseNode.type;
    transformed.id = !baseNode.id.empty()
        ? baseNode.id + "_array_" + std::to_string(index + 1U)
        : (!arrayNode.id.empty() ? arrayNode.id + "_" + std::to_string(index + 1U) : std::string{});
    transformed.position = ri::math::ExtractTranslation(transformMatrix);
    transformed.rotation = ExtractRotationXYZDegrees(transformMatrix);
    transformed.scale = ri::math::ExtractScale(transformMatrix);
    return transformed;
}

std::vector<StructuralNode> ExpandArrayPrimitiveNodes(const std::vector<StructuralNode>& nodes) {
    std::vector<StructuralNode> expanded;
    expanded.reserve(nodes.size());

    for (const StructuralNode& node : nodes) {
        if (node.type != "array_primitive") {
            expanded.push_back(node);
            continue;
        }

        const int count = node.count > 0 ? std::min(node.count, 128) : 3;
        StructuralNode basePrimitive = node.basePrimitive ? *node.basePrimitive : StructuralNode{};
        if (basePrimitive.type.empty()) {
            basePrimitive.type = !node.primitiveType.empty() ? node.primitiveType : "box";
        }

        const ri::math::Mat4 rootMatrix = GetNodeTransformMatrix(node);
        const ri::math::Mat4 baseMatrix = GetNodeTransformMatrix(basePrimitive);
        const ri::math::Mat4 stepMatrix = GetOffsetStepMatrix(node);

        ri::math::Mat4 runningMatrix = ri::math::IdentityMatrix();
        for (int index = 0; index < count; ++index) {
            const ri::math::Mat4 transform = ri::math::Multiply(rootMatrix, ri::math::Multiply(runningMatrix, baseMatrix));
            expanded.push_back(CreateTransformedArrayPrimitiveNode(basePrimitive, transform, node, static_cast<std::size_t>(index)));
            runningMatrix = ri::math::Multiply(runningMatrix, stepMatrix);
        }
    }

    return expanded;
}

std::optional<Bounds> CreateGeometryBoundsForNode(const StructuralNode& node) {
    const std::string resolvedType = ResolveStructuralPrimitiveType(node);
    if (!IsNativeStructuralPrimitive(resolvedType)) {
        return std::nullopt;
    }

    const StructuralPrimitiveOptions options = BuildPrimitiveOptionsFromNode(node);
    const ri::math::Mat4 transform = GetNodeTransformMatrix(node);

    const std::optional<ConvexSolid> solid = CreateConvexPrimitiveSolid(resolvedType, options);
    if (solid.has_value()) {
        return ComputeSolidBounds(TransformSolid(*solid, transform));
    }

    const CompiledMesh mesh = TransformCompiledMesh(BuildPrimitiveMesh(resolvedType, options), transform);
    if (!mesh.hasBounds) {
        return std::nullopt;
    }
    return Bounds{.min = mesh.boundsMin, .max = mesh.boundsMax};
}

StructuralNode ApplyBevelModifiersToNode(const StructuralNode& node,
                                         const std::vector<StructuralNode>& bevelModifiers) {
    if (ResolveStructuralPrimitiveType(node) != "box" || bevelModifiers.empty()) {
        return node;
    }

    const std::optional<Bounds> nodeBounds = CreateGeometryBoundsForNode(node);
    if (!nodeBounds.has_value()) {
        return node;
    }

    float appliedRadius = node.bevelRadius;
    int appliedSegments = node.bevelSegments;

    for (const StructuralNode& modifier : bevelModifiers) {
        StructuralNode modifierVolume = modifier;
        modifierVolume.type = "box";
        modifierVolume.primitiveType.clear();

        const std::optional<Bounds> modifierBounds = CreateGeometryBoundsForNode(modifierVolume);
        if (!modifierBounds.has_value() || !BoundsIntersect(nodeBounds, modifierBounds)) {
            continue;
        }
        if (!modifier.targetIds.empty()) {
            const std::string nodeId = !node.id.empty() ? node.id : node.name;
            if (nodeId.empty() || std::find(modifier.targetIds.begin(), modifier.targetIds.end(), nodeId) == modifier.targetIds.end()) {
                continue;
            }
        }

        const float modifierRadius = ClampFloat(modifier.bevelRadius > 0.0f ? modifier.bevelRadius : 0.12f,
                                                0.01f,
                                                0.45f);
        const int modifierSegments = ClampInt(modifier.bevelSegments > 0 ? modifier.bevelSegments : 4, 1, 12);
        appliedRadius = std::max(appliedRadius, modifierRadius);
        appliedSegments = std::max(appliedSegments, modifierSegments);
    }

    if (!(appliedRadius > 0.0f)) {
        return node;
    }

    StructuralNode beveled = node;
    beveled.bevelRadius = appliedRadius;
    beveled.bevelSegments = appliedSegments > 0 ? appliedSegments : node.bevelSegments;
    return beveled;
}

StructuralNode ApplyStructuralDetailModifiersToNode(const StructuralNode& node,
                                                    const std::vector<StructuralNode>& detailModifiers) {
    if (detailModifiers.empty()) {
        return node;
    }

    const std::optional<Bounds> nodeBounds = CreateGeometryBoundsForNode(node);
    if (!nodeBounds.has_value()) {
        return node;
    }

    bool flaggedAsDetail = !node.isStructural;
    for (const StructuralNode& modifier : detailModifiers) {
        StructuralNode modifierVolume = modifier;
        modifierVolume.type = "box";
        const std::optional<Bounds> modifierBounds = CreateGeometryBoundsForNode(modifierVolume);
        if (!modifierBounds.has_value() || !BoundsIntersect(nodeBounds, modifierBounds)) {
            continue;
        }
        if (!modifier.targetIds.empty()) {
            const std::string nodeId = !node.id.empty() ? node.id : node.name;
            if (nodeId.empty() || std::find(modifier.targetIds.begin(), modifier.targetIds.end(), nodeId) == modifier.targetIds.end()) {
                continue;
            }
        }
        flaggedAsDetail = true;
    }

    if (!flaggedAsDetail) {
        return node;
    }

    StructuralNode detailed = node;
    detailed.isStructural = false;
    detailed.detailOnly = true;
    detailed.excludeFromVisibility = true;
    detailed.excludeFromNavigation = true;
    return detailed;
}

StructuralNode ApplyNonManifoldReconcilersToNode(const StructuralNode& node,
                                                 const std::vector<StructuralNode>& reconcilers,
                                                 const bool allowHighCostHullFallback) {
    if (reconcilers.empty()) {
        return node;
    }

    const std::optional<Bounds> nodeBounds = CreateGeometryBoundsForNode(node);
    if (!nodeBounds.has_value()) {
        return node;
    }

    const std::string nodeType = ResolveStructuralPrimitiveType(node);
    const std::set<std::string> targetableTypes = {
        "hexahedron",
        "convex_hull",
    };

    bool shouldReconcile = false;
    bool forceHull = false;
    for (const StructuralNode& reconciler : reconcilers) {
        StructuralNode reconcilerVolume = reconciler;
        reconcilerVolume.type = "box";
        const std::optional<Bounds> reconcilerBounds = CreateGeometryBoundsForNode(reconcilerVolume);
        if (!reconcilerBounds.has_value() || !BoundsIntersect(nodeBounds, reconcilerBounds)) {
            continue;
        }
        if (!reconciler.targetIds.empty()) {
            const std::string nodeId = !node.id.empty() ? node.id : node.name;
            if (nodeId.empty() || std::find(reconciler.targetIds.begin(), reconciler.targetIds.end(), nodeId) == reconciler.targetIds.end()) {
                continue;
            }
        }
        shouldReconcile = true;
        forceHull = forceHull || (allowHighCostHullFallback && reconciler.forceHull);
    }

    if (!shouldReconcile) {
        return node;
    }
    if (!forceHull && !targetableTypes.contains(nodeType)) {
        return node;
    }
    if (!IsNativeStructuralPrimitive(nodeType)) {
        return node;
    }

    const StructuralPrimitiveOptions options = BuildPrimitiveOptionsFromNode(node);
    const CompiledMesh worldMesh = TransformCompiledMesh(BuildPrimitiveMesh(nodeType, options), GetNodeTransformMatrix(node));
    if (worldMesh.positions.size() < 4U) {
        return node;
    }

    StructuralNode reconciled = node;
    reconciled.type = "convex_hull";
    reconciled.primitiveType.clear();
    reconciled.position = {};
    reconciled.rotation = {};
    reconciled.scale = {1.0f, 1.0f, 1.0f};
    reconciled.points = worldMesh.positions;
    reconciled.vertices.clear();
    reconciled.reconciledNonManifold = true;
    return reconciled;
}

std::vector<std::string> GetBooleanOperatorTargetIds(const StructuralNode& node) {
    if (!node.targetIds.empty()) {
        return node.targetIds;
    }
    if (!node.childNodeList.empty()) {
        return node.childNodeList;
    }
    return {};
}

bool SupportsBooleanAdditiveTarget(const StructuralBooleanTarget& target) {
    return !target.node.id.empty() && !target.solids.empty();
}

bool SupportsBooleanAdditiveTargetNode(const StructuralNode& node) {
    const std::string resolvedType = ResolveStructuralPrimitiveType(node);
    if (resolvedType == "box" || resolvedType == "hollow_box") {
        return true;
    }

    const StructuralPrimitiveOptions options = BuildPrimitiveOptionsFromNode(node);
    return CreateConvexPrimitiveSolid(resolvedType, options).has_value();
}

std::vector<ConvexSolid> BuildBooleanAdditiveSolidsFromNode(const StructuralNode& node) {
    const std::string resolvedType = ResolveStructuralPrimitiveType(node);
    const ri::math::Mat4 transform = GetNodeTransformMatrix(node);

    if (resolvedType == "box") {
        return {CreateWorldSpaceBoxSolid(transform)};
    }

    if (resolvedType == "hollow_box") {
        std::vector<ConvexSolid> solids;
        const auto extents = BuildHollowBoxPrimitiveBoxExtents(node.thickness);
        solids.reserve(extents.size());
        for (const auto& [min, max] : extents) {
            solids.push_back(CreateWorldSpaceBoxSolid(transform, min, max));
        }
        return solids;
    }

    const StructuralPrimitiveOptions options = BuildPrimitiveOptionsFromNode(node);
    const std::optional<ConvexSolid> solid = CreateConvexPrimitiveSolid(resolvedType, options);
    if (!solid.has_value()) {
        return {};
    }

    return {TransformSolid(*solid, transform)};
}

std::vector<Plane> ExtractConvexPlanesFromTriangles(const std::vector<Triangle>& triangles,
                                                    const ri::math::Mat4& transform,
                                                    float epsilon) {
    std::vector<Plane> planes;
    planes.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        const std::vector<ri::math::Vec3> vertices = {
            ri::math::TransformPoint(transform, triangle.a),
            ri::math::TransformPoint(transform, triangle.b),
            ri::math::TransformPoint(transform, triangle.c),
        };
        const std::optional<Plane> plane = ComputePolygonPlane(vertices);
        if (!plane.has_value() || ri::math::LengthSquared(plane->normal) <= 1e-8f) {
            continue;
        }
        planes.push_back(*plane);
    }
    return DedupeConvexPlanes(planes, epsilon);
}

std::vector<ConvexSolid> SubtractConvexPlanesFromSolid(const ConvexSolid& solid,
                                                       const std::vector<Plane>& planes,
                                                       float epsilon) {
    std::vector<ConvexSolid> insidePieces = {CloneSolid(solid)};
    std::vector<ConvexSolid> outsidePieces;

    for (const Plane& plane : planes) {
        std::vector<ConvexSolid> nextInsidePieces;
        for (const ConvexSolid& candidate : insidePieces) {
            const ConvexSolidClipResult clipped = ClipConvexSolidByPlane(candidate, plane, epsilon);
            if (clipped.front.has_value()) {
                outsidePieces.push_back(*clipped.front);
            }
            if (clipped.back.has_value()) {
                nextInsidePieces.push_back(*clipped.back);
            }
        }
        insidePieces = std::move(nextInsidePieces);
        if (insidePieces.empty()) {
            break;
        }
    }

    return outsidePieces;
}

std::vector<ConvexSolid> IntersectSolidWithConvexPlanes(const ConvexSolid& solid,
                                                        const std::vector<Plane>& planes,
                                                        float epsilon) {
    std::vector<ConvexSolid> pieces = {CloneSolid(solid)};
    for (const Plane& plane : planes) {
        std::vector<ConvexSolid> nextPieces;
        for (const ConvexSolid& candidate : pieces) {
            const ConvexSolidClipResult clipped = ClipConvexSolidByPlane(candidate, plane, epsilon);
            if (clipped.back.has_value()) {
                nextPieces.push_back(*clipped.back);
            }
        }
        pieces = std::move(nextPieces);
        if (pieces.empty()) {
            break;
        }
    }
    return pieces;
}

std::vector<CompiledGeometryNode> BuildCompiledGeometryNodesFromSolids(const StructuralNode& baseNode,
                                                                       const std::vector<ConvexSolid>& solids,
                                                                       std::string_view idPrefix) {
    std::vector<CompiledGeometryNode> nodes;
    nodes.reserve(solids.size());

    const std::string prefix(idPrefix);
    for (std::size_t index = 0; index < solids.size(); ++index) {
        CompiledGeometryNode compiled{};
        compiled.node = baseNode;
        if (!prefix.empty() || !baseNode.id.empty()) {
            compiled.node.id = (prefix.empty() ? baseNode.id : prefix) + "_fragment_" + std::to_string(index + 1);
        }
        if (!baseNode.name.empty()) {
            compiled.node.name = baseNode.name + " fragment " + std::to_string(index + 1);
        }
        compiled.compiledMesh = BuildCompiledMeshFromConvexSolid(solids[index]);
        nodes.push_back(std::move(compiled));
    }

    return nodes;
}

StructuralBooleanCompileResult CompileBooleanUnionNode(const StructuralNode& unionNode,
                                                       const std::vector<StructuralBooleanTarget>& targets) {
    const std::vector<std::string> rawTargetIds = GetBooleanOperatorTargetIds(unionNode);
    const std::set<std::string> targetIds(rawTargetIds.begin(), rawTargetIds.end());
    const std::vector<StructuralBooleanTarget> supportedTargets = CollectSupportedBooleanTargets(targetIds, targets);
    if (supportedTargets.empty()) {
        return {};
    }

    std::vector<CompiledGeometryNode> compiledNodes;
    for (std::size_t targetIndex = 0; targetIndex < supportedTargets.size(); ++targetIndex) {
        const StructuralBooleanTarget& target = supportedTargets[targetIndex];
        std::vector<SolidFragment> fragments = BuildFragments(target.solids);

        for (std::size_t otherIndex = targetIndex + 1; otherIndex < supportedTargets.size(); ++otherIndex) {
            const std::vector<PreparedBooleanSolid> otherSolids = PrepareBooleanSolids(supportedTargets[otherIndex].solids);
            for (const PreparedBooleanSolid& otherSolid : otherSolids) {
                std::vector<SolidFragment> nextFragments;
                for (const SolidFragment& fragment : fragments) {
                    if (!BoundsIntersect(fragment.bounds, otherSolid.bounds)) {
                        nextFragments.push_back(fragment);
                        continue;
                    }
                    for (const ConvexSolid& result : SubtractConvexPlanesFromSolid(fragment.solid, otherSolid.planes)) {
                        nextFragments.push_back(SolidFragment{
                            .solid = result,
                            .bounds = ComputeSolidBounds(result),
                        });
                    }
                }
                fragments = std::move(nextFragments);
                if (fragments.empty()) {
                    break;
                }
            }
            if (fragments.empty()) {
                break;
            }
        }

        if (!fragments.empty()) {
            std::vector<ConvexSolid> solids;
            solids.reserve(fragments.size());
            for (const SolidFragment& fragment : fragments) {
                solids.push_back(fragment.solid);
            }
            const std::string prefix = !unionNode.id.empty()
                ? unionNode.id + '_' + (target.node.id.empty() ? std::to_string(targetIndex + 1U) : target.node.id)
                : (target.node.id.empty() ? "boolean_union" : target.node.id);
            std::vector<CompiledGeometryNode> targetNodes =
                BuildCompiledGeometryNodesFromSolids(target.node, solids, prefix);
            compiledNodes.insert(compiledNodes.end(), targetNodes.begin(), targetNodes.end());
        }
    }

    return StructuralBooleanCompileResult{
        .compiledNodes = std::move(compiledNodes),
        .targetIds = CollectSupportedTargetIds(supportedTargets),
    };
}

StructuralBooleanCompileResult CompileBooleanIntersectionNode(const StructuralNode& intersectionNode,
                                                              const std::vector<StructuralBooleanTarget>& targets) {
    const std::vector<std::string> rawTargetIds = GetBooleanOperatorTargetIds(intersectionNode);
    const std::set<std::string> targetIds(rawTargetIds.begin(), rawTargetIds.end());
    const std::vector<StructuralBooleanTarget> supportedTargets = CollectSupportedBooleanTargets(targetIds, targets);
    if (supportedTargets.empty()) {
        return {};
    }

    std::vector<SolidFragment> fragments = BuildFragments(supportedTargets.front().solids);
    for (std::size_t targetIndex = 1; targetIndex < supportedTargets.size(); ++targetIndex) {
        const std::vector<PreparedBooleanSolid> targetSolids = PrepareBooleanSolids(supportedTargets[targetIndex].solids);
        std::vector<SolidFragment> nextFragments;
        for (const SolidFragment& fragment : fragments) {
            for (const PreparedBooleanSolid& targetSolid : targetSolids) {
                if (!BoundsIntersect(fragment.bounds, targetSolid.bounds)) {
                    continue;
                }
                for (const ConvexSolid& result : IntersectSolidWithConvexPlanes(fragment.solid, targetSolid.planes)) {
                    nextFragments.push_back(SolidFragment{
                        .solid = result,
                        .bounds = ComputeSolidBounds(result),
                    });
                }
            }
        }
        fragments = std::move(nextFragments);
        if (fragments.empty()) {
            break;
        }
    }

    std::vector<CompiledGeometryNode> compiledNodes;
    if (!fragments.empty()) {
        std::vector<ConvexSolid> solids;
        solids.reserve(fragments.size());
        for (const SolidFragment& fragment : fragments) {
            solids.push_back(fragment.solid);
        }
        const StructuralNode& baseNode = supportedTargets.front().node;
        const std::string prefix = !intersectionNode.id.empty()
            ? intersectionNode.id
            : (!baseNode.id.empty() ? baseNode.id : "boolean_intersection");
        compiledNodes = BuildCompiledGeometryNodesFromSolids(baseNode, solids, prefix);
    }

    return StructuralBooleanCompileResult{
        .compiledNodes = std::move(compiledNodes),
        .targetIds = CollectSupportedTargetIds(supportedTargets),
    };
}

StructuralBooleanCompileResult CompileBooleanDifferenceNode(const StructuralNode& differenceNode,
                                                            const std::vector<StructuralBooleanTarget>& targets) {
    const std::vector<std::string> rawTargetIds = GetBooleanOperatorTargetIds(differenceNode);
    const std::set<std::string> targetIds(rawTargetIds.begin(), rawTargetIds.end());
    const std::vector<StructuralBooleanTarget> supportedTargets = CollectSupportedBooleanTargets(targetIds, targets);
    if (supportedTargets.empty()) {
        return {};
    }

    std::vector<CompiledGeometryNode> compiledNodes;
    for (std::size_t targetIndex = 0; targetIndex < supportedTargets.size(); ++targetIndex) {
        const StructuralBooleanTarget& target = supportedTargets[targetIndex];
        std::vector<SolidFragment> fragments = BuildFragments(target.solids);

        for (std::size_t otherIndex = 0; otherIndex < supportedTargets.size(); ++otherIndex) {
            if (otherIndex == targetIndex || fragments.empty()) {
                continue;
            }
            const std::vector<PreparedBooleanSolid> otherSolids = PrepareBooleanSolids(supportedTargets[otherIndex].solids);
            for (const PreparedBooleanSolid& otherSolid : otherSolids) {
                std::vector<SolidFragment> nextFragments;
                for (const SolidFragment& fragment : fragments) {
                    if (!BoundsIntersect(fragment.bounds, otherSolid.bounds)) {
                        nextFragments.push_back(fragment);
                        continue;
                    }
                    for (const ConvexSolid& result : SubtractConvexPlanesFromSolid(fragment.solid, otherSolid.planes)) {
                        nextFragments.push_back(SolidFragment{
                            .solid = result,
                            .bounds = ComputeSolidBounds(result),
                        });
                    }
                }
                fragments = std::move(nextFragments);
                if (fragments.empty()) {
                    break;
                }
            }
        }

        if (!fragments.empty()) {
            std::vector<ConvexSolid> solids;
            solids.reserve(fragments.size());
            for (const SolidFragment& fragment : fragments) {
                solids.push_back(fragment.solid);
            }
            const std::string prefix = !differenceNode.id.empty()
                ? differenceNode.id + '_' + (target.node.id.empty() ? std::to_string(targetIndex + 1U) : target.node.id)
                : (target.node.id.empty() ? "boolean_difference" : target.node.id);
            std::vector<CompiledGeometryNode> targetNodes =
                BuildCompiledGeometryNodesFromSolids(target.node, solids, prefix);
            compiledNodes.insert(compiledNodes.end(), targetNodes.begin(), targetNodes.end());
        }
    }

    return StructuralBooleanCompileResult{
        .compiledNodes = std::move(compiledNodes),
        .targetIds = CollectSupportedTargetIds(supportedTargets),
    };
}

std::vector<CompiledGeometryNode> CompileConvexHullAggregateNode(const StructuralNode& aggregateNode,
                                                                 const std::vector<StructuralBooleanTarget>& targets) {
    const std::vector<std::string> rawTargetIds = GetBooleanOperatorTargetIds(aggregateNode);
    const std::set<std::string> targetIds(rawTargetIds.begin(), rawTargetIds.end());
    const std::vector<StructuralBooleanTarget> supportedTargets = CollectSupportedBooleanTargets(targetIds, targets);
    if (supportedTargets.empty()) {
        return {};
    }

    std::vector<ri::math::Vec3> points;
    for (const StructuralBooleanTarget& target : supportedTargets) {
        for (const ConvexSolid& solid : target.solids) {
            for (const ConvexPolygon& polygon : solid.polygons) {
                points.insert(points.end(), polygon.vertices.begin(), polygon.vertices.end());
            }
        }
    }

    if (points.size() < 4U) {
        return {};
    }

    StructuralPrimitiveOptions options{};
    options.points = std::move(points);
    const std::optional<ConvexSolid> hull = CreateConvexPrimitiveSolid("convex_hull", options);
    if (!hull.has_value()) {
        return {};
    }

    const std::string prefix = !aggregateNode.id.empty() ? aggregateNode.id : "convex_hull_aggregate";
    return BuildCompiledGeometryNodesFromSolids(aggregateNode, {*hull}, prefix);
}

StructuralGeometryCompileResult CompileStructuralGeometryNodes(const std::vector<StructuralNode>& nodes,
                                                               const StructuralCompileOptions& options) {
    StructuralGeometryCompileResult result{};
    if (nodes.empty()) {
        return result;
    }

    result.expandedNodes = ExpandArrayPrimitiveNodes(ExpandSymmetryMirrorNodes(nodes));

    std::vector<StructuralNode> bevelModifiers;
    std::vector<StructuralNode> detailModifiers;
    std::vector<StructuralNode> reconcilers;
    std::vector<StructuralNode> aggregateNodes;
    std::vector<StructuralNode> unionNodes;
    std::vector<StructuralNode> intersectionNodes;
    std::vector<StructuralNode> differenceNodes;
    std::vector<StructuralCutterVolume> cutterVolumes;

    for (const StructuralNode& node : result.expandedNodes) {
        if (options.enableHighCostBooleanPasses && node.type == "boolean_union") {
            unionNodes.push_back(node);
            continue;
        }
        if (options.enableHighCostBooleanPasses && node.type == "boolean_intersection" && node.primitiveType.empty()) {
            intersectionNodes.push_back(node);
            continue;
        }
        if (options.enableHighCostBooleanPasses && node.type == "boolean_difference" && node.primitiveType.empty()) {
            differenceNodes.push_back(node);
            continue;
        }
        if (node.type == "bevel_modifier_primitive") {
            bevelModifiers.push_back(node);
            result.passthroughNodes.push_back(node);
            continue;
        }
        if (node.type == "structural_detail_modifier") {
            detailModifiers.push_back(node);
            result.passthroughNodes.push_back(node);
            continue;
        }
        if (node.type == "non_manifold_reconciler") {
            reconcilers.push_back(node);
            result.passthroughNodes.push_back(node);
            continue;
        }
        if (node.type == "convex_hull_aggregate") {
            aggregateNodes.push_back(node);
            result.passthroughNodes.push_back(node);
            continue;
        }
        if (IsDeferredStructuralTargetOperationNode(node)) {
            result.deferredOperations.push_back(CreateDeferredTargetOperation(node));
            continue;
        }
        if (options.enableHighCostBooleanPasses && IsStructuralCutterNode(node)) {
            const std::optional<StructuralCutterVolume> cutterVolume = CreateStructuralCutterVolume(node);
            if (cutterVolume.has_value()) {
                cutterVolumes.push_back(*cutterVolume);
            }
            result.passthroughNodes.push_back(node);
            continue;
        }
        if (IsCompileOnlyStructuralNodeType(node.type)) {
            result.passthroughNodes.push_back(node);
            continue;
        }
    }

    for (const StructuralNode& node : result.expandedNodes) {
        if (IsBooleanOperatorNode(node)
            || IsDeferredStructuralTargetOperationNode(node)
            || IsStructuralCutterNode(node)
            || IsCompileOnlyStructuralNodeType(node.type)) {
            continue;
        }

        StructuralNode processed = ApplyBevelModifiersToNode(node, bevelModifiers);
        if (processed.bevelRadius != node.bevelRadius || processed.bevelSegments != node.bevelSegments) {
            result.bevelModifiersApplied += 1;
        }
        processed = ApplyStructuralDetailModifiersToNode(processed, detailModifiers);
        if (processed.detailOnly != node.detailOnly || processed.isStructural != node.isStructural) {
            result.detailModifiersApplied += 1;
        }
        if (options.enableNonManifoldReconcile) {
            processed = ApplyNonManifoldReconcilersToNode(
                processed,
                reconcilers,
                options.enableHighCostNonManifoldFallback);
        }

        const std::optional<StructuralBooleanTarget> booleanTarget = CreateBooleanTargetFromNode(processed);
        if (booleanTarget.has_value()) {
            result.booleanTargets.push_back(*booleanTarget);
            continue;
        }

        result.passthroughNodes.push_back(processed);
    }

    for (const StructuralNode& aggregateNode : aggregateNodes) {
        AppendCompiledNodes(result.compiledNodes, CompileConvexHullAggregateNode(aggregateNode, result.booleanTargets));
    }

    std::vector<StructuralBooleanTarget> remainingTargets = result.booleanTargets;

    if (options.enableHighCostBooleanPasses) {
        for (const StructuralNode& unionNode : unionNodes) {
            const StructuralBooleanCompileResult unionResult = CompileBooleanUnionNode(unionNode, remainingTargets);
            AppendCompiledNodes(result.compiledNodes, unionResult.compiledNodes);
            ApplySuppressedTargetIds(remainingTargets, result.suppressedTargetIds, unionResult.targetIds);
        }
        for (const StructuralNode& intersectionNode : intersectionNodes) {
            const StructuralBooleanCompileResult intersectionResult =
                CompileBooleanIntersectionNode(intersectionNode, remainingTargets);
            AppendCompiledNodes(result.compiledNodes, intersectionResult.compiledNodes);
            ApplySuppressedTargetIds(remainingTargets, result.suppressedTargetIds, intersectionResult.targetIds);
        }
        for (const StructuralNode& differenceNode : differenceNodes) {
            const StructuralBooleanCompileResult differenceResult = CompileBooleanDifferenceNode(differenceNode, remainingTargets);
            AppendCompiledNodes(result.compiledNodes, differenceResult.compiledNodes);
            ApplySuppressedTargetIds(remainingTargets, result.suppressedTargetIds, differenceResult.targetIds);
        }
    }

    if (options.enableHighCostBooleanPasses && !cutterVolumes.empty()) {
        for (const StructuralBooleanTarget& target : remainingTargets) {
            std::vector<SolidFragment> fragments = BuildFragments(target.solids);
            bool touched = false;

            for (const StructuralCutterVolume& cutterVolume : cutterVolumes) {
                if (!cutterVolume.targetIds.empty()) {
                    const std::string& nodeId = target.node.id;
                    if (nodeId.empty() || !cutterVolume.targetIds.contains(nodeId)) {
                        continue;
                    }
                }

                for (const PreparedBooleanSolid& cutterSolid : cutterVolume.solids) {
                    std::vector<SolidFragment> nextFragments;
                    for (const SolidFragment& fragment : fragments) {
                        if (!BoundsIntersect(fragment.bounds, cutterSolid.bounds)) {
                            nextFragments.push_back(fragment);
                            continue;
                        }

                        touched = true;
                        const std::vector<ConvexSolid> resultSolids = cutterVolume.intersect
                            ? IntersectSolidWithConvexPlanes(fragment.solid, cutterSolid.planes)
                            : SubtractConvexPlanesFromSolid(fragment.solid, cutterSolid.planes);
                        for (const ConvexSolid& resultSolid : resultSolids) {
                            nextFragments.push_back(SolidFragment{
                                .solid = resultSolid,
                                .bounds = ComputeSolidBounds(resultSolid),
                            });
                        }
                    }
                    fragments = std::move(nextFragments);
                    if (fragments.empty()) {
                        break;
                    }
                }
                if (fragments.empty()) {
                    break;
                }
            }

            if (!touched) {
                result.passthroughNodes.push_back(target.node);
                continue;
            }

            if (!fragments.empty()) {
                std::vector<ConvexSolid> solids;
                solids.reserve(fragments.size());
                for (const SolidFragment& fragment : fragments) {
                    solids.push_back(fragment.solid);
                }
                const std::string prefix = !target.node.id.empty()
                    ? target.node.id
                    : ResolveStructuralPrimitiveType(target.node) + "_csg";
                AppendCompiledNodes(result.compiledNodes,
                                    BuildCompiledGeometryNodesFromSolids(target.node, solids, prefix));
            }
        }
        return result;
    }

    for (const StructuralBooleanTarget& target : remainingTargets) {
        result.passthroughNodes.push_back(target.node);
    }

    return result;
}

std::uint64_t BuildStructuralCompileSignature(const std::vector<StructuralNode>& nodes,
                                              const StructuralCompileOptions& options) {
    std::uint64_t signature = 0xcbf29ce484222325ULL;
    signature = HashCombine(signature, HashValue(nodes.size()));
    signature = HashCombine(signature, HashValue(options.enableHighCostBooleanPasses));
    signature = HashCombine(signature, HashValue(options.enableNonManifoldReconcile));
    signature = HashCombine(signature, HashValue(options.enableHighCostNonManifoldFallback));
    for (const StructuralNode& node : nodes) {
        signature = HashCombine(signature, HashValue(node.id));
        signature = HashCombine(signature, HashValue(node.type));
        signature = HashCombine(signature, HashValue(node.primitiveType));
        signature = HashCombine(signature, HashValue(node.opType));
        signature = HashCombine(signature, HashValue(node.targetIds.size()));
        signature = HashCombine(signature, HashValue(node.childNodeList.size()));
        signature = HashCombine(signature, HashValue(node.position.x));
        signature = HashCombine(signature, HashValue(node.position.y));
        signature = HashCombine(signature, HashValue(node.position.z));
        signature = HashCombine(signature, HashValue(node.rotation.x));
        signature = HashCombine(signature, HashValue(node.rotation.y));
        signature = HashCombine(signature, HashValue(node.rotation.z));
        signature = HashCombine(signature, HashValue(node.scale.x));
        signature = HashCombine(signature, HashValue(node.scale.y));
        signature = HashCombine(signature, HashValue(node.scale.z));
        signature = HashCombine(signature, HashValue(node.radialSegments));
        signature = HashCombine(signature, HashValue(node.segments));
        signature = HashCombine(signature, HashValue(node.steps));
        signature = HashCombine(signature, HashValue(node.sides));
        signature = HashCombine(signature, HashValue(node.detail));
        signature = HashCombine(signature, HashValue(node.cellsX));
        signature = HashCombine(signature, HashValue(node.cellsY));
        signature = HashCombine(signature, HashValue(node.cellsZ));
        signature = HashCombine(signature, HashValue(node.thickness));
        signature = HashCombine(signature, HashValue(node.depth));
        signature = HashCombine(signature, HashValue(node.strutRadius));
        signature = HashCombine(signature, HashValue(node.exponentX));
        signature = HashCombine(signature, HashValue(node.exponentY));
        signature = HashCombine(signature, HashValue(node.exponentZ));
        signature = HashCombine(signature, HashValue(node.spanDegrees));
        signature = HashCombine(signature, HashValue(node.sweepDegrees));
        signature = HashCombine(signature, HashValue(node.startDegrees));
        signature = HashCombine(signature, HashValue(node.length));
        signature = HashCombine(signature, HashValue(node.centerColumn));
        signature = HashCombine(signature, HashValue(node.latticeStyle));
        signature = HashCombine(signature, HashValue(node.points.size()));
        for (const ri::math::Vec3& point : node.points) {
            signature = HashCombine(signature, HashValue(point.x));
            signature = HashCombine(signature, HashValue(point.y));
            signature = HashCombine(signature, HashValue(point.z));
        }
    }
    return signature;
}

StructuralCompileIncrementalResult CompileStructuralGeometryNodesIncremental(
    const std::vector<StructuralNode>& nodes,
    const StructuralCompileOptions& options,
    const std::uint64_t previousSignature,
    const StructuralGeometryCompileResult* previousResult) {
    StructuralCompileIncrementalResult incremental{};
    incremental.signature = BuildStructuralCompileSignature(nodes, options);
    if (previousResult != nullptr && incremental.signature == previousSignature) {
        incremental.reusedPrevious = true;
        incremental.result = *previousResult;
        return incremental;
    }
    incremental.result = CompileStructuralGeometryNodes(nodes, options);
    return incremental;
}

} // namespace ri::structural
