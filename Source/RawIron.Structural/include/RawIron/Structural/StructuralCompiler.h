#pragma once

#include "RawIron/Math/Mat4.h"
#include "RawIron/Structural/ConvexClipper.h"
#include "RawIron/Structural/StructuralGraph.h"

#include <optional>
#include <cstdint>
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
    /// Which authored structural template this mesh came from (`primitiveType` if set, else `type`).
    std::string resolvedPrimitiveType;
    /// Index of this convex fragment within the batch emitted for one compile step (0-based).
    std::size_t convexFragmentIndex = 0;
    /// Total convex fragments in that batch (typically matches solid count passed to `BuildCompiledGeometryNodesFromSolids`).
    std::size_t convexFragmentCount = 1;
    /// Stable key for editors and telemetry: `id` if set, else `name`, else resolved template string (before `_fragment_N` ids).
    std::string authoringSourceKey;
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
    /// Non-fatal compile notes (e.g. CSG removed all geometry for a target).
    std::vector<std::string> compileWarnings;
    /// Sum of `CompiledMesh::triangleCount` across `compiledNodes` after compile completes.
    std::size_t totalCompiledTriangles = 0;
    /// Axis-aligned union of `compiledNodes` mesh bounds when every contributor had `CompiledMesh::hasBounds`.
    std::optional<Bounds> compiledGeometryWorldBounds;
    /// Count of structural nodes passed into `CompileStructuralGeometryNodes`.
    std::size_t inputStructuralNodeCount = 0;
    /// Count after array/symmetry expansion (`expandedNodes.size()`).
    std::size_t expandedStructuralNodeCount = 0;
    std::size_t bevelModifiersApplied = 0;
    std::size_t detailModifiersApplied = 0;
};

struct StructuralCompileOptions {
    bool enableHighCostBooleanPasses = true;
    bool enableNonManifoldReconcile = true;
    bool enableHighCostNonManifoldFallback = false;
    /// Half-space epsilon for convex CSG clipping (`SubtractConvexPlanesFromSolid` / intersect). Must be positive;
    /// non-finite or non-positive values fall back to `1e-5`. Clamped to roughly `[1e-8, 1e-2]` so authors can tighten
    /// joints vs ancient fixed eps without destabilizing the clipper.
    float csgPlaneEpsilon = 1e-5f;
    /// When true, append `compileWarnings` for compiled fragments with no triangles or missing bounds.
    bool reportDegenerateConvexFragments = true;
    /// When true, warn when a subtractive/intersect cutter lists `targetIds` that do not match any non-empty `id` on the expanded structural graph.
    bool validateCutterTargetReferences = true;
};

struct StructuralCompileIncrementalResult {
    std::uint64_t signature = 0;
    bool reusedPrevious = false;
    StructuralGeometryCompileResult result;
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
/// Canonical primitive template string for a node (`primitiveType` wins over `type`), used by compile and tooling.
[[nodiscard]] std::string ResolveStructuralPrimitiveType(const StructuralNode& node);
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
/// Authoring-time CSG output: compiled triangle fragments for convex solids (structural build path).
[[nodiscard]] inline std::vector<CompiledGeometryNode> EmitCompiledConvexFragments(const StructuralNode& baseNode,
                                                                                   const std::vector<ConvexSolid>& solids,
                                                                                   std::string_view idPrefix) {
    return BuildCompiledGeometryNodesFromSolids(baseNode, solids, idPrefix);
}
[[nodiscard]] StructuralBooleanCompileResult CompileBooleanUnionNode(const StructuralNode& unionNode,
                                                                     const std::vector<StructuralBooleanTarget>& targets,
                                                                     float csgPlaneEpsilon = 1e-5f);
[[nodiscard]] StructuralBooleanCompileResult CompileBooleanIntersectionNode(const StructuralNode& intersectionNode,
                                                                            const std::vector<StructuralBooleanTarget>& targets,
                                                                            float csgPlaneEpsilon = 1e-5f);
[[nodiscard]] StructuralBooleanCompileResult CompileBooleanDifferenceNode(const StructuralNode& differenceNode,
                                                                          const std::vector<StructuralBooleanTarget>& targets,
                                                                          float csgPlaneEpsilon = 1e-5f);
[[nodiscard]] std::vector<CompiledGeometryNode> CompileConvexHullAggregateNode(const StructuralNode& aggregateNode,
                                                                               const std::vector<StructuralBooleanTarget>& targets);
[[nodiscard]] StructuralGeometryCompileResult CompileStructuralGeometryNodes(
    const std::vector<StructuralNode>& nodes,
    const StructuralCompileOptions& options = {});
[[nodiscard]] std::uint64_t BuildStructuralCompileSignature(const std::vector<StructuralNode>& nodes,
                                                            const StructuralCompileOptions& options = {});
[[nodiscard]] StructuralCompileIncrementalResult CompileStructuralGeometryNodesIncremental(
    const std::vector<StructuralNode>& nodes,
    const StructuralCompileOptions& options,
    std::uint64_t previousSignature,
    const StructuralGeometryCompileResult* previousResult = nullptr);

} // namespace ri::structural
