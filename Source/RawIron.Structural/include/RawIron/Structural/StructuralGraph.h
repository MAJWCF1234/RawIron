#pragma once

#include "RawIron/Math/Vec3.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ri::structural {

enum class StructuralPhase {
    Compile = 0,
    Runtime = 1,
    PostBuild = 2,
    Frame = 3,
};

struct StructuralPhaseBuckets {
    std::size_t compile = 0;
    std::size_t runtime = 0;
    std::size_t postBuild = 0;
    std::size_t frame = 0;
};

struct StructuralDependencyIssue {
    std::string nodeId;
    std::string dependencyId;
};

struct StructuralNode {
    std::string id;
    std::string name;
    std::string type;
    std::string primitiveType;
    std::string opType;
    StructuralPhase phase = StructuralPhase::Compile;
    std::vector<std::string> targetIds;
    std::vector<std::string> childNodeList;
    std::vector<std::string> targetAIds;
    std::vector<std::string> targetBIds;
    std::string pivotAnchorId;
    std::string anchorId;
    ri::math::Vec3 position{};
    ri::math::Vec3 rotation{};
    ri::math::Vec3 scale{1.0f, 1.0f, 1.0f};
    bool isStructural = true;
    bool detailOnly = false;
    bool excludeFromVisibility = false;
    bool excludeFromNavigation = false;
    bool reconciledNonManifold = false;
    bool forceHull = false;
    bool replaceChildColliders = true;
    bool keepSource = false;
    float bevelRadius = 0.0f;
    int count = 0;
    int bevelSegments = 0;
    int radialSegments = 0;
    int segments = 0;
    int sides = 0;
    int detail = 0;
    int hemisphereSegments = 0;
    bool mergeHull = false;
    float thickness = 0.0f;
    float topRadius = 0.0f;
    float bottomRadius = 0.0f;
    float length = 0.0f;
    float spanDegrees = 0.0f;
    float ridgeRatio = 0.0f;
    float width = 0.0f;
    float offsetY = 0.0f;
    float projectionHeight = 0.0f;
    float projectionDistance = 0.0f;
    bool structuralOnly = true;
    ri::math::Vec3 offsetStepPosition{};
    ri::math::Vec3 offsetStepRotation{};
    ri::math::Vec3 offsetStepScale{1.0f, 1.0f, 1.0f};
    std::shared_ptr<StructuralNode> basePrimitive;
    std::string mirrorAxis;
    std::string mirroredFrom;
    std::string archStyle;
    std::vector<ri::math::Vec3> points;
    std::vector<ri::math::Vec3> vertices;
};

struct StructuralGraphSummary {
    std::size_t nodeCount = 0;
    std::size_t edgeCount = 0;
    std::size_t cycleCount = 0;
    std::size_t unresolvedDependencyCount = 0;
    std::vector<StructuralDependencyIssue> unresolvedDependencies;
    StructuralPhaseBuckets phaseBuckets;
};

struct StructuralDependencyGraph {
    std::vector<StructuralNode> orderedNodes;
    StructuralGraphSummary summary;
};

[[nodiscard]] std::string_view ToString(StructuralPhase phase);
/// O(1) `string_view` classification: phase tables use transparent hashing (no per-call `std::string` alloc).
[[nodiscard]] StructuralPhase ClassifyStructuralPhase(std::string_view type);
[[nodiscard]] std::vector<std::string> GetExplicitDependencies(const StructuralNode& node);
[[nodiscard]] StructuralDependencyGraph BuildStructuralDependencyGraph(const std::vector<StructuralNode>& nodes);

} // namespace ri::structural
