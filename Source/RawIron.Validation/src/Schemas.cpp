#include "RawIron/Validation/Schemas.h"

#include <cctype>
#include <cmath>
#include <set>
#include <sstream>
#include <unordered_set>

namespace ri::validation {
namespace {

SchemaValidationMetrics gMetrics{};

void SanitizeFiniteField(std::optional<double>& value, bool& hadFailure) {
    if (value.has_value() && !std::isfinite(*value)) {
        value.reset();
        hadFailure = true;
    }
}

void TrimInPlace(std::string& value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
}

bool TrimOptionalString(std::optional<std::string>& field) {
    if (!field.has_value()) {
        return false;
    }
    std::string& value = *field;
    const std::string before = value;
    TrimInPlace(value);
    if (value.empty()) {
        field.reset();
        return true;
    }
    return value != before;
}

std::vector<std::string> NormalizeStringList(const std::vector<std::string>& values) {
    std::vector<std::string> normalized;
    std::set<std::string> seen;
    for (const std::string& value : values) {
        if (value.empty()) {
            continue;
        }
        if (seen.insert(value).second) {
            normalized.push_back(value);
        }
    }
    return normalized;
}

std::optional<std::string> ValidateActionList(const std::vector<ri::events::EventAction>& actions,
                                              std::string_view pathPrefix) {
    for (std::size_t index = 0; index < actions.size(); ++index) {
        const ri::events::EventAction& action = actions[index];
        if (action.type.empty()) {
            std::ostringstream message;
            message << pathPrefix << '[' << index << "].type must not be empty.";
            return message.str();
        }
        if (!action.actions.empty()) {
            std::ostringstream nestedPath;
            nestedPath << pathPrefix << '[' << index << "].actions";
            if (const std::optional<std::string> nested = ValidateActionList(action.actions, nestedPath.str()); nested.has_value()) {
                return nested;
            }
        }
        if (!action.thenActions.empty()) {
            std::ostringstream nestedPath;
            nestedPath << pathPrefix << '[' << index << "].then";
            if (const std::optional<std::string> nested = ValidateActionList(action.thenActions, nestedPath.str()); nested.has_value()) {
                return nested;
            }
        }
        if (!action.elseActions.empty()) {
            std::ostringstream nestedPath;
            nestedPath << pathPrefix << '[' << index << "].else";
            if (const std::optional<std::string> nested = ValidateActionList(action.elseActions, nestedPath.str()); nested.has_value()) {
                return nested;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> ValidateConnectionMap(const LevelOutputMap& outputs, std::string_view pathPrefix) {
    for (const auto& [outputName, connections] : outputs) {
        for (std::size_t index = 0; index < connections.size(); ++index) {
            const LevelConnection& connection = connections[index];
            if (connection.target.empty()) {
                std::ostringstream message;
                message << pathPrefix << '.' << outputName << '[' << index << "].target must not be empty.";
                return message.str();
            }
            if (connection.input.has_value() && connection.input->empty()) {
                std::ostringstream message;
                message << pathPrefix << '.' << outputName << '[' << index << "].input must not be empty.";
                return message.str();
            }
            if (connection.delayMs.has_value() && (!std::isfinite(*connection.delayMs) || *connection.delayMs < 0.0)) {
                std::ostringstream message;
                message << pathPrefix << '.' << outputName << '[' << index << "].delayMs must be a finite non-negative number.";
                return message.str();
            }
        }
    }
    return std::nullopt;
}

} // namespace

RuntimeTuning ParseStoredRuntimeTuning(const RuntimeTuning& value) {
    gMetrics.tuningParses += 1;
    RuntimeTuning parsed = value;
    bool hadFailure = false;

    SanitizeFiniteField(parsed.sensitivity, hadFailure);
    SanitizeFiniteField(parsed.staminaDrain, hadFailure);
    SanitizeFiniteField(parsed.staminaRegen, hadFailure);
    SanitizeFiniteField(parsed.walkSpeed, hadFailure);
    SanitizeFiniteField(parsed.sprintSpeed, hadFailure);
    SanitizeFiniteField(parsed.gravity, hadFailure);
    SanitizeFiniteField(parsed.jumpForce, hadFailure);
    SanitizeFiniteField(parsed.fallGravityMultiplier, hadFailure);
    SanitizeFiniteField(parsed.lowJumpGravityMultiplier, hadFailure);
    SanitizeFiniteField(parsed.maxFallSpeed, hadFailure);
    SanitizeFiniteField(parsed.crouchSpeed, hadFailure);
    SanitizeFiniteField(parsed.proneSpeed, hadFailure);
    SanitizeFiniteField(parsed.maxStepHeight, hadFailure);

    if (hadFailure) {
        gMetrics.tuningParseFailures += 1;
    }
    return parsed;
}

RuntimeCheckpointState ParseStoredCheckpointState(const RuntimeCheckpointState& value) {
    gMetrics.checkpointParses += 1;
    RuntimeCheckpointState parsed = value;
    bool hadFailure = false;
    if (parsed.level.has_value() && parsed.level->empty()) {
        parsed.level.reset();
        hadFailure = true;
    }
    if (parsed.checkpointId.has_value() && parsed.checkpointId->empty()) {
        parsed.checkpointId.reset();
        hadFailure = true;
    }

    parsed.flags = NormalizeStringList(parsed.flags);
    parsed.eventIds = NormalizeStringList(parsed.eventIds);

    std::unordered_map<std::string, double> sanitizedValues;
    for (const auto& [key, number] : parsed.values) {
        if (!key.empty() && std::isfinite(number)) {
            sanitizedValues[key] = number;
        } else if (key.empty() || !std::isfinite(number)) {
            hadFailure = true;
        }
    }
    parsed.values = std::move(sanitizedValues);
    if (hadFailure) {
        gMetrics.checkpointParseFailures += 1;
    }
    return parsed;
}

std::optional<std::string> ValidateCheckpointState(const RuntimeCheckpointState& checkpoint, std::string_view sourceName) {
    gMetrics.checkpointValidations += 1;
    const std::string prefix = sourceName.empty() ? std::string("checkpoint") : std::string(sourceName);
    if (!checkpoint.level.has_value() || checkpoint.level->empty()) {
        gMetrics.checkpointValidationFailures += 1;
        return prefix + ": level must be a non-empty string.";
    }
    for (std::size_t index = 0; index < checkpoint.flags.size(); ++index) {
        if (checkpoint.flags[index].empty()) {
            gMetrics.checkpointValidationFailures += 1;
            std::ostringstream message;
            message << prefix << ": flags[" << index << "] must not be empty.";
            return message.str();
        }
    }
    for (std::size_t index = 0; index < checkpoint.eventIds.size(); ++index) {
        if (checkpoint.eventIds[index].empty()) {
            gMetrics.checkpointValidationFailures += 1;
            std::ostringstream message;
            message << prefix << ": eventIds[" << index << "] must not be empty.";
            return message.str();
        }
    }
    for (const auto& [key, number] : checkpoint.values) {
        if (key.empty()) {
            gMetrics.checkpointValidationFailures += 1;
            return prefix + ": values must not contain empty keys.";
        }
        if (!std::isfinite(number)) {
            gMetrics.checkpointValidationFailures += 1;
            return prefix + ": values." + key + " must be a finite number.";
        }
    }
    return std::nullopt;
}

void SanitizeLevelPayload(LevelPayload& levelData) {
    gMetrics.levelPayloadSanitizations += 1;
    std::size_t repairs = 0;
    if (TrimOptionalString(levelData.levelName)) {
        repairs += 1;
    }
    if (TrimOptionalString(levelData.objective)) {
        repairs += 1;
    }

    LevelOutputMap sanitizedOutputs;
    for (auto& [outputName, connections] : levelData.worldspawn.outputs) {
        std::string normalizedOutput = outputName;
        TrimInPlace(normalizedOutput);
        if (normalizedOutput != outputName) {
            repairs += 1;
        }
        if (normalizedOutput.empty()) {
            repairs += 1;
            continue;
        }
        for (LevelConnection& connection : connections) {
            const std::string beforeTarget = connection.target;
            TrimInPlace(connection.target);
            if (connection.target != beforeTarget) {
                repairs += 1;
            }
            if (connection.input.has_value()) {
                if (TrimOptionalString(connection.input)) {
                    repairs += 1;
                }
            }
        }
        std::vector<LevelConnection>& mergedConnections = sanitizedOutputs[normalizedOutput];
        mergedConnections.insert(mergedConnections.end(), connections.begin(), connections.end());
    }
    if (sanitizedOutputs.size() != levelData.worldspawn.outputs.size()) {
        repairs += 1;
    }
    levelData.worldspawn.outputs = std::move(sanitizedOutputs);

    LevelActionMap sanitizedInputs;
    for (auto& [inputName, actions] : levelData.worldspawn.inputs) {
        std::string normalizedInput = inputName;
        TrimInPlace(normalizedInput);
        if (normalizedInput != inputName) {
            repairs += 1;
        }
        if (normalizedInput.empty()) {
            repairs += 1;
            continue;
        }
        std::vector<ri::events::EventAction>& mergedActions = sanitizedInputs[normalizedInput];
        mergedActions.insert(mergedActions.end(), actions.begin(), actions.end());
    }
    if (sanitizedInputs.size() != levelData.worldspawn.inputs.size()) {
        repairs += 1;
    }
    levelData.worldspawn.inputs = std::move(sanitizedInputs);

    for (ri::events::EventDefinition& event : levelData.events) {
        const std::string idBefore = event.id;
        TrimInPlace(event.id);
        if (event.id != idBefore) {
            repairs += 1;
        }
        const std::string hookBefore = event.hook;
        TrimInPlace(event.hook);
        if (event.hook != hookBefore) {
            repairs += 1;
        }
    }

    gMetrics.levelPayloadSanitizationRepairs += repairs;
}

std::optional<std::string> ValidateLevelPayload(const LevelPayload& levelData, std::string_view levelFilename) {
    gMetrics.levelValidations += 1;

    const std::string filename = levelFilename.empty() ? std::string("level") : std::string(levelFilename);

    constexpr std::size_t kMaxContentNodes = 1'000'000U;
    const std::size_t totalNodes = levelData.geometry.size() + levelData.lights.size() + levelData.modelInstances.size()
        + levelData.spawners.size() + levelData.prefabInstances.size();
    if (totalNodes > kMaxContentNodes) {
        gMetrics.levelValidationFailures += 1;
        return filename + ": level content node count exceeds engine limit.";
    }

    std::unordered_set<std::string> eventIds;
    for (const ri::events::EventDefinition& event : levelData.events) {
        if (event.id.empty()) {
            continue;
        }
        if (!eventIds.insert(event.id).second) {
            gMetrics.levelValidationFailures += 1;
            return filename + ": duplicate event id '" + event.id + "'.";
        }
    }

    if (const std::optional<std::string> worldspawnOutputs = ValidateConnectionMap(levelData.worldspawn.outputs, "worldspawn.outputs"); worldspawnOutputs.has_value()) {
        gMetrics.levelValidationFailures += 1;
        return filename + ": " + *worldspawnOutputs;
    }
    for (const auto& [inputName, actions] : levelData.worldspawn.inputs) {
        if (const std::optional<std::string> invalid = ValidateActionList(actions, std::string("worldspawn.inputs.") + inputName); invalid.has_value()) {
            gMetrics.levelValidationFailures += 1;
            return filename + ": " + *invalid;
        }
    }

    for (std::size_t index = 0; index < levelData.events.size(); ++index) {
        const ri::events::EventDefinition& event = levelData.events[index];
        if (event.hook.empty()) {
            gMetrics.levelValidationFailures += 1;
            std::ostringstream message;
            message << filename << ": events[" << index << "].hook must not be empty.";
            return message.str();
        }
        if (event.maxRuns.has_value() && *event.maxRuns == 0) {
            gMetrics.levelValidationFailures += 1;
            std::ostringstream message;
            message << filename << ": events[" << index << "].maxRuns must be positive when present.";
            return message.str();
        }
        if (const std::optional<std::string> invalid = ValidateActionList(event.actions, "events[" + std::to_string(index) + "].actions"); invalid.has_value()) {
            gMetrics.levelValidationFailures += 1;
            return filename + ": " + *invalid;
        }
    }

    for (const auto& [groupName, actions] : levelData.actionGroups) {
        if (const std::optional<std::string> invalid = ValidateActionList(actions, std::string("actionGroups.") + groupName); invalid.has_value()) {
            gMetrics.levelValidationFailures += 1;
            return filename + ": " + *invalid;
        }
    }

    for (const auto& [sequenceName, steps] : levelData.sequences) {
        for (std::size_t stepIndex = 0; stepIndex < steps.size(); ++stepIndex) {
            const auto& step = steps[stepIndex];
            if (!std::isfinite(step.delayMs) || step.delayMs < 0.0) {
                gMetrics.levelValidationFailures += 1;
                std::ostringstream message;
                message << filename << ": sequences." << sequenceName << '[' << stepIndex << "].delayMs must be a finite non-negative number.";
                return message.str();
            }
            if (const std::optional<std::string> invalid = ValidateActionList(step.actions, "sequences." + sequenceName + '[' + std::to_string(stepIndex) + "].actions"); invalid.has_value()) {
                gMetrics.levelValidationFailures += 1;
                return filename + ": " + *invalid;
            }
        }
    }

    if ((!levelData.levelName.has_value() || levelData.levelName->empty()) && levelData.geometry.empty() && levelData.lights.empty()) {
        gMetrics.levelValidationFailures += 1;
        return "Level must have levelName, geometry, or lights.";
    }

    return std::nullopt;
}

SchemaValidationMetrics GetSchemaValidationMetrics() {
    return gMetrics;
}

} // namespace ri::validation
