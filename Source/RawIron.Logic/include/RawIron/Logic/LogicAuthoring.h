#pragma once

#include "RawIron/Logic/LogicTypes.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ri::logic {

/// Spatial metadata for a logic node block in the 3D editor.
struct LogicNodePlacement {
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 3> rotationDegrees{0.0f, 0.0f, 0.0f};
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    std::string layer = "logic";
    bool debugVisible = true;
};

struct LogicAutoLayoutOptions {
    std::array<float, 3> origin{1000.0f, 0.0f, 1000.0f};
    std::array<float, 3> spacing{3.0f, 0.0f, 3.0f};
    std::size_t columns = 8;
    std::string layer = "logic_autogen";
    bool debugVisible = true;
    /// When true, only nodes with default placement are auto-positioned.
    bool preserveExplicitPlacements = true;
    /// Optional center/extent for in-world authored space to route fallback I/O toward.
    std::array<float, 3> roomCenter{0.0f, 0.0f, 0.0f};
    std::array<float, 3> roomHalfExtents{16.0f, 0.0f, 16.0f};
    /// Extra offset from room boundary where fallback I/O trunk lanes live.
    std::array<float, 3> ioLaneOffset{4.0f, 0.0f, 0.0f};
    /// Known world endpoint positions used for routing into visible map actors.
    std::unordered_map<std::string, std::array<float, 3>> worldEndpointPositions;
    /// Route fallback control points for long wires connecting auto-layout and in-room endpoints.
    bool routeFallbackIoWires = true;
    /// Keep existing authored control points if present.
    bool preserveWireControlPoints = true;
};

/// Authoring-time wire metadata (spline/polyline points are editor-only; compile ignores them).
struct LogicAuthoringWire {
    std::string id;
    std::string sourceId;
    std::string outputName;
    std::vector<LogicRouteTarget> targets;
    std::vector<std::array<float, 3>> controlPoints;
    bool muted = false;
};

struct LogicNodeInstance {
    LogicNodeDefinition definition;
    LogicNodePlacement placement{};
};

/// 3D authoring graph: blocks + routed wires in world space.
struct LogicAuthoringGraph {
    std::vector<LogicNodeInstance> nodes;
    std::vector<LogicAuthoringWire> wires;
};

enum class LogicAuthoringIssueSeverity {
    Warning,
    Error,
};

struct LogicAuthoringCompileIssue {
    LogicAuthoringIssueSeverity severity = LogicAuthoringIssueSeverity::Warning;
    std::string code;
    std::string message;
    std::string subjectId;
};

enum class LogicAuthoringIssueCategory {
    General,
    Node,
    Wire,
    Port,
    WorldActor,
};

struct LogicAuthoringIssuePresentation {
    LogicAuthoringIssueCategory category = LogicAuthoringIssueCategory::General;
    std::string_view categoryName = "General";
    std::string_view severityName = "Warning";
    bool normalization = false;
    bool portIssue = false;
};

struct LogicAuthoringCompileSummary {
    std::size_t warningCount = 0;
    std::size_t errorCount = 0;
    std::size_t normalizedPortCount = 0;
    std::size_t normalizedNodeCount = 0;
    std::size_t clampedDelayCount = 0;
    std::size_t assumedWorldActorEndpointCount = 0;
    std::size_t unknownPortCount = 0;
};

struct LogicAuthoringCompileResult {
    LogicGraphSpec spec{};
    std::vector<LogicAuthoringCompileIssue> issues;
    LogicAuthoringCompileSummary summary{};
};

struct LogicAuthoringCompileOptions {
    /// Optional explicit allowlist of world actor ids that can be used as route endpoints.
    std::unordered_set<std::string> knownWorldActorIds;
    /// Optional world actor kind per id (e.g. trigger_volume, spawner, door) for port contract validation.
    std::unordered_map<std::string, std::string> knownWorldActorKinds;
    /// If true, unknown non-node ids are treated as world actor endpoints.
    bool allowUnknownWorldActorIds = true;
};

/// Compile spatial authoring data into runtime executable graph spec.
[[nodiscard]] LogicGraphSpec CompileLogicAuthoringGraph(const LogicAuthoringGraph& authoring);
[[nodiscard]] LogicGraphSpec CompileLogicAuthoringGraph(const LogicAuthoringGraph& authoring,
                                                        const LogicAuthoringCompileOptions& options);
[[nodiscard]] LogicAuthoringCompileResult CompileLogicAuthoringGraphWithReport(
    const LogicAuthoringGraph& authoring);
[[nodiscard]] LogicAuthoringCompileResult CompileLogicAuthoringGraphWithReport(
    const LogicAuthoringGraph& authoring,
    const LogicAuthoringCompileOptions& options);

/// Categorize compile issue code for editor filtering and color mapping.
[[nodiscard]] LogicAuthoringIssueCategory GetLogicAuthoringIssueCategory(std::string_view issueCode);

/// Human-readable category label for UI filters and badges.
[[nodiscard]] std::string_view GetLogicAuthoringIssueCategoryName(LogicAuthoringIssueCategory category);

/// Human-readable severity label for UI badges.
[[nodiscard]] std::string_view GetLogicAuthoringIssueSeverityName(LogicAuthoringIssueSeverity severity);

/// True for canonicalization/auto-fix style diagnostics.
[[nodiscard]] bool IsLogicAuthoringNormalizationIssue(std::string_view issueCode);

/// True when issue represents unknown/missing/invalid port contract.
[[nodiscard]] bool IsLogicAuthoringPortIssue(std::string_view issueCode);

/// Precomputed labels/flags for rendering a compile issue in inspector UI.
[[nodiscard]] LogicAuthoringIssuePresentation BuildLogicAuthoringIssuePresentation(
    const LogicAuthoringCompileIssue& issue);

/// Batch helper for inspector tables/lists.
[[nodiscard]] std::vector<LogicAuthoringIssuePresentation> BuildLogicAuthoringIssuePresentations(
    const std::vector<LogicAuthoringCompileIssue>& issues);

/// True when compile report contains one or more errors.
[[nodiscard]] bool LogicAuthoringCompileHasErrors(const LogicAuthoringCompileResult& result);

/// True when compile report is usable without hard failures.
[[nodiscard]] bool LogicAuthoringCompileSucceeded(const LogicAuthoringCompileResult& result);

/// Build deterministic fallback placement for text-authored or unplaced logic nodes.
[[nodiscard]] LogicAuthoringGraph AutoLayoutLogicAuthoringGraph(
    const LogicAuthoringGraph& authoring,
    const LogicAutoLayoutOptions& options = {});

/// Extract stable node placement map for editor/debug visualization.
[[nodiscard]] std::unordered_map<std::string, LogicNodePlacement> BuildLogicNodePlacementMap(
    const LogicAuthoringGraph& authoring);

} // namespace ri::logic
