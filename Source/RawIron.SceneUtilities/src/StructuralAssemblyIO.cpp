#include "RawIron/Scene/StructuralAssemblyIO.h"

#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

namespace ri::scene {
namespace {

std::string Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::vector<std::string> SplitCsv(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream stream(line);
    std::string token;
    while (std::getline(stream, token, ',')) {
        tokens.push_back(Trim(token));
    }
    return tokens;
}

bool ParseFloat(const std::string& text, float& out) {
    try {
        out = std::stof(text);
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

bool ParseInt(const std::string& text, int& out) {
    try {
        out = std::stoi(text);
        return true;
    } catch (...) {
        return false;
    }
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

MaterialStyle ParseMaterialStyleLoose(std::string_view token, MaterialStyle fallback) {
    const std::string key = Lowercase(Trim(std::string(token)));
    if (key == "retro") return MaterialStyle::Retro;
    if (key == "layered") return MaterialStyle::Layered;
    if (key == "mixedmedia" || key == "mixed_media" || key == "mixed-media") return MaterialStyle::MixedMedia;
    if (key == "crystal") return MaterialStyle::Crystal;
    if (key == "standard") return MaterialStyle::Standard;
    return fallback;
}

MaterialWorkflow ParseMaterialWorkflowLoose(std::string_view token, MaterialWorkflow fallback) {
    const std::string key = Lowercase(Trim(std::string(token)));
    if (key == "specgloss" || key == "spec_gloss" || key == "spec-gloss") return MaterialWorkflow::SpecGloss;
    if (key == "metalrough" || key == "metal_rough" || key == "metal-rough") return MaterialWorkflow::MetalRough;
    return fallback;
}

StructuralBrushSemanticRole ParseStructuralBrushSemanticRoleLoose(std::string_view token,
                                                                  StructuralBrushSemanticRole fallback) {
    const std::string key = Lowercase(Trim(std::string(token)));
    if (key == "wall") return StructuralBrushSemanticRole::Wall;
    if (key == "floor") return StructuralBrushSemanticRole::Floor;
    if (key == "ceiling") return StructuralBrushSemanticRole::Ceiling;
    if (key == "pillar") return StructuralBrushSemanticRole::Pillar;
    if (key == "stair" || key == "stairs") return StructuralBrushSemanticRole::Stair;
    if (key == "portal") return StructuralBrushSemanticRole::Portal;
    if (key == "trim") return StructuralBrushSemanticRole::Trim;
    if (key == "cover") return StructuralBrushSemanticRole::Cover;
    if (key == "water") return StructuralBrushSemanticRole::Water;
    if (key == "trigger") return StructuralBrushSemanticRole::Trigger;
    if (key == "decor" || key == "decoration") return StructuralBrushSemanticRole::Decor;
    if (key == "volume") return StructuralBrushSemanticRole::Volume;
    if (key == "structure" || key == "structural") return StructuralBrushSemanticRole::Structure;
    return fallback;
}

StructuralBrushOperation ParseStructuralBrushOperationLoose(std::string_view token,
                                                            StructuralBrushOperation fallback) {
    const std::string key = Lowercase(Trim(std::string(token)));
    if (key == "solid") return StructuralBrushOperation::Solid;
    if (key == "subtract" || key == "subtractive") return StructuralBrushOperation::Subtract;
    if (key == "intersect" || key == "intersection") return StructuralBrushOperation::Intersect;
    if (key == "stamp") return StructuralBrushOperation::Stamp;
    if (key == "merge") return StructuralBrushOperation::Merge;
    if (key == "detail") return StructuralBrushOperation::Detail;
    if (key == "unspecified") return StructuralBrushOperation::Unspecified;
    return fallback;
}

StructuralBrushCollisionPolicy ParseStructuralBrushCollisionPolicyLoose(
    std::string_view token,
    StructuralBrushCollisionPolicy fallback) {
    const std::string key = Lowercase(Trim(std::string(token)));
    if (key == "solid") return StructuralBrushCollisionPolicy::Solid;
    if (key == "none" || key == "ignored") return StructuralBrushCollisionPolicy::None;
    if (key == "query") return StructuralBrushCollisionPolicy::Query;
    if (key == "player") return StructuralBrushCollisionPolicy::Player;
    if (key == "detail") return StructuralBrushCollisionPolicy::Detail;
    if (key == "custom") return StructuralBrushCollisionPolicy::Custom;
    return fallback;
}

StructuralBrushVisibilityPolicy ParseStructuralBrushVisibilityPolicyLoose(
    std::string_view token,
    StructuralBrushVisibilityPolicy fallback) {
    const std::string key = Lowercase(Trim(std::string(token)));
    if (key == "ignored" || key == "none") return StructuralBrushVisibilityPolicy::Ignored;
    if (key == "occluder") return StructuralBrushVisibilityPolicy::Occluder;
    if (key == "portal") return StructuralBrushVisibilityPolicy::Portal;
    if (key == "anti_portal" || key == "antiportal" || key == "anti-portal") {
        return StructuralBrushVisibilityPolicy::AntiPortal;
    }
    if (key == "transparent") return StructuralBrushVisibilityPolicy::Transparent;
    return fallback;
}

StructuralBrushNavigationPolicy ParseStructuralBrushNavigationPolicyLoose(
    std::string_view token,
    StructuralBrushNavigationPolicy fallback) {
    const std::string key = Lowercase(Trim(std::string(token)));
    if (key == "ignored" || key == "none") return StructuralBrushNavigationPolicy::Ignored;
    if (key == "walkable") return StructuralBrushNavigationPolicy::Walkable;
    if (key == "blocker") return StructuralBrushNavigationPolicy::Blocker;
    if (key == "jump") return StructuralBrushNavigationPolicy::Jump;
    if (key == "cover") return StructuralBrushNavigationPolicy::Cover;
    if (key == "ladder") return StructuralBrushNavigationPolicy::Ladder;
    return fallback;
}

StructuralBrushRebuildScope ParseStructuralBrushRebuildScopeLoose(std::string_view token,
                                                                  StructuralBrushRebuildScope fallback) {
    const std::string key = Lowercase(Trim(std::string(token)));
    if (key == "local") return StructuralBrushRebuildScope::Local;
    if (key == "region") return StructuralBrushRebuildScope::Region;
    if (key == "global") return StructuralBrushRebuildScope::Global;
    if (key == "manual") return StructuralBrushRebuildScope::Manual;
    return fallback;
}

ri::math::Vec3 ParseVec3(const std::vector<std::string>& tokens, std::size_t offset, const ri::math::Vec3& fallback) {
    if ((offset + 2U) >= tokens.size()) {
        return fallback;
    }
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (!ParseFloat(tokens[offset + 0U], x) || !ParseFloat(tokens[offset + 1U], y) || !ParseFloat(tokens[offset + 2U], z)) {
        return fallback;
    }
    return ri::math::Vec3{x, y, z};
}

void ApplyOptionalShapeOverrides(ri::structural::StructuralPrimitiveOptions& shape,
                                 const std::vector<std::string>& tokens) {
    if (tokens.size() > 18U) {
        float thickness = shape.thickness;
        if (ParseFloat(tokens[18], thickness)) {
            shape.thickness = thickness;
        }
    }
    if (tokens.size() > 19U) {
        float spanDegrees = shape.spanDegrees;
        if (ParseFloat(tokens[19], spanDegrees)) {
            shape.spanDegrees = spanDegrees;
        }
    }
    if (tokens.size() > 20U) {
        int radialSegments = shape.radialSegments;
        if (ParseInt(tokens[20], radialSegments)) {
            shape.radialSegments = radialSegments;
        }
    }
    if (tokens.size() > 21U && !tokens[21].empty()) {
        shape.archStyle = tokens[21];
    }
    if (tokens.size() > 22U) {
        int steps = shape.steps;
        if (ParseInt(tokens[22], steps)) {
            shape.steps = steps;
        }
    }
}

} // namespace

StructuralAssemblySpawnResult SpawnStructuralAssemblyFromCsv(Scene& scene,
                                                             const std::filesystem::path& csvPath,
                                                             const StructuralAssemblySpawnOptions& options) {
    StructuralAssemblySpawnResult result{};
    if (options.parent == kInvalidHandle) {
        return result;
    }

    std::ifstream input(csvPath);
    if (!input.is_open()) {
        return result;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::vector<std::string> tokens = SplitCsv(line);
        if (tokens.size() < 11U) {
            ++result.skippedCount;
            continue;
        }

        const std::string& typeToken = tokens[1];
        std::string_view structuralType = typeToken;
        ri::structural::StructuralPrimitiveOptions shape{};
        if (const std::optional<StructuralPrimitivePreset> preset = FindStructuralPreset(typeToken)) {
            structuralType = preset->structuralType;
            shape = ShapeFromStructuralPreset(*preset);
        } else {
            shape.radialSegments = 16;
            shape.sides = 16;
        }
        ApplyOptionalShapeOverrides(shape, tokens);

        StructuralBrushSpawnOptions brush{};
        brush.nodeName = tokens[0];
        brush.structuralType = structuralType;
        brush.shape = shape;
        brush.parent = options.parent;
        brush.metadata.brushId = tokens[0];
        brush.transform.position = ParseVec3(tokens, 2U, {});
        brush.transform.scale = ParseVec3(tokens, 5U, ri::math::Vec3{1.0f, 1.0f, 1.0f});
        brush.baseColor = ParseVec3(tokens, 8U, ri::math::Vec3{0.62f, 0.66f, 0.72f});
        brush.shadingModel = (tokens.size() > 11U && tokens[11] == "unlit") ? ShadingModel::Unlit : ShadingModel::Lit;
        if (tokens.size() > 12U && !tokens[12].empty() && tokens[12] != "-") {
            brush.baseColorTexture = tokens[12];
        }
        if (tokens.size() > 14U) {
            float tileX = 1.0f;
            float tileY = 1.0f;
            if (ParseFloat(tokens[13], tileX) && ParseFloat(tokens[14], tileY) && tileX > 0.0f && tileY > 0.0f) {
                brush.textureTiling = ri::math::Vec2{tileX, tileY};
            }
        }
        if (tokens.size() > 17U) {
            brush.transform.rotationDegrees = ParseVec3(tokens, 15U, {});
        }
        if (tokens.size() > 23U && !tokens[23].empty()) {
            brush.materialStyle = ParseMaterialStyleLoose(tokens[23], brush.materialStyle);
        }
        if (tokens.size() > 24U && !tokens[24].empty()) {
            brush.materialWorkflow = ParseMaterialWorkflowLoose(tokens[24], brush.materialWorkflow);
        }
        if (tokens.size() > 25U && !tokens[25].empty() && tokens[25] != "-") {
            brush.normalTexture = tokens[25];
        }
        if (tokens.size() > 26U && !tokens[26].empty() && tokens[26] != "-") {
            brush.ormTexture = tokens[26];
        }
        if (tokens.size() > 27U && !tokens[27].empty() && tokens[27] != "-") {
            brush.detailTexture = tokens[27];
        }
        if (tokens.size() > 28U && !tokens[28].empty() && tokens[28] != "-") {
            brush.emissiveTexture = tokens[28];
        }
        if (tokens.size() > 31U) {
            brush.emissiveColor = ParseVec3(tokens, 29U, brush.emissiveColor);
        }
        if (tokens.size() > 32U) {
            float roughness = brush.roughness;
            if (ParseFloat(tokens[32], roughness)) {
                brush.roughness = std::clamp(roughness, 0.0f, 1.0f);
            }
        }
        if (tokens.size() > 33U) {
            float metallic = brush.metallic;
            if (ParseFloat(tokens[33], metallic)) {
                brush.metallic = std::clamp(metallic, 0.0f, 1.0f);
            }
        }
        if (tokens.size() > 34U) {
            brush.metadata.role = ParseStructuralBrushSemanticRoleLoose(tokens[34], brush.metadata.role);
        }
        if (tokens.size() > 35U && !tokens[35].empty() && tokens[35] != "-") {
            brush.metadata.region = tokens[35];
        }
        if (tokens.size() > 36U) {
            brush.metadata.operation = ParseStructuralBrushOperationLoose(tokens[36], brush.metadata.operation);
        }
        if (tokens.size() > 37U) {
            brush.metadata.collision = ParseStructuralBrushCollisionPolicyLoose(tokens[37], brush.metadata.collision);
        }
        if (tokens.size() > 38U) {
            brush.metadata.visibility = ParseStructuralBrushVisibilityPolicyLoose(tokens[38], brush.metadata.visibility);
        }
        if (tokens.size() > 39U) {
            brush.metadata.navigation = ParseStructuralBrushNavigationPolicyLoose(tokens[39], brush.metadata.navigation);
        }
        if (tokens.size() > 40U) {
            brush.metadata.rebuildScope = ParseStructuralBrushRebuildScopeLoose(tokens[40], brush.metadata.rebuildScope);
        }
        brush.materialName = options.materialNamePrefix + "_" + tokens[0];

        if (AddStructuralBrushNode(scene, brush) == kInvalidHandle) {
            ++result.skippedCount;
        } else {
            ++result.spawnedCount;
        }
    }

    return result;
}

} // namespace ri::scene
