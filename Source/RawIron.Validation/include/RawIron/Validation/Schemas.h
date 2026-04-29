#pragma once

#include "RawIron/Events/EventEngine.h"
#include "RawIron/Structural/StructuralGraph.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::validation {

struct SchemaValidationMetrics {
    std::size_t tuningParses = 0;
    std::size_t tuningParseFailures = 0;
    std::size_t levelValidations = 0;
    std::size_t levelValidationFailures = 0;
};

struct RuntimeTuning {
    std::optional<double> sensitivity;
    std::optional<double> staminaDrain;
    std::optional<double> staminaRegen;
    std::optional<double> walkSpeed;
    std::optional<double> sprintSpeed;
    std::optional<double> gravity;
    std::optional<double> jumpForce;
    std::optional<double> fallGravityMultiplier;
    std::optional<double> lowJumpGravityMultiplier;
    std::optional<double> maxFallSpeed;
};

struct RuntimeCheckpointState {
    std::optional<std::string> level;
    std::optional<std::string> checkpointId;
    std::vector<std::string> flags;
    std::unordered_map<std::string, double> values;
    std::vector<std::string> eventIds;
};

struct LevelConnection {
    std::string target;
    std::optional<std::string> input;
    std::optional<double> delayMs;
    std::optional<bool> once;
};

using LevelActionMap = std::unordered_map<std::string, std::vector<ri::events::EventAction>>;
using LevelOutputMap = std::unordered_map<std::string, std::vector<LevelConnection>>;

struct WorldspawnDefinition {
    LevelOutputMap outputs;
    LevelActionMap inputs;
};

struct LevelPayload {
    std::optional<std::string> levelName;
    std::optional<std::string> objective;
    WorldspawnDefinition worldspawn;
    std::vector<ri::structural::StructuralNode> geometry;
    std::vector<ri::structural::StructuralNode> lights;
    std::vector<ri::structural::StructuralNode> modelInstances;
    std::vector<ri::structural::StructuralNode> spawners;
    std::vector<ri::events::EventDefinition> events;
    ri::events::ActionGroupMap actionGroups;
    ri::events::TargetGroupMap targetGroups;
    ri::events::SequenceMap sequences;
    std::vector<ri::structural::StructuralNode> prefabInstances;
};

[[nodiscard]] RuntimeTuning ParseStoredRuntimeTuning(const RuntimeTuning& value);
[[nodiscard]] RuntimeCheckpointState ParseStoredCheckpointState(const RuntimeCheckpointState& value);
[[nodiscard]] std::optional<std::string> ValidateCheckpointState(const RuntimeCheckpointState& checkpoint,
                                                                 std::string_view sourceName = "checkpoint");
[[nodiscard]] std::optional<std::string> ValidateLevelPayload(const LevelPayload& levelData,
                                                              std::string_view levelFilename = "level");
[[nodiscard]] SchemaValidationMetrics GetSchemaValidationMetrics();

} // namespace ri::validation
