#include "RawIron/Scene/SceneStateIO.h"

#include "RawIron/Math/Vec3.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

namespace ri::scene {

namespace fs = std::filesystem;

bool SaveSceneNodeTransforms(const Scene& scene, const fs::path& outputPath) {
    fs::create_directories(outputPath.parent_path());

    std::ofstream stream(outputPath, std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }

    stream << "RAWIRON_SCENE_STATE_V1\n";
    stream << "node_count " << scene.NodeCount() << "\n";
    const auto& nodes = scene.Nodes();
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const auto& node = nodes[index];
        stream << "node " << index << " " << std::quoted(node.name) << " "
               << node.localTransform.position.x << " "
               << node.localTransform.position.y << " "
               << node.localTransform.position.z << " "
               << node.localTransform.rotationDegrees.x << " "
               << node.localTransform.rotationDegrees.y << " "
               << node.localTransform.rotationDegrees.z << " "
               << node.localTransform.scale.x << " "
               << node.localTransform.scale.y << " "
               << node.localTransform.scale.z << "\n";
    }
    return stream.good();
}

bool LoadSceneNodeTransforms(Scene& scene, const fs::path& inputPath) {
    std::ifstream stream(inputPath);
    if (!stream.is_open()) {
        return false;
    }

    std::string magic;
    std::getline(stream, magic);
    if (magic != "RAWIRON_SCENE_STATE_V1" && magic != "RAWIRON_EDITOR_STATE_V1") {
        return false;
    }

    std::string header;
    std::size_t nodeCount = 0;
    stream >> header >> nodeCount;
    if (!stream.good() || header != "node_count") {
        return false;
    }

    std::size_t loadedCount = 0;
    while (stream.good()) {
        std::string token;
        stream >> token;
        if (!stream.good()) {
            break;
        }
        if (token != "node") {
            return false;
        }

        std::size_t index = 0;
        std::string name;
        ri::math::Vec3 pos{};
        ri::math::Vec3 rot{};
        ri::math::Vec3 scale{};
        stream >> index >> std::quoted(name)
               >> pos.x >> pos.y >> pos.z
               >> rot.x >> rot.y >> rot.z
               >> scale.x >> scale.y >> scale.z;
        if (!stream.good()) {
            return false;
        }

        if (index < scene.NodeCount()) {
            Node& node = scene.GetNode(static_cast<int>(index));
            if (node.name == name) {
                node.localTransform.position = pos;
                node.localTransform.rotationDegrees = rot;
                node.localTransform.scale = scale;
                ++loadedCount;
            }
        }
    }

    return loadedCount > 0 && nodeCount >= loadedCount;
}

} // namespace ri::scene
