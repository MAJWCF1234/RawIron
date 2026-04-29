#include "RawIron/Scene/PrimitivesCsvIO.h"

#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneUtils.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace ri::scene {

namespace {

[[nodiscard]] bool ShouldSkipExportNodeName(std::string_view name) {
    /// Keep in sync with editor tooling — skip rigs, orbit preview, helpers.
    static constexpr const char* kSkipFragments[] = {
        "Grid",
        "Orbit",
        "MainCamera",
        "Player",
        "Sun",
        "Beacon",
        "Water",
        "acid",
        "crate",
        "Floating90s",
        "Prototype",
        "Terrain",
    };
    for (const char* frag : kSkipFragments) {
        if (name.find(frag) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] ri::math::Vec3 ExtractEulerDegreesXYZ(const ri::math::Mat4& wm) {
    ri::math::Vec3 scale = ri::math::ExtractScale(wm);
    const float sx = std::max(scale.x, 1.0e-8f);
    const float sy = std::max(scale.y, 1.0e-8f);
    const float sz = std::max(scale.z, 1.0e-8f);

    const float r00 = wm.m[0][0] / sx;
    const float r10 = wm.m[1][0] / sx;
    const float r20 = wm.m[2][0] / sx;
    const float r21 = wm.m[2][1] / sy;
    const float r22 = wm.m[2][2] / sz;

    const float cy = std::sqrt(r00 * r00 + r10 * r10);
    const float rxRad = std::atan2(r21, r22);
    const float ryRad = std::atan2(-r20, cy);
    const float rzRad = std::atan2(r10, r00);

    constexpr float kRadToDeg = 180.0f / ri::math::kPi;
    return ri::math::Vec3{rxRad * kRadToDeg, ryRad * kRadToDeg, rzRad * kRadToDeg};
}

[[nodiscard]] std::string EscapeCsvToken(std::string_view text) {
    if (text.find(',') == std::string_view::npos && text.find('"') == std::string_view::npos) {
        return std::string(text);
    }
    std::string out = "\"";
    for (char ch : text) {
        if (ch == '"') {
            out += "\"\"";
        } else {
            out += ch;
        }
    }
    out += '"';
    return out;
}

[[nodiscard]] std::string FormatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

[[nodiscard]] std::string RowFromNode(const Scene& scene,
                                      int handle,
                                      int worldRootHandle,
                                      const Mesh& mesh,
                                      const Material& material) {
    ri::math::Vec3 position{};
    ri::math::Vec3 rotationDegrees{};
    ri::math::Vec3 scale{1.0f, 1.0f, 1.0f};

    const Node& node = scene.GetNode(handle);
    if (node.parent == worldRootHandle) {
        position = node.localTransform.position;
        rotationDegrees = node.localTransform.rotationDegrees;
        scale = node.localTransform.scale;
    } else {
        const ri::math::Mat4 world = scene.ComputeWorldMatrix(handle);
        position = ri::math::ExtractTranslation(world);
        scale = ri::math::ExtractScale(world);
        rotationDegrees = ExtractEulerDegreesXYZ(world);
    }

    const std::string typeLabel = mesh.primitive == PrimitiveType::Plane ? "plane" : "cube";
    const std::string shadingLabel = material.shadingModel == ShadingModel::Unlit ? "unlit" : "lit";
    const std::string texture =
        material.baseColorTexture.empty() ? "-" : material.baseColorTexture;

    std::ostringstream line;
    line << EscapeCsvToken(node.name) << ',' << typeLabel << ','
         << FormatFloat(position.x) << ',' << FormatFloat(position.y) << ',' << FormatFloat(position.z)
         << ',' << FormatFloat(scale.x) << ',' << FormatFloat(scale.y) << ',' << FormatFloat(scale.z)
         << ',' << FormatFloat(material.baseColor.x) << ',' << FormatFloat(material.baseColor.y)
         << ',' << FormatFloat(material.baseColor.z) << ',' << shadingLabel << ',' << texture << ','
         << FormatFloat(material.textureTiling.x) << ',' << FormatFloat(material.textureTiling.y)
         << ',' << FormatFloat(rotationDegrees.x) << ',' << FormatFloat(rotationDegrees.y) << ','
         << FormatFloat(rotationDegrees.z);
    return line.str();
}

} // namespace

bool TryExportAssemblyPrimitivesCsv(const Scene& scene,
                                    const int worldRootNodeHandle,
                                    const std::filesystem::path& outputPath,
                                    std::string* errorMessage) {
    if (worldRootNodeHandle < 0 || static_cast<std::size_t>(worldRootNodeHandle) >= scene.NodeCount()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid world root handle for CSV export.";
        }
        return false;
    }

    std::ostringstream body;
    body << "# Exported by RawIron Editor — cube/plane primitives compatible with "
            "Games/*/levels/assembly.primitives.csv\n";
    body << "# name,type,posX,posY,posZ,scaleX,scaleY,scaleZ,colorR,colorG,colorB,shading,texture,"
            "tileX,tileY,rotX,rotY,rotZ\n";

    const std::vector<int> renderables = CollectRenderableNodes(scene);
    int rows = 0;
    for (const int handle : renderables) {
        const Node& node = scene.GetNode(handle);
        if (ShouldSkipExportNodeName(node.name)) {
            continue;
        }
        if (node.mesh == kInvalidHandle || node.material == kInvalidHandle) {
            continue;
        }
        const Mesh& mesh = scene.GetMesh(node.mesh);
        if (mesh.primitive != PrimitiveType::Cube && mesh.primitive != PrimitiveType::Plane) {
            continue;
        }
        const Material& material = scene.GetMaterial(node.material);
        body << RowFromNode(scene, handle, worldRootNodeHandle, mesh, material) << '\n';
        ++rows;
    }

    if (rows == 0) {
        if (errorMessage != nullptr) {
            *errorMessage =
                "No cube/plane primitives found to export (check mesh types and filter rules).";
        }
        return false;
    }

    if (!ri::core::detail::WriteTextFile(outputPath, body.str())) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to write file: " + outputPath.string();
        }
        return false;
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

} // namespace ri::scene
