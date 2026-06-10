#include "EditorLevelExport.h"

#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace ri::editor {

namespace {

[[nodiscard]] std::string FormatFloat(const float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    std::string text = stream.str();
    while (text.size() > 1U && text.back() == '0' && text.find('.') != std::string::npos) {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

[[nodiscard]] bool ShouldSkipColliderNodeName(const std::string_view name) {
    if (name.rfind("Trigger_", 0) == 0) {
        return true;
    }
    static constexpr const char* kSkipFragments[] = {
        "Grid",
        "Orbit",
        "MainCamera",
        "Player",
        "Sun",
        "Beacon",
        "Axes",
        "Logic",
        "Wire",
    };
    for (const char* fragment : kSkipFragments) {
        if (name.find(fragment) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string ColliderNameForNode(const std::string& nodeName) {
    if (nodeName.size() >= 9U && nodeName.compare(nodeName.size() - 9U, 9U, "_collider") == 0) {
        return nodeName;
    }
    return nodeName + "_collider";
}

[[nodiscard]] std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    for (const char ch : line) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (ch == ',' && !inQuotes) {
            tokens.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    tokens.push_back(current);
    return tokens;
}

[[nodiscard]] bool ParseFloatToken(const std::string& token, float& out) {
    try {
        out = std::stof(token);
        return true;
    } catch (...) {
        return false;
    }
}

[[nodiscard]] bool NodeNameExists(const ri::scene::Scene& scene, const std::string_view name) {
    for (const ri::scene::Node& node : scene.Nodes()) {
        if (node.name == name) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool LightNameExists(const ri::scene::Scene& scene, const std::string_view name) {
    for (const int handle : ri::scene::CollectLightNodes(scene)) {
        const ri::scene::Node& node = scene.GetNode(handle);
        if (node.name == name) {
            return true;
        }
        if (node.light != ri::scene::kInvalidHandle
            && static_cast<std::size_t>(node.light) < scene.LightCount()) {
            if (scene.GetLight(node.light).name == name) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] std::string MakeUniqueImportName(const ri::scene::Scene& scene, const std::string& baseName) {
    if (!NodeNameExists(scene, baseName)) {
        return baseName;
    }
    for (int suffix = 1; suffix < 10000; ++suffix) {
        const std::string candidate = baseName + "_import" + std::to_string(suffix);
        if (!NodeNameExists(scene, candidate)) {
            return candidate;
        }
    }
    return baseName + "_import";
}

[[nodiscard]] ri::scene::LightType ParseLightTypeToken(const std::string& token) {
    if (token == "point") {
        return ri::scene::LightType::Point;
    }
    if (token == "spot") {
        return ri::scene::LightType::Spot;
    }
    return ri::scene::LightType::Directional;
}

[[nodiscard]] ri::math::Vec3 RotationDegreesFromForward(const ri::math::Vec3& forward) {
    const ri::math::Vec3 dir = ri::math::Normalize(forward);
    const float yawRadians = std::atan2(dir.x, dir.z);
    const float horizontal = std::sqrt((dir.x * dir.x) + (dir.z * dir.z));
    const float pitchRadians = -std::atan2(dir.y, std::max(horizontal, 1.0e-6f));
    constexpr float kRadToDeg = 180.0f / ri::math::kPi;
    return ri::math::Vec3{pitchRadians * kRadToDeg, yawRadians * kRadToDeg, 0.0f};
}

[[nodiscard]] float ReadTokenFloat(const std::vector<std::string>& tokens,
                                   const std::size_t index,
                                   const float fallback) {
    if (index >= tokens.size()) {
        return fallback;
    }
    float value = fallback;
    (void)ParseFloatToken(tokens[index], value);
    return value;
}

[[nodiscard]] int FindHeaderColumn(const std::vector<std::string>& header, const std::initializer_list<const char*> names) {
    for (std::size_t index = 0; index < header.size(); ++index) {
        for (const char* name : names) {
            if (header[index] == name) {
                return static_cast<int>(index);
            }
        }
    }
    return -1;
}

[[nodiscard]] std::string TriggerNodeNameFromId(const std::string& triggerId) {
    if (triggerId.rfind("Trigger_", 0) == 0) {
        return triggerId;
    }
    return "Trigger_" + triggerId;
}

[[nodiscard]] bool SpawnTriggerVolumeFromBounds(ri::scene::Scene& scene,
                                              const int worldRootNodeHandle,
                                              const std::string& nodeName,
                                              const ri::math::Vec3& boundsMin,
                                              const ri::math::Vec3& boundsMax) {
    if (NodeNameExists(scene, nodeName)) {
        return false;
    }
    const ri::math::Vec3 center{
        (boundsMin.x + boundsMax.x) * 0.5f,
        (boundsMin.y + boundsMax.y) * 0.5f,
        (boundsMin.z + boundsMax.z) * 0.5f,
    };
    const ri::math::Vec3 size{
        std::max(boundsMax.x - boundsMin.x, 0.1f),
        std::max(boundsMax.y - boundsMin.y, 0.1f),
        std::max(boundsMax.z - boundsMin.z, 0.1f),
    };

    ri::scene::PrimitiveNodeOptions options{};
    options.parent = worldRootNodeHandle;
    options.nodeName = nodeName;
    options.primitive = ri::scene::PrimitiveType::Cube;
    options.shadingModel = ri::scene::ShadingModel::Unlit;
    options.materialName = "import_trigger_" + nodeName;
    options.baseColor = ri::math::Vec3{0.15f, 0.75f, 0.28f};
    options.opacity = 0.45f;
    options.transparent = true;
    options.transform.position = center;
    options.transform.scale = size;
    (void)ri::scene::AddPrimitiveNode(scene, options);
    return true;
}

} // namespace

LevelExportResult TryExportAssemblyLightingCsv(const ri::scene::Scene& scene,
                                               const std::filesystem::path& outputPath) {
    LevelExportResult result{};
    std::error_code ec{};
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
        result.error = "could not create lighting export folder";
        return result;
    }

    std::ofstream stream(outputPath, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        result.error = "could not open lighting export file";
        return result;
    }

    stream << "name,type,px,py,pz,dx,dy,dz,r,g,b,intensity,range\n";
    for (const int lightNodeHandle : ri::scene::CollectLightNodes(scene)) {
        const ri::scene::Node& node = scene.GetNode(lightNodeHandle);
        if (node.light == ri::scene::kInvalidHandle
            || static_cast<std::size_t>(node.light) >= scene.LightCount()) {
            continue;
        }
        const ri::scene::Light& light = scene.GetLight(node.light);
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(lightNodeHandle);
        const ri::math::Vec3 position = ri::math::ExtractTranslation(world);
        const ri::math::Vec3 direction = ri::math::ExtractForward(world);
        stream << (light.name.empty() ? node.name : light.name) << ","
               << ri::scene::ToString(light.type) << ","
               << FormatFloat(position.x) << ","
               << FormatFloat(position.y) << ","
               << FormatFloat(position.z) << ","
               << FormatFloat(direction.x) << ","
               << FormatFloat(direction.y) << ","
               << FormatFloat(direction.z) << ","
               << FormatFloat(light.color.x) << ","
               << FormatFloat(light.color.y) << ","
               << FormatFloat(light.color.z) << ","
               << FormatFloat(light.intensity) << ","
               << FormatFloat(light.range) << "\n";
        result.rowCount += 1U;
    }

    if (result.rowCount == 0U) {
        stream << "sun,directional,0,0,0,-0.4,-1.0,0.2,0.92,0.94,1.0,1.35,0\n";
        result.rowCount = 1U;
    }

    if (!stream.good()) {
        result.error = "lighting export write failed";
        return result;
    }

    result.success = true;
    return result;
}

LevelExportResult TryExportAssemblyCollidersCsv(const ri::scene::Scene& scene,
                                                const std::filesystem::path& outputPath) {
    LevelExportResult result{};
    std::error_code ec{};
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
        result.error = "could not create collider export folder";
        return result;
    }

    std::ofstream stream(outputPath, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        result.error = "could not open collider export file";
        return result;
    }

    stream << "name,type,px,py,pz,sx,sy,sz\n";
    for (const int handle : ri::scene::CollectRenderableNodes(scene)) {
        const ri::scene::Node& node = scene.GetNode(handle);
        if (ShouldSkipColliderNodeName(node.name)) {
            continue;
        }
        if (node.mesh == ri::scene::kInvalidHandle) {
            continue;
        }
        const ri::scene::Mesh& mesh = scene.GetMesh(node.mesh);
        if (mesh.primitive != ri::scene::PrimitiveType::Cube
            && mesh.primitive != ri::scene::PrimitiveType::Plane) {
            continue;
        }
        const std::optional<ri::scene::WorldBounds> bounds =
            ri::scene::ComputeNodeWorldBounds(scene, handle, false);
        if (!bounds.has_value()) {
            continue;
        }
        const ri::math::Vec3 center{
            (bounds->min.x + bounds->max.x) * 0.5f,
            (bounds->min.y + bounds->max.y) * 0.5f,
            (bounds->min.z + bounds->max.z) * 0.5f,
        };
        const ri::math::Vec3 halfExtents{
            (bounds->max.x - bounds->min.x) * 0.5f,
            (bounds->max.y - bounds->min.y) * 0.5f,
            (bounds->max.z - bounds->min.z) * 0.5f,
        };
        stream << ColliderNameForNode(node.name) << ",box,"
               << FormatFloat(center.x) << ","
               << FormatFloat(center.y) << ","
               << FormatFloat(center.z) << ","
               << FormatFloat(halfExtents.x) << ","
               << FormatFloat(halfExtents.y) << ","
               << FormatFloat(halfExtents.z) << "\n";
        result.rowCount += 1U;
    }

    if (result.rowCount == 0U) {
        result.error = "no cube/plane geometry found for collider export";
        return result;
    }

    if (!stream.good()) {
        result.error = "collider export write failed";
        return result;
    }

    result.success = true;
    return result;
}

LevelImportResult TryImportAssemblyLightingCsv(ri::scene::Scene& scene,
                                               const int worldRootNodeHandle,
                                               const std::filesystem::path& inputPath) {
    LevelImportResult result{};
    if (worldRootNodeHandle < 0 || static_cast<std::size_t>(worldRootNodeHandle) >= scene.NodeCount()) {
        result.error = "invalid world root for lighting import";
        return result;
    }

    std::ifstream input(inputPath);
    if (!input.is_open()) {
        result.error = "lighting CSV not found";
        return result;
    }

    std::string line{};
    bool headerSeen = false;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!headerSeen) {
            headerSeen = true;
            continue;
        }
        const std::vector<std::string> tokens = SplitCsvLine(line);
        if (tokens.size() < 13U) {
            result.skippedCount += 1U;
            continue;
        }

        std::string lightName = tokens[0];
        if (lightName.empty()) {
            result.skippedCount += 1U;
            continue;
        }
        if (LightNameExists(scene, lightName)) {
            result.skippedCount += 1U;
            continue;
        }
        if (NodeNameExists(scene, lightName)) {
            lightName = MakeUniqueImportName(scene, lightName);
        }

        const ri::math::Vec3 position{
            ReadTokenFloat(tokens, 2U, 0.0f),
            ReadTokenFloat(tokens, 3U, 0.0f),
            ReadTokenFloat(tokens, 4U, 0.0f),
        };
        const ri::math::Vec3 direction{
            ReadTokenFloat(tokens, 5U, 0.0f),
            ReadTokenFloat(tokens, 6U, -1.0f),
            ReadTokenFloat(tokens, 7U, 0.0f),
        };

        ri::scene::LightNodeOptions options{};
        options.parent = worldRootNodeHandle;
        options.nodeName = lightName;
        options.transform.position = position;
        if (ri::math::Length(direction) > 1.0e-4f) {
            options.transform.rotationDegrees = RotationDegreesFromForward(direction);
        }
        options.light = ri::scene::Light{
            .name = lightName,
            .type = ParseLightTypeToken(tokens[1]),
            .color = ri::math::Vec3{
                ReadTokenFloat(tokens, 8U, 1.0f),
                ReadTokenFloat(tokens, 9U, 1.0f),
                ReadTokenFloat(tokens, 10U, 1.0f),
            },
            .intensity = ReadTokenFloat(tokens, 11U, 1.0f),
            .range = ReadTokenFloat(tokens, 12U, 10.0f),
        };
        (void)ri::scene::AddLightNode(scene, options);
        result.importedCount += 1U;
    }

    result.success = true;
    return result;
}

LevelImportResult TryImportAssemblyCollidersCsv(ri::scene::Scene& scene,
                                                const int worldRootNodeHandle,
                                                const std::filesystem::path& inputPath) {
    LevelImportResult result{};
    if (worldRootNodeHandle < 0 || static_cast<std::size_t>(worldRootNodeHandle) >= scene.NodeCount()) {
        result.error = "invalid world root for collider import";
        return result;
    }

    std::ifstream input(inputPath);
    if (!input.is_open()) {
        result.error = "colliders CSV not found";
        return result;
    }

    std::string line{};
    bool headerSeen = false;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (!headerSeen) {
            headerSeen = true;
            continue;
        }
        const std::vector<std::string> tokens = SplitCsvLine(line);
        if (tokens.size() < 8U) {
            result.skippedCount += 1U;
            continue;
        }

        std::string nodeName = tokens[0];
        if (nodeName.empty()) {
            result.skippedCount += 1U;
            continue;
        }
        if (NodeNameExists(scene, nodeName)) {
            result.skippedCount += 1U;
            continue;
        }

        const ri::math::Vec3 center{
            ReadTokenFloat(tokens, 2U, 0.0f),
            ReadTokenFloat(tokens, 3U, 0.0f),
            ReadTokenFloat(tokens, 4U, 0.0f),
        };
        const ri::math::Vec3 halfExtents{
            std::max(ReadTokenFloat(tokens, 5U, 0.5f), 0.05f),
            std::max(ReadTokenFloat(tokens, 6U, 0.5f), 0.05f),
            std::max(ReadTokenFloat(tokens, 7U, 0.5f), 0.05f),
        };

        ri::scene::PrimitiveNodeOptions options{};
        options.parent = worldRootNodeHandle;
        options.nodeName = nodeName;
        options.primitive = ri::scene::PrimitiveType::Cube;
        options.shadingModel = ri::scene::ShadingModel::Unlit;
        options.materialName = "import_collider_" + nodeName;
        options.baseColor = ri::math::Vec3{0.42f, 0.48f, 0.56f};
        options.opacity = 0.35f;
        options.transparent = true;
        options.transform.position = center;
        options.transform.scale = halfExtents * 2.0f;
        (void)ri::scene::AddPrimitiveNode(scene, options);
        result.importedCount += 1U;
    }

    result.success = true;
    return result;
}

LevelExportResult TryExportAssemblyTriggersCsv(const ri::scene::Scene& scene,
                                               const std::filesystem::path& outputPath) {
    LevelExportResult result{};
    std::error_code ec{};
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
        result.error = "could not create trigger export folder";
        return result;
    }

    std::ofstream stream(outputPath, std::ios::out | std::ios::trunc);
    if (!stream.is_open()) {
        result.error = "could not open trigger export file";
        return result;
    }

    stream << "trigger_id,event_type,min_x,min_y,min_z,max_x,max_y,max_z,param\n";
    for (std::size_t index = 0; index < scene.NodeCount(); ++index) {
        const ri::scene::Node& node = scene.GetNode(static_cast<int>(index));
        if (node.name.rfind("Trigger_", 0) != 0) {
            continue;
        }
        if (node.mesh == ri::scene::kInvalidHandle) {
            continue;
        }
        const ri::scene::Mesh& mesh = scene.GetMesh(node.mesh);
        if (mesh.primitive != ri::scene::PrimitiveType::Cube) {
            continue;
        }
        const std::optional<ri::scene::WorldBounds> bounds =
            ri::scene::ComputeNodeWorldBounds(scene, static_cast<int>(index), false);
        if (!bounds.has_value()) {
            continue;
        }
        stream << node.name
               << ",generic_trigger_volume,"
               << FormatFloat(bounds->min.x) << ","
               << FormatFloat(bounds->min.y) << ","
               << FormatFloat(bounds->min.z) << ","
               << FormatFloat(bounds->max.x) << ","
               << FormatFloat(bounds->max.y) << ","
               << FormatFloat(bounds->max.z) << ","
               << "\n";
        result.rowCount += 1U;
    }

    if (!stream.good()) {
        result.error = "trigger export write failed";
        return result;
    }
    if (result.rowCount == 0U) {
        result.error = "no Trigger_* cube nodes found to export";
        return result;
    }

    result.success = true;
    return result;
}

LevelImportResult TryImportAssemblyTriggersCsv(ri::scene::Scene& scene,
                                               const int worldRootNodeHandle,
                                               const std::filesystem::path& inputPath) {
    LevelImportResult result{};
    if (worldRootNodeHandle < 0 || static_cast<std::size_t>(worldRootNodeHandle) >= scene.NodeCount()) {
        result.error = "invalid world root for trigger import";
        return result;
    }

    std::ifstream input(inputPath);
    if (!input.is_open()) {
        result.error = "triggers CSV not found";
        return result;
    }

    std::string headerLine{};
    if (!std::getline(input, headerLine)) {
        result.error = "triggers CSV is empty";
        return result;
    }
    const std::vector<std::string> header = SplitCsvLine(headerLine);
    const int idCol = FindHeaderColumn(header, {"trigger_id", "name", "id"});
    const int minXCol = FindHeaderColumn(header, {"min_x"});
    const int minYCol = FindHeaderColumn(header, {"min_y"});
    const int minZCol = FindHeaderColumn(header, {"min_z"});
    const int maxXCol = FindHeaderColumn(header, {"max_x"});
    const int maxYCol = FindHeaderColumn(header, {"max_y"});
    const int maxZCol = FindHeaderColumn(header, {"max_z"});
    const int pxCol = FindHeaderColumn(header, {"px"});
    const int pyCol = FindHeaderColumn(header, {"py"});
    const int pzCol = FindHeaderColumn(header, {"pz"});
    const int sxCol = FindHeaderColumn(header, {"sx"});
    const int syCol = FindHeaderColumn(header, {"sy"});
    const int szCol = FindHeaderColumn(header, {"sz"});
    const bool boundsFormat = minXCol >= 0 && maxXCol >= 0;
    const bool centerFormat = pxCol >= 0 && sxCol >= 0;
    if (idCol < 0 || (!boundsFormat && !centerFormat)) {
        result.error = "unsupported triggers CSV header";
        return result;
    }

    std::string line{};
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> tokens = SplitCsvLine(line);
        if (idCol >= static_cast<int>(tokens.size()) || tokens[static_cast<std::size_t>(idCol)].empty()) {
            result.skippedCount += 1U;
            continue;
        }

        const std::string nodeName = TriggerNodeNameFromId(tokens[static_cast<std::size_t>(idCol)]);
        ri::math::Vec3 boundsMin{};
        ri::math::Vec3 boundsMax{};
        if (boundsFormat) {
            boundsMin = ri::math::Vec3{
                ReadTokenFloat(tokens, static_cast<std::size_t>(minXCol), 0.0f),
                ReadTokenFloat(tokens, static_cast<std::size_t>(minYCol), 0.0f),
                ReadTokenFloat(tokens, static_cast<std::size_t>(minZCol), 0.0f),
            };
            boundsMax = ri::math::Vec3{
                ReadTokenFloat(tokens, static_cast<std::size_t>(maxXCol), 0.0f),
                ReadTokenFloat(tokens, static_cast<std::size_t>(maxYCol), 0.0f),
                ReadTokenFloat(tokens, static_cast<std::size_t>(maxZCol), 0.0f),
            };
        } else {
            const ri::math::Vec3 center{
                ReadTokenFloat(tokens, static_cast<std::size_t>(pxCol), 0.0f),
                ReadTokenFloat(tokens, static_cast<std::size_t>(pyCol), 0.0f),
                ReadTokenFloat(tokens, static_cast<std::size_t>(pzCol), 0.0f),
            };
            const ri::math::Vec3 size{
                std::max(ReadTokenFloat(tokens, static_cast<std::size_t>(sxCol), 1.0f), 0.1f),
                std::max(ReadTokenFloat(tokens, static_cast<std::size_t>(syCol), 1.0f), 0.1f),
                std::max(ReadTokenFloat(tokens, static_cast<std::size_t>(szCol), 1.0f), 0.1f),
            };
            boundsMin = center - (size * 0.5f);
            boundsMax = center + (size * 0.5f);
        }

        if (SpawnTriggerVolumeFromBounds(scene, worldRootNodeHandle, nodeName, boundsMin, boundsMax)) {
            result.importedCount += 1U;
        } else {
            result.skippedCount += 1U;
        }
    }

    result.success = true;
    return result;
}

} // namespace ri::editor
