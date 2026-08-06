#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Logic/LogicKitManifest.h"
#include "RawIron/Logic/LogicVisualPrimitives.h"
#include "RawIron/Logic/LogicAuthoringEditorIO.h"
#include "RawIron/Logic/LogicAuthoringWireLayout.h"

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/ModelLoader.h"
#include "RawIron/Scene/PrimitivesCsvIO.h"
#include "RawIron/Scene/StructuralAssemblyIO.h"
#include "RawIron/Scene/StructuralPrimitiveBundle.h"
#include "RawIron/Scene/SceneUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ri::games::liminal {

namespace {

namespace fs = std::filesystem;

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

std::map<std::string, ri::math::Vec3, std::less<>> LoadLiminalPalette(const fs::path& gameRoot) {
    std::map<std::string, ri::math::Vec3, std::less<>> palette;
    const fs::path path = gameRoot / "assets" / "palette.ripalette";
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return palette;
    }

    std::string line;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> tokens = SplitCsv(line);
        if (tokens.size() < 4U) {
            continue;
        }
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        if (!ParseFloat(tokens[1], r) || !ParseFloat(tokens[2], g) || !ParseFloat(tokens[3], b)) {
            continue;
        }
        palette[tokens[0]] = ri::math::Vec3{
            std::clamp(r, 0.0f, 1.0f),
            std::clamp(g, 0.0f, 1.0f),
            std::clamp(b, 0.0f, 1.0f),
        };
    }
    return palette;
}

ri::math::Vec3 PaletteOr(const std::map<std::string, ri::math::Vec3, std::less<>>& palette,
                         std::string_view key,
                         const ri::math::Vec3& fallback) {
    const auto it = palette.find(std::string(key));
    return it == palette.end() ? fallback : it->second;
}

struct SceneMoodHandles {
    int fractalGate = ri::scene::kInvalidHandle;
    int neonSun = ri::scene::kInvalidHandle;
    int neonSunShell = ri::scene::kInvalidHandle;
    int pulsingBrainSphere = ri::scene::kInvalidHandle;
    int brainHalo = ri::scene::kInvalidHandle;
    int checkerObeliskLeft = ri::scene::kInvalidHandle;
    int checkerObeliskRight = ri::scene::kInvalidHandle;
    int glitchPyramid = ri::scene::kInvalidHandle;
    int gateLight = ri::scene::kInvalidHandle;
    int brainLight = ri::scene::kInvalidHandle;
    int sunLight = ri::scene::kInvalidHandle;
};

SceneMoodHandles ResolveSceneMoodHandles(const ri::scene::Scene& scene) {
    SceneMoodHandles handles{};
    handles.fractalGate = ri::scene::FindNodeByName(scene, "FractalGate").value_or(ri::scene::kInvalidHandle);
    handles.neonSun = ri::scene::FindNodeByName(scene, "NeonSun").value_or(ri::scene::kInvalidHandle);
    handles.neonSunShell = ri::scene::FindNodeByName(scene, "NeonSunShell").value_or(ri::scene::kInvalidHandle);
    handles.pulsingBrainSphere =
        ri::scene::FindNodeByName(scene, "PulsingBrainSphere").value_or(ri::scene::kInvalidHandle);
    handles.brainHalo = ri::scene::FindNodeByName(scene, "BrainHalo").value_or(ri::scene::kInvalidHandle);
    handles.checkerObeliskLeft =
        ri::scene::FindNodeByName(scene, "CheckerObeliskLeft").value_or(ri::scene::kInvalidHandle);
    handles.checkerObeliskRight =
        ri::scene::FindNodeByName(scene, "CheckerObeliskRight").value_or(ri::scene::kInvalidHandle);
    handles.glitchPyramid = ri::scene::FindNodeByName(scene, "GlitchPyramid").value_or(ri::scene::kInvalidHandle);
    handles.gateLight = ri::scene::FindNodeByName(scene, "GateLight").value_or(ri::scene::kInvalidHandle);
    handles.brainLight = ri::scene::FindNodeByName(scene, "BrainLight").value_or(ri::scene::kInvalidHandle);
    handles.sunLight = ri::scene::FindNodeByName(scene, "SunLight").value_or(ri::scene::kInvalidHandle);
    return handles;
}

const SceneMoodHandles& GetSceneMoodHandles(const ri::scene::Scene& scene) {
    static std::unordered_map<const ri::scene::Scene*, SceneMoodHandles> cache;
    const auto [it, inserted] = cache.try_emplace(&scene);
    if (inserted) {
        it->second = ResolveSceneMoodHandles(scene);
    }
    return it->second;
}

void AnimateHoverNode(ri::scene::Scene& scene,
                      int handle,
                      const ri::math::Vec3& basePosition,
                      const ri::math::Vec3& baseScale,
                      const float bobAmplitude,
                      const double elapsedSeconds,
                      const double bobSpeed,
                      const float yawRateDegrees,
                      const float scalePulseAmplitude = 0.0f,
                      const double scalePulseSpeed = 0.0) {
    if (handle == ri::scene::kInvalidHandle) {
        return;
    }

    ri::scene::Node& node = scene.GetNode(handle);
    const float bob = static_cast<float>(std::sin(bobSpeed * elapsedSeconds) * bobAmplitude);
    const float yaw = static_cast<float>(std::fmod(elapsedSeconds * static_cast<double>(yawRateDegrees), 360.0));
    const float scalePulse = scalePulseAmplitude <= 0.0f
                                 ? 0.0f
                                 : static_cast<float>(std::sin(scalePulseSpeed * elapsedSeconds) * scalePulseAmplitude);
    node.localTransform.position = basePosition + ri::math::Vec3{0.0f, bob, 0.0f};
    node.localTransform.rotationDegrees = ri::math::Vec3{0.0f, yaw, 0.0f};
    node.localTransform.scale = baseScale * (1.0f + scalePulse);
}

void PulseLight(ri::scene::Scene& scene,
                int handle,
                const float baseIntensity,
                const float amplitude,
                const double elapsedSeconds,
                const double speed) {
    if (handle == ri::scene::kInvalidHandle) {
        return;
    }

    ri::scene::Node& node = scene.GetNode(handle);
    if (node.light == ri::scene::kInvalidHandle) {
        return;
    }

    scene.GetLight(node.light).intensity =
        baseIntensity + static_cast<float>((std::sin(elapsedSeconds * speed) * 0.5 + 0.5) * amplitude);
}

void AnimateSceneMood(ri::scene::Scene& scene, const double elapsedSeconds) {
    const SceneMoodHandles& handles = GetSceneMoodHandles(scene);

    AnimateHoverNode(scene,
                     handles.fractalGate,
                     ri::math::Vec3{0.0f, 6.0f, 15.0f},
                     ri::math::Vec3{4.0f, 6.0f, 0.5f},
                     1.2f,
                     elapsedSeconds,
                     2.5,
                     90.0f,
                     0.15f,
                     3.0);
    AnimateHoverNode(scene,
                     handles.checkerObeliskLeft,
                     ri::math::Vec3{-12.0f, 4.0f, 8.0f},
                     ri::math::Vec3{2.0f, 8.0f, 2.0f},
                     0.8f,
                     elapsedSeconds,
                     1.2,
                     -45.0f);
    AnimateHoverNode(scene,
                     handles.checkerObeliskRight,
                     ri::math::Vec3{12.0f, 4.0f, 8.0f},
                     ri::math::Vec3{2.0f, 8.0f, 2.0f},
                     0.8f,
                     elapsedSeconds,
                     1.3,
                     55.0f);
    AnimateHoverNode(scene,
                     handles.pulsingBrainSphere,
                     ri::math::Vec3{25.0f, 15.0f, -20.0f},
                     ri::math::Vec3{10.0f, 10.0f, 10.0f},
                     2.0f,
                     elapsedSeconds,
                     4.0,
                     15.0f,
                     0.2f,
                     6.0);
    AnimateHoverNode(scene,
                     handles.brainHalo,
                     ri::math::Vec3{25.0f, 15.0f, -20.0f},
                     ri::math::Vec3{16.0f, 0.2f, 16.0f},
                     2.0f,
                     elapsedSeconds,
                     4.0,
                     -120.0f,
                     0.1f,
                     6.0);
    AnimateHoverNode(scene,
                     handles.neonSun,
                     ri::math::Vec3{0.0f, 30.0f, 80.0f},
                     ri::math::Vec3{15.0f, 15.0f, 15.0f},
                     1.0f,
                     elapsedSeconds,
                     0.5,
                     10.0f);
    AnimateHoverNode(scene,
                     handles.neonSunShell,
                     ri::math::Vec3{0.0f, 30.0f, 80.0f},
                     ri::math::Vec3{18.0f, 18.0f, 18.0f},
                     1.0f,
                     elapsedSeconds,
                     0.5,
                     -25.0f,
                     0.1f,
                     2.0);
    AnimateHoverNode(scene,
                     handles.glitchPyramid,
                     ri::math::Vec3{-30.0f, 8.0f, 35.0f},
                     ri::math::Vec3{6.0f, 6.0f, 6.0f},
                     4.0f,
                     elapsedSeconds,
                     0.4,
                     180.0f);

    PulseLight(scene, handles.gateLight, 14.0f, 2.5f, elapsedSeconds, 0.35);
    PulseLight(scene, handles.brainLight, 16.0f, 1.5f, elapsedSeconds, 0.25);
    PulseLight(scene, handles.sunLight, 22.0f, 2.0f, elapsedSeconds, 0.18);
}

} // namespace

