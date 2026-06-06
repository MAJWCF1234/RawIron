#include "RawIron/Core/CommandLine.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/AssetDocument.h"
#include "RawIron/Content/AssetPackageManifest.h"
#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Core/Version.h"
#include "RawIron/Render/PostProcessProfiles.h"
#include "RawIron/Render/VulkanBootstrap.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/SoftwarePreview.h"
#include "RawIron/Scene/Raycast.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Scene/SceneKit.h"
#include "RawIron/Scene/SceneStateIO.h"
#include "RawIron/Scene/SceneUtils.h"
#include "EditorProjectScaffolding.h"
#include "EditorWorkspace.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

    if (extension == ".asset") {
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

fs::path ExtractRipakArchiveToTemp(const fs::path& archivePath) {
    const fs::path absoluteArchive = fs::weakly_canonical(archivePath);
    const fs::path extractRoot = UniquePackageTempDirectory("extract", absoluteArchive);
    const fs::path zipScratch = extractRoot.parent_path() / (extractRoot.filename().string() + ".zip");
    fs::create_directories(extractRoot.parent_path());

    std::error_code ec{};
    fs::copy_file(absoluteArchive, zipScratch, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        throw std::runtime_error("Failed to stage .ripak archive for extraction: " + ec.message());
    }

    const std::string script =
        "if (Test-Path -LiteralPath " + QuotePowerShellLiteral(extractRoot) + ") { Remove-Item -LiteralPath "
        + QuotePowerShellLiteral(extractRoot) + " -Recurse -Force }; "
        + "New-Item -ItemType Directory -Force -Path " + QuotePowerShellLiteral(extractRoot) + " | Out-Null; "
        + "Expand-Archive -LiteralPath " + QuotePowerShellLiteral(zipScratch)
        + " -DestinationPath " + QuotePowerShellLiteral(extractRoot) + " -Force; "
        + "Remove-Item -LiteralPath " + QuotePowerShellLiteral(zipScratch) + " -Force";
    RunPowerShellArchiveCommand(script, "extract");
    return extractRoot;
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
    const fs::path packageRoot = IsRipakArchivePath(fs::path(*packageArg))
        ? ExtractRipakArchiveToTemp(fs::path(*packageArg))
        : fs::path(*packageArg);
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
    ri::core::LogInfo("  Assets: " + std::to_string(manifest->assets.size()));
}

fs::path ResolveProjectRootOption(const WorkspaceLayout& workspace, const ri::core::CommandLine& commandLine) {
    if (const auto projectArg = commandLine.GetValue("--project"); projectArg.has_value() && !projectArg->empty()) {
        return fs::weakly_canonical(fs::path(*projectArg));
    }
    return workspace.root;
}

std::optional<ri::content::InstalledAssetPackage> LoadValidatedPackageForInstall(const fs::path& packageArg) {
    const fs::path packageRoot = IsRipakArchivePath(packageArg)
        ? ExtractRipakArchiveToTemp(packageArg)
        : packageArg;
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
    std::optional<ri::content::InstalledAssetPackage> package =
        LoadValidatedPackageForInstall(fs::path(*packageArg));
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
    std::optional<ri::content::InstalledAssetPackage> package =
        LoadValidatedPackageForInstall(fs::path(*packageArg));
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
    int copiedCount = 0;
    for (const ri::content::AssetPackageEntry& asset : package->manifest.assets) {
        const fs::path source = package->packageRoot / fs::path(asset.path);
        const fs::path relativeDestination = asset.installPath.empty()
            ? DefaultProjectInstallPath(package->manifest, asset)
            : fs::path(asset.installPath);
        CopyFileChecked(source, projectRoot / relativeDestination);
        ++copiedCount;
    }

    const fs::path receiptPath = projectRoot / "assets" / "package_receipts" /
        (package->manifest.packageId + ".ri_package.json");
    CopyFileChecked(package->manifestPath, receiptPath);

    ri::core::LogInfo("RawIron package installed into project.");
    ri::core::LogInfo("  Project root: " + projectRoot.string());
    ri::core::LogInfo("  Package: " + package->manifest.packageId);
    ri::core::LogInfo("  Installed assets: " + std::to_string(copiedCount));
    ri::core::LogInfo("  Receipt: " + receiptPath.string());
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
    ri::render::vulkan::VulkanBootstrapSummary diagnostics{};
    try {
        diagnostics = ri::render::vulkan::RunBootstrap(ri::render::vulkan::VulkanBootstrapOptions{
            .windowTitle = "ri_tool Vulkan Diagnostics",
            .createSurface = false,
        });
    } catch (const std::exception&) {
        ri::core::LogInfo("Vulkan diagnostics unavailable. The platform Vulkan runtime could not be loaded.");
        return;
    }

    ri::core::LogInfo("Vulkan platform: " + diagnostics.platformName);
    ri::core::LogInfo("Vulkan SDK root: " + (tooling.sdkRoot.empty() ? std::string("<not set>") : tooling.sdkRoot));
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

        if (commandLine.GetValue("--create-project").has_value()) {
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

        if (commandLine.GetValue("--doctor-project").has_value()) {
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
            ri::core::LogInfo("  .riscript  (RawIron-owned Lua-like scripting language for behavior/config/tests)");
            ri::core::LogInfo("Third-party authoring/import inputs:");
            ri::core::LogInfo("  .blend  (Blender authoring source; export/rebuild into RawIron mesh/material outputs before shipping)");
            ri::core::LogInfo("Legacy/experimental aliases:");
            ri::core::LogInfo("  .ri_model .ri_mesh .ri_scene .ri_mat .ri_tex .ri_audio .ri_meshc");
            return 0;
        }

        if (commandLine.GetValue("--asset-standardize").has_value()) {
            StandardizeSingleAsset(workspace, commandLine);
            return 0;
        }

        if (commandLine.GetValue("--asset-standardize-dir").has_value()) {
            StandardizeAssetDirectory(workspace, commandLine);
            return 0;
        }

        if (commandLine.GetValue("--asset-package-build").has_value()) {
            BuildAssetPackage(workspace, commandLine);
            return 0;
        }

        if (commandLine.GetValue("--asset-package-validate").has_value()) {
            ValidateAssetPackage(commandLine);
            return 0;
        }

        if (commandLine.GetValue("--asset-package-import").has_value()) {
            ImportAssetPackage(workspace, commandLine);
            return 0;
        }

        if (commandLine.GetValue("--asset-package-install").has_value()) {
            InstallAssetPackage(workspace, commandLine);
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

        if (commandLine.GetValue("--scenekit-example").has_value()) {
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

        ri::core::LogInfo("Planned commands:");
        ri::core::LogInfo("  ri_tool --workspace");
        ri::core::LogInfo("  ri_tool --list-projects [--root <workspace-root>]");
        ri::core::LogInfo("  ri_tool --create-project <slug-id> [--name <display>] [--author <label>] [--type <game|test-game|experience>] [--version <semver>] [--description <text>] [--game-root <path>]");
        ri::core::LogInfo("  ri_tool --describe-project --game <id> | --game-root <path>");
        ri::core::LogInfo("  ri_tool --doctor-project --game <id> | --game-root <path>");
        ri::core::LogInfo("  ri_tool --list-project-resources --game <id> | --game-root <path> [--resource-category <name>]");
        ri::core::LogInfo("  ri_tool --scaffold-project --game <id> | --game-root <path>");
        ri::core::LogInfo("  ri_tool --ensure-workspace");
        ri::core::LogInfo("  ri_tool --formats");
        ri::core::LogInfo("  ri_tool --scenekit-targets");
        ri::core::LogInfo("  ri_tool --scenekit-checks [--output-dir <path>] [--width <px>] [--height <px>]");
        ri::core::LogInfo("  ri_tool --scenekit-example <slug> [--output <path>] [--width <px>] [--height <px>]");
        ri::core::LogInfo("  ri_tool --vulkan-diagnostics");
        ri::core::LogInfo("  ri_tool --render-cube [--output <path>] [--width <px>] [--height <px>]");
        ri::core::LogInfo("  ri_tool --sample-scene");
        ri::core::LogInfo("  ri_tool --save-scene-state [--state-file <path>]");
        ri::core::LogInfo("  ri_tool --load-scene-state [--state-file <path>]");
        ri::core::LogInfo("  ri_tool --asset-standardize <source-path> [--output <file.ri_asset.json>]");
        ri::core::LogInfo("  ri_tool --asset-standardize-dir <source-dir> [--output-dir <directory>]");
        ri::core::LogInfo("  ri_tool --asset-package-build <source-dir> [--output-dir <package-dir-or-file.ripak>] [--package <file.ripak-or-manifest>]");
        ri::core::LogInfo("  ri_tool --asset-package-validate <file.ripak-or-package-dir-or-manifest>");
        ri::core::LogInfo("  ri_tool --asset-package-import <file.ripak-or-package-dir-or-manifest> [--project <project-root>] [--output-dir <mounted-package-dir>]");
        ri::core::LogInfo("  ri_tool --asset-package-install <file.ripak-or-package-dir-or-manifest> [--project <project-root>]");
        return 0;
    } catch (const std::exception& exception) {
        ri::core::LogSection("ri_tool Failure");
        ri::core::LogInfo(exception.what());
        return 1;
    }
}
