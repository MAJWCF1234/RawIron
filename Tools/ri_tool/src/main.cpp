#include "RawIron/Core/CommandLine.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GamePackageRequirements.h"
#include "RawIron/Content/ExtensionDescriptor.h"
#include "RawIron/Content/PluginPackage.h"
#include "RawIron/Content/PluginProjectData.h"
#include "RawIron/Content/PluginRuntime.h"
#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/AssetPackageManifest.h"
#include "RawIron/Content/AuthoringHandoff.h"
#include "RawIron/Content/BlockCharacterDocument.h"
#include "RawIron/Content/NativeAnimationDocument.h"
#include "RawIron/Content/NativeSculptDocument.h"
#include "RawIron/Content/PackageMountRegistry.h"
#include "RawIron/Content/PackageResolver.h"
#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Core/Version.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Render/PostProcessProfiles.h"
#include "RawIron/Render/VulkanBootstrap.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Scene/NativeAnimation.h"
#include "RawIron/Scene/NativeSculpt.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Scene/SceneKit.h"
#include "RawIron/Scene/SceneStateIO.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/Transform.h"
#include "EditorProjectScaffolding.h"
#include "EditorWorkspace.h"
#include "SecureRipakArchive.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <process.h>
#else
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;
namespace json_scan = ri::core::detail;

struct WorkspaceLayout {
    fs::path root;
    fs::path documentation;
    fs::path source;
    fs::path apps;
    fs::path tools;
    fs::path config;
    fs::path assetsSource;
    fs::path assetsCooked;
    fs::path projects;
    fs::path sandboxProject;
    fs::path sandboxContent;
    fs::path sandboxScenes;
    fs::path sandboxSaved;
    fs::path saved;
    fs::path scripts;
    fs::path thirdParty;
};

struct SceneReferenceTarget {
    const char* slug;
    const char* title;
    const char* url;
    const char* rawIronTrack;
    const char* status;
};

struct VulkanToolingDiagnostics {
    std::string sdkRoot;
    std::vector<std::string> availableTools;
};

struct ProjectCommandContext {
    fs::path workspaceRoot;
    ri::content::GameManifest manifest{};
};

constexpr std::array<SceneReferenceTarget, 11> kSceneReferenceTargets = {{
    {"scene_controls_orbit", "Orbit controls", "reference://scene_controls_orbit",
     "orbit camera + helpers + viewport shell", "foundation-live"},
    {"scene_geometry_cube", "Geometry cube", "reference://scene_geometry_cube",
     "primitive mesh nodes + materials + transforms", "foundation-live"},
    {"scene_interactive_cubes", "Interactive cubes", "reference://scene_interactive_cubes",
     "scene raycast utilities + primitive picking + input shell", "foundation-live"},
    {"scene_terrain_raycast", "Terrain raycasting", "reference://scene_terrain_raycast",
     "scene raycast utilities + custom terrain mesh preview", "preview-live"},
    {"scene_lighting_spotlights", "Spot lights", "reference://scene_lighting_spotlights",
     "light descriptors + renderer spot-light path", "preview-live"},
    {"scene_loader_gltf", "GLTF loader", "reference://scene_loader_gltf",
     "asset import pipeline + scene instantiation", "preview-live"},
    {"scene_animation_keyframes", "Animation keyframes", "reference://scene_animation_keyframes",
     "scene-authored keyframe sampling preview", "preview-live"},
    {"scene_instancing_performance", "Instancing performance", "reference://scene_instancing_performance",
     "repeated-node density preview for future instance submission", "preview-live"},
    {"scene_materials_envmaps", "Environment maps", "reference://scene_materials_envmaps",
     "reflection-bay material staging preview", "preview-live"},
    {"scene_audio_orientation", "Positional audio orientation", "reference://scene_audio_orientation",
     "listener/source layout preview for future spatial audio", "preview-live"},
    {"scene_particles", "Particle fountain", "reference://scene_particles",
     "cpu particle simulation + mesh instance batch preview", "preview-live"},
}};

std::string GetEnvironmentVariable(const char* name) {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t valueLength = 0;
    if (_dupenv_s(&value, &valueLength, name) != 0 || value == nullptr) {
        return {};
    }
    const std::string result(value);
    free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
#endif
}

std::string FormatHex32(std::uint32_t value) {
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << value;
    return stream.str();
}

#if defined(__linux__)
std::optional<fs::path> FindToolOnPath(std::string_view toolName) {
    const std::string pathValue = GetEnvironmentVariable("PATH");
    if (pathValue.empty()) {
        return std::nullopt;
    }

#if defined(_WIN32)
    constexpr char kPathSeparator = ';';
#else
    constexpr char kPathSeparator = ':';
#endif

    std::size_t offset = 0;
    while (offset <= pathValue.size()) {
        const std::size_t next = pathValue.find(kPathSeparator, offset);
        const std::string_view segment = next == std::string::npos
            ? std::string_view(pathValue).substr(offset)
            : std::string_view(pathValue).substr(offset, next - offset);
        if (!segment.empty()) {
            const fs::path candidate = fs::path(std::string(segment)) / std::string(toolName);
            if (fs::exists(candidate)) {
                return candidate;
            }
        }
        if (next == std::string::npos) {
            break;
        }
        offset = next + 1;
    }

    return std::nullopt;
}
#endif

void CollectKnownVulkanTools(const fs::path& baseDirectory, std::vector<std::string>& output) {
#if defined(_WIN32)
    constexpr std::array<const char*, 4> kToolNames = {"vkcube.exe", "glslc.exe", "vkconfig.exe", "vulkaninfoSDK.exe"};
#else
    constexpr std::array<const char*, 4> kToolNames = {"vkcube", "glslc", "vkconfig", "vulkaninfo"};
#endif
    for (const char* toolName : kToolNames) {
        const fs::path toolPath = baseDirectory / toolName;
        if (fs::exists(toolPath)) {
            output.push_back(toolPath.string());
        }
    }
}

VulkanToolingDiagnostics CollectVulkanToolingDiagnostics() {
    VulkanToolingDiagnostics diagnostics{};
    diagnostics.sdkRoot = GetEnvironmentVariable("VULKAN_SDK");

    if (!diagnostics.sdkRoot.empty()) {
#if defined(_WIN32)
        CollectKnownVulkanTools(fs::path(diagnostics.sdkRoot) / "Bin", diagnostics.availableTools);
#else
        CollectKnownVulkanTools(fs::path(diagnostics.sdkRoot) / "bin", diagnostics.availableTools);
        CollectKnownVulkanTools(fs::path(diagnostics.sdkRoot) / "Bin", diagnostics.availableTools);
#endif
    }

#if defined(__linux__)
    for (const char* toolName : {"vkcube", "glslc", "vkconfig", "vulkaninfo"}) {
        if (const auto tool = FindToolOnPath(toolName); tool.has_value()) {
            diagnostics.availableTools.push_back(tool->string());
        }
    }
#endif

    std::sort(diagnostics.availableTools.begin(), diagnostics.availableTools.end());
    diagnostics.availableTools.erase(
        std::unique(diagnostics.availableTools.begin(), diagnostics.availableTools.end()),
        diagnostics.availableTools.end());
    return diagnostics;
}

bool LooksLikeWorkspaceRoot(const fs::path& path) {
    return fs::exists(path / "CMakeLists.txt") &&
           fs::exists(path / "Source") &&
           (fs::exists(path / "Documentation") || fs::exists(path / "Games"));
}

fs::path DetectWorkspaceRoot(const ri::core::CommandLine& commandLine) {
    if (const auto rootArg = commandLine.GetValue("--root"); rootArg.has_value()) {
        return fs::weakly_canonical(fs::path(*rootArg));
    }

    fs::path current = fs::current_path();
    while (!current.empty()) {
        if (LooksLikeWorkspaceRoot(current)) {
            return current;
        }

        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    return fs::current_path();
}

WorkspaceLayout BuildWorkspaceLayout(const fs::path& root) {
    WorkspaceLayout layout{};
    layout.root = root;
    layout.documentation = root / "Documentation";
    layout.source = root / "Source";
    layout.apps = root / "Apps";
    layout.tools = root / "Tools";
    layout.config = root / "Config";
    layout.assetsSource = root / "Assets" / "Source";
    layout.assetsCooked = root / "Assets" / "Cooked";
    layout.projects = root / "Projects";
    layout.sandboxProject = root / "Projects" / "Sandbox";
    layout.sandboxContent = layout.sandboxProject / "Content";
    layout.sandboxScenes = layout.sandboxProject / "Scenes";
    layout.sandboxSaved = layout.sandboxProject / "Saved";
    layout.saved = root / "Saved";
    layout.scripts = root / "Scripts";
    layout.thirdParty = root / "ThirdParty";
    return layout;
}

std::vector<fs::path> RequiredWorkspacePaths(const WorkspaceLayout& layout) {
    return {
        layout.documentation,
        layout.source,
        layout.apps,
        layout.tools,
        layout.config,
        layout.assetsSource,
        layout.assetsCooked,
        layout.projects,
        layout.sandboxProject,
        layout.sandboxContent,
        layout.sandboxScenes,
        layout.sandboxSaved,
        layout.saved,
        layout.scripts,
        layout.thirdParty,
    };
}

void EnsureParentDirectoryExists(const fs::path& path) {
    const fs::path parent = path.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent);
    }
}

std::string TrimAscii(std::string value) {
    const auto notSpace = [](unsigned char c) {
        return c != ' ' && c != '\t' && c != '\r' && c != '\n';
    };
    while (!value.empty() && !notSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && !notSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string CurrentUtcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &nowTime);
#else
    gmtime_r(&nowTime, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

std::string SanitizeAssetId(std::string_view raw) {
    std::string id;
    id.reserve(raw.size());
    for (char c : raw) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            id.push_back(static_cast<char>(std::tolower(uc)));
        } else {
            id.push_back('_');
        }
    }
    while (!id.empty() && id.front() == '_') {
        id.erase(id.begin());
    }
    while (!id.empty() && id.back() == '_') {
        id.pop_back();
    }
    return id.empty() ? std::string("asset") : id;
}

std::optional<float> ParseFloatToken(std::string_view raw) {
    const std::string trimmed = TrimAscii(std::string(raw));
    if (trimmed.empty()) {
        return std::nullopt;
    }
    try {
        return std::stof(trimmed);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> ParseIntToken(std::string_view raw) {
    const std::string trimmed = TrimAscii(std::string(raw));
    if (trimmed.empty()) {
        return std::nullopt;
    }
    try {
        return std::stoi(trimmed);
    } catch (...) {
        return std::nullopt;
    }
}

std::string JoinStrings(const std::vector<std::string>& values, std::string_view separator) {
    std::string joined;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            joined += separator;
        }
        joined += values[index];
    }
    return joined;
}

bool TryParseWorkspaceResourceCategory(std::string_view rawValue,
                                       ri::editor::WorkspaceResourceCategory& category) {
    const std::string value = ToLowerAscii(std::string(rawValue));
    if (value == "manifest" || value == "manifests") {
        category = ri::editor::WorkspaceResourceCategory::Manifest;
        return true;
    }
    if (value == "level" || value == "levels") {
        category = ri::editor::WorkspaceResourceCategory::Level;
        return true;
    }
    if (value == "script" || value == "scripts") {
        category = ri::editor::WorkspaceResourceCategory::Script;
        return true;
    }
    if (value == "test" || value == "tests") {
        category = ri::editor::WorkspaceResourceCategory::Test;
        return true;
    }
    if (value == "ui" || value == "screen" || value == "screens" || value == "ui-screen") {
        category = ri::editor::WorkspaceResourceCategory::UiScreen;
        return true;
    }
    if (value == "menu" || value == "menus") {
        category = ri::editor::WorkspaceResourceCategory::Menu;
        return true;
    }
    if (value == "asset" || value == "assets") {
        category = ri::editor::WorkspaceResourceCategory::Asset;
        return true;
    }
    if (value == "other") {
        category = ri::editor::WorkspaceResourceCategory::Other;
        return true;
    }
    return false;
}

struct Vec3Scalar {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

std::optional<Vec3Scalar> ParseYamlVec3(std::string_view line) {
    const std::size_t openBrace = line.find('{');
    const std::size_t closeBrace = line.find('}');
    if (openBrace == std::string_view::npos || closeBrace == std::string_view::npos || closeBrace <= openBrace) {
        return std::nullopt;
    }
    const std::string_view body = line.substr(openBrace + 1, closeBrace - openBrace - 1);
    const std::size_t xPos = body.find("x:");
    const std::size_t yPos = body.find("y:");
    const std::size_t zPos = body.find("z:");
    if (xPos == std::string_view::npos || yPos == std::string_view::npos || zPos == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t xEnd = body.find(',', xPos);
    const std::size_t yEnd = body.find(',', yPos);
    const std::string_view xRaw = body.substr(xPos + 2, xEnd == std::string_view::npos ? body.size() - (xPos + 2) : xEnd - (xPos + 2));
    const std::string_view yRaw = body.substr(yPos + 2, yEnd == std::string_view::npos ? body.size() - (yPos + 2) : yEnd - (yPos + 2));
    const std::string_view zRaw = body.substr(zPos + 2);
    const std::optional<float> x = ParseFloatToken(xRaw);
    const std::optional<float> y = ParseFloatToken(yRaw);
    const std::optional<float> z = ParseFloatToken(zRaw);
    if (!x.has_value() || !y.has_value() || !z.has_value()) {
        return std::nullopt;
    }
    return Vec3Scalar{*x, *y, *z};
}

struct UnityMeshAssetSummary {
    std::string name{};
    int subMeshCount = 0;
    int totalIndexCount = 0;
    int totalVertexCount = 0;
    bool hasBounds = false;
    Vec3Scalar center{};
    Vec3Scalar extent{};
};

std::optional<UnityMeshAssetSummary> TryParseUnityMeshAsset(const fs::path& sourcePath) {
    std::ifstream input(sourcePath, std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }

    UnityMeshAssetSummary summary{};
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = TrimAscii(line);
        if (trimmed.rfind("m_Name:", 0) == 0 && summary.name.empty()) {
            summary.name = TrimAscii(trimmed.substr(std::string("m_Name:").size()));
            continue;
        }
        if (trimmed.rfind("indexCount:", 0) == 0) {
            if (const std::optional<int> indexCount = ParseIntToken(trimmed.substr(std::string("indexCount:").size()))) {
                summary.totalIndexCount += std::max(*indexCount, 0);
                ++summary.subMeshCount;
            }
            continue;
        }
        if (trimmed.rfind("vertexCount:", 0) == 0) {
            if (const std::optional<int> vertexCount = ParseIntToken(trimmed.substr(std::string("vertexCount:").size()))) {
                summary.totalVertexCount += std::max(*vertexCount, 0);
            }
            continue;
        }
        if (trimmed.rfind("m_Center:", 0) == 0) {
            if (const std::optional<Vec3Scalar> parsed = ParseYamlVec3(trimmed)) {
                summary.center = *parsed;
                summary.hasBounds = true;
            }
            continue;
        }
        if (trimmed.rfind("m_Extent:", 0) == 0) {
            if (const std::optional<Vec3Scalar> parsed = ParseYamlVec3(trimmed)) {
                summary.extent = *parsed;
                summary.hasBounds = true;
            }
            continue;
        }
    }

    if (summary.name.empty() && summary.subMeshCount == 0 && summary.totalIndexCount == 0) {
        return std::nullopt;
    }
    if (summary.name.empty()) {
        summary.name = sourcePath.stem().string();
    }
    return summary;
}

std::string InferAssetTypeFromExtension(const fs::path& sourcePath) {
    const std::string extension = ToLowerAscii(sourcePath.extension().string());
    const std::string stem = ToLowerAscii(sourcePath.stem().string());
    const std::string filename = ToLowerAscii(sourcePath.filename().string());
    if (filename.ends_with(".ri_rig.json")) {
        return "rig";
    }
    if (extension == ".uasset") {
        if (stem.rfind("m_", 0) == 0) {
            return "material";
        }
        if (stem.rfind("sm_", 0) == 0 || stem.rfind("sk_", 0) == 0) {
            return "mesh";
        }
        if (stem.rfind("t_", 0) == 0 || stem.find("texture") != std::string::npos) {
            return "texture";
        }
        return "unreal-asset";
    }
    if (extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb" || extension == ".blend"
        || extension == ".asset" || extension == ".spm") {
        return "mesh";
    }
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga"
        || extension == ".bmp" || extension == ".hdr" || extension == ".tif" || extension == ".tiff") {
        return "texture";
    }
    if (extension == ".wav" || extension == ".ogg" || extension == ".mp3" || extension == ".flac") {
        return "audio";
    }
    if (extension == ".mat") {
        return "material";
    }
    if (extension == ".riscript" || extension == ".lua" || extension == ".cs" || extension == ".js"
        || extension == ".boo") {
        return "script";
    }
    if (extension == ".prefab") {
        return "prefab";
    }
    if (extension == ".anim" || extension == ".controller" || extension == ".overridecontroller") {
        return "animation";
    }
    if (extension == ".shader" || extension == ".hlsl" || extension == ".glsl") {
        return "shader";
    }
    if (extension == ".unity" || extension == ".scene" || extension == ".ri_scene") {
        return "scene";
    }
    if (extension == ".zip" || extension == ".ripak" || extension == ".unitypackage" || extension == ".tar"
        || extension == ".gz" || extension == ".7z") {
        return "archive";
    }
    return "generic";
}

std::string BuildUnityMeshPayloadJson(const UnityMeshAssetSummary& meshSummary) {
    std::ostringstream payload;
    payload << "{";
    payload << "\"sourceFormat\":\"unity-mesh-yaml\",";
    payload << "\"meshName\":\"" << json_scan::EscapeJsonString(meshSummary.name) << "\",";
    payload << "\"subMeshCount\":" << meshSummary.subMeshCount << ",";
    payload << "\"indexCount\":" << meshSummary.totalIndexCount << ",";
    payload << "\"vertexCount\":" << meshSummary.totalVertexCount;
    if (meshSummary.hasBounds) {
        payload << ",\"bounds\":{\"center\":{\"x\":" << meshSummary.center.x
                << ",\"y\":" << meshSummary.center.y
                << ",\"z\":" << meshSummary.center.z
                << "},\"extent\":{\"x\":" << meshSummary.extent.x
                << ",\"y\":" << meshSummary.extent.y
                << ",\"z\":" << meshSummary.extent.z
                << "}}";
    }
    payload << "}";
    return payload.str();
}

std::string BuildForeignScriptPayloadJson(const fs::path& sourcePath) {
    std::ostringstream payload;
    payload << "{";
    payload << "\"sourceFormat\":\"" << json_scan::EscapeJsonString(ToLowerAscii(sourcePath.extension().string())) << "\",";
    payload << "\"reconstructedFormat\":\"riscript\",";
    payload << "\"reconstructionStatus\":\"generated-review-required\"";
    payload << "}";
    return payload.str();
}

std::vector<std::string> ExtractPrintableTokensFromBinary(const fs::path& sourcePath,
                                                          const std::size_t maxTokens) {
    std::ifstream input(sourcePath, std::ios::binary);
    if (!input) {
        return {};
    }

    std::vector<std::string> tokens;
    std::string current;
    char ch = '\0';
    while (input.get(ch)) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        if (uc >= 32U && uc <= 126U) {
            current.push_back(static_cast<char>(uc));
            continue;
        }
        if (current.size() >= 4U
            && std::find(tokens.begin(), tokens.end(), current) == tokens.end()) {
            tokens.push_back(current);
            if (tokens.size() >= maxTokens) {
                break;
            }
        }
        current.clear();
    }
    if (tokens.size() < maxTokens && current.size() >= 4U
        && std::find(tokens.begin(), tokens.end(), current) == tokens.end()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string BuildStringArrayJson(const std::vector<std::string>& values) {
    std::ostringstream json;
    json << "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0U) {
            json << ",";
        }
        json << "\"" << json_scan::EscapeJsonString(values[index]) << "\"";
    }
    json << "]";
    return json.str();
}

std::string BuildUnrealAssetPayloadJson(const fs::path& sourcePath) {
    const std::vector<std::string> tokens = ExtractPrintableTokensFromBinary(sourcePath, 80U);
    std::ostringstream payload;
    payload << "{";
    payload << "\"sourceFormat\":\"unreal-uasset\",";
    payload << "\"conversionStatus\":\"metadata-extracted-reconstruct-required\",";
    payload << "\"tokens\":" << BuildStringArrayJson(tokens);
    payload << "}";
    return payload.str();
}

std::string BuildBlenderAssetPayloadJson() {
    return "{\"sourceFormat\":\"blender-blend\",\"conversionStatus\":\"authoring-source-export-required\","
           "\"preferredExports\":[\"gltf\",\"glb\",\"fbx\"],\"runtimePolicy\":\"do-not-ship-authoring-container\"}";
}

std::string BuildRigAssetPayloadJson(const fs::path& sourcePath) {
    std::ostringstream payload;
    payload << "{\"sourceFormat\":\"rawiron-rig\"";
    const std::optional<ri::scene::RigDefinition> rig = ri::scene::LoadRigDefinition(sourcePath);
    if (!rig.has_value()) {
        payload << ",\"validation\":\"parse-failed\"}";
        return payload.str();
    }
    const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(*rig);
    payload << ",\"profile\":\"" << ri::scene::RigProfileName(rig->profile) << "\"";
    payload << ",\"boneCount\":" << rig->bones.size();
    payload << ",\"rootBoneCount\":" << report.rootBoneCount;
    payload << ",\"validation\":\"" << (report.valid ? "valid" : "invalid") << "\"";
    if (rig->profile == ri::scene::RigProfile::Humanoid) {
        payload << ",\"humanoidCoverage\":{\"matched\":" << report.humanoidMatchedBoneCount
                << ",\"required\":" << report.humanoidRequiredBoneCount << '}';
    }
    payload << '}';
    return payload.str();
}

bool IsForeignScriptExtension(const std::string& extension) {
    return extension == ".cs" || extension == ".lua" || extension == ".js" || extension == ".boo";
}

std::string ReconstructRiscriptFromForeignScript(const fs::path& sourcePath,
                                                 const std::string_view assetId,
                                                 const std::string_view sourceRelativePath) {
    std::ostringstream script;
    script << "# RawIron reconstructed script\n";
    script << "# source: " << sourceRelativePath << "\n";
    script << "# asset: " << assetId << "\n";
    script << "# status: generated-review-required\n\n";
    script << "script \"" << assetId << "\" {\n";
    script << "  source_format = \"" << ToLowerAscii(sourcePath.extension().string()) << "\"\n";
    script << "  source_path = \"" << sourceRelativePath << "\"\n";
    script << "  lifecycle = [\"awake\", \"start\", \"update\", \"fixed_update\", \"late_update\"]\n";
    script << "  reconstruction = \"metadata_stub\"\n";
    script << "}\n";
    return script.str();
}

std::optional<fs::path> TryRelativeToRoot(const fs::path& absolutePath, const fs::path& root) {
    std::error_code ec{};
    const fs::path relative = fs::relative(absolutePath, root, ec);
    if (!ec && !relative.empty()) {
        return relative.lexically_normal();
    }
    return std::nullopt;
}

ri::content::AssetDocument BuildStandardAssetDocument(const fs::path& sourcePath, const WorkspaceLayout& workspace) {
    ri::content::AssetDocument document{};
    const fs::path normalizedSource = fs::weakly_canonical(sourcePath);
    const std::string extension = ToLowerAscii(normalizedSource.extension().string());
    const bool isRig = ToLowerAscii(normalizedSource.filename().string()).ends_with(".ri_rig.json");
    const std::optional<fs::path> relative = TryRelativeToRoot(normalizedSource, workspace.root);

    document.id = SanitizeAssetId(normalizedSource.stem().string());
    document.type = InferAssetTypeFromExtension(normalizedSource);
    document.displayName = normalizedSource.stem().string();
    document.sourcePath = relative.has_value() ? relative->generic_string() : normalizedSource.generic_string();
    document.references.push_back(ri::content::AssetReference{
        .kind = "source",
        .id = document.id + "_source",
        .path = document.sourcePath,
    });

    if (isRig) {
        document.type = "rig";
        document.payloadJson = BuildRigAssetPayloadJson(normalizedSource);
    } else if (extension == ".asset") {
        if (const std::optional<UnityMeshAssetSummary> meshSummary = TryParseUnityMeshAsset(normalizedSource)) {
            document.type = "mesh";
            document.payloadJson = BuildUnityMeshPayloadJson(*meshSummary);
        } else {
            document.payloadJson = "{\"sourceFormat\":\"unity-asset\"}";
        }
    } else if (extension == ".uasset") {
        document.payloadJson = BuildUnrealAssetPayloadJson(normalizedSource);
    } else if (extension == ".blend") {
        document.payloadJson = BuildBlenderAssetPayloadJson();
    } else if (extension == ".cs" || extension == ".lua" || extension == ".js" || extension == ".boo") {
        document.type = "script";
        document.payloadJson = BuildForeignScriptPayloadJson(normalizedSource);
    } else {
        document.payloadJson = "{\"sourceFormat\":\"native\"}";
    }

    return document;
}

fs::path DefaultStandardizedOutputPath(const WorkspaceLayout& workspace, const fs::path& sourcePath) {
    const fs::path sourceName = sourcePath.filename();
    return workspace.assetsCooked / "Standardized" / (sourceName.string() + ".ri_asset.json");
}

bool ShouldStandardizeExtension(const fs::path& path) {
    const std::string extension = ToLowerAscii(path.extension().string());
    if (ToLowerAscii(path.filename().string()).ends_with(".ri_rig.json")) {
        return true;
    }
    return extension == ".asset" || extension == ".spm" || extension == ".fbx" || extension == ".obj"
        || extension == ".gltf" || extension == ".glb" || extension == ".blend" || extension == ".png" || extension == ".jpg"
        || extension == ".jpeg" || extension == ".tga" || extension == ".bmp" || extension == ".hdr"
        || extension == ".tif" || extension == ".tiff" || extension == ".wav" || extension == ".ogg"
        || extension == ".mp3" || extension == ".flac" || extension == ".mat" || extension == ".unity"
        || extension == ".riscript" || extension == ".lua" || extension == ".cs"
        || extension == ".js" || extension == ".boo" || extension == ".prefab"
        || extension == ".anim" || extension == ".controller" || extension == ".overridecontroller"
        || extension == ".shader" || extension == ".hlsl" || extension == ".glsl";
}

int StandardizeAssetDirectoryToOutput(const WorkspaceLayout& workspace,
                                      const fs::path& sourceDirectory,
                                      const fs::path& outputDirectory) {
    fs::create_directories(outputDirectory);

    int convertedCount = 0;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(sourceDirectory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const fs::path sourcePath = entry.path();
        if (!ShouldStandardizeExtension(sourcePath)) {
            continue;
        }

        const std::optional<fs::path> relativePath = TryRelativeToRoot(sourcePath, sourceDirectory);
        const fs::path outputPath = relativePath.has_value()
            ? (outputDirectory / relativePath->generic_string()).replace_extension(relativePath->extension().string() + ".ri_asset.json")
            : (outputDirectory / (sourcePath.filename().string() + ".ri_asset.json"));
        EnsureParentDirectoryExists(outputPath);

        ri::content::AssetDocument document = BuildStandardAssetDocument(sourcePath, workspace);
        if (relativePath.has_value()) {
            document.id = SanitizeAssetId(relativePath->generic_string());
            if (!document.references.empty()) {
                document.references.front().id = document.id + "_source";
            }

            const std::string extension = ToLowerAscii(sourcePath.extension().string());
            if (IsForeignScriptExtension(extension)) {
                fs::path scriptOutputPath = outputDirectory.parent_path() / "scripts" / relativePath->generic_string();
                scriptOutputPath.replace_extension(".riscript");
                EnsureParentDirectoryExists(scriptOutputPath);
                const std::string sourceRelative = relativePath->generic_string();
                if (!json_scan::WriteTextFile(
                        scriptOutputPath,
                        ReconstructRiscriptFromForeignScript(sourcePath, document.id, sourceRelative))) {
                    throw std::runtime_error("Failed to write reconstructed RawIron script: " + scriptOutputPath.string());
                }
                document.references.push_back(ri::content::AssetReference{
                    .kind = "reconstructed-script",
                    .id = document.id + "_riscript",
                    .path = TryRelativeToRoot(scriptOutputPath, outputDirectory.parent_path())
                                .value_or(scriptOutputPath)
                                .generic_string(),
                });
            }
        }
        if (ri::content::SaveAssetDocument(outputPath, document)) {
            ++convertedCount;
        }
    }
    return convertedCount;
}

fs::path ResolvePackageManifestPath(const fs::path& packagePath) {
    if (fs::is_directory(packagePath)) {
        return packagePath / "package.ri_package.json";
    }
    return packagePath;
}

bool IsRipakArchivePath(const fs::path& path) {
    const std::string extension = ToLowerAscii(path.extension().string());
    return extension == ".ripak" || extension == ".zip";
}

