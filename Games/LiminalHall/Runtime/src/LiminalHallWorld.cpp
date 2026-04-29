#include "RawIron/Games/LiminalHall/LiminalHallWorld.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Logic/LogicKitManifest.h"
#include "RawIron/Logic/LogicVisualPrimitives.h"

#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/ModelLoader.h"
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

std::vector<std::string> BuildImportedWaterAnimationFrames() {
    std::vector<std::string> frames;
    frames.reserve(147U);
    char buffer[64]{};
    for (int index = 0; index < 147; ++index) {
        std::snprintf(buffer, sizeof(buffer), "ri_psx_water_anim_%03d.png", index);
        frames.emplace_back(buffer);
    }
    return frames;
}

[[nodiscard]] std::vector<std::string> ResolvePsxWaterAnimationFrames(const fs::path& gameRoot) {
    const fs::path workspaceRoot = ri::content::DetectWorkspaceRoot(gameRoot);
    const fs::path waterDir = workspaceRoot / "Assets" / "Source" / "PSX_Water";
    const fs::path texturesRoot = workspaceRoot / "Assets" / "Textures";
    if (!fs::is_directory(waterDir) || !fs::is_directory(texturesRoot)) {
        return BuildImportedWaterAnimationFrames();
    }

    std::vector<fs::path> files;
    for (const fs::directory_entry& entry : fs::directory_iterator(waterDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const fs::path extension = entry.path().extension();
        if (extension.empty()) {
            continue;
        }
        std::string ext = extension.string();
        for (char& character : ext) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (ext != ".png") {
            continue;
        }
        files.push_back(entry.path());
    }
    if (files.empty()) {
        return BuildImportedWaterAnimationFrames();
    }
    std::sort(files.begin(), files.end());

    std::vector<std::string> relativeFrames;
    relativeFrames.reserve(files.size());
    std::error_code relativeError{};
    for (const fs::path& filePath : files) {
        fs::path relative = fs::relative(filePath, texturesRoot, relativeError);
        if (relativeError) {
            relative = fs::path("..") / "Source" / "PSX_Water" / filePath.filename();
        }
        relativeFrames.push_back(relative.generic_string());
    }
    return relativeFrames;
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

    PulseLight(scene, handles.gateLight, 8.0f, 15.0f, elapsedSeconds, 10.0);
    PulseLight(scene, handles.brainLight, 12.0f, 6.0f, elapsedSeconds, 5.0);
    PulseLight(scene, handles.sunLight, 20.0f, 8.0f, elapsedSeconds, 1.5);
}

} // namespace

namespace fs = std::filesystem;

using namespace ri::scene;

