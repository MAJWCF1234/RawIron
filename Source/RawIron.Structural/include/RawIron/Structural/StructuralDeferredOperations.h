#pragma once

#include "RawIron/Structural/StructuralCompiler.h"

#include <optional>
#include <string>
#include <vector>

namespace ri::structural {

struct StructuralDeferredOperationStats {
    std::string operationId;
    std::string normalizedType;
    std::size_t targetCount = 0U;
    std::size_t inputNodeCount = 0U;
    std::size_t generatedNodeCount = 0U;
    std::size_t modifiedCount = 0U;
    std::size_t replacedCount = 0U;
    bool skippedNoTargets = false;
    bool skippedUnsupportedType = false;
    bool succeeded = false;
};

struct StructuralDeferredExecutionResult {
    std::vector<CompiledGeometryNode> nodes;
    std::vector<std::string> modifiedTargetIds;
    std::vector<std::string> replacedTargetIds;
    std::vector<StructuralDeferredOperationStats> operationStats;
};

struct StructuralDeferredPipelineResult {
    std::vector<CompiledGeometryNode> nodes;
    StructuralGeometryCompileResult compileResult;
    StructuralDeferredExecutionResult deferredExecution;
    std::vector<std::string> unsupportedOperationIds;
};

[[nodiscard]] CompiledMesh ApplyTerrainHoleCutoutToMesh(const CompiledMesh& mesh,
                                                        const StructuralNode& cutoutNode);
[[nodiscard]] std::vector<CompiledGeometryNode> BuildSurfaceScatterCompiledNodes(
    const StructuralDeferredTargetOperation& operation,
    const std::vector<CompiledGeometryNode>& targetNodes);
[[nodiscard]] std::vector<CompiledGeometryNode> BuildSplineMeshDeformerCompiledNodes(
    const StructuralDeferredTargetOperation& operation,
    const std::vector<CompiledGeometryNode>& targetNodes);
[[nodiscard]] std::optional<CompiledGeometryNode> BuildSplineDecalRibbonCompiledNode(
    const StructuralDeferredTargetOperation& operation,
    const std::vector<CompiledGeometryNode>& projectionNodes);
[[nodiscard]] std::optional<CompiledGeometryNode> BuildShrinkwrapCompiledNode(
    const StructuralNode& shrinkwrapNode,
    const std::vector<CompiledGeometryNode>& targetNodes);
[[nodiscard]] StructuralDeferredExecutionResult ExecuteStructuralDeferredTargetOperations(
    const std::vector<StructuralDeferredTargetOperation>& operations,
    const std::vector<CompiledGeometryNode>& compiledNodes);
[[nodiscard]] StructuralDeferredPipelineResult ExecuteStructuralDeferredPipeline(
    const StructuralGeometryCompileResult& compileResult);
[[nodiscard]] StructuralDeferredPipelineResult CompileAndExecuteStructuralDeferredPipeline(
    const std::vector<StructuralNode>& nodes);
[[nodiscard]] std::string BuildStructuralDeferredPipelineReport(
    const StructuralDeferredPipelineResult& result);

} // namespace ri::structural
