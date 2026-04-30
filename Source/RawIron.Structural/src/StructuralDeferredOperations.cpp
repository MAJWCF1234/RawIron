#include "RawIron/Structural/StructuralDeferredOperations.h"
#include "RawIron/Structural/StructuralPrimitives.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <set>

namespace ri::structural {
namespace {

std::vector<std::string> ResolveDeferredTargetIds(const StructuralDeferredTargetOperation& operation) {
    const std::vector<std::string>* source = nullptr;
    if (!operation.targetIds.empty()) {
        source = &operation.targetIds;
    } else if (!operation.node.childNodeList.empty()) {
        source = &operation.node.childNodeList;
    } else {
        source = &operation.node.targetIds;
    }

    std::vector<std::string> ids;
    ids.reserve(source->size());
    for (const std::string& id : *source) {
        if (id.empty()) {
            continue;
        }
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    }
    return ids;
}

bool IsPointInsideConvexSolid(const ri::math::Vec3& point, const ConvexSolid& solid, float epsilon = 1e-4f) {
    for (const ConvexPolygon& polygon : solid.polygons) {
        if (DistanceToPlane(polygon.plane, point) > epsilon) {
            return false;
        }
    }
    return !solid.polygons.empty();
}

CompiledMesh BuildCompiledMeshFromTriangles(const std::vector<ri::math::Vec3>& positions,
                                            const std::vector<ri::math::Vec3>& normals) {
    CompiledMesh mesh{};
    if (positions.empty()) {
        return mesh;
    }

    const bool useProvidedNormals = normals.size() == positions.size();
    for (std::size_t index = 0; index + 2U < positions.size(); index += 3U) {
        const ri::math::Vec3& a = positions[index];
        const ri::math::Vec3& b = positions[index + 1U];
        const ri::math::Vec3& c = positions[index + 2U];
        ri::math::Vec3 safeNormal = {0.0f, 1.0f, 0.0f};
        if (!useProvidedNormals) {
            const ri::math::Vec3 faceNormal = ri::math::Normalize(ri::math::Cross(b - a, c - a));
            if (ri::math::LengthSquared(faceNormal) > 1e-12f) {
                safeNormal = faceNormal;
            }
        }

        for (std::size_t vertex = 0; vertex < 3U; ++vertex) {
            const ri::math::Vec3& position = positions[index + vertex];
            mesh.positions.push_back(position);
            mesh.normals.push_back(useProvidedNormals ? normals[index + vertex] : safeNormal);
            if (!mesh.hasBounds) {
                mesh.hasBounds = true;
                mesh.boundsMin = position;
                mesh.boundsMax = position;
            } else {
                mesh.boundsMin.x = std::min(mesh.boundsMin.x, position.x);
                mesh.boundsMin.y = std::min(mesh.boundsMin.y, position.y);
                mesh.boundsMin.z = std::min(mesh.boundsMin.z, position.z);
                mesh.boundsMax.x = std::max(mesh.boundsMax.x, position.x);
                mesh.boundsMax.y = std::max(mesh.boundsMax.y, position.y);
                mesh.boundsMax.z = std::max(mesh.boundsMax.z, position.z);
            }
        }

        mesh.triangleCount += 1U;
    }

    return mesh;
}

void AddUniqueId(std::vector<std::string>& ids, const std::string& id) {
    if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end()) {
        ids.push_back(id);
    }
}

void AppendCompiledNodes(std::vector<CompiledGeometryNode>& destination,
                         const std::vector<CompiledGeometryNode>& source) {
    destination.insert(destination.end(), source.begin(), source.end());
}

int ClampInteger(int value, int fallback, int minValue, int maxValue) {
    if (value < minValue || value > maxValue) {
        return fallback;
    }
    return value;
}

float SeededUnitValue(int seed) {
    const float value = std::sin((static_cast<float>(seed) * 127.1f) + 311.7f) * 43758.5453123f;
    return value - std::floor(value);
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
    if (node.hemisphereSegments > 0) {
        options.hemisphereSegments = node.hemisphereSegments;
    }
    if (node.thickness > 0.0f) {
        options.thickness = node.thickness;
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

ri::math::Vec3 GetSafeScale(const ri::math::Vec3& scale) {
    return {
        std::fabs(scale.x) > 1e-5f ? scale.x : 1.0f,
        std::fabs(scale.y) > 1e-5f ? scale.y : 1.0f,
        std::fabs(scale.z) > 1e-5f ? scale.z : 1.0f,
    };
}

ri::math::Mat4 CreateInverseNodeTransformMatrix(const StructuralNode& node) {
    const ri::math::Vec3 safeScale = GetSafeScale(node.scale);
    const ri::math::Mat4 inverseScale = ri::math::ScaleMatrix({1.0f / safeScale.x, 1.0f / safeScale.y, 1.0f / safeScale.z});
    const ri::math::Mat4 inverseRotation = ri::math::Multiply(
        ri::math::RotationXDegrees(-node.rotation.x),
        ri::math::Multiply(
            ri::math::RotationYDegrees(-node.rotation.y),
            ri::math::RotationZDegrees(-node.rotation.z)));
    const ri::math::Mat4 inverseTranslation = ri::math::TranslationMatrix(node.position * -1.0f);
    return ri::math::Multiply(inverseScale, ri::math::Multiply(inverseRotation, inverseTranslation));
}

ri::math::Mat4 CreateOrientationMatrixFromForward(const ri::math::Vec3& forwardVector) {
    ri::math::Vec3 forward = ri::math::Normalize(forwardVector);
    if (ri::math::LengthSquared(forward) <= 1e-8f) {
        forward = {0.0f, 0.0f, 1.0f};
    }

    ri::math::Vec3 up = {0.0f, 1.0f, 0.0f};
    if (std::fabs(ri::math::Dot(forward, up)) > 0.98f) {
        up = {1.0f, 0.0f, 0.0f};
    }

    const ri::math::Vec3 right = ri::math::Normalize(ri::math::Cross(up, forward));
    const ri::math::Vec3 orthogonalUp = ri::math::Normalize(ri::math::Cross(forward, right));

    ri::math::Mat4 matrix = ri::math::IdentityMatrix();
    matrix.m[0][0] = right.x;
    matrix.m[1][0] = right.y;
    matrix.m[2][0] = right.z;
    matrix.m[0][1] = orthogonalUp.x;
    matrix.m[1][1] = orthogonalUp.y;
    matrix.m[2][1] = orthogonalUp.z;
    matrix.m[0][2] = forward.x;
    matrix.m[1][2] = forward.y;
    matrix.m[2][2] = forward.z;
    return matrix;
}

CompiledMesh CreateLocalMeshFromNode(const CompiledGeometryNode& node) {
    if (node.compiledMesh.positions.empty()) {
        return {};
    }

    const ri::math::Mat4 inverse = CreateInverseNodeTransformMatrix(node.node);
    std::vector<ri::math::Vec3> positions;
    std::vector<ri::math::Vec3> normals;
    positions.reserve(node.compiledMesh.positions.size());
    normals.reserve(node.compiledMesh.normals.size());

    for (const ri::math::Vec3& position : node.compiledMesh.positions) {
        positions.push_back(ri::math::TransformPoint(inverse, position));
    }
    for (const ri::math::Vec3& normal : node.compiledMesh.normals) {
        normals.push_back(ri::math::Normalize(ri::math::TransformVector(inverse, normal)));
    }

    return BuildCompiledMeshFromTriangles(positions, normals);
}

ri::math::Vec3 SampleCatmullRomPoint(const std::vector<ri::math::Vec3>& points, float t) {
    if (points.empty()) {
        return {};
    }
    if (points.size() == 1U) {
        return points.front();
    }

    const float clampedT = std::clamp(t, 0.0f, 1.0f);
    const float scaled = clampedT * static_cast<float>(points.size() - 1U);
    const std::size_t segment = std::min(static_cast<std::size_t>(scaled), points.size() - 2U);
    const float localT = scaled - static_cast<float>(segment);

    const ri::math::Vec3& p0 = points[segment > 0U ? segment - 1U : segment];
    const ri::math::Vec3& p1 = points[segment];
    const ri::math::Vec3& p2 = points[segment + 1U];
    const ri::math::Vec3& p3 = points[(segment + 2U) < points.size() ? segment + 2U : segment + 1U];

    const ri::math::Vec3 m1 = (p2 - p0) * 0.25f;
    const ri::math::Vec3 m2 = (p3 - p1) * 0.25f;
    const float t2 = localT * localT;
    const float t3 = t2 * localT;

    return (p1 * ((2.0f * t3) - (3.0f * t2) + 1.0f))
        + (m1 * (t3 - (2.0f * t2) + localT))
        + (p2 * ((-2.0f * t3) + (3.0f * t2)))
        + (m2 * (t3 - t2));
}

ri::math::Vec3 SampleCatmullRomTangent(const std::vector<ri::math::Vec3>& points, float t) {
    if (points.size() < 2U) {
        return {0.0f, 0.0f, 1.0f};
    }

    const float previousT = std::max(0.0f, t - 0.01f);
    const float nextT = std::min(1.0f, t + 0.01f);
    const ri::math::Vec3 tangent = SampleCatmullRomPoint(points, nextT) - SampleCatmullRomPoint(points, previousT);
    if (ri::math::LengthSquared(tangent) <= 1e-8f) {
        return {0.0f, 0.0f, 1.0f};
    }
    return ri::math::Normalize(tangent);
}

float ApproximateSplineLength(const std::vector<ri::math::Vec3>& points) {
    if (points.size() < 2U) {
        return 0.0f;
    }

    float length = 0.0f;
    ri::math::Vec3 previous = points.front();
    constexpr int kSamples = 48;
    for (int sample = 1; sample <= kSamples; ++sample) {
        const float t = static_cast<float>(sample) / static_cast<float>(kSamples);
        const ri::math::Vec3 current = SampleCatmullRomPoint(points, t);
        length += ri::math::Distance(current, previous);
        previous = current;
    }
    return length;
}

ri::math::Vec3 ProjectRibbonPoint(const ri::math::Vec3& samplePoint,
                                  float projectionHeight,
                                  float projectionDistance,
                                  float offsetY,
                                  const std::vector<CompiledGeometryNode>& projectionNodes) {
    const float rayOriginY = samplePoint.y + projectionHeight;
    float bestY = -std::numeric_limits<float>::infinity();
    bool hit = false;

    for (const CompiledGeometryNode& node : projectionNodes) {
        if (!node.compiledMesh.hasBounds) {
            continue;
        }
        const bool insideXZ =
            samplePoint.x >= node.compiledMesh.boundsMin.x && samplePoint.x <= node.compiledMesh.boundsMax.x
            && samplePoint.z >= node.compiledMesh.boundsMin.z && samplePoint.z <= node.compiledMesh.boundsMax.z;
        if (!insideXZ) {
            continue;
        }

        const float topY = node.compiledMesh.boundsMax.y;
        const float distance = rayOriginY - topY;
        if (distance < 0.0f || distance > projectionDistance) {
            continue;
        }

        if (!hit || topY > bestY) {
            bestY = topY;
            hit = true;
        }
    }

    if (hit) {
        return {samplePoint.x, bestY + offsetY, samplePoint.z};
    }
    return {samplePoint.x, samplePoint.y + offsetY, samplePoint.z};
}

CompiledMesh BuildSplineRibbonMesh(const std::vector<ri::math::Vec3>& pathPoints, int steps, float width) {
    if (pathPoints.size() < 2U || steps < 1) {
        return {};
    }

    std::vector<ri::math::Vec3> positions;
    positions.reserve(static_cast<std::size_t>(steps) * 6U);
    std::vector<ri::math::Vec3> strip;
    strip.reserve((static_cast<std::size_t>(steps) + 1U) * 2U);
    const ri::math::Vec3 up{0.0f, 1.0f, 0.0f};

    for (int index = 0; index <= steps; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(steps);
        const ri::math::Vec3 point = SampleCatmullRomPoint(pathPoints, t);
        ri::math::Vec3 tangent = SampleCatmullRomTangent(pathPoints, t);
        ri::math::Vec3 lateral = ri::math::Cross(up, tangent);
        if (ri::math::LengthSquared(lateral) <= 1e-10f) {
            lateral = {1.0f, 0.0f, 0.0f};
        } else {
            lateral = ri::math::Normalize(lateral);
        }

        strip.push_back(point + (lateral * (width * 0.5f)));
        strip.push_back(point - (lateral * (width * 0.5f)));
    }

    for (int index = 0; index < steps; ++index) {
        const std::size_t base = static_cast<std::size_t>(index) * 2U;
        positions.push_back(strip[base]);
        positions.push_back(strip[base + 2U]);
        positions.push_back(strip[base + 1U]);
        positions.push_back(strip[base + 1U]);
        positions.push_back(strip[base + 2U]);
        positions.push_back(strip[base + 3U]);
    }

    return BuildCompiledMeshFromTriangles(positions, {});
}

std::vector<CompiledGeometryNode> CollectTargetNodes(const std::vector<CompiledGeometryNode>& nodes,
                                                     const std::set<std::string>& targetSet) {
    std::vector<CompiledGeometryNode> targets;
    for (const CompiledGeometryNode& node : nodes) {
        if (targetSet.contains(node.node.id)) {
            targets.push_back(node);
        }
    }
    return targets;
}

} // namespace

CompiledMesh ApplyTerrainHoleCutoutToMesh(const CompiledMesh& mesh, const StructuralNode& cutoutNode) {
    if (mesh.positions.size() < 3U) {
        return mesh;
    }

    const ConvexSolid cutoutSolid = CreateWorldSpaceBoxSolid(GetNodeTransformMatrix(cutoutNode));
    std::vector<ri::math::Vec3> keptPositions;
    std::vector<ri::math::Vec3> keptNormals;
    keptPositions.reserve(mesh.positions.size());
    keptNormals.reserve(mesh.normals.size());

    for (std::size_t index = 0; index + 2 < mesh.positions.size(); index += 3U) {
        const ri::math::Vec3& a = mesh.positions[index];
        const ri::math::Vec3& b = mesh.positions[index + 1U];
        const ri::math::Vec3& c = mesh.positions[index + 2U];
        const ri::math::Vec3 centroid = (a + b + c) / 3.0f;
        if (IsPointInsideConvexSolid(centroid, cutoutSolid)) {
            continue;
        }

        keptPositions.push_back(a);
        keptPositions.push_back(b);
        keptPositions.push_back(c);

        if (mesh.normals.size() == mesh.positions.size()) {
            keptNormals.push_back(mesh.normals[index]);
            keptNormals.push_back(mesh.normals[index + 1U]);
            keptNormals.push_back(mesh.normals[index + 2U]);
        } else {
            const ri::math::Vec3 faceNormal = ri::math::Normalize(ri::math::Cross(b - a, c - a));
            const ri::math::Vec3 safeNormal =
                ri::math::LengthSquared(faceNormal) > 1e-12f ? faceNormal : ri::math::Vec3{0.0f, 1.0f, 0.0f};
            keptNormals.push_back(safeNormal);
            keptNormals.push_back(safeNormal);
            keptNormals.push_back(safeNormal);
        }
    }

    return BuildCompiledMeshFromTriangles(keptPositions, keptNormals);
}

std::vector<CompiledGeometryNode> BuildSurfaceScatterCompiledNodes(
    const StructuralDeferredTargetOperation& operation,
    const std::vector<CompiledGeometryNode>& targetNodes) {
    if (targetNodes.empty()) {
        return {};
    }

    StructuralNode sourcePrimitive = operation.node.basePrimitive ? *operation.node.basePrimitive : StructuralNode{};
    if (sourcePrimitive.type.empty()) {
        sourcePrimitive.type = !operation.node.primitiveType.empty() ? operation.node.primitiveType : "box";
        sourcePrimitive.scale = {0.22f, 0.22f, 0.22f};
    }

    const std::string sourceType = ResolveStructuralPrimitiveType(sourcePrimitive);
    if (!IsNativeStructuralPrimitive(sourceType)) {
        return {};
    }

    const CompiledMesh sourceMesh = TransformCompiledMesh(
        BuildPrimitiveMesh(sourceType, BuildPrimitiveOptionsFromNode(sourcePrimitive)),
        ri::math::ScaleMatrix(GetSafeScale(sourcePrimitive.scale)));
    if (!sourceMesh.hasBounds) {
        return {};
    }

    const int count = ClampInteger(operation.node.count > 0 ? operation.node.count : 36, 36, 1, 512);
    std::vector<CompiledGeometryNode> generatedNodes;
    generatedNodes.reserve(static_cast<std::size_t>(count));

    for (int index = 0; index < count; ++index) {
        const CompiledGeometryNode& target = targetNodes[static_cast<std::size_t>(index) % targetNodes.size()];
        if (!target.compiledMesh.hasBounds) {
            continue;
        }

        const ri::math::Vec3 position{
            target.compiledMesh.boundsMin.x + ((target.compiledMesh.boundsMax.x - target.compiledMesh.boundsMin.x) * SeededUnitValue(index + 1)),
            target.compiledMesh.boundsMax.y,
            target.compiledMesh.boundsMin.z + ((target.compiledMesh.boundsMax.z - target.compiledMesh.boundsMin.z) * SeededUnitValue(index + 101)),
        };
        const float uniformScale = 0.65f + (0.7f * SeededUnitValue(index + 203));

        CompiledGeometryNode generated{};
        generated.node = sourcePrimitive;
        generated.node.id = !operation.node.id.empty()
            ? operation.node.id + "_" + std::to_string(index + 1)
            : "surface_scatter_" + std::to_string(index + 1);
        generated.node.name = generated.node.id;
        generated.node.type = sourceType;
        generated.node.primitiveType.clear();
        generated.node.position = position;
        generated.node.scale = GetSafeScale(sourcePrimitive.scale) * uniformScale;
        generated.compiledMesh = TransformCompiledMesh(sourceMesh, ri::math::TRS(position, {}, {uniformScale, uniformScale, uniformScale}));
        generated.compiledWorldSpace = true;
        generated.compiledFromStructuralCsg = false;
        generatedNodes.push_back(std::move(generated));
    }

    return generatedNodes;
}

std::vector<CompiledGeometryNode> BuildSplineMeshDeformerCompiledNodes(
    const StructuralDeferredTargetOperation& operation,
    const std::vector<CompiledGeometryNode>& targetNodes) {
    if (targetNodes.empty() || operation.node.points.size() < 2U) {
        return {};
    }

    const float approximateLength = ApproximateSplineLength(operation.node.points);
    const int derivedCount = std::max(2, static_cast<int>(std::round(approximateLength / 2.0f)));
    const int count = ClampInteger(operation.node.count > 0 ? operation.node.count : derivedCount, 6, 2, 128);
    std::vector<CompiledGeometryNode> generatedNodes;
    generatedNodes.reserve(targetNodes.size() * static_cast<std::size_t>(count));

    for (std::size_t targetIndex = 0; targetIndex < targetNodes.size(); ++targetIndex) {
        const CompiledGeometryNode& target = targetNodes[targetIndex];
        const CompiledMesh localMesh = CreateLocalMeshFromNode(target);
        if (!localMesh.hasBounds) {
            continue;
        }

        const ri::math::Mat4 sourceRotation = ri::math::RotationXYZDegrees(target.node.rotation);
        const ri::math::Mat4 sourceScale = ri::math::ScaleMatrix(GetSafeScale(target.node.scale));
        for (int index = 0; index < count; ++index) {
            const float t = count <= 1 ? 0.0f : static_cast<float>(index) / static_cast<float>(count - 1);
            const ri::math::Vec3 point = SampleCatmullRomPoint(operation.node.points, t);
            const ri::math::Vec3 tangent = SampleCatmullRomTangent(operation.node.points, t);
            const ri::math::Mat4 transform = ri::math::Multiply(
                ri::math::TranslationMatrix(point),
                ri::math::Multiply(CreateOrientationMatrixFromForward(tangent), ri::math::Multiply(sourceRotation, sourceScale)));

            CompiledGeometryNode generated{};
            generated.node = target.node;
            generated.node.id = !operation.node.id.empty()
                ? operation.node.id + "_" + std::to_string(targetIndex + 1U) + "_" + std::to_string(index + 1)
                : target.node.id + "_spline_" + std::to_string(index + 1);
            generated.node.name = generated.node.id;
            generated.node.position = point;
            generated.compiledMesh = TransformCompiledMesh(localMesh, transform);
            generated.compiledWorldSpace = true;
            generated.compiledFromStructuralCsg = false;
            generatedNodes.push_back(std::move(generated));
        }
    }

    return generatedNodes;
}

std::optional<CompiledGeometryNode> BuildSplineDecalRibbonCompiledNode(
    const StructuralDeferredTargetOperation& operation,
    const std::vector<CompiledGeometryNode>& projectionNodes) {
    if (operation.node.points.size() < 2U) {
        return std::nullopt;
    }

    const int steps = ClampInteger(operation.node.segments > 0 ? operation.node.segments : 32, 32, 4, 256);
    const float width = operation.node.width > 0.0f ? operation.node.width : 0.4f;
    const float projectionHeight = operation.node.projectionHeight > 0.0f ? operation.node.projectionHeight : 40.0f;
    const float projectionDistance = operation.node.projectionDistance > 0.0f ? operation.node.projectionDistance : 120.0f;
    const float offsetY = operation.node.offsetY != 0.0f ? operation.node.offsetY : 0.03f;

    std::vector<ri::math::Vec3> projectedPath;
    projectedPath.reserve(static_cast<std::size_t>(steps) + 1U);
    for (int index = 0; index <= steps; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(steps);
        const ri::math::Vec3 samplePoint = SampleCatmullRomPoint(operation.node.points, t);
        projectedPath.push_back(ProjectRibbonPoint(
            samplePoint,
            projectionHeight,
            projectionDistance,
            offsetY,
            projectionNodes));
    }

    const CompiledMesh mesh = BuildSplineRibbonMesh(projectedPath, steps, width);
    if (!mesh.hasBounds) {
        return std::nullopt;
    }

    CompiledGeometryNode generated{};
    generated.node = operation.node;
    generated.node.id = !operation.node.id.empty() ? operation.node.id : "spline_decal";
    generated.node.name = generated.node.id;
    generated.compiledMesh = mesh;
    generated.compiledWorldSpace = true;
    generated.compiledFromStructuralCsg = false;
    return generated;
}

std::optional<CompiledGeometryNode> BuildShrinkwrapCompiledNode(
    const StructuralNode& shrinkwrapNode,
    const std::vector<CompiledGeometryNode>& targetNodes) {
    std::vector<ri::math::Vec3> points;
    for (const CompiledGeometryNode& targetNode : targetNodes) {
        points.insert(points.end(), targetNode.compiledMesh.positions.begin(), targetNode.compiledMesh.positions.end());
    }

    if (points.size() < 4U) {
        return std::nullopt;
    }

    StructuralPrimitiveOptions options{};
    options.points = std::move(points);
    const std::optional<ConvexSolid> hull = CreateConvexPrimitiveSolid("convex_hull", options);
    if (!hull.has_value()) {
        return std::nullopt;
    }

    CompiledGeometryNode compiled{};
    compiled.node = shrinkwrapNode;
    compiled.node.type = "convex_hull";
    compiled.node.primitiveType.clear();
    compiled.compiledMesh = BuildCompiledMeshFromConvexSolid(*hull);
    compiled.compiledWorldSpace = true;
    compiled.compiledFromStructuralCsg = false;
    return compiled;
}

std::vector<CompiledGeometryNode> BuildConvexSubdivisionCompiledNodes(
    const StructuralDeferredTargetOperation& operation,
    const std::vector<CompiledGeometryNode>& targetNodes) {
    std::vector<CompiledGeometryNode> generatedNodes;
    generatedNodes.reserve(targetNodes.size());

    std::size_t generatedIndex = 0U;
    for (const CompiledGeometryNode& targetNode : targetNodes) {
        if (targetNode.compiledMesh.positions.size() < 4U) {
            continue;
        }
        const std::optional<CompiledGeometryNode> hull = BuildShrinkwrapCompiledNode(operation.node, {targetNode});
        if (!hull.has_value()) {
            continue;
        }

        CompiledGeometryNode generated = *hull;
        generated.node.id = !operation.node.id.empty()
            ? operation.node.id + "_convex_" + std::to_string(++generatedIndex)
            : targetNode.node.id + "_convex";
        generated.node.name = generated.node.id;
        generatedNodes.push_back(std::move(generated));
    }

    return generatedNodes;
}

StructuralDeferredExecutionResult ExecuteStructuralDeferredTargetOperations(
    const std::vector<StructuralDeferredTargetOperation>& operations,
    const std::vector<CompiledGeometryNode>& compiledNodes) {
    StructuralDeferredExecutionResult result{};
    result.nodes = compiledNodes;

    for (const StructuralDeferredTargetOperation& operation : operations) {
        const std::vector<std::string> targetIds = ResolveDeferredTargetIds(operation);
        const std::set<std::string> targetSet(targetIds.begin(), targetIds.end());
        const std::string& operationType = operation.normalizedType;
        const std::size_t modifiedBefore = result.modifiedTargetIds.size();
        const std::size_t replacedBefore = result.replacedTargetIds.size();
        const std::size_t nodesBefore = result.nodes.size();
        StructuralDeferredOperationStats stats{};
        stats.operationId = !operation.node.id.empty() ? operation.node.id : operationType;
        stats.normalizedType = operationType;
        stats.targetCount = targetIds.size();

        const auto finalizeStats = [&](bool succeeded, bool skippedNoTargets, bool skippedUnsupportedType) {
            stats.inputNodeCount = nodesBefore;
            stats.generatedNodeCount = result.nodes.size() > nodesBefore ? (result.nodes.size() - nodesBefore) : 0U;
            stats.modifiedCount = result.modifiedTargetIds.size() - modifiedBefore;
            stats.replacedCount = result.replacedTargetIds.size() - replacedBefore;
            stats.succeeded = succeeded;
            stats.skippedNoTargets = skippedNoTargets;
            stats.skippedUnsupportedType = skippedUnsupportedType;
            result.operationStats.push_back(stats);
        };

        if (operationType == "spline_decal_ribbon") {
            const std::vector<CompiledGeometryNode> projectionNodes =
                targetSet.empty() ? result.nodes : CollectTargetNodes(result.nodes, targetSet);
            const std::optional<CompiledGeometryNode> generated =
                BuildSplineDecalRibbonCompiledNode(operation, projectionNodes);
            if (generated.has_value()) {
                result.nodes.push_back(*generated);
            }
            finalizeStats(generated.has_value(), false, false);
            continue;
        }

        if (targetSet.empty()) {
            finalizeStats(false, true, false);
            continue;
        }

        if (operationType == "terrain_hole_cutout") {
            for (CompiledGeometryNode& node : result.nodes) {
                if (!targetSet.contains(node.node.id)) {
                    continue;
                }

                const CompiledMesh cutMesh = ApplyTerrainHoleCutoutToMesh(node.compiledMesh, operation.node);
                if (cutMesh.triangleCount != node.compiledMesh.triangleCount) {
                    node.compiledMesh = cutMesh;
                    AddUniqueId(result.modifiedTargetIds, node.node.id);
                }
            }
            finalizeStats(true, false, false);
            continue;
        }

        if (operationType == "surface_scatter_volume") {
            const std::vector<CompiledGeometryNode> targets = CollectTargetNodes(result.nodes, targetSet);
            const std::vector<CompiledGeometryNode> generated = BuildSurfaceScatterCompiledNodes(operation, targets);
            if (!generated.empty()) {
                for (const std::string& id : targetIds) {
                    AddUniqueId(result.modifiedTargetIds, id);
                }
            }
            AppendCompiledNodes(result.nodes, generated);
            finalizeStats(!generated.empty(), false, false);
            continue;
        }

        if (operationType == "spline_mesh_deformer") {
            const std::vector<CompiledGeometryNode> targets = CollectTargetNodes(result.nodes, targetSet);
            const std::vector<CompiledGeometryNode> generated = BuildSplineMeshDeformerCompiledNodes(operation, targets);
            if (!generated.empty()) {
                for (const std::string& id : targetIds) {
                    AddUniqueId(result.modifiedTargetIds, id);
                }
            }
            if (!generated.empty() && !operation.node.keepSource) {
                result.nodes.erase(std::remove_if(result.nodes.begin(),
                                                  result.nodes.end(),
                                                  [&](const CompiledGeometryNode& node) {
                                                      const bool matched = targetSet.contains(node.node.id);
                                                      if (matched) {
                                                          AddUniqueId(result.replacedTargetIds, node.node.id);
                                                      }
                                                      return matched;
                                                  }),
                                   result.nodes.end());
            }
            AppendCompiledNodes(result.nodes, generated);
            finalizeStats(!generated.empty(), false, false);
            continue;
        }

        if (operationType == "automatic_convex_subdivision_modifier") {
            const std::vector<CompiledGeometryNode> targets = CollectTargetNodes(result.nodes, targetSet);
            const std::vector<CompiledGeometryNode> generated = BuildConvexSubdivisionCompiledNodes(operation, targets);
            if (!generated.empty()) {
                for (const std::string& id : targetIds) {
                    AddUniqueId(result.modifiedTargetIds, id);
                }
            }

            if (!generated.empty() && operation.node.replaceChildColliders) {
                result.nodes.erase(std::remove_if(result.nodes.begin(),
                                                  result.nodes.end(),
                                                  [&](const CompiledGeometryNode& node) {
                                                      const bool matched = targetSet.contains(node.node.id);
                                                      if (matched) {
                                                          AddUniqueId(result.replacedTargetIds, node.node.id);
                                                      }
                                                      return matched;
                                                  }),
                                   result.nodes.end());
            }
            AppendCompiledNodes(result.nodes, generated);
            finalizeStats(!generated.empty(), false, false);
            continue;
        }

        if (operationType == "shrinkwrap_modifier_primitive"
            || operationType == "fillet_boolean_modifier"
            || operationType == "sdf_blend_node"
            || operationType == "sdf_intersection_node") {
            const std::vector<CompiledGeometryNode> targets = CollectTargetNodes(result.nodes, targetSet);
            const std::optional<CompiledGeometryNode> generated = BuildShrinkwrapCompiledNode(operation.node, targets);
            if (!generated.has_value()) {
                finalizeStats(false, false, false);
                continue;
            }
            for (const std::string& id : targetIds) {
                AddUniqueId(result.modifiedTargetIds, id);
            }

            if (operation.node.replaceChildColliders) {
                result.nodes.erase(std::remove_if(result.nodes.begin(),
                                                  result.nodes.end(),
                                                  [&](const CompiledGeometryNode& node) {
                                                      const bool matched = targetSet.contains(node.node.id);
                                                      if (matched) {
                                                          AddUniqueId(result.replacedTargetIds, node.node.id);
                                                      }
                                                      return matched;
                                                  }),
                                   result.nodes.end());
            }
            result.nodes.push_back(*generated);
            finalizeStats(true, false, false);
            continue;
        }

        finalizeStats(false, false, true);
    }

    return result;
}

StructuralDeferredPipelineResult ExecuteStructuralDeferredPipeline(
    const StructuralGeometryCompileResult& compileResult) {
    StructuralDeferredPipelineResult result{};
    result.compileResult = compileResult;

    std::vector<CompiledGeometryNode> deferredInputNodes = compileResult.compiledNodes;
    for (const StructuralBooleanTarget& target : compileResult.booleanTargets) {
        const std::string prefix = !target.node.id.empty() ? target.node.id : target.node.type;
        std::vector<CompiledGeometryNode> generated =
            BuildCompiledGeometryNodesFromSolids(target.node, target.solids, prefix);
        for (CompiledGeometryNode& node : generated) {
            if (!target.node.id.empty()) {
                node.node.id = target.node.id;
                node.node.name = target.node.name;
            }
        }
        AppendCompiledNodes(deferredInputNodes, generated);
    }

    result.deferredExecution = ExecuteStructuralDeferredTargetOperations(
        compileResult.deferredOperations,
        deferredInputNodes);
    result.nodes = result.deferredExecution.nodes;

    for (const StructuralDeferredOperationStats& stats : result.deferredExecution.operationStats) {
        if (!stats.skippedUnsupportedType) {
            continue;
        }
        AddUniqueId(result.unsupportedOperationIds, stats.operationId);
    }

    return result;
}

StructuralDeferredPipelineResult CompileAndExecuteStructuralDeferredPipeline(
    const std::vector<StructuralNode>& nodes) {
    return ExecuteStructuralDeferredPipeline(CompileStructuralGeometryNodes(nodes));
}

std::string BuildStructuralDeferredPipelineReport(
    const StructuralDeferredPipelineResult& result) {
    std::ostringstream stream;
    stream << "Structural deferred pipeline report\n";
    stream << "  compiled_nodes=" << result.compileResult.compiledNodes.size()
           << " boolean_targets=" << result.compileResult.booleanTargets.size()
           << " deferred_operations=" << result.compileResult.deferredOperations.size()
           << " final_nodes=" << result.nodes.size() << '\n';
    stream << "  modified_targets=" << result.deferredExecution.modifiedTargetIds.size()
           << " replaced_targets=" << result.deferredExecution.replacedTargetIds.size()
           << " unsupported_operations=" << result.unsupportedOperationIds.size() << '\n';

    stream << "  operations:\n";
    if (result.deferredExecution.operationStats.empty()) {
        stream << "    (none)\n";
    } else {
        for (const StructuralDeferredOperationStats& stats : result.deferredExecution.operationStats) {
            stream << "    - id=" << stats.operationId
                   << " type=" << stats.normalizedType
                   << " targets=" << stats.targetCount
                   << " generated=" << stats.generatedNodeCount
                   << " modified=" << stats.modifiedCount
                   << " replaced=" << stats.replacedCount
                   << " status=";
            if (stats.succeeded) {
                stream << "ok";
            } else if (stats.skippedNoTargets) {
                stream << "skipped_no_targets";
            } else if (stats.skippedUnsupportedType) {
                stream << "skipped_unsupported";
            } else {
                stream << "failed_or_no_output";
            }
            stream << '\n';
        }
    }

    if (!result.unsupportedOperationIds.empty()) {
        stream << "  unsupported_ids=";
        for (std::size_t index = 0; index < result.unsupportedOperationIds.size(); ++index) {
            if (index > 0U) {
                stream << ",";
            }
            stream << result.unsupportedOperationIds[index];
        }
        stream << '\n';
    }

    return stream.str();
}

} // namespace ri::structural
