#pragma once

#include "RawIron/Math/Vec3.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ri::world {

using InfoPanelValue = std::variant<std::monostate, std::string, double, bool>;
using LogicEntityPropertyMap = std::unordered_map<std::string, InfoPanelValue>;
using LogicEntityStateMap = std::unordered_map<std::string, LogicEntityPropertyMap>;

struct RuntimeInfoPanelCounts {
    std::size_t physicsObjects = 0;
    std::size_t enemies = 0;
    std::size_t logicEntities = 0;
    std::size_t structuralCollidables = 0;
    std::size_t bvhMeshes = 0;
    std::size_t triggerVolumes = 0;
    std::size_t interactives = 0;
    std::size_t collidables = 0;
};

struct InfoPanelBinding {
    std::string label;
    std::string logicEntityId;
    std::string property;
    std::string worldValue;
    std::string worldFlag;
    std::string runtimeMetric;
    InfoPanelValue value;
    InfoPanelValue fallback;
};

struct InfoPanelDefinition {
    std::vector<std::string> lines;
    std::vector<InfoPanelBinding> bindings;
    bool replaceBindings = false;
    std::string text;
};

struct DynamicInfoPanelSpawner {
    std::string id;
    std::string title;
    ri::math::Vec3 position{};
    ri::math::Vec3 size{2.2f, 1.25f, 0.08f};
    double refreshIntervalSeconds = 0.25;
    bool focusable = true;
    std::string interactionPrompt = "Read Panel";
    std::string interactionHook;
    InfoPanelDefinition panel{};
};

struct DynamicInfoPanelRenderState {
    std::string id;
    std::string title;
    ri::math::Vec3 position{};
    ri::math::Vec3 size{};
    std::vector<std::string> lines;
    bool dirty = false;
    std::uint64_t revision = 0;
};

} // namespace ri::world
