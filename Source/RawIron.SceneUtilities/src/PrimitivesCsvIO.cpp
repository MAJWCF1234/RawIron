#include "RawIron/Scene/PrimitivesCsvIO.h"

#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/SceneUtils.h"

#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_map>
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

std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> tokens{};
    std::string current{};
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
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
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

[[nodiscard]] std::string NormalizeColumnName(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return normalized;
}

using CsvColumnMap = std::unordered_map<std::string, std::size_t>;

[[nodiscard]] CsvColumnMap BuildColumnMap(const std::vector<std::string>& tokens) {
    CsvColumnMap columns;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const std::string name = NormalizeColumnName(tokens[index]);
        if (!name.empty()) {
            columns.try_emplace(name, index);
        }
    }
    return columns;
}

[[nodiscard]] std::optional<std::size_t> FindColumn(
    const CsvColumnMap& columns,
    const std::initializer_list<std::string_view> aliases) {
    for (const std::string_view alias : aliases) {
        const auto found = columns.find(std::string(alias));
        if (found != columns.end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool IsAssemblyHeader(const CsvColumnMap& columns) {
    return FindColumn(columns, {"name", "id"}).has_value()
        && FindColumn(columns, {"type", "primitive"}).has_value()
        && FindColumn(columns, {"x", "px", "posx", "positionx"}).has_value();
}

[[nodiscard]] bool NodeNameExists(const Scene& scene, std::string_view name) {
    for (const Node& node : scene.Nodes()) {
        if (node.name == name) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string MakeUniqueImportName(const Scene& scene, std::string baseName) {
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

bool TryImportAssemblyPrimitivesCsv(Scene& scene,
                                    const int worldRootNodeHandle,
                                    const std::filesystem::path& inputPath,
                                    AssemblyPrimitivesImportResult* result,
                                    std::string* errorMessage) {
    if (worldRootNodeHandle < 0 || static_cast<std::size_t>(worldRootNodeHandle) >= scene.NodeCount()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Invalid world root handle for CSV import.";
        }
        return false;
    }

    std::ifstream input(inputPath);
    if (!input.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Could not open CSV: " + inputPath.string();
        }
        return false;
    }

    AssemblyPrimitivesImportResult localResult{};
    CsvColumnMap columns{};
    std::string line{};
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        if (line[0] == '#') {
            std::string headerCandidate = line.substr(1);
            const std::size_t firstText = headerCandidate.find_first_not_of(" \t");
            if (firstText != std::string::npos) {
                headerCandidate.erase(0, firstText);
                const CsvColumnMap candidate = BuildColumnMap(SplitCsvLine(headerCandidate));
                if (IsAssemblyHeader(candidate)) {
                    columns = candidate;
                }
            }
            continue;
        }
        const std::vector<std::string> tokens = SplitCsvLine(line);
        const CsvColumnMap possibleHeader = BuildColumnMap(tokens);
        if (IsAssemblyHeader(possibleHeader)) {
            columns = possibleHeader;
            continue;
        }
        if (tokens.size() < 2U) {
            localResult.skippedRows += 1;
            continue;
        }

        const auto column = [&columns, &tokens](const std::initializer_list<std::string_view> aliases,
                                                const std::size_t fallback) {
            const std::optional<std::size_t> mapped = FindColumn(columns, aliases);
            if (mapped.has_value()) {
                return *mapped;
            }
            return columns.empty() ? fallback : tokens.size();
        };
        const std::size_t nameIndex = column({"name", "id"}, 0U);
        const std::size_t typeIndex = column({"type", "primitive"}, 1U);
        if (nameIndex >= tokens.size() || typeIndex >= tokens.size()) {
            localResult.skippedRows += 1;
            continue;
        }

        std::string nodeName = tokens[nameIndex];
        if (nodeName.empty()) {
            localResult.skippedRows += 1;
            continue;
        }
        if (NodeNameExists(scene, nodeName)) {
            nodeName = MakeUniqueImportName(scene, nodeName);
            localResult.renamedCount += 1;
        }

        const std::string& typeToken = tokens[typeIndex];
        const PrimitiveType primitive =
            (typeToken == "plane") ? PrimitiveType::Plane : PrimitiveType::Cube;

        const auto readFloat = [&](const std::size_t index, const float fallback) {
            float value = fallback;
            if (index < tokens.size()) {
                (void)ParseFloatToken(tokens[index], value);
            }
            return value;
        };

        PrimitiveNodeOptions options{};
        options.parent = worldRootNodeHandle;
        options.nodeName = nodeName;
        options.primitive = primitive;
        const std::size_t materialIndex = column({"material", "materialname"}, tokens.size());
        options.materialName = materialIndex < tokens.size() && !tokens[materialIndex].empty()
            ? tokens[materialIndex]
            : "import_" + nodeName;
        options.transform.position = ri::math::Vec3{
            readFloat(column({"x", "px", "posx", "positionx"}, 2U), 0.0f),
            readFloat(column({"y", "py", "posy", "positiony"}, 3U), 0.0f),
            readFloat(column({"z", "pz", "posz", "positionz"}, 4U), 0.0f),
        };
        options.transform.scale = ri::math::Vec3{
            std::max(readFloat(column({"sx", "scalex"}, 5U), 1.0f), 0.01f),
            std::max(readFloat(column({"sy", "scaley"}, 6U), 1.0f), 0.01f),
            std::max(readFloat(column({"sz", "scalez"}, 7U), 1.0f), 0.01f),
        };
        options.baseColor = ri::math::Vec3{
            readFloat(column({"r", "colorr", "colorred"}, 8U), 0.7f),
            readFloat(column({"g", "colorg", "colorgreen"}, 9U), 0.7f),
            readFloat(column({"b", "colorb", "colorblue"}, 10U), 0.7f),
        };
        const std::size_t shadingIndex = column({"shading", "shadingmodel"}, 11U);
        if (shadingIndex < tokens.size() && tokens[shadingIndex] == "unlit") {
            options.shadingModel = ShadingModel::Unlit;
        }
        const std::size_t textureIndex = column({"texture", "basecolortexture"}, 12U);
        if (textureIndex < tokens.size() && !tokens[textureIndex].empty() && tokens[textureIndex] != "-") {
            options.baseColorTexture = tokens[textureIndex];
        }
        const std::size_t tileXIndex = column({"tx", "tilex", "tilingx"}, 13U);
        const std::size_t tileYIndex = column({"ty", "tiley", "tilingy"}, 14U);
        if (tileXIndex < tokens.size() || tileYIndex < tokens.size()) {
            options.textureTiling = ri::math::Vec2{
                std::max(readFloat(tileXIndex, 1.0f), 0.01f),
                std::max(readFloat(tileYIndex, 1.0f), 0.01f),
            };
        }
        options.transform.rotationDegrees = ri::math::Vec3{
            readFloat(column({"rx", "rotx", "rotationx"}, 15U), 0.0f),
            readFloat(column({"ry", "roty", "rotationy"}, 16U), 0.0f),
            readFloat(column({"rz", "rotz", "rotationz"}, 17U), 0.0f),
        };

        (void)AddPrimitiveNode(scene, options);
        localResult.spawnedCount += 1;
    }

    if (localResult.spawnedCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "No primitive rows imported from: " + inputPath.string();
        }
        return false;
    }

    if (result != nullptr) {
        *result = localResult;
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

} // namespace ri::scene
