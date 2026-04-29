#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ri::logic {

enum class LogicVisualPrimitiveKind {
    NodeBody,
    InputStub,
    OutputStub,
    IoDeviceBox,
    WireSegment,
    LabelAnchor,
    Junction,
};

struct LogicVisualPrimitiveDefinition {
    std::string id;
    LogicVisualPrimitiveKind kind = LogicVisualPrimitiveKind::NodeBody;
    std::array<float, 3> localPosition{0.0f, 0.0f, 0.0f};
    std::array<float, 3> localRotationDegrees{0.0f, 0.0f, 0.0f};
    std::array<float, 3> localScale{1.0f, 1.0f, 1.0f};
    std::array<float, 3> inactiveColor{0.25f, 0.25f, 0.28f};
    std::array<float, 3> activeColor{0.85f, 0.9f, 0.25f};
    std::array<float, 3> inactiveEmissive{0.02f, 0.02f, 0.03f};
    std::array<float, 3> activeEmissive{0.22f, 0.24f, 0.08f};
};

struct LogicVisualNodeStyle {
    std::string nodeKind;
    std::vector<LogicVisualPrimitiveDefinition> primitives;
};

struct LogicVisualLibrary {
    std::unordered_map<std::string, LogicVisualNodeStyle> nodeStyles;
    LogicVisualNodeStyle worldIoStyle{};
    LogicVisualPrimitiveDefinition wireSegmentStyle{};
    LogicVisualPrimitiveDefinition junctionStyle{};
};

struct LogicVisualPrimitiveInstance {
    std::string id;
    LogicVisualPrimitiveKind kind = LogicVisualPrimitiveKind::NodeBody;
    std::array<float, 3> worldPosition{0.0f, 0.0f, 0.0f};
    std::array<float, 3> worldRotationDegrees{0.0f, 0.0f, 0.0f};
    std::array<float, 3> worldScale{1.0f, 1.0f, 1.0f};
    std::array<float, 3> color{0.25f, 0.25f, 0.28f};
    std::array<float, 3> emissive{0.02f, 0.02f, 0.03f};
};

[[nodiscard]] LogicVisualLibrary BuildDefaultLogicVisualLibrary();

[[nodiscard]] const LogicVisualNodeStyle* FindLogicVisualNodeStyle(const LogicVisualLibrary& library,
                                                                   std::string_view nodeKind);

[[nodiscard]] std::vector<LogicVisualPrimitiveInstance> BuildLogicVisualNodeInstances(
    const LogicVisualLibrary& library,
    std::string_view nodeKind,
    std::string_view nodeId,
    const std::array<float, 3>& worldPosition,
    bool active);

[[nodiscard]] std::optional<LogicVisualPrimitiveInstance> BuildLogicVisualWireSegmentInstance(
    const LogicVisualLibrary& library,
    std::string_view segmentId,
    const std::array<float, 3>& worldPosition,
    const std::array<float, 3>& worldScale,
    bool active);

} // namespace ri::logic