/// Maps engine logic node kinds (used by `LogicGraph` / port schema) to LogicKit manifest `id` values that ship
/// with `Assets/Source/LogicKit/glb/*.glb` meshes.
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
            const fs::path asPath(tex);
            if (asPath.is_absolute()) {
                return;
            }
            if (fs::exists(engineTexturesRoot / tex)) {
                return;
            }
            const fs::path kitAbs = (logicKitRoot / tex).lexically_normal();
            if (!fs::is_regular_file(kitAbs)) {
                return;
            }
            std::error_code relativeError{};
            fs::path relative = fs::relative(kitAbs, engineTexturesRoot, relativeError);
            if (!relativeError && !relative.empty()) {
                tex = relative.generic_string();
                return;
            }
            tex = (fs::path("..") / "Source" / "LogicKit" / fs::path(tex)).lexically_normal().generic_string();
        };
        fixTexturePath(mat.baseColorTexture);
        fixTexturePath(mat.emissiveTexture);
        fixTexturePath(mat.normalTexture);
        fixTexturePath(mat.ormTexture);
        fixTexturePath(mat.opacityTexture);
        fixTexturePath(mat.occlusionTexture);
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

    world.handles.root = scene.CreateNode("DreamEmulatorLayer");

    LightNodeOptions sun{};
    sun.nodeName = "VoidLight";
    sun.parent = world.handles.root;
    sun.transform.rotationDegrees = ri::math::Vec3{-60.0f, 45.0f, 0.0f};
    sun.light = Light{
        .name = "VoidLight",
        .type = LightType::Directional,
        .color = PaletteOr(palette, "void_ambient", ri::math::Vec3{0.18f, 0.05f, 0.25f}),
        .intensity = 1.8f,
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
        .target = ri::math::Vec3{0.0f, 2.0f, 0.0f},
        .distance = 25.0f,
        .yawDegrees = 45.0f,
        .pitchDegrees = -15.0f,
    };
    world.handles.orbitCamera = AddOrbitCamera(scene, orbitCamera);

    GridHelperOptions grid{};
    grid.parent = world.handles.root;
    grid.nodeName = "WireframeIllusion";
    grid.size = 100.0f;
    grid.transform.position = ri::math::Vec3{0.0f, -0.05f, 0.0f};
    grid.color = PaletteOr(palette, "grid_neon", ri::math::Vec3{0.1f, 1.0f, 0.2f});
    world.handles.grid = AddGridHelper(scene, grid);

    AxesHelperOptions axes{};
    axes.parent = world.handles.root;
    axes.axisLength = 2.0f;
    axes.axisThickness = 0.08f;
    axes.transform.position = ri::math::Vec3{0.0f, 0.01f, 0.0f};
    world.handles.axes = AddAxesHelper(scene, axes);

    const std::vector<std::string> importedAnimFrames = ResolvePsxWaterAnimationFrames(gameRoot);

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

    auto addAnimatedPrimitive = [&](const std::string& nodeName,
                                    PrimitiveType primitive,
                                    const ri::math::Vec3& position,
                                    const ri::math::Vec3& scale,
                                    const ri::math::Vec3& color,
                                    const std::vector<std::string>& frames,
                                    float framesPerSecond,
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
        primitiveOptions.baseColorTextureFrames = frames;
        primitiveOptions.baseColorTextureFramesPerSecond = framesPerSecond;
        primitiveOptions.textureTiling = tiling;
        primitiveOptions.transform.position = position;
        primitiveOptions.transform.scale = scale;
        primitiveOptions.transform.rotationDegrees = rotation;
        return AddPrimitiveNode(scene, primitiveOptions);
    };

    auto addCollider = [&](const std::string& id, const ri::math::Vec3& min, const ri::math::Vec3& max) {
        world.colliders.push_back(ri::trace::TraceCollider{
            .id = id,
            .bounds = ri::spatial::Aabb{.min = min, .max = max},
            .structural = true,
        });
    };

    world.handles.orbitCamera.orbit.target = ri::math::Vec3{0.0f, 5.0f, 0.0f};
    SetOrbitCameraState(scene, world.handles.orbitCamera, world.handles.orbitCamera.orbit);

    // --- LSD Dream Emulator 90s CGI Open Void ---

    world.handles.floor = addAnimatedPrimitive("AcidSea",
                                               PrimitiveType::Plane,
                                               ri::math::Vec3{0.0f, -4.0f, 0.0f},
                                               ri::math::Vec3{800.0f, 1.0f, 800.0f},
                                               ri::math::Vec3{0.2f, 1.0f, 0.0f},
                                               importedAnimFrames,
                                               24.0f,
                                               ri::math::Vec2{128.0f, 128.0f},
                                               "lsd-acid-sea",
                                               ShadingModel::Unlit);

    (void)addPrimitive("MainCheckerPlaza",
                       PrimitiveType::Plane,
                       ri::math::Vec3{0.0f, 0.0f, 0.0f},
                       ri::math::Vec3{40.0f, 1.0f, 40.0f},
                       ri::math::Vec3{1.0f, 0.1f, 0.8f},
                       "ri_prototype_checkers_04.png",
                       ri::math::Vec2{10.0f, 10.0f},
                       "lsd-main-plaza",
                       ShadingModel::Unlit);
    (void)addPrimitive("FleshRamp",
                       PrimitiveType::Cube,
                       ri::math::Vec3{0.0f, 6.0f, 35.0f},
                       ri::math::Vec3{8.0f, 1.0f, 40.0f},
                       ri::math::Vec3{0.9f, 0.5f, 0.5f},
                       "ri_psx_official_stone_08.png",
                       ri::math::Vec2{2.0f, 10.0f},
                       "lsd-flesh-ramp",
                       ShadingModel::Lit,
                       ri::math::Vec3{25.0f, 0.0f, 0.0f});
    (void)addPrimitive("SkyBridgeYellow",
                       PrimitiveType::Cube,
                       ri::math::Vec3{-45.0f, 12.0f, 15.0f},
                       ri::math::Vec3{30.0f, 0.5f, 6.0f},
                       ri::math::Vec3{1.0f, 0.9f, 0.1f},
                       "ri_psx_official_tile_21.png",
                       ri::math::Vec2{15.0f, 3.0f},
                       "lsd-yellow-bridge",
                       ShadingModel::Lit,
                       ri::math::Vec3{0.0f, -30.0f, 15.0f});
    (void)addPrimitive("CheckerObeliskLeft",
                       PrimitiveType::Cube,
                       ri::math::Vec3{-12.0f, 4.0f, 8.0f},
                       ri::math::Vec3{2.0f, 8.0f, 2.0f},
                       ri::math::Vec3{0.0f, 1.0f, 1.0f},
                       "ri_prototype_checkers_04.png",
                       ri::math::Vec2{2.0f, 8.0f},
                       "lsd-obelisk-left",
                       ShadingModel::Unlit);
    (void)addPrimitive("CheckerObeliskRight",
                       PrimitiveType::Cube,
                       ri::math::Vec3{12.0f, 4.0f, 8.0f},
                       ri::math::Vec3{2.0f, 8.0f, 2.0f},
                       ri::math::Vec3{0.0f, 1.0f, 1.0f},
                       "ri_prototype_checkers_04.png",
                       ri::math::Vec2{2.0f, 8.0f},
                       "lsd-obelisk-right",
                       ShadingModel::Unlit);
    (void)addPrimitive("FractalGate",
                       PrimitiveType::Cube,
                       ri::math::Vec3{0.0f, 6.0f, 15.0f},
                       ri::math::Vec3{4.0f, 6.0f, 0.5f},
                       ri::math::Vec3{1.0f, 1.0f, 1.0f},
                       "ri_prototype_texture_atlas.png",
                       ri::math::Vec2{4.0f, 4.0f},
                       "lsd-fractal-gate",
                       ShadingModel::Unlit);
    (void)addPrimitive("FractalGateHole",
                       PrimitiveType::Cube,
                       ri::math::Vec3{0.0f, 6.0f, 15.0f},
                       ri::math::Vec3{2.5f, 4.5f, 1.0f},
                       ri::math::Vec3{0.0f, 0.0f, 0.0f},
                       "ri_prototype_black.png",
                       ri::math::Vec2{1.0f, 1.0f},
                       "lsd-fractal-gate-hole",
                       ShadingModel::Unlit);
    (void)addPrimitive("PulsingBrainSphere",
                       PrimitiveType::Sphere,
                       ri::math::Vec3{25.0f, 15.0f, -20.0f},
                       ri::math::Vec3{10.0f, 10.0f, 10.0f},
                       ri::math::Vec3{0.9f, 0.2f, 0.4f},
                       "ri_psx_engine_caustics_atlas.png",
                       ri::math::Vec2{4.0f, 4.0f},
                       "lsd-brain-sphere",
                       ShadingModel::Lit);
    (void)addPrimitive("BrainHalo",
                       PrimitiveType::Cube,
                       ri::math::Vec3{25.0f, 15.0f, -20.0f},
                       ri::math::Vec3{16.0f, 0.2f, 16.0f},
                       ri::math::Vec3{1.0f, 1.0f, 0.0f},
                       "ri_prototype_yellow.png",
                       ri::math::Vec2{1.0f, 1.0f},
                       "lsd-brain-halo",
                       ShadingModel::Unlit);
    (void)addPrimitive("GlitchPyramid",
                       PrimitiveType::Cube,
                       ri::math::Vec3{-30.0f, 8.0f, 35.0f},
                       ri::math::Vec3{6.0f, 6.0f, 6.0f},
                       ri::math::Vec3{0.1f, 0.1f, 0.9f},
                       "ri_psx_official_metal_06.png",
                       ri::math::Vec2{1.0f, 1.0f},
                       "lsd-glitch-pyramid",
                       ShadingModel::Unlit,
                       ri::math::Vec3{45.0f, 45.0f, 0.0f});
    (void)addPrimitive("NeonSun",
                       PrimitiveType::Sphere,
                       ri::math::Vec3{0.0f, 30.0f, 80.0f},
                       ri::math::Vec3{15.0f, 15.0f, 15.0f},
                       ri::math::Vec3{1.0f, 0.5f, 0.0f},
                       "ri_prototype_orange_ochre.png",
                       ri::math::Vec2{1.0f, 1.0f},
                       "lsd-neon-sun",
                       ShadingModel::Unlit);
    (void)addPrimitive("NeonSunShell",
                       PrimitiveType::Sphere,
                       ri::math::Vec3{0.0f, 30.0f, 80.0f},
                       ri::math::Vec3{18.0f, 18.0f, 18.0f},
                       ri::math::Vec3{1.0f, 0.0f, 0.0f},
                       "ri_psx_wall_vent.png",
                       ri::math::Vec2{10.0f, 2.0f},
                       "lsd-neon-sun-shell",
                       ShadingModel::Unlit);

    // Logic demo chain: pressure plate -> door open -> portal spawn.
    const ri::math::Vec3 pressurePlatePos{0.0f, 0.08f, -1.5f};
    const ri::math::Vec3 pressurePlateScale{1.4f, 0.08f, 1.4f};
    world.logicDemo.pressurePlateNode =
        addPrimitive("LogicDemoPressurePlate",
                     PrimitiveType::Cube,
                     pressurePlatePos,
                     pressurePlateScale,
                     ri::math::Vec3{0.2f, 0.95f, 0.35f},
                     "ri_prototype_green.png",
                     ri::math::Vec2{1.0f, 1.0f},
                     "logic-demo-pressure-plate",
                     ShadingModel::Unlit);
    world.logicDemo.pressurePlateBounds = ri::spatial::Aabb{
        .min = pressurePlatePos - ri::math::Vec3{1.2f, 0.6f, 1.2f},
        .max = pressurePlatePos + ri::math::Vec3{1.2f, 1.8f, 1.2f},
    };

    world.logicDemo.doorClosedPosition = ri::math::Vec3{0.0f, 2.0f, 6.0f};
    world.logicDemo.doorOpenPosition = world.logicDemo.doorClosedPosition + ri::math::Vec3{0.0f, 4.5f, 0.0f};
    world.logicDemo.doorNode =
        addPrimitive("LogicDemoDoor",
                     PrimitiveType::Cube,
                     world.logicDemo.doorClosedPosition,
                     ri::math::Vec3{2.3f, 3.8f, 0.35f},
                     ri::math::Vec3{0.1f, 0.7f, 1.0f},
                     "ri_prototype_cyan.png",
                     ri::math::Vec2{1.0f, 2.0f},
                     "logic-demo-door",
                     ShadingModel::Lit);

    const ri::math::Vec3 portalPos{0.0f, 1.5f, 10.5f};
    world.logicDemo.portalNode =
        addPrimitive("LogicDemoPortal",
                     PrimitiveType::Cube,
                     portalPos,
                     ri::math::Vec3{0.01f, 0.01f, 0.01f},
                     ri::math::Vec3{0.85f, 0.2f, 1.0f},
                     "ri_prototype_magenta.png",
                     ri::math::Vec2{1.0f, 1.0f},
                     "logic-demo-portal",
                     ShadingModel::Unlit);
    world.logicDemo.portalBounds = ri::spatial::Aabb{
        .min = portalPos - ri::math::Vec3{1.0f, 1.5f, 0.8f},
        .max = portalPos + ri::math::Vec3{1.0f, 1.5f, 0.8f},
    };

    // Author LogicKit manifest (repo `Assets/Source/LogicKit`) drives port names and extra node kinds when present.
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
                         "ri_prototype_white.png",
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
                                    "ri_prototype_white.png",
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

    for (int i = 0; i < 4; ++i) {
        const float t = static_cast<float>(i) * 1.570796f;
        const float radius = 18.0f;
        const float x = std::cos(t) * radius;
        const float z = std::sin(t) * radius;
        LightNodeOptions wildLight{};
        wildLight.nodeName = "TripLight" + std::to_string(i);
        wildLight.parent = world.handles.root;
        wildLight.transform.position = ri::math::Vec3{x, 2.0f, z};
        std::array<ri::math::Vec3, 4> colors = {
            ri::math::Vec3{1.0f, 0.0f, 1.0f},
            ri::math::Vec3{0.0f, 1.0f, 1.0f},
            ri::math::Vec3{0.0f, 1.0f, 0.0f},
            ri::math::Vec3{1.0f, 1.0f, 0.0f},
        };
        wildLight.light = Light{
            .name = "TripLight" + std::to_string(i),
            .type = LightType::Point,
            .color = colors[static_cast<std::size_t>(i)],
            .intensity = 15.0f,
            .range = 30.0f,
        };
        (void)AddLightNode(scene, wildLight);
    }

    LightNodeOptions gateLight{};
    gateLight.nodeName = "GateLight";
    gateLight.parent = world.handles.root;
    gateLight.transform.position = ri::math::Vec3{0.0f, 6.0f, 15.0f};
    gateLight.light = Light{
        .name = "GateLight",
        .type = LightType::Point,
        .color = ri::math::Vec3{1.0f, 1.0f, 1.0f},
        .intensity = 20.0f,
        .range = 40.0f,
    };
    (void)AddLightNode(scene, gateLight);

    LightNodeOptions brainLight{};
    brainLight.nodeName = "BrainLight";
    brainLight.parent = world.handles.root;
    brainLight.transform.position = ri::math::Vec3{25.0f, 15.0f, -20.0f};
    brainLight.light = Light{
        .name = "BrainLight",
        .type = LightType::Point,
        .color = ri::math::Vec3{1.0f, 0.1f, 0.1f},
        .intensity = 18.0f,
        .range = 50.0f,
    };
    (void)AddLightNode(scene, brainLight);

    LightNodeOptions sunLight{};
    sunLight.nodeName = "SunLight";
    sunLight.parent = world.handles.root;
    sunLight.transform.position = ri::math::Vec3{0.0f, 30.0f, 80.0f};
    sunLight.light = Light{
        .name = "SunLight",
        .type = LightType::Point,
        .color = ri::math::Vec3{1.0f, 0.6f, 0.0f},
        .intensity = 25.0f,
        .range = 100.0f,
    };
    (void)AddLightNode(scene, sunLight);

    world.colliders.clear();
    addCollider("acid-sea-death-plane", ri::math::Vec3{-400.0f, -4.5f, -400.0f}, ri::math::Vec3{400.0f, -3.8f, 400.0f});
    addCollider("main-plaza", ri::math::Vec3{-20.0f, -0.5f, -20.0f}, ri::math::Vec3{20.0f, 0.5f, 20.0f});
    addCollider("flesh-ramp-base", ri::math::Vec3{-4.0f, 0.0f, 15.0f}, ri::math::Vec3{4.0f, 1.5f, 25.0f});
    addCollider("flesh-ramp-mid", ri::math::Vec3{-4.0f, 1.5f, 25.0f}, ri::math::Vec3{4.0f, 4.5f, 35.0f});
    addCollider("flesh-ramp-top", ri::math::Vec3{-4.0f, 4.5f, 35.0f}, ri::math::Vec3{4.0f, 8.5f, 45.0f});

    world.playerRig = scene.CreateNode("PlayerRig", world.handles.root);
    scene.GetNode(world.playerRig).localTransform.position = ri::math::Vec3{0.0f, 1.0f, 0.0f};
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
        std::ifstream assembly(gameRoot / "levels" / "assembly.primitives.csv");
        std::string line;
        while (std::getline(assembly, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }
            const std::vector<std::string> tokens = SplitCsv(line);
            if (tokens.size() < 11U) {
                continue;
            }
            PrimitiveNodeOptions extra{};
            extra.nodeName = tokens[0];
            extra.parent = world.handles.root;
            extra.primitive = (tokens[1] == "plane") ? PrimitiveType::Plane : PrimitiveType::Cube;
            extra.materialName = "lsd-assembly";
            extra.transform.position = ParseVec3(tokens, 2U, {});
            extra.transform.scale = ParseVec3(tokens, 5U, ri::math::Vec3{1.0f, 1.0f, 1.0f});
            extra.baseColor = ParseVec3(tokens, 8U, ri::math::Vec3{1.0f, 0.0f, 1.0f});
            if (tokens.size() > 11U && tokens[11] == "unlit") {
                extra.shadingModel = ShadingModel::Unlit;
            }
            if (tokens.size() > 12U && !tokens[12].empty() && tokens[12] != "-") {
                extra.baseColorTexture = tokens[12];
            }
            if (tokens.size() > 14U) {
                float tileX = 1.0f;
                float tileY = 1.0f;
                if (ParseFloat(tokens[13], tileX) && ParseFloat(tokens[14], tileY) &&
                    std::isfinite(tileX) && std::isfinite(tileY) &&
                    tileX > 0.0f && tileY > 0.0f) {
                    extra.textureTiling = ri::math::Vec2{tileX, tileY};
                }
            }
            if (tokens.size() > 17U) {
                extra.transform.rotationDegrees = ParseVec3(tokens, 15U, {});
            }
            (void)AddPrimitiveNode(scene, extra);
        }
    }

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

    {
        const fs::path importedModel = workspaceRoot / "Assets" / "Source" / "psx_retro_dolphin.glb";
        if (fs::exists(importedModel)) {
            std::string importError;
            (void)AddModelNode(scene,
                               ImportedModelOptions{
                                   .sourcePath = importedModel,
                                   .nodeName = "Floating90sDolphin",
                                   .parent = world.handles.root,
                                   .transform =
                                       Transform{
                                           .position = ri::math::Vec3{0.0f, 8.0f, 0.0f},
                                           .rotationDegrees = ri::math::Vec3{0.0f, 90.0f, 45.0f},
                                           .scale = ri::math::Vec3{15.0f, 15.0f, 15.0f},
                                       },
                               },
                               &importError);
        }
    }

    return world;
}

void AnimateWorld(World& world, double elapsedSeconds) {
    const float lightSwing = static_cast<float>(std::fmod(elapsedSeconds * 45.0, 360.0));
    world.scene.GetNode(world.handles.sun).localTransform.rotationDegrees =
        ri::math::Vec3{-60.0f, lightSwing, 0.0f};
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
    const float lightSwing = static_cast<float>(std::fmod(elapsedSeconds * 45.0, 360.0));
    starterScene.scene.GetNode(starterScene.handles.sun).localTransform.rotationDegrees =
        ri::math::Vec3{-60.0f, lightSwing, 0.0f};
    AnimateSceneMood(starterScene.scene, elapsedSeconds);
}

} // namespace ri::games::liminal
