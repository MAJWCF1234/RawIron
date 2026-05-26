#include "RawIron/Logic/LogicAuthoring.h"
#include "RawIron/Logic/LogicPortSchema.h"
#include "LogicAuthoringDetail.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace ri::logic {
namespace {

[[nodiscard]] bool IsDefaultPlacement(const LogicNodePlacement& placement) {
    const bool defaultPosition = placement.position == std::array<float, 3>{0.0f, 0.0f, 0.0f};
    const bool defaultRotation = placement.rotationDegrees == std::array<float, 3>{0.0f, 0.0f, 0.0f};
    const bool defaultScale = placement.scale == std::array<float, 3>{1.0f, 1.0f, 1.0f};
    const bool defaultLayer = placement.layer == "logic";
    return defaultPosition && defaultRotation && defaultScale && defaultLayer;
}

[[nodiscard]] bool IsNodeInAutoCluster(const std::unordered_set<std::string>& autoNodeIds, std::string_view nodeId) {
    return autoNodeIds.contains(std::string(nodeId));
}

[[nodiscard]] std::array<float, 3> ComputeIoLaneAnchor(const LogicAutoLayoutOptions& options,
                                                       const std::array<float, 3>& endpoint) {
    const float roomMinX = options.roomCenter[0] - options.roomHalfExtents[0];
    const float laneX = roomMinX - std::abs(options.ioLaneOffset[0]);
    return std::array<float, 3>{laneX, endpoint[1], endpoint[2]};
}

[[nodiscard]] std::array<float, 3> ResolveEndpointPosition(
    std::string_view endpointId,
    const std::unordered_map<std::string, std::array<float, 3>>& nodePositions,
    const LogicAutoLayoutOptions& options) {
    const auto nodeIt = nodePositions.find(std::string(endpointId));
    if (nodeIt != nodePositions.end()) {
        return nodeIt->second;
    }
    const auto worldIt = options.worldEndpointPositions.find(std::string(endpointId));
    if (worldIt != options.worldEndpointPositions.end()) {
        return worldIt->second;
    }
    return options.roomCenter;
}

[[nodiscard]] bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
}

[[nodiscard]] LogicAuthoringCompileSummary BuildCompileSummary(
    const std::vector<LogicAuthoringCompileIssue>& issues) {
    LogicAuthoringCompileSummary summary{};
    for (const LogicAuthoringCompileIssue& issue : issues) {
        if (issue.severity == LogicAuthoringIssueSeverity::Error) {
            ++summary.errorCount;
        } else {
            ++summary.warningCount;
        }

        if (IsLogicAuthoringNormalizationIssue(issue.code)) {
            ++summary.normalizedPortCount;
        }
        if (GetLogicAuthoringIssueCategory(issue.code) == LogicAuthoringIssueCategory::Node &&
            IsLogicAuthoringNormalizationIssue(issue.code)) {
            ++summary.normalizedNodeCount;
        }
        if (EndsWith(issue.code, "_delay_clamped")) {
            ++summary.clampedDelayCount;
        }
        if (issue.code.find("world_actor") != std::string::npos && EndsWith(issue.code, "_assumed")) {
            ++summary.assumedWorldActorEndpointCount;
        }
        if (IsLogicAuthoringPortIssue(issue.code) && EndsWith(issue.code, "_unknown")) {
            ++summary.unknownPortCount;
        }
    }
    return summary;
}

} // namespace

LogicAuthoringIssueCategory GetLogicAuthoringIssueCategory(std::string_view issueCode) {
    if (issueCode.rfind("node.", 0) == 0) {
        return LogicAuthoringIssueCategory::Node;
    }
    if (issueCode.rfind("wire.", 0) == 0) {
        if (issueCode.find("world_actor") != std::string::npos) {
            return LogicAuthoringIssueCategory::WorldActor;
        }
        if (issueCode.find("_input_") != std::string::npos || issueCode.find("_output_") != std::string::npos) {
            return LogicAuthoringIssueCategory::Port;
        }
        return LogicAuthoringIssueCategory::Wire;
    }
    return LogicAuthoringIssueCategory::General;
}

std::string_view GetLogicAuthoringIssueCategoryName(const LogicAuthoringIssueCategory category) {
    switch (category) {
    case LogicAuthoringIssueCategory::General:
        return "General";
    case LogicAuthoringIssueCategory::Node:
        return "Node";
    case LogicAuthoringIssueCategory::Wire:
        return "Wire";
    case LogicAuthoringIssueCategory::Port:
        return "Port";
    case LogicAuthoringIssueCategory::WorldActor:
        return "WorldActor";
    }
    return "General";
}