std::string QuotePowerShellLiteral(const fs::path& path) {
    std::string text = path.string();
    std::string quoted = "'";
    for (const char ch : text) {
        if (ch == '\'') {
            quoted += "''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

void RunPowerShellArchiveCommand(const std::string& script, const std::string& action) {
    const std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"" + script + "\"";
    const int result = std::system(command.c_str());
    if (result != 0) {
        throw std::runtime_error("PowerShell archive " + action + " failed.");
    }
}

fs::path UniquePackageTempDirectory(const std::string_view prefix, const fs::path& packagePath) {
    const auto tick = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return fs::temp_directory_path() / "RawIronRipak" /
        (std::string(prefix) + "_" + SanitizeAssetId(packagePath.stem().string()) + "_" + std::to_string(tick));
}

void WriteRipakArchiveFromDirectory(const fs::path& packageDirectory, const fs::path& archivePath) {
    const fs::path absolutePackageDirectory = fs::weakly_canonical(packageDirectory);
    const fs::path absoluteArchivePath = fs::absolute(archivePath).lexically_normal();
    fs::create_directories(absoluteArchivePath.parent_path());
    const fs::path zipScratch = absoluteArchivePath.parent_path() / (absoluteArchivePath.stem().string() + ".zip");

    const std::string script =
        "if (Test-Path -LiteralPath " + QuotePowerShellLiteral(zipScratch) + ") { Remove-Item -LiteralPath "
        + QuotePowerShellLiteral(zipScratch) + " -Force }; "
        + "if (Test-Path -LiteralPath " + QuotePowerShellLiteral(absoluteArchivePath) + ") { Remove-Item -LiteralPath "
        + QuotePowerShellLiteral(absoluteArchivePath) + " -Force }; "
        + "Get-ChildItem -LiteralPath " + QuotePowerShellLiteral(absolutePackageDirectory)
        + " | Compress-Archive -DestinationPath " + QuotePowerShellLiteral(zipScratch) + " -Force; "
        + "Move-Item -LiteralPath " + QuotePowerShellLiteral(zipScratch)
        + " -Destination " + QuotePowerShellLiteral(absoluteArchivePath) + " -Force";
    RunPowerShellArchiveCommand(script, "create");
}

void StandardizeSingleAsset(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto sourceArg = commandLine.GetValue("--asset-standardize");
    if (!sourceArg.has_value() || sourceArg->empty()) {
        throw std::runtime_error("Missing --asset-standardize <source-path>.");
    }
    const fs::path sourcePath = fs::weakly_canonical(fs::path(*sourceArg));
    if (!fs::exists(sourcePath)) {
        throw std::runtime_error("Asset source does not exist: " + sourcePath.string());
    }

    fs::path outputPath = DefaultStandardizedOutputPath(workspace, sourcePath);
    if (const auto outputArg = commandLine.GetValue("--output"); outputArg.has_value() && !outputArg->empty()) {
        outputPath = fs::path(*outputArg);
    }
    EnsureParentDirectoryExists(outputPath);

    const ri::content::AssetDocument document = BuildStandardAssetDocument(sourcePath, workspace);
    if (!ri::content::SaveAssetDocument(outputPath, document)) {
        throw std::runtime_error("Failed to write standardized asset document: " + outputPath.string());
    }
    ri::core::LogInfo("Standardized asset:");
    ri::core::LogInfo("  Source: " + sourcePath.string());
    ri::core::LogInfo("  Type: " + document.type);
    ri::core::LogInfo("  Output: " + outputPath.string());
}

void StandardizeAssetDirectory(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto directoryArg = commandLine.GetValue("--asset-standardize-dir");
    if (!directoryArg.has_value() || directoryArg->empty()) {
        throw std::runtime_error("Missing --asset-standardize-dir <directory-path>.");
    }
    const fs::path sourceDirectory = fs::weakly_canonical(fs::path(*directoryArg));
    if (!fs::is_directory(sourceDirectory)) {
        throw std::runtime_error("Not a directory: " + sourceDirectory.string());
    }

    fs::path outputDirectory = workspace.assetsCooked / "Standardized";
    if (const auto outputArg = commandLine.GetValue("--output-dir"); outputArg.has_value() && !outputArg->empty()) {
        outputDirectory = fs::path(*outputArg);
    }
    const int convertedCount = StandardizeAssetDirectoryToOutput(workspace, sourceDirectory, outputDirectory);

    ri::core::LogInfo("Standardized asset batch complete.");
    ri::core::LogInfo("  Source directory: " + sourceDirectory.string());
    ri::core::LogInfo("  Output directory: " + outputDirectory.string());
    ri::core::LogInfo("  Converted: " + std::to_string(convertedCount));
}

void BuildAssetPackage(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto directoryArg = commandLine.GetValue("--asset-package-build");
    if (!directoryArg.has_value() || directoryArg->empty()) {
        throw std::runtime_error("Missing --asset-package-build <source-dir>.");
    }
    const fs::path sourceDirectory = fs::weakly_canonical(fs::path(*directoryArg));
    if (!fs::is_directory(sourceDirectory)) {
        throw std::runtime_error("Not a directory: " + sourceDirectory.string());
    }

    const std::string packageId = SanitizeAssetId(sourceDirectory.filename().string());
    fs::path archivePath = workspace.assetsCooked / "Packages" / (packageId + ".ripak");
    fs::path packageDirectory = workspace.assetsCooked / "Packages" / packageId;
    if (const auto outputArg = commandLine.GetValue("--output-dir"); outputArg.has_value() && !outputArg->empty()) {
        const fs::path outputPath = fs::path(*outputArg);
        if (IsRipakArchivePath(outputPath)) {
            archivePath = outputPath;
            packageDirectory = UniquePackageTempDirectory("build", outputPath);
        } else {
            packageDirectory = outputPath;
            archivePath = packageDirectory;
            archivePath += ".ripak";
        }
    }
    const fs::path assetDirectory = packageDirectory / "assets";
    const int convertedCount = StandardizeAssetDirectoryToOutput(workspace, sourceDirectory, assetDirectory);

    const std::optional<fs::path> relativeSource = TryRelativeToRoot(sourceDirectory, workspace.root);
    ri::content::AssetPackageManifest manifest = ri::content::BuildAssetPackageManifest(
        packageDirectory,
        packageId,
        sourceDirectory.filename().string(),
        relativeSource.has_value() ? relativeSource->generic_string() : sourceDirectory.generic_string(),
        CurrentUtcTimestamp());

    fs::path manifestPath = packageDirectory / "package.ri_package.json";
    if (const auto packageArg = commandLine.GetValue("--package"); packageArg.has_value() && !packageArg->empty()) {
        const fs::path packagePath = fs::path(*packageArg);
        if (IsRipakArchivePath(packagePath)) {
            archivePath = packagePath;
        } else {
            manifestPath = packagePath;
        }
    }
    EnsureParentDirectoryExists(manifestPath);
    if (!ri::content::SaveAssetPackageManifest(manifestPath, manifest)) {
        throw std::runtime_error("Failed to write RawIron package manifest: " + manifestPath.string());
    }

    const fs::path validationRoot = manifestPath.parent_path();
    const ri::content::AssetPackageValidationReport report =
        ri::content::ValidateAssetPackageManifest(manifest, validationRoot);
    if (!report.valid) {
        for (const std::string& issue : report.issues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error("RawIron package validation failed after build.");
    }

    ri::core::LogInfo("RawIron asset package built.");
    ri::core::LogInfo("  Source directory: " + sourceDirectory.string());
    ri::core::LogInfo("  Package directory: " + packageDirectory.string());
    ri::core::LogInfo("  Manifest: " + manifestPath.string());
    ri::core::LogInfo("  Converted: " + std::to_string(convertedCount));
    ri::core::LogInfo("  Packaged assets: " + std::to_string(manifest.assets.size()));

    WriteRipakArchiveFromDirectory(packageDirectory, archivePath);
    ri::core::LogInfo("  Archive: " + archivePath.string());
}

void ValidateAssetPackage(const ri::core::CommandLine& commandLine) {
    const auto packageArg = commandLine.GetValue("--asset-package-validate");
    if (!packageArg.has_value() || packageArg->empty()) {
        throw std::runtime_error("Missing --asset-package-validate <package-dir-or-manifest>.");
    }
    std::optional<ri::tooling::SecureRipakExtraction> extraction;
    fs::path packageRoot = fs::path(*packageArg);
    if (IsRipakArchivePath(packageRoot)) {
        extraction.emplace(ri::tooling::SecureRipakExtraction::Extract(packageRoot));
        packageRoot = extraction->Root();
    }
    const fs::path manifestPath = ResolvePackageManifestPath(packageRoot);
    const std::optional<ri::content::AssetPackageManifest> manifest =
        ri::content::LoadAssetPackageManifest(manifestPath);
    if (!manifest.has_value()) {
        throw std::runtime_error("Failed to load RawIron package manifest: " + manifestPath.string());
    }

    const ri::content::AssetPackageValidationReport report =
        ri::content::ValidateAssetPackageManifest(*manifest, manifestPath.parent_path());
    if (!report.valid) {
        ri::core::LogInfo("RawIron asset package validation failed.");
        for (const std::string& issue : report.issues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error("RawIron asset package validation failed.");
    }

    ri::core::LogInfo("RawIron asset package validated.");
    ri::core::LogInfo("  Manifest: " + manifestPath.string());
    ri::core::LogInfo("  Package: " + manifest->packageId);
    ri::core::LogInfo("  Kind: " + manifest->packageKind);
    ri::core::LogInfo("  Version: " + manifest->packageVersion);
    ri::core::LogInfo("  Engine API: " + manifest->engineApiRequirement);
    ri::core::LogInfo("  Runtime: " + manifest->runtime.executionMode);
    ri::core::LogInfo("  Assets: " + std::to_string(manifest->assets.size()));
}

void PrintRigToolchainReport() {
    ri::core::LogInfo("RawIron modeling and rigging toolchain:");
    ri::core::LogInfo("  Authoring source: .ri_rig.json portable skeleton definitions.");
    ri::core::LogInfo("  Baseline: --rig-create-humanoid generates root-motion + 21-bone humanoid convention.");
    ri::core::LogInfo("  Validation: hierarchy cycles, duplicate names, rest-transform validity, and humanoid coverage.");
    ri::core::LogInfo("  Mesh inputs: OBJ, glTF/GLB, and FBX import through RawIron.SceneUtilities.");
    ri::core::LogInfo("  Animation: imported transform clips plus stable humanoid bone-name canonicalization.");
    ri::core::LogInfo("  Forge clay/motion: .ri_sculpt.json + .ri_anim.json via --sculpt-* / --anim-* / --forge-*.");
    ri::core::LogInfo("  Editor workflow: save under Assets/Source/{rigs,sculpts,anims}, then inspect/package with project assets.");
}

void CreateHumanoidRig(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto idArg = commandLine.GetValue("--rig-create-humanoid");
    if (!idArg.has_value() || idArg->empty()) {
        throw std::runtime_error("Missing --rig-create-humanoid <rig-id>.");
    }

    const std::string rigId = SanitizeAssetId(*idArg);
    fs::path output = workspace.assetsSource / "rigs" / (rigId + ".ri_rig.json");
    if (const auto outputArg = commandLine.GetValue("--output"); outputArg.has_value() && !outputArg->empty()) {
        output = fs::path(*outputArg);
    }
    std::error_code existsError{};
    if (fs::exists(output, existsError) && !commandLine.HasFlag("--overwrite")) {
        throw std::runtime_error("Rig output already exists (use --overwrite to replace it): " + output.string());
    }

    const std::string displayName = commandLine.GetValue("--name").value_or(*idArg);
    const ri::scene::RigDefinition rig = ri::scene::CreateHumanoidRigDefinition(rigId, displayName);
    const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(rig);
    if (!report.valid) {
        throw std::runtime_error("Internal humanoid rig template failed validation.");
    }
    if (!ri::scene::SaveRigDefinition(output, rig)) {
        throw std::runtime_error("Failed to write rig: " + output.string());
    }

    ri::core::LogInfo("Created humanoid rig:");
    ri::core::LogInfo("  Id: " + rig.id);
    ri::core::LogInfo("  Name: " + rig.displayName);
    ri::core::LogInfo("  Output: " + output.string());
    ri::core::LogInfo("  Bones: " + std::to_string(rig.bones.size()));
    ri::core::LogInfo("  Humanoid convention: " + std::to_string(report.humanoidMatchedBoneCount) + "/" +
                      std::to_string(report.humanoidRequiredBoneCount));
}

fs::path ResolveAuthoringPath(const WorkspaceLayout& workspace, const fs::path& path);

void ValidateRig(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto pathArg = commandLine.GetValue("--rig-validate");
    if (!pathArg.has_value() || pathArg->empty()) {
        throw std::runtime_error("Missing --rig-validate <file.ri_rig.json>.");
    }
    const fs::path path = ResolveAuthoringPath(workspace, fs::path(*pathArg));
    const std::optional<ri::scene::RigDefinition> rig = ri::scene::LoadRigDefinition(path);
    if (!rig.has_value()) {
        throw std::runtime_error("Could not parse RawIron rig: " + path.string());
    }
    const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(*rig);
    ri::core::LogInfo("RawIron rig validation:");
    ri::core::LogInfo("  Input: " + path.string());
    ri::core::LogInfo("  Id: " + rig->id);
    ri::core::LogInfo("  Bones: " + std::to_string(rig->bones.size()));
    ri::core::LogInfo("  Roots: " + std::to_string(report.rootBoneCount));
    if (rig->profile == ri::scene::RigProfile::Humanoid) {
        ri::core::LogInfo("  Humanoid convention: " + std::to_string(report.humanoidMatchedBoneCount) + "/" +
                          std::to_string(report.humanoidRequiredBoneCount));
    }
    for (const std::string& warning : report.warnings) {
        ri::core::LogInfo("  Warning: " + warning);
    }
    if (!report.valid) {
        for (const std::string& error : report.errors) {
            ri::core::LogInfo("  Error: " + error);
        }
        throw std::runtime_error("RawIron rig validation failed.");
    }
    ri::core::LogInfo("  Result: valid");
}

fs::path ResolveAuthoringPath(const WorkspaceLayout& workspace, const fs::path& path) {
    if (path.empty()) {
        return {};
    }
    if (path.is_absolute()) {
        return path;
    }
    const fs::path fromRoot = workspace.root / path;
    std::error_code existsError{};
    if (fs::exists(fromRoot, existsError)) {
        return fromRoot;
    }
    const fs::path fromSource = workspace.assetsSource / path;
    if (fs::exists(fromSource, existsError)) {
        return fromSource;
    }
    return fromRoot;
}

std::string RelativeAuthoringSourcePath(const WorkspaceLayout& workspace, const fs::path& absolutePath) {
    std::error_code error{};
    const fs::path relative = fs::relative(absolutePath, workspace.assetsSource, error);
    if (!error) {
        const std::string relativeText = relative.generic_string();
        // Animation docs reject directory traversal in stored relative paths.
        if (relativeText.find("..") == std::string::npos) {
            return relativeText;
        }
    }
    return fs::weakly_canonical(absolutePath, error).generic_string();
}

fs::path ResolveClipRigPath(
    const WorkspaceLayout& workspace,
    const std::string& rigPathText,
    const fs::path& animPath) {
    if (rigPathText.empty()) {
        return {};
    }
    const fs::path rigPath(rigPathText);
    std::error_code existsError{};
    if (rigPath.is_absolute() && fs::exists(rigPath, existsError)) {
        return rigPath;
    }
    const fs::path fromSource = workspace.assetsSource / rigPath;
    if (fs::exists(fromSource, existsError)) {
        return fromSource;
    }
    const fs::path fromRoot = workspace.root / rigPath;
    if (fs::exists(fromRoot, existsError)) {
        return fromRoot;
    }
    const fs::path besideClip = animPath.parent_path() / rigPath;
    if (fs::exists(besideClip, existsError)) {
        return besideClip;
    }
    return {};
}

void PrintForgeToolchainReport(const WorkspaceLayout& workspace) {
    auto countSuffix = [](const fs::path& folder, const std::string_view suffix) {
        std::size_t count = 0U;
        std::error_code error{};
        if (!fs::exists(folder, error) || !fs::is_directory(folder, error)) {
            return count;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(folder, error)) {
            if (!entry.is_regular_file(error)) {
                continue;
            }
            const std::string name = entry.path().filename().string();
            if (name.size() >= suffix.size()
                && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
                ++count;
            }
        }
        return count;
    };

    ri::core::LogInfo("RawIron Forge authoring (CLI — no UI required):");
    ri::core::LogInfo("  Clay: --sculpt-create / --sculpt-bind / --sculpt-fit-rig / --sculpt-flood-weights / --sculpt-flood-free");
    ri::core::LogInfo("  Clay: --sculpt-soft / --sculpt-soft-bone / --sculpt-cap4 / --sculpt-normalize / --sculpt-prune");
    ri::core::LogInfo("  Clay: --sculpt-mirror / --sculpt-seal / --sculpt-invert / --sculpt-ceil / --sculpt-harden");
    ri::core::LogInfo("  Clay: --sculpt-transfer / --sculpt-swap / --sculpt-grow / --sculpt-shrink / --sculpt-paint");
    ri::core::LogInfo("  Clay: --sculpt-bounds / --sculpt-bone-weights / --sculpt-audit-weights / --sculpt-validate");
    ri::core::LogInfo("  Clay: --sculpt-unbind / --sculpt-clear-weights");
    ri::core::LogInfo("  Motion: --anim-create / --anim-key-rest / --anim-key / --anim-hold / --anim-breakdown");
    ri::core::LogInfo("  Motion: --anim-event / --anim-trim / --anim-fit-duration / --anim-align-start");
    ri::core::LogInfo("  Motion: --anim-validate / --anim-report");
    ri::core::LogInfo("  Scaffold: --forge-character-create <id> [--cage sphere|cube] (clay only)");
    ri::core::LogInfo("  Block model: --blockchar-create / --blockchar-add-part / --blockchar-set-part / --blockchar-nudge");
    ri::core::LogInfo("  Block sync: --blockchar-sync-sculpt | recipes: --function <name> (Tools/ri_tool/functions)");
    ri::core::LogInfo("  Inspect: --forge-assets-list | --forge-report | --forge-handoff-probe <asset>");
    ri::core::LogInfo("  Rigs: --rig-create-humanoid / --rig-validate (shared with Forge UI)");
    ri::core::LogInfo("  Source roots:");
    ri::core::LogInfo("    sculpts: " + (workspace.assetsSource / "sculpts").string());
    ri::core::LogInfo("    rigs: " + (workspace.assetsSource / "rigs").string());
    ri::core::LogInfo("    anims: " + (workspace.assetsSource / "anims").string());
    ri::core::LogInfo(
        "  Counts: sculpt=" + std::to_string(countSuffix(workspace.assetsSource / "sculpts", ".ri_sculpt.json"))
        + " rig=" + std::to_string(countSuffix(workspace.assetsSource / "rigs", ".ri_rig.json"))
        + " anim=" + std::to_string(countSuffix(workspace.assetsSource / "anims", ".ri_anim.json")));
}

void ListForgeAssets(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const std::string kind = commandLine.GetValue("--kind").value_or("all");
    auto listFolder = [&](const std::string_view label, const fs::path& folder, const std::string_view suffix) {
        if (kind != "all" && kind != label) {
            return;
        }
        std::error_code error{};
        if (!fs::exists(folder, error) || !fs::is_directory(folder, error)) {
            return;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(folder, error)) {
            if (!entry.is_regular_file(error)) {
                continue;
            }
            const std::string name = entry.path().filename().string();
            if (name.size() < suffix.size()
                || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
                continue;
            }
            const std::string relative = RelativeAuthoringSourcePath(workspace, entry.path());
            bool valid = false;
            if (label == "sculpt") {
                valid = ri::content::LoadNativeSculptDocument(entry.path()).has_value();
            } else if (label == "rig") {
                const auto rig = ri::scene::LoadRigDefinition(entry.path());
                valid = rig.has_value() && ri::scene::ValidateRigDefinition(*rig).valid;
            } else if (label == "anim") {
                const auto clip = ri::content::LoadNativeAnimationDocument(entry.path());
                valid = clip.has_value() && ri::content::ValidateNativeAnimationDocument(*clip).valid;
            }
            ri::core::LogInfo(
                std::string(label) + "\t" + (valid ? "ok" : "bad") + "\t" + relative);
        }
    };
    listFolder("sculpt", workspace.assetsSource / "sculpts", ".ri_sculpt.json");
    listFolder("rig", workspace.assetsSource / "rigs", ".ri_rig.json");
    listFolder("anim", workspace.assetsSource / "anims", ".ri_anim.json");
}

[[nodiscard]] ri::math::Vec3 ParseAtVec3(const std::string_view raw) {
    std::string text(raw);
    for (char& ch : text) {
        if (ch == ',' || ch == ';') {
            ch = ' ';
        }
    }
    std::istringstream stream(text);
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (!(stream >> x >> y >> z)) {
        throw std::runtime_error("--at must be three numbers like 0,1.2,0 or \"0 1.2 0\".");
    }
    return ri::math::Vec3{x, y, z};
}

[[nodiscard]] ri::scene::NativeSculptWeightPaint ParseWeightPaintMode(const std::string_view raw) {
    if (raw == "assign" || raw == "set") {
        return ri::scene::NativeSculptWeightPaint::Assign;
    }
    if (raw == "add" || raw == "blend") {
        return ri::scene::NativeSculptWeightPaint::Add;
    }
    if (raw == "smooth" || raw == "smth") {
        return ri::scene::NativeSculptWeightPaint::Smooth;
    }
    if (raw == "clear" || raw == "erase") {
        return ri::scene::NativeSculptWeightPaint::Clear;
    }
    throw std::runtime_error("--mode must be assign|add|smooth|clear.");
}

struct SculptBounds {
    ri::math::Vec3 min{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 max{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 center{0.0f, 0.0f, 0.0f};
    ri::math::Vec3 extent{0.0f, 0.0f, 0.0f};
};

[[nodiscard]] SculptBounds ComputeSculptBounds(const ri::content::NativeSculptDocument& document) {
    SculptBounds bounds{};
    if (document.mesh.positions.empty()) {
        return bounds;
    }
    bounds.min = document.mesh.positions.front();
    bounds.max = document.mesh.positions.front();
    for (const ri::math::Vec3& position : document.mesh.positions) {
        bounds.min.x = std::min(bounds.min.x, position.x);
        bounds.min.y = std::min(bounds.min.y, position.y);
        bounds.min.z = std::min(bounds.min.z, position.z);
        bounds.max.x = std::max(bounds.max.x, position.x);
        bounds.max.y = std::max(bounds.max.y, position.y);
        bounds.max.z = std::max(bounds.max.z, position.z);
    }
    bounds.center = (bounds.min + bounds.max) * 0.5f;
    bounds.extent = bounds.max - bounds.min;
    return bounds;
}

/// Uniformly scales/translates clay so its AABB covers deform-bone rest positions (with padding).
[[nodiscard]] bool FitSculptMeshToRig(
    ri::content::NativeSculptDocument& document,
    const ri::scene::RigDefinition& rig,
    const float padding = 1.15f) {
    if (document.mesh.positions.empty()) {
        return false;
    }
    const auto restWorld = ri::scene::RestBoneWorldMatrices(rig);
    bool any = false;
    ri::math::Vec3 boneMin{};
    ri::math::Vec3 boneMax{};
    for (const ri::scene::RigBone& bone : rig.bones) {
        if (!bone.deform || bone.name.empty()) {
            continue;
        }
        const auto found = restWorld.find(bone.name);
        if (found == restWorld.end()) {
            continue;
        }
        const ri::math::Vec3 position = ri::math::ExtractTranslation(found->second);
        if (!any) {
            boneMin = position;
            boneMax = position;
            any = true;
        } else {
            boneMin.x = std::min(boneMin.x, position.x);
            boneMin.y = std::min(boneMin.y, position.y);
            boneMin.z = std::min(boneMin.z, position.z);
            boneMax.x = std::max(boneMax.x, position.x);
            boneMax.y = std::max(boneMax.y, position.y);
            boneMax.z = std::max(boneMax.z, position.z);
        }
    }
    if (!any) {
        return false;
    }
    const ri::math::Vec3 boneCenter = (boneMin + boneMax) * 0.5f;
    ri::math::Vec3 boneExtent = boneMax - boneMin;
    // Keep a usable volume even for skinny skeletons.
    boneExtent.x = std::max(boneExtent.x, 0.35f);
    boneExtent.y = std::max(boneExtent.y, 0.35f);
    boneExtent.z = std::max(boneExtent.z, 0.35f);
    boneExtent = boneExtent * std::max(padding, 1.0f);

    const SculptBounds meshBounds = ComputeSculptBounds(document);
    const float meshSpan = std::max({meshBounds.extent.x, meshBounds.extent.y, meshBounds.extent.z, 0.0001f});
    const float boneSpan = std::max({boneExtent.x, boneExtent.y, boneExtent.z});
    const float scale = boneSpan / meshSpan;
    for (ri::math::Vec3& position : document.mesh.positions) {
        position = boneCenter + (position - meshBounds.center) * scale;
    }
    if (!document.mesh.normals.empty()) {
        ri::scene::RecalculateSculptNormals(document.mesh);
    }
    return true;
}

void WriteSolidColorTga(
    const fs::path& path,
    const float r,
    const float g,
    const float b,
    const int size = 32) {
    EnsureParentDirectoryExists(path);
    std::vector<std::uint8_t> bytes{};
    bytes.reserve(18U + static_cast<std::size_t>(size * size * 3));
    bytes.insert(bytes.end(), {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    bytes.push_back(static_cast<std::uint8_t>(size & 0xff));
    bytes.push_back(static_cast<std::uint8_t>((size >> 8) & 0xff));
    bytes.push_back(static_cast<std::uint8_t>(size & 0xff));
    bytes.push_back(static_cast<std::uint8_t>((size >> 8) & 0xff));
    bytes.push_back(24);
    bytes.push_back(0x20); // top-left origin
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            // Cheap facet noise + edge darken so solid maps read as PS1 tiles, not flat wash.
            const float nx = static_cast<float>(x) / static_cast<float>(std::max(1, size - 1));
            const float ny = static_cast<float>(y) / static_cast<float>(std::max(1, size - 1));
            const float edge = std::min({nx, ny, 1.0f - nx, 1.0f - ny});
            const float dither =
                0.06f * (((x * 17 + y * 31) & 7) / 7.0f - 0.5f) + (edge < 0.12f ? -0.08f : 0.0f);
            const auto channel = [dither](const float c) {
                return static_cast<std::uint8_t>(
                    std::clamp(c + dither, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            bytes.push_back(channel(b));
            bytes.push_back(channel(g));
            bytes.push_back(channel(r));
        }
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to write texture: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

[[nodiscard]] fs::path RiToolPackageRoot(const WorkspaceLayout& workspace) {
#if defined(_WIN32)
    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) != 0U) {
        const fs::path exeDir = fs::path(modulePath).parent_path();
        const std::vector<fs::path> climb = {
            exeDir / ".." / ".." / ".." / ".." / ".." / ".." / "Tools" / "ri_tool",
            exeDir / ".." / ".." / ".." / ".." / "Tools" / "ri_tool",
        };
        std::error_code ec{};
        for (const fs::path& candidate : climb) {
            if (fs::is_directory(candidate, ec)) {
                return fs::weakly_canonical(candidate, ec);
            }
        }
    }
#endif
    if (fs::is_directory(workspace.tools / "ri_tool")) {
        return workspace.tools / "ri_tool";
    }
    return workspace.root / "Tools" / "ri_tool";
}

[[nodiscard]] float ParseFloatOr(const std::optional<std::string>& raw, const float fallback) {
    if (!raw.has_value() || raw->empty()) {
        return fallback;
    }
    return std::stof(*raw);
}

[[nodiscard]] ri::content::DeclarativeVec3 ParseRgbOr(
    const std::optional<std::string>& raw,
    const ri::content::DeclarativeVec3 fallback) {
    if (!raw.has_value() || raw->empty()) {
        return fallback;
    }
    float r = fallback.x;
    float g = fallback.y;
    float b = fallback.z;
    char sep = ',';
    std::istringstream stream(*raw);
    if (!(stream >> r >> sep >> g >> sep >> b)) {
        throw std::runtime_error("Expected --color r,g,b (0-1 floats).");
    }
    return {.x = r, .y = g, .z = b};
}

[[nodiscard]] fs::path ResolveBlockCharPath(const WorkspaceLayout& workspace, const std::string& raw) {
    const fs::path path(raw);
    if (path.is_absolute()) {
        return path;
    }
    std::error_code ec{};
    const fs::path fromSource = workspace.assetsSource / path;
    if (fs::exists(fromSource, ec) || path.extension().string().find("blockchar") != std::string::npos
        || path.generic_string().find("blockchar") != std::string::npos) {
        return fromSource;
    }
    return ResolveAuthoringPath(workspace, path);
}

void LaunchForgePreview(const WorkspaceLayout& workspace, const fs::path& assetPath) {
#if defined(_WIN32)
    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0U) {
        throw std::runtime_error("Could not resolve ri_tool path to locate RawIron.Forge.");
    }
    const fs::path toolExe = fs::path(modulePath);
    const fs::path configName = toolExe.parent_path().filename();
    // .../build/<cfg>/Tools/ri_tool/<Config>/ri_tool.exe → Apps/RawIron.Forge/<Config>/
    const fs::path buildRoot =
        toolExe.parent_path().parent_path().parent_path().parent_path();
    const fs::path forgeExe =
        buildRoot / "Apps" / "RawIron.Forge" / configName / "RawIron.Forge.exe";
    std::error_code existsError{};
    if (!fs::exists(forgeExe, existsError)) {
        throw std::runtime_error("RawIron.Forge.exe not found next to this build: " + forgeExe.string());
    }
    const std::wstring command = L"\"" + forgeExe.wstring() + L"\" --open-asset \""
        + assetPath.wstring() + L"\" --auto-play --root \"" + workspace.root.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = command;
    if (!CreateProcessW(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            workspace.root.wstring().c_str(),
            &startup,
            &process)) {
        throw std::runtime_error("Failed to launch RawIron.Forge (CreateProcess).");
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    ri::core::LogInfo("Opened Forge preview: " + assetPath.string());
#else
    (void)workspace;
    (void)assetPath;
    throw std::runtime_error("--open is only supported on Windows builds of ri_tool.");
#endif
}

[[nodiscard]] ri::content::BlockCharacterDocument LoadRequiredBlockChar(
    const WorkspaceLayout& workspace,
    const ri::core::CommandLine& commandLine) {
    const auto pathArg = commandLine.GetValue("--blockchar");
    if (!pathArg.has_value() || pathArg->empty()) {
        throw std::runtime_error("Requires --blockchar <path>.");
    }
    const fs::path path = ResolveBlockCharPath(workspace, *pathArg);
    auto document = ri::content::LoadBlockCharacterDocument(path);
    if (!document.has_value()) {
        throw std::runtime_error("Could not load block character: " + path.string());
    }
    return *document;
}

void SaveRequiredBlockChar(
    const WorkspaceLayout& workspace,
    const ri::core::CommandLine& commandLine,
    const ri::content::BlockCharacterDocument& document) {
    const auto pathArg = commandLine.GetValue("--blockchar");
    const fs::path path = ResolveBlockCharPath(workspace, *pathArg);
    const auto report = ri::content::ValidateBlockCharacterDocument(document);
    if (!report.valid) {
        std::string detail = "Block character failed validation.";
        for (const std::string& error : report.errors) {
            detail += " ";
            detail += error;
        }
        throw std::runtime_error(detail);
    }
    EnsureParentDirectoryExists(path);
    if (!ri::content::SaveBlockCharacterDocument(path, document)) {
        throw std::runtime_error("Failed to write block character: " + path.string());
    }
}

[[nodiscard]] ri::content::BlockCharacterPart* FindPartByIdOrName(
    ri::content::BlockCharacterDocument& document,
    const std::string& key) {
    for (ri::content::BlockCharacterPart& part : document.parts) {
        if (part.id == key || part.name == key) {
            return &part;
        }
    }
    return nullptr;
}

[[nodiscard]] std::unordered_map<std::string, ri::math::Vec3> ComputeRigBoneWorldPositions(
    const ri::scene::RigDefinition& rig) {
    std::unordered_map<std::string, ri::math::Vec3> worlds{};
    std::vector<ri::math::Mat4> worldMats(rig.bones.size(), ri::math::Mat4{});
    for (std::size_t i = 0; i < rig.bones.size(); ++i) {
        const ri::scene::RigBone& bone = rig.bones[i];
        const ri::math::Mat4 local = bone.restLocal.LocalMatrix();
        if (bone.parentIndex < 0 || bone.parentIndex >= static_cast<int>(rig.bones.size())) {
            worldMats[i] = local;
        } else {
            worldMats[i] = ri::math::Multiply(worldMats[static_cast<std::size_t>(bone.parentIndex)], local);
        }
        worlds[bone.name] = ri::math::ExtractTranslation(worldMats[i]);
    }
    return worlds;
}

[[nodiscard]] ri::content::BlockCharacterPart BuildPartFromCommandLine(
    const ri::core::CommandLine& commandLine, const std::string& fallbackId) {
    const auto boneArg = commandLine.GetValue("--bone");
    if (!boneArg.has_value() || boneArg->empty()) {
        throw std::runtime_error("Part requires --bone <name>.");
    }
    ri::content::BlockCharacterPart part{};
    part.id = SanitizeAssetId(commandLine.GetValue("--part").value_or(fallbackId));
    part.name = commandLine.GetValue("--part-name").value_or(part.id);
    part.boneName = *boneArg;
    part.shape = commandLine.GetValue("--shape").value_or("box");
    part.center = {
        .x = ParseFloatOr(commandLine.GetValue("--x"), 0.0f),
        .y = ParseFloatOr(commandLine.GetValue("--y"), 0.0f),
        .z = ParseFloatOr(commandLine.GetValue("--z"), 0.0f),
    };
    part.halfExtent = {
        .x = ParseFloatOr(commandLine.GetValue("--sx"), 0.1f),
        .y = ParseFloatOr(commandLine.GetValue("--sy"), 0.1f),
        .z = ParseFloatOr(commandLine.GetValue("--sz"), 0.1f),
    };
    part.halfExtentTop = {
        .x = ParseFloatOr(commandLine.GetValue("--tsx"), 0.0f),
        .y = ParseFloatOr(commandLine.GetValue("--tsy"), 0.0f),
        .z = ParseFloatOr(commandLine.GetValue("--tsz"), 0.0f),
    };
    part.rotationDegrees = {
        .x = ParseFloatOr(commandLine.GetValue("--rx"), 0.0f),
        .y = ParseFloatOr(commandLine.GetValue("--ry"), 0.0f),
        .z = ParseFloatOr(commandLine.GetValue("--rz"), 0.0f),
    };
    part.albedoColor = ParseRgbOr(commandLine.GetValue("--color"), {.x = 0.62f, .y = 0.55f, .z = 0.46f});
    part.roughness = ParseFloatOr(commandLine.GetValue("--roughness"), 0.88f);
    part.metallic = ParseFloatOr(commandLine.GetValue("--metallic"), 0.0f);
    part.sides = static_cast<int>(ParseFloatOr(commandLine.GetValue("--sides"), 0.0f));
    part.bevel = std::clamp(ParseFloatOr(commandLine.GetValue("--bevel"), 0.0f), 0.0f, 0.45f);
    part.albedoTexture = commandLine.GetValue("--texture").value_or("");
    return part;
}

void CreateBlockCharacter(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto idArg = commandLine.GetValue("--blockchar-create");
    if (!idArg.has_value() || idArg->empty()) {
        throw std::runtime_error("Missing --blockchar-create <character-id>.");
    }
    const std::string characterId = SanitizeAssetId(*idArg);
    const std::string displayName = commandLine.GetValue("--name").value_or(*idArg);
    fs::path output = workspace.assetsSource / "blockchars" / (characterId + ".ri_blockchar.json");
    if (const auto outputArg = commandLine.GetValue("--output"); outputArg.has_value() && !outputArg->empty()) {
        output = ResolveBlockCharPath(workspace, *outputArg);
    }
    std::error_code existsError{};
    if (fs::exists(output, existsError) && !commandLine.HasFlag("--overwrite")) {
        throw std::runtime_error("Block character already exists (use --overwrite): " + output.string());
    }

    ri::content::BlockCharacterDocument document{};
    document.id = characterId;
    document.displayName = displayName;
    if (const auto rigArg = commandLine.GetValue("--rig"); rigArg.has_value() && !rigArg->empty()) {
        const fs::path rigPath = ResolveAuthoringPath(workspace, fs::path(*rigArg));
        document.rigPath = RelativeAuthoringSourcePath(workspace, rigPath);
    }
    // Optional clone of a USER file (not an engine recipe). Empty document is the default.
    if (const auto fromArg = commandLine.GetValue("--from"); fromArg.has_value() && !fromArg->empty()) {
        const fs::path fromPath = ResolveBlockCharPath(workspace, *fromArg);
        auto seeded = ri::content::LoadBlockCharacterDocument(fromPath);
        if (!seeded.has_value()) {
            throw std::runtime_error("Could not load --from block character: " + fromPath.string());
        }
        document.parts = std::move(seeded->parts);
        if (document.rigPath.empty()) {
            document.rigPath = seeded->rigPath;
        }
    }

    EnsureParentDirectoryExists(output);
    if (!ri::content::ValidateBlockCharacterDocument(document).valid
        || !ri::content::SaveBlockCharacterDocument(output, document)) {
        throw std::runtime_error("Failed to write block character: " + output.string());
    }
    ri::core::LogInfo("Created block character (empty model — add parts with --blockchar-add-part):");
    ri::core::LogInfo("  Id: " + characterId);
    ri::core::LogInfo("  Output: " + output.string());
    ri::core::LogInfo("  Parts: " + std::to_string(document.parts.size()));
}

void AddBlockCharacterPart(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-add-part requires --blockchar <path>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    const std::string fallbackId = "part_" + std::to_string(document.parts.size());
    ri::content::BlockCharacterPart part = BuildPartFromCommandLine(commandLine, fallbackId);
    if (FindPartByIdOrName(document, part.id) != nullptr) {
        throw std::runtime_error("Part id already exists: " + part.id + " (use --blockchar-set-part).");
    }
    document.parts.push_back(std::move(part));
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo(
        "Added part '" + document.parts.back().id + "' on bone '" + document.parts.back().boneName
        + "' (" + document.parts.back().shape + "). Parts now: "
        + std::to_string(document.parts.size()));
}

void SetBlockCharacterPart(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-set-part requires --blockchar <path>.");
    }
    const auto partArg = commandLine.GetValue("--part");
    if (!partArg.has_value() || partArg->empty()) {
        throw std::runtime_error("--blockchar-set-part requires --part <id>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    ri::content::BlockCharacterPart* part = FindPartByIdOrName(document, *partArg);
    if (part == nullptr) {
        throw std::runtime_error("Part not found: " + *partArg);
    }
    if (const auto bone = commandLine.GetValue("--bone"); bone.has_value() && !bone->empty()) {
        part->boneName = *bone;
    }
    if (const auto shape = commandLine.GetValue("--shape"); shape.has_value() && !shape->empty()) {
        part->shape = *shape;
    }
    if (commandLine.GetValue("--x").has_value()) {
        part->center.x = ParseFloatOr(commandLine.GetValue("--x"), part->center.x);
    }
    if (commandLine.GetValue("--y").has_value()) {
        part->center.y = ParseFloatOr(commandLine.GetValue("--y"), part->center.y);
    }
    if (commandLine.GetValue("--z").has_value()) {
        part->center.z = ParseFloatOr(commandLine.GetValue("--z"), part->center.z);
    }
    if (commandLine.GetValue("--sx").has_value()) {
        part->halfExtent.x = ParseFloatOr(commandLine.GetValue("--sx"), part->halfExtent.x);
    }
    if (commandLine.GetValue("--sy").has_value()) {
        part->halfExtent.y = ParseFloatOr(commandLine.GetValue("--sy"), part->halfExtent.y);
    }
    if (commandLine.GetValue("--sz").has_value()) {
        part->halfExtent.z = ParseFloatOr(commandLine.GetValue("--sz"), part->halfExtent.z);
    }
    if (commandLine.GetValue("--tsx").has_value()) {
        part->halfExtentTop.x = ParseFloatOr(commandLine.GetValue("--tsx"), part->halfExtentTop.x);
    }
    if (commandLine.GetValue("--tsy").has_value()) {
        part->halfExtentTop.y = ParseFloatOr(commandLine.GetValue("--tsy"), part->halfExtentTop.y);
    }
    if (commandLine.GetValue("--tsz").has_value()) {
        part->halfExtentTop.z = ParseFloatOr(commandLine.GetValue("--tsz"), part->halfExtentTop.z);
    }
    if (commandLine.GetValue("--rx").has_value()) {
        part->rotationDegrees.x = ParseFloatOr(commandLine.GetValue("--rx"), part->rotationDegrees.x);
    }
    if (commandLine.GetValue("--ry").has_value()) {
        part->rotationDegrees.y = ParseFloatOr(commandLine.GetValue("--ry"), part->rotationDegrees.y);
    }
    if (commandLine.GetValue("--rz").has_value()) {
        part->rotationDegrees.z = ParseFloatOr(commandLine.GetValue("--rz"), part->rotationDegrees.z);
    }
    if (commandLine.GetValue("--color").has_value()) {
        part->albedoColor = ParseRgbOr(commandLine.GetValue("--color"), part->albedoColor);
    }
    if (commandLine.GetValue("--roughness").has_value()) {
        part->roughness = ParseFloatOr(commandLine.GetValue("--roughness"), part->roughness);
    }
    if (commandLine.GetValue("--metallic").has_value()) {
        part->metallic = ParseFloatOr(commandLine.GetValue("--metallic"), part->metallic);
    }
    if (commandLine.GetValue("--texture").has_value()) {
        part->albedoTexture = commandLine.GetValue("--texture").value_or("");
    }
    if (commandLine.GetValue("--sides").has_value()) {
        part->sides = static_cast<int>(ParseFloatOr(commandLine.GetValue("--sides"), 0.0f));
    }
    if (commandLine.GetValue("--bevel").has_value()) {
        part->bevel = std::clamp(ParseFloatOr(commandLine.GetValue("--bevel"), part->bevel), 0.0f, 0.45f);
    }
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo("Updated part '" + part->id + "'.");
}

void NudgeBlockCharacterPart(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-nudge requires --blockchar <path>.");
    }
    const auto partArg = commandLine.GetValue("--part");
    if (!partArg.has_value() || partArg->empty()) {
        throw std::runtime_error("--blockchar-nudge requires --part <id>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    ri::content::BlockCharacterPart* part = FindPartByIdOrName(document, *partArg);
    if (part == nullptr) {
        throw std::runtime_error("Part not found: " + *partArg);
    }
    part->center.x += ParseFloatOr(commandLine.GetValue("--dx"), 0.0f);
    part->center.y += ParseFloatOr(commandLine.GetValue("--dy"), 0.0f);
    part->center.z += ParseFloatOr(commandLine.GetValue("--dz"), 0.0f);
    part->halfExtent.x = std::max(0.001f, part->halfExtent.x * ParseFloatOr(commandLine.GetValue("--scale"), 1.0f));
    part->halfExtent.y = std::max(0.001f, part->halfExtent.y * ParseFloatOr(commandLine.GetValue("--scale"), 1.0f));
    part->halfExtent.z = std::max(0.001f, part->halfExtent.z * ParseFloatOr(commandLine.GetValue("--scale"), 1.0f));
    if (commandLine.GetValue("--dsx").has_value()) {
        part->halfExtent.x = std::max(0.001f, part->halfExtent.x + ParseFloatOr(commandLine.GetValue("--dsx"), 0.0f));
    }
    if (commandLine.GetValue("--dsy").has_value()) {
        part->halfExtent.y = std::max(0.001f, part->halfExtent.y + ParseFloatOr(commandLine.GetValue("--dsy"), 0.0f));
    }
    if (commandLine.GetValue("--dsz").has_value()) {
        part->halfExtent.z = std::max(0.001f, part->halfExtent.z + ParseFloatOr(commandLine.GetValue("--dsz"), 0.0f));
    }
    part->rotationDegrees.x += ParseFloatOr(commandLine.GetValue("--drx"), 0.0f);
    part->rotationDegrees.y += ParseFloatOr(commandLine.GetValue("--dry"), 0.0f);
    part->rotationDegrees.z += ParseFloatOr(commandLine.GetValue("--drz"), 0.0f);
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo(
        "Nudged '" + part->id + "' to (" + std::to_string(part->center.x) + ", "
        + std::to_string(part->center.y) + ", " + std::to_string(part->center.z) + ")");
}

void RemoveBlockCharacterPart(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-remove-part requires --blockchar <path>.");
    }
    const auto partArg = commandLine.GetValue("--part");
    if (!partArg.has_value() || partArg->empty()) {
        throw std::runtime_error("--blockchar-remove-part requires --part <id>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    const auto before = document.parts.size();
    document.parts.erase(
        std::remove_if(
            document.parts.begin(),
            document.parts.end(),
            [&](const ri::content::BlockCharacterPart& part) {
                return part.id == *partArg || part.name == *partArg;
            }),
        document.parts.end());
    if (document.parts.size() == before) {
        throw std::runtime_error("Part not found: " + *partArg);
    }
    // Allow empty after remove: write via Serialize if valid, else empty shell.
    const fs::path path = ResolveBlockCharPath(workspace, *commandLine.GetValue("--blockchar"));
    if (document.parts.empty()) {
        document.parts.clear();
        std::ostringstream json;
        json << "{\n"
             << "  \"formatVersion\": " << document.formatVersion << ",\n"
             << "  \"id\": \"" << document.id << "\",\n"
             << "  \"displayName\": \"" << document.displayName << "\",\n"
             << "  \"rigPath\": \"" << document.rigPath << "\",\n"
             << "  \"parts\": []\n"
             << "}\n";
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << json.str();
    } else {
        SaveRequiredBlockChar(workspace, commandLine, document);
    }
    ri::core::LogInfo("Removed part '" + *partArg + "'. Parts now: " + std::to_string(document.parts.size()));
}

void ListBlockCharacterShapes() {
    ri::core::LogInfo("Block character primitives:");
    ri::core::LogInfo("  box       tapered box (--tsx/--tsy/--tsz optional)");
    ri::core::LogInfo("  prism     N-gon limb tube (--sides 4..16, default 6)");
    ri::core::LogInfo("  cylinder  rounder prism (--sides default 10)");
    ri::core::LogInfo("  dome      faceted helmet mass");
    ri::core::LogInfo("  wedge     boot / pointed mass along +Z");
    ri::core::LogInfo("  spike     pyramid / antenna tip (--sides default 4)");
    ri::core::LogInfo("  slab      thin armor plate with chamfer (--bevel)");
    ri::core::LogInfo("  bevel     chamfered box (--bevel 0..0.45)");
    ri::core::LogInfo("  capsule   cylinder + dome caps (--sides)");
}

void DuplicateBlockCharacterPart(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-duplicate requires --blockchar <path>.");
    }
    const auto partArg = commandLine.GetValue("--part");
    const auto asArg = commandLine.GetValue("--as");
    if (!partArg.has_value() || partArg->empty() || !asArg.has_value() || asArg->empty()) {
        throw std::runtime_error("--blockchar-duplicate requires --part <id> --as <new-id>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    const ri::content::BlockCharacterPart* source = FindPartByIdOrName(document, *partArg);
    if (source == nullptr) {
        throw std::runtime_error("Part not found: " + *partArg);
    }
    if (FindPartByIdOrName(document, *asArg) != nullptr) {
        throw std::runtime_error("Target part id already exists: " + *asArg);
    }
    ri::content::BlockCharacterPart copy = *source;
    copy.id = SanitizeAssetId(*asArg);
    copy.name = copy.id;
    copy.center.x += ParseFloatOr(commandLine.GetValue("--dx"), 0.0f);
    copy.center.y += ParseFloatOr(commandLine.GetValue("--dy"), 0.0f);
    copy.center.z += ParseFloatOr(commandLine.GetValue("--dz"), 0.0f);
    if (const auto bone = commandLine.GetValue("--bone"); bone.has_value() && !bone->empty()) {
        copy.boneName = *bone;
    }
    document.parts.push_back(std::move(copy));
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo("Duplicated '" + *partArg + "' → '" + SanitizeAssetId(*asArg) + "'.");
}

void MirrorBlockCharacterPart(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-mirror requires --blockchar <path>.");
    }
    const auto partArg = commandLine.GetValue("--part");
    if (!partArg.has_value() || partArg->empty()) {
        throw std::runtime_error("--blockchar-mirror requires --part <id>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    const ri::content::BlockCharacterPart* source = FindPartByIdOrName(document, *partArg);
    if (source == nullptr) {
        throw std::runtime_error("Part not found: " + *partArg);
    }
    const std::string axis = commandLine.GetValue("--axis").value_or("x");
    ri::content::BlockCharacterPart copy = *source;
    const std::string asId = commandLine.GetValue("--as").value_or(copy.id + "_mir");
    if (FindPartByIdOrName(document, asId) != nullptr) {
        throw std::runtime_error("Target part id already exists: " + asId);
    }
    copy.id = SanitizeAssetId(asId);
    copy.name = copy.id;
    if (axis == "x" || axis == "X") {
        copy.center.x = -copy.center.x;
        copy.rotationDegrees.y = -copy.rotationDegrees.y;
        copy.rotationDegrees.z = -copy.rotationDegrees.z;
    } else if (axis == "z" || axis == "Z") {
        copy.center.z = -copy.center.z;
        copy.rotationDegrees.x = -copy.rotationDegrees.x;
        copy.rotationDegrees.y = -copy.rotationDegrees.y;
    } else {
        throw std::runtime_error("--axis must be x or z for character mirroring.");
    }
    // Swap left_/right_ bone names when present.
    const std::string bone = copy.boneName;
    if (bone.rfind("left_", 0) == 0) {
        copy.boneName = "right_" + bone.substr(5);
    } else if (bone.rfind("right_", 0) == 0) {
        copy.boneName = "left_" + bone.substr(6);
    }
    document.parts.push_back(std::move(copy));
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo("Mirrored '" + *partArg + "' → '" + SanitizeAssetId(asId) + "' across " + axis + ".");
}

void InsetBlockCharacterDetail(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-inset requires --blockchar <path>.");
    }
    const auto partArg = commandLine.GetValue("--part");
    if (!partArg.has_value() || partArg->empty()) {
        throw std::runtime_error("--blockchar-inset requires --part <parent-id>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    const ri::content::BlockCharacterPart* parent = FindPartByIdOrName(document, *partArg);
    if (parent == nullptr) {
        throw std::runtime_error("Part not found: " + *partArg);
    }
    const float inset = std::clamp(ParseFloatOr(commandLine.GetValue("--inset"), 0.78f), 0.2f, 0.98f);
    const float depth = ParseFloatOr(commandLine.GetValue("--depth"), parent->halfExtent.z * 0.08f);
    const std::string face = commandLine.GetValue("--face").value_or("front");
    ri::content::BlockCharacterPart detail = *parent;
    const std::string asId = commandLine.GetValue("--as").value_or(parent->id + "_inset");
    if (FindPartByIdOrName(document, asId) != nullptr) {
        throw std::runtime_error("Target part id already exists: " + asId);
    }
    detail.id = SanitizeAssetId(asId);
    detail.name = detail.id;
    detail.shape = commandLine.GetValue("--shape").value_or("slab");
    detail.bevel = std::clamp(ParseFloatOr(commandLine.GetValue("--bevel"), 0.16f), 0.0f, 0.45f);
    detail.halfExtent.x = std::max(0.001f, parent->halfExtent.x * inset);
    detail.halfExtent.y = std::max(0.001f, parent->halfExtent.y * inset);
    detail.halfExtent.z = std::max(0.001f, std::min(parent->halfExtent.z * 0.35f, depth + 0.01f));
    detail.halfExtentTop = {};
    if (face == "front") {
        detail.center.z = parent->center.z + parent->halfExtent.z + detail.halfExtent.z * 0.15f;
    } else if (face == "back") {
        detail.center.z = parent->center.z - parent->halfExtent.z - detail.halfExtent.z * 0.15f;
    } else if (face == "top") {
        detail.center.y = parent->center.y + parent->halfExtent.y + detail.halfExtent.y * 0.15f;
        std::swap(detail.halfExtent.y, detail.halfExtent.z);
    } else if (face == "bottom") {
        detail.center.y = parent->center.y - parent->halfExtent.y - detail.halfExtent.y * 0.15f;
        std::swap(detail.halfExtent.y, detail.halfExtent.z);
    } else {
        throw std::runtime_error("--face must be front|back|top|bottom.");
    }
    if (commandLine.GetValue("--color").has_value()) {
        detail.albedoColor = ParseRgbOr(commandLine.GetValue("--color"), detail.albedoColor);
    } else {
        detail.albedoColor.x = std::clamp(detail.albedoColor.x * 0.82f, 0.0f, 1.0f);
        detail.albedoColor.y = std::clamp(detail.albedoColor.y * 0.82f, 0.0f, 1.0f);
        detail.albedoColor.z = std::clamp(detail.albedoColor.z * 0.82f, 0.0f, 1.0f);
    }
    if (const auto tex = commandLine.GetValue("--texture"); tex.has_value()) {
        detail.albedoTexture = *tex;
    }
    document.parts.push_back(std::move(detail));
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo("Inset detail '" + SanitizeAssetId(asId) + "' on '" + *partArg + "' (" + face + ").");
}

void AttachBlockCharacterDetail(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-attach requires --blockchar <path>.");
    }
    const auto parentArg = commandLine.GetValue("--parent");
    if (!parentArg.has_value() || parentArg->empty()) {
        throw std::runtime_error("--blockchar-attach requires --parent <part-id>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    const ri::content::BlockCharacterPart* parent = FindPartByIdOrName(document, *parentArg);
    if (parent == nullptr) {
        throw std::runtime_error("Parent part not found: " + *parentArg);
    }
    const std::string along = commandLine.GetValue("--along").value_or("front");
    const float offset = ParseFloatOr(commandLine.GetValue("--offset"), 0.0f);
    ri::content::BlockCharacterPart detail{};
    detail.id = SanitizeAssetId(commandLine.GetValue("--part").value_or(
        parent->id + "_detail_" + std::to_string(document.parts.size())));
    detail.name = commandLine.GetValue("--part-name").value_or(detail.id);
    detail.boneName = commandLine.GetValue("--bone").value_or(parent->boneName);
    detail.shape = commandLine.GetValue("--shape").value_or("box");
    detail.halfExtent = {
        .x = ParseFloatOr(commandLine.GetValue("--sx"), 0.06f),
        .y = ParseFloatOr(commandLine.GetValue("--sy"), 0.06f),
        .z = ParseFloatOr(commandLine.GetValue("--sz"), 0.06f),
    };
    detail.halfExtentTop = {
        .x = ParseFloatOr(commandLine.GetValue("--tsx"), 0.0f),
        .y = ParseFloatOr(commandLine.GetValue("--tsy"), 0.0f),
        .z = ParseFloatOr(commandLine.GetValue("--tsz"), 0.0f),
    };
    detail.rotationDegrees = parent->rotationDegrees;
    detail.rotationDegrees.x += ParseFloatOr(commandLine.GetValue("--rx"), 0.0f);
    detail.rotationDegrees.y += ParseFloatOr(commandLine.GetValue("--ry"), 0.0f);
    detail.rotationDegrees.z += ParseFloatOr(commandLine.GetValue("--rz"), 0.0f);
    detail.albedoColor = ParseRgbOr(commandLine.GetValue("--color"), parent->albedoColor);
    detail.roughness = ParseFloatOr(commandLine.GetValue("--roughness"), parent->roughness);
    detail.metallic = ParseFloatOr(commandLine.GetValue("--metallic"), parent->metallic);
    detail.sides = static_cast<int>(ParseFloatOr(commandLine.GetValue("--sides"), 0.0f));
    detail.bevel = std::clamp(ParseFloatOr(commandLine.GetValue("--bevel"), 0.0f), 0.0f, 0.45f);
    detail.albedoTexture = commandLine.GetValue("--texture").value_or(parent->albedoTexture);
    detail.center = parent->center;
    if (along == "front") {
        detail.center.z += parent->halfExtent.z + detail.halfExtent.z + offset;
    } else if (along == "back") {
        detail.center.z -= parent->halfExtent.z + detail.halfExtent.z + offset;
    } else if (along == "top") {
        detail.center.y += parent->halfExtent.y + detail.halfExtent.y + offset;
    } else if (along == "bottom") {
        detail.center.y -= parent->halfExtent.y + detail.halfExtent.y + offset;
    } else if (along == "left") {
        detail.center.x -= parent->halfExtent.x + detail.halfExtent.x + offset;
    } else if (along == "right") {
        detail.center.x += parent->halfExtent.x + detail.halfExtent.x + offset;
    } else {
        throw std::runtime_error("--along must be front|back|top|bottom|left|right.");
    }
    if (commandLine.GetValue("--x").has_value()) {
        detail.center.x = ParseFloatOr(commandLine.GetValue("--x"), detail.center.x);
    }
    if (commandLine.GetValue("--y").has_value()) {
        detail.center.y = ParseFloatOr(commandLine.GetValue("--y"), detail.center.y);
    }
    if (commandLine.GetValue("--z").has_value()) {
        detail.center.z = ParseFloatOr(commandLine.GetValue("--z"), detail.center.z);
    }
    if (FindPartByIdOrName(document, detail.id) != nullptr) {
        throw std::runtime_error("Part id already exists: " + detail.id);
    }
    document.parts.push_back(std::move(detail));
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo(
        "Attached '" + document.parts.back().id + "' to '" + *parentArg + "' along " + along + ".");
}

void RecolorBlockCharacterParts(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-recolor requires --blockchar <path>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    const auto partArg = commandLine.GetValue("--part");
    const auto mul = ParseRgbOr(commandLine.GetValue("--mul"), {.x = 1.0f, .y = 1.0f, .z = 1.0f});
    const bool hasColor = commandLine.GetValue("--color").has_value();
    const auto color = ParseRgbOr(commandLine.GetValue("--color"), {});
    std::size_t touched = 0;
    for (ri::content::BlockCharacterPart& part : document.parts) {
        if (partArg.has_value() && !partArg->empty() && part.id != *partArg && part.name != *partArg) {
            continue;
        }
        if (hasColor) {
            part.albedoColor = color;
        } else {
            part.albedoColor.x = std::clamp(part.albedoColor.x * mul.x, 0.0f, 1.0f);
            part.albedoColor.y = std::clamp(part.albedoColor.y * mul.y, 0.0f, 1.0f);
            part.albedoColor.z = std::clamp(part.albedoColor.z * mul.z, 0.0f, 1.0f);
        }
        if (commandLine.GetValue("--roughness").has_value()) {
            part.roughness = ParseFloatOr(commandLine.GetValue("--roughness"), part.roughness);
        }
        if (commandLine.GetValue("--metallic").has_value()) {
            part.metallic = ParseFloatOr(commandLine.GetValue("--metallic"), part.metallic);
        }
        ++touched;
    }
    if (touched == 0U) {
        throw std::runtime_error("No parts matched for recolor.");
    }
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo("Recolored " + std::to_string(touched) + " part(s).");
}

void BevelBlockCharacterPart(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-bevel requires --blockchar <path>.");
    }
    const auto partArg = commandLine.GetValue("--part");
    if (!partArg.has_value() || partArg->empty()) {
        throw std::runtime_error("--blockchar-bevel requires --part <id>.");
    }
    auto document = LoadRequiredBlockChar(workspace, commandLine);
    ri::content::BlockCharacterPart* part = FindPartByIdOrName(document, *partArg);
    if (part == nullptr) {
        throw std::runtime_error("Part not found: " + *partArg);
    }
    part->shape = "bevel";
    part->bevel = std::clamp(ParseFloatOr(commandLine.GetValue("--bevel"), 0.2f), 0.02f, 0.45f);
    SaveRequiredBlockChar(workspace, commandLine, document);
    ri::core::LogInfo("Beveled '" + part->id + "' amount=" + std::to_string(part->bevel));
}

void ReportBlockCharacter(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto pathArg = commandLine.GetValue("--blockchar-report");
    const std::string pathText = pathArg.has_value() && !pathArg->empty()
        ? *pathArg
        : commandLine.GetValue("--blockchar").value_or("");
    if (pathText.empty()) {
        throw std::runtime_error("Missing --blockchar-report <path> (or --blockchar <path>).");
    }
    const fs::path path = ResolveBlockCharPath(workspace, pathText);
    const auto document = ri::content::LoadBlockCharacterDocument(path);
    if (!document.has_value()) {
        throw std::runtime_error("Could not load block character: " + path.string());
    }
    const auto report = ri::content::ValidateBlockCharacterDocument(*document);
    ri::core::LogInfo("Block character report:");
    ri::core::LogInfo("  Path: " + path.string());
    ri::core::LogInfo("  Id: " + document->id);
    ri::core::LogInfo("  Name: " + document->displayName);
    ri::core::LogInfo("  Rig: " + (document->rigPath.empty() ? "(none)" : document->rigPath));
    ri::core::LogInfo("  Parts: " + std::to_string(document->parts.size()));
    ri::core::LogInfo(std::string("  Valid: ") + (report.valid ? "yes" : "no"));
    for (const auto& part : document->parts) {
        ri::core::LogInfo(
            "  - " + part.id + " | bone=" + part.boneName + " | shape=" + part.shape
            + " | pos=(" + std::to_string(part.center.x) + "," + std::to_string(part.center.y) + ","
            + std::to_string(part.center.z) + ")"
            + " | half=(" + std::to_string(part.halfExtent.x) + "," + std::to_string(part.halfExtent.y)
            + "," + std::to_string(part.halfExtent.z) + ")"
            + " | rot=(" + std::to_string(part.rotationDegrees.x) + ","
            + std::to_string(part.rotationDegrees.y) + "," + std::to_string(part.rotationDegrees.z)
            + ")"
            + (part.sides > 0 ? (" | sides=" + std::to_string(part.sides)) : "")
            + (part.bevel > 0.0f ? (" | bevel=" + std::to_string(part.bevel)) : "")
            + (part.albedoTexture.empty() ? "" : (" | tex=" + part.albedoTexture)));
    }
    for (const std::string& error : report.errors) {
        ri::core::LogInfo("  Error: " + error);
    }
}

void WriteBlockCharacterTextures(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-write-textures requires --blockchar <path>.");
    }
    const auto document = LoadRequiredBlockChar(workspace, commandLine);
    std::unordered_map<std::string, ri::content::DeclarativeVec3> textureColors{};
    for (const auto& part : document.parts) {
        if (!part.albedoTexture.empty()) {
            textureColors[part.albedoTexture] = part.albedoColor;
        }
    }
    std::size_t written = 0;
    for (const auto& [relativeTexture, color] : textureColors) {
        WriteSolidColorTga(
            workspace.root / "Assets" / "Textures" / relativeTexture, color.x, color.y, color.z);
        ++written;
    }
    ri::core::LogInfo("Wrote " + std::to_string(written) + " solid albedo map(s) under Assets/Textures.");
}

void SyncBlockCharacterSculpt(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (!commandLine.GetValue("--blockchar").has_value()) {
        throw std::runtime_error("--blockchar-sync-sculpt requires --blockchar <path>.");
    }
    const auto document = LoadRequiredBlockChar(workspace, commandLine);
    if (document.parts.empty()) {
        throw std::runtime_error("Block character has no parts to sync — model with --blockchar-add-part first.");
    }
    const fs::path blockCharPath = ResolveBlockCharPath(workspace, *commandLine.GetValue("--blockchar"));
    fs::path sculptPath = workspace.assetsSource / "sculpts" / (document.id + ".ri_sculpt.json");
    if (const auto sculptArg = commandLine.GetValue("--sculpt"); sculptArg.has_value() && !sculptArg->empty()) {
        sculptPath = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    }
    std::error_code existsError{};
    if (fs::exists(sculptPath, existsError) && !commandLine.HasFlag("--overwrite")) {
        throw std::runtime_error("Sculpt already exists (use --overwrite): " + sculptPath.string());
    }

    fs::path rigPath{};
    if (const auto rigArg = commandLine.GetValue("--rig"); rigArg.has_value() && !rigArg->empty()) {
        rigPath = ResolveAuthoringPath(workspace, fs::path(*rigArg));
    } else if (!document.rigPath.empty()) {
        rigPath = ResolveAuthoringPath(workspace, fs::path(document.rigPath));
    }
    if (rigPath.empty()) {
        throw std::runtime_error("--blockchar-sync-sculpt needs --rig <path> or blockchar.rigPath.");
    }
    const auto rig = ri::scene::LoadRigDefinition(rigPath);
    if (!rig.has_value() || !ri::scene::ValidateRigDefinition(*rig).valid) {
        throw std::runtime_error("Invalid rig: " + rigPath.string());
    }

    ri::content::NativeSculptDocument sculpt =
        ri::scene::CreateBlockCharacterSculptDocument(document.id, document.displayName, document);
    sculpt.blockCharPath = RelativeAuthoringSourcePath(workspace, blockCharPath);
    const std::string relativeRig = RelativeAuthoringSourcePath(workspace, rigPath);
    const ri::scene::NativeSculptBindResult bound =
        ri::scene::BindNativeSculptToRig(sculpt, *rig, relativeRig, false);
    if (!bound.valid) {
        throw std::runtime_error(bound.summary.empty() ? "Bind failed." : bound.summary);
    }
    ri::scene::LimitNativeSculptInfluences(sculpt);
    ri::scene::NormalizeAllNativeSculptWeights(sculpt);
    EnsureParentDirectoryExists(sculptPath);
    if (!ri::content::SaveNativeSculptDocument(sculptPath, sculpt)) {
        throw std::runtime_error("Failed to write sculpt: " + sculptPath.string());
    }
    ri::core::LogInfo("Synced sculpt from authored block character:");
    ri::core::LogInfo("  BlockChar: " + blockCharPath.string());
    ri::core::LogInfo("  Sculpt: " + sculptPath.string());
    ri::core::LogInfo("  Parts: " + std::to_string(document.parts.size()));
    ri::core::LogInfo("  Verts: " + std::to_string(sculpt.mesh.positions.size()));
    ri::core::LogInfo("  Bind: " + bound.summary);
}

void AuthorHumanoidMotionClip(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine, const bool walk) {
    const char* flag = walk ? "--anim-author-walk" : "--anim-author-idle";
    const auto animArg = commandLine.GetValue(flag);
    if (!animArg.has_value() || animArg->empty()) {
        throw std::runtime_error(std::string("Missing ") + flag + " <anim-path>.");
    }
    const fs::path animPath = ResolveAuthoringPath(workspace, fs::path(*animArg));
    auto clip = ri::content::LoadNativeAnimationDocument(animPath);
    if (!clip.has_value()) {
        throw std::runtime_error("Could not load animation: " + animPath.string());
    }
    fs::path rigPath{};
    if (const auto rigArg = commandLine.GetValue("--rig"); rigArg.has_value() && !rigArg->empty()) {
        rigPath = ResolveAuthoringPath(workspace, fs::path(*rigArg));
    } else if (!clip->rigPath.empty()) {
        rigPath = ResolveAuthoringPath(workspace, fs::path(clip->rigPath));
    }
    if (rigPath.empty()) {
        throw std::runtime_error(std::string(flag) + " needs --rig <path> or clip.rigPath.");
    }
    const auto rig = ri::scene::LoadRigDefinition(rigPath);
    if (!rig.has_value() || !ri::scene::ValidateRigDefinition(*rig).valid) {
        throw std::runtime_error("Invalid rig: " + rigPath.string());
    }
    const float intensity = std::clamp(ParseFloatOr(commandLine.GetValue("--motion-intensity"), 1.0f), 0.25f, 2.5f);
    const ri::scene::HumanoidMotionAuthorOptions options{.intensity = intensity, .looping = true};
    if (walk) {
        ri::scene::AuthorHumanoidWalkClip(*clip, *rig, options);
    } else {
        ri::scene::AuthorHumanoidIdleClip(*clip, *rig, options);
    }
    if (!ri::content::SaveNativeAnimationDocument(animPath, *clip)) {
        throw std::runtime_error("Failed to write animation: " + animPath.string());
    }
    ri::core::LogInfo(
        std::string(walk ? "Authored walk" : "Authored idle") + " keys on " + animPath.string()
        + " (intensity=" + std::to_string(intensity) + ")");
}

[[nodiscard]] std::vector<fs::path> ListFunctionDirectories(const WorkspaceLayout& workspace) {
    return {
        RiToolPackageRoot(workspace) / "functions",
        workspace.root / "Tools" / "ri_tool" / "functions",
        workspace.assetsSource / "functions",
    };
}

[[nodiscard]] fs::path FindFunctionFile(const WorkspaceLayout& workspace, const std::string& name) {
    const std::vector<std::string> names = {
        name,
        name + ".rifunction",
        name + ".cmd",
        name + ".bat",
        name + ".ps1",
    };
    std::error_code ec{};
    for (const fs::path& dir : ListFunctionDirectories(workspace)) {
        if (!fs::is_directory(dir, ec)) {
            continue;
        }
        for (const std::string& fileName : names) {
            const fs::path candidate = dir / fileName;
            if (fs::is_regular_file(candidate, ec)) {
                return fs::weakly_canonical(candidate, ec);
            }
        }
    }
    throw std::runtime_error(
        "Function not found: " + name
        + " (drop a .rifunction into Tools/ri_tool/functions or Assets/Source/functions)");
}

void ListToolFunctions(const WorkspaceLayout& workspace) {
    ri::core::LogInfo("ri_tool functions (user-dropped recipes — not engine content):");
    std::error_code ec{};
    std::size_t count = 0;
    for (const fs::path& dir : ListFunctionDirectories(workspace)) {
        if (!fs::is_directory(dir, ec)) {
            continue;
        }
        ri::core::LogInfo("  Folder: " + dir.string());
        for (const fs::directory_entry& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) {
                continue;
            }
            const std::string ext = entry.path().extension().string();
            if (ext != ".rifunction" && ext != ".cmd" && ext != ".bat" && ext != ".ps1") {
                continue;
            }
            ri::core::LogInfo("    " + entry.path().filename().string());
            ++count;
        }
    }
    if (count == 0U) {
        ri::core::LogInfo("  (none yet — model with --blockchar-add-part, or drop a .rifunction recipe)");
    }
}

[[nodiscard]] std::string SubstituteFunctionVars(
    std::string line,
    const std::unordered_map<std::string, std::string>& vars) {
    for (const auto& [key, value] : vars) {
        const std::string dollar = "$" + key;
        const std::string braced = "${" + key + "}";
        for (std::string::size_type pos = 0; (pos = line.find(braced, pos)) != std::string::npos;) {
            line.replace(pos, braced.size(), value);
            pos += value.size();
        }
        for (std::string::size_type pos = 0; (pos = line.find(dollar, pos)) != std::string::npos;) {
            // Avoid replacing $id inside $identity-like tokens: require end or non-alnum.
            const std::string::size_type end = pos + dollar.size();
            if (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) || line[end] == '_')) {
                pos = end;
                continue;
            }
            line.replace(pos, dollar.size(), value);
            pos += value.size();
        }
    }
    return line;
}

[[nodiscard]] std::vector<std::string> TokenizeFunctionLine(const std::string& line) {
    std::vector<std::string> tokens{};
    std::string current{};
    bool inQuotes = false;
    for (const char character : line) {
        if (character == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && std::isspace(static_cast<unsigned char>(character))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(character);
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

void RunToolFunctionStep(
    const WorkspaceLayout& workspace,
    const fs::path& toolExe,
    const std::vector<std::string>& tokens) {
    if (tokens.empty()) {
        return;
    }
#if defined(_WIN32)
    std::wstring command = L"\"" + toolExe.wstring() + L"\" --root \"" + workspace.root.wstring() + L"\"";
    for (const std::string& token : tokens) {
        command.push_back(L' ');
        command.push_back(L'"');
        command += std::wstring(token.begin(), token.end());
        command.push_back(L'"');
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = command;
    if (!CreateProcessW(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            workspace.root.wstring().c_str(),
            &startup,
            &process)) {
        throw std::runtime_error("Function step failed to launch: " + tokens.front());
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exitCode != 0) {
        throw std::runtime_error("Function step failed (" + std::to_string(exitCode) + "): " + tokens.front());
    }
#else
    (void)workspace;
    (void)toolExe;
    throw std::runtime_error("--function is only supported on Windows builds of ri_tool currently.");
#endif
}

void RunToolFunction(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto nameArg = commandLine.GetValue("--function");
    if (!nameArg.has_value() || nameArg->empty()) {
        throw std::runtime_error("Missing --function <name>.");
    }
    const auto idArg = commandLine.GetValue("--id");
    if (!idArg.has_value() || idArg->empty()) {
        throw std::runtime_error("--function requires --id <character-id>.");
    }
    const std::string characterId = SanitizeAssetId(*idArg);
    const std::string displayName = commandLine.GetValue("--name").value_or(*idArg);
    const fs::path functionPath = FindFunctionFile(workspace, *nameArg);
    const std::string ext = functionPath.extension().string();

#if defined(_WIN32)
    wchar_t modulePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0U) {
        throw std::runtime_error("Could not resolve ri_tool path.");
    }
    const fs::path toolExe = fs::path(modulePath);
#else
    const fs::path toolExe{};
#endif

    ri::core::LogInfo("Running user function: " + functionPath.string());
    if (ext == ".ps1" || ext == ".cmd" || ext == ".bat") {
#if defined(_WIN32)
        std::wstring command;
        if (ext == ".ps1") {
            command = L"powershell -NoProfile -ExecutionPolicy Bypass -File \"" + functionPath.wstring()
                + L"\" -Id \"" + std::wstring(characterId.begin(), characterId.end())
                + L"\" -Name \"" + std::wstring(displayName.begin(), displayName.end())
                + L"\" -Root \"" + workspace.root.wstring() + L"\" -Tool \"" + toolExe.wstring() + L"\"";
        } else {
            command = L"cmd /C \"" + functionPath.wstring() + L"\" \""
                + std::wstring(characterId.begin(), characterId.end()) + L"\" \""
                + std::wstring(displayName.begin(), displayName.end()) + L"\" \""
                + workspace.root.wstring() + L"\" \"" + toolExe.wstring() + L"\"";
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        std::wstring mutableCommand = command;
        if (!CreateProcessW(
                nullptr,
                mutableCommand.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                workspace.root.wstring().c_str(),
                &startup,
                &process)) {
            throw std::runtime_error("Failed to launch function script.");
        }
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        if (exitCode != 0) {
            throw std::runtime_error("Function script failed with exit " + std::to_string(exitCode));
        }
#else
        throw std::runtime_error("Script functions require Windows.");
#endif
        return;
    }

    std::unordered_map<std::string, std::string> vars{
        {"id", characterId},
        {"name", displayName},
        {"root", workspace.root.generic_string()},
    };
    if (commandLine.HasFlag("--overwrite")) {
        vars.emplace("overwrite", "--overwrite");
    } else {
        vars.emplace("overwrite", "");
    }

    std::ifstream input(functionPath);
    if (!input) {
        throw std::runtime_error("Could not read function: " + functionPath.string());
    }
    std::string line;
    std::size_t step = 0;
    while (std::getline(input, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line = SubstituteFunctionVars(line, vars);
        // Collapse leftover empty overwrite token spacing.
        while (line.find("  ") != std::string::npos) {
            const auto pos = line.find("  ");
            line.replace(pos, 2, " ");
        }
        auto tokens = TokenizeFunctionLine(line);
        if (tokens.empty()) {
            continue;
        }
        ++step;
        ri::core::LogInfo("  Step " + std::to_string(step) + ": " + tokens.front());
        RunToolFunctionStep(workspace, toolExe, tokens);
    }
    ri::core::LogInfo("Function complete: " + *nameArg + " → " + characterId);
    if (commandLine.HasFlag("--open")) {
        const fs::path walkPath =
            workspace.assetsSource / "anims" / (characterId + "_walk.ri_anim.json");
        const fs::path blockPath =
            workspace.assetsSource / "blockchars" / (characterId + ".ri_blockchar.json");
        std::error_code ec{};
        if (fs::is_regular_file(walkPath, ec)) {
            LaunchForgePreview(workspace, walkPath);
        } else if (fs::is_regular_file(blockPath, ec)) {
            LaunchForgePreview(workspace, blockPath);
        }
    }
}

void CreateForgeCharacter(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto idArg = commandLine.GetValue("--forge-character-create");
    if (!idArg.has_value() || idArg->empty()) {
        throw std::runtime_error("Missing --forge-character-create <character-id>.");
    }
    const std::string styleOrCage = commandLine.GetValue("--style").value_or(
        commandLine.GetValue("--cage").value_or("sphere"));
    const std::string styleLower = [&]() {
        std::string value = styleOrCage;
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }();
    if (styleLower == "psx" || styleLower == "psx_humanoid" || styleLower == "block"
        || styleLower == "blockchar") {
        throw std::runtime_error(
            "--forge-character-create no longer emits prebuilt characters. "
            "Model with --blockchar-create / --blockchar-add-part / --blockchar-set-part / "
            "--blockchar-nudge / --blockchar-sync-sculpt (and optional --function recipes you drop "
            "in Tools/ri_tool/functions). Clay scaffold: --style sphere|cube.");
    }

    const std::string characterId = SanitizeAssetId(*idArg);
    const std::string displayName = commandLine.GetValue("--name").value_or(*idArg);
    const bool overwrite = commandLine.HasFlag("--overwrite");
    const ri::scene::NativeSculptCage cage = ri::scene::ParseNativeSculptCage(styleOrCage);
    if (cage == ri::scene::NativeSculptCage::PsxHumanoid) {
        throw std::runtime_error(
            "psx cage is not a clay scaffold. Author parts with --blockchar-add-part instead.");
    }
    const bool wantWalk = commandLine.HasFlag("--walk");
    const bool openForge = commandLine.HasFlag("--open");
    const float motionIntensity = std::clamp(
        ParseFloatOr(commandLine.GetValue("--motion-intensity"), 1.0f), 0.25f, 2.5f);
    const ri::scene::HumanoidMotionAuthorOptions motionOptions{.intensity = motionIntensity, .looping = true};

    const fs::path rigPath = workspace.assetsSource / "rigs" / (characterId + ".ri_rig.json");
    const fs::path sculptPath =
        workspace.assetsSource / "sculpts" / (characterId + ".ri_sculpt.json");
    const fs::path idlePath =
        workspace.assetsSource / "anims" / (characterId + "_idle.ri_anim.json");
    const fs::path walkPath =
        workspace.assetsSource / "anims" / (characterId + "_walk.ri_anim.json");

    std::error_code existsError{};
    std::vector<fs::path> outputs = {rigPath, sculptPath, idlePath};
    if (wantWalk) {
        outputs.push_back(walkPath);
    }
    for (const fs::path& path : outputs) {
        if (fs::exists(path, existsError) && !overwrite) {
            throw std::runtime_error(
                "Character output already exists (use --overwrite): " + path.string());
        }
    }

    EnsureParentDirectoryExists(rigPath);
    EnsureParentDirectoryExists(sculptPath);
    EnsureParentDirectoryExists(idlePath);
    if (wantWalk) {
        EnsureParentDirectoryExists(walkPath);
    }

    const ri::scene::RigDefinition rig =
        ri::scene::CreateHumanoidRigDefinition(characterId, displayName);
    if (!ri::scene::ValidateRigDefinition(rig).valid) {
        throw std::runtime_error("Internal humanoid rig template failed validation.");
    }
    if (!ri::scene::SaveRigDefinition(rigPath, rig)) {
        throw std::runtime_error("Failed to write rig: " + rigPath.string());
    }

    const std::string relativeRig = RelativeAuthoringSourcePath(workspace, rigPath);
    ri::content::NativeSculptDocument sculpt =
        ri::scene::CreateNativeSculptDocument(characterId, displayName + " Clay", cage);
    if (!FitSculptMeshToRig(sculpt, rig)) {
        throw std::runtime_error("Could not fit clay cage to humanoid bone bounds.");
    }
    const ri::scene::NativeSculptBindResult bound =
        ri::scene::BindNativeSculptToRig(sculpt, rig, relativeRig, true);
    if (!bound.valid) {
        throw std::runtime_error(bound.summary.empty() ? "Bind failed." : bound.summary);
    }
    const std::size_t softA = ri::scene::SmoothNativeSculptWeights(sculpt);
    const std::size_t softB = ri::scene::SmoothNativeSculptWeights(sculpt);
    const std::size_t capped = ri::scene::LimitNativeSculptInfluences(sculpt);
    const std::size_t normalized = ri::scene::NormalizeAllNativeSculptWeights(sculpt);
    if (!ri::content::SaveNativeSculptDocument(sculptPath, sculpt)) {
        throw std::runtime_error("Failed to write sculpt: " + sculptPath.string());
    }

    ri::content::NativeAnimationDocument idle =
        ri::content::CreateNativeAnimationDocument(
            characterId + "_idle", displayName + " Idle", relativeRig);
    ri::scene::AuthorHumanoidIdleClip(idle, rig, motionOptions);
    if (!ri::content::SaveNativeAnimationDocument(idlePath, idle)) {
        throw std::runtime_error("Failed to write animation: " + idlePath.string());
    }
    if (wantWalk) {
        ri::content::NativeAnimationDocument walk = ri::content::CreateNativeAnimationDocument(
            characterId + "_walk", displayName + " Walk", relativeRig);
        ri::scene::AuthorHumanoidWalkClip(walk, rig, motionOptions);
        if (!ri::content::SaveNativeAnimationDocument(walkPath, walk)) {
            throw std::runtime_error("Failed to write walk animation: " + walkPath.string());
        }
    }

    ri::core::LogInfo("Created clay character scaffold (not a prebuilt look):");
    ri::core::LogInfo("  Id: " + characterId);
    ri::core::LogInfo("  Cage: " + std::string(ri::scene::NativeSculptCageName(cage)));
    ri::core::LogInfo("  Rig: " + rigPath.string());
    ri::core::LogInfo("  Sculpt: " + sculptPath.string());
    ri::core::LogInfo("  Idle: " + idlePath.string());
    if (wantWalk) {
        ri::core::LogInfo("  Walk: " + walkPath.string());
    }
    ri::core::LogInfo("  Bind: " + bound.summary);
    ri::core::LogInfo(
        "  Soft/Cap/Norm: soft=" + std::to_string(softA + softB) + " cap=" + std::to_string(capped)
        + " norm=" + std::to_string(normalized));
    ri::core::LogInfo(
        "  Tip: block characters are modeled with --blockchar-add-part (place/scale/rotate primitives)");
    if (openForge) {
        LaunchForgePreview(workspace, wantWalk ? walkPath : sculptPath);
    }
}


void ProbeForgeHandoff(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    auto pathArg = commandLine.GetValue("--forge-handoff-probe");
    if (!pathArg.has_value() || pathArg->empty()) {
        pathArg = commandLine.GetValue("--asset");
    }
    if (!pathArg.has_value() || pathArg->empty()) {
        throw std::runtime_error(
            "Missing --forge-handoff-probe <asset> (or --asset <path>).");
    }
    const fs::path assetPath = ResolveAuthoringPath(workspace, fs::path(*pathArg));
    const ri::content::AuthoringHandoffReport report = ri::content::BuildAuthoringHandoff({
        .workspaceRoot = workspace.root,
        .assetPath = assetPath,
        .gameId = commandLine.GetValue("--game").value_or(""),
    });
    ri::core::LogInfo(
        std::string("Forge handoff: ") + (report.valid ? "ready" : "rejected"));
    ri::core::LogInfo(std::string("Asset kind: ") + std::string(ri::content::ToString(report.assetKind)));
    ri::core::LogInfo("Asset: " + report.assetPath.string());
    if (!report.workspaceRelativePath.empty()) {
        ri::core::LogInfo("Relative: " + report.workspaceRelativePath.generic_string());
    }
    if (!report.editorArguments.empty()) {
        std::string args = "Editor args:";
        for (const std::string& argument : report.editorArguments) {
            args += " ";
            args += argument;
        }
        ri::core::LogInfo(args);
    }
    for (const std::string& issue : report.issues) {
        ri::core::LogInfo("Issue: " + issue);
    }
    if (!report.valid) {
        throw std::runtime_error("Forge handoff rejected.");
    }
}

void CreateNativeSculptAsset(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto idArg = commandLine.GetValue("--sculpt-create");
    if (!idArg.has_value() || idArg->empty()) {
        throw std::runtime_error("Missing --sculpt-create <sculpt-id>.");
    }
    const ri::scene::NativeSculptCage cage =
        ri::scene::ParseNativeSculptCage(commandLine.GetValue("--cage").value_or("sphere"));
    const std::string sculptId = SanitizeAssetId(*idArg);
    fs::path output = workspace.assetsSource / "sculpts" / (sculptId + ".ri_sculpt.json");
    if (const auto outputArg = commandLine.GetValue("--output"); outputArg.has_value() && !outputArg->empty()) {
        output = ResolveAuthoringPath(workspace, fs::path(*outputArg));
    }
    std::error_code existsError{};
    if (fs::exists(output, existsError) && !commandLine.HasFlag("--overwrite")) {
        throw std::runtime_error("Sculpt output already exists (use --overwrite to replace it): " + output.string());
    }
    EnsureParentDirectoryExists(output);
    const std::string displayName = commandLine.GetValue("--name").value_or(
        cage == ri::scene::NativeSculptCage::Cube
            ? "Hard Surface Block"
            : (cage == ri::scene::NativeSculptCage::PsxHumanoid ? "PSX Humanoid" : "Clay Sphere"));
    const ri::content::NativeSculptDocument document =
        ri::scene::CreateNativeSculptDocument(sculptId, displayName, cage);
    if (!ri::content::SaveNativeSculptDocument(output, document)) {
        throw std::runtime_error("Failed to write sculpt: " + output.string());
    }
    ri::core::LogInfo("Created native sculpt:");
    ri::core::LogInfo("  Id: " + document.id);
    ri::core::LogInfo("  Cage: " + document.cage);
    ri::core::LogInfo("  Output: " + output.string());
    ri::core::LogInfo("  Vertices: " + std::to_string(document.mesh.positions.size()));
}

void CreateNativeAnimationAsset(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto idArg = commandLine.GetValue("--anim-create");
    if (!idArg.has_value() || idArg->empty()) {
        throw std::runtime_error("Missing --anim-create <clip-id>.");
    }
    const auto rigArg = commandLine.GetValue("--rig");
    if (!rigArg.has_value() || rigArg->empty()) {
        throw std::runtime_error("--anim-create requires --rig <path>.");
    }
    const fs::path rigPath = ResolveAuthoringPath(workspace, fs::path(*rigArg));
    const auto rig = ri::scene::LoadRigDefinition(rigPath);
    if (!rig.has_value() || !ri::scene::ValidateRigDefinition(*rig).valid) {
        throw std::runtime_error("Clip needs a valid rig document: " + rigPath.string());
    }
    const std::string clipId = SanitizeAssetId(*idArg);
    fs::path output = workspace.assetsSource / "anims" / (clipId + ".ri_anim.json");
    if (const auto outputArg = commandLine.GetValue("--output"); outputArg.has_value() && !outputArg->empty()) {
        output = ResolveAuthoringPath(workspace, fs::path(*outputArg));
    }
    std::error_code existsError{};
    if (fs::exists(output, existsError) && !commandLine.HasFlag("--overwrite")) {
        throw std::runtime_error("Animation output already exists (use --overwrite to replace it): " + output.string());
    }
    EnsureParentDirectoryExists(output);
    const std::string displayName = commandLine.GetValue("--name").value_or(*idArg);
    ri::content::NativeAnimationDocument document = ri::content::CreateNativeAnimationDocument(
        clipId, displayName, RelativeAuthoringSourcePath(workspace, rigPath));
    ri::scene::SeedNativeAnimationFromRig(document, *rig);
    const ri::content::NativeAnimationValidationReport validation =
        ri::content::ValidateNativeAnimationDocument(document);
    if (!validation.valid) {
        std::string detail = "Animation document failed validation.";
        for (const std::string& error : validation.errors) {
            detail += " ";
            detail += error;
        }
        throw std::runtime_error(detail);
    }
    if (!ri::content::SaveNativeAnimationDocument(output, document)) {
        throw std::runtime_error("Failed to write animation: " + output.string());
    }
    ri::core::LogInfo("Created motion clip:");
    ri::core::LogInfo("  Id: " + document.id);
    ri::core::LogInfo("  Rig: " + document.rigPath);
    ri::core::LogInfo("  Output: " + output.string());
    ri::core::LogInfo("  Tracks: " + std::to_string(document.tracks.size()));
}

void BindSculptToRig(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto sculptArg = commandLine.GetValue("--sculpt");
    const auto rigArg = commandLine.GetValue("--rig");
    if (!sculptArg.has_value() || sculptArg->empty() || !rigArg.has_value() || rigArg->empty()) {
        throw std::runtime_error("--sculpt-bind requires --sculpt <path> and --rig <path>.");
    }
    const fs::path sculptPath = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    const fs::path rigPath = ResolveAuthoringPath(workspace, fs::path(*rigArg));
    auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not load clay: " + sculptPath.string());
    }
    const auto rig = ri::scene::LoadRigDefinition(rigPath);
    if (!rig.has_value() || !ri::scene::ValidateRigDefinition(*rig).valid) {
        throw std::runtime_error("Invalid rig: " + rigPath.string());
    }
    const ri::scene::NativeSculptBindResult bound = ri::scene::BindNativeSculptToRig(
        *sculpt, *rig, RelativeAuthoringSourcePath(workspace, rigPath), commandLine.HasFlag("--replace"));
    if (!bound.valid) {
        throw std::runtime_error(bound.summary.empty() ? "Bind failed." : bound.summary);
    }
    if (!ri::content::SaveNativeSculptDocument(sculptPath, *sculpt)) {
        throw std::runtime_error("Could not save clay: " + sculptPath.string());
    }
    ri::core::LogInfo(bound.summary);
    ri::core::LogInfo("Bound sculpt: " + sculptPath.string());
}

void FloodSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto sculptArg = commandLine.GetValue("--sculpt");
    const auto boneArg = commandLine.GetValue("--bone");
    if (!sculptArg.has_value() || sculptArg->empty() || !boneArg.has_value() || boneArg->empty()) {
        throw std::runtime_error("--sculpt-flood-weights requires --sculpt <path> and --bone <name>.");
    }
    const fs::path sculptPath = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not load clay: " + sculptPath.string());
    }
    const std::size_t changed = ri::scene::FloodNativeSculptWeights(*sculpt, *boneArg);
    if (!ri::content::SaveNativeSculptDocument(sculptPath, *sculpt)) {
        throw std::runtime_error("Could not save clay: " + sculptPath.string());
    }
    ri::core::LogInfo("Flooded " + std::to_string(changed) + " verts onto " + *boneArg);
}

void SoftSculptBoneWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto sculptArg = commandLine.GetValue("--sculpt");
    const auto boneArg = commandLine.GetValue("--bone");
    if (!sculptArg.has_value() || sculptArg->empty() || !boneArg.has_value() || boneArg->empty()) {
        throw std::runtime_error("--sculpt-soft-bone requires --sculpt <path> and --bone <name>.");
    }
    const fs::path sculptPath = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not load clay: " + sculptPath.string());
    }
    const std::size_t changed = ri::scene::SmoothNativeSculptBoneWeights(*sculpt, *boneArg);
    if (!ri::content::SaveNativeSculptDocument(sculptPath, *sculpt)) {
        throw std::runtime_error("Could not save clay: " + sculptPath.string());
    }
    ri::core::LogInfo("Softened " + *boneArg + " on " + std::to_string(changed) + " verts");
}

void CapSculptInfluences(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto sculptArg = commandLine.GetValue("--sculpt");
    if (!sculptArg.has_value() || sculptArg->empty()) {
        throw std::runtime_error("--sculpt-cap4 requires --sculpt <path>.");
    }
    const fs::path sculptPath = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not load clay: " + sculptPath.string());
    }
    const std::size_t changed = ri::scene::LimitNativeSculptInfluences(*sculpt);
    if (!ri::content::SaveNativeSculptDocument(sculptPath, *sculpt)) {
        throw std::runtime_error("Could not save clay: " + sculptPath.string());
    }
    ri::core::LogInfo("Capped influences on " + std::to_string(changed) + " verts");
}

void AuditSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto sculptArg = commandLine.GetValue("--sculpt");
    if (!sculptArg.has_value() || sculptArg->empty()) {
        throw std::runtime_error("--sculpt-audit-weights requires --sculpt <path>.");
    }
    const fs::path sculptPath = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    const auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not load clay: " + sculptPath.string());
    }
    const ri::scene::NativeSculptWeightAudit audit = ri::scene::AuditNativeSculptWeights(*sculpt);
    ri::core::LogInfo("sculpt\t" + sculptPath.string());
    ri::core::LogInfo("verts\t" + std::to_string(audit.vertexCount));
    ri::core::LogInfo("bound\t" + std::to_string(audit.boundCount));
    ri::core::LogInfo("unbound\t" + std::to_string(audit.unboundCount));
    ri::core::LogInfo("blended\t" + std::to_string(audit.blendedCount));
    ri::core::LogInfo("over_cap\t" + std::to_string(audit.overInfluencedCount));
    ri::core::LogInfo("non_normalized\t" + std::to_string(audit.nonNormalizedCount));
}

void UnbindSculptFromRig(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto sculptArg = commandLine.GetValue("--sculpt");
    if (!sculptArg.has_value() || sculptArg->empty()) {
        throw std::runtime_error("--sculpt-unbind requires --sculpt <path>.");
    }
    const fs::path sculptPath = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not load clay: " + sculptPath.string());
    }
    const std::size_t changed = ri::scene::UnbindNativeSculptFromRig(*sculpt);
    if (!ri::content::SaveNativeSculptDocument(sculptPath, *sculpt)) {
        throw std::runtime_error("Could not save clay: " + sculptPath.string());
    }
    ri::core::LogInfo("Unbound sculpt (" + std::to_string(changed) + " verts): " + sculptPath.string());
}

void ClearSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto sculptArg = commandLine.GetValue("--sculpt");
    if (!sculptArg.has_value() || sculptArg->empty()) {
        throw std::runtime_error("--sculpt-clear-weights requires --sculpt <path>.");
    }
    const fs::path sculptPath = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    auto sculpt = ri::content::LoadNativeSculptDocument(sculptPath);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not load clay: " + sculptPath.string());
    }
    const std::size_t changed = ri::scene::ClearNativeSculptWeights(*sculpt);
    if (!ri::content::SaveNativeSculptDocument(sculptPath, *sculpt)) {
        throw std::runtime_error("Could not save clay: " + sculptPath.string());
    }
    ri::core::LogInfo("Cleared weights on " + std::to_string(changed) + " verts");
}

[[nodiscard]] float RequireOptionalFloat(
    const ri::core::CommandLine& commandLine,
    const std::string_view flag,
    const float fallback) {
    const auto value = commandLine.GetValue(flag);
    if (!value.has_value() || value->empty()) {
        return fallback;
    }
    try {
        return std::stof(*value);
    } catch (...) {
        throw std::runtime_error(std::string(flag) + " must be a number.");
    }
}

[[nodiscard]] double RequireOptionalDouble(
    const ri::core::CommandLine& commandLine,
    const std::string_view flag,
    const double fallback) {
    const auto value = commandLine.GetValue(flag);
    if (!value.has_value() || value->empty()) {
        return fallback;
    }
    try {
        return std::stod(*value);
    } catch (...) {
        throw std::runtime_error(std::string(flag) + " must be a number.");
    }
}

struct LoadedSculptEdit {
    fs::path path{};
    ri::content::NativeSculptDocument document{};
};

[[nodiscard]] LoadedSculptEdit LoadSculptForEdit(
    const WorkspaceLayout& workspace,
    const ri::core::CommandLine& commandLine,
    const std::string_view commandName) {
    const auto sculptArg = commandLine.GetValue("--sculpt");
    if (!sculptArg.has_value() || sculptArg->empty()) {
        throw std::runtime_error(std::string(commandName) + " requires --sculpt <path>.");
    }
    LoadedSculptEdit loaded{};
    loaded.path = ResolveAuthoringPath(workspace, fs::path(*sculptArg));
    auto sculpt = ri::content::LoadNativeSculptDocument(loaded.path);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not load clay: " + loaded.path.string());
    }
    loaded.document = std::move(*sculpt);
    return loaded;
}

void SaveSculptEdit(const LoadedSculptEdit& loaded) {
    if (!ri::content::SaveNativeSculptDocument(loaded.path, loaded.document)) {
        throw std::runtime_error("Could not save clay: " + loaded.path.string());
    }
}

[[nodiscard]] std::string RequireBoneName(
    const ri::core::CommandLine& commandLine,
    const std::string_view commandName) {
    const auto boneArg = commandLine.GetValue("--bone");
    if (!boneArg.has_value() || boneArg->empty()) {
        throw std::runtime_error(std::string(commandName) + " requires --bone <name>.");
    }
    return *boneArg;
}

void NormalizeSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-normalize");
    const std::size_t changed = ri::scene::NormalizeAllNativeSculptWeights(loaded.document);
    SaveSculptEdit(loaded);
    ri::core::LogInfo("Normalized weights on " + std::to_string(changed) + " verts");
}

void PruneSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-prune");
    const float minWeight = RequireOptionalFloat(commandLine, "--min", 0.05f);
    const std::size_t changed = ri::scene::PruneAllNativeSculptWeights(loaded.document, minWeight);
    SaveSculptEdit(loaded);
    ri::core::LogInfo(
        "Pruned weights below " + std::to_string(minWeight) + " on " + std::to_string(changed) + " verts");
}

void MirrorSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-mirror");
    const std::string axis = commandLine.GetValue("--axis").value_or("x");
    bool mirrorX = false;
    bool mirrorY = false;
    bool mirrorZ = false;
    if (axis == "x" || axis == "X") {
        mirrorX = true;
    } else if (axis == "y" || axis == "Y") {
        mirrorY = true;
    } else if (axis == "z" || axis == "Z") {
        mirrorZ = true;
    } else {
        throw std::runtime_error("--axis must be x, y, or z.");
    }
    const std::size_t changed =
        ri::scene::MirrorNativeSculptWeights(loaded.document, mirrorX, mirrorY, mirrorZ);
    SaveSculptEdit(loaded);
    ri::core::LogInfo("Mirrored weights on axis " + axis + " for " + std::to_string(changed) + " verts");
}

void SealSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-seal");
    const std::size_t changed = ri::scene::SealUnboundNativeSculptWeightsFromNeighbors(loaded.document);
    SaveSculptEdit(loaded);
    ri::core::LogInfo("Sealed " + std::to_string(changed) + " unbound verts from neighbors");
}

void InvertSculptBoneWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-invert");
    const std::string bone = RequireBoneName(commandLine, "--sculpt-invert");
    const std::size_t changed = ri::scene::InvertNativeSculptBoneWeights(loaded.document, bone);
    SaveSculptEdit(loaded);
    ri::core::LogInfo("Inverted " + bone + " on " + std::to_string(changed) + " verts");
}

void CeilSculptBoneWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-ceil");
    const std::string bone = RequireBoneName(commandLine, "--sculpt-ceil");
    const float maxWeight = RequireOptionalFloat(commandLine, "--max", 0.85f);
    const std::size_t changed =
        ri::scene::CeilNativeSculptBoneWeights(loaded.document, bone, maxWeight);
    SaveSculptEdit(loaded);
    ri::core::LogInfo(
        "Ceiled " + bone + " to " + std::to_string(maxWeight) + " on " + std::to_string(changed)
        + " verts");
}

void HardenSculptBoneWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-harden");
    const std::string bone = RequireBoneName(commandLine, "--sculpt-harden");
    const float threshold = RequireOptionalFloat(commandLine, "--threshold", 0.5f);
    const std::size_t changed =
        ri::scene::HardenNativeSculptBoneWeights(loaded.document, bone, threshold);
    SaveSculptEdit(loaded);
    ri::core::LogInfo(
        "Hardened " + bone + " at " + std::to_string(threshold) + " on " + std::to_string(changed)
        + " verts");
}

void FloodUnboundSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-flood-free");
    const std::string bone = RequireBoneName(commandLine, "--sculpt-flood-free");
    const std::size_t changed = ri::scene::FloodUnboundNativeSculptWeights(loaded.document, bone);
    SaveSculptEdit(loaded);
    ri::core::LogInfo("Flooded " + std::to_string(changed) + " free verts onto " + bone);
}

