#include "RawIron/World/BuiltinEntityCommandRouting.h"

#include "RawIron/Logic/LogicGraph.h"
#include "RawIron/World/RuntimeState.h"

#include <cctype>
#include <cstdlib>
#include <string_view>

namespace ri::world {
namespace {

void ToLowerInPlace(std::string& value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
}

void LogLine(std::vector<std::string>* logLines, const std::string& line) {
    if (logLines != nullptr) {
        logLines->push_back(line);
    }
}

std::string NormalizeVerb(std::string_view verb) {
    std::string v(verb);
    ToLowerInPlace(v);
    return v;
}

} // namespace

std::string EntityPresentationHiddenFlagKey(const std::string_view targetId) {
    return "entity.presentation.hidden." + std::string(targetId);
}

BuiltinEntityCommandResult DispatchBuiltinEntityCommand(RuntimeEnvironmentService& environment,
                                                        ri::logic::LogicGraph& graph,
                                                        const BuiltinEntityCommand& command,
                                                        std::vector<std::string>* logLines) {
    const std::string verb = NormalizeVerb(command.verb);
    if (verb.empty() || command.targetId.empty()) {
        return BuiltinEntityCommandResult{.ok = false, .message = "builtin_command_missing_verb_or_target"};
    }

    if (verb == "set_hidden" || verb == "hide") {
        environment.SetWorldFlag(EntityPresentationHiddenFlagKey(command.targetId), true);
        LogLine(logLines, "entity presentation: hidden " + command.targetId);
        return BuiltinEntityCommandResult{.ok = true, .message = "ok_hidden"};
    }
    if (verb == "set_visible" || verb == "show") {
        environment.ClearWorldFlag(EntityPresentationHiddenFlagKey(command.targetId));
        LogLine(logLines, "entity presentation: visible " + command.targetId);
        return BuiltinEntityCommandResult{.ok = true, .message = "ok_visible"};
    }
    if (verb == "set_light_intensity" || verb == "light_intensity") {
        const auto it = command.args.find("value");
        if (it == command.args.end()) {
            return BuiltinEntityCommandResult{.ok = false, .message = "light_intensity_missing_value"};
        }
        const float value = std::strtof(it->second.c_str(), nullptr);
        if (!std::isfinite(value) || value < 0.0f) {
            return BuiltinEntityCommandResult{.ok = false, .message = "light_intensity_invalid_value"};
        }
        if (!environment.SetLevelLightIntensity(command.targetId, value)) {
            LogLine(logLines, "light_intensity: no light " + command.targetId);
            return BuiltinEntityCommandResult{.ok = false, .message = "light_not_found"};
        }
        LogLine(logLines, "light " + command.targetId + " intensity " + it->second);
        return BuiltinEntityCommandResult{.ok = true, .message = "ok_light_intensity"};
    }
    if (verb == "logic_input" || verb == "dispatch_input") {
        const auto in = command.args.find("input");
        if (in == command.args.end() || in->second.empty()) {
            return BuiltinEntityCommandResult{.ok = false, .message = "logic_input_missing_input"};
        }
        ri::logic::LogicContext ctx;
        const auto inst = command.args.find("instigator");
        if (inst != command.args.end()) {
            ctx.instigatorId = inst->second;
        }
        if (environment.ApplyWorldActorLogicInput(graph, command.targetId, in->second, ctx)) {
            LogLine(logLines, "logic " + command.targetId + " " + in->second);
            return BuiltinEntityCommandResult{.ok = true, .message = "ok_logic_input"};
        }
        LogLine(logLines, "logic_input: no route for " + command.targetId);
        return BuiltinEntityCommandResult{.ok = false, .message = "logic_input_unrouted"};
    }
    return BuiltinEntityCommandResult{.ok = false, .message = "unknown_verb"};
}

} // namespace ri::world
