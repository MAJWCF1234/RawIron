#pragma once

#include "RawIron/Math/Mat4.h"
#include "RawIron/Structural/ConvexClipper.h"
#include "RawIron/Structural/StructuralGraph.h"

#include <optional>
#include <string_view>
#include <vector>

namespace ri::structural {

struct Bounds {
    ri::math::Vec3 min{};
    ri::math::Vec3 max{};
};

struct Triangle {
    ri::math::Vec3 a{};
    ri::math::Vec3 b{};
    ri::math::Vec3 c{};
};

struct CompiledGeometryNode {
    StructuralNode node;
    CompiledMesh compiledMesh;
    bool compiledWorldSpace = true;
    bool compiledFromStructuralCsg = true;
};

struct StructuralBooleanTarget {
    StructuralNode node;
    std::vector<ConvexSolid> solids;
};

struct StructuralBooleanCompileResult {
    std::vector<CompiledGeometryNode> compiledNodes;
    std::vector<std::string> targetIds;
};

struct StructuralDeferredTargetOperation {
    StructuralNode node;
    std::string normalizedType;
    std::vector<std::string> targetIds;
};

struct StructuralGeometryCompileResult {
    std::vector<StructuralNode> expandedNodes;
    std::vector<StructuralNode> passthroughNodes;
    std::vector<StructuralBooleanTarget> booleanTargets;
    std::vector<StructuralDeferredTargetOperation> deferredOperations;
    std::vector<CompiledGeometryNode> compiledNodes;
    std::vector<std::string> suppressedTargetIds;
};

struct StructuralCompileOptions {
    bool enableHighCostBooleanPasses = true;
    bool enableNonManifoldReconcile = true;
    bool enableHighCostNonManifoldFallback = false;
};

[[nodiscard]] std::optional<Bounds> ComputeSolidBounds(const ConvexSolid& solid);
[[nodiscard]] ConvexSolid TransformSolid(const ConvexSolid& solid, const ri::math::Mat4& matrix);
[[nodiscard]] ConvexSolid CreateWorldSpaceBoxSolid(const ri::math::Mat4& matrix,
                                                   const ri::math::Vec3& min = {-0.5f, -0.5f, -0.5f},
                                                   const ri::math::Vec3& max = {0.5f, 0.5f, 0.5f});
[[nodiscard]] ri::math::Mat4 GetNodeTransformMatrix(const StructuralNode& node);
[[nodiscard]] ri::math::Mat4 GetOffsetStepMatrix(const StructuralNode& node);
[[nodiscard]] ri::math::Vec3 MirrorAxisVector(std::string_view axis,
                                              float origin,
                                              const ri::math::Vec3& position);
[[nodiscard]] ri::math::Vec3 MirrorAxisRotation(std::string_view axis,
                                                const ri::math::Vec3& rotationDegrees);
[[nodiscard]] StructuralNode CreateMirroredStructuralNode(const StructuralNode& baseNode,
                                                          const StructuralNode& mirrorNode,
                                                          std::size_t index);
[[nodiscard]] std::vector<StructuralNode> ExpandSymmetryMirrorNodes(const std::vector<StructuralNode>& nodes);
[[nodiscard]] StructuralNode CreateTransformedArrayPrimitiveNode(const StructuralNode& baseNode,
                                                                 const ri::math::Mat4& transformMatrix,
                                                                 const StructuralNode& arrayNode,
                                                                 std::size_t index);
[[nodiscard]] std::vector<StructuralNode> ExpandArrayPrimitiveNodes(const std::vector<StructuralNode>& nodes);
[[nodiscard]] std::optional<Bounds> CreateGeometryBoundsForNode(const StructuralNode& node);
[[nodiscard]] StructuralNode ApplyBevelModifiersToNode(const StructuralNode& node,
                                                       const std::vector<StructuralNode>& bevelModifiers);
[[nodiscard]] StructuralNode ApplyStructuralDetailModifiersToNode(const StructuralNode& node,
                                                                  const std::vector<StructuralNode>& detailModifiers);
[[nodiscard]] StructuralNode ApplyNonManifoldReconcilersToNode(const StructuralNode& node,
                                                               const std::vector<StructuralNode>& reconcilers,
                                                               bool allowHighCostHullFallback = false);
[[nodiscard]] std::vector<std::string> GetBooleanOperatorTargetIds(const StructuralNode& node);
[[nodiscard]] bool SupportsBooleanAdditiveTarget(const StructuralBooleanTarget& target);
[[nodiscard]] bool SupportsBooleanAdditiveTargetNode(const StructuralNode& node);
[[nodiscard]] std::vector<ConvexSolid> BuildBooleanAdditiveSolidsFromNode(const StructuralNode& node);
[[nodiscard]] std::vector<Plane> ExtractConvexPlanesFromTriangles(const std::vector<Triangle>& triangles,
                                                                  const ri::math::Mat4& transform = ri::math::IdentityMatrix(),
                                                                  float epsilon = 1e-4f);
[[nodiscard]] std::vector<ConvexSolid> SubtractConvexPlanesFromSolid(const ConvexSolid& solid,
                                                                     const std::vector<Plane>& planes,
                                                                     float epsilon = 1e-5f);
[[nodiscard]] std::vector<ConvexSolid> IntersectSolidWithConvexPlanes(const ConvexSolid& solid,
                                                                      const std::vector<Plane>& planes,
                                                                      float epsilon = 1e-5f);
[[nodiscard]] std::vector<CompiledGeometryNode> BuildCompiledGeometryNodesFromSolids(const StructuralNode& baseNode,
                                                                                      const std::vector<ConvexSolid>& solids,
                                                                                      std::string_view idPrefix);
[[nodiscard]] StructuralBooleanCompileResult CompileBooleanUnionNode(const StructuralNode& unionNode,
                                                                     const std::vector<StructuralBooleanTarget>& targets);
[[nodiscard]] StructuralBooleanCompileResult CompileBooleanIntersectionNode(const StructuralNode& intersectionNode,
                                                                            const std::vector<StructuralBooleanTarget>& targets);
[[nodiscard]] StructuralBooleanCompileResult CompileBooleanDifferenceNode(const StructuralNode& differenceNode,
                                                                          const std::vector<StructuralBooleanTarget>& targets);
[[nodiscard]] std::vector<CompiledGeometryNode> CompileConvexHullAggregateNode(const StructuralNode& aggregateNode,
                                                                               const std::vector<StructuralBooleanTarget>& targets);
[[nodiscard]] StructuralGeometryCompileResult CompileStructuralGeometryNodes(
    const std::vector<StructuralNode>& nodes,
    const StructuralCompileOptions& options = {});

} // namespace ri::structural