std::string_view GetLogicAuthoringIssueSeverityName(const LogicAuthoringIssueSeverity severity) {
    switch (severity) {
    case LogicAuthoringIssueSeverity::Warning:
        return "Warning";
    case LogicAuthoringIssueSeverity::Error:
        return "Error";
    }
    return "Warning";
}

bool IsLogicAuthoringNormalizationIssue(std::string_view issueCode) {
    return issueCode.find("_normalized") != std::string::npos || issueCode.find("_clamped") != std::string::npos ||
           issueCode.find("_swapped") != std::string::npos || issueCode.find("_adjusted") != std::string::npos ||
           issueCode.find("_trimmed") != std::string::npos || issueCode.find("_defaulted") != std::string::npos;
}

bool IsLogicAuthoringPortIssue(std::string_view issueCode) {
    const LogicAuthoringIssueCategory category = GetLogicAuthoringIssueCategory(issueCode);
    return category == LogicAuthoringIssueCategory::Port || category == LogicAuthoringIssueCategory::WorldActor;
}

LogicAuthoringIssuePresentation BuildLogicAuthoringIssuePresentation(const LogicAuthoringCompileIssue& issue) {
    const LogicAuthoringIssueCategory category = GetLogicAuthoringIssueCategory(issue.code);
    return LogicAuthoringIssuePresentation{
        .category = category,
        .categoryName = GetLogicAuthoringIssueCategoryName(category),
        .severityName = GetLogicAuthoringIssueSeverityName(issue.severity),
        .normalization = IsLogicAuthoringNormalizationIssue(issue.code),
        .portIssue = IsLogicAuthoringPortIssue(issue.code),
    };
}

std::vector<LogicAuthoringIssuePresentation> BuildLogicAuthoringIssuePresentations(
    const std::vector<LogicAuthoringCompileIssue>& issues) {
    std::vector<LogicAuthoringIssuePresentation> presentations{};
    presentations.reserve(issues.size());
    for (const LogicAuthoringCompileIssue& issue : issues) {
        presentations.push_back(BuildLogicAuthoringIssuePresentation(issue));
    }
    return presentations;
}

bool LogicAuthoringCompileHasErrors(const LogicAuthoringCompileResult& result) {
    if (result.summary.errorCount > 0) {
        return true;
    }
    for (const LogicAuthoringCompileIssue& issue : result.issues) {
        if (issue.severity == LogicAuthoringIssueSeverity::Error) {
            return true;
        }
    }
    return false;
}

bool LogicAuthoringCompileSucceeded(const LogicAuthoringCompileResult& result) {
    return !LogicAuthoringCompileHasErrors(result);
}

LogicAuthoringGraph AutoLayoutLogicAuthoringGraph(const LogicAuthoringGraph& authoring,
                                                  const LogicAutoLayoutOptions& options) {
    LogicAuthoringGraph laidOut = authoring;
    const std::size_t columns = std::max<std::size_t>(1, options.columns);
    std::size_t autoIndex = 0;
    std::unordered_set<std::string> autoNodeIds{};
    autoNodeIds.reserve(laidOut.nodes.size());
    for (LogicNodeInstance& node : laidOut.nodes) {
        if (options.preserveExplicitPlacements && !IsDefaultPlacement(node.placement)) {
            continue;
        }
        const std::string nodeId = detail::NodeDefinitionId(node.definition);
        const std::size_t row = autoIndex / columns;
        const std::size_t col = autoIndex % columns;
        node.placement.position = std::array<float, 3>{
            options.origin[0] + static_cast<float>(col) * options.spacing[0],
            options.origin[1] + static_cast<float>(row) * options.spacing[1],
            options.origin[2] + static_cast<float>(row) * options.spacing[2],
        };
        node.placement.rotationDegrees = std::array<float, 3>{0.0f, 0.0f, 0.0f};
        node.placement.scale = std::array<float, 3>{1.0f, 1.0f, 1.0f};
        node.placement.layer = options.layer;
        node.placement.debugVisible = options.debugVisible;
        if (!nodeId.empty()) {
            autoNodeIds.insert(nodeId);
        }
        ++autoIndex;
    }

    if (options.routeFallbackIoWires) {
        std::unordered_map<std::string, std::array<float, 3>> nodePositions{};
        nodePositions.reserve(laidOut.nodes.size());
        for (const LogicNodeInstance& node : laidOut.nodes) {
            const std::string nodeId = detail::NodeDefinitionId(node.definition);
            if (!nodeId.empty()) {
                nodePositions[nodeId] = node.placement.position;
            }
        }

        for (LogicAuthoringWire& wire : laidOut.wires) {
            if (wire.muted) {
                continue;
            }
            if (options.preserveWireControlPoints && !wire.controlPoints.empty()) {
                continue;
            }
            if (wire.targets.empty()) {
                continue;
            }

            const bool sourceIsAuto = IsNodeInAutoCluster(autoNodeIds, wire.sourceId);
            const std::array<float, 3> sourcePos = ResolveEndpointPosition(wire.sourceId, nodePositions, options);

            // Route once per wire using the first target as representative trunk direction.
            const LogicRouteTarget& target = wire.targets.front();
            const bool targetIsAuto = IsNodeInAutoCluster(autoNodeIds, target.targetId);
            if (sourceIsAuto == targetIsAuto) {
                continue;
            }

            const std::array<float, 3> targetPos = ResolveEndpointPosition(target.targetId, nodePositions, options);
            const std::array<float, 3> inRoomPos = sourceIsAuto ? targetPos : sourcePos;
            const std::array<float, 3> laneA = ComputeIoLaneAnchor(options, sourcePos);
            const std::array<float, 3> laneB = ComputeIoLaneAnchor(options, inRoomPos);

            wire.controlPoints.clear();
            wire.controlPoints.push_back(laneA);
            wire.controlPoints.push_back(laneB);
        }
    }

    return laidOut;
}