void FitSculptToRigCommand(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-fit-rig");
    const auto rigArg = commandLine.GetValue("--rig");
    if (!rigArg.has_value() || rigArg->empty()) {
        throw std::runtime_error("--sculpt-fit-rig requires --rig <path>.");
    }
    const fs::path rigPath = ResolveAuthoringPath(workspace, fs::path(*rigArg));
    const auto rig = ri::scene::LoadRigDefinition(rigPath);
    if (!rig.has_value() || !ri::scene::ValidateRigDefinition(*rig).valid) {
        throw std::runtime_error("Invalid rig: " + rigPath.string());
    }
    const float padding = RequireOptionalFloat(commandLine, "--padding", 1.15f);
    if (!FitSculptMeshToRig(loaded.document, *rig, padding)) {
        throw std::runtime_error("Could not fit clay to rig bone bounds.");
    }
    SaveSculptEdit(loaded);
    const SculptBounds bounds = ComputeSculptBounds(loaded.document);
    ri::core::LogInfo("Fitted sculpt to rig bone bounds:");
    ri::core::LogInfo("  Sculpt: " + loaded.path.string());
    ri::core::LogInfo("  Rig: " + rigPath.string());
    ri::core::LogInfo(
        "  Bounds: min=(" + std::to_string(bounds.min.x) + "," + std::to_string(bounds.min.y) + ","
        + std::to_string(bounds.min.z) + ") max=(" + std::to_string(bounds.max.x) + ","
        + std::to_string(bounds.max.y) + "," + std::to_string(bounds.max.z) + ")");
}

void SoftenAllSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-soft");
    const std::size_t changed = ri::scene::SmoothNativeSculptWeights(loaded.document);
    SaveSculptEdit(loaded);
    ri::core::LogInfo("Softened all weights on " + std::to_string(changed) + " verts");
}

void ReportSculptBounds(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-bounds");
    const SculptBounds bounds = ComputeSculptBounds(loaded.document);
    ri::core::LogInfo("sculpt\t" + loaded.path.string());
    ri::core::LogInfo(
        "min\t" + std::to_string(bounds.min.x) + "\t" + std::to_string(bounds.min.y) + "\t"
        + std::to_string(bounds.min.z));
    ri::core::LogInfo(
        "max\t" + std::to_string(bounds.max.x) + "\t" + std::to_string(bounds.max.y) + "\t"
        + std::to_string(bounds.max.z));
    ri::core::LogInfo(
        "center\t" + std::to_string(bounds.center.x) + "\t" + std::to_string(bounds.center.y) + "\t"
        + std::to_string(bounds.center.z));
    ri::core::LogInfo(
        "extent\t" + std::to_string(bounds.extent.x) + "\t" + std::to_string(bounds.extent.y) + "\t"
        + std::to_string(bounds.extent.z));
    ri::core::LogInfo("verts\t" + std::to_string(loaded.document.mesh.positions.size()));
}

void ReportSculptBoneWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-bone-weights");
    const auto weights = [&]() {
        if (const auto boneArg = commandLine.GetValue("--bone");
            boneArg.has_value() && !boneArg->empty()) {
            return std::vector<std::string>{*boneArg};
        }
        std::set<std::string> names{};
        for (const std::string& name : loaded.document.vertexBoneNames) {
            if (!name.empty()) {
                names.insert(name);
            }
        }
        for (const auto& influences : loaded.document.vertexInfluences) {
            for (const auto& influence : influences) {
                if (!influence.boneName.empty()) {
                    names.insert(influence.boneName);
                }
            }
        }
        return std::vector<std::string>(names.begin(), names.end());
    }();
    ri::core::LogInfo("sculpt\t" + loaded.path.string());
    for (const std::string& bone : weights) {
        const std::vector<float> values =
            ri::scene::ExtractNativeSculptBoneWeights(loaded.document, bone);
        float sum = 0.0f;
        std::size_t nonzero = 0U;
        float peak = 0.0f;
        for (const float value : values) {
            if (value > 0.0001f) {
                ++nonzero;
                sum += value;
                peak = std::max(peak, value);
            }
        }
        ri::core::LogInfo(
            "bone\t" + bone + "\tverts=" + std::to_string(nonzero) + "\tsum=" + std::to_string(sum)
            + "\tpeak=" + std::to_string(peak));
    }
}

void TransferSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-transfer");
    const auto fromArg = commandLine.GetValue("--from");
    const auto toArg = commandLine.GetValue("--to");
    if (!fromArg.has_value() || fromArg->empty() || !toArg.has_value() || toArg->empty()) {
        throw std::runtime_error("--sculpt-transfer requires --from <bone> and --to <bone>.");
    }
    const std::size_t changed =
        ri::scene::TransferNativeSculptWeights(loaded.document, *fromArg, *toArg);
    SaveSculptEdit(loaded);
    ri::core::LogInfo(
        "Transferred " + *fromArg + " → " + *toArg + " on " + std::to_string(changed) + " verts");
}

void SwapSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-swap");
    const auto leftArg = commandLine.GetValue("--left");
    const auto rightArg = commandLine.GetValue("--right");
    if (!leftArg.has_value() || leftArg->empty() || !rightArg.has_value() || rightArg->empty()) {
        throw std::runtime_error("--sculpt-swap requires --left <bone> and --right <bone>.");
    }
    const std::size_t changed =
        ri::scene::SwapNativeSculptWeights(loaded.document, *leftArg, *rightArg);
    SaveSculptEdit(loaded);
    ri::core::LogInfo(
        "Swapped " + *leftArg + " ↔ " + *rightArg + " on " + std::to_string(changed) + " verts");
}

void GrowSculptBoneWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-grow");
    const std::string bone = RequireBoneName(commandLine, "--sculpt-grow");
    const std::size_t changed = ri::scene::GrowNativeSculptBoneWeights(loaded.document, bone);
    SaveSculptEdit(loaded);
    ri::core::LogInfo("Grew " + bone + " onto " + std::to_string(changed) + " verts");
}

void ShrinkSculptBoneWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-shrink");
    const std::string bone = RequireBoneName(commandLine, "--sculpt-shrink");
    const std::size_t changed = ri::scene::ShrinkNativeSculptBoneWeights(loaded.document, bone);
    SaveSculptEdit(loaded);
    ri::core::LogInfo("Shrunk " + bone + " from " + std::to_string(changed) + " verts");
}

void PaintSculptWeights(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedSculptEdit loaded = LoadSculptForEdit(workspace, commandLine, "--sculpt-paint");
    const std::string bone = RequireBoneName(commandLine, "--sculpt-paint");
    const auto atArg = commandLine.GetValue("--at");
    const bool atBone = commandLine.HasFlag("--at-bone");
    if ((!atArg.has_value() || atArg->empty()) && !atBone) {
        throw std::runtime_error(
            "--sculpt-paint requires --at <x,y,z> and/or --at-bone (uses bone rest from --rig or sculpt rigPath).");
    }

    ri::math::Vec3 at{};
    if (atBone) {
        fs::path rigPath{};
        if (const auto rigArg = commandLine.GetValue("--rig"); rigArg.has_value() && !rigArg->empty()) {
            rigPath = ResolveAuthoringPath(workspace, fs::path(*rigArg));
        } else if (!loaded.document.rigPath.empty()) {
            rigPath = ResolveClipRigPath(workspace, loaded.document.rigPath, loaded.path);
            if (rigPath.empty()) {
                rigPath = ResolveAuthoringPath(workspace, fs::path(loaded.document.rigPath));
            }
        }
        if (rigPath.empty()) {
            throw std::runtime_error("--at-bone needs --rig <path> or a sculpt already bound to a rig.");
        }
        const auto rig = ri::scene::LoadRigDefinition(rigPath);
        if (!rig.has_value()) {
            throw std::runtime_error("Could not load rig for --at-bone: " + rigPath.string());
        }
        const auto restWorld = ri::scene::RestBoneWorldMatrices(*rig);
        const auto found = restWorld.find(bone);
        if (found == restWorld.end()) {
            throw std::runtime_error("Bone not found on rig for --at-bone: " + bone);
        }
        at = ri::math::ExtractTranslation(found->second);
    } else {
        at = ParseAtVec3(*atArg);
        if (commandLine.HasFlag("--unit")) {
            const SculptBounds bounds = ComputeSculptBounds(loaded.document);
            at = ri::math::Vec3{
                bounds.min.x + bounds.extent.x * std::clamp(at.x, 0.0f, 1.0f),
                bounds.min.y + bounds.extent.y * std::clamp(at.y, 0.0f, 1.0f),
                bounds.min.z + bounds.extent.z * std::clamp(at.z, 0.0f, 1.0f),
            };
        }
    }

    if (commandLine.HasFlag("--snap") || atBone) {
        // Sphere cages leave empty AABB corners; snap to nearest surface vert.
        float best = std::numeric_limits<float>::max();
        ri::math::Vec3 nearest = at;
        for (const ri::math::Vec3& position : loaded.document.mesh.positions) {
            const float dx = position.x - at.x;
            const float dy = position.y - at.y;
            const float dz = position.z - at.z;
            const float distance = dx * dx + dy * dy + dz * dz;
            if (distance < best) {
                best = distance;
                nearest = position;
            }
        }
        at = nearest;
    }

    const SculptBounds bounds = ComputeSculptBounds(loaded.document);
    const float defaultRadius =
        std::max({bounds.extent.x, bounds.extent.y, bounds.extent.z, 0.35f}) * (atBone ? 0.12f : 0.16f);
    const float radius = RequireOptionalFloat(commandLine, "--radius", defaultRadius);
    const float strength = RequireOptionalFloat(commandLine, "--strength", 1.0f);
    const ri::scene::NativeSculptWeightPaint mode =
        ParseWeightPaintMode(commandLine.GetValue("--mode").value_or(atBone ? "assign" : "add"));
    const bool mirrorX = commandLine.HasFlag("--mirror-x") || commandLine.HasFlag("--mirror");
    const bool mirrorY = commandLine.HasFlag("--mirror-y");
    const bool mirrorZ = commandLine.HasFlag("--mirror-z");
    const std::size_t changed = ri::scene::PaintNativeSculptWeights(
        loaded.document, at, radius, bone, mode, mirrorX, mirrorY, mirrorZ, strength);
    SaveSculptEdit(loaded);
    ri::core::LogInfo(
        "Painted " + bone + " (" + commandLine.GetValue("--mode").value_or(atBone ? "assign" : "add")
        + ") at " + std::to_string(at.x) + "," + std::to_string(at.y) + "," + std::to_string(at.z)
        + " r=" + std::to_string(radius) + " on " + std::to_string(changed) + " verts");
}

void ValidateNativeSculptAsset(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    auto pathArg = commandLine.GetValue("--sculpt-validate");
    if (!pathArg.has_value() || pathArg->empty()) {
        pathArg = commandLine.GetValue("--sculpt");
    }
    if (!pathArg.has_value() || pathArg->empty()) {
        throw std::runtime_error(
            "Missing --sculpt-validate <file.ri_sculpt.json> (or --sculpt <path>).");
    }
    const fs::path path = ResolveAuthoringPath(workspace, fs::path(*pathArg));
    const auto sculpt = ri::content::LoadNativeSculptDocument(path);
    if (!sculpt.has_value()) {
        throw std::runtime_error("Could not parse RawIron sculpt: " + path.string());
    }
    const ri::content::NativeSculptValidationReport report =
        ri::content::ValidateNativeSculptDocument(*sculpt);
    ri::core::LogInfo("RawIron sculpt validation:");
    ri::core::LogInfo("  Input: " + path.string());
    ri::core::LogInfo("  Id: " + sculpt->id);
    ri::core::LogInfo("  Cage: " + sculpt->cage);
    ri::core::LogInfo("  Rig: " + (sculpt->rigPath.empty() ? std::string("(unbound)") : sculpt->rigPath));
    ri::core::LogInfo("  Verts: " + std::to_string(report.vertexCount));
    ri::core::LogInfo("  Tris: " + std::to_string(report.triangleCount));
    ri::core::LogInfo("  Unbound: " + std::to_string(report.unboundVertexCount));
    ri::core::LogInfo("  Blended: " + std::to_string(report.blendedVertexCount));
    for (const std::string& warning : report.warnings) {
        ri::core::LogInfo("  Warning: " + warning);
    }
    if (!report.valid) {
        for (const std::string& error : report.errors) {
            ri::core::LogInfo("  Error: " + error);
        }
        throw std::runtime_error("RawIron sculpt validation failed.");
    }
    ri::core::LogInfo("  Result: valid");
}

struct LoadedAnimEdit {
    fs::path path{};
    ri::content::NativeAnimationDocument document{};
};

[[nodiscard]] LoadedAnimEdit LoadAnimForEdit(
    const WorkspaceLayout& workspace,
    const ri::core::CommandLine& commandLine,
    const std::string_view commandName) {
    const auto animArg = commandLine.GetValue("--anim");
    if (!animArg.has_value() || animArg->empty()) {
        throw std::runtime_error(std::string(commandName) + " requires --anim <path>.");
    }
    LoadedAnimEdit loaded{};
    loaded.path = ResolveAuthoringPath(workspace, fs::path(*animArg));
    auto clip = ri::content::LoadNativeAnimationDocument(loaded.path);
    if (!clip.has_value()) {
        throw std::runtime_error("Could not load animation: " + loaded.path.string());
    }
    loaded.document = std::move(*clip);
    return loaded;
}

void SaveAnimEdit(const LoadedAnimEdit& loaded) {
    if (!ri::content::SaveNativeAnimationDocument(loaded.path, loaded.document)) {
        throw std::runtime_error("Could not save animation: " + loaded.path.string());
    }
}

void HoldAnimationPose(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedAnimEdit loaded = LoadAnimForEdit(workspace, commandLine, "--anim-hold");
    const double timeSeconds = RequireOptionalDouble(commandLine, "--time", 0.0);
    const std::string bone = commandLine.GetValue("--bone").value_or("");
    std::size_t changed = 0U;
    if (bone.empty()) {
        changed = ri::scene::HoldNativeAnimationPose(loaded.document, timeSeconds);
    } else if (ri::scene::HoldNativeAnimationKey(loaded.document, bone, timeSeconds)) {
        changed = 1U;
    } else {
        throw std::runtime_error("Hold failed: no earlier key for bone '" + bone + "'.");
    }
    SaveAnimEdit(loaded);
    ri::core::LogInfo("Held " + std::to_string(changed) + " track(s) at " + std::to_string(timeSeconds) + "s");
}

void BreakdownAnimationPose(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedAnimEdit loaded = LoadAnimForEdit(workspace, commandLine, "--anim-breakdown");
    const double timeSeconds = RequireOptionalDouble(commandLine, "--time", 0.0);
    const std::string bone = commandLine.GetValue("--bone").value_or("");
    std::size_t changed = 0U;
    if (bone.empty()) {
        changed = ri::scene::BreakdownNativeAnimationPose(loaded.document, timeSeconds);
    } else if (ri::scene::BreakdownNativeAnimationKey(loaded.document, bone, timeSeconds)) {
        changed = 1U;
    } else {
        throw std::runtime_error(
            "Breakdown failed: playhead must sit strictly between two keys"
            + (bone.empty() ? "." : (" on '" + bone + "'.")));
    }
    SaveAnimEdit(loaded);
    ri::core::LogInfo(
        "Breakdown keyed " + std::to_string(changed) + " track(s) at " + std::to_string(timeSeconds) + "s");
}

void FitAnimationDuration(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedAnimEdit loaded = LoadAnimForEdit(workspace, commandLine, "--anim-fit-duration");
    if (!ri::scene::FitNativeAnimationDuration(loaded.document)) {
        throw std::runtime_error("Fit duration failed (clip may have no keys).");
    }
    SaveAnimEdit(loaded);
    ri::core::LogInfo("Fitted duration to " + std::to_string(loaded.document.durationSeconds) + "s");
}

void AlignAnimationStart(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedAnimEdit loaded = LoadAnimForEdit(workspace, commandLine, "--anim-align-start");
    if (!ri::scene::AlignNativeAnimationStart(loaded.document)) {
        throw std::runtime_error("Align start failed (clip may have no keys).");
    }
    SaveAnimEdit(loaded);
    ri::core::LogInfo("Aligned clip start to 0s; duration " + std::to_string(loaded.document.durationSeconds) + "s");
}

void TrimAnimationClip(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedAnimEdit loaded = LoadAnimForEdit(workspace, commandLine, "--anim-trim");
    const auto startArg = commandLine.GetValue("--start");
    const auto endArg = commandLine.GetValue("--end");
    if (!startArg.has_value() || startArg->empty() || !endArg.has_value() || endArg->empty()) {
        throw std::runtime_error("--anim-trim requires --start <seconds> and --end <seconds>.");
    }
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    try {
        startSeconds = std::stod(*startArg);
        endSeconds = std::stod(*endArg);
    } catch (...) {
        throw std::runtime_error("--start and --end must be numbers.");
    }
    const bool shiftToZero = !commandLine.HasFlag("--no-shift");
    const ri::scene::NativeAnimationTrimResult trim =
        ri::scene::TrimNativeAnimation(loaded.document, startSeconds, endSeconds, shiftToZero);
    if (!trim.valid) {
        throw std::runtime_error(trim.summary.empty() ? "Trim failed." : trim.summary);
    }
    SaveAnimEdit(loaded);
    ri::core::LogInfo(trim.summary);
    ri::core::LogInfo(
        "Removed keys=" + std::to_string(trim.removedKeys) + " events="
        + std::to_string(trim.removedEvents));
}

void ReportAnimationClip(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto pathArg = commandLine.GetValue("--anim-report");
    if (!pathArg.has_value() || pathArg->empty()) {
        throw std::runtime_error("Missing --anim-report <file.ri_anim.json>.");
    }
    const fs::path path = ResolveAuthoringPath(workspace, fs::path(*pathArg));
    const auto clip = ri::content::LoadNativeAnimationDocument(path);
    if (!clip.has_value()) {
        throw std::runtime_error("Could not load animation: " + path.string());
    }
    const ri::content::NativeAnimationValidationReport report =
        ri::content::ValidateNativeAnimationDocument(*clip);
    ri::core::LogInfo("anim\t" + path.string());
    ri::core::LogInfo("id\t" + clip->id);
    ri::core::LogInfo("rig\t" + clip->rigPath);
    ri::core::LogInfo("duration\t" + std::to_string(clip->durationSeconds));
    ri::core::LogInfo("tracks\t" + std::to_string(report.trackCount));
    ri::core::LogInfo("keys\t" + std::to_string(report.keyCount));
    ri::core::LogInfo("events\t" + std::to_string(report.eventCount));
    ri::core::LogInfo("valid\t" + std::string(report.valid ? "yes" : "no"));
    for (const auto& track : clip->tracks) {
        ri::core::LogInfo("track\t" + track.boneName + "\t" + std::to_string(track.keys.size()));
    }
}

void UpsertAnimationEvent(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedAnimEdit loaded = LoadAnimForEdit(workspace, commandLine, "--anim-event");
    const auto nameArg = commandLine.GetValue("--name");
    if (!nameArg.has_value() || nameArg->empty()) {
        throw std::runtime_error("--anim-event requires --name <marker>.");
    }
    const double timeSeconds = RequireOptionalDouble(commandLine, "--time", 0.0);
    if (!ri::scene::UpsertNativeAnimationEvent(loaded.document, timeSeconds, *nameArg)) {
        throw std::runtime_error("Could not upsert animation event '" + *nameArg + "'.");
    }
    SaveAnimEdit(loaded);
    ri::core::LogInfo(
        "Event '" + *nameArg + "' at " + std::to_string(timeSeconds) + "s (events="
        + std::to_string(loaded.document.events.size()) + ")");
}

void UpsertAnimationKey(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    LoadedAnimEdit loaded = LoadAnimForEdit(workspace, commandLine, "--anim-key");
    const std::string bone = RequireBoneName(commandLine, "--anim-key");
    const double timeSeconds = RequireOptionalDouble(commandLine, "--time", 0.0);
    const bool fromPrevious = commandLine.HasFlag("--from-previous");

    ri::scene::Transform transform{};
    bool haveBase = false;

    // Default: deltas are relative to rest pose so --rx 25 means "25° from rest", not compounded.
    const fs::path rigPath = ResolveClipRigPath(workspace, loaded.document.rigPath, loaded.path);
    if (!rigPath.empty()) {
        if (const auto rig = ri::scene::LoadRigDefinition(rigPath); rig.has_value()) {
            for (const auto& rigBone : rig->bones) {
                if (rigBone.name != bone) {
                    continue;
                }
                transform = rigBone.restLocal;
                haveBase = true;
                break;
            }
        }
    }

    if (fromPrevious || !haveBase) {
        for (const auto& track : loaded.document.tracks) {
            if (track.boneName != bone) {
                continue;
            }
            std::optional<ri::content::NativeAnimationKeyframe> base{};
            for (const auto& key : track.keys) {
                if (key.timeSeconds <= timeSeconds + 1.0e-6) {
                    if (!base.has_value() || key.timeSeconds >= base->timeSeconds) {
                        base = key;
                    }
                }
            }
            if (!base.has_value() && !track.keys.empty()) {
                base = track.keys.front();
            }
            if (base.has_value()) {
                transform.position = {base->translation.x, base->translation.y, base->translation.z};
                transform.rotationDegrees = {
                    base->rotationDegrees.x, base->rotationDegrees.y, base->rotationDegrees.z};
                transform.scale = {base->scale.x, base->scale.y, base->scale.z};
                haveBase = true;
            }
            break;
        }
    }
    if (!haveBase) {
        transform.scale = {1.0f, 1.0f, 1.0f};
    }

    transform.position.x += RequireOptionalFloat(commandLine, "--tx", 0.0f);
    transform.position.y += RequireOptionalFloat(commandLine, "--ty", 0.0f);
    transform.position.z += RequireOptionalFloat(commandLine, "--tz", 0.0f);
    transform.rotationDegrees.x += RequireOptionalFloat(commandLine, "--rx", 0.0f);
    transform.rotationDegrees.y += RequireOptionalFloat(commandLine, "--ry", 0.0f);
    transform.rotationDegrees.z += RequireOptionalFloat(commandLine, "--rz", 0.0f);
    if (commandLine.GetValue("--sx").has_value()) {
        transform.scale.x = RequireOptionalFloat(commandLine, "--sx", transform.scale.x);
    }
    if (commandLine.GetValue("--sy").has_value()) {
        transform.scale.y = RequireOptionalFloat(commandLine, "--sy", transform.scale.y);
    }
    if (commandLine.GetValue("--sz").has_value()) {
        transform.scale.z = RequireOptionalFloat(commandLine, "--sz", transform.scale.z);
    }

    ri::scene::UpsertNativeAnimationKey(loaded.document, bone, timeSeconds, transform);
    if (timeSeconds > loaded.document.durationSeconds) {
        loaded.document.durationSeconds = timeSeconds;
    }
    SaveAnimEdit(loaded);
    ri::core::LogInfo(
        "Keyed " + bone + " at " + std::to_string(timeSeconds) + "s"
        + " t=(" + std::to_string(transform.position.x) + "," + std::to_string(transform.position.y)
        + "," + std::to_string(transform.position.z) + ")"
        + " r=(" + std::to_string(transform.rotationDegrees.x) + ","
        + std::to_string(transform.rotationDegrees.y) + ","
        + std::to_string(transform.rotationDegrees.z) + ")");
}

void KeyAnimationRest(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto animArg = commandLine.GetValue("--anim");
    if (!animArg.has_value() || animArg->empty()) {
        throw std::runtime_error("--anim-key-rest requires --anim <path>.");
    }
    const fs::path animPath = ResolveAuthoringPath(workspace, fs::path(*animArg));
    auto clip = ri::content::LoadNativeAnimationDocument(animPath);
    if (!clip.has_value()) {
        throw std::runtime_error("Could not load animation: " + animPath.string());
    }
    const fs::path rigPath = ResolveClipRigPath(workspace, clip->rigPath, animPath);
    if (rigPath.empty()) {
        throw std::runtime_error("Clip has no resolvable rig path: " + clip->rigPath);
    }
    const auto rig = ri::scene::LoadRigDefinition(rigPath);
    if (!rig.has_value()) {
        throw std::runtime_error("Could not load rig: " + rigPath.string());
    }
    double timeSeconds = 0.0;
    if (const auto typed = commandLine.GetValue("--time"); typed.has_value() && !typed->empty()) {
        try {
            timeSeconds = std::stod(*typed);
        } catch (...) {
            throw std::runtime_error("--time must be a number.");
        }
    }
    const std::string bone = commandLine.GetValue("--bone").value_or("");
    const std::size_t changed =
        ri::scene::InsertNativeAnimationRestKeys(*clip, *rig, timeSeconds, bone);
    if (!ri::content::SaveNativeAnimationDocument(animPath, *clip)) {
        throw std::runtime_error("Could not save animation: " + animPath.string());
    }
    ri::core::LogInfo(
        "Rest-keyed " + std::to_string(changed) + " bones at " + std::to_string(timeSeconds) + "s");
}

void ValidateNativeAnimationAsset(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto pathArg = commandLine.GetValue("--anim-validate");
    if (!pathArg.has_value() || pathArg->empty()) {
        throw std::runtime_error("Missing --anim-validate <file.ri_anim.json>.");
    }
    const fs::path path = ResolveAuthoringPath(workspace, fs::path(*pathArg));
    const auto clip = ri::content::LoadNativeAnimationDocument(path);
    if (!clip.has_value()) {
        throw std::runtime_error("Could not parse RawIron animation: " + path.string());
    }
    const ri::content::NativeAnimationValidationReport report =
        ri::content::ValidateNativeAnimationDocument(*clip);
    ri::core::LogInfo("RawIron animation validation:");
    ri::core::LogInfo("  Input: " + path.string());
    ri::core::LogInfo("  Id: " + clip->id);
    ri::core::LogInfo("  Rig: " + clip->rigPath);
    ri::core::LogInfo("  Tracks: " + std::to_string(report.trackCount));
    ri::core::LogInfo("  Keys: " + std::to_string(report.keyCount));
    for (const std::string& warning : report.warnings) {
        ri::core::LogInfo("  Warning: " + warning);
    }
    if (!report.valid) {
        for (const std::string& error : report.errors) {
            ri::core::LogInfo("  Error: " + error);
        }
        throw std::runtime_error("RawIron animation validation failed.");
    }
    ri::core::LogInfo("  Result: valid");
}

fs::path ResolveProjectRootOption(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (const auto projectArg = commandLine.GetValue("--project"); projectArg.has_value() && !projectArg->empty()) {
        return fs::weakly_canonical(fs::path(*projectArg));
    }
    return workspace.root;
}

std::vector<std::string> SplitPackageOptionList(const std::string_view value) {
    std::vector<std::string> tokens;
    std::size_t cursor = 0U;
    while (cursor < value.size()) {
        const std::size_t delimiter = value.find_first_of(",;", cursor);
        const std::size_t end = delimiter == std::string_view::npos ? value.size() : delimiter;
        std::string token(value.substr(cursor, end - cursor));
        token.erase(token.begin(), std::find_if(token.begin(), token.end(), [](const unsigned char ch) {
            return std::isspace(ch) == 0;
        }));
        token.erase(std::find_if(token.rbegin(), token.rend(), [](const unsigned char ch) {
            return std::isspace(ch) == 0;
        }).base(), token.end());
        if (!token.empty()) {
            tokens.push_back(std::move(token));
        }
        if (delimiter == std::string_view::npos) {
            break;
        }
        cursor = delimiter + 1U;
    }
    return tokens;
}

std::vector<ri::content::InstalledAssetPackage> DiscoverWorkspacePackageCatalog(
    const WorkspaceLayout& workspace,
    const fs::path& projectRoot) {
    std::vector<fs::path> manifestPaths =
        ri::content::FindAssetPackageManifestPaths(projectRoot);
    const std::vector<fs::path> workspacePackagePaths =
        ri::content::FindAssetPackageManifestPaths(workspace.root / "Assets");
    manifestPaths.insert(
        manifestPaths.end(),
        workspacePackagePaths.begin(),
        workspacePackagePaths.end());

    std::set<std::string, std::less<>> loadedIdentities;
    std::vector<ri::content::InstalledAssetPackage> catalog;
    for (const fs::path& manifestPath : manifestPaths) {
        const std::optional<ri::content::AssetPackageManifest> manifest =
            ri::content::LoadAssetPackageManifest(manifestPath);
        if (!manifest.has_value()) {
            ri::core::LogInfo("  Skipped unreadable package manifest: " + manifestPath.string());
            continue;
        }
        const std::string identity = manifest->packageId + "@" + manifest->packageVersion;
        if (!loadedIdentities.insert(identity).second) {
            continue;
        }
        ri::content::InstalledAssetPackage package{};
        package.manifestPath = manifestPath;
        package.packageRoot = manifestPath.parent_path();
        package.manifest = *manifest;
        catalog.push_back(std::move(package));
    }
    return catalog;
}