namespace fs = std::filesystem;

using namespace ri::scene;

/// Maps engine logic node kinds (used by `LogicGraph` / port schema) to LogicKit manifest `id` values that ship
/// with `Assets/Packages/LogicKit/glb/*.glb` meshes.
[[nodiscard]] const char* LogicDemoEngineKindToKitVisualId(const std::string_view logicKind) {
    if (logicKind == "logic_trigger_detector") {
        return "io_trigger";
    }
    if (logicKind == "logic_relay") {
        return "flow_relay";
    }
    if (logicKind == "logic_pulse") {
        return "flow_oneshot";
    }
    return nullptr;
}

struct KitNodePortAnchors {
    std::optional<ri::math::Vec3> firstInputWorld{};
    std::optional<ri::math::Vec3> firstOutputWorld{};
};

void ExtractFirstInputOutputAnchors(const std::vector<ri::logic::LogicVisualPrimitiveInstance>& instances,
                                    KitNodePortAnchors& out) {
    for (const ri::logic::LogicVisualPrimitiveInstance& instance : instances) {
        const ri::math::Vec3 p{instance.worldPosition[0], instance.worldPosition[1], instance.worldPosition[2]};
        if (instance.kind == ri::logic::LogicVisualPrimitiveKind::InputStub && !out.firstInputWorld.has_value()) {
            out.firstInputWorld = p;
        }
        if (instance.kind == ri::logic::LogicVisualPrimitiveKind::OutputStub && !out.firstOutputWorld.has_value()) {
            out.firstOutputWorld = p;
        }
    }
}

void RemapMaterialsUsedByGltfSubtree(ri::scene::Scene& scene,
                                     const int rootNode,
                                     const fs::path& engineTexturesRoot,
                                     const fs::path& logicKitRoot) {
    if (rootNode == ri::scene::kInvalidHandle || engineTexturesRoot.empty() || logicKitRoot.empty()) {
        return;
    }
    const std::size_t matCount = scene.MaterialCount();
    if (matCount == 0U) {
        return;
    }
    std::vector<std::uint8_t> touched(matCount, 0);
    std::vector<int> stack;
    stack.push_back(rootNode);
    while (!stack.empty()) {
        const int handle = stack.back();
        stack.pop_back();
        const ri::scene::Node& node = scene.GetNode(handle);
        for (const int child : node.children) {
            stack.push_back(child);
        }
        if (node.material < 0 || static_cast<std::size_t>(node.material) >= matCount) {
            continue;
        }
        const std::size_t materialIndex = static_cast<std::size_t>(node.material);
        if (touched[materialIndex] != 0) {
            continue;
        }
        touched[materialIndex] = 1;
        ri::scene::Material& mat = scene.GetMaterial(node.material);
        auto fixTexturePath = [&](std::string& tex) {
            if (tex.empty()) {
                return;
            }
            fs::path asPath(tex);
            // Rewrite legacy machine-absolute LRT package paths to package-relative tails.
            if (asPath.is_absolute()) {
                const std::string generic = asPath.generic_string();
                constexpr std::string_view kLrtMarker = "/Assets/Packages/LRT - Texture Pack - RT28.8 - 128x/";
                const std::size_t marker = generic.find(kLrtMarker);
                if (marker == std::string::npos) {
                    return;
                }
                tex = generic.substr(marker + kLrtMarker.size());
                asPath = fs::path(tex);
            }
            if (fs::exists(engineTexturesRoot / asPath)) {
                return;
            }
            const fs::path lrtAbs =
                (engineTexturesRoot / ".." / "Packages" / "LRT - Texture Pack - RT28.8 - 128x" / asPath)
                    .lexically_normal();
            if (fs::is_regular_file(lrtAbs)) {
                tex = (fs::path("..") / "Packages" / "LRT - Texture Pack - RT28.8 - 128x" / asPath)
                          .lexically_normal()
                          .generic_string();
                return;
            }
            const fs::path kitAbs = (logicKitRoot / asPath).lexically_normal();
            if (!fs::is_regular_file(kitAbs)) {
                return;
            }
            std::error_code relativeError{};
            fs::path relative = fs::relative(kitAbs, engineTexturesRoot, relativeError);
            if (!relativeError && !relative.empty()) {
                tex = relative.generic_string();
                return;
            }
            tex = (fs::path("..") / "Packages" / "LogicKit" / asPath).lexically_normal().generic_string();
        };
        fixTexturePath(mat.baseColorTexture);
        fixTexturePath(mat.emissiveTexture);
        fixTexturePath(mat.normalTexture);
        fixTexturePath(mat.ormTexture);
        fixTexturePath(mat.opacityTexture);
        fixTexturePath(mat.occlusionTexture);
        fixTexturePath(mat.detailTexture);
    }
}

