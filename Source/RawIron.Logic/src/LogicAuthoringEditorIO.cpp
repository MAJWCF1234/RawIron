#include "RawIron/Logic/LogicAuthoringEditorIO.h"

#include "RawIron/Logic/LogicKitNodeFactory.h"

#include <fstream>
#include <sstream>

namespace ri::logic {
namespace {

[[nodiscard]] bool ParseNodeLine(const std::string& line, LogicAuthoringEditorNodeRecord& out) {
    std::stringstream parser(line);
    std::string kind;
    std::getline(parser, kind, ',');
    if (kind != "node") {
        return false;
    }
    std::getline(parser, out.logicNodeId, ',');
    std::getline(parser, out.kitId, ',');
    parser >> out.position[0];
    parser.ignore(1);
    parser >> out.position[1];
    parser.ignore(1);
    parser >> out.position[2];
    parser.ignore(1);
    std::string executableFlag;
    std::getline(parser, executableFlag, ',');
    out.executable = executableFlag == "1";
    return !out.logicNodeId.empty() && !out.kitId.empty();
}

[[nodiscard]] bool ParseTriggerLine(const std::string& line, LogicAuthoringEditorTriggerRecord& out) {
    std::stringstream parser(line);
    std::string kind;
    std::getline(parser, kind, ',');
    if (kind != "trigger") {
        return false;
    }
    std::getline(parser, out.triggerId, ',');
    parser >> out.position[0];
    parser.ignore(1);
    parser >> out.position[1];
    parser.ignore(1);
    parser >> out.position[2];
    return !out.triggerId.empty();
}

[[nodiscard]] bool ParseWireLine(const std::string& line, LogicAuthoringEditorWireRecord& out) {
    std::stringstream parser(line);
    std::string kind;
    std::getline(parser, kind, ',');
    if (kind != "wire") {
        return false;
    }
    std::getline(parser, out.wireId, ',');
    std::getline(parser, out.sourceLogicId, ',');
    std::getline(parser, out.outputName, ',');
    std::getline(parser, out.targetLogicId, ',');
    std::getline(parser, out.inputName, ',');
    return !out.sourceLogicId.empty() && !out.targetLogicId.empty();
}

} // namespace

std::optional<LogicAuthoringEditorFile> LoadLogicAuthoringEditorFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return std::nullopt;
    }

    LogicAuthoringEditorFile file{};
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (const std::size_t eq = line.find('='); eq != std::string::npos) {
            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            if (key == "creator_visible") {
                file.creatorVisible = value == "1";
                continue;
            }
            if (key == "player_hidden") {
                file.playerHidden = value == "1";
                continue;
            }
        }

        LogicAuthoringEditorNodeRecord node{};
        if (ParseNodeLine(line, node)) {
            file.nodes.push_back(std::move(node));
            continue;
        }
        LogicAuthoringEditorTriggerRecord trigger{};
        if (ParseTriggerLine(line, trigger)) {
            file.triggers.push_back(std::move(trigger));
            continue;
        }
        LogicAuthoringEditorWireRecord wire{};
        if (ParseWireLine(line, wire)) {
            file.wires.push_back(std::move(wire));
        }
    }

    if (file.nodes.empty() && file.wires.empty() && file.triggers.empty()) {
        return std::nullopt;
    }
    return file;
}

LogicAuthoringGraph BuildLogicAuthoringGraphFromEditorFile(const LogicAuthoringEditorFile& file) {
    LogicAuthoringGraph graph{};
    graph.nodes.reserve(file.nodes.size());
    for (const LogicAuthoringEditorNodeRecord& record : file.nodes) {
        const std::optional<LogicKitNodeFactoryResult> factory =
            CreateLogicNodeFromKitId(record.kitId, record.logicNodeId);
        if (!factory.has_value()) {
            continue;
        }
        LogicNodeInstance instance{};
        instance.definition = factory->definition;
        instance.sourceKitId = record.kitId;
        instance.placement.position = record.position;
        instance.placement.layer = "logic";
        instance.placement.debugVisible = file.creatorVisible;
        graph.nodes.push_back(std::move(instance));
    }

    graph.wires.reserve(file.wires.size());
    for (const LogicAuthoringEditorWireRecord& wire : file.wires) {
        LogicAuthoringWire authoringWire{};
        authoringWire.id = wire.wireId;
        authoringWire.sourceId = wire.sourceLogicId;
        authoringWire.outputName = wire.outputName;
        LogicRouteTarget target{};
        target.targetId = wire.targetLogicId;
        target.inputName = wire.inputName;
        authoringWire.targets.push_back(target);
        graph.wires.push_back(std::move(authoringWire));
    }
    return graph;
}

LogicAuthoringCompileOptions BuildCompileOptionsFromEditorFile(const LogicAuthoringEditorFile& file) {
    LogicAuthoringCompileOptions options{};
    for (const LogicAuthoringEditorTriggerRecord& trigger : file.triggers) {
        options.knownWorldActorIds.insert(trigger.triggerId);
        options.knownWorldActorKinds[trigger.triggerId] = "trigger_volume";
    }
    for (const LogicAuthoringEditorWireRecord& wire : file.wires) {
        if (wire.sourceLogicId.rfind("Trigger_", 0) == 0) {
            options.knownWorldActorIds.insert(wire.sourceLogicId);
            options.knownWorldActorKinds[wire.sourceLogicId] = "trigger_volume";
        }
        if (wire.targetLogicId.rfind("Trigger_", 0) == 0) {
            options.knownWorldActorIds.insert(wire.targetLogicId);
            options.knownWorldActorKinds[wire.targetLogicId] = "trigger_volume";
        }
    }
    return options;
}

} // namespace ri::logic