ri::content::PackageResolverOptions PackageOptionsFromCommandLine(
    const ri::core::CommandLine& commandLine) {
    ri::content::PackageResolverOptions options{};
    options.engineApiVersion = commandLine.GetValue("--engine-api").value_or("1.0.0");
#if defined(_WIN32)
    options.platform = commandLine.GetValue("--platform").value_or("windows-x64");
#elif defined(__linux__)
    options.platform = commandLine.GetValue("--platform").value_or("linux-x64");
#else
    options.platform = commandLine.GetValue("--platform").value_or("unknown");
#endif
    if (const std::optional<std::string> capabilities = commandLine.GetValue("--capabilities")) {
        options.engineCapabilities = SplitPackageOptionList(*capabilities);
    }
    if (const std::optional<std::string> permissions = commandLine.GetValue("--grant-permissions")) {
        options.grantedPermissions = SplitPackageOptionList(*permissions);
    }
    options.enforcePermissions = !commandLine.HasFlag("--ignore-package-permissions");
    options.includeOptionalDependencies = commandLine.HasFlag("--include-optional-packages");
    return options;
}

void ResolveAssetPackageGraph(
    const WorkspaceLayout& workspace,
    const ri::core::CommandLine& commandLine) {
    const std::optional<std::string> rootId = commandLine.GetValue("--asset-package-resolve");
    if (!rootId.has_value() || rootId->empty()) {
        throw std::runtime_error("Missing --asset-package-resolve <package-id>.");
    }
    const fs::path projectRoot = ResolveProjectRootOption(workspace, commandLine);
    const std::vector<ri::content::InstalledAssetPackage> installedCatalog =
        DiscoverWorkspacePackageCatalog(workspace, projectRoot);
    std::vector<ri::content::AssetPackageManifest> catalog;
    catalog.reserve(installedCatalog.size());
    for (const ri::content::InstalledAssetPackage& package : installedCatalog) {
        catalog.push_back(package.manifest);
    }

    const std::vector<ri::content::PackageRequest> roots{{
        .packageId = *rootId,
        .versionRequirement = commandLine.GetValue("--package-version").value_or("*"),
    }};
    const ri::content::PackageResolutionResult resolution =
        ri::content::ResolvePackages(catalog, roots, PackageOptionsFromCommandLine(commandLine));
    ri::core::LogInfo("RawIron package resolution:");
    ri::core::LogInfo("  Root: " + *rootId + " " + roots.front().versionRequirement);
    ri::core::LogInfo("  Catalog packages: " + std::to_string(catalog.size()));
    if (!resolution.resolved) {
        for (const std::string& issue : resolution.issues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error("RawIron package dependency resolution failed.");
    }
    ri::core::LogInfo("  Activation order:");
    for (std::size_t index = 0U; index < resolution.loadOrder.size(); ++index) {
        const ri::content::ResolvedPackage& package = resolution.loadOrder[index];
        ri::core::LogInfo(
            "    " + std::to_string(index + 1U) + ". "
            + package.packageId + "@" + package.packageVersion);
    }
}

void CheckAssetPackageMount(
    const WorkspaceLayout& workspace,
    const ri::core::CommandLine& commandLine) {
    const std::optional<std::string> rootId =
        commandLine.GetValue("--asset-package-mount-check");
    if (!rootId.has_value() || rootId->empty()) {
        throw std::runtime_error("Missing --asset-package-mount-check <package-id>.");
    }
    const fs::path projectRoot = ResolveProjectRootOption(workspace, commandLine);
    const std::vector<ri::content::InstalledAssetPackage> catalog =
        DiscoverWorkspacePackageCatalog(workspace, projectRoot);
    const std::vector<ri::content::PackageRequest> roots{{
        .packageId = *rootId,
        .versionRequirement = commandLine.GetValue("--package-version").value_or("*"),
    }};
    ri::content::PackageMountRegistry registry;
    const ri::content::PackageActivationResult activation = registry.Activate(
        catalog,
        roots,
        PackageOptionsFromCommandLine(commandLine));

    ri::core::LogInfo("RawIron package mount check:");
    ri::core::LogInfo("  Root: " + *rootId + " " + roots.front().versionRequirement);
    ri::core::LogInfo("  Catalog packages: " + std::to_string(catalog.size()));
    if (!activation.activated) {
        for (const std::string& issue : activation.issues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error("RawIron package mount check failed.");
    }
    ri::core::LogInfo("  Activation: " + std::to_string(activation.activationId));
    ri::core::LogInfo("  Live mounts:");
    const std::vector<ri::content::MountedPackageInfo> mounts = registry.MountedPackages();
    for (std::size_t index = 0U; index < mounts.size(); ++index) {
        const ri::content::MountedPackageInfo& mount = mounts[index];
        ri::core::LogInfo(
            "    " + std::to_string(index + 1U) + ". "
            + mount.packageId + "@" + mount.packageVersion
            + " -> " + mount.mountPoint
            + " [" + mount.packageRoot.string() + "]");
    }
}

std::optional<ri::content::InstalledAssetPackage> LoadValidatedPackageForInstall(const fs::path& packageRoot) {
    const fs::path manifestPath = ResolvePackageManifestPath(packageRoot);
    const std::optional<ri::content::AssetPackageManifest> manifest =
        ri::content::LoadAssetPackageManifest(manifestPath);
    if (!manifest.has_value()) {
        return std::nullopt;
    }

    ri::content::InstalledAssetPackage package{};
    package.manifestPath = manifestPath;
    package.packageRoot = manifestPath.parent_path();
    package.manifest = *manifest;
    package.validation = ri::content::ValidateAssetPackageManifest(package.manifest, package.packageRoot);
    return package;
}

void CopyFileChecked(const fs::path& source, const fs::path& destination) {
    EnsureParentDirectoryExists(destination);
    std::error_code ec{};
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        throw std::runtime_error("Failed to copy " + source.string() + " to " + destination.string() + ": " + ec.message());
    }
}

struct PackageInstallCopyPlan {
    fs::path source{};
    std::string relativeDestination{};
    fs::path resolvedDestination{};
    std::string label{};
};

struct PackageInstallPromotionRecord {
    fs::path destination{};
    fs::path backupPath{};
    bool createdNew = false;
};

class ExclusivePackageInstallStaging final {
public:
    ExclusivePackageInstallStaging() = default;
    ~ExclusivePackageInstallStaging() noexcept { Cleanup(); }

    ExclusivePackageInstallStaging(const ExclusivePackageInstallStaging&) = delete;
    ExclusivePackageInstallStaging& operator=(const ExclusivePackageInstallStaging&) = delete;

    ExclusivePackageInstallStaging(ExclusivePackageInstallStaging&& other) noexcept
        : root_(std::move(other.root_)) {
        other.root_.clear();
    }

    ExclusivePackageInstallStaging& operator=(ExclusivePackageInstallStaging&& other) noexcept {
        if (this != &other) {
            Cleanup();
            root_ = std::move(other.root_);
            other.root_.clear();
        }
        return *this;
    }

    [[nodiscard]] const fs::path& Root() const noexcept { return root_; }
    [[nodiscard]] explicit operator bool() const noexcept { return !root_.empty(); }

    static ExclusivePackageInstallStaging Create() {
        std::error_code error;
        const fs::path temporaryRoot = fs::canonical(fs::temp_directory_path(), error);
        if (error || !fs::is_directory(temporaryRoot)) {
            throw std::runtime_error(
                "Could not resolve the system temporary directory for package install staging: "
                + error.message());
        }

        static std::atomic<std::uint64_t> counter{0U};
        std::random_device random;
#if defined(_WIN32)
        const auto processId = static_cast<std::uint64_t>(_getpid());
#else
        const auto processId = static_cast<std::uint64_t>(getpid());
#endif
        for (std::size_t attempt = 0U; attempt < 128U; ++attempt) {
            std::ostringstream suffix;
            suffix << "RawIronPackageInstall." << processId << '.'
                   << std::hex << std::setw(8) << std::setfill('0') << random()
                   << std::setw(8) << random() << '.'
                   << counter.fetch_add(1U, std::memory_order_relaxed);
            const fs::path candidate = temporaryRoot / suffix.str();
            error.clear();
            if (fs::create_directory(candidate, error)) {
                const fs::path canonicalCandidate = fs::canonical(candidate, error);
                if (error || canonicalCandidate.parent_path() != temporaryRoot) {
                    std::error_code cleanupError;
                    fs::remove(candidate, cleanupError);
                    throw std::runtime_error(
                        "Exclusive package-install staging root failed containment validation.");
                }
                return ExclusivePackageInstallStaging(canonicalCandidate);
            }
            if (error && error != std::errc::file_exists) {
                throw std::runtime_error(
                    "Could not create exclusive package-install staging root: " + error.message());
            }
        }
        throw std::runtime_error(
            "Could not allocate a unique package-install staging root after 128 attempts.");
    }

private:
    explicit ExclusivePackageInstallStaging(fs::path root) : root_(std::move(root)) {}

    void Cleanup() noexcept {
        if (root_.empty()) {
            return;
        }
        std::error_code ignored;
        fs::remove_all(root_, ignored);
        root_.clear();
    }

    fs::path root_{};
};

ri::content::PackageInstallPathResolution ResolvePackageInstallPathOrThrow(
    const fs::path& projectRoot,
    const std::string_view relativeDestination,
    const std::string_view label) {
    ri::content::PackageInstallPathResolution resolution =
        ri::content::ResolvePackageInstallPath(projectRoot, relativeDestination);
    if (!resolution.safe) {
        throw std::runtime_error(
            "Unsafe RawIron package destination for " + std::string(label) + " ('"
            + std::string(relativeDestination) + "'): " + resolution.issue);
    }
    return resolution;
}

[[nodiscard]] bool IsExclusiveCreateCollision(const std::system_error& error) {
#if defined(_WIN32)
    const int value = error.code().value();
    return value == ERROR_FILE_EXISTS || value == ERROR_ALREADY_EXISTS;
#else
    return error.code().value() == EEXIST;
#endif
}

[[nodiscard]] fs::path MakePackagePromoteTempPath(
    const fs::path& destination,
    const std::uint64_t sequence) {
    const std::uint64_t stamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#if defined(_WIN32)
    const unsigned long pid = GetCurrentProcessId();
#else
    const long pid = static_cast<long>(::getpid());
#endif
    fs::path temporary = destination;
    temporary += ".promote-tmp." + std::to_string(pid) + "." + std::to_string(stamp) + "."
        + std::to_string(sequence);
    return temporary;
}

void AssertPackageOverwriteDestinationReplaceable(const fs::path& destination) {
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(destination.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        throw std::system_error(
            static_cast<int>(GetLastError()),
            std::system_category(),
            "package install destination attributes cannot be inspected");
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        throw std::runtime_error(
            "package install destination is a directory: " + destination.string());
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
        throw std::runtime_error(
            "package install destination is a reparse point: " + destination.string());
    }
    if ((attributes & FILE_ATTRIBUTE_READONLY) != 0U) {
        throw std::runtime_error(
            "package install destination is not writable: " + destination.string());
    }
#else
    std::error_code statusError;
    const fs::file_status linkStatus = fs::symlink_status(destination, statusError);
    if (statusError) {
        throw std::runtime_error(
            "package install destination cannot be inspected: " + statusError.message());
    }
    if (fs::is_symlink(linkStatus) || fs::is_directory(linkStatus)) {
        throw std::runtime_error(
            "package install destination is not a replaceable regular file: " + destination.string());
    }
    int probeFlags = O_WRONLY;
#if defined(O_NOFOLLOW)
    probeFlags |= O_NOFOLLOW;
#endif
    const int probe = ::open(destination.c_str(), probeFlags);
    if (probe < 0) {
        throw std::system_error(
            errno, std::generic_category(), "package install destination is not writable");
    }
    ::close(probe);
#endif
}

void CopyFileExclusiveCreate(const fs::path& source, const fs::path& destination) {
#if defined(_WIN32)
    const HANDLE handle = CreateFileW(
        destination.c_str(),
        GENERIC_WRITE,
        0U,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        throw std::system_error(
            static_cast<int>(GetLastError()),
            std::system_category(),
            "exclusive package install destination creation failed");
    }
    bool committed = false;
    try {
        std::ifstream input(source, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Failed to open staged package source: " + source.string());
        }
        std::array<char, 64U * 1024U> buffer{};
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize got = input.gcount();
            if (got <= 0) {
                break;
            }
            DWORD written = 0U;
            if (!WriteFile(
                    handle,
                    buffer.data(),
                    static_cast<DWORD>(got),
                    &written,
                    nullptr)
                || written != static_cast<DWORD>(got)) {
                throw std::system_error(
                    static_cast<int>(GetLastError()),
                    std::system_category(),
                    "exclusive package install write failed");
            }
        }
        if (!input && !input.eof()) {
            throw std::runtime_error("Failed while reading staged package source: " + source.string());
        }
        if (!FlushFileBuffers(handle)) {
            throw std::system_error(
                static_cast<int>(GetLastError()),
                std::system_category(),
                "exclusive package install flush failed");
        }
        committed = true;
    } catch (...) {
        CloseHandle(handle);
        if (!committed) {
            std::error_code ignored;
            fs::remove(destination, ignored);
        }
        throw;
    }
    CloseHandle(handle);
#else
    const int descriptor = open(destination.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        throw std::system_error(errno, std::generic_category(),
                                "exclusive package install destination creation failed");
    }
    bool committed = false;
    try {
        std::ifstream input(source, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Failed to open staged package source: " + source.string());
        }
        std::array<char, 64U * 1024U> buffer{};
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize got = input.gcount();
            if (got <= 0) {
                break;
            }
            std::streamsize offset = 0;
            while (offset < got) {
                const ssize_t written = write(
                    descriptor, buffer.data() + offset, static_cast<std::size_t>(got - offset));
                if (written < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    throw std::system_error(errno, std::generic_category(),
                                            "exclusive package install write failed");
                }
                if (written == 0) {
                    throw std::runtime_error("exclusive package install write made no progress");
                }
                offset += written;
            }
        }
        if (!input && !input.eof()) {
            throw std::runtime_error("Failed while reading staged package source: " + source.string());
        }
        if (fsync(descriptor) != 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "exclusive package install flush failed");
        }
        committed = true;
    } catch (...) {
        close(descriptor);
        if (!committed) {
            std::error_code ignored;
            fs::remove(destination, ignored);
        }
        throw;
    }
    close(descriptor);
#endif
}