[[nodiscard]] std::string Lowercase(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

[[nodiscard]] bool ContainsAny(std::string_view text, std::initializer_list<std::string_view> needles) {
    for (const std::string_view needle : needles) {
        if (!needle.empty() && text.find(needle) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

void ApplyLiminalRendererShowcaseMaterials(ri::scene::Scene& scene, const fs::path& engineTexturesRoot) {
    const fs::path lrtPackageRoot =
        fs::weakly_canonical(engineTexturesRoot / ".." / "Packages" / "LRT - Texture Pack - RT28.8 - 128x");
    const auto packagePath = [&lrtPackageRoot](std::string_view tail) {
        return (lrtPackageRoot / fs::path(tail)).lexically_normal().generic_string();
    };
    const auto packageExists = [&lrtPackageRoot](std::string_view tail) {
        return fs::exists((lrtPackageRoot / fs::path(tail)).lexically_normal());
    };
    auto forcePackageTriplet = [&](ri::scene::Material& material,
                                   std::string_view albedoTail,
                                   std::string_view normalTail,
                                   std::string_view specTail) {
        material.baseColorTexture.clear();
        material.baseColorTextureFrames.clear();
        material.baseColorTextureFramesPerSecond = 0.0f;
        material.normalTexture.clear();
        material.ormTexture.clear();
        if (packageExists(albedoTail)) {
            material.baseColorTexture = packagePath(albedoTail);
        }
        if (packageExists(normalTail)) {
            material.normalTexture = packagePath(normalTail);
        }
        if (packageExists(specTail)) {
            material.ormTexture = packagePath(specTail);
        }
    };
    auto clearAnimatedTextureState = [&](ri::scene::Material& material) {
        material.baseColorTextureFrames.clear();
        material.baseColorTextureFramesPerSecond = 0.0f;
    };
    constexpr ri::math::Vec3 kConcreteTint{0.74f, 0.73f, 0.71f};
    constexpr ri::math::Vec3 kStoneTint{0.71f, 0.70f, 0.68f};
    constexpr ri::math::Vec3 kMonolithTint{0.69f, 0.68f, 0.66f};
    constexpr ri::math::Vec3 kWarmFluorescent{1.0f, 0.92f, 0.78f};
    auto setBrutalistConcrete = [&](ri::scene::Material& material, const ri::math::Vec3& tint, const float roughness) {
        material.shadingModel = ri::scene::ShadingModel::Lit;
        material.materialStyle = ri::scene::MaterialStyle::Layered;
        material.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
        material.baseColor = tint;
        material.emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
        material.metallic = 0.0f;
        material.roughness = roughness;
        material.transparent = false;
        material.additiveBlend = false;
        forcePackageTriplet(material, "ctm/RT_all_concrete_1.png", "ctm/RT_all_concrete_1_n.png", "ctm/RT_all_concrete_1_s.png");
        material.detailTexture = packageExists("tile/RT_andesite.png") ? packagePath("tile/RT_andesite.png") : std::string{};
    };
    auto setCarvedStone = [&](ri::scene::Material& material,
                              const ri::math::Vec3& tint,
                              const float roughness,
                              const bool brickVariant) {
        material.shadingModel = ri::scene::ShadingModel::Lit;
        material.materialStyle = ri::scene::MaterialStyle::Layered;
        material.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
        material.baseColor = tint;
        material.emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
        material.metallic = 0.0f;
        material.roughness = roughness;
        material.transparent = false;
        material.additiveBlend = false;
        if (brickVariant) {
            forcePackageTriplet(material,
                                "tile/RT_tuff_bricks.png",
                                "tile/RT_tuff_bricks_n.png",
                                "tile/RT_tuff_bricks_s.png");
        } else {
            forcePackageTriplet(material, "tile/RT_tuff.png", "tile/RT_tuff_n.png", "tile/RT_tuff_s.png");
        }
        material.detailTexture = packageExists("tile/RT_tuff.png") ? packagePath("tile/RT_tuff.png") : std::string{};
    };
    auto setDarkAperture = [&](ri::scene::Material& material) {
        material.shadingModel = ri::scene::ShadingModel::Unlit;
        material.materialStyle = ri::scene::MaterialStyle::Standard;
        material.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
        material.baseColor = ri::math::Vec3{0.045f, 0.047f, 0.048f};
        material.emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
        material.metallic = 0.0f;
        material.roughness = 1.0f;
        material.transparent = false;
        material.additiveBlend = false;
        material.baseColorTexture.clear();
        clearAnimatedTextureState(material);
        material.normalTexture.clear();
        material.ormTexture.clear();
        material.detailTexture.clear();
    };
    auto setIndustrialGrate = [&](ri::scene::Material& material) {
        material.shadingModel = ri::scene::ShadingModel::Lit;
        material.materialStyle = ri::scene::MaterialStyle::MixedMedia;
        material.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
        material.baseColor = ri::math::Vec3{0.34f, 0.35f, 0.37f};
        material.emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
        material.metallic = 0.30f;
        material.roughness = 0.72f;
        forcePackageTriplet(material,
                            "tile/RT_iron_bars.png",
                            "tile/RT_iron_bars_n.png",
                            "tile/RT_iron_bars_s.png");
        material.detailTexture = packageExists("tile/RT_stainless_steel.png") ? packagePath("tile/RT_stainless_steel.png")
                                                                               : std::string{};
    };
    auto setIndustrialBlock = [&](ri::scene::Material& material, const ri::math::Vec3& tint, const float roughness) {
        material.shadingModel = ri::scene::ShadingModel::Lit;
        material.materialStyle = ri::scene::MaterialStyle::MixedMedia;
        material.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
        material.baseColor = tint;
        material.emissiveColor = ri::math::Vec3{0.0f, 0.0f, 0.0f};
        material.metallic = 0.38f;
        material.roughness = roughness;
        material.transparent = false;
        material.additiveBlend = false;
        forcePackageTriplet(material, "tile/RT_iron_block.png", "tile/RT_iron_block_n.png", "tile/RT_iron_block_s.png");
        material.detailTexture = packageExists("tile/RT_stainless_steel.png") ? packagePath("tile/RT_stainless_steel.png")
                                                                               : std::string{};
    };
    auto setSignalMetal = [&](ri::scene::Material& material, const ri::math::Vec3& tint, const float emissiveBoost) {
        setIndustrialBlock(material, tint, 0.64f);
        material.emissiveColor = tint * emissiveBoost;
    };
    auto setGameplayPlate = [&](ri::scene::Material& material) {
        material.shadingModel = ri::scene::ShadingModel::Lit;
        material.materialStyle = ri::scene::MaterialStyle::MixedMedia;
        material.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
        material.baseColor = ri::math::Vec3{0.26f, 0.31f, 0.25f};
        material.emissiveColor = ri::math::Vec3{0.05f, 0.20f, 0.08f};
        material.metallic = 0.24f;
        material.roughness = 0.58f;
        material.transparent = false;
        material.additiveBlend = false;
        forcePackageTriplet(material,
                            "tile/RT_iron_trapdoor.png",
                            "tile/RT_iron_trapdoor_n.png",
                            "tile/RT_iron_trapdoor_s.png");
        material.detailTexture = packageExists("tile/RT_stainless_steel.png") ? packagePath("tile/RT_stainless_steel.png")
                                                                               : std::string{};
    };
    auto setGameplayDoor = [&](ri::scene::Material& material) {
        material.shadingModel = ri::scene::ShadingModel::Lit;
        material.materialStyle = ri::scene::MaterialStyle::MixedMedia;
        material.materialWorkflow = ri::scene::MaterialWorkflow::SpecGloss;
        material.baseColor = ri::math::Vec3{0.22f, 0.29f, 0.35f};
        material.emissiveColor = ri::math::Vec3{0.02f, 0.06f, 0.10f};
        material.metallic = 0.40f;
        material.roughness = 0.48f;
        material.transparent = false;
        material.additiveBlend = false;
        forcePackageTriplet(material, "tile/RT_iron_block.png", "tile/RT_iron_block_n.png", "tile/RT_iron_block_s.png");
        material.detailTexture = packageExists("tile/RT_stainless_steel.png") ? packagePath("tile/RT_stainless_steel.png")
                                                                               : std::string{};
    };
    auto setGameplayPortal = [&](ri::scene::Material& material) {
        material.shadingModel = ri::scene::ShadingModel::Unlit;
        material.materialStyle = ri::scene::MaterialStyle::Crystal;
        material.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
        material.baseColor = ri::math::Vec3{0.72f, 0.30f, 0.88f};
        material.emissiveColor = ri::math::Vec3{0.22f, 0.08f, 0.30f};
        material.metallic = 0.0f;
        material.roughness = 0.16f;
        material.transparent = false;
        material.additiveBlend = false;
        forcePackageTriplet(material,
                            "tile/RT_white_stained_glass.png",
                            "tile/RT_white_stained_glass_n.png",
                            "tile/RT_white_stained_glass_s.png");
        material.detailTexture.clear();
    };
    auto setLightAperture = [&](ri::scene::Material& material, const ri::math::Vec3& glow, const bool warm) {
        const std::string materialNameLower = Lowercase(material.name);
        material.shadingModel = ri::scene::ShadingModel::Unlit;
        material.materialStyle = ri::scene::MaterialStyle::Standard;
        material.materialWorkflow = ri::scene::MaterialWorkflow::MetalRough;
        material.baseColor = glow;
        material.emissiveColor = warm ? ri::math::Vec3{0.48f, 0.40f, 0.28f} : ri::math::Vec3{0.18f, 0.19f, 0.22f};
        material.metallic = 0.0f;
        material.roughness = 0.40f;
        material.transparent = false;
        material.additiveBlend = false;
        if (ContainsAny(materialNameLower, {"doorglow", "door"})) {
            forcePackageTriplet(material, "tile/RT_sea_lantern.png", "", "tile/RT_sea_lantern_s.png");
        } else if (warm) {
            forcePackageTriplet(material, "tile/RT_shroomlight.png", "tile/RT_shroomlight_n.png", "tile/RT_shroomlight_s.png");
        } else {
            forcePackageTriplet(material, "tile/RT_white_stained_glass.png", "tile/RT_white_stained_glass_n.png", "tile/RT_white_stained_glass_s.png");
        }
    };

    for (std::size_t materialIndex = 0; materialIndex < scene.MaterialCount(); ++materialIndex) {
        ri::scene::Material& material = scene.GetMaterial(static_cast<int>(materialIndex));
        const std::string key = Lowercase(material.name + "|" + material.baseColorTexture + "|" + material.emissiveTexture);

        if (ContainsAny(key, {"logicdemopressureplate", "pressureplate", "logic-demo-pressure-plate", "plate_io"})) {
            setGameplayPlate(material);
            continue;
        }
        if (ContainsAny(key, {"logicdemodoor", "logic-demo-door", "door_io"})) {
            setGameplayDoor(material);
            continue;
        }
        if (ContainsAny(key, {"logicdemoportal", "logic-demo-portal", "portal_io"})) {
            setGameplayPortal(material);
            continue;
        }
        if (ContainsAny(key, {"logicport", "logiclayer", "wire", "logicio", "logic_trigger", "logic_relay"})) {
            setSignalMetal(material, ri::math::Vec3{0.62f, 0.64f, 0.68f}, 0.03f);
            continue;
        }
        if (ContainsAny(key, {"glow", "oculus", "aperture", "fluorescent", "door"})) {
            setLightAperture(material,
                             ContainsAny(key, {"fluorescent"}) ? kWarmFluorescent : ri::math::Vec3{0.94f, 0.94f, 0.92f},
                             ContainsAny(key, {"fluorescent"}));
            continue;
        }
        if (ContainsAny(key, {"catwalk", "grille", "grate", "deck", "landing"})) {
            setIndustrialGrate(material);
            continue;
        }
        if (ContainsAny(key, {"bridge", "brace", "leg", "beam", "spine", "slab", "metal"})) {
            setIndustrialBlock(material, ri::math::Vec3{0.31f, 0.33f, 0.36f}, 0.66f);
            continue;
        }
        if (ContainsAny(key, {"wall", "entry", "apse", "arch", "doorframe", "stair", "retaining", "slope"})) {
            setCarvedStone(material, kStoneTint, 0.98f, true);
            continue;
        }
        if (ContainsAny(key, {"tuff", "drum", "tower", "monolith", "pillar", "shaft", "pod", "ring", "cylinder"})) {
            setCarvedStone(material, kMonolithTint, 0.99f, false);
            continue;
        }
        setBrutalistConcrete(material, kConcreteTint, 0.99f);
    }

    for (std::size_t nodeIndex = 0; nodeIndex < scene.NodeCount(); ++nodeIndex) {
        ri::scene::Node& node = scene.GetNode(static_cast<int>(nodeIndex));
        if (node.material == ri::scene::kInvalidHandle) {
            continue;
        }
        ri::scene::Material& material = scene.GetMaterial(node.material);
        const std::string nodeName = Lowercase(node.name);
        if (ContainsAny(nodeName, {"oculus", "glow", "aperture", "fluorescent"})) {
            setLightAperture(material,
                             ContainsAny(nodeName, {"fluorescent"}) ? kWarmFluorescent : ri::math::Vec3{0.94f, 0.94f, 0.92f},
                             ContainsAny(nodeName, {"fluorescent"}));
            continue;
        }
        if (ContainsAny(nodeName, {"windowcard", "midtowerwindow", "window"})
            && !ContainsAny(nodeName, {"glow", "fluorescent"})) {
            setDarkAperture(material);
            continue;
        }
        if (ContainsAny(nodeName, {"pressureplate"})) {
            setGameplayPlate(material);
            continue;
        }
        if (ContainsAny(nodeName, {"logicdemodoor"})) {
            setGameplayDoor(material);
            continue;
        }
        if (ContainsAny(nodeName, {"logicdemoportal"})) {
            setGameplayPortal(material);
            continue;
        }
        if (ContainsAny(nodeName, {"logicport", "logiclayer", "wire", "logicio"})) {
            setSignalMetal(material, ri::math::Vec3{0.62f, 0.64f, 0.68f}, 0.03f);
            continue;
        }
        if (ContainsAny(nodeName, {"catwalk", "deck", "landing"})) {
            setIndustrialGrate(material);
            continue;
        }
        if (ContainsAny(nodeName, {"bridge", "brace", "leg", "beam", "spine", "slab"})) {
            setIndustrialBlock(material, ri::math::Vec3{0.31f, 0.33f, 0.36f}, 0.66f);
            continue;
        }
        if (ContainsAny(nodeName, {"floor", "plaza", "causeway", "basin"})) {
            setBrutalistConcrete(material, kConcreteTint, 0.99f);
            continue;
        }
        if (ContainsAny(nodeName, {"entry", "apse", "wall", "arch", "doorframe", "stair", "retaining", "slope"})) {
            setCarvedStone(material, kStoneTint, 0.98f, true);
            continue;
        }
        if (ContainsAny(nodeName, {"pillar", "tower", "monolith", "shaft", "pod", "ring", "drum", "cylinder"})) {
            setCarvedStone(material, kMonolithTint, 0.99f, false);
        }
    }
}

[[nodiscard]] ri::math::Vec3 QuadraticBezier(const ri::math::Vec3& p0,
                                           const ri::math::Vec3& p1,
                                           const ri::math::Vec3& p2,
                                           const float t) {
    const float u = 1.0f - t;
    return (p0 * (u * u)) + (p1 * (2.0f * u * t)) + (p2 * (t * t));
}

[[nodiscard]] ri::math::Vec3 RotationDegreesForAxisAlignedCubeAlongZ(const ri::math::Vec3& dirRaw) {
    const ri::math::Vec3 n = ri::math::Normalize(dirRaw);
    const float lenXZ = std::sqrt((n.x * n.x) + (n.z * n.z));
    const float pitchRad = std::atan2(-n.y, lenXZ);
    const float yawRad = std::atan2(n.x, n.z);
    return ri::math::Vec3{ri::math::RadiansToDegrees(pitchRad), ri::math::RadiansToDegrees(yawRad), 0.0f};
}

World BuildWorld(std::string_view sceneName, const fs::path& gameRoot) {
    World world{};
    world.scene = Scene(std::string(sceneName));
    Scene& scene = world.scene;
    const auto palette = LoadLiminalPalette(gameRoot);
    const fs::path workspaceRoot = ri::content::DetectWorkspaceRoot(gameRoot);
    const fs::path logicKitManifestPath = workspaceRoot / std::string(ri::logic::kLogicKitNodesJsonRelative);
    const fs::path lrtPackageRoot = workspaceRoot / "Assets" / "Packages" / "LRT - Texture Pack - RT28.8 - 128x";
    const auto liminalPackageTexture = [&lrtPackageRoot](std::string_view relativePath) {
        return (lrtPackageRoot / fs::path(relativePath)).lexically_normal().generic_string();
    };

    world.handles.root = scene.CreateNode("DreamEmulatorLayer");

    LightNodeOptions sun{};
    sun.nodeName = "VoidLight";
    sun.parent = world.handles.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-52.0f, 38.0f, 0.0f};
    sun.light = Light{
        .name = "VoidLight",
        .type = LightType::Directional,
        .color = PaletteOr(palette, "void_ambient", ri::math::Vec3{0.52f, 0.51f, 0.49f}),
        .intensity = 1.15f,
    };
    world.handles.sun = AddLightNode(scene, sun);

    OrbitCameraOptions orbitCamera{};
    orbitCamera.parent = world.handles.root;
    orbitCamera.camera = Camera{
        .name = "EditorCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 90.0f,
        .nearClip = 0.05f,
        .farClip = 1000.0f,
    };
    orbitCamera.orbit = OrbitCameraState{
        .target = ri::math::Vec3{0.0f, 4.0f, 0.0f},
        .distance = 32.0f,
        .yawDegrees = 38.0f,
        .pitchDegrees = -12.0f,
    };
    world.handles.orbitCamera = AddOrbitCamera(scene, orbitCamera);

    world.handles.grid = ri::scene::kInvalidHandle;
    world.handles.axes = {};

    auto addPrimitive = [&](const std::string& nodeName,
                            PrimitiveType primitive,
                            const ri::math::Vec3& position,
                            const ri::math::Vec3& scale,
                            const ri::math::Vec3& color,
                            const std::string& texture,
                            const ri::math::Vec2& tiling,
                            std::string materialName,
                            ShadingModel shading = ShadingModel::Lit,
                            const ri::math::Vec3& rotation = ri::math::Vec3{}) {
        PrimitiveNodeOptions primitiveOptions{};
        primitiveOptions.nodeName = nodeName;
        primitiveOptions.parent = world.handles.root;
        primitiveOptions.primitive = primitive;
        primitiveOptions.materialName = std::move(materialName);
        primitiveOptions.shadingModel = shading;
        primitiveOptions.baseColor = color;
        primitiveOptions.baseColorTexture = texture;
        primitiveOptions.textureTiling = tiling;
        primitiveOptions.transform.position = position;
        primitiveOptions.transform.scale = scale;
        primitiveOptions.transform.rotationDegrees = rotation;
        return AddPrimitiveNode(scene, primitiveOptions);
    };

    world.handles.orbitCamera.orbit.target = ri::math::Vec3{0.0f, 4.0f, 2.0f};
    SetOrbitCameraState(scene, world.handles.orbitCamera, world.handles.orbitCamera.orbit);

    // --- Hub: late-90s checker void (wings authored in assembly.primitives.csv) ---

    world.handles.floor = ri::scene::kInvalidHandle;

    // Logic demo chain: pressure plate -> door open -> portal spawn.
    const ri::math::Vec3 pressurePlatePos{58.0f, 0.08f, -42.0f};
    const ri::math::Vec3 pressurePlateScale{1.4f, 0.08f, 1.4f};
    world.logicDemo.pressurePlateNode =
        addPrimitive("LogicDemoPressurePlate",
                     PrimitiveType::Cube,
                     pressurePlatePos,
                     pressurePlateScale,
                     ri::math::Vec3{0.2f, 0.95f, 0.35f},
                     liminalPackageTexture("tile/RT_iron_trapdoor.png"),
                     ri::math::Vec2{1.0f, 1.0f},
                     "logic-demo-pressure-plate",
                     ShadingModel::Unlit);
    world.logicDemo.pressurePlateBounds = ri::spatial::Aabb{
        .min = pressurePlatePos - ri::math::Vec3{1.2f, 0.6f, 1.2f},
        .max = pressurePlatePos + ri::math::Vec3{1.2f, 1.8f, 1.2f},
    };

    world.logicDemo.doorClosedPosition = ri::math::Vec3{58.0f, 2.0f, -35.0f};
    world.logicDemo.doorOpenPosition = world.logicDemo.doorClosedPosition + ri::math::Vec3{0.0f, 4.5f, 0.0f};
    world.logicDemo.doorNode =
        addPrimitive("LogicDemoDoor",
                     PrimitiveType::Cube,
                     world.logicDemo.doorClosedPosition,
                     ri::math::Vec3{2.3f, 3.8f, 0.35f},
                     ri::math::Vec3{0.1f, 0.7f, 1.0f},
                     liminalPackageTexture("tile/RT_iron_block.png"),
                     ri::math::Vec2{1.0f, 2.0f},
                     "logic-demo-door",
                     ShadingModel::Lit);

    const ri::math::Vec3 portalPos{58.0f, 1.5f, -28.0f};
    world.logicDemo.portalNode =
        addPrimitive("LogicDemoPortal",
                     PrimitiveType::Cube,
                     portalPos,
                     ri::math::Vec3{0.01f, 0.01f, 0.01f},
                     ri::math::Vec3{0.85f, 0.2f, 1.0f},
                     liminalPackageTexture("tile/RT_white_stained_glass.png"),
                     ri::math::Vec2{1.0f, 1.0f},
                     "logic-demo-portal",
                     ShadingModel::Unlit);
    world.logicDemo.portalBounds = ri::spatial::Aabb{
        .min = portalPos - ri::math::Vec3{1.0f, 1.5f, 0.8f},
        .max = portalPos + ri::math::Vec3{1.0f, 1.5f, 0.8f},
    };

    // Packaged LogicKit manifest (repo `Assets/Packages/LogicKit`) drives port names and extra node kinds when present.
    static std::unique_ptr<ri::logic::LogicKitManifest> s_logicKitManifest;
    static bool s_logicKitLoadAttempted = false;
    if (!s_logicKitLoadAttempted) {
        s_logicKitLoadAttempted = true;
        if (std::optional<ri::logic::LogicKitManifest> loaded = ri::logic::LoadLogicKitManifest(logicKitManifestPath)) {
            s_logicKitManifest = std::make_unique<ri::logic::LogicKitManifest>(std::move(*loaded));
            ri::logic::SetActiveLogicKitManifest(s_logicKitManifest.get());
        }
    }

    // Engine-level logic visual primitive library used by this game (reusable across games).
    const ri::logic::LogicVisualLibrary logicVisualLibrary = ri::logic::BuildDefaultLogicVisualLibrary();
    const ri::math::Vec3 logicSideOrigin{-42.0f, 2.0f, -18.0f};
    const fs::path engineTexturesRoot = workspaceRoot / "Assets" / "Textures";
    const fs::path logicKitRoot = ri::logic::LogicKitRootDirectory(logicKitManifestPath);

    auto spawnVisual = [&](const ri::logic::LogicVisualPrimitiveInstance& instance, const std::string& namePrefix) {
        const ri::math::Vec3 position{instance.worldPosition[0], instance.worldPosition[1], instance.worldPosition[2]};
        const ri::math::Vec3 scale{instance.worldScale[0], instance.worldScale[1], instance.worldScale[2]};
        const ri::math::Vec3 color{instance.color[0], instance.color[1], instance.color[2]};
        const ri::math::Vec3 rotation{
            instance.worldRotationDegrees[0], instance.worldRotationDegrees[1], instance.worldRotationDegrees[2]};
        const int handle =
            addPrimitive(namePrefix + "_" + instance.id,
                         PrimitiveType::Cube,
                         position,
                         scale,
                         color,
                         liminalPackageTexture("tile/RT_iron_block.png"),
                         ri::math::Vec2{1.0f, 1.0f},
                         "logic-visual-" + instance.id,
                         ShadingModel::Unlit,
                         rotation);
        if (handle != ri::scene::kInvalidHandle) {
            ri::scene::Node& node = scene.GetNode(handle);
            if (node.material != ri::scene::kInvalidHandle) {
                ri::scene::Material& material = scene.GetMaterial(node.material);
                material.emissiveColor =
                    ri::math::Vec3{instance.emissive[0], instance.emissive[1], instance.emissive[2]};
            }
            world.logicDemo.logicLayerNodes.push_back(handle);
            world.logicDemo.logicLayerVisibleScales.push_back(scale);
            node.localTransform.scale = ri::math::Vec3{0.01f, 0.01f, 0.01f};
        }
        return handle;
    };

    auto registerLogicLayerHiddenUntilDebug = [&](const int handle, const ri::math::Vec3& visibleScale) {
        if (handle == ri::scene::kInvalidHandle) {
            return;
        }
        ri::scene::Node& node = scene.GetNode(handle);
        world.logicDemo.logicLayerNodes.push_back(handle);
        world.logicDemo.logicLayerVisibleScales.push_back(visibleScale);
        node.localTransform.scale = ri::math::Vec3{0.01f, 0.01f, 0.01f};
    };

    auto spawnNodeVisuals = [&](const std::string& nodeKind,
                                const std::string& nodeId,
                                const ri::math::Vec3& position,
                                std::vector<int>* allNodeHandles,
                                int* nodeBodyOut,
                                KitNodePortAnchors* portAnchors) {
        const std::array<float, 3> worldPos{position.x, position.y, position.z};
        const ri::logic::LogicKitManifest* kit = ri::logic::ActiveLogicKitManifest();
        const char* kitVisualId = LogicDemoEngineKindToKitVisualId(nodeKind);
        const std::string stubKind = (kitVisualId != nullptr) ? std::string(kitVisualId) : nodeKind;
        std::vector<ri::logic::LogicVisualPrimitiveInstance> layoutInstances =
            ri::logic::BuildLogicVisualNodeInstances(logicVisualLibrary, stubKind, nodeId, worldPos, false);
        if (portAnchors != nullptr) {
            ExtractFirstInputOutputAnchors(layoutInstances, *portAnchors);
        }

        if (kit != nullptr && kitVisualId != nullptr) {
            if (const ri::logic::LogicKitNodeManifestEntry* entry = ri::logic::FindLogicKitNodeManifestEntry(*kit, kitVisualId)) {
                if (!entry->glbRelative.empty()) {
                    const fs::path glbPath = ri::logic::ResolveLogicKitGlbPath(logicKitManifestPath, entry->glbRelative);
                    if (fs::is_regular_file(glbPath)) {
                        std::string importError;
                        const ri::math::Vec3 modelScale{2.2f, 2.2f, 2.2f};
                        const int handle = AddModelNode(
                            scene,
                            ImportedModelOptions{
                                .sourcePath = glbPath,
                                .nodeName = std::string("LogicLayerKit_") + nodeId + "_" + kitVisualId,
                                .parent = world.handles.root,
                                .transform =
                                    Transform{
                                        .position = position,
                                        .rotationDegrees = ri::math::Vec3{0.0f, 0.0f, 0.0f},
                                        .scale = modelScale,
                                    },
                            },
                            &importError);
                        if (handle != ri::scene::kInvalidHandle) {
                            RemapMaterialsUsedByGltfSubtree(scene, handle, engineTexturesRoot, logicKitRoot);
                            int stubSerial = 0;
                            for (const ri::logic::LogicVisualPrimitiveInstance& instance : layoutInstances) {
                                if (instance.kind != ri::logic::LogicVisualPrimitiveKind::InputStub &&
                                    instance.kind != ri::logic::LogicVisualPrimitiveKind::OutputStub) {
                                    continue;
                                }
                                const ri::math::Vec3 stubPos{instance.worldPosition[0], instance.worldPosition[1],
                                                              instance.worldPosition[2]};
                                const ri::math::Vec3 stubScale{instance.worldScale[0], instance.worldScale[1],
                                                               instance.worldScale[2]};
                                const ri::math::Vec3 stubColor{instance.color[0], instance.color[1], instance.color[2]};
                                const ri::math::Vec3 stubRot{instance.worldRotationDegrees[0],
                                                             instance.worldRotationDegrees[1],
                                                             instance.worldRotationDegrees[2]};
                                const int stubHandle = addPrimitive(
                                    std::string("LogicPort_") + nodeId + "_" + std::to_string(stubSerial++),
                                    PrimitiveType::Cube,
                                    stubPos,
                                    stubScale,
                                    stubColor,
                                    liminalPackageTexture("tile/RT_iron_block.png"),
                                    ri::math::Vec2{1.0f, 1.0f},
                                    std::string("logic-port-") + nodeId + "-" + instance.id,
                                    ShadingModel::Unlit,
                                    stubRot);
                                if (stubHandle != ri::scene::kInvalidHandle) {
                                    if (allNodeHandles != nullptr) {
                                        allNodeHandles->push_back(stubHandle);
                                    }
                                    ri::scene::Node& stubNode = scene.GetNode(stubHandle);
                                    if (stubNode.material != ri::scene::kInvalidHandle) {
                                        ri::scene::Material& stubMaterial = scene.GetMaterial(stubNode.material);
                                        stubMaterial.emissiveColor = ri::math::Vec3{instance.emissive[0], instance.emissive[1],
                                                                                   instance.emissive[2]};
                                    }
                                    registerLogicLayerHiddenUntilDebug(stubHandle, stubScale);
                                }
                            }
                            if (allNodeHandles != nullptr) {
                                allNodeHandles->push_back(handle);
                            }
                            registerLogicLayerHiddenUntilDebug(handle, modelScale);
                            if (nodeBodyOut != nullptr) {
                                *nodeBodyOut = handle;
                            }
                            return;
                        }
                    }
                }
            }
            // Mapped LogicKit visuals should never downgrade to generic fallback blocks for this demo.
            return;
        }

        layoutInstances =
            ri::logic::BuildLogicVisualNodeInstances(logicVisualLibrary, nodeKind, nodeId, worldPos, false);
        if (portAnchors != nullptr) {
            ExtractFirstInputOutputAnchors(layoutInstances, *portAnchors);
        }
        for (const ri::logic::LogicVisualPrimitiveInstance& instance : layoutInstances) {
            const int h = spawnVisual(instance, "LogicLayer");
            if (h == ri::scene::kInvalidHandle) {
                continue;
            }
            if (allNodeHandles != nullptr) {
                allNodeHandles->push_back(h);
            }
            if (nodeBodyOut != nullptr && instance.kind == ri::logic::LogicVisualPrimitiveKind::NodeBody) {
                *nodeBodyOut = h;
            }
        }
    };

    KitNodePortAnchors triggerPorts{};
    KitNodePortAnchors relayPorts{};
    KitNodePortAnchors pulsePorts{};

    spawnNodeVisuals("logic_trigger_detector",
                     "logic_demo_trigger",
                     logicSideOrigin + ri::math::Vec3{0.0f, 0.0f, 0.0f},
                     &world.logicDemo.logicPressureVisualNodes,
                     &world.logicDemo.logicNodePressure,
                     &triggerPorts);
    spawnNodeVisuals("logic_relay",
                     "logic_demo_door",
                     logicSideOrigin + ri::math::Vec3{4.0f, 0.0f, 0.0f},
                     &world.logicDemo.logicDoorVisualNodes,
                     &world.logicDemo.logicNodeDoor,
                     &relayPorts);
    spawnNodeVisuals("logic_pulse",
                     "logic_demo_portal",
                     logicSideOrigin + ri::math::Vec3{8.0f, 0.0f, 0.0f},
                     &world.logicDemo.logicPortalVisualNodes,
                     &world.logicDemo.logicNodePortal,
                     &pulsePorts);

    int demoWireSerial = 0;
    auto pushThickBezierWire = [&](const ri::math::Vec3& from, const ri::math::Vec3& to) {
        const ri::math::Vec3 delta = to - from;
        const float span = ri::math::Length(delta);
        if (span < 0.04f) {
            return;
        }
        const float lift = std::clamp(0.24f * span, 0.28f, 1.85f);
        const ri::math::Vec3 p0 = from;
        const ri::math::Vec3 p2 = to;
        const ri::math::Vec3 p1 = (p0 + p2) * 0.5f + ri::math::Vec3{0.0f, lift, 0.0f};
        const int beads = std::clamp(static_cast<int>(span / 0.18f), 14, 56);
        const float beadRadius = 0.16f;
        for (int i = 0; i <= beads; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(beads);
            const ri::math::Vec3 center = QuadraticBezier(p0, p1, p2, t);
            const int wireHandle = addPrimitive(
                "LogicDemoBezierWire_" + std::to_string(demoWireSerial++),
                PrimitiveType::Sphere,
                center,
                ri::math::Vec3{beadRadius, beadRadius, beadRadius},
                ri::math::Vec3{0.95f, 0.82f, 0.18f},
                "ri_prototype_yellow.png",
                ri::math::Vec2{1.0f, 1.0f},
                "logic-demo-bezier-wire",
                ShadingModel::Unlit);
            if (wireHandle != ri::scene::kInvalidHandle) {
                world.logicDemo.logicWireVisualNodes.push_back(wireHandle);
                ri::scene::Node& wn = scene.GetNode(wireHandle);
                if (wn.material != ri::scene::kInvalidHandle) {
                    ri::scene::Material& wm = scene.GetMaterial(wn.material);
                    wm.emissiveColor = ri::math::Vec3{0.35f, 0.28f, 0.06f};
                }
                registerLogicLayerHiddenUntilDebug(
                    wireHandle, ri::math::Vec3{beadRadius, beadRadius, beadRadius});
            }
        }
    };

    const ri::math::Vec3 triggerOutput =
        triggerPorts.firstOutputWorld.value_or(logicSideOrigin + ri::math::Vec3{1.2f, 0.0f, 0.0f});
    const ri::math::Vec3 relayInput =
        relayPorts.firstInputWorld.value_or(logicSideOrigin + ri::math::Vec3{2.8f, 0.0f, 0.0f});
    const ri::math::Vec3 pulseInput =
        pulsePorts.firstInputWorld.value_or(logicSideOrigin + ri::math::Vec3{6.8f, 0.0f, 0.0f});
    pushThickBezierWire(triggerOutput, relayInput);
    const ri::math::Vec3 splitBump{0.0f, 0.22f, 0.08f};
    pushThickBezierWire(triggerOutput + splitBump, pulseInput);
    const auto ioTrunk = ri::logic::BuildLogicVisualWireSegmentInstance(
        logicVisualLibrary, "wire_io", {-20.0f, 1.0f, -2.5f}, {22.0f, 0.12f, 0.12f}, false);
    if (ioTrunk.has_value()) {
        world.logicDemo.logicIoTrunk = spawnVisual(*ioTrunk, "LogicLayer");
        if (world.logicDemo.logicIoTrunk != ri::scene::kInvalidHandle) {
            world.logicDemo.logicWireVisualNodes.push_back(world.logicDemo.logicIoTrunk);
        }
    }

    auto spawnIoStyle = [&](const std::string& idPrefix, const ri::math::Vec3& basePosition) {
        for (const ri::logic::LogicVisualPrimitiveDefinition& primitive : logicVisualLibrary.worldIoStyle.primitives) {
            ri::logic::LogicVisualPrimitiveInstance instance{};
            instance.id = idPrefix + ":" + primitive.id;
            instance.kind = primitive.kind;
            instance.worldPosition = std::array<float, 3>{
                basePosition.x + primitive.localPosition[0],
                basePosition.y + primitive.localPosition[1],
                basePosition.z + primitive.localPosition[2]};
            instance.worldRotationDegrees = primitive.localRotationDegrees;
            instance.worldScale = primitive.localScale;
            instance.color = primitive.inactiveColor;
            instance.emissive = primitive.inactiveEmissive;
            (void)spawnVisual(instance, "LogicIO");
        }
    };
    spawnIoStyle("plate_io", pressurePlatePos + ri::math::Vec3{-2.2f, 0.55f, 0.0f});
    spawnIoStyle("door_io", world.logicDemo.doorClosedPosition + ri::math::Vec3{-2.8f, 0.0f, 0.0f});
    spawnIoStyle("portal_io", portalPos + ri::math::Vec3{-2.6f, 0.0f, 0.0f});

    LightNodeOptions corridorLight{};
    corridorLight.nodeName = "SouthCorridorLight";
    corridorLight.parent = world.handles.root;
    corridorLight.transform.position = ri::math::Vec3{0.0f, 8.3f, -34.0f};
    corridorLight.light = Light{
        .name = "SouthCorridorLight",
        .type = LightType::Point,
        .color = ri::math::Vec3{1.0f, 0.92f, 0.78f},
        .intensity = 18.0f,
        .range = 26.0f,
    };
    (void)AddLightNode(scene, corridorLight);

    LightNodeOptions skylightFill{};
    skylightFill.nodeName = "MainSkylightFill";
    skylightFill.parent = world.handles.root;
    skylightFill.transform.position = ri::math::Vec3{0.0f, 24.0f, 8.0f};
    skylightFill.light = Light{
        .name = "MainSkylightFill",
        .type = LightType::Point,
        .color = ri::math::Vec3{0.78f, 0.79f, 0.80f},
        .intensity = 12.0f,
        .range = 96.0f,
    };
    (void)AddLightNode(scene, skylightFill);

    LightNodeOptions northApseLight{};
    northApseLight.nodeName = "NorthApseLight";
    northApseLight.parent = world.handles.root;
    northApseLight.transform.position = ri::math::Vec3{0.0f, 4.6f, 51.0f};
    northApseLight.light = Light{
        .name = "NorthApseLight",
        .type = LightType::Point,
        .color = ri::math::Vec3{0.96f, 0.93f, 0.86f},
        .intensity = 10.0f,
        .range = 30.0f,
    };
    (void)AddLightNode(scene, northApseLight);

    world.colliders.clear();

    world.playerRig = scene.CreateNode("PlayerRig", world.handles.root);
    scene.GetNode(world.playerRig).localTransform.position = ri::math::Vec3{0.0f, 1.0f, -34.0f};
    world.playerCameraNode = scene.CreateNode("PlayerCameraNode", world.playerRig);
    const int playerCamera = scene.AddCamera(Camera{
        .name = "PlayerCamera",
        .projection = ProjectionType::Perspective,
        .fieldOfViewDegrees = 85.0f,
        .nearClip = 0.05f,
        .farClip = 1000.0f,
    });
    scene.AttachCamera(world.playerCameraNode, playerCamera);
    scene.GetNode(world.playerCameraNode).localTransform.rotationDegrees = ri::math::Vec3{0.0f, 0.0f, 0.0f};

    world.handles.crate = world.playerRig;
    world.handles.beacon = world.playerCameraNode;

    {
        ri::scene::AssemblyPrimitivesImportResult primitiveImport{};
        std::string primitiveImportError;
        (void)ri::scene::TryImportAssemblyPrimitivesCsv(
            scene,
            world.handles.root,
            gameRoot / "levels" / "assembly.primitives.csv",
            &primitiveImport,
            &primitiveImportError);
        // CSV textures are package-relative (tile/..., ctm/...). Resolve them against the
        // committed LRT pack so imports do not depend on machine-absolute paths.
        for (std::size_t materialIndex = 0; materialIndex < scene.MaterialCount(); ++materialIndex) {
            ri::scene::Material& material = scene.GetMaterial(static_cast<int>(materialIndex));
            auto resolveRelative = [&](std::string& texturePath) {
                if (texturePath.empty()) {
                    return;
                }
                const fs::path authored(texturePath);
                if (authored.is_absolute()) {
                    return;
                }
                const fs::path candidate = (lrtPackageRoot / authored).lexically_normal();
                if (fs::is_regular_file(candidate)) {
                    texturePath = candidate.generic_string();
                }
            };
            resolveRelative(material.baseColorTexture);
            resolveRelative(material.normalTexture);
            resolveRelative(material.ormTexture);
            resolveRelative(material.emissiveTexture);
            resolveRelative(material.opacityTexture);
            resolveRelative(material.detailTexture);
        }
    }

    {
        ri::scene::StructuralPrimitiveGalleryOptions galleryOptions{};
        galleryOptions.parent = world.handles.root;
        galleryOptions.nodeNamePrefix = "LiminalStructuralGallery";
        galleryOptions.transform.position = {-52.0f, 0.0f, -8.0f};
        galleryOptions.cellSpacing = {3.6f, 4.2f};
        galleryOptions.itemScale = {1.5f, 1.5f, 1.5f};
        galleryOptions.platformMargin = {3.5f, 0.0f, 3.5f};
        galleryOptions.platformThickness = 0.45f;
        galleryOptions.materialRows = ri::scene::BuildDefaultStructuralGalleryMaterials();
        (void)ri::scene::SpawnStructuralPrimitiveGallery(scene, galleryOptions);
    }
    {
        const ri::scene::StructuralAssemblySpawnResult structuralSpawn = ri::scene::SpawnStructuralAssemblyFromCsv(
            scene,
            gameRoot / "levels" / "assembly.structural.csv",
            ri::scene::StructuralAssemblySpawnOptions{
                .parent = world.handles.root,
                .materialNamePrefix = "liminal_struct",
            });
        (void)structuralSpawn;
    }

    // Surreal mood landmarks — base positions must stay in sync with AnimateSceneMood().
    auto addMoodLandmark = [&](const std::string& name,
                               const PrimitiveType primitive,
                               const ri::math::Vec3& position,
                               const ri::math::Vec3& scale,
                               const ri::math::Vec3& color,
                               const ri::math::Vec3& emissive,
                               const std::string& texture = "tile/RT_white_stained_glass.png") {
        const int handle = addPrimitive(name,
                                        primitive,
                                        position,
                                        scale,
                                        color,
                                        liminalPackageTexture(texture),
                                        ri::math::Vec2{1.0f, 1.0f},
                                        "liminal-mood-" + name,
                                        ShadingModel::Unlit);
        if (handle != ri::scene::kInvalidHandle && scene.GetNode(handle).material != ri::scene::kInvalidHandle) {
            scene.GetMaterial(scene.GetNode(handle).material).emissiveColor = emissive;
        }
        return handle;
    };
    (void)addMoodLandmark("FractalGate",
                          PrimitiveType::Cube,
                          ri::math::Vec3{0.0f, 6.0f, 15.0f},
                          ri::math::Vec3{4.0f, 6.0f, 0.5f},
                          ri::math::Vec3{0.18f, 0.92f, 0.86f},
                          ri::math::Vec3{0.22f, 0.78f, 0.72f},
                          "tile/RT_cyan_glazed_terracotta.png");
    (void)addMoodLandmark("CheckerObeliskLeft",
                          PrimitiveType::Cube,
                          ri::math::Vec3{-12.0f, 4.0f, 8.0f},
                          ri::math::Vec3{2.0f, 8.0f, 2.0f},
                          ri::math::Vec3{0.92f, 0.92f, 0.92f},
                          ri::math::Vec3{0.08f, 0.08f, 0.08f},
                          "tile/RT_black_concrete.png");
    (void)addMoodLandmark("CheckerObeliskRight",
                          PrimitiveType::Cube,
                          ri::math::Vec3{12.0f, 4.0f, 8.0f},
                          ri::math::Vec3{2.0f, 8.0f, 2.0f},
                          ri::math::Vec3{0.08f, 0.08f, 0.08f},
                          ri::math::Vec3{0.92f, 0.92f, 0.92f},
                          "tile/RT_white_concrete.png");
    (void)addMoodLandmark("PulsingBrainSphere",
                          PrimitiveType::Sphere,
                          ri::math::Vec3{25.0f, 15.0f, -20.0f},
                          ri::math::Vec3{10.0f, 10.0f, 10.0f},
                          ri::math::Vec3{0.72f, 0.18f, 0.42f},
                          ri::math::Vec3{0.48f, 0.08f, 0.22f},
                          "tile/RT_magenta_glazed_terracotta.png");
    (void)addMoodLandmark("BrainHalo",
                          PrimitiveType::Cube,
                          ri::math::Vec3{25.0f, 15.0f, -20.0f},
                          ri::math::Vec3{16.0f, 0.2f, 16.0f},
                          ri::math::Vec3{0.86f, 0.32f, 0.58f},
                          ri::math::Vec3{0.36f, 0.12f, 0.28f},
                          "tile/RT_pink_stained_glass.png");
    (void)addMoodLandmark("NeonSun",
                          PrimitiveType::Sphere,
                          ri::math::Vec3{0.0f, 30.0f, 80.0f},
                          ri::math::Vec3{15.0f, 15.0f, 15.0f},
                          ri::math::Vec3{1.0f, 0.82f, 0.22f},
                          ri::math::Vec3{0.92f, 0.58f, 0.08f},
                          "tile/RT_orange_glazed_terracotta.png");
    (void)addMoodLandmark("NeonSunShell",
                          PrimitiveType::Sphere,
                          ri::math::Vec3{0.0f, 30.0f, 80.0f},
                          ri::math::Vec3{18.0f, 18.0f, 18.0f},
                          ri::math::Vec3{1.0f, 0.72f, 0.18f},
                          ri::math::Vec3{0.42f, 0.24f, 0.04f},
                          "tile/RT_yellow_stained_glass.png");
    const int glitchPyramid =
        addMoodLandmark("GlitchPyramid",
                        PrimitiveType::Cube,
                        ri::math::Vec3{-30.0f, 8.0f, 35.0f},
                        ri::math::Vec3{6.0f, 6.0f, 6.0f},
                        ri::math::Vec3{0.22f, 0.88f, 0.72f},
                        ri::math::Vec3{0.14f, 0.52f, 0.44f},
                        "tile/RT_lime_glazed_terracotta.png");
    if (glitchPyramid != ri::scene::kInvalidHandle) {
        scene.GetNode(glitchPyramid).localTransform.rotationDegrees = {0.0f, 45.0f, 45.0f};
    }

    auto addMoodLight = [&](const std::string& name,
                            const ri::math::Vec3& position,
                            const ri::math::Vec3& color,
                            const float intensity,
                            const float range) {
        LightNodeOptions lightOptions{};
        lightOptions.nodeName = name;
        lightOptions.parent = world.handles.root;
        lightOptions.transform.position = position;
        lightOptions.light = Light{
            .name = name,
            .type = LightType::Point,
            .color = color,
            .intensity = intensity,
            .range = range,
        };
        (void)AddLightNode(scene, lightOptions);
    };
    addMoodLight("GateLight", ri::math::Vec3{0.0f, 8.0f, 15.0f}, ri::math::Vec3{0.22f, 0.92f, 0.82f}, 14.0f, 28.0f);
    addMoodLight("BrainLight", ri::math::Vec3{25.0f, 17.0f, -20.0f}, ri::math::Vec3{0.92f, 0.22f, 0.48f}, 16.0f, 36.0f);
    addMoodLight("SunLight", ri::math::Vec3{0.0f, 32.0f, 80.0f}, ri::math::Vec3{1.0f, 0.78f, 0.28f}, 22.0f, 120.0f);

    {
        PrimitiveNodeOptions basinWater{};
        basinWater.nodeName = "OuterBasinWater";
        basinWater.parent = world.handles.root;
        basinWater.primitive = PrimitiveType::Plane;
        basinWater.materialName = "liminal-basin-water";
        basinWater.transform.position = {0.0f, -1.35f, 6.0f};
        basinWater.transform.rotationDegrees = {0.0f, 0.0f, 0.0f};
        basinWater.transform.scale = {96.0f, 1.0f, 72.0f};
        basinWater.baseColor = {0.08f, 0.24f, 0.34f};
        basinWater.baseColorTexture = liminalPackageTexture("tile/RT_cyan_stained_glass.png");
        basinWater.emissiveColor = {0.04f, 0.14f, 0.22f};
        basinWater.textureTiling = {12.0f, 9.0f};
        basinWater.shadingModel = ShadingModel::Lit;
        basinWater.materialStyle = MaterialStyle::Crystal;
        basinWater.materialWorkflow = MaterialWorkflow::SpecGloss;
        basinWater.transparent = true;
        basinWater.opacity = 0.68f;
        basinWater.doubleSided = true;
        basinWater.roughness = 0.08f;
        basinWater.metallic = 0.02f;
        (void)AddPrimitiveNode(scene, basinWater);
    }

    LightNodeOptions westFill{};
    westFill.nodeName = "WestVoidFill";
    westFill.parent = world.handles.root;
    westFill.transform.position = ri::math::Vec3{-18.0f, 15.0f, 10.0f};
    westFill.light = Light{
        .name = "WestVoidFill",
        .type = LightType::Point,
        .color = ri::math::Vec3{0.72f, 0.74f, 0.76f},
        .intensity = 6.0f,
        .range = 52.0f,
    };
    (void)AddLightNode(scene, westFill);

    {
        std::ifstream colliders(gameRoot / "levels" / "assembly.colliders.csv");
        std::string line;
        while (std::getline(colliders, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const std::vector<std::string> tokens = SplitCsv(line);
            if (tokens.size() < 7U) {
                continue;
            }
            const ri::math::Vec3 center = ParseVec3(tokens, 1U, {});
            const ri::math::Vec3 extents = ParseVec3(tokens, 4U, ri::math::Vec3{1.0f, 1.0f, 1.0f});
            world.colliders.push_back(ri::trace::TraceCollider{
                .id = tokens[0],
                .bounds = ri::spatial::Aabb{
                    .min = center - extents,
                    .max = center + extents,
                },
                .structural = true,
            });
        }
    }

    ApplyLiminalRendererShowcaseMaterials(scene, engineTexturesRoot);
    return world;
}

void AnimateWorld(World& world, double elapsedSeconds) {
    (void)elapsedSeconds;
    world.scene.GetNode(world.handles.sun).localTransform.rotationDegrees =
        ri::math::Vec3{-58.0f, 42.0f, 0.0f};
    AnimateSceneMood(world.scene, elapsedSeconds);
}

StarterScene BuildEditorStarterScene(std::string_view sceneName, const fs::path& gameRoot) {
    World world = BuildWorld(sceneName, gameRoot);
    return StarterScene{
        .scene = std::move(world.scene),
        .handles = world.handles,
    };
}

void AnimateEditorStarterScene(StarterScene& starterScene, double elapsedSeconds) {
    (void)elapsedSeconds;
    starterScene.scene.GetNode(starterScene.handles.sun).localTransform.rotationDegrees =
        ri::math::Vec3{-58.0f, 42.0f, 0.0f};
    AnimateSceneMood(starterScene.scene, elapsedSeconds);
}

void SpawnEditorAuthoredLogicVisuals(World& world,
                                     const ri::logic::LogicAuthoringEditorFile& file,
                                     const fs::path& workspaceRoot) {
    if (world.handles.root == ri::scene::kInvalidHandle
        || (file.nodes.empty() && file.wires.empty() && file.triggers.empty())) {
        return;
    }

    const fs::path manifestPath = workspaceRoot / std::string(ri::logic::kLogicKitNodesJsonRelative);
    if (std::optional<ri::logic::LogicKitManifest> loaded = ri::logic::LoadLogicKitManifest(manifestPath)) {
        static std::unique_ptr<ri::logic::LogicKitManifest> s_editorKitManifest;
        s_editorKitManifest = std::make_unique<ri::logic::LogicKitManifest>(std::move(*loaded));
        ri::logic::SetActiveLogicKitManifest(s_editorKitManifest.get());
    }

    ri::scene::Scene& scene = world.scene;
    const int logicFolder = scene.CreateNode("EditorAuthoredLogic", world.handles.root);
    const ri::logic::LogicVisualLibrary library = ri::logic::BuildDefaultLogicVisualLibrary();

    auto registerHiddenLogicHandle = [&](const int handle,
                                         const ri::math::Vec3& visibleScale,
                                         const std::string& logicProbeId) {
        if (handle == ri::scene::kInvalidHandle) {
            return;
        }
        world.logicDemo.logicLayerNodes.push_back(handle);
        world.logicDemo.logicLayerNodeProbeIds.push_back(logicProbeId);
        world.logicDemo.logicLayerVisibleScales.push_back(visibleScale);
        scene.GetNode(handle).localTransform.scale = ri::math::Vec3{0.01f, 0.01f, 0.01f};
    };

    for (const ri::logic::LogicAuthoringEditorNodeRecord& record : file.nodes) {
        const ri::math::Vec3 position{record.position[0], record.position[1], record.position[2]};
        const std::array<float, 3> worldPos{position.x, position.y, position.z};
        bool importedKitMesh = false;

        if (const ri::logic::LogicKitManifest* kit = ri::logic::ActiveLogicKitManifest()) {
            if (const ri::logic::LogicKitNodeManifestEntry* entry =
                    ri::logic::FindLogicKitNodeManifestEntry(*kit, record.kitId)) {
                const fs::path glbPath = ri::logic::ResolveLogicKitGlbPath(manifestPath, entry->glbRelative);
                std::error_code ec{};
                if (fs::is_regular_file(glbPath, ec)) {
                    ri::scene::GltfModelOptions glb{};
                    glb.sourcePath = glbPath;
                    glb.wrapperNodeName = record.logicNodeId + "_Kit";
                    glb.parent = logicFolder;
                    glb.transform.position = position;
                    const int glbRoot = ri::scene::AddGltfModelNode(scene, glb);
                    registerHiddenLogicHandle(glbRoot, ri::math::Vec3{1.0f, 1.0f, 1.0f}, record.logicNodeId);
                    importedKitMesh = glbRoot != ri::scene::kInvalidHandle;
                }
            }
        }

        const std::vector<ri::logic::LogicVisualPrimitiveInstance> layoutInstances =
            ri::logic::BuildLogicVisualNodeInstances(library, record.kitId, record.logicNodeId, worldPos, false);
        for (const ri::logic::LogicVisualPrimitiveInstance& instance : layoutInstances) {
            if (importedKitMesh && instance.kind == ri::logic::LogicVisualPrimitiveKind::NodeBody) {
                continue;
            }
            const ri::math::Vec3 instancePos{instance.worldPosition[0], instance.worldPosition[1], instance.worldPosition[2]};
            const ri::math::Vec3 instanceScale{instance.worldScale[0], instance.worldScale[1], instance.worldScale[2]};
            const ri::math::Vec3 instanceColor{instance.color[0], instance.color[1], instance.color[2]};
            const ri::math::Vec3 instanceEmissive{instance.emissive[0], instance.emissive[1], instance.emissive[2]};
            ri::scene::PrimitiveNodeOptions options{};
            options.parent = logicFolder;
            options.primitive = ri::scene::PrimitiveType::Cube;
            options.shadingModel = ri::scene::ShadingModel::Unlit;
            options.nodeName = instance.id;
            options.materialName = std::string("logic_") + record.kitId;
            options.baseColor = instanceColor * 0.35f;
            options.emissiveColor = instanceEmissive * 0.85f;
            options.transform.position = instancePos;
            options.transform.scale = instanceScale;
            registerHiddenLogicHandle(
                ri::scene::AddPrimitiveNode(scene, options), instanceScale, record.logicNodeId);
        }
    }

    const ri::logic::LogicEditorPortLayout portLayout = ri::logic::BuildLogicEditorPortLayout(file);
    const int wireFolder = scene.CreateNode("EditorAuthoredLogicWires", logicFolder);
    int wireSerial = 0;
    constexpr float beadRadius = 0.10f;
    for (const ri::logic::LogicAuthoringEditorWireRecord& wire : file.wires) {
        const std::optional<std::array<float, 3>> from =
            ri::logic::ResolveLogicEditorWireEndpoint(portLayout, file, wire.sourceLogicId, wire.outputName, false);
        const std::optional<std::array<float, 3>> to =
            ri::logic::ResolveLogicEditorWireEndpoint(portLayout, file, wire.targetLogicId, wire.inputName, true);
        if (!from.has_value() || !to.has_value()) {
            continue;
        }
        const std::vector<std::array<float, 3>> beadPositions =
            ri::logic::BuildLogicWireBezierBeadPositions(*from, *to);
        for (const std::array<float, 3>& beadPos : beadPositions) {
            ri::scene::PrimitiveNodeOptions options{};
            options.parent = wireFolder;
            options.primitive = ri::scene::PrimitiveType::Sphere;
            options.shadingModel = ri::scene::ShadingModel::Unlit;
            options.nodeName = wire.wireId + "_bead_" + std::to_string(wireSerial++);
            options.materialName = "logic_wire_bead";
            options.baseColor = ri::math::Vec3{0.55f, 0.42f, 0.08f};
            options.emissiveColor = ri::math::Vec3{0.95f, 0.78f, 0.18f};
            options.transform.position =
                ri::math::Vec3{beadPos[0], beadPos[1], beadPos[2]};
            options.transform.scale = ri::math::Vec3{beadRadius, beadRadius, beadRadius};
            const int handle = ri::scene::AddPrimitiveNode(scene, options);
            const ri::math::Vec3 visibleScale{beadRadius, beadRadius, beadRadius};
            registerHiddenLogicHandle(handle, visibleScale, wire.sourceLogicId);
            world.logicDemo.logicWireVisualNodes.push_back(handle);
            world.logicDemo.logicWireProbeSources.push_back(wire.sourceLogicId);
        }
    }

    ri::core::LogInfo(
        "Spawned editor-authored logic visuals: " + std::to_string(file.nodes.size()) + " nodes, "
        + std::to_string(file.wires.size()) + " wires (layer hidden until debug).");
}

} // namespace ri::games::liminal