LogicGraphSpec CompileLogicAuthoringGraph(const LogicAuthoringGraph& authoring) {
    return CompileLogicAuthoringGraphWithReport(authoring, {}).spec;
}

LogicGraphSpec CompileLogicAuthoringGraph(const LogicAuthoringGraph& authoring,
                                          const LogicAuthoringCompileOptions& options) {
    return CompileLogicAuthoringGraphWithReport(authoring, options).spec;
}

LogicAuthoringCompileResult CompileLogicAuthoringGraphWithReport(const LogicAuthoringGraph& authoring) {
    return CompileLogicAuthoringGraphWithReport(authoring, {});
}

LogicAuthoringCompileResult CompileLogicAuthoringGraphWithReport(const LogicAuthoringGraph& authoring,
                                                                 const LogicAuthoringCompileOptions& options) {
    LogicAuthoringCompileResult result{};
    LogicGraphSpec& spec = result.spec;
    std::unordered_set<std::string> seenNodeIds{};
    seenNodeIds.reserve(authoring.nodes.size());
    std::unordered_map<std::string, std::string> nodeKindsById{};
    nodeKindsById.reserve(authoring.nodes.size());

    for (const LogicNodeInstance& instance : authoring.nodes) {
        const std::string nodeId = detail::NodeDefinitionId(instance.definition);
        if (nodeId.empty()) {
            detail::AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Error,
                     "node.missing_id",
                     "Skipped logic node with empty id.");
            continue;
        }
        if (seenNodeIds.insert(nodeId).second) {
            const LogicNodeDefinition normalized = detail::NormalizeNodeDefinition(instance.definition, result.issues);
            nodeKindsById[nodeId] = std::string(GetLogicNodeKindName(normalized));
            spec.nodes.push_back(normalized);
        } else {
            detail::AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Warning,
                     "node.duplicate_id",
                     "Skipped duplicate logic node id.",
                     nodeId);
        }
    }

    std::unordered_set<std::string> seenWireIds{};
    seenWireIds.reserve(authoring.wires.size());
    for (const LogicAuthoringWire& wire : authoring.wires) {
        if (wire.muted) {
            continue;
        }
        if (!wire.id.empty()) {
            if (!seenWireIds.insert(wire.id).second) {
                detail::AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.duplicate_id",
                         "Skipped duplicate wire id.",
                         wire.id);
                continue;
            }
        }
        if (wire.sourceId.empty()) {
            detail::AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Error,
                     "wire.missing_source",
                     "Skipped wire with empty source id.",
                     wire.id);
            continue;
        }
        const bool sourceIsNode = seenNodeIds.contains(wire.sourceId);
        const bool sourceIsKnownWorldActor = options.knownWorldActorIds.contains(wire.sourceId);
        const bool sourceAcceptedAsWorldActor = !sourceIsNode && (sourceIsKnownWorldActor || options.allowUnknownWorldActorIds);
        if (!sourceIsNode && !sourceAcceptedAsWorldActor) {
            detail::AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Error,
                     "wire.unknown_source",
                     "Skipped wire whose source node does not exist in authoring graph.",
                     wire.id.empty() ? wire.sourceId : wire.id);
            continue;
        }
        if (sourceAcceptedAsWorldActor && !sourceIsKnownWorldActor) {
            detail::AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Warning,
                     "wire.world_actor_source_assumed",
                     "Wire source is not a logic node; treated as world actor endpoint.",
                     wire.sourceId);
        }
        if (wire.outputName.empty()) {
            detail::AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Error,
                     "wire.missing_output",
                     "Skipped wire with empty output name.",
                     wire.id.empty() ? wire.sourceId : wire.id);
            continue;
        }
        if (sourceIsNode) {
            const auto kindIt = nodeKindsById.find(wire.sourceId);
            const LogicNodePortSchema schema =
                GetLogicNodePortSchema(kindIt == nodeKindsById.end() ? std::string_view{} : std::string_view(kindIt->second));
            const detail::PortResolution port = detail::ResolvePortName(schema, wire.outputName, false);
            if (!port.recognized) {
                detail::AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.source_output_unknown",
                         "Source output port name is not defined for node kind.",
                         wire.id.empty() ? wire.sourceId : wire.id);
            } else if (port.normalized) {
                detail::AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.source_output_normalized",
                         "Source output port normalized to canonical port name.",
                         wire.id.empty() ? wire.sourceId : wire.id);
            }
        } else if (sourceIsKnownWorldActor) {
            const auto kindIt = options.knownWorldActorKinds.find(wire.sourceId);
            if (kindIt != options.knownWorldActorKinds.end() && !kindIt->second.empty()) {
                const LogicNodePortSchema schema = GetWorldActorPortSchema(kindIt->second);
                const detail::PortResolution port = detail::ResolvePortName(schema, wire.outputName, false);
                if (!port.recognized) {
                    detail::AddIssue(result.issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "wire.world_source_output_unknown",
                             "Source output port name is not defined for world actor kind.",
                             wire.id.empty() ? wire.sourceId : wire.id);
                } else if (port.normalized) {
                    detail::AddIssue(result.issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "wire.world_source_output_normalized",
                             "World actor source output normalized to canonical port name.",
                             wire.id.empty() ? wire.sourceId : wire.id);
                }
            }
        }
        if (wire.targets.empty()) {
            detail::AddIssue(result.issues,
                     LogicAuthoringIssueSeverity::Warning,
                     "wire.no_targets",
                     "Skipped wire with no route targets.",
                     wire.id.empty() ? wire.sourceId : wire.id);
            continue;
        }

        LogicRoute route{};
        route.sourceId = wire.sourceId;
        route.outputName = wire.outputName;
        if (sourceIsNode) {
            const auto kindIt = nodeKindsById.find(wire.sourceId);
            const LogicNodePortSchema schema =
                GetLogicNodePortSchema(kindIt == nodeKindsById.end() ? std::string_view{} : std::string_view(kindIt->second));
            const detail::PortResolution port = detail::ResolvePortName(schema, wire.outputName, false);
            if (port.recognized) {
                route.outputName = port.canonical;
            }
        } else if (sourceIsKnownWorldActor) {
            const auto kindIt = options.knownWorldActorKinds.find(wire.sourceId);
            if (kindIt != options.knownWorldActorKinds.end() && !kindIt->second.empty()) {
                const LogicNodePortSchema schema = GetWorldActorPortSchema(kindIt->second);
                const detail::PortResolution port = detail::ResolvePortName(schema, wire.outputName, false);
                if (port.recognized) {
                    route.outputName = port.canonical;
                }
            }
        }
        route.targets.reserve(wire.targets.size());
        for (const LogicRouteTarget& target : wire.targets) {
            if (target.targetId.empty() || target.inputName.empty()) {
                detail::AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.target_incomplete",
                         "Skipped route target with missing targetId or inputName.",
                         wire.id.empty() ? wire.sourceId : wire.id);
                continue;
            }
            const bool targetIsNode = seenNodeIds.contains(target.targetId);
            const bool targetIsKnownWorldActor = options.knownWorldActorIds.contains(target.targetId);
            const bool targetAcceptedAsWorldActor =
                !targetIsNode && (targetIsKnownWorldActor || options.allowUnknownWorldActorIds);
            if (!targetIsNode && !targetAcceptedAsWorldActor) {
                detail::AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.target_unknown",
                         "Skipped route target that does not match a node id in this authoring graph. "
                         "Use world-actor routes separately when targeting map actors.",
                         target.targetId);
                continue;
            }
            if (targetAcceptedAsWorldActor && !targetIsKnownWorldActor) {
                detail::AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.world_actor_target_assumed",
                         "Route target is not a logic node; treated as world actor endpoint.",
                         target.targetId);
            }
            if (targetIsNode) {
                const auto kindIt = nodeKindsById.find(target.targetId);
                const LogicNodePortSchema schema = GetLogicNodePortSchema(
                    kindIt == nodeKindsById.end() ? std::string_view{} : std::string_view(kindIt->second));
                const detail::PortResolution port = detail::ResolvePortName(schema, target.inputName, true);
                if (!port.recognized) {
                    detail::AddIssue(result.issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "wire.target_input_unknown",
                             "Target input port name is not defined for node kind.",
                             target.targetId);
                } else if (port.normalized) {
                    detail::AddIssue(result.issues,
                             LogicAuthoringIssueSeverity::Warning,
                             "wire.target_input_normalized",
                             "Target input port normalized to canonical port name.",
                             target.targetId);
                }
            } else if (targetIsKnownWorldActor) {
                const auto kindIt = options.knownWorldActorKinds.find(target.targetId);
                if (kindIt != options.knownWorldActorKinds.end() && !kindIt->second.empty()) {
                    const LogicNodePortSchema schema = GetWorldActorPortSchema(kindIt->second);
                    const detail::PortResolution port = detail::ResolvePortName(schema, target.inputName, true);
                    if (!port.recognized) {
                        detail::AddIssue(result.issues,
                                 LogicAuthoringIssueSeverity::Warning,
                                 "wire.world_target_input_unknown",
                                 "Target input port name is not defined for world actor kind.",
                                 target.targetId);
                    } else if (port.normalized) {
                        detail::AddIssue(result.issues,
                                 LogicAuthoringIssueSeverity::Warning,
                                 "wire.world_target_input_normalized",
                                 "World actor target input normalized to canonical port name.",
                                 target.targetId);
                    }
                }
            }
            LogicRouteTarget normalizedTarget = target;
            normalizedTarget.delayMs = detail::ClampRouteDelayMs(target.delayMs);
            if (normalizedTarget.delayMs != target.delayMs) {
                detail::AddIssue(result.issues,
                         LogicAuthoringIssueSeverity::Warning,
                         "wire.target_delay_clamped",
                         "Route target delay exceeded maximum and was clamped.",
                         target.targetId);
            }
            if (targetIsNode) {
                const auto kindIt = nodeKindsById.find(target.targetId);
                const LogicNodePortSchema schema = GetLogicNodePortSchema(
                    kindIt == nodeKindsById.end() ? std::string_view{} : std::string_view(kindIt->second));
                const detail::PortResolution port = detail::ResolvePortName(schema, target.inputName, true);
                if (port.recognized) {
                    normalizedTarget.inputName = port.canonical;
                }
            } else if (targetIsKnownWorldActor) {
                const auto kindIt = options.knownWorldActorKinds.find(target.targetId);
                if (kindIt != options.knownWorldActorKinds.end() && !kindIt->second.empty()) {
                    const LogicNodePortSchema schema = GetWorldActorPortSchema(kindIt->second);
                    const detail::PortResolution port = detail::ResolvePortName(schema, target.inputName, true);
                    if (port.recognized) {
                        normalizedTarget.inputName = port.canonical;
                    }
                }
            }
            route.targets.push_back(std::move(normalizedTarget));
        }
        if (route.targets.empty()) {
            continue;
        }
        spec.routes.push_back(std::move(route));
    }

    result.summary = BuildCompileSummary(result.issues);
    return result;
}

std::unordered_map<std::string, LogicNodePlacement> BuildLogicNodePlacementMap(const LogicAuthoringGraph& authoring) {
    std::unordered_map<std::string, LogicNodePlacement> placements{};
    placements.reserve(authoring.nodes.size());
    for (const LogicNodeInstance& instance : authoring.nodes) {
        const std::string nodeId = detail::NodeDefinitionId(instance.definition);
        if (nodeId.empty()) {
            continue;
        }
        placements[nodeId] = instance.placement;
    }
    return placements;
}

} // namespace ri::logic