void ReplaceExistingFileFromStaged(const fs::path& source, const fs::path& destination) {
    AssertPackageOverwriteDestinationReplaceable(destination);

    static std::atomic<std::uint64_t> sequence{0U};
    constexpr std::size_t kCollisionRetries = 8U;
    for (std::size_t attempt = 0U; attempt < kCollisionRetries; ++attempt) {
        const fs::path temporary = MakePackagePromoteTempPath(
            destination, sequence.fetch_add(1U, std::memory_order_relaxed));
        try {
            CopyFileExclusiveCreate(source, temporary);
        } catch (const std::system_error& error) {
            if (IsExclusiveCreateCollision(error)) {
                continue;
            }
            throw;
        }

        try {
#if defined(_WIN32)
            if (MoveFileExW(
                    temporary.c_str(),
                    destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
                == FALSE) {
                throw std::system_error(
                    static_cast<int>(GetLastError()),
                    std::system_category(),
                    "package install destination replace failed");
            }
#else
            if (::rename(temporary.c_str(), destination.c_str()) != 0) {
                throw std::system_error(
                    errno, std::generic_category(), "package install destination replace failed");
            }
#endif
            return;
        } catch (...) {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            throw;
        }
    }
    throw std::runtime_error(
        "Failed to allocate an exclusive package promote temp beside "
        + destination.string());
}

void RollbackPackageInstallPromotions(const std::vector<PackageInstallPromotionRecord>& promotions) {
    for (auto it = promotions.rbegin(); it != promotions.rend(); ++it) {
        std::error_code error;
        if (it->createdNew) {
            fs::remove(it->destination, error);
            continue;
        }
        if (it->backupPath.empty() || !fs::is_regular_file(it->backupPath, error)) {
            continue;
        }
        try {
            ReplaceExistingFileFromStaged(it->backupPath, it->destination);
        } catch (...) {
            error.clear();
            fs::copy_file(
                it->backupPath, it->destination, fs::copy_options::overwrite_existing, error);
        }
    }
}

void PromotePackageInstallTransactionally(
    const fs::path& projectRoot,
    const fs::path& stagingRoot,
    const std::vector<PackageInstallCopyPlan>& copyPlan) {
    const fs::path stagedFilesRoot = stagingRoot / "files";
    const fs::path backupRoot = stagingRoot / "backups";
    std::error_code error;
    fs::create_directories(stagedFilesRoot, error);
    if (error) {
        throw std::runtime_error("Failed to create package install stage directory: " + error.message());
    }
    fs::create_directories(backupRoot, error);
    if (error) {
        throw std::runtime_error("Failed to create package install backup directory: " + error.message());
    }

    // Stage every payload under the exclusive temp root before the first project mutation.
    for (const PackageInstallCopyPlan& plan : copyPlan) {
        const fs::path stagedDestination = stagedFilesRoot / fs::path(plan.relativeDestination);
        EnsureParentDirectoryExists(stagedDestination);
        error.clear();
        fs::copy_file(plan.source, stagedDestination, fs::copy_options::overwrite_existing, error);
        if (error) {
            throw std::runtime_error(
                "Failed to stage " + plan.label + " (" + plan.source.string() + "): " + error.message());
        }
    }

    std::vector<PackageInstallPromotionRecord> promotions;
    promotions.reserve(copyPlan.size());
    try {
        for (const PackageInstallCopyPlan& plan : copyPlan) {
            const ri::content::PackageInstallPathResolution beforeDirectories =
                ResolvePackageInstallPathOrThrow(projectRoot, plan.relativeDestination, plan.label);
            if (beforeDirectories.destination != plan.resolvedDestination) {
                throw std::runtime_error(
                    "RawIron package destination changed after preflight for " + plan.label
                    + "; install aborted.");
            }

            EnsureParentDirectoryExists(beforeDirectories.destination);

            const ri::content::PackageInstallPathResolution beforeCopy =
                ResolvePackageInstallPathOrThrow(projectRoot, plan.relativeDestination, plan.label);
            if (beforeCopy.destination != plan.resolvedDestination) {
                throw std::runtime_error(
                    "RawIron package destination changed while preparing " + plan.label
                    + "; install aborted.");
            }

            const fs::path stagedSource = stagedFilesRoot / fs::path(plan.relativeDestination);
            PackageInstallPromotionRecord record{
                .destination = beforeCopy.destination,
                .backupPath = {},
                .createdNew = false,
            };

            error.clear();
            const fs::file_status destinationLinkStatus =
                fs::symlink_status(beforeCopy.destination, error);
            const bool missingDestination = destinationLinkStatus.type() == fs::file_type::not_found
                || error == std::errc::no_such_file_or_directory;
            if (error && !missingDestination) {
                throw std::runtime_error(
                    "Failed to inspect package install destination for " + plan.label + ": "
                    + error.message());
            }
            if (!missingDestination) {
#if defined(_WIN32)
                const DWORD destinationAttributes = GetFileAttributesW(beforeCopy.destination.c_str());
                if (destinationAttributes == INVALID_FILE_ATTRIBUTES) {
                    throw std::system_error(
                        static_cast<int>(GetLastError()),
                        std::system_category(),
                        "package install destination attributes cannot be inspected");
                }
                if ((destinationAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
                    || (destinationAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
                    throw std::runtime_error(
                        "Refusing to overwrite a reparse or directory package destination for "
                        + plan.label);
                }
#endif
                if (fs::is_symlink(destinationLinkStatus) || !fs::is_regular_file(destinationLinkStatus)) {
                    throw std::runtime_error(
                        "Refusing to overwrite a non-regular package destination for " + plan.label);
                }
                record.backupPath = backupRoot / fs::path(plan.relativeDestination);
                EnsureParentDirectoryExists(record.backupPath);
                error.clear();
                fs::copy_file(
                    beforeCopy.destination,
                    record.backupPath,
                    fs::copy_options::overwrite_existing,
                    error);
                if (error) {
                    throw std::runtime_error(
                        "Failed to backup existing package destination for " + plan.label + ": "
                        + error.message());
                }
                record.createdNew = false;
                promotions.push_back(record);
                ReplaceExistingFileFromStaged(stagedSource, beforeCopy.destination);
            } else {
                record.createdNew = true;
                CopyFileExclusiveCreate(stagedSource, beforeCopy.destination);
                promotions.push_back(record);
            }
        }
    } catch (...) {
        RollbackPackageInstallPromotions(promotions);
        throw;
    }
}

fs::path DefaultProjectInstallPath(const ri::content::AssetPackageManifest& manifest,
                                   const ri::content::AssetPackageEntry& asset) {
    fs::path relativeAssetPath = fs::path(asset.path).lexically_normal();
    if (!relativeAssetPath.empty() && *relativeAssetPath.begin() == "assets") {
        relativeAssetPath = relativeAssetPath.lexically_relative("assets");
    }

    const std::string type = ToLowerAscii(asset.type);
    if (type == "script" || type == "behavior") {
        return fs::path("scripts") / "packages" / manifest.packageId / relativeAssetPath;
    }
    if (type == "scene") {
        return fs::path("levels") / "packages" / manifest.packageId / relativeAssetPath;
    }
    return fs::path("assets") / "packages" / manifest.packageId / relativeAssetPath;
}

void ImportAssetPackage(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto packageArg = commandLine.GetValue("--asset-package-import");
    if (!packageArg.has_value() || packageArg->empty()) {
        throw std::runtime_error("Missing --asset-package-import <package-dir-or-manifest>.");
    }
    std::optional<ri::tooling::SecureRipakExtraction> extraction;
    fs::path packageRoot = fs::path(*packageArg);
    if (IsRipakArchivePath(packageRoot)) {
        extraction.emplace(ri::tooling::SecureRipakExtraction::Extract(packageRoot));
        packageRoot = extraction->Root();
    }
    std::optional<ri::content::InstalledAssetPackage> package =
        LoadValidatedPackageForInstall(packageRoot);
    if (!package.has_value()) {
        throw std::runtime_error("Failed to load RawIron package: " + *packageArg);
    }
    if (!package->validation.valid) {
        for (const std::string& issue : package->validation.issues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error("RawIron package validation failed; import aborted.");
    }
    if (package->manifest.installScope == "project") {
        throw std::runtime_error("Package installScope is project-only; use --asset-package-install.");
    }

    const fs::path projectRoot = ResolveProjectRootOption(workspace, commandLine);
    fs::path targetRoot = projectRoot / "Packages" / package->manifest.packageId;
    if (const auto outputArg = commandLine.GetValue("--output-dir"); outputArg.has_value() && !outputArg->empty()) {
        targetRoot = fs::path(*outputArg);
    }
    fs::create_directories(targetRoot);

    for (const ri::content::AssetPackageEntry& asset : package->manifest.assets) {
        CopyFileChecked(package->packageRoot / fs::path(asset.path), targetRoot / fs::path(asset.path));
    }
    CopyFileChecked(package->manifestPath, targetRoot / "package.ri_package.json");

    ri::core::LogInfo("RawIron package imported as mounted package.");
    ri::core::LogInfo("  Project root: " + projectRoot.string());
    ri::core::LogInfo("  Package: " + package->manifest.packageId);
    ri::core::LogInfo("  Mounted at: " + targetRoot.string());
    ri::core::LogInfo("  Assets: " + std::to_string(package->manifest.assets.size()));
}

void InstallAssetPackage(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto packageArg = commandLine.GetValue("--asset-package-install");
    if (!packageArg.has_value() || packageArg->empty()) {
        throw std::runtime_error("Missing --asset-package-install <package-dir-or-manifest>.");
    }
    std::optional<ri::tooling::SecureRipakExtraction> extraction;
    fs::path packageRoot = fs::path(*packageArg);
    if (IsRipakArchivePath(packageRoot)) {
        extraction.emplace(ri::tooling::SecureRipakExtraction::Extract(packageRoot));
        packageRoot = extraction->Root();
    }
    std::optional<ri::content::InstalledAssetPackage> package =
        LoadValidatedPackageForInstall(packageRoot);
    if (!package.has_value()) {
        throw std::runtime_error("Failed to load RawIron package: " + *packageArg);
    }
    if (!package->validation.valid) {
        for (const std::string& issue : package->validation.issues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error("RawIron package validation failed; install aborted.");
    }
    if (package->manifest.installScope == "mounted") {
        throw std::runtime_error("Package installScope is mounted-only; use --asset-package-import.");
    }

    const fs::path projectRoot = ResolveProjectRootOption(workspace, commandLine);
    std::vector<PackageInstallCopyPlan> copyPlan;
    copyPlan.reserve(package->manifest.assets.size() + 1U);
    std::set<fs::path, ri::content::PackageInstallDestinationLess> resolvedDestinations;
    for (const ri::content::AssetPackageEntry& asset : package->manifest.assets) {
        const std::string relativeDestination = asset.installPath.empty()
            ? DefaultProjectInstallPath(package->manifest, asset).generic_string()
            : asset.installPath;
        const std::string label = "asset " + asset.id;
        const ri::content::PackageInstallPathResolution resolved =
            ResolvePackageInstallPathOrThrow(projectRoot, relativeDestination, label);
        if (!resolvedDestinations.insert(resolved.destination).second) {
            throw std::runtime_error(
                "RawIron package assets resolve to the same project destination: "
                + resolved.destination.string());
        }
        copyPlan.push_back({
            .source = package->packageRoot / fs::path(asset.path),
            .relativeDestination = relativeDestination,
            .resolvedDestination = resolved.destination,
            .label = label,
        });
    }

    const std::string receiptRelativePath =
        (fs::path("assets") / "package_receipts"
         / (package->manifest.packageId + ".ri_package.json")).generic_string();
    const ri::content::PackageInstallPathResolution resolvedReceipt =
        ResolvePackageInstallPathOrThrow(projectRoot, receiptRelativePath, "package receipt");
    if (!resolvedDestinations.insert(resolvedReceipt.destination).second) {
        throw std::runtime_error("RawIron package asset destination collides with its install receipt.");
    }
    copyPlan.push_back({
        .source = package->manifestPath,
        .relativeDestination = receiptRelativePath,
        .resolvedDestination = resolvedReceipt.destination,
        .label = "package receipt",
    });

    // Validate every destination, stage under an exclusive temp root, then promote with
    // backup/rollback so a mid-install failure does not leave a partial project mutation.
    ExclusivePackageInstallStaging staging = ExclusivePackageInstallStaging::Create();
    PromotePackageInstallTransactionally(projectRoot, staging.Root(), copyPlan);

    ri::core::LogInfo("RawIron package installed into project.");
    ri::core::LogInfo("  Project root: " + projectRoot.string());
    ri::core::LogInfo("  Package: " + package->manifest.packageId);
    ri::core::LogInfo("  Installed assets: " + std::to_string(package->manifest.assets.size()));
    ri::core::LogInfo("  Receipt: " + resolvedReceipt.destination.string());
}

int ParsePositiveIntOption(const ri::core::CommandLine& commandLine,
                          std::string_view option,
                          int fallback) {
    const std::optional<std::string> rawValue = commandLine.GetValue(option);
    if (!rawValue.has_value()) {
        return fallback;
    }

    const std::optional<int> parsedValue = commandLine.TryGetInt(option);
    if (!parsedValue.has_value()) {
        throw std::runtime_error("Invalid " + std::string(option) + " value '" + *rawValue + "'. Expected an integer.");
    }
    if (*parsedValue <= 0) {
        throw std::runtime_error(std::string(option) + " must be greater than zero.");
    }
    return *parsedValue;
}

ri::render::software::ScenePreviewOptions BuildScenePreviewOptions(const ri::core::CommandLine& commandLine,
                                                                   int defaultWidth,
                                                                   int defaultHeight) {
    ri::render::software::ScenePreviewOptions previewOptions{};
    previewOptions.width = std::max(64, ParsePositiveIntOption(commandLine, "--width", defaultWidth));
    previewOptions.height = std::max(64, ParsePositiveIntOption(commandLine, "--height", defaultHeight));
    return previewOptions;
}

void PrintWorkspace(const WorkspaceLayout& layout) {
    ri::core::LogInfo("Workspace root: " + layout.root.string());
    ri::core::LogInfo("  Documentation: " + layout.documentation.string());
    ri::core::LogInfo("  Source: " + layout.source.string());
    ri::core::LogInfo("  Apps: " + layout.apps.string());
    ri::core::LogInfo("  Tools: " + layout.tools.string());
    ri::core::LogInfo("  Config: " + layout.config.string());
    ri::core::LogInfo("  Assets/Source: " + layout.assetsSource.string());
    ri::core::LogInfo("  Assets/Cooked: " + layout.assetsCooked.string());
    ri::core::LogInfo("  Projects: " + layout.projects.string());
    ri::core::LogInfo("  Projects/Sandbox: " + layout.sandboxProject.string());
    ri::core::LogInfo("  Projects/Sandbox/Content: " + layout.sandboxContent.string());
    ri::core::LogInfo("  Projects/Sandbox/Scenes: " + layout.sandboxScenes.string());
    ri::core::LogInfo("  Projects/Sandbox/Saved: " + layout.sandboxSaved.string());
    ri::core::LogInfo("  Saved: " + layout.saved.string());
    ri::core::LogInfo("  Scripts: " + layout.scripts.string());
    ri::core::LogInfo("  ThirdParty: " + layout.thirdParty.string());
}

std::string EscapeJsonString(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string TitleCaseFromSlug(std::string_view slug) {
    std::string title;
    bool capitalizeNext = true;
    for (char ch : slug) {
        if (ch == '-' || ch == '_' || ch == ' ') {
            title.push_back(' ');
            capitalizeNext = true;
            continue;
        }
        if (capitalizeNext) {
            title.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            capitalizeNext = false;
        } else {
            title.push_back(ch);
        }
    }
    return title.empty() ? std::string("RawIron Project") : title;
}

std::string CompactPascalCase(std::string_view text) {
    std::string result;
    bool capitalizeNext = true;
    for (char ch : text) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        if (!std::isalnum(uc)) {
            capitalizeNext = true;
            continue;
        }
        if (capitalizeNext) {
            result.push_back(static_cast<char>(std::toupper(uc)));
            capitalizeNext = false;
        } else {
            result.push_back(ch);
        }
    }
    return result.empty() ? std::string("Project") : result;
}

std::string BuildProjectManifestJson(std::string_view projectId,
                                     std::string_view projectName,
                                     std::string_view author,
                                     std::string_view type,
                                     std::string_view version,
                                     std::string_view description) {
    const std::string projectIdString(projectId);
    const std::string projectNameString(projectName);
    const std::string authorString(author);
    const std::string typeString(type);
    const std::string versionString(version);
    const std::string descriptionString(description);
    const std::string moduleStem = CompactPascalCase(projectNameString);
    const std::string runtimeModule = "RawIron.Game." + moduleStem;
    const std::string entry = "RawIron." + moduleStem + "Game";
    const std::string editorProjectArg = "--game=" + projectIdString;

    std::ostringstream stream;
    stream << "{\n"
           << "  \"id\": \"" << EscapeJsonString(projectIdString) << "\",\n"
           << "  \"name\": \"" << EscapeJsonString(projectNameString) << "\",\n"
           << "  \"format\": \"rawiron-game-v1.3.7\",\n"
           << "  \"type\": \"" << EscapeJsonString(typeString) << "\",\n"
           << "  \"entry\": \"" << EscapeJsonString(entry) << "\",\n"
           << "  \"runtimeContract\": \"rawiron-runtime-v1\",\n"
           << "  \"runtimeModule\": \"" << EscapeJsonString(runtimeModule) << "\",\n"
           << "  \"runtimeHost\": \"RuntimeCore\",\n"
           << "  \"runtimeServices\": [\n"
           << "    \"lifecycle\",\n"
           << "    \"events\",\n"
           << "    \"services\",\n"
           << "    \"paths\",\n"
           << "    \"frame-clock\"\n"
           << "  ],\n"
           << "  \"version\": \"" << EscapeJsonString(versionString) << "\",\n"
           << "  \"author\": \"" << EscapeJsonString(authorString) << "\",\n"
           << "  \"editorProjectArg\": \"" << EscapeJsonString(editorProjectArg) << "\",\n"
           << "  \"primaryLevel\": \"levels/assembly.primitives.csv\",\n"
           << "  \"description\": \"" << EscapeJsonString(descriptionString) << "\",\n"
           << "  \"editorPreviewScene\": \"" << EscapeJsonString(projectIdString) << "\",\n"
           << "  \"controls\": {\n"
           << "    \"move\": \"WASD\",\n"
           << "    \"look\": \"Mouse\",\n"
           << "    \"jump\": \"Space\",\n"
           << "    \"sprint\": \"Shift\",\n"
           << "    \"quit\": \"Esc\"\n"
           << "  },\n"
           << "  \"editorOpenArgs\": [\n"
           << "    \"" << EscapeJsonString(editorProjectArg) << "\"\n"
           << "  ]\n"
           << "}\n";
    return stream.str();
}

std::vector<std::pair<fs::path, std::string>> BuildProjectContractFallbackFiles(std::string_view projectId,
                                                                                std::string_view projectName) {
    return {
        {fs::path("README.md"), "# " + std::string(projectName) + "\n\nCreated with RawIron tooling.\n"},
        {fs::path("scripts/logic.riscript"), "# RawIron logic script\nlogic.enabled=1\n"},
        {fs::path("scripts/state.riscript"), "# RawIron state script\nstate.bootstrap=\"default\"\n"},
        {fs::path("config/input.map"), "# RawIron input map\nmove_forward=W\nmove_back=S\nmove_left=A\nmove_right=D\njump=Space\nsprint=Shift\n"},
        {fs::path("levels/assembly.navmesh"), "# RawIron navmesh placeholder\n"},
        {fs::path("levels/assembly.ai.nodes"), "name,px,py,pz\nspawn_anchor,0,1,0\n"},
        {fs::path("levels/assembly.cinematics.csv"), "name,type,px,py,pz,payload\nintro_marker,marker,0,1,0,opening\n"},
        {fs::path("levels/assembly.occlusion.csv"), "name,type,px,py,pz,sx,sy,sz\nocclusion_anchor,box,0,1,0,4,4,4\n"},
        {fs::path("levels/assembly.audio.zones"), "name,type,px,py,pz,sx,sy,sz,preset\nambient_core,box,0,1,0,10,4,10,default\n"},
        {fs::path("levels/assembly.lods.csv"), "name,group,near,mid,far\nstarter_block,default,8,16,32\n"},
        {fs::path("assets/palette.ripalette"), "# RawIron palette\nentry 0 255 255 255 255\n"},
        {fs::path("assets/layers.config"), "# RawIron layer config\ndefault=world\n"},
        {fs::path("assets/manifest.assets"), "# RawIron asset manifest\n"},
        {fs::path("assets/metadata.json"), "{\n  \"rawironMetadataVersion\": 1,\n  \"project\": \"" + EscapeJsonString(projectId) + "\"\n}\n"},
        {fs::path("assets/dependencies.json"), "{\n  \"packages\": []\n}\n"},
        {fs::path("data/schema.db"), "SQLite format 3"},
        {fs::path("data/telemetry.db"), "SQLite format 3"},
        {fs::path("ai/factions.cfg"), "# RawIron factions\nplayer=allies\ndefault=neutral\n"},
        {fs::path("ai/perception.cfg"), "# RawIron perception\nvision_range=24\nhearing_range=12\n"},
        {fs::path("ai/squad.tactics"), "# RawIron squad tactics\nformation=loose\nfallback=hold\n"},
    };
}

std::size_t RepairMissingProjectContractFiles(const fs::path& projectRoot,
                                              std::string_view projectId,
                                              std::string_view projectName) {
    const auto fallbacks = BuildProjectContractFallbackFiles(projectId, projectName);
    std::size_t repairedCount = 0;
    for (const auto& [relativePath, body] : fallbacks) {
        const fs::path absolutePath = projectRoot / relativePath;
        if (fs::exists(absolutePath)) {
            continue;
        }
        fs::create_directories(absolutePath.parent_path());
        if (!ri::core::detail::WriteTextFile(absolutePath, body)) {
            throw std::runtime_error("Unable to write fallback project file: " + relativePath.generic_string());
        }
        ++repairedCount;
    }
    return repairedCount;
}

void PrintRawIronProjects(const fs::path& workspaceRoot) {
    const std::vector<ri::editor::WorkspaceGameEntry> games = ri::editor::EnumerateWorkspaceGames(workspaceRoot);
    ri::core::LogInfo("Workspace root: " + workspaceRoot.string());
    if (games.empty()) {
        ri::core::LogInfo("No game projects found under Games/.");
        return;
    }
    for (const ri::editor::WorkspaceGameEntry& game : games) {
        ri::core::LogInfo("project id=" + game.id + " name=\"" + game.displayName + "\" root=" + game.rootPath.string());
    }
    ri::core::LogInfo("Project count: " + std::to_string(games.size()));
}

std::optional<ProjectCommandContext> ResolveProjectCommandContext(const ri::core::CommandLine& commandLine,
                                                                 const WorkspaceLayout& workspace,
                                                                 std::string& error) {
    std::optional<ri::content::GameManifest> manifest;
    if (const auto gameRootArg = commandLine.GetValue("--game-root"); gameRootArg.has_value() && !gameRootArg->empty()) {
        const fs::path gameRoot = fs::weakly_canonical(fs::path(*gameRootArg));
        manifest = ri::content::LoadGameManifest(gameRoot / "manifest.json");
        if (!manifest.has_value()) {
            error = "Unable to load game manifest from --game-root.";
            return std::nullopt;
        }
    }

    if (!manifest.has_value()) {
        const std::optional<std::string> gameArg = commandLine.GetValue("--game");
        if (gameArg.has_value() && !gameArg->empty()) {
            manifest = ri::content::ResolveGameManifest(workspace.root, *gameArg);
            if (!manifest.has_value()) {
                error = "Unable to resolve game manifest for '" + *gameArg + "'.";
                return std::nullopt;
            }
        }
    }

    if (!manifest.has_value()) {
        error = "Project command requires --game=<id> or --game-root=<path>.";
        return std::nullopt;
    }

    return ProjectCommandContext{
        .workspaceRoot = workspace.root,
        .manifest = *manifest,
    };
}

void CheckGamePackageMounts(
    const WorkspaceLayout& workspace,
    const ri::core::CommandLine& commandLine) {
    std::string error;
    const std::optional<ProjectCommandContext> context =
        ResolveProjectCommandContext(commandLine, workspace, error);
    if (!context.has_value()) {
        throw std::runtime_error(error);
    }

    ri::content::PackageMountRegistry registry;
    const ri::content::GamePackageMountReport report =
        ri::content::MountDeclaredGamePackages(registry, context->manifest.rootPath);
    ri::core::LogInfo("RawIron game package mount check:");
    ri::core::LogInfo("  Game: " + context->manifest.id);
    ri::core::LogInfo("  Declared packages: " + std::to_string(report.requirements.packages.size()));
    ri::core::LogInfo(
        "  Required packages: "
        + std::string(report.requiredPackagesMounted ? "mounted" : "failed"));
    for (const std::string& issue : report.issues) {
        ri::core::LogInfo("  Issue: " + issue);
    }
    if (!report.requiredPackagesMounted) {
        throw std::runtime_error("RawIron game package mount check failed.");
    }
    const std::vector<ri::content::MountedPackageInfo> mounts = registry.MountedPackages();
    ri::core::LogInfo("  Live mounts: " + std::to_string(mounts.size()));
    for (const ri::content::MountedPackageInfo& mount : mounts) {
        ri::core::LogInfo(
            "    " + mount.packageId + "@" + mount.packageVersion
            + " -> " + mount.mountPoint);
    }
    ri::content::ReleaseDeclaredGamePackages(registry, report);
}

void DescribeRawIronProject(const ProjectCommandContext& context) {
    const std::vector<ri::editor::WorkspaceResourceEntry> resources =
        ri::editor::CollectWorkspaceGameResources(context.manifest.rootPath);
    const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(context.manifest);
    std::array<int, 8> categoryCounts{};
    for (const ri::editor::WorkspaceResourceEntry& entry : resources) {
        categoryCounts[static_cast<std::size_t>(entry.category)] += 1;
    }

    std::vector<std::string> categoriesSummary;
    static constexpr std::array<ri::editor::WorkspaceResourceCategory, 8> kCategories = {
        ri::editor::WorkspaceResourceCategory::Manifest,
        ri::editor::WorkspaceResourceCategory::Level,
        ri::editor::WorkspaceResourceCategory::Script,
        ri::editor::WorkspaceResourceCategory::Test,
        ri::editor::WorkspaceResourceCategory::UiScreen,
        ri::editor::WorkspaceResourceCategory::Menu,
        ri::editor::WorkspaceResourceCategory::Asset,
        ri::editor::WorkspaceResourceCategory::Other,
    };
    for (ri::editor::WorkspaceResourceCategory category : kCategories) {
        categoriesSummary.push_back(
            ri::editor::WorkspaceCategoryShortLabel(category) + "="
            + std::to_string(categoryCounts[static_cast<std::size_t>(category)]));
    }

    ri::core::LogInfo("Workspace root: " + context.workspaceRoot.string());
    ri::core::LogInfo("Project id: " + context.manifest.id);
    ri::core::LogInfo("Project name: " + context.manifest.name);
    ri::core::LogInfo("Project version: " + context.manifest.version);
    ri::core::LogInfo("Project author: " + context.manifest.author);
    ri::core::LogInfo("Project root: " + context.manifest.rootPath.string());
    ri::core::LogInfo("Manifest path: " + context.manifest.manifestPath.string());
    ri::core::LogInfo("Runtime module: " + context.manifest.runtimeModule);
    ri::core::LogInfo("Primary level: " + context.manifest.primaryLevel);
    ri::core::LogInfo("Editor preview scene: " + context.manifest.editorPreviewScene);
    ri::core::LogInfo("Editor launch token: " + context.manifest.editorProjectArg);
    ri::core::LogInfo("Resource counts: " + JoinStrings(categoriesSummary, " | "));
    if (formatIssues.empty()) {
        ri::core::LogInfo("Format validation: pass");
    } else {
        ri::core::LogInfo("Format validation: fail (" + std::to_string(formatIssues.size()) + " issues)");
        for (const std::string& issue : formatIssues) {
            ri::core::LogInfo("  - " + issue);
        }
    }
}

void ListRawIronProjectResources(const ProjectCommandContext& context, const ri::core::CommandLine& commandLine) {
    std::optional<ri::editor::WorkspaceResourceCategory> filterCategory;
    if (const auto filter = commandLine.GetValue("--resource-category"); filter.has_value() && !filter->empty()) {
        ri::editor::WorkspaceResourceCategory parsed{};
        if (!TryParseWorkspaceResourceCategory(*filter, parsed)) {
            throw std::runtime_error("Unknown --resource-category value: " + *filter);
        }
        filterCategory = parsed;
    }

    const std::vector<ri::editor::WorkspaceResourceEntry> resources =
        ri::editor::CollectWorkspaceGameResources(context.manifest.rootPath);
    ri::core::LogInfo("Project id: " + context.manifest.id);
    ri::core::LogInfo("Project root: " + context.manifest.rootPath.string());
    if (filterCategory.has_value()) {
        ri::core::LogInfo("Filter category: " + ri::editor::WorkspaceCategoryLabel(*filterCategory));
    }

    std::size_t count = 0;
    for (const ri::editor::WorkspaceResourceEntry& entry : resources) {
        if (filterCategory.has_value() && entry.category != *filterCategory) {
            continue;
        }
        ri::core::LogInfo("[" + ri::editor::WorkspaceCategoryShortLabel(entry.category) + "] " + entry.relativePathUtf8);
        ++count;
    }
    ri::core::LogInfo("Resource rows: " + std::to_string(count));
}

bool DoctorRawIronProject(const ProjectCommandContext& context) {
    const std::vector<ri::editor::WorkspaceResourceEntry> resources =
        ri::editor::CollectWorkspaceGameResources(context.manifest.rootPath);
    const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(context.manifest);

    std::array<int, 8> categoryCounts{};
    for (const ri::editor::WorkspaceResourceEntry& entry : resources) {
        categoryCounts[static_cast<std::size_t>(entry.category)] += 1;
    }

    std::vector<std::string> categoriesSummary;
    static constexpr std::array<ri::editor::WorkspaceResourceCategory, 8> kCategories = {
        ri::editor::WorkspaceResourceCategory::Manifest,
        ri::editor::WorkspaceResourceCategory::Level,
        ri::editor::WorkspaceResourceCategory::Script,
        ri::editor::WorkspaceResourceCategory::Test,
        ri::editor::WorkspaceResourceCategory::UiScreen,
        ri::editor::WorkspaceResourceCategory::Menu,
        ri::editor::WorkspaceResourceCategory::Asset,
        ri::editor::WorkspaceResourceCategory::Other,
    };
    for (ri::editor::WorkspaceResourceCategory category : kCategories) {
        categoriesSummary.push_back(
            ri::editor::WorkspaceCategoryShortLabel(category) + "="
            + std::to_string(categoryCounts[static_cast<std::size_t>(category)]));
    }

    ri::core::LogInfo("Doctor project id: " + context.manifest.id);
    ri::core::LogInfo("Project root: " + context.manifest.rootPath.string());
    ri::core::LogInfo("Manifest path: " + context.manifest.manifestPath.string());
    ri::core::LogInfo("Runtime module: " + context.manifest.runtimeModule);
    ri::core::LogInfo("Primary level: " + context.manifest.primaryLevel);
    ri::core::LogInfo("Resource counts: " + JoinStrings(categoriesSummary, " | "));
    if (formatIssues.empty()) {
        ri::core::LogInfo("Doctor result: healthy");
        return true;
    }

    ri::core::LogInfo("Doctor result: unhealthy (" + std::to_string(formatIssues.size()) + " issues)");
    for (const std::string& issue : formatIssues) {
        ri::core::LogInfo("  - " + issue);
    }
    return false;
}

bool InspectProjectPlugins(const ProjectCommandContext& context, const bool doctorMode) {
    const ri::content::PluginProjectData data = ri::content::LoadPluginProjectData(context.manifest.rootPath);
    ri::core::LogInfo(std::string(doctorMode ? "Plugin doctor project: " : "Plugin project: ") + context.manifest.id);
    ri::core::LogInfo("Plugin summary: " + ri::content::SummarizePluginProjectData(data));
    for (const ri::content::PluginManifestEntry& manifest : data.manifestEntries) {
        const bool active = std::any_of(data.activePlugins.begin(), data.activePlugins.end(), [&](const auto& plugin) {
            return plugin.manifest.id == manifest.id;
        });
        const auto registry = std::find_if(data.registryEntries.begin(), data.registryEntries.end(), [&](const auto& entry) {
            return entry.id == manifest.id;
        });
        std::string status = "INACTIVE";
        if (active) {
            status = "ACTIVE";
        } else if (manifest.blockedByPolicy) {
            status = "BLOCKED";
        } else if (!manifest.entryPathValid) {
            status = "REJECTED";
        } else if (!manifest.entryIsRemote && !manifest.entryExists) {
            status = "MISSING";
        } else if (registry != data.registryEntries.end() && !registry->enabled) {
            status = "DISABLED";
        }
        ri::core::LogInfo("[" + status + "] " + manifest.id + " version=" + manifest.version
                          + " source=" + std::string(ri::content::ToString(manifest.sourceKind))
                          + " entry=" + manifest.entryPath);
        if (!manifest.resolvedEntryPath.empty()) {
            ri::core::LogInfo("  resolved=" + manifest.resolvedEntryPath.string());
        }
        if (manifest.blockedByPolicy) {
            ri::core::LogInfo("  policy=" + manifest.policyBlockReason);
        }
        if (registry != data.registryEntries.end()) {
            ri::core::LogInfo(
                "  capabilities="
                + (registry->capabilities.empty() ? std::string("<none>")
                                                  : JoinStrings(registry->capabilities, ",")));
        }
    }
    if (data.issues.empty()) {
        ri::core::LogInfo(std::string(doctorMode ? "Plugin doctor result: healthy" : "Plugin validation: pass"));
        return true;
    }
    ri::core::LogInfo(std::string(doctorMode ? "Plugin doctor result: unhealthy" : "Plugin validation: fail")
                      + " (" + std::to_string(data.issues.size()) + " issues)");
    for (const ri::content::PluginValidationIssue& issue : data.issues) {
        ri::core::LogInfo("  - " + issue.message);
    }
    return false;
}

void PrintPluginHandlers() {
    const std::vector<ri::content::PluginHookHandlerInfo> handlers =
        ri::content::RegisteredPluginHookHandlers();
    ri::core::LogInfo("Registered Raw Iron plugin hook handlers:");
    for (const ri::content::PluginHookHandlerInfo& handler : handlers) {
        ri::core::LogInfo(
            "  " + handler.eventName + " capability="
            + (handler.requiredCapability.empty() ? std::string("<none>") : handler.requiredCapability));
    }
    ri::core::LogInfo("Handler count: " + std::to_string(handlers.size()));
}

bool ValidatePluginPackageFile(const ri::core::CommandLine& commandLine) {
    const std::optional<std::string> pathArg = commandLine.GetValue("--plugin-package-validate");
    if (!pathArg.has_value() || pathArg->empty()) {
        throw std::runtime_error(
            "--plugin-package-validate requires a plugin package directory, "
            "package.riplugin.json path, or .ripak archive.");
    }
    std::optional<ri::tooling::SecureRipakExtraction> extraction;
    fs::path packageRoot = fs::path(*pathArg);
    if (IsRipakArchivePath(packageRoot)) {
        extraction.emplace(ri::tooling::SecureRipakExtraction::Extract(packageRoot));
        packageRoot = extraction->Root();
    }
    const ri::content::PluginPackageValidationReport report =
        ri::content::ValidatePluginPackage(packageRoot);
    ri::core::LogInfo("Plugin package root: "
                      + (report.packageRoot.empty() ? std::string("<unresolved>") : report.packageRoot.string()));
    if (!report.descriptor.id.empty()) {
        ri::core::LogInfo("Plugin package id: " + report.descriptor.id);
        ri::core::LogInfo("Plugin package version: " + report.descriptor.version);
    }
    ri::core::LogInfo(std::string("Plugin package validation: ") + (report.valid ? "pass" : "fail"));
    for (const std::string& issue : report.issues) {
        ri::core::LogInfo("  - " + issue);
    }
    return report.valid;
}

void BuildPluginPackage(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto packageArg = commandLine.GetValue("--plugin-package-build");
    if (!packageArg.has_value() || packageArg->empty()) {
        throw std::runtime_error(
            "Missing --plugin-package-build <plugin-package-dir-or-package.riplugin.json>.");
    }

    const ri::content::PluginPackageArchivePlan plan =
        ri::content::PlanPluginPackageArchive(fs::path(*packageArg));
    if (!plan.valid) {
        for (const std::string& issue : plan.issues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error("Plugin package validation failed; archive build aborted.");
    }

    fs::path archivePath = workspace.assetsCooked / "Packages" / "Plugins"
        / (plan.descriptor.id + "-" + plan.descriptor.version + ".ripak");
    if (const auto outputArg = commandLine.GetValue("--output"); outputArg.has_value() && !outputArg->empty()) {
        archivePath = fs::path(*outputArg);
    } else if (const auto packageOut = commandLine.GetValue("--package");
               packageOut.has_value() && !packageOut->empty()) {
        archivePath = fs::path(*packageOut);
    }
    if (!IsRipakArchivePath(archivePath)) {
        archivePath += ".ripak";
    }

    ExclusivePackageInstallStaging staging = ExclusivePackageInstallStaging::Create();
    std::vector<std::string> stageIssues;
    if (!ri::content::StagePluginPackageArchive(plan, staging.Root(), stageIssues)) {
        for (const std::string& issue : stageIssues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error("Plugin package staging failed; archive build aborted.");
    }

    // Native STORED writer: PowerShell Compress-Archive DEFLATE often leaves trailing
    // bytes that SecureRipakExtraction rejects.
    ri::tooling::WriteStoredRipakArchiveFromDirectory(staging.Root(), archivePath);

    // Round-trip proof: extract the archive and re-validate the public contract.
    const ri::tooling::SecureRipakExtraction extraction =
        ri::tooling::SecureRipakExtraction::Extract(archivePath);
    const ri::content::PluginPackageValidationReport extracted =
        ri::content::ValidatePluginPackage(extraction.Root());
    if (!extracted.valid || extracted.descriptor.id != plan.descriptor.id) {
        for (const std::string& issue : extracted.issues) {
            ri::core::LogInfo("  Issue: " + issue);
        }
        throw std::runtime_error(
            "Plugin package archive failed post-build validation: " + archivePath.string());
    }

    ri::core::LogInfo("RawIron plugin package built.");
    ri::core::LogInfo("  Package root: " + plan.packageRoot.string());
    ri::core::LogInfo("  Package id: " + plan.descriptor.id);
    ri::core::LogInfo("  Package version: " + plan.descriptor.version);
    ri::core::LogInfo("  Archive entries: " + std::to_string(plan.relativeEntries.size()));
    ri::core::LogInfo("  Archive: " + archivePath.string());
}

bool ValidateExtensionFile(const ri::core::CommandLine& commandLine) {
    const std::optional<std::string> pathArg = commandLine.GetValue("--extension-validate");
    if (!pathArg.has_value() || pathArg->empty()) {
        throw std::runtime_error("--extension-validate requires a descriptor or package JSON path.");
    }
    const fs::path path(*pathArg);
    const std::string text = ri::core::detail::ReadTextFile(path);
    if (text.empty()) {
        throw std::runtime_error("Unable to read extension descriptor: " + path.string());
    }
    std::optional<ri::content::ExtensionDescriptor> descriptor = ri::content::ExtractExtensionDescriptor(text);
    if (!descriptor.has_value()) {
        descriptor = ri::content::ParseExtensionDescriptor(text);
    }
    if (!descriptor.has_value()) {
        ri::core::LogInfo("Extension validation: fail (descriptor could not be parsed)");
        return false;
    }
    const ri::content::ExtensionValidationReport report = ri::content::ValidateExtensionDescriptor(*descriptor);
    ri::core::LogInfo("Extension id: " + descriptor->id);
    ri::core::LogInfo("Extension kind: " + std::string(ri::content::ToString(descriptor->kind)));
    ri::core::LogInfo("Extension host: " + std::string(ri::content::ToString(descriptor->host)));
    ri::core::LogInfo(std::string("Extension validation: ") + (report.valid ? "pass" : "fail"));
    for (const std::string& issue : report.issues) {
        ri::core::LogInfo("  - " + issue);
    }
    return report.valid;
}

void PrintToolHelp() {
    ri::core::LogInfo("Usage: ri_tool <command> [options]");
    ri::core::LogInfo("Workspace and projects:");
    ri::core::LogInfo("  --workspace | --ensure-workspace | --list-projects");
    ri::core::LogInfo("  --create-project <id> | --describe-project | --doctor-project | --scaffold-project");
    ri::core::LogInfo("  --list-project-resources [--resource-category <name>]");
    ri::core::LogInfo("Plugins, mods, and extensions:");
    ri::core::LogInfo("  --plugins-list --game <id> | --plugins-doctor --game <id>");
    ri::core::LogInfo("  --plugin-handlers | --extension-validate <package.riplugin.json>");
    ri::core::LogInfo("  --plugin-package-validate <package-dir-or-package.riplugin.json|.ripak>");
    ri::core::LogInfo("  --plugin-package-build <package-dir-or-package.riplugin.json> [--output|--package <out.ripak>]");
    ri::core::LogInfo("Assets and authoring:");
    ri::core::LogInfo("  --formats | --asset-standardize <path> | --asset-standardize-dir <dir>");
    ri::core::LogInfo("  --asset-package-build | --asset-package-validate | --asset-package-import | --asset-package-install");
    ri::core::LogInfo("  --asset-package-resolve <id> [--package-version <range>] [--project <root>]");
    ri::core::LogInfo("  --asset-package-mount-check <id> [--package-version <range>] [--project <root>]");
    ri::core::LogInfo("  --game-package-mount-check --game <id> | --game-root <path>");
    ri::core::LogInfo("  --rig-toolchain-report | --rig-create-humanoid <id> | --rig-validate <path>");
    ri::core::LogInfo("Forge clay, block modeling, and motion (headless — no UI):");
    ri::core::LogInfo("  --forge-report | --forge-assets-list [--kind sculpt|rig|anim|block|all]");
    ri::core::LogInfo("  --forge-character-create <id> [--name] [--style sphere|cube] [--walk] [--open] [--overwrite]");
    ri::core::LogInfo("  --blockchar-create <id> [--name] [--rig <path>] [--from <user-blockchar>] [--overwrite]");
    ri::core::LogInfo("  --blockchar-list-shapes");
    ri::core::LogInfo("  --blockchar-add-part --blockchar <path> --bone <name> [--part <id>] [--shape box|prism|dome|wedge|cylinder|spike|slab|bevel|capsule]");
    ri::core::LogInfo("      [--x|--y|--z] [--sx|--sy|--sz] [--rx|--ry|--rz] [--tsx|--tsy|--tsz] [--sides N] [--bevel 0..0.45]");
    ri::core::LogInfo("      [--color r,g,b] [--texture <rel>] [--roughness] [--metallic]");
    ri::core::LogInfo("  --blockchar-set-part --blockchar <path> --part <id> (same transform/look/shape flags)");
    ri::core::LogInfo("  --blockchar-nudge --blockchar <path> --part <id> [--dx|--dy|--dz|--drx|--dry|--drz|--dsx|--dsy|--dsz|--scale]");
    ri::core::LogInfo("  --blockchar-duplicate --blockchar <path> --part <id> --as <new-id> [--dx|--dy|--dz] [--bone]");
    ri::core::LogInfo("  --blockchar-mirror --blockchar <path> --part <id> [--axis x|z] [--as <new-id>]");
    ri::core::LogInfo("  --blockchar-inset --blockchar <path> --part <id> [--face front|back|top|bottom] [--inset] [--depth] [--as] [--shape] [--bevel]");
    ri::core::LogInfo("  --blockchar-attach --blockchar <path> --parent <id> --along front|back|top|bottom|left|right [--offset] [--shape] [--sx|--sy|--sz] ...");
    ri::core::LogInfo("  --blockchar-recolor --blockchar <path> [--part <id>|all] [--color r,g,b|--mul r,g,b]");
    ri::core::LogInfo("  --blockchar-bevel --blockchar <path> --part <id> [--bevel 0.2]");
    ri::core::LogInfo("  --blockchar-remove-part --blockchar <path> --part <id>");
    ri::core::LogInfo("  --blockchar-report <path> | --blockchar-write-textures --blockchar <path>");
    ri::core::LogInfo("  --blockchar-sync-sculpt --blockchar <path> [--rig <path>] [--sculpt <path>] [--overwrite]");
    ri::core::LogInfo("  --function <name> --id <character-id> [--name] [--overwrite] [--open] | --functions-list");
    ri::core::LogInfo("  --forge-handoff-probe <asset> [--game <id>]");
    ri::core::LogInfo("  --sculpt-create <id> [--cage sphere|cube] [--output <path>] [--overwrite] [--name <label>]");
    ri::core::LogInfo("  --sculpt-bind --sculpt <path> --rig <path> [--replace]");
    ri::core::LogInfo("  --sculpt-fit-rig --sculpt <path> --rig <path> [--padding <scale>]");
    ri::core::LogInfo("  --sculpt-flood-weights|--sculpt-flood-free --sculpt <path> --bone <name>");
    ri::core::LogInfo("  --sculpt-soft|--sculpt-soft-bone|--sculpt-cap4|--sculpt-normalize|--sculpt-seal --sculpt <path>");
    ri::core::LogInfo("  --sculpt-prune --sculpt <path> [--min <w>] | --sculpt-mirror --sculpt <path> [--axis x|y|z]");
    ri::core::LogInfo("  --sculpt-invert|--sculpt-ceil|--sculpt-harden|--sculpt-grow|--sculpt-shrink --sculpt <path> --bone <name>");
    ri::core::LogInfo("  --sculpt-transfer --sculpt <path> --from <bone> --to <bone>");
    ri::core::LogInfo("  --sculpt-swap --sculpt <path> --left <bone> --right <bone>");
    ri::core::LogInfo("  --sculpt-paint --sculpt <path> --bone <name> (--at <x,y,z> [--unit]|--at-bone) [--snap] [--radius] [--mode] [--mirror]");
    ri::core::LogInfo("  --sculpt-bounds|--sculpt-bone-weights|--sculpt-audit-weights|--sculpt-validate --sculpt <path>");
    ri::core::LogInfo("  --sculpt-unbind|--sculpt-clear-weights --sculpt <path>");
    ri::core::LogInfo("  --anim-create <id> --rig <path> [--output <path>] [--overwrite] [--name <label>]");
    ri::core::LogInfo("  --anim-author-idle|--anim-author-walk <path> [--rig <path>] [--motion-intensity <0.25-2.5>]");
    ri::core::LogInfo("  --anim-key-rest|--anim-hold|--anim-breakdown --anim <path> [--bone <name>] [--time <s>]");
    ri::core::LogInfo("  --anim-key --anim <path> --bone <name> --time <s> [--tx|--ty|--tz|--rx|--ry|--rz] [--from-previous]");
    ri::core::LogInfo("  --anim-event --anim <path> --name <marker> [--time <s>]");
    ri::core::LogInfo("  --anim-trim --anim <path> --start <s> --end <s> [--no-shift]");
    ri::core::LogInfo("  --anim-fit-duration|--anim-align-start --anim <path>");
    ri::core::LogInfo("  --anim-validate <path> | --anim-report <path>");
    ri::core::LogInfo("Rendering and diagnostics:");
    ri::core::LogInfo("  --scenekit-targets | --scenekit-checks | --scenekit-example <slug>");
    ri::core::LogInfo("  --postprocess-presets | --vulkan-diagnostics | --render-cube | --sample-scene");
    ri::core::LogInfo("General options: --root <workspace> --game <id> --game-root <path> --help --version");
}

bool CommandRequested(const ri::core::CommandLine& commandLine, const std::string_view command) {
    return commandLine.HasFlag(command) || commandLine.GetValue(command).has_value();
}

void ValidateSinglePrimaryCommand(const ri::core::CommandLine& commandLine) {
    static constexpr std::array<std::string_view, 97> commands = {{
        "--workspace",
        "--list-projects",
        "--ensure-workspace",
        "--create-project",
        "--describe-project",
        "--doctor-project",
        "--list-project-resources",
        "--scaffold-project",
        "--formats",
        "--plugins-list",
        "--plugins-doctor",
        "--plugin-handlers",
        "--extension-validate",
        "--plugin-package-validate",
        "--plugin-package-build",
        "--rig-toolchain-report",
        "--rig-create-humanoid",
        "--rig-validate",
        "--forge-report",
        "--forge-assets-list",
        "--forge-character-create",
        "--blockchar-create",
        "--blockchar-add-part",
        "--blockchar-set-part",
        "--blockchar-nudge",
        "--blockchar-remove-part",
        "--blockchar-list-shapes",
        "--blockchar-duplicate",
        "--blockchar-mirror",
        "--blockchar-inset",
        "--blockchar-attach",
        "--blockchar-recolor",
        "--blockchar-bevel",
        "--blockchar-report",
        "--blockchar-write-textures",
        "--blockchar-sync-sculpt",
        "--function",
        "--functions-list",
        "--forge-handoff-probe",
        "--sculpt-create",
        "--sculpt-bind",
        "--sculpt-fit-rig",
        "--sculpt-flood-weights",
        "--sculpt-flood-free",
        "--sculpt-soft",
        "--sculpt-soft-bone",
        "--sculpt-cap4",
        "--sculpt-normalize",
        "--sculpt-prune",
        "--sculpt-mirror",
        "--sculpt-seal",
        "--sculpt-invert",
        "--sculpt-ceil",
        "--sculpt-harden",
        "--sculpt-transfer",
        "--sculpt-swap",
        "--sculpt-grow",
        "--sculpt-shrink",
        "--sculpt-paint",
        "--sculpt-bounds",
        "--sculpt-bone-weights",
        "--sculpt-audit-weights",
        "--sculpt-validate",
        "--sculpt-unbind",
        "--sculpt-clear-weights",
        "--anim-create",
        "--anim-author-idle",
        "--anim-author-walk",
        "--anim-key-rest",
        "--anim-key",
        "--anim-hold",
        "--anim-breakdown",
        "--anim-event",
        "--anim-trim",
        "--anim-fit-duration",
        "--anim-align-start",
        "--anim-validate",
        "--anim-report",
        "--asset-standardize",
        "--asset-standardize-dir",
        "--asset-package-build",
        "--asset-package-validate",
        "--asset-package-import",
        "--asset-package-install",
        "--asset-package-resolve",
        "--asset-package-mount-check",
        "--game-package-mount-check",
        "--scenekit-targets",
        "--postprocess-presets",
        "--scenekit-checks",
        "--scenekit-example",
        "--vulkan-diagnostics",
        "--render-cube",
        "--sample-scene",
        "--save-scene-state",
        "--load-scene-state",
    }};
    std::vector<std::string_view> requested{};
    for (const std::string_view command : commands) {
        if (CommandRequested(commandLine, command)
            && std::find(requested.begin(), requested.end(), command) == requested.end()) {
            requested.push_back(command);
        }
    }
    if (requested.size() <= 1U) {
        return;
    }
    std::string message = "Multiple primary commands were provided:";
    for (const std::string_view command : requested) {
        message += " ";
        message += command;
    }
    message += ". Run one command at a time.";
    throw std::runtime_error(message);
}

void ScaffoldRawIronProject(const ProjectCommandContext& context) {
    std::size_t createdCount = 0;
    std::vector<std::string> createdFiles;
    std::string error;
    if (!ri::editor::EnsureMountedGameScaffold(context.manifest, createdCount, createdFiles, &error)) {
        throw std::runtime_error("Scaffold failed: " + error);
    }
    ri::editor::EnsureProjectDevConfig(context.manifest.rootPath);
    ri::core::LogInfo("Project id: " + context.manifest.id);
    ri::core::LogInfo("Created files: " + std::to_string(createdCount));
    if (createdFiles.empty()) {
        ri::core::LogInfo("Scaffold already present. No files created.");
        return;
    }
    for (const std::string& file : createdFiles) {
        ri::core::LogInfo("  + " + file);
    }
}

void CreateRawIronProject(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto projectIdArg = commandLine.GetValue("--create-project");
    if (!projectIdArg.has_value() || projectIdArg->empty()) {
        throw std::runtime_error("--create-project requires a lowercase slug id.");
    }

    const std::string projectId = ToLowerAscii(TrimAscii(*projectIdArg));
    const std::string projectName = commandLine.GetValue("--name").has_value() && !commandLine.GetValue("--name")->empty()
        ? TrimAscii(*commandLine.GetValue("--name"))
        : TitleCaseFromSlug(projectId);
    const std::string author = commandLine.GetValue("--author").has_value() && !commandLine.GetValue("--author")->empty()
        ? TrimAscii(*commandLine.GetValue("--author"))
        : std::string("RawIron Team");
    const std::string type = commandLine.GetValue("--type").has_value() && !commandLine.GetValue("--type")->empty()
        ? TrimAscii(*commandLine.GetValue("--type"))
        : std::string("game");
    const std::string version = commandLine.GetValue("--version").has_value() && !commandLine.GetValue("--version")->empty()
        ? TrimAscii(*commandLine.GetValue("--version"))
        : std::string("1.0.0");
    const std::string description = commandLine.GetValue("--description").has_value() && !commandLine.GetValue("--description")->empty()
        ? TrimAscii(*commandLine.GetValue("--description"))
        : std::string("Project created by ri_tool.");

    const fs::path projectRoot = commandLine.GetValue("--game-root").has_value() && !commandLine.GetValue("--game-root")->empty()
        ? fs::weakly_canonical(fs::path(*commandLine.GetValue("--game-root")).parent_path()) / fs::path(*commandLine.GetValue("--game-root")).filename()
        : (workspace.root / "Games" / CompactPascalCase(projectName));
    std::error_code ec{};

    if (fs::exists(projectRoot / "manifest.json")) {
        throw std::runtime_error("Target project already contains manifest.json: " + (projectRoot / "manifest.json").string());
    }

    if (fs::exists(projectRoot)) {
        const auto directoryIterator = fs::directory_iterator(projectRoot, ec);
        if (!ec && directoryIterator != fs::directory_iterator{}) {
            throw std::runtime_error(
                "Target project root must be empty or absent for --create-project: " + projectRoot.string());
        }
        ec.clear();
    }

    fs::create_directories(projectRoot, ec);
    if (ec) {
        throw std::runtime_error("Unable to create project root: " + projectRoot.string());
    }

    const fs::path manifestPath = projectRoot / "manifest.json";
    const std::string manifestJson = BuildProjectManifestJson(projectId, projectName, author, type, version, description);
    if (!ri::core::detail::WriteTextFile(manifestPath, manifestJson)) {
        throw std::runtime_error("Unable to write manifest.json for new project.");
    }

    const std::optional<ri::content::GameManifest> manifest = ri::content::LoadGameManifest(manifestPath);
    if (!manifest.has_value()) {
        throw std::runtime_error("New project manifest could not be reloaded after write.");
    }

    std::size_t createdCount = 0;
    std::vector<std::string> createdFiles;
    std::string scaffoldError;
    if (!ri::editor::EnsureMountedGameScaffold(*manifest, createdCount, createdFiles, &scaffoldError)) {
        throw std::runtime_error("Scaffold failed: " + scaffoldError);
    }
    ri::editor::EnsureProjectDevConfig(projectRoot);

    std::size_t repairedCount = 0;
    std::optional<ri::content::GameManifest> reloaded = ri::content::LoadGameManifest(manifestPath);
    if (!reloaded.has_value()) {
        throw std::runtime_error("New project manifest could not be reloaded after scaffold.");
    }
    std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(*reloaded);
    if (!formatIssues.empty()) {
        repairedCount = RepairMissingProjectContractFiles(projectRoot, reloaded->id, reloaded->name);
        reloaded = ri::content::LoadGameManifest(manifestPath);
        if (!reloaded.has_value()) {
            throw std::runtime_error("New project manifest could not be reloaded after contract repair.");
        }
        formatIssues = ri::content::ValidateGameProjectFormat(*reloaded);
    }
    if (!formatIssues.empty()) {
        throw std::runtime_error("New project failed format validation: " + JoinStrings(formatIssues, " | "));
    }

    ri::core::LogInfo("Created RawIron project.");
    ri::core::LogInfo("Project id: " + reloaded->id);
    ri::core::LogInfo("Project name: " + reloaded->name);
    ri::core::LogInfo("Project root: " + projectRoot.string());
    ri::core::LogInfo("Manifest path: " + manifestPath.string());
    ri::core::LogInfo("Runtime module: " + reloaded->runtimeModule);
    ri::core::LogInfo("Editor launch token: " + reloaded->editorProjectArg);
    ri::core::LogInfo("Scaffold files created: " + std::to_string(createdCount));
    ri::core::LogInfo("Contract repairs applied: " + std::to_string(repairedCount));
}

void EnsureWorkspace(const WorkspaceLayout& layout) {
    for (const fs::path& path : RequiredWorkspacePaths(layout)) {
        fs::create_directories(path);
    }
}

void PrintSceneKitTargets() {
    ri::core::LogInfo("Scene Kit parity gate: " + std::to_string(kSceneReferenceTargets.size()) + " tracked references");
    ri::core::LogInfo("RawIron should not call RawIron Scene Kit usable until these examples are reproducible.");
    for (const SceneReferenceTarget& target : kSceneReferenceTargets) {
        ri::core::LogInfo("  [" + std::string(target.status) + "] " + target.slug + " - " + target.title);
        ri::core::LogInfo("    URL: " + std::string(target.url));
        ri::core::LogInfo("    RawIron: " + std::string(target.rawIronTrack));
    }
}

void PrintPostProcessPresets() {
    const std::span<const ri::render::PostProcessPresetDefinition> definitions =
        ri::render::GetPostProcessPresetDefinitions();
    ri::core::LogInfo("RawIron post-process preset catalog: " + std::to_string(definitions.size()) + " options");
    ri::core::LogInfo("These presets are native effect families for stacking and editor/runtime selection.");
    for (const ri::render::PostProcessPresetDefinition& definition : definitions) {
        const ri::render::PostProcessParameters parameters =
            ri::render::MakePostProcessPreset(definition.preset);
        std::ostringstream summary;
        summary << "  [" << definition.slug << "] " << definition.label
                << " | noise=" << std::fixed << std::setprecision(4) << parameters.noiseAmount
                << " | scan=" << parameters.scanlineAmount
                << " | barrel=" << parameters.barrelDistortion
                << " | chroma=" << parameters.chromaticAberration
                << " | tint=" << parameters.tintStrength
                << " | blur=" << parameters.blurAmount
                << " | static=" << parameters.staticFadeAmount
                << " | cas=" << parameters.casSharpenAmount << "/" << parameters.casContrastAdaptation
                << " | bloom=" << parameters.bloomIntensity << "@" << parameters.bloomThreshold
                << " | deband=" << parameters.debandStrength << " | curve=" << parameters.toneCurveStrength
                << " | tdither=" << parameters.outputDitherStrength
                << " | vig=" << parameters.vignetteStrength << " | film=" << parameters.filmGrainIntensity;
        ri::core::LogInfo(summary.str());
        ri::core::LogInfo("    " + std::string(definition.summary));
    }
}

void PrintVulkanDiagnostics() {
    const VulkanToolingDiagnostics tooling = CollectVulkanToolingDiagnostics();
    std::string platformName = "Unknown";
#if defined(_WIN32)
    platformName = "Windows";
#elif defined(__linux__)
    platformName = "Linux";
#elif defined(__APPLE__)
    platformName = "macOS";
#endif
    ri::core::LogInfo("Vulkan platform: " + platformName);
    ri::core::LogInfo("Vulkan SDK root: " + (tooling.sdkRoot.empty() ? std::string("<not set>") : tooling.sdkRoot));

    ri::render::vulkan::VulkanBootstrapSummary diagnostics{};
    try {
        diagnostics = ri::render::vulkan::RunBootstrap(ri::render::vulkan::VulkanBootstrapOptions{
            .windowTitle = "ri_tool Vulkan Diagnostics",
            .createSurface = false,
        });
    } catch (const std::exception& error) {
        ri::core::LogInfo("Vulkan runtime status: unavailable");
        ri::core::LogInfo("Vulkan runtime detail: " + std::string(error.what()));
        return;
    }

    ri::core::LogInfo("Vulkan runtime status: ready");
    ri::core::LogInfo("Vulkan runtime library: " + diagnostics.loaderPath);
    ri::core::LogInfo("Vulkan instance API version: " +
                      (diagnostics.instanceApiVersion.empty() ? std::string("<unknown>") : diagnostics.instanceApiVersion));
    ri::core::LogInfo("Vulkan surface status: " + diagnostics.surfaceStatus);
    ri::core::LogInfo("Vulkan validation layer: " + std::string(diagnostics.validationLayerAvailable ? "available" : "missing"));
    ri::core::LogInfo("Vulkan instance extensions: " + std::to_string(diagnostics.instanceExtensions.size()));
    for (const std::string& extension : diagnostics.instanceExtensions) {
        ri::core::LogInfo("  ext " + extension);
    }
    ri::core::LogInfo("Vulkan instance layers: " + std::to_string(diagnostics.instanceLayers.size()));
    for (const std::string& layer : diagnostics.instanceLayers) {
        ri::core::LogInfo("  layer " + layer);
    }
    ri::core::LogInfo("Vulkan selected device: " +
                      (diagnostics.selectedDeviceName.empty() ? std::string("<none>") : diagnostics.selectedDeviceName));
    for (const ri::render::vulkan::VulkanDeviceSummary& device : diagnostics.devices) {
        ri::core::LogInfo("  device " + device.name +
                          " type=" + device.type +
                          " api=" + device.apiVersion +
                          " vendor=" + FormatHex32(device.vendorId) +
                          " device=" + FormatHex32(device.deviceId) +
                          " graphicsQueues=" + std::to_string(device.graphicsQueueFamilyCount) +
                          " presentQueues=" + std::to_string(device.presentQueueFamilyCount) +
                          " present=" + std::string(device.presentSupport ? "yes" : "no"));
    }
    ri::core::LogInfo("SDK tools:");
    if (tooling.availableTools.empty()) {
        ri::core::LogInfo("  <none discovered>");
    } else {
        for (const std::string& toolPath : tooling.availableTools) {
            ri::core::LogInfo("  " + toolPath);
        }
    }
}

void RenderCubePreviewToFile(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    fs::path outputPath = workspace.saved / "Previews" / "rawiron_shaded_cube.bmp";
    if (const auto outputArg = commandLine.GetValue("--output"); outputArg.has_value()) {
        outputPath = fs::path(*outputArg);
    }

    EnsureParentDirectoryExists(outputPath);
    const ri::render::software::ScenePreviewOptions previewOptions =
        BuildScenePreviewOptions(commandLine, 512, 512);

    const ri::scene::SceneKitPreview preview = ri::scene::BuildLitCubeSceneKitPreview();
    const ri::render::software::SoftwareImage image = ri::render::software::RenderScenePreview(
        preview.scene,
        preview.orbitCamera.cameraNode,
        previewOptions);
    if (!ri::render::software::SaveBmp(image, outputPath.string())) {
        throw std::runtime_error("Failed to write shaded cube preview.");
    }

    ri::core::LogInfo("Rendered shaded cube preview.");
    ri::core::LogInfo("  Output: " + outputPath.string());
    ri::core::LogInfo("  Size: " + std::to_string(image.width) + "x" + std::to_string(image.height));
}

void RenderSceneKitExampleToFile(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    const auto slugArg = commandLine.GetValue("--scenekit-example");
    if (!slugArg.has_value()) {
        throw std::runtime_error("Missing Scene Kit example slug.");
    }

    fs::path outputPath = workspace.saved / "Previews" / "SceneKit" / (*slugArg + ".bmp");
    if (const auto outputArg = commandLine.GetValue("--output"); outputArg.has_value()) {
        outputPath = fs::path(*outputArg);
    }
    EnsureParentDirectoryExists(outputPath);
    const ri::render::software::ScenePreviewOptions previewOptions =
        BuildScenePreviewOptions(commandLine, 512, 512);

    const std::optional<ri::scene::SceneKitMilestoneResult> result = ri::scene::BuildSceneKitMilestone(
        *slugArg,
        ri::scene::SceneKitMilestoneOptions{
            .assetRoot = workspace.assetsSource,
        });
    if (!result.has_value()) {
        throw std::runtime_error("Unknown Scene Kit example slug: " + *slugArg);
    }

    const ri::render::software::SoftwareImage image =
        ri::render::software::RenderScenePreview(result->scene, result->cameraNode, previewOptions);
    if (!ri::render::software::SaveBmp(image, outputPath.string())) {
        throw std::runtime_error("Failed to write Scene Kit example preview.");
    }

    ri::core::LogInfo("Rendered Scene Kit example: " + result->slug);
    ri::core::LogInfo("  Title: " + result->title);
    ri::core::LogInfo("  Status: " + result->statusLabel);
    ri::core::LogInfo("  Detail: " + result->detail);
    ri::core::LogInfo("  Output: " + outputPath.string());
}

void RunSceneKitChecks(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    fs::path outputDirectory = workspace.saved / "Previews" / "SceneKit";
    if (const auto outputArg = commandLine.GetValue("--output-dir"); outputArg.has_value()) {
        outputDirectory = fs::path(*outputArg);
    }
    fs::create_directories(outputDirectory);

    const ri::render::software::ScenePreviewOptions previewOptions =
        BuildScenePreviewOptions(commandLine, 512, 512);

    ri::scene::SceneKitMilestoneCallbacks callbacks{};
    callbacks.renderValidator = [&](const std::string& slug, const ri::scene::Scene& scene, int cameraNode, std::string& detail) {
        const ri::render::software::SoftwareImage image =
            ri::render::software::RenderScenePreview(scene, cameraNode, previewOptions);
        const fs::path outputPath = outputDirectory / (slug + ".bmp");
        if (!ri::render::software::SaveBmp(image, outputPath.string())) {
            detail += " | preview save failed";
            return false;
        }

        detail += " | preview=" + outputPath.string();
        return true;
    };

    const std::vector<ri::scene::SceneKitMilestoneResult> results = ri::scene::RunSceneKitMilestoneChecks(
        ri::scene::SceneKitMilestoneOptions{
            .assetRoot = workspace.assetsSource,
        },
        callbacks);

    ri::core::LogInfo("Scene Kit milestone checks:");
    int passedCount = 0;
    for (const ri::scene::SceneKitMilestoneResult& result : results) {
        ri::core::LogInfo(std::string(result.passed ? "[PASS] " : "[FAIL] ") + result.title);
        ri::core::LogInfo("  " + result.detail);
        if (result.passed) {
            ++passedCount;
        }
    }

    ri::core::LogInfo("Scene Kit checks summary: " +
                      std::to_string(passedCount) + "/" + std::to_string(results.size()) + " passed");
    if (!ri::scene::AllSceneKitMilestonesPassed(results)) {
        throw std::runtime_error("One or more Scene Kit milestone checks failed.");
    }
}

fs::path ResolveSceneStatePath(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (const auto stateArg = commandLine.GetValue("--state-file"); stateArg.has_value()) {
        return fs::path(*stateArg);
    }
    return workspace.saved / "Tooling" / "scene_state.ri_state";
}

void SaveToolSceneState(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    ri::scene::StarterScene starterScene = ri::scene::BuildStarterScene("ToolStateScene");
    const fs::path outputPath = ResolveSceneStatePath(workspace, commandLine);
    if (!ri::scene::SaveSceneNodeTransforms(starterScene.scene, outputPath)) {
        throw std::runtime_error("Failed to save scene state to: " + outputPath.string());
    }

    ri::core::LogInfo("Saved scene state.");
    ri::core::LogInfo("  Output: " + outputPath.string());
    ri::core::LogInfo("  Nodes: " + std::to_string(starterScene.scene.NodeCount()));
}

void LoadToolSceneState(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    ri::scene::StarterScene starterScene = ri::scene::BuildStarterScene("ToolStateScene");
    const fs::path inputPath = ResolveSceneStatePath(workspace, commandLine);
    if (!ri::scene::LoadSceneNodeTransforms(starterScene.scene, inputPath)) {
        throw std::runtime_error("Failed to load scene state from: " + inputPath.string());
    }

    ri::core::LogInfo("Loaded scene state.");
    ri::core::LogInfo("  Input: " + inputPath.string());
    ri::core::LogInfo("  Scene: " + starterScene.scene.GetName());
    ri::core::LogInfo("  Nodes: " + std::to_string(starterScene.scene.NodeCount()));
}

} // namespace

int main(int argc, char** argv) {
    try {
        ri::core::CommandLine commandLine(argc, argv);
        const WorkspaceLayout workspace = BuildWorkspaceLayout(DetectWorkspaceRoot(commandLine));

        ri::core::LogSection("ri_tool");
        ri::core::LogInfo(std::string(ri::core::kEngineName) + " tools bootstrap");

        if (commandLine.HasFlag("--help") || commandLine.HasFlag("-h")) {
            PrintToolHelp();
            return 0;
        }

        if (commandLine.HasFlag("--version") && !commandLine.GetValue("--version").has_value()) {
            ri::core::LogInfo(std::string(ri::core::kEngineName) + " "
                              + std::to_string(ri::core::kVersionMajor) + "."
                              + std::to_string(ri::core::kVersionMinor) + "."
                              + std::to_string(ri::core::kVersionPatch));
            return 0;
        }

        ValidateSinglePrimaryCommand(commandLine);

        if (commandLine.HasFlag("--workspace")) {
            PrintWorkspace(workspace);
            return 0;
        }

        if (commandLine.HasFlag("--list-projects")) {
            PrintRawIronProjects(workspace.root);
            return 0;
        }

        if (commandLine.HasFlag("--ensure-workspace")) {
            EnsureWorkspace(workspace);
            ri::core::LogInfo("Workspace ensured.");
            PrintWorkspace(workspace);
            return 0;
        }

        if (CommandRequested(commandLine, "--create-project")) {
            CreateRawIronProject(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--describe-project")) {
            std::string error;
            const std::optional<ProjectCommandContext> context =
                ResolveProjectCommandContext(commandLine, workspace, error);
            if (!context.has_value()) {
                throw std::runtime_error(error);
            }
            DescribeRawIronProject(*context);
            return 0;
        }

        if (CommandRequested(commandLine, "--doctor-project")) {
            std::string error;
            const std::optional<ProjectCommandContext> context =
                ResolveProjectCommandContext(commandLine, workspace, error);
            if (!context.has_value()) {
                throw std::runtime_error(error);
            }
            return DoctorRawIronProject(*context) ? 0 : 1;
        }

        if (commandLine.HasFlag("--list-project-resources")) {
            std::string error;
            const std::optional<ProjectCommandContext> context =
                ResolveProjectCommandContext(commandLine, workspace, error);
            if (!context.has_value()) {
                throw std::runtime_error(error);
            }
            ListRawIronProjectResources(*context, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--scaffold-project")) {
            std::string error;
            const std::optional<ProjectCommandContext> context =
                ResolveProjectCommandContext(commandLine, workspace, error);
            if (!context.has_value()) {
                throw std::runtime_error(error);
            }
            ScaffoldRawIronProject(*context);
            return 0;
        }

        if (commandLine.HasFlag("--formats")) {
            ri::core::LogInfo("RawIron standard asset format:");
            ri::core::LogInfo("  .ri_asset.json  (single unified document for mesh/material/texture/audio/scene/behavior)");
            ri::core::LogInfo("  .ri_package.json  (portable asset/resource package manifest)");
            ri::core::LogInfo("  .ripak  (ZIP-compatible RawIron package archive containing package.ri_package.json)");
            ri::core::LogInfo("  .ri_rig.json  (portable skeleton/rest-pose source with humanoid validation)");
            ri::core::LogInfo("  .ri_sculpt.json  (Forge clay cage + skin weights)");
            ri::core::LogInfo("  .ri_anim.json  (Forge motion clip bound to a rig)");
            ri::core::LogInfo("  .riscript  (RawIron-owned Lua-like scripting language for behavior/config/tests)");
            ri::core::LogInfo("Third-party authoring/import inputs:");
            ri::core::LogInfo("  .blend  (Blender authoring source; export/rebuild into RawIron mesh/material outputs before shipping)");
            ri::core::LogInfo("Legacy/experimental aliases:");
            ri::core::LogInfo("  .ri_model .ri_mesh .ri_scene .ri_mat .ri_tex .ri_audio .ri_meshc");
            return 0;
        }

        if (commandLine.HasFlag("--plugins-list") || commandLine.HasFlag("--plugins-doctor")) {
            std::string error;
            const std::optional<ProjectCommandContext> context =
                ResolveProjectCommandContext(commandLine, workspace, error);
            if (!context.has_value()) {
                throw std::runtime_error(error);
            }
            const bool doctorMode = commandLine.HasFlag("--plugins-doctor");
            const bool healthy = InspectProjectPlugins(*context, doctorMode);
            return doctorMode && !healthy ? 1 : 0;
        }

        if (commandLine.HasFlag("--plugin-handlers")) {
            PrintPluginHandlers();
            return 0;
        }

        if (CommandRequested(commandLine, "--extension-validate")) {
            return ValidateExtensionFile(commandLine) ? 0 : 1;
        }

        if (CommandRequested(commandLine, "--plugin-package-validate")) {
            return ValidatePluginPackageFile(commandLine) ? 0 : 1;
        }

        if (CommandRequested(commandLine, "--plugin-package-build")) {
            BuildPluginPackage(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--rig-toolchain-report")) {
            PrintRigToolchainReport();
            return 0;
        }

        if (CommandRequested(commandLine, "--rig-create-humanoid")) {
            CreateHumanoidRig(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--rig-validate")) {
            ValidateRig(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--forge-report")) {
            PrintForgeToolchainReport(workspace);
            return 0;
        }

        if (commandLine.HasFlag("--forge-assets-list")) {
            ListForgeAssets(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--forge-character-create")) {
            CreateForgeCharacter(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--blockchar-create")) {
            CreateBlockCharacter(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-add-part")) {
            AddBlockCharacterPart(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-set-part")) {
            SetBlockCharacterPart(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-nudge")) {
            NudgeBlockCharacterPart(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-remove-part")) {
            RemoveBlockCharacterPart(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-list-shapes")) {
            ListBlockCharacterShapes();
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-duplicate")) {
            DuplicateBlockCharacterPart(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-mirror")) {
            MirrorBlockCharacterPart(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-inset")) {
            InsetBlockCharacterDetail(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-attach")) {
            AttachBlockCharacterDetail(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-recolor")) {
            RecolorBlockCharacterParts(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-bevel")) {
            BevelBlockCharacterPart(workspace, commandLine);
            return 0;
        }
        if (CommandRequested(commandLine, "--blockchar-report")) {
            ReportBlockCharacter(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-write-textures")) {
            WriteBlockCharacterTextures(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--blockchar-sync-sculpt")) {
            SyncBlockCharacterSculpt(workspace, commandLine);
            return 0;
        }
        if (commandLine.HasFlag("--functions-list")) {
            ListToolFunctions(workspace);
            return 0;
        }
        if (CommandRequested(commandLine, "--function")) {
            RunToolFunction(workspace, commandLine);
            return 0;
        }
        if (CommandRequested(commandLine, "--anim-author-idle")) {
            AuthorHumanoidMotionClip(workspace, commandLine, false);
            return 0;
        }
        if (CommandRequested(commandLine, "--anim-author-walk")) {
            AuthorHumanoidMotionClip(workspace, commandLine, true);
            return 0;
        }

        if (CommandRequested(commandLine, "--forge-handoff-probe")) {
            ProbeForgeHandoff(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--sculpt-create")) {
            CreateNativeSculptAsset(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-bind")) {
            BindSculptToRig(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-fit-rig")) {
            FitSculptToRigCommand(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-flood-weights")) {
            FloodSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-soft-bone")) {
            SoftSculptBoneWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-cap4")) {
            CapSculptInfluences(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-audit-weights")) {
            AuditSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-unbind")) {
            UnbindSculptFromRig(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-clear-weights")) {
            ClearSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-normalize")) {
            NormalizeSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-prune")) {
            PruneSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-mirror")) {
            MirrorSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-seal")) {
            SealSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-invert")) {
            InvertSculptBoneWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-ceil")) {
            CeilSculptBoneWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-harden")) {
            HardenSculptBoneWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-flood-free")) {
            FloodUnboundSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-soft")) {
            SoftenAllSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-transfer")) {
            TransferSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-swap")) {
            SwapSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-grow")) {
            GrowSculptBoneWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-shrink")) {
            ShrinkSculptBoneWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-paint")) {
            PaintSculptWeights(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-bounds")) {
            ReportSculptBounds(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sculpt-bone-weights")) {
            ReportSculptBoneWeights(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--sculpt-validate")) {
            ValidateNativeSculptAsset(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--anim-create")) {
            CreateNativeAnimationAsset(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--anim-key-rest")) {
            KeyAnimationRest(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--anim-key")) {
            UpsertAnimationKey(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--anim-hold")) {
            HoldAnimationPose(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--anim-breakdown")) {
            BreakdownAnimationPose(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--anim-event")) {
            UpsertAnimationEvent(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--anim-trim")) {
            TrimAnimationClip(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--anim-fit-duration")) {
            FitAnimationDuration(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--anim-align-start")) {
            AlignAnimationStart(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--anim-validate")) {
            ValidateNativeAnimationAsset(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--anim-report")) {
            ReportAnimationClip(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--asset-standardize")) {
            StandardizeSingleAsset(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--asset-standardize-dir")) {
            StandardizeAssetDirectory(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--asset-package-build")) {
            BuildAssetPackage(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--asset-package-validate")) {
            ValidateAssetPackage(commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--asset-package-import")) {
            ImportAssetPackage(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--asset-package-install")) {
            InstallAssetPackage(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--asset-package-resolve")) {
            ResolveAssetPackageGraph(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--asset-package-mount-check")) {
            CheckAssetPackageMount(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--game-package-mount-check")) {
            CheckGamePackageMounts(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--scenekit-targets")) {
            PrintSceneKitTargets();
            return 0;
        }

        if (commandLine.HasFlag("--postprocess-presets")) {
            PrintPostProcessPresets();
            return 0;
        }

        if (commandLine.HasFlag("--scenekit-checks")) {
            RunSceneKitChecks(workspace, commandLine);
            return 0;
        }

        if (CommandRequested(commandLine, "--scenekit-example")) {
            RenderSceneKitExampleToFile(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--vulkan-diagnostics")) {
            PrintVulkanDiagnostics();
            return 0;
        }

        if (commandLine.HasFlag("--render-cube")) {
            RenderCubePreviewToFile(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--sample-scene")) {
            const ri::scene::StarterScene starterScene = ri::scene::BuildStarterScene("ToolPreview");
            ri::core::LogInfo(starterScene.scene.Describe());
            ri::core::LogInfo("Renderable nodes: " + std::to_string(ri::scene::CollectRenderableNodes(starterScene.scene).size()));
            ri::core::LogInfo("Orbit camera path: " + ri::scene::DescribeNodePath(starterScene.scene, starterScene.handles.orbitCamera.cameraNode));
            const std::optional<ri::scene::RaycastHit> crateHit = ri::scene::RaycastSceneNearest(
                starterScene.scene,
                ri::scene::Ray{
                    .origin = ri::math::Vec3{0.0f, 0.5f, -5.0f},
                    .direction = ri::math::Vec3{0.0f, 0.0f, 1.0f},
                });
            if (crateHit.has_value()) {
                ri::core::LogInfo("Starter-scene raycast hit: " + starterScene.scene.GetNode(crateHit->node).name +
                                  " at " + ri::math::ToString(crateHit->position));
            }
            return 0;
        }

        if (commandLine.HasFlag("--save-scene-state")) {
            SaveToolSceneState(workspace, commandLine);
            return 0;
        }

        if (commandLine.HasFlag("--load-scene-state")) {
            LoadToolSceneState(workspace, commandLine);
            return 0;
        }

        PrintToolHelp();
        return 0;
    } catch (const std::exception& exception) {
        ri::core::LogSection("ri_tool Failure");
        ri::core::LogInfo(exception.what());
        return 1;
    }
}
