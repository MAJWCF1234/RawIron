#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/GameRuntimeSupport.h"
#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Core/CrashDiagnostics.h"
#include "RawIron/Core/Host.h"
#include "RawIron/Core/Log.h"
#include "RawIron/Core/MainLoop.h"
#include "RawIron/Math/Vec3.h"
#include "RawIron/Editor/BundledGamePreviews.h"
#include "EditorFilesInspector.h"
#include "EditorInput.h"
#include "EditorInspectorPanels.h"
#include "EditorLeftPanel.h"
#include "EditorPlaytestLauncher.h"
#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "EditorHierarchy.h"
#include "EditorProjectScaffolding.h"
#include "EditorResourceBrowser.h"
#include "EditorResourceDocument.h"
#include "EditorRenderer.h"
#include "EditorResourceTextEditor.h"
#include "EditorSceneController.h"
#include "EditorViewportRenderer.h"
#include "EditorWorkspace.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Runtime/ExperiencePresets.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/PrimitivesCsvIO.h"
#include "RawIron/Scene/SceneStateIO.h"
#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Ui/UiJsonIO.h"
#include "RawIron/Ui/UiPaths.h"
#include "RawIron/World/Instrumentation.h"
#include "RawIron/World/InventoryState.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
#include <set>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

namespace fs = std::filesystem;
using ri::editor::CollectWorkspaceGameResources;
using ri::editor::ComputeVisibleResourceScrollTop;
using ri::editor::EnsureMountedGameScaffold;
using ri::editor::EnsureProjectDevConfig;
using ri::editor::EnumerateWorkspaceGames;
using ri::editor::FindResourceRowByRelativePath;
using ri::editor::IsLikelyTextResourcePath;
using ri::editor::BuildFilteredHierarchyDrawOrder;
using ri::editor::BuildHierarchyDrawOrder;
using ri::editor::BuildFilesInspectorPanelModel;
using ri::editor::ComputeVisibleHierarchyScrollTop;
using ri::editor::ComputeGameplayPanelLayout;
using ri::editor::ComputeUiWorkbenchLayout;
using ri::editor::FindDrawOrderIndex;
using ri::editor::LoadResourceDocument;
using ri::editor::ComputeProjectShortcutLayout;
using ri::editor::ProjectShortcutLayout;
using ri::editor::EditorRenderer;
using ri::editor::EditorSceneControllerContext;
using ri::editor::NodeInspectorPanelModel;
using ri::editor::BrushInspectorPanelModel;
using ri::editor::GameplayInspectorPanelModel;
using ri::editor::GameplayPanelLayout;
using ri::editor::UiWorkbenchPanelModel;
using ri::editor::UiWorkbenchPreviewBlock;
using ri::editor::UiWorkbenchScreenSummary;
using ri::editor::UiWorkbenchBlockTone;
using ri::editor::UiWorkbenchLayout;
using ri::editor::DestroyResourceTextEditorControl;
using ri::editor::EnsureResourceTextEditorCreated;
using ri::editor::LayoutResourceTextEditorControl;
using ri::editor::OpenActiveResourceInExplorer;
using ri::editor::RenderBrushInspectorPanel;
using ri::editor::RenderGameplayInspectorPanel;
using ri::editor::RenderNodeInspectorPanel;
using ri::editor::RenderUiWorkbenchPanel;
using ri::editor::ResolveDirtyResourceBeforeContextSwitch;
using ri::editor::SaveResourceDocumentUtf8;
using ri::editor::SaveActiveResourceFileFromEditor;
using ri::editor::WorkspaceCategoryBit;
using ri::editor::WorkspaceCategoryLabel;
using ri::editor::WorkspaceCategoryShortLabel;
using ri::editor::WorkspaceGameEntry;
using ri::editor::WorkspaceResourceCategory;
using ri::editor::WorkspaceResourceEntry;
using ri::editor::TryCreateGroupNode;
using ri::editor::TryDeleteSelectedNode;
using ri::editor::TryDuplicateSelectedNode;
using ri::editor::TryGroupSelectedNode;
using ri::editor::TryReparentSelectedToWorldRoot;
using ri::editor::TryResetSelectedTransform;
using ri::editor::TrySelectAdjacentAuthoredNode;
using ri::editor::TrySnapSelectedNodeToGrid;
using ri::editor::TryUngroupSelectedNode;
using ri::content::DescribeOptionalAssetState;

[[nodiscard]] std::string ToLowerAsciiCopy(std::string_view text) {
    std::string lowered(text);
    for (char& ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

double ResolveFixedDeltaSeconds(const ri::core::CommandLine& commandLine, int fallbackTickHz) {
    const std::optional<int> tickHz = commandLine.TryGetInt("--tick-hz");
    if (!tickHz.has_value() || *tickHz <= 0) {
        return 1.0 / static_cast<double>(fallbackTickHz);
    }
    return 1.0 / static_cast<double>(*tickHz);
}

struct EditorSceneConfig {
    fs::path workspaceRoot;
    fs::path sceneStatePath;
    std::optional<ri::content::GameManifest> gameManifest;
    /// From manifest `editorPreviewScene`, or `"starter"` when absent / no project loaded.
    std::string editorPreviewScene = "starter";
    std::string sceneName = "EditorWorkspace";
    std::string windowTitle = "RawIron Editor";
    std::string workspaceLabel = "Authoring (no game manifest)";
    std::string statusMessage;
};

bool LooksLikeWorkspaceRoot(const fs::path& path) {
    std::error_code ec{};
    return fs::exists(path / "CMakeLists.txt", ec)
        && fs::exists(path / "Source", ec)
        && fs::exists(path / "Games", ec);
}

fs::path BuildEditorSceneStatePath(const fs::path& workspaceRoot, std::string_view sceneId) {
    return workspaceRoot / "Saved" / "Editor" / std::string(sceneId) / "scene_state.ri_state";
}

fs::path ResolveEditorWorkspaceRoot(const fs::path& fallbackWorkspaceRoot,
                                    const std::optional<ri::content::GameManifest>& manifest,
                                    const std::optional<fs::path>& explicitGameRoot) {
    if (explicitGameRoot.has_value()) {
        const fs::path detected = ri::content::DetectWorkspaceRoot(*explicitGameRoot);
        if (LooksLikeWorkspaceRoot(detected)) {
            return detected;
        }
        if (manifest.has_value()) {
            return manifest->rootPath;
        }
        return *explicitGameRoot;
    }

    if (manifest.has_value()) {
        const fs::path detected = ri::content::DetectWorkspaceRoot(manifest->rootPath);
        if (LooksLikeWorkspaceRoot(detected)) {
            return detected;
        }
        return manifest->rootPath;
    }

    return fallbackWorkspaceRoot;
}

[[nodiscard]] fs::path NormalizePathForConfig(std::string_view rawPath) {
    if (rawPath.empty()) {
        return {};
    }
    std::error_code ec{};
    fs::path normalized = fs::weakly_canonical(fs::path(rawPath), ec);
    if (ec) {
        // Preserve user-provided path when canonicalization fails so follow-up errors are still actionable.
        return fs::path(rawPath);
    }
    return normalized;
}

struct EditorAuthoredNodeRecord {
    std::string name;
    std::string parentName;
    ri::scene::Transform localTransform{};
    bool hasMesh = false;
    ri::scene::Mesh mesh{};
    bool hasMaterial = false;
    ri::scene::Material material{};
};

void WriteVec2(std::ostream& stream, const ri::math::Vec2& value) {
    stream << value.x << " " << value.y;
}

void WriteVec3(std::ostream& stream, const ri::math::Vec3& value) {
    stream << value.x << " " << value.y << " " << value.z;
}

bool ReadVec2(std::istream& stream, ri::math::Vec2& value) {
    return static_cast<bool>(stream >> value.x >> value.y);
}

bool ReadVec3(std::istream& stream, ri::math::Vec3& value) {
    return static_cast<bool>(stream >> value.x >> value.y >> value.z);
}

bool SaveEditorAuthoredSceneState(const ri::scene::Scene& scene,
                                  const std::size_t authoredNodeStart,
                                  const int editorTrashHandle,
                                  const fs::path& outputPath) {
    std::vector<EditorAuthoredNodeRecord> records;
    const std::size_t nodeCount = scene.NodeCount();
    for (std::size_t index = authoredNodeStart; index < nodeCount; ++index) {
        const ri::scene::Node& node = scene.GetNode(static_cast<int>(index));
        if (static_cast<int>(index) == editorTrashHandle || node.parent == editorTrashHandle) {
            continue;
        }

        EditorAuthoredNodeRecord record{};
        record.name = node.name;
        record.localTransform = node.localTransform;
        if (node.parent != ri::scene::kInvalidHandle
            && static_cast<std::size_t>(node.parent) < scene.NodeCount()) {
            record.parentName = scene.GetNode(node.parent).name;
        }
        if (node.mesh != ri::scene::kInvalidHandle && static_cast<std::size_t>(node.mesh) < scene.MeshCount()) {
            record.hasMesh = true;
            record.mesh = scene.GetMesh(node.mesh);
        }
        if (node.material != ri::scene::kInvalidHandle
            && static_cast<std::size_t>(node.material) < scene.MaterialCount()) {
            record.hasMaterial = true;
            record.material = scene.GetMaterial(node.material);
        }
        records.push_back(std::move(record));
    }

    fs::create_directories(outputPath.parent_path());
    std::ofstream stream(outputPath, std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }

    stream << "RAWIRON_EDITOR_AUTHORED_SCENE_V1\n";
    stream << "node_count " << records.size() << "\n";
    for (const EditorAuthoredNodeRecord& record : records) {
        stream << "node "
               << std::quoted(record.name) << " "
               << std::quoted(record.parentName) << " ";
        WriteVec3(stream, record.localTransform.position);
        stream << " ";
        WriteVec3(stream, record.localTransform.rotationDegrees);
        stream << " ";
        WriteVec3(stream, record.localTransform.scale);
        stream << " " << (record.hasMesh ? 1 : 0) << " " << (record.hasMaterial ? 1 : 0) << "\n";

        if (record.hasMesh) {
            stream << "mesh "
                   << std::quoted(record.mesh.name) << " "
                   << static_cast<int>(record.mesh.primitive) << " "
                   << record.mesh.vertexCount << " "
                   << record.mesh.indexCount << " "
                   << record.mesh.positions.size() << " ";
            for (const ri::math::Vec3& position : record.mesh.positions) {
                WriteVec3(stream, position);
                stream << " ";
            }
            stream << record.mesh.texCoords.size() << " ";
            for (const ri::math::Vec2& texCoord : record.mesh.texCoords) {
                WriteVec2(stream, texCoord);
                stream << " ";
            }
            stream << record.mesh.indices.size();
            for (const int index : record.mesh.indices) {
                stream << " " << index;
            }
            stream << "\n";
        }

        if (record.hasMaterial) {
            stream << "material "
                   << std::quoted(record.material.name) << " "
                   << static_cast<int>(record.material.shadingModel) << " ";
            WriteVec3(stream, record.material.baseColor);
            stream << " "
                   << std::quoted(record.material.baseColorTexture) << " "
                   << record.material.baseColorTextureFrames.size();
            for (const std::string& frame : record.material.baseColorTextureFrames) {
                stream << " " << std::quoted(frame);
            }
            stream << " " << record.material.baseColorTextureFramesPerSecond << " ";
            WriteVec2(stream, record.material.textureTiling);
            stream << " ";
            WriteVec3(stream, record.material.emissiveColor);
            stream << " "
                   << record.material.metallic << " "
                   << record.material.roughness << " "
                   << record.material.opacity << " "
                   << record.material.alphaCutoff << " "
                   << (record.material.doubleSided ? 1 : 0) << " "
                   << (record.material.transparent ? 1 : 0) << " "
                   << std::quoted(record.material.normalTexture) << " "
                   << std::quoted(record.material.ormTexture) << " "
                   << std::quoted(record.material.roughnessTexture) << " "
                   << std::quoted(record.material.metallicTexture) << " "
                   << std::quoted(record.material.emissiveTexture) << " "
                   << std::quoted(record.material.opacityTexture) << " "
                   << std::quoted(record.material.occlusionTexture) << "\n";
        }
    }

    return stream.good();
}

bool LoadEditorAuthoredSceneState(ri::scene::Scene& scene,
                                  const fs::path& inputPath,
                                  std::string* errorMessage) {
    std::ifstream stream(inputPath);
    if (!stream.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "authored scene sidecar missing";
        }
        return false;
    }

    std::string magic;
    std::getline(stream, magic);
    if (magic != "RAWIRON_EDITOR_AUTHORED_SCENE_V1") {
        if (errorMessage != nullptr) {
            *errorMessage = "invalid authored scene sidecar header";
        }
        return false;
    }

    std::string header;
    std::size_t nodeCount = 0;
    stream >> header >> nodeCount;
    if (!stream.good() || header != "node_count") {
        if (errorMessage != nullptr) {
            *errorMessage = "invalid authored scene sidecar count";
        }
        return false;
    }

    std::unordered_map<std::string, int> nodeByName;
    for (std::size_t index = 0; index < scene.NodeCount(); ++index) {
        nodeByName.emplace(scene.GetNode(static_cast<int>(index)).name, static_cast<int>(index));
    }

    std::size_t loaded = 0;
    while (loaded < nodeCount && stream.good()) {
        std::string token;
        stream >> token;
        if (!stream.good() || token != "node") {
            break;
        }

        EditorAuthoredNodeRecord record{};
        int hasMesh = 0;
        int hasMaterial = 0;
        stream >> std::quoted(record.name) >> std::quoted(record.parentName);
        if (!ReadVec3(stream, record.localTransform.position)
            || !ReadVec3(stream, record.localTransform.rotationDegrees)
            || !ReadVec3(stream, record.localTransform.scale)
            || !(stream >> hasMesh >> hasMaterial)) {
            if (errorMessage != nullptr) {
                *errorMessage = "malformed authored node record";
            }
            return false;
        }
        record.hasMesh = hasMesh != 0;
        record.hasMaterial = hasMaterial != 0;

        if (record.hasMesh) {
            std::string meshToken;
            std::size_t positionCount = 0;
            std::size_t texCoordCount = 0;
            std::size_t indexCount = 0;
            int primitive = 0;
            stream >> meshToken
                   >> std::quoted(record.mesh.name)
                   >> primitive
                   >> record.mesh.vertexCount
                   >> record.mesh.indexCount
                   >> positionCount;
            if (!stream.good() || meshToken != "mesh") {
                if (errorMessage != nullptr) {
                    *errorMessage = "malformed mesh record";
                }
                return false;
            }
            record.mesh.primitive = static_cast<ri::scene::PrimitiveType>(primitive);
            record.mesh.positions.resize(positionCount);
            for (ri::math::Vec3& position : record.mesh.positions) {
                if (!ReadVec3(stream, position)) {
                    if (errorMessage != nullptr) {
                        *errorMessage = "malformed mesh positions";
                    }
                    return false;
                }
            }
            stream >> texCoordCount;
            record.mesh.texCoords.resize(texCoordCount);
            for (ri::math::Vec2& texCoord : record.mesh.texCoords) {
                if (!ReadVec2(stream, texCoord)) {
                    if (errorMessage != nullptr) {
                        *errorMessage = "malformed mesh texcoords";
                    }
                    return false;
                }
            }
            stream >> indexCount;
            record.mesh.indices.resize(indexCount);
            for (int& indexValue : record.mesh.indices) {
                if (!(stream >> indexValue)) {
                    if (errorMessage != nullptr) {
                        *errorMessage = "malformed mesh indices";
                    }
                    return false;
                }
            }
        }

        if (record.hasMaterial) {
            std::string materialToken;
            std::size_t frameCount = 0;
            int shadingModel = 0;
            int doubleSided = 0;
            int transparent = 0;
            stream >> materialToken
                   >> std::quoted(record.material.name)
                   >> shadingModel;
            if (!stream.good() || materialToken != "material") {
                if (errorMessage != nullptr) {
                    *errorMessage = "malformed material record";
                }
                return false;
            }
            record.material.shadingModel = static_cast<ri::scene::ShadingModel>(shadingModel);
            if (!ReadVec3(stream, record.material.baseColor)
                || !(stream >> std::quoted(record.material.baseColorTexture) >> frameCount)) {
                if (errorMessage != nullptr) {
                    *errorMessage = "malformed material header";
                }
                return false;
            }
            record.material.baseColorTextureFrames.resize(frameCount);
            for (std::string& frame : record.material.baseColorTextureFrames) {
                if (!(stream >> std::quoted(frame))) {
                    if (errorMessage != nullptr) {
                        *errorMessage = "malformed material frame list";
                    }
                    return false;
                }
            }
            if (!(stream >> record.material.baseColorTextureFramesPerSecond)
                || !ReadVec2(stream, record.material.textureTiling)
                || !ReadVec3(stream, record.material.emissiveColor)
                || !(stream >> record.material.metallic
                     >> record.material.roughness
                     >> record.material.opacity
                     >> record.material.alphaCutoff
                     >> doubleSided
                     >> transparent
                     >> std::quoted(record.material.normalTexture)
                     >> std::quoted(record.material.ormTexture)
                     >> std::quoted(record.material.roughnessTexture)
                     >> std::quoted(record.material.metallicTexture)
                     >> std::quoted(record.material.emissiveTexture)
                     >> std::quoted(record.material.opacityTexture)
                     >> std::quoted(record.material.occlusionTexture))) {
                if (errorMessage != nullptr) {
                    *errorMessage = "malformed material payload";
                }
                return false;
            }
            record.material.doubleSided = doubleSided != 0;
            record.material.transparent = transparent != 0;
        }

        int parent = ri::scene::kInvalidHandle;
        if (!record.parentName.empty()) {
            const auto found = nodeByName.find(record.parentName);
            if (found == nodeByName.end()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "missing parent node '" + record.parentName + "'";
                }
                return false;
            }
            parent = found->second;
        }

        const int nodeHandle = scene.CreateNode(record.name, parent);
        ri::scene::Node& node = scene.GetNode(nodeHandle);
        node.localTransform = record.localTransform;
        if (record.hasMaterial) {
            const int materialHandle = scene.AddMaterial(record.material);
            if (record.hasMesh) {
                const int meshHandle = scene.AddMesh(record.mesh);
                scene.AttachMesh(nodeHandle, meshHandle, materialHandle);
            }
        } else if (record.hasMesh) {
            const int meshHandle = scene.AddMesh(record.mesh);
            scene.AttachMesh(nodeHandle, meshHandle);
        }

        nodeByName[record.name] = nodeHandle;
        ++loaded;
    }

    if (loaded != nodeCount) {
        if (errorMessage != nullptr) {
            *errorMessage = "authored scene sidecar truncated";
        }
        return false;
    }

    return true;
}

EditorSceneConfig ResolveSceneConfig(const ri::core::CommandLine& commandLine) {
    EditorSceneConfig config{};
    fs::path defaultWorkspaceRoot = ri::content::DetectWorkspaceRoot(fs::current_path());
    if (const auto workspaceRoot = commandLine.GetValue("--workspace-root");
        workspaceRoot.has_value() && !workspaceRoot->empty()) {
        defaultWorkspaceRoot = NormalizePathForConfig(*workspaceRoot);
    } else if (const auto workspace = commandLine.GetValue("--workspace");
               workspace.has_value() && !workspace->empty()) {
        // Launch scripts (and UiMenu-style tools) pass --workspace=<repo root>; treat like --workspace-root.
        defaultWorkspaceRoot = NormalizePathForConfig(*workspace);
    }
    config.workspaceRoot = defaultWorkspaceRoot;

    std::optional<std::string> experiencePreset{};
    if (const auto preset = commandLine.GetValue("--experience"); preset.has_value() && !preset->empty()) {
        experiencePreset = *preset;
    } else if (const auto presetFallback = commandLine.GetValue("--preset");
               presetFallback.has_value() && !presetFallback->empty()) {
        experiencePreset = *presetFallback;
    }
    std::optional<ri::runtime::ExperiencePresetPatch> experiencePatch{};
    if (experiencePreset.has_value()) {
        experiencePatch = ri::runtime::ResolveExperiencePreset(*experiencePreset);
        if (!experiencePatch.has_value()) {
            std::string supported = "Supported presets:";
            for (const std::string_view name : ri::runtime::SupportedExperiencePresets()) {
                supported += " ";
                supported += std::string(name);
            }
            config.statusMessage = "Unknown --experience preset '" + *experiencePreset + "'. " + supported;
            return config;
        }
    }

    std::optional<ri::content::GameManifest> manifest;
    std::optional<fs::path> explicitGameRoot{};
    const std::optional<std::string> projectRootArg = commandLine.GetValue("--project-root");
    const std::optional<std::string> gameRootArg =
        projectRootArg.has_value() ? projectRootArg : commandLine.GetValue("--game-root");
    if (gameRootArg.has_value() && !gameRootArg->empty()) {
        explicitGameRoot = NormalizePathForConfig(*gameRootArg);
        manifest = ri::content::LoadGameManifest(*explicitGameRoot / "manifest.json");
        if (!manifest.has_value()) {
            config.statusMessage = "Unable to load game manifest from --project-root/--game-root.";
            return config;
        }
    }

    const std::optional<std::string> projectArg = commandLine.GetValue("--project");
    const std::optional<std::string> gameArg = projectArg.has_value() ? projectArg : commandLine.GetValue("--game");
    if (!manifest.has_value() && gameArg.has_value() && !gameArg->empty()) {
        manifest = ri::content::ResolveGameManifest(config.workspaceRoot, *gameArg);
        if (!manifest.has_value()) {
            config.statusMessage = "Unable to resolve game manifest for '" + *gameArg + "'.";
            const std::vector<WorkspaceGameEntry> availableGames = EnumerateWorkspaceGames(config.workspaceRoot);
            if (!availableGames.empty()) {
                config.statusMessage += " Available projects:";
                for (const WorkspaceGameEntry& entry : availableGames) {
                    config.statusMessage += " ";
                    config.statusMessage += entry.id;
                }
            }
            return config;
        }
    } else if (experiencePatch.has_value() && !experiencePatch->gameId.empty()) {
        manifest = ri::content::ResolveGameManifest(config.workspaceRoot, experiencePatch->gameId);
        if (!manifest.has_value()) {
            config.statusMessage =
                "Unable to resolve game manifest for experience preset '" + *experiencePreset + "'.";
            return config;
        }
    }

    if (!manifest.has_value() && !explicitGameRoot.has_value() && !gameArg.has_value()
        && !experiencePatch.has_value()) {
        const std::vector<WorkspaceGameEntry> workspaceGames = EnumerateWorkspaceGames(config.workspaceRoot);
        std::string defaultGameId = "liminal-hall";
        bool defaultExists = false;
        for (const WorkspaceGameEntry& entry : workspaceGames) {
            if (entry.id == defaultGameId) {
                defaultExists = true;
                break;
            }
        }
        if (!defaultExists && !workspaceGames.empty()) {
            defaultGameId = workspaceGames.front().id;
        }
        if (!workspaceGames.empty()) {
            manifest = ri::content::ResolveGameManifest(config.workspaceRoot, defaultGameId);
            if (!manifest.has_value()) {
                config.statusMessage = "Unable to auto-open default game '" + defaultGameId + "'.";
                return config;
            }
        }
    }

    config.workspaceRoot = ResolveEditorWorkspaceRoot(defaultWorkspaceRoot, manifest, explicitGameRoot);
    config.sceneStatePath = BuildEditorSceneStatePath(config.workspaceRoot, "starter");

    if (!manifest.has_value()) {
        config.statusMessage =
            "No game project loaded. Use Resources > game strip or launch with --game=<id>.";
        return config;
    }

    EnsureProjectDevConfig(manifest->rootPath);
    manifest = ri::content::LoadGameManifest(manifest->manifestPath);
    if (!manifest.has_value()) {
        config.statusMessage = "Unable to reload game manifest after project bootstrap.";
        return config;
    }
    config.gameManifest = manifest;
    const std::string manifestVersionLabel = manifest->version.empty() ? "v?" : "v" + manifest->version;
    const std::string manifestAuthorLabel = manifest->author.empty() ? "unknown" : manifest->author;
    config.workspaceLabel = std::string("Authoring — ") + manifest->name + " " + manifestVersionLabel;
    config.windowTitle =
        std::string("RawIron Editor — ") + manifest->name + " " + manifestVersionLabel + " (" + manifestAuthorLabel + ")";
    config.sceneStatePath = BuildEditorSceneStatePath(config.workspaceRoot, manifest->id);
    config.sceneName = "EditorWorkspace_" + manifest->id;
    config.editorPreviewScene =
        manifest->editorPreviewScene.empty() ? "starter" : manifest->editorPreviewScene;
    config.statusMessage = "Opened game project '" + manifest->name + "' " + manifestVersionLabel
        + " by " + manifestAuthorLabel + ".";
    if (!manifest->primaryLevel.empty()) {
        config.statusMessage += " Primary level: " + manifest->primaryLevel + ".";
    }
    const ri::content::ScriptScalarMap uiScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/ui.riscript"));
    const ri::content::ScriptScalarMap audioScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/audio.riscript"));
    const ri::content::ScriptScalarMap streamingScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/streaming.riscript"));
    const ri::content::ScriptScalarMap localizationScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/localization.riscript"));
    const ri::content::ScriptScalarMap physicsScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/physics.riscript"));
    const ri::content::ScriptScalarMap postprocessScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/postprocess.riscript"));
    const ri::content::ScriptScalarMap initScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/init.riscript"));
    const ri::content::ScriptScalarMap gameCfgScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/game.cfg"));
    const ri::content::ScriptScalarMap networkScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/network.riscript"));
    const ri::content::ScriptScalarMap persistenceScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/persistence.riscript"));
    const ri::content::ScriptScalarMap aiScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/ai.riscript"));
    const ri::content::ScriptScalarMap pluginsScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/plugins.riscript"));
    const ri::content::ScriptScalarMap animationScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/animation.riscript"));
    const ri::content::ScriptScalarMap vfxScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "scripts/vfx.riscript"));
    const ri::content::ScriptScalarMap networkCfgScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/network.cfg"));
    const ri::content::ScriptScalarMap buildProfileScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/build.profile"));
    const ri::content::ScriptScalarMap securityPolicyScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/security.policy"));
    const ri::content::ScriptScalarMap pluginsPolicyScalars = ri::content::LoadScriptScalars(
        ri::content::ResolveGameAssetPath(manifest->rootPath, "config/plugins.policy"));
    const ri::content::GameRuntimeSupportData runtimeSupportData =
        ri::content::LoadGameRuntimeSupportData(manifest->rootPath);
    if (!uiScalars.empty()) {
        config.statusMessage += " UI{diag="
            + std::string(ri::content::ScriptScalarOrBool(uiScalars, "show_runtime_diagnostics", false) ? "1" : "0")
            + ",crosshair="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(uiScalars, "crosshair_mode", 1, 0, 4))
            + ",scale="
            + std::to_string(ri::content::ScriptScalarOrClamped(uiScalars, "crosshair_scale", 1.0f, 0.1f, 4.0f))
            + "}.";
    }
    if (!gameCfgScalars.empty()) {
        config.statusMessage += " CFG{runtime="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(gameCfgScalars, "runtime_profile", 1, 0, 16))
            + ",editor="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(gameCfgScalars, "editor_profile", 1, 0, 16))
            + "}.";
    }
    if (!audioScalars.empty()) {
        config.statusMessage += " Audio{master="
            + std::to_string(ri::content::ScriptScalarOrClamped(audioScalars, "audio_master_gain", 1.0f, 0.0f, 4.0f))
            + ",envBlend="
            + std::to_string(ri::content::ScriptScalarOrClamped(audioScalars, "audio_environment_blend", 1.0f, 0.0f, 2.0f))
            + "}.";
    }
    if (!streamingScalars.empty()) {
        config.statusMessage += " Streaming{budget="
            + std::to_string(
                ri::content::ScriptScalarOrClamped(streamingScalars, "streaming_budget_scale", 1.0f, 0.1f, 8.0f))
            + ",autosave="
            + std::string(ri::content::ScriptScalarOrBool(streamingScalars, "checkpoint_autosave_enabled", true) ? "1" : "0")
            + "}.";
    }
    if (!localizationScalars.empty()) {
        config.statusMessage += " Loc{default="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(localizationScalars, "default_locale", 0, 0, 16))
            + "}.";
    }
    if (!physicsScalars.empty()) {
        config.statusMessage += " Physics{gravity="
            + std::to_string(ri::content::ScriptScalarOrClamped(physicsScalars, "global_gravity_scale", 1.0f, 0.1f, 4.0f))
            + "}.";
    }
    if (!postprocessScalars.empty()) {
        config.statusMessage += " PostFx{quality="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(postprocessScalars, "postprocess_quality", 1, 0, 3))
            + "}.";
    }
    if (!initScalars.empty()) {
        config.statusMessage += " Init{warmup="
            + std::to_string(ri::content::ScriptScalarOrIntClamped(initScalars, "warmup_frames", 2, 0, 120))
            + "}.";
    }
    if (!networkScalars.empty() || !persistenceScalars.empty() || !aiScalars.empty() || !pluginsScalars.empty()
        || !animationScalars.empty() || !vfxScalars.empty() || !networkCfgScalars.empty()
        || !buildProfileScalars.empty() || !securityPolicyScalars.empty() || !pluginsPolicyScalars.empty()) {
        config.statusMessage += " RuntimeExt{network="
            + std::to_string(networkScalars.size())
            + ",persistence="
            + std::to_string(persistenceScalars.size())
            + ",ai="
            + std::to_string(aiScalars.size())
            + ",plugins="
            + std::to_string(pluginsScalars.size())
            + ",animation="
            + std::to_string(animationScalars.size())
            + ",vfx="
            + std::to_string(vfxScalars.size())
            + ",networkCfg="
            + std::to_string(networkCfgScalars.size())
            + ",buildProfile="
            + std::to_string(buildProfileScalars.size())
            + ",security="
            + std::to_string(securityPolicyScalars.size())
            + ",pluginsPolicy="
            + std::to_string(pluginsPolicyScalars.size())
            + ",streamRules="
            + std::to_string(runtimeSupportData.streamingPrioritiesByPath.size())
            + ",lookupKeys="
            + std::to_string(runtimeSupportData.lookupIndex.size())
            + ",gen13xTriggers="
            + std::to_string(runtimeSupportData.levelTriggers.size())
            + ",gen13xOcclusion="
            + std::to_string(runtimeSupportData.occlusionVolumes.size())
            + ",gen137AudioZones="
            + std::to_string(runtimeSupportData.audioZones.size())
            + ",gen137Lods="
            + std::to_string(runtimeSupportData.lodRanges.size())
            + "}.";
    }
    const std::string navmeshPath = ri::content::ResolveLookupValueOr(
        "levels.navmesh",
        "levels/assembly.navmesh",
        runtimeSupportData);
    const std::string zonesPath = ri::content::ResolveLookupValueOr(
        "levels.zones",
        "levels/assembly.zones.csv",
        runtimeSupportData);
    const std::string dependenciesPath = ri::content::ResolveLookupValueOr(
        "assets.dependencies",
        "assets/dependencies.json",
        runtimeSupportData);
    const std::string streamingManifestPath = ri::content::ResolveLookupValueOr(
        "assets.streaming_manifest",
        "assets/streaming.manifest",
        runtimeSupportData);
    const std::string schemaDbPath = ri::content::ResolveLookupValueOr(
        "data.schema",
        "data/schema.db",
        runtimeSupportData);
    const std::string lookupPath = ri::content::ResolveLookupValueOr(
        "data.lookup",
        "data/lookup.index",
        runtimeSupportData);
    config.statusMessage += " Assets{navmesh="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, navmeshPath))
        + ",zones="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, zonesPath))
        + ",aiNodes="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.ai.nodes"))
        + ",deps="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, dependenciesPath))
        + ",streamingManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, streamingManifestPath))
        + ",shadersManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/shaders.manifest"))
        + ",schema="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, schemaDbPath))
        + ",lookup="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, lookupPath))
        + ",entityRegistry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "data/entity.registry"))
        + ",aiBehavior="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/behavior.tree"))
        + ",aiBlackboard="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/blackboard.json"))
        + ",aiFactions="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/factions.cfg"))
        + ",lighting="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.lighting.csv"))
        + ",cinematics="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.cinematics.csv"))
        + ",pluginsManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "plugins/manifest.plugins"))
        + ",pluginsLoadOrder="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "plugins/load_order.cfg"))
        + ",pluginsRegistry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "plugins/registry.json"))
        + ",pluginsHooks="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "plugins/hooks.riplugin"))
        + ",animationGraph="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/animation.graph"))
        + ",vfxManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/vfx.manifest"))
        + ",triggersCsv="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.triggers.csv"))
        + ",occlusionCsv="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.occlusion.csv"))
        + ",audioZones="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.audio.zones"))
        + ",lodsCsv="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "levels/assembly.lods.csv"))
        + ",materialsManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/materials.manifest"))
        + ",audioBanks="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/audio.banks"))
        + ",fontsManifest="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "assets/fonts.manifest"))
        + ",saveSchema="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "data/save.schema"))
        + ",achievementsRegistry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "data/achievements.registry"))
        + ",perceptionCfg="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/perception.cfg"))
        + ",squadTactics="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ai/squad.tactics"))
        + ",uiLayout="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ui/layout.xml"))
        + ",uiStyling="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "ui/styling.css"))
        + ",gameplayTests="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "tests/gameplay.test.riscript"))
        + ",renderingTests="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "tests/rendering.test.riscript"))
        + ",networkTests="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "tests/network.test.riscript"))
        + ",uiTests="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "tests/ui.test.riscript"))
        + ",telemetry="
        + DescribeOptionalAssetState(ri::content::ResolveGameAssetPath(manifest->rootPath, "data/telemetry.db"), true)
        + "}.";
    config.statusMessage += " Gen13x{materials="
        + std::to_string(runtimeSupportData.materialsById.size())
        + ",audioBanks="
        + std::to_string(runtimeSupportData.audioBankPathById.size())
        + ",fonts="
        + std::to_string(runtimeSupportData.fontPathByFontKey.size())
        + ",saveSchema="
        + (runtimeSupportData.saveSchemaVersion.has_value()
               ? std::to_string(*runtimeSupportData.saveSchemaVersion)
               : std::string("-"))
        + ",achievements="
        + std::to_string(runtimeSupportData.achievementIdsByPlatform.size())
        + ",perceptionKeys="
        + std::to_string(runtimeSupportData.perceptionScalars.size())
        + ",squads="
        + std::to_string(runtimeSupportData.squadTactics.size())
        + ",audioZones="
        + std::to_string(runtimeSupportData.audioZones.size())
        + ",lods="
        + std::to_string(runtimeSupportData.lodRanges.size())
        + "}.";
    const std::vector<std::string> formatIssues = ri::content::ValidateGameProjectFormat(*manifest);
    if (!formatIssues.empty()) {
        std::string combined = "Game format issues:";
        for (const std::string& issue : formatIssues) {
            combined += " ";
            combined += issue;
        }
        if (!config.statusMessage.empty()) {
            config.statusMessage += " ";
        }
        config.statusMessage += combined;
    }
    if (experiencePatch.has_value()) {
        if (experiencePatch->windowTitle.has_value()) {
            config.windowTitle = *experiencePatch->windowTitle + " - Creator Editor";
        }
        config.statusMessage = "Opened creator experience preset '" + *experiencePreset + "'.";
    }

    return config;
}

class EditorHost final : public ri::core::Host {
public:
    [[nodiscard]] std::string_view GetName() const noexcept override {
        return "RawIron.Editor";
    }

    [[nodiscard]] std::string_view GetMode() const noexcept override {
        return "editor";
    }

    void OnStartup(const ri::core::CommandLine& commandLine) override {
        dumpSceneEveryFrame_ = commandLine.HasFlag("--dump-scene-every-frame");
        dumpScene_ = !commandLine.HasFlag("--no-scene-dump");
        logEveryFrame_ = commandLine.HasFlag("--log-every-frame");
        sceneConfig_ = ResolveSceneConfig(commandLine);
        ri::editor::RegisterBundledGameEditorPreviews();
        starterScene_ = ri::editor::BuildEditorWorkspaceScene(
            sceneConfig_.editorPreviewScene,
            sceneConfig_.sceneName,
            sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->rootPath : fs::path{});

        ri::core::LogSection("Editor Startup");
        ri::core::LogInfo("Authoring shell: place primitives/brushes, save persistent editor state with Ctrl+S, then export game CSV with Ctrl+E.");
        ri::core::LogInfo("Scene graph and helpers run in the shared runtime; this is the live edit buffer for level geometry.");
        ri::core::LogInfo("Workspace: " + sceneConfig_.workspaceLabel);
        if (!sceneConfig_.statusMessage.empty()) {
            ri::core::LogInfo(sceneConfig_.statusMessage);
        }
        ri::core::LogInfo("Workspace root: " + sceneConfig_.workspaceRoot.string());
        if (sceneConfig_.gameManifest.has_value()) {
            ri::core::LogInfo("Game root: " + sceneConfig_.gameManifest->rootPath.string());
        }
        ri::core::LogInfo("Scene state: " + sceneConfig_.sceneStatePath.string());
        ri::core::LogInfo("Root nodes: " + std::to_string(ri::scene::CollectRootNodes(starterScene_.scene).size()));
        ri::core::LogInfo("Grid path: " + ri::scene::DescribeNodePath(starterScene_.scene, starterScene_.handles.grid));

        if (dumpScene_) {
            ri::core::LogSection("Editor Scene");
            ri::core::LogInfo(starterScene_.scene.Describe());
        }
    }

    [[nodiscard]] bool OnFrame(const ri::core::FrameContext& frame) override {
        ri::editor::AnimateEditorWorkspaceScene(sceneConfig_.editorPreviewScene, starterScene_, frame.elapsedSeconds);

        if (frame.frameIndex == 0 || logEveryFrame_) {
            const ri::scene::Scene& scene = starterScene_.scene;
            const ri::scene::Node& orbitRig = scene.GetNode(starterScene_.handles.orbitCamera.root);
            const ri::math::Vec3 cameraPosition = scene.ComputeWorldPosition(starterScene_.handles.orbitCamera.cameraNode);
            ri::core::LogInfo(
                "Editor frame " + std::to_string(frame.frameIndex) +
                " orbitYaw=" + std::to_string(orbitRig.localTransform.rotationDegrees.y) +
                " camera=" + ri::math::ToString(cameraPosition));
        }

        if (dumpSceneEveryFrame_) {
            const ri::scene::Scene& scene = starterScene_.scene;
            ri::core::LogSection("Editor Scene Frame " + std::to_string(frame.frameIndex));
            ri::core::LogInfo(scene.Describe());
        }

        return true;
    }

    void OnShutdown() override {
        ri::core::LogSection("Editor Shutdown");
        ri::core::LogInfo("Editor host shutdown complete.");
    }

private:
    bool dumpScene_ = true;
    bool dumpSceneEveryFrame_ = false;
    bool logEveryFrame_ = false;
    EditorSceneConfig sceneConfig_{};
    ri::scene::StarterScene starterScene_{};
};

#if defined(_WIN32)
std::wstring Widen(const std::string& value) {
    return EditorRenderer::Widen(value);
}

void FillRectColor(HDC dc, const RECT& rect, COLORREF color) {
    EditorRenderer::FillRectColor(dc, rect, color);
}

void DrawPanelFrame(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow) {
    EditorRenderer::DrawPanelFrame(dc, rect, fill, highlight, shadow);
}

void DrawInsetFrame(HDC dc, const RECT& rect, COLORREF fill, COLORREF highlight, COLORREF shadow) {
    EditorRenderer::DrawInsetFrame(dc, rect, fill, highlight, shadow);
}

void DrawTextLine(HDC dc, const RECT& rect, const std::string& text, COLORREF color, HFONT font, UINT format) {
    EditorRenderer::DrawTextLine(dc, rect, text, color, font, format);
}

HFONT CreateUiFont(int height, int weight, const wchar_t* faceName = L"Segoe UI") {
    return CreateFontW(
        height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        faceName);
}

[[nodiscard]] int ComputeNodeDepth(const ri::scene::Scene& scene, int nodeIndex) {
    int depth = 0;
    int parent = scene.GetNode(nodeIndex).parent;
    while (parent != ri::scene::kInvalidHandle) {
        ++depth;
        parent = scene.GetNode(parent).parent;
    }
    return depth;
}

enum class RawIronFlatProjection {
    TopXz,
    FrontXy,
    SideZy,
};

void DcStrokeLine(HDC dc, LONG x1, LONG y1, LONG x2, LONG y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

[[nodiscard]] float PickNiceGridStep(float worldDiameter) {
    const float raw = worldDiameter / 9.0f;
    const float safe = std::max(raw, 0.0001f);
    const float power = std::pow(10.0f, std::floor(std::log10(safe)));
    const float mantissa = safe / power;
    float factor = 1.0f;
    if (mantissa < 1.5f) {
        factor = 1.0f;
    } else if (mantissa < 3.5f) {
        factor = 2.0f;
    } else if (mantissa < 7.0f) {
        factor = 5.0f;
    } else {
        factor = 10.0f;
    }
    return factor * power;
}

[[nodiscard]] std::optional<ri::scene::WorldBounds> TryMergeRenderableBounds(const ri::scene::Scene& scene) {
    const std::vector<int> handles = ri::scene::CollectRenderableNodes(scene);
    return ri::scene::ComputeCombinedWorldBounds(scene, handles, true);
}

[[nodiscard]] ri::scene::WorldBounds DefaultEditorBounds() {
    return ri::scene::WorldBounds{
        .min = ri::math::Vec3{-10.0f, -4.0f, -10.0f},
        .max = ri::math::Vec3{10.0f, 10.0f, 10.0f},
    };
}

void ProjectRawIronTop(const RECT& cell,
                      float centerX,
                      float centerZ,
                      float halfSpan,
                      float wx,
                      float wz,
                      LONG& sx,
                      LONG& sy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(cell.right - cell.left) - kPad * 2.0f;
    const float uh = static_cast<float>(cell.bottom - cell.top) - kPad * 2.0f;
    const float minX = centerX - halfSpan;
    const float minZ = centerZ - halfSpan;
    const float u = (wx - minX) / (halfSpan * 2.0f);
    const float v = (wz - minZ) / (halfSpan * 2.0f);
    sx = cell.left + static_cast<LONG>(kPad + std::clamp(u, -0.02f, 1.02f) * uw);
    sy = cell.top + static_cast<LONG>(kPad + (1.0f - std::clamp(v, -0.02f, 1.02f)) * uh);
}

void ProjectRawIronFront(const RECT& cell,
                        float centerX,
                        float centerY,
                        float halfSpan,
                        float wx,
                        float wy,
                        LONG& sx,
                        LONG& sy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(cell.right - cell.left) - kPad * 2.0f;
    const float uh = static_cast<float>(cell.bottom - cell.top) - kPad * 2.0f;
    const float minX = centerX - halfSpan;
    const float minY = centerY - halfSpan;
    const float u = (wx - minX) / (halfSpan * 2.0f);
    const float v = (wy - minY) / (halfSpan * 2.0f);
    sx = cell.left + static_cast<LONG>(kPad + std::clamp(u, -0.02f, 1.02f) * uw);
    sy = cell.top + static_cast<LONG>(kPad + (1.0f - std::clamp(v, -0.02f, 1.02f)) * uh);
}

void ProjectRawIronSide(const RECT& cell,
                       float centerZ,
                       float centerY,
                       float halfSpan,
                       float wz,
                       float wy,
                       LONG& sx,
                       LONG& sy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(cell.right - cell.left) - kPad * 2.0f;
    const float uh = static_cast<float>(cell.bottom - cell.top) - kPad * 2.0f;
    const float minZ = centerZ - halfSpan;
    const float minY = centerY - halfSpan;
    const float u = (wz - minZ) / (halfSpan * 2.0f);
    const float v = (wy - minY) / (halfSpan * 2.0f);
    sx = cell.left + static_cast<LONG>(kPad + std::clamp(u, -0.02f, 1.02f) * uw);
    sy = cell.top + static_cast<LONG>(kPad + (1.0f - std::clamp(v, -0.02f, 1.02f)) * uh);
}

struct OrthoFrameAxes {
    float cxA = 0.0f;
    float cxB = 0.0f;
    float halfSpan = 1.0f;
};

[[nodiscard]] OrthoFrameAxes ComputeOrthoFrame(const ri::scene::Scene& scene, RawIronFlatProjection projection) {
    OrthoFrameAxes out{};
    ri::scene::WorldBounds bounds = DefaultEditorBounds();
    if (const auto merged = TryMergeRenderableBounds(scene); merged.has_value()) {
        bounds = *merged;
    }
    const ri::math::Vec3 center = ri::scene::GetBoundsCenter(bounds);
    const ri::math::Vec3 size = ri::scene::GetBoundsSize(bounds);
    const float margin = std::max(ri::math::Length(size) * 0.08f, 1.25f);
    if (projection == RawIronFlatProjection::TopXz) {
        out.cxA = center.x;
        out.cxB = center.z;
        out.halfSpan = std::max(std::max(size.x, size.z) * 0.5f + margin, 6.0f);
    } else if (projection == RawIronFlatProjection::FrontXy) {
        out.cxA = center.x;
        out.cxB = center.y;
        out.halfSpan = std::max(std::max(size.x, size.y) * 0.5f + margin, 6.0f);
    } else {
        out.cxA = center.z;
        out.cxB = center.y;
        out.halfSpan = std::max(std::max(size.z, size.y) * 0.5f + margin, 6.0f);
    }
    return out;
}

void ScreenToRawIronTopInv(const RECT& plot, int mx, int my, float cx, float cz, float halfSpan, float& wx, float& wz) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(plot.right - plot.left) - kPad * 2.0f;
    const float uh = static_cast<float>(plot.bottom - plot.top) - kPad * 2.0f;
    const float u = (static_cast<float>(mx) - static_cast<float>(plot.left) - kPad) / std::max(uw, 0.001f);
    const float v = 1.0f - (static_cast<float>(my) - static_cast<float>(plot.top) - kPad) / std::max(uh, 0.001f);
    const float uu = std::clamp(u, 0.0f, 1.0f);
    const float vv = std::clamp(v, 0.0f, 1.0f);
    wx = (cx - halfSpan) + uu * (halfSpan * 2.0f);
    wz = (cz - halfSpan) + vv * (halfSpan * 2.0f);
}

void ScreenToRawIronFrontInv(const RECT& plot,
                            int mx,
                            int my,
                            float cx,
                            float cy,
                            float halfSpan,
                            float& wx,
                            float& wy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(plot.right - plot.left) - kPad * 2.0f;
    const float uh = static_cast<float>(plot.bottom - plot.top) - kPad * 2.0f;
    const float u = (static_cast<float>(mx) - static_cast<float>(plot.left) - kPad) / std::max(uw, 0.001f);
    const float v = 1.0f - (static_cast<float>(my) - static_cast<float>(plot.top) - kPad) / std::max(uh, 0.001f);
    const float uu = std::clamp(u, 0.0f, 1.0f);
    const float vv = std::clamp(v, 0.0f, 1.0f);
    wx = (cx - halfSpan) + uu * (halfSpan * 2.0f);
    wy = (cy - halfSpan) + vv * (halfSpan * 2.0f);
}

void ScreenToRawIronSideInv(const RECT& plot,
                           int mx,
                           int my,
                           float cz,
                           float cy,
                           float halfSpan,
                           float& wz,
                           float& wy) {
    constexpr float kPad = 5.0f;
    const float uw = static_cast<float>(plot.right - plot.left) - kPad * 2.0f;
    const float uh = static_cast<float>(plot.bottom - plot.top) - kPad * 2.0f;
    const float u = (static_cast<float>(mx) - static_cast<float>(plot.left) - kPad) / std::max(uw, 0.001f);
    const float v = 1.0f - (static_cast<float>(my) - static_cast<float>(plot.top) - kPad) / std::max(uh, 0.001f);
    const float uu = std::clamp(u, 0.0f, 1.0f);
    const float vv = std::clamp(v, 0.0f, 1.0f);
    wz = (cz - halfSpan) + uu * (halfSpan * 2.0f);
    wy = (cy - halfSpan) + vv * (halfSpan * 2.0f);
}

[[nodiscard]] std::optional<int> PickRenderableInOrthoView(const RECT& plot,
                                                             RawIronFlatProjection projection,
                                                             int mx,
                                                             int my,
                                                             const ri::scene::Scene& scene) {
    if (plot.right <= plot.left + 4 || plot.bottom <= plot.top + 4) {
        return std::nullopt;
    }
    const OrthoFrameAxes frame = ComputeOrthoFrame(scene, projection);
    float a0 = 0.0f;
    float a1 = 0.0f;
    if (projection == RawIronFlatProjection::TopXz) {
        ScreenToRawIronTopInv(plot, mx, my, frame.cxA, frame.cxB, frame.halfSpan, a0, a1);
    } else if (projection == RawIronFlatProjection::FrontXy) {
        ScreenToRawIronFrontInv(plot, mx, my, frame.cxA, frame.cxB, frame.halfSpan, a0, a1);
    } else {
        ScreenToRawIronSideInv(plot, mx, my, frame.cxA, frame.cxB, frame.halfSpan, a0, a1);
    }

    float bestD2 = std::numeric_limits<float>::infinity();
    int best = ri::scene::kInvalidHandle;
    for (const int handle : ri::scene::CollectRenderableNodes(scene)) {
        const std::optional<ri::scene::WorldBounds> bounds =
            ri::scene::ComputeNodeWorldBounds(scene, handle, true);
        if (!bounds.has_value()) {
            continue;
        }

        bool inside = false;
        float pc0 = 0.0f;
        float pc1 = 0.0f;
        if (projection == RawIronFlatProjection::TopXz) {
            inside = a0 >= bounds->min.x && a0 <= bounds->max.x && a1 >= bounds->min.z && a1 <= bounds->max.z;
            const ri::math::Vec3 center = ri::scene::GetBoundsCenter(*bounds);
            pc0 = center.x;
            pc1 = center.z;
        } else if (projection == RawIronFlatProjection::FrontXy) {
            inside = a0 >= bounds->min.x && a0 <= bounds->max.x && a1 >= bounds->min.y && a1 <= bounds->max.y;
            const ri::math::Vec3 center = ri::scene::GetBoundsCenter(*bounds);
            pc0 = center.x;
            pc1 = center.y;
        } else {
            inside = a0 >= bounds->min.z && a0 <= bounds->max.z && a1 >= bounds->min.y && a1 <= bounds->max.y;
            const ri::math::Vec3 center = ri::scene::GetBoundsCenter(*bounds);
            pc0 = center.z;
            pc1 = center.y;
        }
        if (!inside) {
            continue;
        }

        const float dx = a0 - pc0;
        const float dy = a1 - pc1;
        const float dist2 = dx * dx + dy * dy;
        if (dist2 < bestD2) {
            bestD2 = dist2;
            best = handle;
        }
    }

    if (best == ri::scene::kInvalidHandle) {
        return std::nullopt;
    }
    return best;
}

[[nodiscard]] fs::path EditorOrbitSidecarPath(const fs::path& sceneStatePath) {
    return sceneStatePath.parent_path() / "editor_orbit.ri_cam";
}

bool TryLoadEditorOrbitSidecar(const fs::path& sceneStatePath, ri::scene::OrbitCameraState& out) {
    const fs::path path = EditorOrbitSidecarPath(sceneStatePath);
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }
    std::string magic;
    stream >> magic;
    if (magic != "RAWIRON_EDITOR_ORBIT_V1") {
        return false;
    }
    stream >> out.target.x >> out.target.y >> out.target.z;
    stream >> out.distance;
    stream >> out.yawDegrees >> out.pitchDegrees;
    return stream.good();
}

bool TryLoadEditorOrbitStateFromPath(const fs::path& path, ri::scene::OrbitCameraState& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }
    std::string magic;
    stream >> magic;
    if (magic != "RAWIRON_EDITOR_ORBIT_V1") {
        return false;
    }
    stream >> out.target.x >> out.target.y >> out.target.z;
    stream >> out.distance;
    stream >> out.yawDegrees >> out.pitchDegrees;
    return stream.good();
}

bool SaveEditorOrbitStateToPath(const fs::path& path, const ri::scene::OrbitCameraState& orbit) {
    std::error_code ec{};
    fs::create_directories(path.parent_path(), ec);
    (void)ec;
    std::ofstream stream(path, std::ios::trunc);
    if (!stream.is_open()) {
        return false;
    }
    stream << "RAWIRON_EDITOR_ORBIT_V1\n";
    stream << std::fixed << std::setprecision(8);
    stream << orbit.target.x << " " << orbit.target.y << " " << orbit.target.z << "\n";
    stream << orbit.distance << "\n";
    stream << orbit.yawDegrees << " " << orbit.pitchDegrees << "\n";
    return static_cast<bool>(stream);
}

void DrawRawIronOrthoGrid(HDC dc,
                         const RECT& cell,
                         RawIronFlatProjection projection,
                         float cxA,
                         float cxB,
                         float halfSpan) {
    const COLORREF lineA = RGB(105, 105, 105);
    const COLORREF lineB = RGB(88, 88, 88);
    const float step = PickNiceGridStep(halfSpan * 2.0f);
    const float startA = std::floor((cxA - halfSpan) / step) * step;
    const float startB = std::floor((cxB - halfSpan) / step) * step;

    for (float a = startA; a <= cxA + halfSpan + step * 0.5f; a += step) {
        LONG x1 = 0;
        LONG y1 = 0;
        LONG x2 = 0;
        LONG y2 = 0;
        if (projection == RawIronFlatProjection::TopXz) {
            ProjectRawIronTop(cell, cxA, cxB, halfSpan, a, cxB - halfSpan, x1, y1);
            ProjectRawIronTop(cell, cxA, cxB, halfSpan, a, cxB + halfSpan, x2, y2);
        } else if (projection == RawIronFlatProjection::FrontXy) {
            ProjectRawIronFront(cell, cxA, cxB, halfSpan, a, cxB - halfSpan, x1, y1);
            ProjectRawIronFront(cell, cxA, cxB, halfSpan, a, cxB + halfSpan, x2, y2);
        } else {
            ProjectRawIronSide(cell, cxA, cxB, halfSpan, a, cxB - halfSpan, x1, y1);
            ProjectRawIronSide(cell, cxA, cxB, halfSpan, a, cxB + halfSpan, x2, y2);
        }
        DcStrokeLine(dc, x1, y1, x2, y2, lineA, 1);
    }

    for (float b = startB; b <= cxB + halfSpan + step * 0.5f; b += step) {
        LONG x1 = 0;
        LONG y1 = 0;
        LONG x2 = 0;
        LONG y2 = 0;
        if (projection == RawIronFlatProjection::TopXz) {
            ProjectRawIronTop(cell, cxA, cxB, halfSpan, cxA - halfSpan, b, x1, y1);
            ProjectRawIronTop(cell, cxA, cxB, halfSpan, cxA + halfSpan, b, x2, y2);
        } else if (projection == RawIronFlatProjection::FrontXy) {
            ProjectRawIronFront(cell, cxA, cxB, halfSpan, cxA - halfSpan, b, x1, y1);
            ProjectRawIronFront(cell, cxA, cxB, halfSpan, cxA + halfSpan, b, x2, y2);
        } else {
            ProjectRawIronSide(cell, cxA, cxB, halfSpan, cxA - halfSpan, b, x1, y1);
            ProjectRawIronSide(cell, cxA, cxB, halfSpan, cxA + halfSpan, b, x2, y2);
        }
        DcStrokeLine(dc, x1, y1, x2, y2, lineB, 1);
    }
}

void DrawRawIronFlatSceneView(HDC dc,
                              const RECT& cell,
                              const ri::scene::Scene& scene,
                              std::size_t selectedNode,
                              const ri::math::Vec3& orbitFocus,
                              RawIronFlatProjection projection,
                              const char* title,
                              HFONT labelFont) {
    DrawInsetFrame(dc, cell, RGB(40, 40, 40), RGB(150, 150, 150), RGB(16, 16, 16));
    RECT inner{cell.left + 2, cell.top + 2, cell.right - 2, cell.bottom - 2};
    FillRectColor(dc, inner, RGB(56, 56, 56));

    DrawTextLine(dc,
                 RECT{inner.left + 6, inner.top + 4, inner.right - 6, inner.top + 22},
                 std::string(title),
                 RGB(255, 255, 200),
                 labelFont,
                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    RECT plot{inner.left + 4, inner.top + 24, inner.right - 4, inner.bottom - 4};
    if (plot.right <= plot.left + 8 || plot.bottom <= plot.top + 8) {
        return;
    }

    ri::scene::WorldBounds bounds = DefaultEditorBounds();
    if (const auto merged = TryMergeRenderableBounds(scene); merged.has_value()) {
        bounds = *merged;
    }

    const ri::math::Vec3 center = ri::scene::GetBoundsCenter(bounds);
    const ri::math::Vec3 size = ri::scene::GetBoundsSize(bounds);
    const float margin = std::max(ri::math::Length(size) * 0.08f, 1.25f);

    float cxA = 0.0f;
    float cxB = 0.0f;
    float halfSpan = 8.0f;
    if (projection == RawIronFlatProjection::TopXz) {
        cxA = center.x;
        cxB = center.z;
        halfSpan = std::max(std::max(size.x, size.z) * 0.5f + margin, 6.0f);
        DrawRawIronOrthoGrid(dc, plot, projection, cxA, cxB, halfSpan);
        LONG fx = 0;
        LONG fy = 0;
        ProjectRawIronTop(plot, cxA, cxB, halfSpan, orbitFocus.x, orbitFocus.z, fx, fy);
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, RGB(255, 220, 80), 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, RGB(255, 220, 80), 1);
    } else if (projection == RawIronFlatProjection::FrontXy) {
        cxA = center.x;
        cxB = center.y;
        halfSpan = std::max(std::max(size.x, size.y) * 0.5f + margin, 6.0f);
        DrawRawIronOrthoGrid(dc, plot, projection, cxA, cxB, halfSpan);
        LONG fx = 0;
        LONG fy = 0;
        ProjectRawIronFront(plot, cxA, cxB, halfSpan, orbitFocus.x, orbitFocus.y, fx, fy);
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, RGB(255, 220, 80), 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, RGB(255, 220, 80), 1);
    } else {
        cxA = center.z;
        cxB = center.y;
        halfSpan = std::max(std::max(size.z, size.y) * 0.5f + margin, 6.0f);
        DrawRawIronOrthoGrid(dc, plot, projection, cxA, cxB, halfSpan);
        LONG fx = 0;
        LONG fy = 0;
        ProjectRawIronSide(plot, cxA, cxB, halfSpan, orbitFocus.z, orbitFocus.y, fx, fy);
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, RGB(255, 220, 80), 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, RGB(255, 220, 80), 1);
    }

    const std::vector<int> renderables = ri::scene::CollectRenderableNodes(scene);
    for (const int handle : renderables) {
        const std::optional<ri::scene::WorldBounds> nb = ri::scene::ComputeNodeWorldBounds(scene, handle, true);
        if (!nb.has_value()) {
            continue;
        }
        const bool isSelected = static_cast<std::size_t>(handle) == selectedNode;
        const COLORREF stroke = isSelected ? RGB(255, 255, 40) : RGB(200, 200, 200);
        const int penW = isSelected ? 2 : 1;

        const float minx = nb->min.x;
        const float maxx = nb->max.x;
        const float miny = nb->min.y;
        const float maxy = nb->max.y;
        const float minz = nb->min.z;
        const float maxz = nb->max.z;

        LONG ax = 0;
        LONG ay = 0;
        LONG bx = 0;
        LONG by = 0;
        LONG cx = 0;
        LONG cy = 0;
        LONG dx = 0;
        LONG dy = 0;

        if (projection == RawIronFlatProjection::TopXz) {
            ProjectRawIronTop(plot, cxA, cxB, halfSpan, minx, minz, ax, ay);
            ProjectRawIronTop(plot, cxA, cxB, halfSpan, maxx, minz, bx, by);
            ProjectRawIronTop(plot, cxA, cxB, halfSpan, maxx, maxz, cx, cy);
            ProjectRawIronTop(plot, cxA, cxB, halfSpan, minx, maxz, dx, dy);
        } else if (projection == RawIronFlatProjection::FrontXy) {
            ProjectRawIronFront(plot, cxA, cxB, halfSpan, minx, miny, ax, ay);
            ProjectRawIronFront(plot, cxA, cxB, halfSpan, maxx, miny, bx, by);
            ProjectRawIronFront(plot, cxA, cxB, halfSpan, maxx, maxy, cx, cy);
            ProjectRawIronFront(plot, cxA, cxB, halfSpan, minx, maxy, dx, dy);
        } else {
            ProjectRawIronSide(plot, cxA, cxB, halfSpan, minz, miny, ax, ay);
            ProjectRawIronSide(plot, cxA, cxB, halfSpan, maxz, miny, bx, by);
            ProjectRawIronSide(plot, cxA, cxB, halfSpan, maxz, maxy, cx, cy);
            ProjectRawIronSide(plot, cxA, cxB, halfSpan, minz, maxy, dx, dy);
        }

        DcStrokeLine(dc, ax, ay, bx, by, stroke, penW);
        DcStrokeLine(dc, bx, by, cx, cy, stroke, penW);
        DcStrokeLine(dc, cx, cy, dx, dy, stroke, penW);
        DcStrokeLine(dc, dx, dy, ax, ay, stroke, penW);
    }
}

struct AuthoringToolbarRects {
    RECT addCube{};
    RECT addPlane{};
    RECT addTrigger{};
    RECT duplicate{};
    RECT exportCsv{};
    RECT play{};
};

struct TopChromeRects {
    RECT save{};
    RECT scaffold{};
    RECT exportScene{};
    RECT play{};
    RECT files{};
};

[[nodiscard]] std::string SanitizeBrushLabelForName(std::string_view label) {
    std::string out;
    out.reserve(label.size());
    for (const unsigned char ch : label) {
        if (std::isalnum(ch) != 0) {
            out.push_back(static_cast<char>(ch));
        } else if (ch == '_' || ch == '-') {
            out.push_back('_');
        }
    }
    if (out.empty()) {
        return "brush";
    }
    return out;
}

[[nodiscard]] ri::math::Vec3 StructuralBrushSpawnPosition(const std::string_view structuralType,
                                                            const ri::math::Vec3& orbitTarget) {
    if (structuralType == "plane") {
        return {orbitTarget.x, 0.0f, orbitTarget.z};
    }
    return {orbitTarget.x, orbitTarget.y + 0.5f, orbitTarget.z};
}

[[nodiscard]] AuthoringToolbarRects ComputeAuthoringToolbarRects(const RECT& toolStrip) {
    const LONG rowTop = toolStrip.top + 8;
    const LONG rowBot = toolStrip.bottom - 8;
    const LONG x0 = toolStrip.left + 720;
    AuthoringToolbarRects rects{};
    rects.addCube = {x0, rowTop, x0 + 88, rowBot};
    rects.addPlane = {x0 + 94, rowTop, x0 + 188, rowBot};
    rects.addTrigger = {x0 + 194, rowTop, x0 + 300, rowBot};
    rects.duplicate = {x0 + 306, rowTop, x0 + 388, rowBot};
    rects.exportCsv = {x0 + 394, rowTop, x0 + 484, rowBot};
    rects.play = {x0 + 490, rowTop, x0 + 568, rowBot};
    return rects;
}

[[nodiscard]] TopChromeRects ComputeTopChromeRects(const RECT& topBar) {
    const LONG rowTop = topBar.top + 10;
    const LONG rowBot = topBar.top + 36;
    const LONG right = topBar.right - 18;
    TopChromeRects rects{};
    rects.files = {right - 94, rowTop, right, rowBot};
    rects.play = {right - 194, rowTop, right - 100, rowBot};
    rects.exportScene = {right - 300, rowTop, right - 200, rowBot};
    rects.scaffold = {right - 406, rowTop, right - 306, rowBot};
    rects.save = {right - 512, rowTop, right - 412, rowBot};
    return rects;
}

class RawIronEditorWindow {
public:
    explicit RawIronEditorWindow(const ri::core::CommandLine& commandLine)
        : logEveryFrame_(commandLine.HasFlag("--log-every-frame")),
          dumpScene_(commandLine.HasFlag("--dump-scene-every-frame")),
          sceneConfig_(ResolveSceneConfig(commandLine)),
          statsOverlayVisible_(commandLine.HasFlag("--stats-overlay")),
          statsOverlayState_(true),
          autoOrbitPreview_(commandLine.HasFlag("--auto-orbit")) {
        ri::editor::RegisterBundledGameEditorPreviews();
        starterScene_ = ri::editor::BuildEditorWorkspaceScene(
            sceneConfig_.editorPreviewScene,
            sceneConfig_.sceneName,
            sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->rootPath : fs::path{});
        editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
        (void)TryLoadEditorOrbitSidecar(sceneConfig_.sceneStatePath, editorOrbitState_);
        ApplyEditorOrbitToScene();
        statsOverlayState_.SetAttached(true);
        statsOverlayState_.SetVisible(statsOverlayVisible_);
        if (!sceneConfig_.statusMessage.empty()) {
            lastIoStatus_ = sceneConfig_.statusMessage + "  ";
        }
        lastIoStatus_ += "Camera: drag in CAMERA, wheel zooms. Tab: full 3D / quad.";
        lastIoStatus_ +=
            "  Authoring: +Cube/+Plane, T/R/U, Ctrl+S save, Ctrl+E export, Ctrl+D duplicate, F2 rename, Playtest.";
        if (autoOrbitPreview_) {
            lastIoStatus_ += "  (--auto-orbit: demo camera motion on)";
        }
        RefreshWorkspaceGamesAndResources();
        RebuildFilteredHierarchyOrder();
        EnsureEditorTrashFolder();
        LoadCreatorPolicyFromDisk();
        authoredNodeStart_ = starterScene_.scene.NodeCount();
        baselineStarterScene_ = starterScene_.scene;
        std::error_code loadEc{};
        if (fs::exists(ResolveSceneStatePath(), loadEc) || fs::exists(ResolveAuthoredSceneStatePath(), loadEc)) {
            (void)TryLoadPersistentEditorScene(ResolveSceneStatePath(), false);
        } else {
            TryImportPrimaryLevelCsv();
        }
        lastAutosaveSteady_ = std::chrono::steady_clock::now();
        std::error_code ec{};
        if (fs::exists(ResolveAutosaveScenePath(), ec)) {
            lastIoStatus_ += "  Autosave found (Ctrl+Shift+L loads it).";
        }
    }

    int Run(HINSTANCE instance) {
        const wchar_t* className = L"RawIronEditorWindow";
        WNDCLASSW windowClass{};
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &RawIronEditorWindow::WindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&windowClass);

        hwnd_ = CreateWindowExW(
            0,
            className,
            Widen(sceneConfig_.windowTitle).c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1520,
            900,
            nullptr,
            nullptr,
            instance,
            this);
        if (hwnd_ == nullptr) {
            return 1;
        }

        titleFont_ = CreateUiFont(-20, FW_BOLD, L"Segoe UI");
        headerFont_ = CreateUiFont(-15, FW_SEMIBOLD, L"Segoe UI");
        bodyFont_ = CreateUiFont(-14, FW_SEMIBOLD, L"Segoe UI");
        smallFont_ = CreateUiFont(-12, FW_NORMAL, L"Segoe UI");

        SetTimer(hwnd_, 1, 33, nullptr);
        lastTick_ = std::chrono::steady_clock::now();

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        DeleteObject(titleFont_);
        DeleteObject(headerFont_);
        DeleteObject(bodyFont_);
        DeleteObject(smallFont_);
        return static_cast<int>(message.wParam);
    }

private:
    enum class EditMode {
        Translate,
        Rotate,
        Scale,
    };
    enum class LeftPanelMode {
        Scene,
        Resources,
    };
    enum class InspectorPanel {
        Node,
        Brush,
        Gameplay,
        Files,
        UiWorkbench,
    };
enum class UiWorkbenchSource {
    AutoSelection,
    MenuSample,
    VnSample,
};

enum class UiWorkbenchTextEditTarget {
    None,
    ScreenTitle,
    BlockText,
    BlockSpeaker,
    BlockLabel,
    BlockImagePath,
};
    static constexpr int kHierarchyRowHeight_ = 25;
    static constexpr int kHierarchyBottomGutter_ = 26;
    static constexpr int kLeftPanelTabHeight_ = 24;
    static constexpr int kLeftPanelGameStripHeight_ = 28;
    static constexpr int kResourceFilterStripHeight_ = 26;
    static constexpr int kResourceListRowHeight_ = 22;
    struct EditorLayout {
        RECT toolStrip{};
        RECT hierarchy{};
        RECT hierarchyInner{};
        RECT viewport{};
        RECT inspector{};
        RECT viewportInner{};
        RECT inspectorInner{};
        RECT hierarchySplitter{};
        RECT inspectorSplitter{};
    };
    struct TransformEditAction {
        std::size_t nodeIndex = 0;
        ri::scene::Transform before{};
        ri::scene::Transform after{};
    };
    struct SceneGraphEditAction {
        ri::scene::Scene beforeScene{};
        ri::scene::Scene afterScene{};
        std::size_t beforeSelectedNode = 0;
        std::size_t afterSelectedNode = 0;
    };
    struct UiWorkbenchEditAction {
        fs::path manifestPath{};
        std::string beforeJson;
        std::string afterJson;
        int beforeScreenIndex = 0;
        int afterScreenIndex = 0;
        int beforeBlockIndex = -1;
        int afterBlockIndex = -1;
    };
    using EditorEditAction = std::variant<TransformEditAction, SceneGraphEditAction>;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        RawIronEditorWindow* self = nullptr;
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<LPCREATESTRUCTW>(lParam);
            self = static_cast<RawIronEditorWindow*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        } else {
            self = reinterpret_cast<RawIronEditorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self == nullptr) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        switch (message) {
            case WM_COMMAND: {
                if (self->resourceTextEditHwnd_ != nullptr
                    && reinterpret_cast<HWND>(lParam) == self->resourceTextEditHwnd_
                    && HIWORD(wParam) == EN_CHANGE) {
                    self->resourceFileDirty_ = true;
                }
            } break;
            case WM_TIMER:
                self->OnTick();
                return 0;
            case WM_KEYDOWN:
                return self->OnKeyDown(wParam);
            case WM_CHAR:
                return self->OnChar(wParam);
            case WM_LBUTTONDOWN:
                return self->OnLeftButtonDown(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_LBUTTONUP:
                return self->OnLeftButtonUp(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_MOUSEMOVE:
                return self->OnMouseMove(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)), wParam);
            case WM_CAPTURECHANGED:
                self->OnCaptureLost();
                break;
            case WM_MOUSEWHEEL: {
                const short delta = static_cast<short>(HIWORD(wParam));
                const int screenX = static_cast<short>(LOWORD(lParam));
                const int screenY = static_cast<short>(HIWORD(lParam));
                if (self->OnMouseWheel(delta, screenX, screenY)) {
                    return 0;
                }
                break;
            }
            case WM_SIZE:
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case WM_PAINT:
                self->Paint();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_CLOSE:
                if (!self->ResolveDirtyResourceBeforeContextSwitch("closing the editor")) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                self->DestroyResourceTextEditorControl();
                PostQuitMessage(0);
                return 0;
            default:
                break;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void OnTick() {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> delta = now - lastTick_;
        lastTick_ = now;
        elapsedSeconds_ += delta.count();
        statsOverlayState_.RecordFrameDeltaSeconds(delta.count());
        statsOverlayState_.SetAttached(true);
        statsOverlayState_.SetVisible(statsOverlayVisible_);
        ri::editor::AnimateEditorWorkspaceScene(sceneConfig_.editorPreviewScene, starterScene_, elapsedSeconds_);
        if (!autoOrbitPreview_) {
            ApplyEditorOrbitToScene();
        } else {
            editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
        }
        MaybeAutosaveState();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void ApplyEditorOrbitToScene() {
        editorOrbitState_.pitchDegrees = std::clamp(editorOrbitState_.pitchDegrees, -85.0f, 85.0f);
        editorOrbitState_.distance = std::clamp(editorOrbitState_.distance, 0.75f, 180.0f);
        ri::scene::SetOrbitCameraState(starterScene_.scene, starterScene_.handles.orbitCamera, editorOrbitState_);
    }

    void RefreshWorkspaceGamesAndResources() {
        workspaceGames_ = EnumerateWorkspaceGames(sceneConfig_.workspaceRoot);
        focusedWorkspaceGameIndex_ = 0;
        if (!workspaceGames_.empty() && sceneConfig_.gameManifest.has_value()) {
            const std::string& wantId = sceneConfig_.gameManifest->id;
            for (std::size_t i = 0; i < workspaceGames_.size(); ++i) {
                if (workspaceGames_[i].id == wantId) {
                    focusedWorkspaceGameIndex_ = static_cast<int>(i);
                    break;
                }
            }
        }
        RefreshWorkspaceResourceRows();
    }

    void RefreshWorkspaceResourceRows() {
        if (!ResolveDirtyResourceBeforeContextSwitch("refreshing resources")) {
            if (hwnd_ != nullptr) {
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        }
        resourceCatalogEntries_.clear();
        selectedResourceRow_ = -1;
        selectedResourceVisibleRow_ = -1;
        resourceCatalogScrollTopRow_ = 0;
        loadedResourceAbsolutePath_.clear();
        loadedResourceUtf8_.clear();
        resourceEditorAuxMessage_.clear();
        resourceFileDirty_ = false;
        DestroyResourceTextEditorControl();
        if (focusedWorkspaceGameIndex_ >= 0 &&
            focusedWorkspaceGameIndex_ < static_cast<int>(workspaceGames_.size())) {
            resourceCatalogEntries_ = CollectWorkspaceGameResources(
                workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)].rootPath);
        }
        RebuildFilteredResourceRows();
        if (hwnd_ != nullptr) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    [[nodiscard]] fs::path ResolveCreatorPolicyPath() const {
        if (!sceneConfig_.gameManifest.has_value()) {
            return {};
        }
        return BuildEditorSceneStatePath(sceneConfig_.workspaceRoot, sceneConfig_.gameManifest->id).parent_path()
            / "creator.policy";
    }

    void LoadCreatorPolicyFromDisk() {
        const fs::path policyPath = ResolveCreatorPolicyPath();
        if (policyPath.empty()) {
            return;
        }
        const ri::content::ScriptScalarMap scalars = ri::content::LoadScriptScalars(policyPath);
        if (scalars.empty()) {
            return;
        }
        const int presentation = ri::content::ScriptScalarOrIntClamped(scalars, "presentation", 2, 0, 2);
        creatorInventoryPolicy_.presentation = presentation == 0
            ? ri::world::InventoryPresentationMode::Disabled
            : (presentation == 1 ? ri::world::InventoryPresentationMode::HiddenDataOnly
                                 : ri::world::InventoryPresentationMode::Visible);
        creatorInventoryPolicy_.allowOffHand = ri::content::ScriptScalarOrBool(scalars, "allow_off_hand", true);
        creatorInventoryPolicy_.hotbarSize =
            ri::content::ScriptScalarOrIntClamped(scalars, "hotbar_size", 8, 1, 12);
        creatorInventoryPolicy_.backpackSize =
            ri::content::ScriptScalarOrIntClamped(scalars, "backpack_size", 24, 4, 64);
    }

    void SaveCreatorPolicyToDisk() {
        const fs::path policyPath = ResolveCreatorPolicyPath();
        if (policyPath.empty()) {
            return;
        }
        std::error_code ec{};
        fs::create_directories(policyPath.parent_path(), ec);
        int presentation = 2;
        switch (creatorInventoryPolicy_.presentation) {
        case ri::world::InventoryPresentationMode::Disabled:
            presentation = 0;
            break;
        case ri::world::InventoryPresentationMode::HiddenDataOnly:
            presentation = 1;
            break;
        case ri::world::InventoryPresentationMode::Visible:
            presentation = 2;
            break;
        }
        std::ostringstream body;
        body << "# RawIron editor creator policy (session defaults for playtest)\n";
        body << "presentation=" << presentation << '\n';
        body << "allow_off_hand=" << (creatorInventoryPolicy_.allowOffHand ? 1 : 0) << '\n';
        body << "hotbar_size=" << creatorInventoryPolicy_.hotbarSize << '\n';
        body << "backpack_size=" << creatorInventoryPolicy_.backpackSize << '\n';
        (void)ri::core::detail::WriteTextFile(policyPath, body.str());
    }

    void TryImportPrimaryLevelCsv() {
        if (!sceneConfig_.gameManifest.has_value() || sceneConfig_.gameManifest->primaryLevel.empty()) {
            return;
        }
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            return;
        }
        const fs::path levelPath = sceneConfig_.gameManifest->rootPath / sceneConfig_.gameManifest->primaryLevel;
        std::error_code ec{};
        if (!fs::exists(levelPath, ec)) {
            return;
        }
        const std::size_t importStart = starterScene_.scene.NodeCount();
        ri::scene::AssemblyPrimitivesImportResult importResult{};
        std::string importError;
        if (!ri::scene::TryImportAssemblyPrimitivesCsv(
                starterScene_.scene,
                starterScene_.handles.root,
                levelPath,
                &importResult,
                &importError)) {
            lastIoStatus_ += "  Level CSV import skipped: " + importError;
            return;
        }
        if (importStart < starterScene_.scene.NodeCount()) {
            authoredNodeStart_ = std::min(authoredNodeStart_, importStart);
        }
        lastIoStatus_ += "  Imported " + std::to_string(importResult.spawnedCount) + " primitives from "
            + sceneConfig_.gameManifest->primaryLevel + ".";
    }

    void ReloadEditorSceneForFocusedGame(bool importPrimaryLevelIfMissing) {
        if (focusedWorkspaceGameIndex_ < 0
            || focusedWorkspaceGameIndex_ >= static_cast<int>(workspaceGames_.size())) {
            return;
        }
        const WorkspaceGameEntry& game = workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)];
        std::optional<ri::content::GameManifest> manifest =
            ri::content::LoadGameManifest(game.rootPath / "manifest.json");
        if (!manifest.has_value()) {
            lastIoStatus_ = "Failed to load manifest for " + game.displayName + ".";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        sceneConfig_.gameManifest = manifest;
        sceneConfig_.sceneStatePath = BuildEditorSceneStatePath(sceneConfig_.workspaceRoot, manifest->id);
        sceneConfig_.sceneName = "EditorWorkspace_" + manifest->id;
        sceneConfig_.editorPreviewScene =
            manifest->editorPreviewScene.empty() ? "starter" : manifest->editorPreviewScene;
        sceneConfig_.workspaceLabel = std::string("Authoring — ") + manifest->name;
        sceneConfig_.windowTitle = std::string("RawIron Editor — ") + manifest->name;

        starterScene_ = ri::editor::BuildEditorWorkspaceScene(
            sceneConfig_.editorPreviewScene,
            sceneConfig_.sceneName,
            manifest->rootPath);
        editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
        (void)TryLoadEditorOrbitSidecar(sceneConfig_.sceneStatePath, editorOrbitState_);
        ApplyEditorOrbitToScene();
        EnsureEditorTrashFolder();
        authoredNodeStart_ = starterScene_.scene.NodeCount();
        baselineStarterScene_ = starterScene_.scene;
        undoStack_.clear();
        redoStack_.clear();
        selectedNode_ = starterScene_.handles.root >= 0 ? static_cast<std::size_t>(starterScene_.handles.root) : 0U;

        std::error_code loadEc{};
        if (fs::exists(ResolveSceneStatePath(), loadEc) || fs::exists(ResolveAuthoredSceneStatePath(), loadEc)) {
            (void)TryLoadPersistentEditorScene(ResolveSceneStatePath(), false);
        }
        LoadCreatorPolicyFromDisk();
        bool importCsv = importPrimaryLevelIfMissing;
        if (importCsv) {
            std::error_code importEc{};
            importCsv = !fs::exists(ResolveAuthoredSceneStatePath(), importEc);
        }
        if (importCsv) {
            TryImportPrimaryLevelCsv();
        }

        if (hwnd_ != nullptr) {
            SetWindowTextW(hwnd_, Widen(sceneConfig_.windowTitle).c_str());
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        lastIoStatus_ = "Switched project: " + game.displayName + " (preview reloaded). Use Ctrl+S to persist edits.";
    }

    void SwitchFocusedWorkspaceGame() {
        ReloadEditorSceneForFocusedGame(true);
        RefreshWorkspaceResourceRows();
        RebuildFilteredHierarchyOrder();
    }

    void TryScaffoldMountedGame() {
        if (!sceneConfig_.gameManifest.has_value()) {
            lastIoStatus_ = "Scaffold needs an open game manifest.";
            return;
        }
        std::size_t createdCount = 0;
        std::vector<std::string> createdFiles;
        std::string error;
        if (!EnsureMountedGameScaffold(*sceneConfig_.gameManifest, createdCount, createdFiles, &error)) {
            lastIoStatus_ = error.empty() ? "Scaffold failed." : error;
            return;
        }
        EnsureProjectDevConfig(sceneConfig_.gameManifest->rootPath);
        RefreshWorkspaceResourceRows();
        RebuildFilteredResourceRows();
        if (createdCount == 0) {
            lastIoStatus_ = "Project scaffold already present. Resources refreshed.";
            return;
        }
        lastIoStatus_ = "Created " + std::to_string(createdCount) + " missing authoring files.";
    }

    void OpenProjectResourceShortcut(std::string_view relativePath) {
        if (!ResolveDirtyResourceBeforeContextSwitch("opening a project shortcut")) {
            return;
        }
        int rowIndex = ri::editor::FindResourceRowByRelativePath(resourceCatalogEntries_, relativePath);
        if (rowIndex < 0 && sceneConfig_.gameManifest.has_value()) {
            TryScaffoldMountedGame();
            RefreshWorkspaceResourceRows();
            RebuildFilteredResourceRows();
            rowIndex = ri::editor::FindResourceRowByRelativePath(resourceCatalogEntries_, relativePath);
        }
        if (rowIndex < 0) {
            lastIoStatus_ = "Project shortcut not found: " + std::string(relativePath);
            return;
        }
        leftPanelMode_ = LeftPanelMode::Resources;
        SelectWorkspaceResourceRow(rowIndex);
        const EditorLayout layout = ComputeLayout();
        EnsureSelectedResourceVisible(layout.hierarchyInner);
    }

    void SelectWorkspaceResourceRow(const int rowIndex) {
        if (rowIndex < 0 || rowIndex >= static_cast<int>(resourceCatalogEntries_.size())) {
            return;
        }
        if (rowIndex == selectedResourceRow_) {
            inspectorPanel_ = InspectorPanel::Files;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (!ResolveDirtyResourceBeforeContextSwitch("opening another resource")) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        selectedResourceRow_ = rowIndex;
        selectedResourceVisibleRow_ = -1;
        for (int i = 0; i < static_cast<int>(filteredResourceRows_.size()); ++i) {
            if (filteredResourceRows_[static_cast<std::size_t>(i)] == rowIndex) {
                selectedResourceVisibleRow_ = i;
                break;
            }
        }
        inspectorPanel_ = InspectorPanel::Files;
        loadedResourceUtf8_.clear();
        resourceEditorAuxMessage_.clear();
        resourceFileDirty_ = false;
        DestroyResourceTextEditorControl();

        const WorkspaceResourceEntry& entry =
            resourceCatalogEntries_[static_cast<std::size_t>(rowIndex)];
        const ri::editor::ResourceDocumentData document = LoadResourceDocument(entry);
        loadedResourceAbsolutePath_ = document.absolutePath;
        loadedResourceUtf8_ = document.utf8;
        resourceEditorAuxMessage_ = document.auxMessage;
        resourceManifestIssues_ = document.manifestIssues;
        lastIoStatus_ = "Resource: " + entry.relativePathUtf8;
        if (document.isTextEditable) {
            const EditorLayout layout = ComputeLayout();
            EnsureResourceTextEditorCreated();
            LayoutResourceTextEditorControl(layout.inspectorInner);
#if defined(_WIN32)
            if (resourceTextEditHwnd_ != nullptr) {
                SetFocus(resourceTextEditHwnd_);
            }
#endif
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool IsUiManifestResource(const WorkspaceResourceEntry& entry) const {
        const std::string relLower = ToLowerAsciiCopy(entry.relativePathUtf8);
        if (entry.category == WorkspaceResourceCategory::UiScreen || entry.category == WorkspaceResourceCategory::Menu) {
            return relLower.ends_with(".json") || relLower.ends_with(".ui.json") || relLower.ends_with(".menu");
        }
        return relLower.ends_with(".ui.json");
    }

    [[nodiscard]] std::optional<fs::path> ResolveAutoUiWorkbenchManifestPath() const {
        if (selectedResourceRow_ >= 0 && selectedResourceRow_ < static_cast<int>(resourceCatalogEntries_.size())) {
            const WorkspaceResourceEntry& entry = resourceCatalogEntries_[static_cast<std::size_t>(selectedResourceRow_)];
            if (IsUiManifestResource(entry)) {
                return entry.absolutePath;
            }
        }
        for (const WorkspaceResourceEntry& entry : resourceCatalogEntries_) {
            if (!IsUiManifestResource(entry)) {
                continue;
            }
            if (entry.relativePathUtf8 == "ui/main.ui.json") {
                return entry.absolutePath;
            }
        }
        for (const WorkspaceResourceEntry& entry : resourceCatalogEntries_) {
            if (!IsUiManifestResource(entry)) {
                continue;
            }
            if (entry.relativePathUtf8 == "ui/vn_intro.ui.json") {
                return entry.absolutePath;
            }
        }
        for (const WorkspaceResourceEntry& entry : resourceCatalogEntries_) {
            if (IsUiManifestResource(entry)) {
                return entry.absolutePath;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<fs::path> ResolveUiWorkbenchManifestPath() const {
        switch (uiWorkbenchSource_) {
            case UiWorkbenchSource::AutoSelection:
                return ResolveAutoUiWorkbenchManifestPath();
            case UiWorkbenchSource::MenuSample:
                return ri::ui::DefaultUiManifestPath(sceneConfig_.workspaceRoot);
            case UiWorkbenchSource::VnSample:
                return ri::ui::VisualNovelDemoManifestPath(sceneConfig_.workspaceRoot);
        }
        return std::nullopt;
    }

    [[nodiscard]] UiWorkbenchPanelModel BuildUiWorkbenchPanelModel(const RECT& inspectorInner) {
        UiWorkbenchPanelModel model{};
        model.layout = ComputeUiWorkbenchLayout(inspectorInner);
        model.headingLine = "2D / UI / VN workbench";
        model.usingAutoSource = uiWorkbenchSource_ == UiWorkbenchSource::AutoSelection;
        model.usingMenuSample = uiWorkbenchSource_ == UiWorkbenchSource::MenuSample;
        model.usingVnSample = uiWorkbenchSource_ == UiWorkbenchSource::VnSample;
        model.hintLine = "Auto mode stays inside the mounted game. Workspace demos are opt-in only.";
        model.actionsHeaderLine = "Authoring actions write directly into the selected game-local manifest.";
        model.blockActionsHeaderLine = "Block actions target the selected screen card and selected block below.";
        model.previewFooterLine = "F2 edits the selected block or screen title. Shift+F2 edits dialogue speaker when available.";

        const std::optional<fs::path> manifestPath = ResolveUiWorkbenchManifestPath();
        if (!manifestPath.has_value()) {
            model.sourceLine = "Source: no UI manifest resolved";
            model.statusLine = "No mounted UI manifest";
            model.errorLine = "Use Scaffold to create `ui/main.ui.json` and `ui/vn_intro.ui.json` under the mounted game. Demo buttons are optional templates only.";
            model.screenHeaderLine = "Screens";
            model.previewTitleLine = "Preview stage";
            model.previewMetaLine = "No screen available";
            return model;
        }

        model.manifestResolved = true;
        std::error_code relEc{};
        fs::path shownPath = fs::relative(*manifestPath, sceneConfig_.workspaceRoot, relEc);
        if (relEc || shownPath.empty()) {
            shownPath = *manifestPath;
        }
        model.sourceLine = "Source: " + shownPath.generic_string();

        ri::ui::UiManifest manifest{};
        std::string error;
        bool parsed = false;
        if (loadedResourceAbsolutePath_ == *manifestPath && !loadedResourceUtf8_.empty()) {
            parsed = ri::ui::TryParseUiManifestFromJson(loadedResourceUtf8_, manifest, &error);
        } else {
            parsed = ri::ui::TryLoadUiManifestFromJsonFile(*manifestPath, manifest, &error);
        }
        if (!parsed) {
            model.statusLine = "Manifest parse failed";
            model.errorLine = error.empty() ? "Unknown parse error." : error;
            model.screenHeaderLine = "Screens";
            model.previewTitleLine = manifestPath->filename().string();
            model.previewMetaLine = "Invalid UI manifest";
            return model;
        }

        model.manifestParsed = true;
        model.statusLine = "Manifest loaded";
        model.screenHeaderLine =
            std::to_string(manifest.screens.size()) + " screen(s)  |  " + std::to_string(manifest.variables.size()) + " variable(s)";

        if (manifest.screens.empty()) {
            model.previewTitleLine = manifestPath->filename().string();
            model.previewMetaLine = "Manifest has no screens";
            return model;
        }

        const int screenCount = static_cast<int>(manifest.screens.size());
        uiWorkbenchSelectedScreenIndex_ = std::clamp(uiWorkbenchSelectedScreenIndex_, 0, screenCount - 1);
        const ri::ui::UiScreen& selectedScreen =
            manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)];

        for (int i = 0; i < screenCount; ++i) {
            const ri::ui::UiScreen& screen = manifest.screens[static_cast<std::size_t>(i)];
            UiWorkbenchScreenSummary summary{};
            summary.selected = i == uiWorkbenchSelectedScreenIndex_;
            summary.titleLine = std::to_string(i + 1) + ". " + (screen.title.empty() ? screen.id : screen.title);
            summary.metaLine = screen.id + "  |  " + std::to_string(screen.blocks.size()) + " blocks";
            model.screens.push_back(std::move(summary));
        }

        model.previewTitleLine = selectedScreen.title.empty() ? selectedScreen.id : selectedScreen.title;
        model.previewMetaLine =
            selectedScreen.id + "  |  blocks " + std::to_string(selectedScreen.blocks.size()) + "  |  start "
            + (manifest.startScreenId == selectedScreen.id ? "yes" : "no");

        if (uiWorkbenchTextEditActive_) {
            model.statusLine = "Editing " + uiWorkbenchTextEditLabel_;
            model.hintLine = "Type in-place, then Enter to apply or Esc to cancel.";
            model.previewFooterLine = "Edit draft: " + (uiWorkbenchTextEditDraft_.empty() ? std::string("<empty>") : uiWorkbenchTextEditDraft_);
        }

        uiWorkbenchSelectedBlockIndex_ = selectedScreen.blocks.empty()
            ? -1
            : std::clamp(uiWorkbenchSelectedBlockIndex_, 0, static_cast<int>(selectedScreen.blocks.size()) - 1);

        for (std::size_t blockIndex = 0; blockIndex < selectedScreen.blocks.size(); ++blockIndex) {
            const ri::ui::UiBlock& block = selectedScreen.blocks[blockIndex];
            UiWorkbenchPreviewBlock preview{};
            switch (block.kind) {
                case ri::ui::UiBlockKind::Heading:
                    preview.tone = UiWorkbenchBlockTone::Heading;
                    preview.titleLine = "Heading";
                    preview.detailLine = block.text;
                    break;
                case ri::ui::UiBlockKind::Say:
                    preview.tone = UiWorkbenchBlockTone::Say;
                    preview.titleLine = block.speaker.empty() ? "Dialogue" : ("Say - " + block.speaker);
                    preview.detailLine = block.text;
                    break;
                case ri::ui::UiBlockKind::Narration:
                    preview.tone = UiWorkbenchBlockTone::Narration;
                    preview.titleLine = "Narration";
                    preview.detailLine = block.text;
                    break;
                case ri::ui::UiBlockKind::Choices:
                    preview.tone = UiWorkbenchBlockTone::Choices;
                    preview.titleLine = "Choices";
                    preview.detailLine = std::to_string(block.choices.size()) + " branch option(s)";
                    break;
                case ri::ui::UiBlockKind::Image:
                    preview.tone = UiWorkbenchBlockTone::Image;
                    preview.titleLine = "Image";
                    preview.detailLine = block.imageRelativePath.empty() ? "No image path" : block.imageRelativePath;
                    break;
                case ri::ui::UiBlockKind::HistoryNote:
                    preview.tone = UiWorkbenchBlockTone::Note;
                    preview.titleLine = "History note";
                    preview.detailLine = block.text;
                    break;
                case ri::ui::UiBlockKind::Label:
                    preview.tone = UiWorkbenchBlockTone::Other;
                    preview.titleLine = "Label";
                    preview.detailLine = block.text;
                    break;
                case ri::ui::UiBlockKind::Paragraph:
                    preview.tone = UiWorkbenchBlockTone::Other;
                    preview.titleLine = "Paragraph";
                    preview.detailLine = block.text;
                    break;
                case ri::ui::UiBlockKind::Spacer:
                    preview.tone = UiWorkbenchBlockTone::Other;
                    preview.titleLine = "Spacer";
                    preview.detailLine = "Height " + std::to_string(static_cast<int>(block.spacerHeight));
                    break;
                case ri::ui::UiBlockKind::Separator:
                    preview.tone = UiWorkbenchBlockTone::Other;
                    preview.titleLine = "Separator";
                    break;
                case ri::ui::UiBlockKind::Button:
                    preview.tone = UiWorkbenchBlockTone::Choices;
                    preview.titleLine = "Button";
                    preview.detailLine = block.label;
                    break;
            }
            if (preview.detailLine.size() > 72) {
                preview.detailLine = preview.detailLine.substr(0, 69) + "...";
            }
            preview.selected = static_cast<int>(blockIndex) == uiWorkbenchSelectedBlockIndex_;
            model.previewBlocks.push_back(std::move(preview));
        }

        return model;
    }

    void CycleUiWorkbenchScreen(const int delta) {
        const std::optional<fs::path> manifestPath = ResolveUiWorkbenchManifestPath();
        if (!manifestPath.has_value()) {
            lastIoStatus_ = "UI/VN workbench: no manifest available.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        ri::ui::UiManifest manifest{};
        std::string error;
        const bool parsed =
            (loadedResourceAbsolutePath_ == *manifestPath && !loadedResourceUtf8_.empty())
                ? ri::ui::TryParseUiManifestFromJson(loadedResourceUtf8_, manifest, &error)
                : ri::ui::TryLoadUiManifestFromJsonFile(*manifestPath, manifest, &error);
        if (!parsed || manifest.screens.empty()) {
            lastIoStatus_ = "UI/VN workbench: " + (error.empty() ? std::string("manifest unavailable.") : error);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const int screenCount = static_cast<int>(manifest.screens.size());
        uiWorkbenchSelectedScreenIndex_ =
            (uiWorkbenchSelectedScreenIndex_ + delta + screenCount) % screenCount;
        const auto& blocks =
            manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
        uiWorkbenchSelectedBlockIndex_ = blocks.empty()
            ? -1
            : std::clamp(uiWorkbenchSelectedBlockIndex_, 0, static_cast<int>(blocks.size()) - 1);
        lastIoStatus_ = "UI/VN workbench: screen " + std::to_string(uiWorkbenchSelectedScreenIndex_ + 1) + " / "
            + std::to_string(screenCount) + ".";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] std::vector<RECT> ComputeUiWorkbenchScreenRowRects(const RECT& inspectorInner, const int screenCount) const {
        const RECT manifestCard{inspectorInner.left + 10, inspectorInner.top + 258, inspectorInner.right - 10, inspectorInner.top + 334};
        const RECT railCard{inspectorInner.left + 10, manifestCard.bottom + 10, inspectorInner.right - 10, manifestCard.bottom + 120};
        std::vector<RECT> rects{};
        rects.reserve(static_cast<std::size_t>(std::max(0, screenCount)));
        int rowTop = railCard.top + 34;
        for (int i = 0; i < screenCount; ++i) {
            RECT rowRect{railCard.left + 10, rowTop, railCard.right - 10, rowTop + 22};
            if (rowRect.bottom > railCard.bottom - 28) {
                break;
            }
            rects.push_back(rowRect);
            rowTop += 24;
        }
        return rects;
    }

    [[nodiscard]] std::vector<RECT> ComputeUiWorkbenchPreviewBlockRects(const RECT& inspectorInner,
                                                                        const UiWorkbenchPanelModel& model) const {
        const RECT manifestCard{inspectorInner.left + 10, inspectorInner.top + 258, inspectorInner.right - 10, inspectorInner.top + 334};
        const RECT railCard{inspectorInner.left + 10, manifestCard.bottom + 10, inspectorInner.right - 10, manifestCard.bottom + 120};
        const RECT previewCard{inspectorInner.left + 10, railCard.bottom + 10, inspectorInner.right - 10, inspectorInner.bottom - 72};
        const RECT stageRect{previewCard.left + 14, previewCard.top + 58, previewCard.right - 14, previewCard.bottom - 32};
        std::vector<RECT> rects{};
        rects.reserve(model.previewBlocks.size());
        int blockTop = stageRect.top + 12;
        for (const UiWorkbenchPreviewBlock& block : model.previewBlocks) {
            const int blockHeight = block.detailLine.empty() ? 28 : 44;
            RECT blockRect{stageRect.left + 12, blockTop, stageRect.right - 12, blockTop + blockHeight};
            if (blockRect.bottom > stageRect.bottom - 8) {
                break;
            }
            rects.push_back(blockRect);
            blockTop += blockHeight + 10;
        }
        return rects;
    }

    void SelectUiWorkbenchScreen(const int screenIndex) {
        fs::path manifestPath;
        ri::ui::UiManifest manifest{};
        std::string error;
        if (!LoadUiWorkbenchManifest(manifestPath, manifest, error) || manifest.screens.empty()) {
            lastIoStatus_ = "UI/VN workbench: " + (error.empty() ? std::string("no screen available.") : error);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        uiWorkbenchSelectedScreenIndex_ = std::clamp(screenIndex, 0, static_cast<int>(manifest.screens.size()) - 1);
        const auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
        uiWorkbenchSelectedBlockIndex_ = blocks.empty()
            ? -1
            : std::clamp(uiWorkbenchSelectedBlockIndex_, 0, static_cast<int>(blocks.size()) - 1);
        lastIoStatus_ = "UI/VN workbench: selected screen "
            + std::to_string(uiWorkbenchSelectedScreenIndex_ + 1) + " / " + std::to_string(manifest.screens.size()) + ".";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void SelectUiWorkbenchBlock(const int blockIndex) {
        fs::path manifestPath;
        ri::ui::UiManifest manifest{};
        std::string error;
        if (!LoadUiWorkbenchManifest(manifestPath, manifest, error) || manifest.screens.empty()) {
            lastIoStatus_ = "UI/VN workbench: " + (error.empty() ? std::string("no block available.") : error);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        uiWorkbenchSelectedScreenIndex_ =
            std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
        const auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
        if (blocks.empty()) {
            uiWorkbenchSelectedBlockIndex_ = -1;
            lastIoStatus_ = "UI/VN workbench: selected screen has no blocks yet.";
        } else {
            uiWorkbenchSelectedBlockIndex_ = std::clamp(blockIndex, 0, static_cast<int>(blocks.size()) - 1);
            lastIoStatus_ = "UI/VN workbench: selected block "
                + std::to_string(uiWorkbenchSelectedBlockIndex_ + 1) + " / " + std::to_string(blocks.size()) + ".";
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool TryHandleUiWorkbenchInspectorSelectionClick(const RECT& inspectorInner, const POINT& point) {
        if (inspectorPanel_ != InspectorPanel::UiWorkbench) {
            return false;
        }
        UiWorkbenchPanelModel model = BuildUiWorkbenchPanelModel(inspectorInner);
        const std::vector<RECT> screenRows = ComputeUiWorkbenchScreenRowRects(inspectorInner, static_cast<int>(model.screens.size()));
        for (std::size_t i = 0; i < screenRows.size(); ++i) {
            if (PtInRect(&screenRows[i], point) != FALSE) {
                SelectUiWorkbenchScreen(static_cast<int>(i));
                return true;
            }
        }
        const std::vector<RECT> blockRows = ComputeUiWorkbenchPreviewBlockRects(inspectorInner, model);
        for (std::size_t i = 0; i < blockRows.size(); ++i) {
            if (PtInRect(&blockRows[i], point) != FALSE) {
                SelectUiWorkbenchBlock(static_cast<int>(i));
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool LoadUiWorkbenchManifest(fs::path& manifestPathOut, ri::ui::UiManifest& manifestOut, std::string& errorOut) const {
        const std::optional<fs::path> manifestPath = ResolveUiWorkbenchManifestPath();
        if (!manifestPath.has_value()) {
            errorOut = "no manifest available";
            return false;
        }
        manifestPathOut = *manifestPath;
        if (loadedResourceAbsolutePath_ == *manifestPath && !loadedResourceUtf8_.empty()) {
            return ri::ui::TryParseUiManifestFromJson(loadedResourceUtf8_, manifestOut, &errorOut);
        }
        return ri::ui::TryLoadUiManifestFromJsonFile(*manifestPath, manifestOut, &errorOut);
    }

    [[nodiscard]] bool SaveUiWorkbenchManifest(const fs::path& manifestPath,
                                               const ri::ui::UiManifest& manifest,
                                               std::string_view successLabel) {
        const std::string json = ri::ui::SerializeUiManifestToJson(manifest);
        if (!ri::core::detail::WriteTextFile(manifestPath, json)) {
            lastIoStatus_ = "UI/VN workbench: failed to write " + manifestPath.filename().string() + ".";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return false;
        }
        if (loadedResourceAbsolutePath_ == manifestPath) {
            loadedResourceUtf8_ = json;
            resourceFileDirty_ = false;
        }
        RefreshWorkspaceResourceRows();
        lastIoStatus_ = std::string(successLabel);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    void PushUiWorkbenchEditAction(const UiWorkbenchEditAction& action) {
        uiWorkbenchUndoStack_.push_back(action);
        if (uiWorkbenchUndoStack_.size() > kMaxUndoActions) {
            uiWorkbenchUndoStack_.erase(uiWorkbenchUndoStack_.begin());
        }
        uiWorkbenchRedoStack_.clear();
    }

    [[nodiscard]] bool ApplyUiWorkbenchSnapshot(const UiWorkbenchEditAction& action,
                                                const bool useAfter,
                                                std::string_view successLabel) {
        const std::string& snapshot = useAfter ? action.afterJson : action.beforeJson;
        const int screenIndex = useAfter ? action.afterScreenIndex : action.beforeScreenIndex;
        const int blockIndex = useAfter ? action.afterBlockIndex : action.beforeBlockIndex;
        if (!ri::core::detail::WriteTextFile(action.manifestPath, snapshot)) {
            lastIoStatus_ = "UI/VN workbench: failed to apply snapshot.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return false;
        }
        if (loadedResourceAbsolutePath_ == action.manifestPath) {
            loadedResourceUtf8_ = snapshot;
            resourceFileDirty_ = false;
        }
        uiWorkbenchSelectedScreenIndex_ = screenIndex;
        uiWorkbenchSelectedBlockIndex_ = blockIndex;
        RefreshWorkspaceResourceRows();
        lastIoStatus_ = std::string(successLabel);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    template <typename Mutator>
    void ApplyUiWorkbenchMutation(std::string_view successLabel, Mutator&& mutator) {
        fs::path manifestPath;
        ri::ui::UiManifest manifest{};
        std::string error;
        if (!LoadUiWorkbenchManifest(manifestPath, manifest, error)) {
            lastIoStatus_ = "UI/VN workbench: " + error;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const std::string beforeJson = ri::ui::SerializeUiManifestToJson(manifest);
        const int beforeScreenIndex = uiWorkbenchSelectedScreenIndex_;
        const int beforeBlockIndex = uiWorkbenchSelectedBlockIndex_;
        if (!mutator(manifest)) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const std::string afterJson = ri::ui::SerializeUiManifestToJson(manifest);
        if (beforeJson == afterJson) {
            lastIoStatus_ = "UI/VN workbench: no manifest change.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (!SaveUiWorkbenchManifest(manifestPath, manifest, successLabel)) {
            return;
        }
        PushUiWorkbenchEditAction(UiWorkbenchEditAction{
            .manifestPath = manifestPath,
            .beforeJson = beforeJson,
            .afterJson = afterJson,
            .beforeScreenIndex = beforeScreenIndex,
            .afterScreenIndex = uiWorkbenchSelectedScreenIndex_,
            .beforeBlockIndex = beforeBlockIndex,
            .afterBlockIndex = uiWorkbenchSelectedBlockIndex_,
        });
    }

    void UiWorkbenchCreateScreen() {
        ApplyUiWorkbenchMutation("UI/VN workbench: created new screen.", [this](ri::ui::UiManifest& manifest) {
            ri::ui::UiScreen screen{};
            const int nextIndex = static_cast<int>(manifest.screens.size()) + 1;
            screen.id = "screen_" + std::to_string(nextIndex);
            screen.title = "New Screen " + std::to_string(nextIndex);
            screen.backgroundRgba = {0.04f, 0.05f, 0.10f, 0.98f};
            screen.blocks.push_back(ri::ui::UiBlock{
                .kind = ri::ui::UiBlockKind::Heading,
                .text = screen.title,
                .align = "center",
            });
            screen.blocks.push_back(ri::ui::UiBlock{
                .kind = ri::ui::UiBlockKind::Paragraph,
                .text = "Replace this placeholder text in the Files view or a future direct editor.",
                .align = "center",
            });
            manifest.screens.push_back(std::move(screen));
            uiWorkbenchSelectedScreenIndex_ = static_cast<int>(manifest.screens.size()) - 1;
            uiWorkbenchSelectedBlockIndex_ = 0;
            if (manifest.startScreenId.empty()) {
                manifest.startScreenId = manifest.screens.front().id;
            }
            return true;
        });
    }

    void UiWorkbenchDuplicateScreen() {
        ApplyUiWorkbenchMutation("UI/VN workbench: duplicated current screen.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "UI/VN workbench: nothing to duplicate.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            ri::ui::UiScreen duplicate = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)];
              duplicate.id += "_copy";
              duplicate.title += " Copy";
              manifest.screens.push_back(std::move(duplicate));
              uiWorkbenchSelectedScreenIndex_ = static_cast<int>(manifest.screens.size()) - 1;
              uiWorkbenchSelectedBlockIndex_ = manifest.screens.back().blocks.empty() ? -1 : 0;
              return true;
          });
      }

    void UiWorkbenchAddChoicesBlock() {
        ApplyUiWorkbenchMutation("UI/VN workbench: added choices block.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "UI/VN workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            ri::ui::UiBlock block{};
            block.kind = ri::ui::UiBlockKind::Choices;
            block.choices.push_back(ri::ui::UiChoiceItem{
                .label = "Choice A",
                .action = ri::ui::UiAction{.kind = ri::ui::UiActionKind::Navigate, .target = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].id},
            });
            block.choices.push_back(ri::ui::UiChoiceItem{
                .label = "Choice B",
                .action = ri::ui::UiAction{.kind = ri::ui::UiActionKind::Navigate, .target = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].id},
            });
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            blocks.push_back(std::move(block));
            uiWorkbenchSelectedBlockIndex_ = static_cast<int>(blocks.size()) - 1;
            return true;
        });
    }

    void UiWorkbenchSetStartScreen() {
        ApplyUiWorkbenchMutation("UI/VN workbench: updated start screen.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "UI/VN workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            manifest.startScreenId = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].id;
            return true;
        });
    }

    void UiWorkbenchAddDialogueBlock() {
        ApplyUiWorkbenchMutation("UI/VN workbench: added dialogue block.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "UI/VN workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            ri::ui::UiBlock block{};
            block.kind = ri::ui::UiBlockKind::Say;
            block.speaker = "Speaker";
            block.text = "New dialogue line.";
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            blocks.push_back(std::move(block));
            uiWorkbenchSelectedBlockIndex_ = static_cast<int>(blocks.size()) - 1;
            return true;
        });
    }

    void UiWorkbenchAddNarrationBlock() {
        ApplyUiWorkbenchMutation("UI/VN workbench: added narration block.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "UI/VN workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            ri::ui::UiBlock block{};
            block.kind = ri::ui::UiBlockKind::Narration;
            block.text = "New narration line.";
            block.align = "left";
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            blocks.push_back(std::move(block));
            uiWorkbenchSelectedBlockIndex_ = static_cast<int>(blocks.size()) - 1;
            return true;
        });
    }

    void UiWorkbenchMoveBlock(const int delta) {
        ApplyUiWorkbenchMutation(delta < 0 ? "UI/VN workbench: moved block up." : "UI/VN workbench: moved block down.",
                                 [this, delta](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "UI/VN workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            if (blocks.empty() || uiWorkbenchSelectedBlockIndex_ < 0 || uiWorkbenchSelectedBlockIndex_ >= static_cast<int>(blocks.size())) {
                lastIoStatus_ = "UI/VN workbench: no block selected.";
                return false;
            }
            const int nextIndex = uiWorkbenchSelectedBlockIndex_ + delta;
            if (nextIndex < 0 || nextIndex >= static_cast<int>(blocks.size())) {
                lastIoStatus_ = "UI/VN workbench: block is already at the edge.";
                return false;
            }
            std::swap(blocks[static_cast<std::size_t>(uiWorkbenchSelectedBlockIndex_)], blocks[static_cast<std::size_t>(nextIndex)]);
            uiWorkbenchSelectedBlockIndex_ = nextIndex;
            return true;
        });
    }

    void UiWorkbenchDeleteSelectedBlock() {
        ApplyUiWorkbenchMutation("UI/VN workbench: deleted selected block.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "UI/VN workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            if (blocks.empty() || uiWorkbenchSelectedBlockIndex_ < 0 || uiWorkbenchSelectedBlockIndex_ >= static_cast<int>(blocks.size())) {
                lastIoStatus_ = "UI/VN workbench: no block selected.";
                return false;
            }
            blocks.erase(blocks.begin() + uiWorkbenchSelectedBlockIndex_);
            if (blocks.empty()) {
                uiWorkbenchSelectedBlockIndex_ = -1;
            } else {
                uiWorkbenchSelectedBlockIndex_ = std::clamp(uiWorkbenchSelectedBlockIndex_, 0, static_cast<int>(blocks.size()) - 1);
            }
            return true;
        });
    }

    void BeginUiWorkbenchTextEdit(const UiWorkbenchTextEditTarget target,
                                  std::string draft,
                                  std::string label) {
        uiWorkbenchTextEditTarget_ = target;
        uiWorkbenchTextEditDraft_ = std::move(draft);
        uiWorkbenchTextEditLabel_ = std::move(label);
        uiWorkbenchTextEditActive_ = true;
        lastIoStatus_ = "UI/VN edit: " + uiWorkbenchTextEditLabel_ + "  (type, Enter=apply, Esc=cancel)";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void TryBeginUiWorkbenchPrimaryEdit() {
        if (inspectorPanel_ != InspectorPanel::UiWorkbench) {
            return;
        }
        fs::path manifestPath;
        ri::ui::UiManifest manifest{};
        std::string error;
        if (!LoadUiWorkbenchManifest(manifestPath, manifest, error) || manifest.screens.empty()) {
            lastIoStatus_ = "UI/VN workbench: " + (error.empty() ? std::string("no manifest available.") : error);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        uiWorkbenchSelectedScreenIndex_ =
            std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
        ri::ui::UiScreen& screen = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)];
        if (uiWorkbenchSelectedBlockIndex_ < 0 || uiWorkbenchSelectedBlockIndex_ >= static_cast<int>(screen.blocks.size())) {
            BeginUiWorkbenchTextEdit(UiWorkbenchTextEditTarget::ScreenTitle,
                                     screen.title.empty() ? screen.id : screen.title,
                                     "screen title");
            return;
        }
        const ri::ui::UiBlock& block = screen.blocks[static_cast<std::size_t>(uiWorkbenchSelectedBlockIndex_)];
        switch (block.kind) {
            case ri::ui::UiBlockKind::Button:
                BeginUiWorkbenchTextEdit(UiWorkbenchTextEditTarget::BlockLabel, block.label, "button label");
                return;
            case ri::ui::UiBlockKind::Image:
                BeginUiWorkbenchTextEdit(UiWorkbenchTextEditTarget::BlockImagePath, block.imageRelativePath, "image path");
                return;
            case ri::ui::UiBlockKind::Spacer:
            case ri::ui::UiBlockKind::Separator:
                lastIoStatus_ = "UI/VN workbench: selected block has no direct text field. Select a text-bearing block.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            default:
                BeginUiWorkbenchTextEdit(UiWorkbenchTextEditTarget::BlockText, block.text, "block text");
                return;
        }
    }

    void TryBeginUiWorkbenchSecondaryEdit() {
        if (inspectorPanel_ != InspectorPanel::UiWorkbench) {
            return;
        }
        fs::path manifestPath;
        ri::ui::UiManifest manifest{};
        std::string error;
        if (!LoadUiWorkbenchManifest(manifestPath, manifest, error) || manifest.screens.empty()) {
            lastIoStatus_ = "UI/VN workbench: " + (error.empty() ? std::string("no manifest available.") : error);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        uiWorkbenchSelectedScreenIndex_ =
            std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
        ri::ui::UiScreen& screen = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)];
        if (uiWorkbenchSelectedBlockIndex_ < 0 || uiWorkbenchSelectedBlockIndex_ >= static_cast<int>(screen.blocks.size())) {
            lastIoStatus_ = "UI/VN workbench: select a dialogue block to edit its speaker.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const ri::ui::UiBlock& block = screen.blocks[static_cast<std::size_t>(uiWorkbenchSelectedBlockIndex_)];
        if (block.kind != ri::ui::UiBlockKind::Say) {
            lastIoStatus_ = "UI/VN workbench: secondary edit is available on dialogue speaker fields only.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        BeginUiWorkbenchTextEdit(UiWorkbenchTextEditTarget::BlockSpeaker, block.speaker, "dialogue speaker");
    }

    void CancelUiWorkbenchTextEdit() {
        uiWorkbenchTextEditActive_ = false;
        uiWorkbenchTextEditTarget_ = UiWorkbenchTextEditTarget::None;
        uiWorkbenchTextEditDraft_.clear();
        uiWorkbenchTextEditLabel_.clear();
        lastIoStatus_ = "UI/VN edit cancelled.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void CommitUiWorkbenchTextEdit() {
        if (!uiWorkbenchTextEditActive_) {
            return;
        }
        const UiWorkbenchTextEditTarget target = uiWorkbenchTextEditTarget_;
        const std::string draft = uiWorkbenchTextEditDraft_;
        const std::string label = uiWorkbenchTextEditLabel_;
        uiWorkbenchTextEditActive_ = false;
        uiWorkbenchTextEditTarget_ = UiWorkbenchTextEditTarget::None;
        uiWorkbenchTextEditDraft_.clear();
        uiWorkbenchTextEditLabel_.clear();
        ApplyUiWorkbenchMutation("UI/VN workbench: updated " + label + ".", [this, target, draft](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "UI/VN workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            ri::ui::UiScreen& screen = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)];
            switch (target) {
                case UiWorkbenchTextEditTarget::ScreenTitle:
                    screen.title = draft;
                    if (screen.title.empty()) {
                        screen.title = screen.id;
                    }
                    return true;
                case UiWorkbenchTextEditTarget::BlockText:
                case UiWorkbenchTextEditTarget::BlockSpeaker:
                case UiWorkbenchTextEditTarget::BlockLabel:
                case UiWorkbenchTextEditTarget::BlockImagePath:
                    break;
                case UiWorkbenchTextEditTarget::None:
                    lastIoStatus_ = "UI/VN workbench: no edit target active.";
                    return false;
            }
            auto& blocks = screen.blocks;
            if (uiWorkbenchSelectedBlockIndex_ < 0 || uiWorkbenchSelectedBlockIndex_ >= static_cast<int>(blocks.size())) {
                lastIoStatus_ = "UI/VN workbench: no block selected.";
                return false;
            }
            ri::ui::UiBlock& block = blocks[static_cast<std::size_t>(uiWorkbenchSelectedBlockIndex_)];
            switch (target) {
                case UiWorkbenchTextEditTarget::BlockText:
                    block.text = draft;
                    return true;
                case UiWorkbenchTextEditTarget::BlockSpeaker:
                    block.speaker = draft;
                    return true;
                case UiWorkbenchTextEditTarget::BlockLabel:
                    block.label = draft;
                    return true;
                case UiWorkbenchTextEditTarget::BlockImagePath:
                    block.imageRelativePath = draft;
                    return true;
                case UiWorkbenchTextEditTarget::ScreenTitle:
                case UiWorkbenchTextEditTarget::None:
                    return false;
            }
            return false;
        });
    }

    void UiWorkbenchUndo() {
        if (uiWorkbenchUndoStack_.empty()) {
            lastIoStatus_ = "UI/VN workbench: nothing to undo.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const UiWorkbenchEditAction action = uiWorkbenchUndoStack_.back();
        uiWorkbenchUndoStack_.pop_back();
        if (ApplyUiWorkbenchSnapshot(action, false, "UI/VN workbench: undo applied.")) {
            uiWorkbenchRedoStack_.push_back(action);
        }
    }

    void UiWorkbenchRedo() {
        if (uiWorkbenchRedoStack_.empty()) {
            lastIoStatus_ = "UI/VN workbench: nothing to redo.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const UiWorkbenchEditAction action = uiWorkbenchRedoStack_.back();
        uiWorkbenchRedoStack_.pop_back();
        if (ApplyUiWorkbenchSnapshot(action, true, "UI/VN workbench: redo applied.")) {
            uiWorkbenchUndoStack_.push_back(action);
        }
    }

    void SetUiWorkbenchSource(const UiWorkbenchSource source) {
        uiWorkbenchSource_ = source;
        uiWorkbenchSelectedScreenIndex_ = 0;
        uiWorkbenchSelectedBlockIndex_ = -1;
        lastIoStatus_ = "UI/VN workbench source updated.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void DestroyResourceTextEditorControl() {
#if defined(_WIN32)
        ri::editor::DestroyResourceTextEditorControl(resourceTextEditHwnd_);
#endif
    }

#if defined(_WIN32)
    void EnsureResourceTextEditorCreated() {
        ri::editor::EnsureResourceTextEditorCreated(hwnd_,
                                                    resourceTextEditHwnd_,
                                                    bodyFont_,
                                                    inspectorPanel_ == InspectorPanel::Files,
                                                    loadedResourceAbsolutePath_,
                                                    loadedResourceUtf8_,
                                                    resourceEditorAuxMessage_,
                                                    resourceFileDirty_);
        ri::editor::SyncResourceTextEditorContent(resourceTextEditHwnd_, loadedResourceUtf8_, resourceFileDirty_);
    }

    void LayoutResourceTextEditorControl(const RECT& inspectorInner) {
        ri::editor::LayoutResourceTextEditorControl(hwnd_,
                                                    resourceTextEditHwnd_,
                                                    inspectorPanel_ == InspectorPanel::Files,
                                                    loadedResourceAbsolutePath_,
                                                    resourceEditorAuxMessage_,
                                                    inspectorInner);
    }

    void SyncResourceTextEditorContent() {
        ri::editor::SyncResourceTextEditorContent(resourceTextEditHwnd_, loadedResourceUtf8_, resourceFileDirty_);
    }

    [[nodiscard]] bool SaveActiveResourceFileFromEditor() {
        if (!ri::editor::SaveActiveResourceFileFromEditor(
                resourceTextEditHwnd_, loadedResourceAbsolutePath_, loadedResourceUtf8_)) {
            return false;
        }
        resourceFileDirty_ = false;
        lastIoStatus_ = "Saved resource: " + loadedResourceAbsolutePath_.filename().string();
        return true;
    }

    void OpenActiveResourceInExplorer() const {
        ri::editor::OpenActiveResourceInExplorer(hwnd_, loadedResourceAbsolutePath_);
    }
#else
    void EnsureResourceTextEditorCreated() {
    }
    void LayoutResourceTextEditorControl(const RECT& /*inspectorInner*/) {
    }
    void SyncResourceTextEditorContent() {
    }
    [[nodiscard]] bool SaveActiveResourceFileFromEditor() {
        return false;
    }
    void OpenActiveResourceInExplorer() const {
    }
#endif

    [[nodiscard]] bool ResolveDirtyResourceBeforeContextSwitch(std::string_view action) {
        return ri::editor::ResolveDirtyResourceBeforeContextSwitch(
            hwnd_,
            action,
            loadedResourceAbsolutePath_,
            resourceFileDirty_,
            [this]() { return this->SaveActiveResourceFileFromEditor(); },
            lastIoStatus_);
    }

    [[nodiscard]] bool SetInspectorPanel(InspectorPanel panel) {
        if (inspectorPanel_ == panel) {
            return true;
        }
        if (inspectorPanel_ == InspectorPanel::Files && panel != InspectorPanel::Files) {
            if (!ResolveDirtyResourceBeforeContextSwitch("switching inspector tabs")) {
                return false;
            }
            DestroyResourceTextEditorControl();
        }
        inspectorPanel_ = panel;
        return true;
    }

    [[nodiscard]] int LeftPanelContentTop(const RECT& hierarchyInner) const {
        int top = hierarchyInner.top + 6 + kLeftPanelTabHeight_;
        if (leftPanelMode_ == LeftPanelMode::Resources) {
            top += kLeftPanelGameStripHeight_ + 4;
            top += kResourceFilterStripHeight_ + 6;
            top += 24 + 6;
        } else {
            top += 24 + 6;
        }
        return top;
    }

    [[nodiscard]] RECT SceneSearchBoxRect(const RECT& hierarchyInner) const {
        const int tabStripBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight_;
        return RECT{
            hierarchyInner.left + 6,
            tabStripBottom + 4,
            hierarchyInner.right - 40,
            tabStripBottom + 26
        };
    }

    [[nodiscard]] RECT SceneSearchClearRect(const RECT& hierarchyInner) const {
        const RECT searchRect = SceneSearchBoxRect(hierarchyInner);
        return RECT{searchRect.right + 4, searchRect.top, searchRect.right + 30, searchRect.bottom};
    }

    [[nodiscard]] int LeftPanelSceneListBottom(const RECT& hierarchyInner) const {
        return hierarchyInner.bottom - kHierarchyBottomGutter_;
    }

    [[nodiscard]] RECT ResourceSearchBoxRect(const RECT& hierarchyInner) const {
        const int tabStripBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight_;
        const int filterTop = tabStripBottom + 4 + kLeftPanelGameStripHeight_ + 4;
        return RECT{
            hierarchyInner.left + 6,
            filterTop + kResourceFilterStripHeight_ + 4,
            hierarchyInner.right - 40,
            filterTop + kResourceFilterStripHeight_ + 26
        };
    }

    [[nodiscard]] RECT ResourceSearchClearRect(const RECT& hierarchyInner) const {
        const RECT searchRect = ResourceSearchBoxRect(hierarchyInner);
        return RECT{searchRect.right + 4, searchRect.top, searchRect.right + 30, searchRect.bottom};
    }

    [[nodiscard]] int CountVisibleSceneRows(const RECT& hierarchyInner) const {
        const int h =
            std::max(0, LeftPanelSceneListBottom(hierarchyInner) - LeftPanelContentTop(hierarchyInner) - 8);
        return std::max(1, h / kHierarchyRowHeight_);
    }

    [[nodiscard]] int CountVisibleResourceRows(const RECT& hierarchyInner) const {
        const int h =
            std::max(0, LeftPanelSceneListBottom(hierarchyInner) - LeftPanelContentTop(hierarchyInner) - 8);
        return std::max(1, h / kResourceListRowHeight_);
    }

    void RebuildFilteredHierarchyOrder() {
        filteredHierarchyOrder_ = BuildFilteredHierarchyDrawOrder(
            starterScene_.scene, editorTrashFolderHandle_, resourceSearchQuery_);
    }

    void RebuildFilteredResourceRows() {
        const ri::editor::FilteredResourceView filtered =
            ri::editor::BuildFilteredResourceRows(
                resourceCatalogEntries_, resourceCategoryMask_, resourceSearchQuery_, selectedResourceRow_);
        filteredResourceRows_ = filtered.rows;
        selectedResourceVisibleRow_ = filtered.selectedVisibleRow;
        if (selectedResourceVisibleRow_ < 0) {
            resourceCatalogScrollTopRow_ = filtered.resetScrollTop;
        }
    }

    void EnsureSelectedResourceVisible(const RECT& hierarchyInner) {
        const int listTop = LeftPanelContentTop(hierarchyInner);
        const int listBottom = LeftPanelSceneListBottom(hierarchyInner);
        const int innerHeight = std::max(0, listBottom - listTop - 8);
        const int visibleRows = std::max(1, innerHeight / kResourceListRowHeight_);
        resourceCatalogScrollTopRow_ = ri::editor::ComputeVisibleResourceScrollTop(
            resourceCatalogScrollTopRow_,
            selectedResourceVisibleRow_,
            static_cast<int>(filteredResourceRows_.size()),
            visibleRows);
    }

    void SelectResourceVisibleRow(int visibleRow, const RECT& hierarchyInner) {
        if (visibleRow < 0 || visibleRow >= static_cast<int>(filteredResourceRows_.size())) {
            return;
        }
        selectedResourceVisibleRow_ = visibleRow;
        SelectWorkspaceResourceRow(filteredResourceRows_[static_cast<std::size_t>(visibleRow)]);
        EnsureSelectedResourceVisible(hierarchyInner);
    }

    void UpdateCameraPlotRect(const RECT& viewportInner) {
        constexpr int kBannerHeight = 24;
        constexpr int kMetaStrip = 26;
        const RECT menuBanner{viewportInner.left + 4,
                              viewportInner.top + 6,
                              viewportInner.right - 4,
                              viewportInner.top + 6 + kBannerHeight};
        const RECT quadArea{viewportInner.left + 4,
                            menuBanner.bottom + 4,
                            viewportInner.right - 4,
                            viewportInner.bottom - 4 - kMetaStrip};

        if (full3DViewport_) {
            RECT plot{quadArea.left + 6, quadArea.top + 22, quadArea.right - 6, quadArea.bottom - 6};
            if (plot.right > plot.left + 8 && plot.bottom > plot.top + 8) {
                cameraPlotRect_ = plot;
            } else {
                cameraPlotRect_ = quadArea;
            }
            return;
        }

        if (quadArea.right <= quadArea.left + 32 || quadArea.bottom <= quadArea.top + 32) {
            cameraPlotRect_ = quadArea;
            return;
        }

        const int midX = (quadArea.left + quadArea.right) / 2;
        const int midY = (quadArea.top + quadArea.bottom) / 2;
        const RECT cellCamera{midX + 1, midY + 1, quadArea.right, quadArea.bottom};
        const RECT cameraInner{cellCamera.left + 2, cellCamera.top + 2, cellCamera.right - 2, cellCamera.bottom - 2};
        RECT plot{cameraInner.left + 4, cameraInner.top + 24, cameraInner.right - 4, cameraInner.bottom - 4};
        if (plot.right > plot.left + 4 && plot.bottom > plot.top + 4) {
            cameraPlotRect_ = plot;
        } else {
            cameraPlotRect_ = cameraInner;
        }
    }

    [[nodiscard]] std::vector<int> HierarchyDrawOrder() const {
        if (!filteredHierarchyOrder_.empty() || !resourceSearchQuery_.empty()) {
            return filteredHierarchyOrder_;
        }
        return BuildHierarchyDrawOrder(starterScene_.scene, editorTrashFolderHandle_);
    }

    void EnsureHierarchySelectionVisible(const RECT& hierarchyInner) {
        const std::vector<int> order = HierarchyDrawOrder();
        const int listTop = LeftPanelContentTop(hierarchyInner);
        const int listBottom = LeftPanelSceneListBottom(hierarchyInner);
        const int innerHeight = std::max(0, listBottom - listTop - 8);
        const int visibleRows = std::max(1, innerHeight / kHierarchyRowHeight_);
        hierarchyScrollTopRow_ =
            ComputeVisibleHierarchyScrollTop(hierarchyScrollTopRow_, order, selectedNode_, visibleRows);
    }

    [[nodiscard]] bool OnMouseWheel(short wheelDelta, int screenX, int screenY) {
        POINT point{screenX, screenY};
        ScreenToClient(hwnd_, &point);
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        if (PtInRect(&cameraPlotRect_, point) != FALSE) {
            if (!autoOrbitPreview_) {
                const float steps = static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA);
                const float factor = std::exp(-steps * 0.14f);
                editorOrbitState_.distance *= factor;
                ApplyEditorOrbitToScene();
                lastIoStatus_ = "Camera: zoom.";
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (PtInRect(&layout.hierarchyInner, point) == FALSE) {
            return false;
        }

        if (leftPanelMode_ == LeftPanelMode::Resources) {
            const int visibleRows = CountVisibleResourceRows(layout.hierarchyInner);
            resourceCatalogScrollTopRow_ = ri::editor::ComputeWheelScrollTop(
                resourceCatalogScrollTopRow_,
                static_cast<int>(filteredResourceRows_.size()),
                visibleRows,
                wheelDelta,
                3);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }

        const std::vector<int> order = HierarchyDrawOrder();
        const int visibleRows = CountVisibleSceneRows(layout.hierarchyInner);
        hierarchyScrollTopRow_ = ri::editor::ComputeWheelScrollTop(
            hierarchyScrollTopRow_,
            static_cast<int>(order.size()),
            visibleRows,
            wheelDelta,
            3);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    [[nodiscard]] bool HandleTopChromeClick(const POINT& point) {
        RECT client{};
        GetClientRect(hwnd_, &client);
        const RECT clientRect{0, 0, client.right, client.bottom};
        return ri::editor::DispatchEditorTopChromeClick(
            clientRect,
            point,
            {
                .onSave = [this]() {
                std::string saveError;
                if (SavePersistentEditorScene(ResolveSceneStatePath(),
                                              EditorOrbitSidecarPath(ResolveSceneStatePath()),
                                              &saveError)) {
                    lastIoStatus_ = "Saved scene transforms, authored nodes, and orbit camera.";
                    autosavePending_ = false;
                    lastAutosaveSteady_ = std::chrono::steady_clock::now();
                } else {
                    lastIoStatus_ = "Failed to save editor scene state: " + saveError + ".";
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onScaffold = [this]() {
                    TryScaffoldMountedGame();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onExportScene = [this]() {
                    TryExportAssemblyPrimitivesCsv();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onPlay = [this]() {
                    TryLaunchPlayer();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onFiles = [this]() {
                    leftPanelMode_ = LeftPanelMode::Resources;
                    (void)SetInspectorPanel(InspectorPanel::Files);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
            });
    }

    [[nodiscard]] bool HandleInspectorTabClick(const EditorLayout& layout, const POINT& point) {
        return ri::editor::DispatchEditorInspectorTabClick(
            layout.inspectorInner,
            point,
            {
                .onNode = [this]() {
                    if (!SetInspectorPanel(InspectorPanel::Node)) {
                        InvalidateRect(hwnd_, nullptr, FALSE);
                        return;
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onBrush = [this]() {
                    if (!SetInspectorPanel(InspectorPanel::Brush)) {
                        InvalidateRect(hwnd_, nullptr, FALSE);
                        return;
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onGameplay = [this]() {
                    if (!SetInspectorPanel(InspectorPanel::Gameplay)) {
                        InvalidateRect(hwnd_, nullptr, FALSE);
                        return;
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onFiles = [this]() {
                    (void)SetInspectorPanel(InspectorPanel::Files);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onUiWorkbench = [this]() {
                    if (!SetInspectorPanel(InspectorPanel::UiWorkbench)) {
                        InvalidateRect(hwnd_, nullptr, FALSE);
                        return;
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
            });
    }

    [[nodiscard]] bool HandleInspectorPanelClick(const EditorLayout& layout, const POINT& point) {
        const ProjectShortcutLayout shortcuts = ComputeProjectShortcutLayout(layout.inspectorInner);
        const std::string primaryLevelShortcutPath =
            sceneConfig_.gameManifest.has_value() && !sceneConfig_.gameManifest->primaryLevel.empty()
                ? sceneConfig_.gameManifest->primaryLevel
                : "levels/assembly.primitives.csv";
        return ri::editor::DispatchEditorInspectorPanelClick(
            point,
            {
                .inspectorInner = layout.inspectorInner,
                .gameplayLayout = gameplayPanelLayout_,
                .uiWorkbenchLayout = uiWorkbenchLayout_,
                .projectShortcuts = shortcuts,
                .primaryLevelShortcutPath = primaryLevelShortcutPath,
                .brushPanelActive = inspectorPanel_ == InspectorPanel::Brush,
                .filesPanelActive = inspectorPanel_ == InspectorPanel::Files,
                .gameplayPanelActive = inspectorPanel_ == InspectorPanel::Gameplay,
                .uiWorkbenchActive = inspectorPanel_ == InspectorPanel::UiWorkbench,
                .tryHandleNudge = [this](const POINT& clickPoint) {
                    return TryHandleInspectorNudgeClick(clickPoint);
                },
                .onCycleBrushPreset = [this](int delta) {
                    CycleStructuralPrimitivePreset(delta);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onSaveResource = [this]() {
                    lastIoStatus_ = SaveActiveResourceFileFromEditor() ? "Saved resource file." : "Save failed.";
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onOpenExplorer = [this]() {
                    OpenActiveResourceInExplorer();
                    lastIoStatus_ = "Opened Explorer beside resource.";
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onOpenProjectShortcut = [this](const std::string& relativePath) {
                    OpenProjectResourceShortcut(relativePath);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onCycleInventoryMode = [this]() {
                    CycleInventoryPresentation();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onToggleOffHand = [this]() {
                    creatorInventoryPolicy_.allowOffHand = !creatorInventoryPolicy_.allowOffHand;
                    lastIoStatus_ = creatorInventoryPolicy_.allowOffHand
                        ? "Gameplay policy: off-hand enabled."
                        : "Gameplay policy: off-hand disabled.";
                    SaveCreatorPolicyToDisk();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onAddTrigger = [this]() {
                    AddTriggerVolumePrimitive();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onExportGameplay = [this]() {
                    TryExportAssemblyPrimitivesCsv();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onGameplayPlaytest = [this]() {
                    TryLaunchPlayer();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onCycleUiWorkbenchScreen = [this](int delta) {
                    CycleUiWorkbenchScreen(delta);
                },
                .onUseAutoUiWorkbenchSource = [this]() {
                    SetUiWorkbenchSource(UiWorkbenchSource::AutoSelection);
                },
                .onUseMenuSampleUiWorkbenchSource = [this]() {
                    SetUiWorkbenchSource(UiWorkbenchSource::MenuSample);
                },
                .onUseVnSampleUiWorkbenchSource = [this]() {
                    SetUiWorkbenchSource(UiWorkbenchSource::VnSample);
                },
                .onUiWorkbenchNewScreen = [this]() {
                    UiWorkbenchCreateScreen();
                },
                .onUiWorkbenchDuplicateScreen = [this]() {
                    UiWorkbenchDuplicateScreen();
                },
                .onUiWorkbenchAddChoiceBlock = [this]() {
                    UiWorkbenchAddChoicesBlock();
                },
                .onUiWorkbenchSetStartScreen = [this]() {
                    UiWorkbenchSetStartScreen();
                },
                .onUiWorkbenchAddDialogueBlock = [this]() {
                    UiWorkbenchAddDialogueBlock();
                },
                .onUiWorkbenchAddNarrationBlock = [this]() {
                    UiWorkbenchAddNarrationBlock();
                },
                .onUiWorkbenchMoveBlockUp = [this]() {
                    UiWorkbenchMoveBlock(-1);
                },
                .onUiWorkbenchMoveBlockDown = [this]() {
                    UiWorkbenchMoveBlock(1);
                },
                .onUiWorkbenchDeleteBlock = [this]() {
                    UiWorkbenchDeleteSelectedBlock();
                },
            });
    }

    [[nodiscard]] bool HandleLeftPanelNavigationKey(WPARAM key,
                                                    const EditorLayout& layoutForNav,
                                                    const std::vector<int>& drawOrder,
                                                    const int visibleSceneRows,
                                                    const int visibleResRows) {
        const int maxSceneScroll = std::max(0, static_cast<int>(drawOrder.size()) - visibleSceneRows);
        const int maxResourceScroll =
            std::max(0, static_cast<int>(filteredResourceRows_.size()) - visibleResRows);

        if (leftPanelMode_ == LeftPanelMode::Resources) {
            if (key == VK_PRIOR) {
                resourceCatalogScrollTopRow_ = std::max(0, resourceCatalogScrollTopRow_ - visibleResRows);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            }
            if (key == VK_NEXT) {
                resourceCatalogScrollTopRow_ =
                    std::min(maxResourceScroll, resourceCatalogScrollTopRow_ + visibleResRows);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            }
            if (!filteredResourceRows_.empty()) {
                if (key == VK_UP) {
                    const int next = selectedResourceVisibleRow_ <= 0
                        ? static_cast<int>(filteredResourceRows_.size()) - 1
                        : selectedResourceVisibleRow_ - 1;
                    SelectResourceVisibleRow(next, layoutForNav.hierarchyInner);
                    return true;
                }
                if (key == VK_DOWN) {
                    const int next = selectedResourceVisibleRow_ + 1 >= static_cast<int>(filteredResourceRows_.size())
                        ? 0
                        : selectedResourceVisibleRow_ + 1;
                    SelectResourceVisibleRow(next, layoutForNav.hierarchyInner);
                    return true;
                }
                if (key == VK_HOME) {
                    SelectResourceVisibleRow(0, layoutForNav.hierarchyInner);
                    return true;
                }
                if (key == VK_END) {
                    SelectResourceVisibleRow(static_cast<int>(filteredResourceRows_.size()) - 1,
                                             layoutForNav.hierarchyInner);
                    return true;
                }
            }
            return false;
        }

        if (key == VK_PRIOR) {
            hierarchyScrollTopRow_ = std::max(0, hierarchyScrollTopRow_ - visibleSceneRows);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_NEXT) {
            hierarchyScrollTopRow_ = std::min(maxSceneScroll, hierarchyScrollTopRow_ + visibleSceneRows);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_UP && !drawOrder.empty()) {
            const std::optional<int> currentPos = FindDrawOrderIndex(drawOrder, selectedNode_);
            const int nextPos =
                currentPos.has_value() && *currentPos > 0 ? *currentPos - 1 : static_cast<int>(drawOrder.size()) - 1;
            selectedNode_ = static_cast<std::size_t>(drawOrder[static_cast<std::size_t>(nextPos)]);
            EnsureHierarchySelectionVisible(layoutForNav.hierarchyInner);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_DOWN && !drawOrder.empty()) {
            const std::optional<int> currentPos = FindDrawOrderIndex(drawOrder, selectedNode_);
            const int nextPos = currentPos.has_value() && *currentPos + 1 < static_cast<int>(drawOrder.size())
                ? *currentPos + 1
                : 0;
            selectedNode_ = static_cast<std::size_t>(drawOrder[static_cast<std::size_t>(nextPos)]);
            EnsureHierarchySelectionVisible(layoutForNav.hierarchyInner);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_HOME && !drawOrder.empty()) {
            selectedNode_ = static_cast<std::size_t>(drawOrder.front());
            EnsureHierarchySelectionVisible(layoutForNav.hierarchyInner);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_END && !drawOrder.empty()) {
            selectedNode_ = static_cast<std::size_t>(drawOrder.back());
            EnsureHierarchySelectionVisible(layoutForNav.hierarchyInner);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        return false;
    }

    [[nodiscard]] bool HandleGlobalEditorHotkeys(WPARAM key, const bool controlHeld, const bool shiftHeld) {
        if (key == VK_TAB) {
            full3DViewport_ = !full3DViewport_;
            lastIoStatus_ = full3DViewport_ ? "Layout: full 3D (Tab for quad views)." : "Layout: quad views (Tab for full 3D).";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == 'G' && !controlHeld) {
            gridSnapEnabled_ = !gridSnapEnabled_;
            lastIoStatus_ = std::string("Grid snap ") + (gridSnapEnabled_ ? "ON" : "OFF") + " (" + GridSnapLabel() + ").";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == 'G' && controlHeld) {
            if (SnapSelectedNodeToGridNow()) {
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return true;
        }
        if (key == VK_OEM_MINUS) {
            CycleGridSnapStep(-1);
            lastIoStatus_ = "Grid step: " + GridSnapLabel() + ".";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_OEM_PLUS || key == VK_ADD) {
            CycleGridSnapStep(1);
            lastIoStatus_ = "Grid step: " + GridSnapLabel() + ".";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_SPACE) {
            autoOrbitPreview_ = !autoOrbitPreview_;
            lastIoStatus_ = autoOrbitPreview_ ? "Camera: auto-orbit preview ON." : "Camera: auto-orbit preview OFF.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (controlHeld && shiftHeld && key == 'Q') {
            DestroyWindow(hwnd_);
            return true;
        }
        if (key == VK_ESCAPE) {
            if (static_cast<int>(selectedNode_) != starterScene_.handles.root &&
                starterScene_.handles.root != ri::scene::kInvalidHandle) {
                selectedNode_ = static_cast<std::size_t>(starterScene_.handles.root);
                lastIoStatus_ = "Selection cleared (World root). Ctrl+Shift+Q exits the editor.";
            } else {
                lastIoStatus_ = "Tip: Ctrl+Shift+Q to exit · Del removes authored mesh nodes.";
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_F6) {
            statsOverlayVisible_ = !statsOverlayVisible_;
            statsOverlayState_.SetVisible(statsOverlayVisible_);
            lastIoStatus_ = statsOverlayVisible_ ? "Diagnostics overlay visible." : "Diagnostics overlay hidden.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        return false;
    }

    LRESULT OnKeyDown(WPARAM key) {
        const bool controlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool altHeld = (GetKeyState(VK_MENU) & 0x8000) != 0;
        const EditorLayout layoutForNav = ComputeLayout();
        const std::vector<int> drawOrder = HierarchyDrawOrder();
        const int visibleSceneRows = CountVisibleSceneRows(layoutForNav.hierarchyInner);
        const int visibleResRows = CountVisibleResourceRows(layoutForNav.hierarchyInner);

        if (inspectorPanel_ == InspectorPanel::UiWorkbench && key == VK_F2) {
            if (shiftHeld) {
                TryBeginUiWorkbenchSecondaryEdit();
            } else {
                TryBeginUiWorkbenchPrimaryEdit();
            }
            return 0;
        }
        if (controlHeld && (key == 'F' || key == 'f')) {
            resourceSearchActive_ = true;
            lastIoStatus_ = leftPanelMode_ == LeftPanelMode::Scene
                ? "Scene search active: type to filter hierarchy (name or index)."
                : "Resource search active: type to filter files.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (HandleLeftPanelNavigationKey(key, layoutForNav, drawOrder, visibleSceneRows, visibleResRows)) {
            return 0;
        }
        if (HandleGlobalEditorHotkeys(key, controlHeld, shiftHeld)) {
            return 0;
        }
        if (ri::editor::DispatchEditorCommandHotkey(
                {
                    .key = key,
                    .controlHeld = controlHeld,
                    .shiftHeld = shiftHeld,
                      .filesInspectorActive = inspectorPanel_ == InspectorPanel::Files,
                      .gameplayInspectorActive = inspectorPanel_ == InspectorPanel::Gameplay,
                      .resourceFileDirty = resourceFileDirty_,
                      .onUndo = [this]() {
                          if (inspectorPanel_ == InspectorPanel::UiWorkbench) {
                              UiWorkbenchUndo();
                              return;
                          }
                          lastIoStatus_ = UndoLastEdit() ? "Undo applied." : "Nothing to undo.";
                          InvalidateRect(hwnd_, nullptr, FALSE);
                      },
                      .onRedo = [this]() {
                          if (inspectorPanel_ == InspectorPanel::UiWorkbench) {
                              UiWorkbenchRedo();
                              return;
                          }
                          lastIoStatus_ = RedoLastEdit() ? "Redo applied." : "Nothing to redo.";
                          InvalidateRect(hwnd_, nullptr, FALSE);
                      },
                    .onSaveTimestampedSnapshot = [this]() {
                        TrySaveTimestampedSceneSnapshot();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onScaffoldMountedGame = [this]() {
                        TryScaffoldMountedGame();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSaveActiveResource = [this]() {
                        lastIoStatus_ =
                            SaveActiveResourceFileFromEditor() ? "Saved active resource file." : "Failed to save resource file.";
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSaveEditorScene = [this]() {
                        std::string saveError;
                        if (SavePersistentEditorScene(ResolveSceneStatePath(),
                                                      EditorOrbitSidecarPath(ResolveSceneStatePath()),
                                                      &saveError)) {
                            lastIoStatus_ = "Saved scene transforms, authored nodes, and orbit camera.";
                            autosavePending_ = false;
                            lastAutosaveSteady_ = std::chrono::steady_clock::now();
                        } else {
                            lastIoStatus_ = "Failed to save editor scene state: " + saveError + ".";
                        }
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onExportAssemblyCsv = [this]() {
                        TryExportAssemblyPrimitivesCsv();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onResetSelectedTransform = [this]() {
                        TryResetSelectedTransform();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAddCube = [this]() {
                        AddAuthoringPrimitive(ri::scene::PrimitiveType::Cube);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAddPlane = [this]() {
                        AddAuthoringPrimitive(ri::scene::PrimitiveType::Plane);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAddTrigger = [this]() {
                        AddTriggerVolumePrimitive();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSpawnStructuralBrush = [this]() {
                        SpawnStructuralBrushAtFocus();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSelectStructuralPresetDigit = [this](int digit) {
                        SelectStructuralPrimitivePresetByDigit(digit);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onDuplicateSelectedNode = [this]() {
                        TryDuplicateSelectedNode();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onBeginNodeRename = [this]() {
                        TryBeginNodeRename();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onCreateGroupNode = [this]() {
                        TryCreateGroupNode();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onUngroupSelectedNode = [this]() {
                        TryUngroupSelectedNode();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onGroupSelectedNode = [this]() {
                        TryGroupSelectedNode();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onReparentSelectedToWorldRoot = [this]() {
                        TryReparentSelectedToWorldRoot();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onCycleStructuralPreset = [this](int delta) {
                        CycleStructuralPrimitivePreset(delta);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onDeleteSelectedNode = [this]() {
                        TryDeleteSelectedNode();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onImportPrimaryLevelCsv = [this]() {
                        TryImportPrimaryLevelCsv();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onReloadFocusedGameScene = [this]() {
                        ReloadEditorSceneForFocusedGame(false);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onLoadAutosaveState = [this]() {
                        TryLoadAutosaveState();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onLoadPersistentEditorScene = [this]() {
                        (void)TryLoadPersistentEditorScene(ResolveSceneStatePath(), false);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onFrameAllRenderables = [this]() {
                        TryFrameAllRenderables();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onFrameSelection = [this]() {
                        if (!autoOrbitPreview_ && selectedNode_ < starterScene_.scene.NodeCount()) {
                            const std::vector<int> handles = {static_cast<int>(selectedNode_)};
                            if (ri::scene::FrameNodesWithOrbitCamera(starterScene_.scene,
                                                                     starterScene_.handles.orbitCamera,
                                                                     handles,
                                                                     1.35f)) {
                                editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
                                ApplyEditorOrbitToScene();
                                lastIoStatus_ = "Framed selection in orbit camera (Ctrl+S saves orbit).";
                            } else {
                                lastIoStatus_ = "Could not frame selection.";
                            }
                        }
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSetEditModeTranslate = [this]() {
                        editMode_ = EditMode::Translate;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSetEditModeRotate = [this]() {
                        editMode_ = EditMode::Rotate;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSetEditModeScale = [this]() {
                        editMode_ = EditMode::Scale;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSetAxisX = [this]() {
                        activeAxis_ = 0;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSetAxisY = [this]() {
                        activeAxis_ = 1;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSetAxisZ = [this]() {
                        activeAxis_ = 2;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSelectInspectorNode = [this]() {
                        if (!SetInspectorPanel(InspectorPanel::Node)) {
                            InvalidateRect(hwnd_, nullptr, FALSE);
                            return;
                        }
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSelectInspectorBrush = [this]() {
                        if (!SetInspectorPanel(InspectorPanel::Brush)) {
                            InvalidateRect(hwnd_, nullptr, FALSE);
                            return;
                        }
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSelectInspectorGameplay = [this]() {
                        if (!SetInspectorPanel(InspectorPanel::Gameplay)) {
                            InvalidateRect(hwnd_, nullptr, FALSE);
                            return;
                        }
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSelectInspectorFiles = [this]() {
                        (void)SetInspectorPanel(InspectorPanel::Files);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSelectInspectorUiWorkbench = [this]() {
                        if (!SetInspectorPanel(InspectorPanel::UiWorkbench)) {
                            InvalidateRect(hwnd_, nullptr, FALSE);
                            return;
                        }
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSelectAdjacentAuthoredNode = [this](int direction) {
                        TrySelectAdjacentAuthoredNode(direction);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onCycleGameplayInventoryMode = [this]() {
                        CycleInventoryPresentation();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onToggleGameplayOffHand = [this]() {
                        creatorInventoryPolicy_.allowOffHand = !creatorInventoryPolicy_.allowOffHand;
                        lastIoStatus_ = creatorInventoryPolicy_.allowOffHand
                            ? "Gameplay policy: off-hand enabled."
                            : "Gameplay policy: off-hand disabled.";
                        SaveCreatorPolicyToDisk();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                })) {
            return 0;
        }
        if (!controlHeld && editMode_ == EditMode::Translate &&
            (key == 'W' || key == 'A' || key == 'S' || key == 'D' || key == 'Q' || key == 'E')) {
            if (IsProtectedEditorNode(static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Edit blocked: select an authored node, not World/rigs/helpers.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const float step = ApplyStepModifiers(0.1f, shiftHeld, altHeld);
            ri::math::Vec3 delta{0.0f, 0.0f, 0.0f};
            switch (key) {
                case 'W': delta.z += step; break;
                case 'S': delta.z -= step; break;
                case 'A': delta.x -= step; break;
                case 'D': delta.x += step; break;
                case 'E': delta.y += step; break;
                case 'Q': delta.y -= step; break;
            }
            ApplySelectedNodeTranslationDelta(delta);
            lastIoStatus_ = shiftHeld ? "Translate: WASDQE (fine)." :
                (altHeld ? "Translate: WASDQE (coarse)." : "Translate: WASDQE.");
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (!controlHeld && editMode_ == EditMode::Rotate &&
            (key == 'W' || key == 'A' || key == 'S' || key == 'D' || key == 'Q' || key == 'E')) {
            if (IsProtectedEditorNode(static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Edit blocked: select an authored node, not World/rigs/helpers.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const float step = ApplyStepModifiers(2.5f, shiftHeld, altHeld);
            ri::math::Vec3 delta{0.0f, 0.0f, 0.0f};
            switch (key) {
                case 'W': delta.x += step; break;
                case 'S': delta.x -= step; break;
                case 'A': delta.y -= step; break;
                case 'D': delta.y += step; break;
                case 'Q': delta.z -= step; break;
                case 'E': delta.z += step; break;
            }
            ApplySelectedNodeRotationDelta(delta);
            lastIoStatus_ = shiftHeld ? "Rotate: WASDQE (fine)." :
                (altHeld ? "Rotate: WASDQE (coarse)." : "Rotate: WASDQE.");
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (!controlHeld && editMode_ == EditMode::Scale &&
            (key == 'W' || key == 'A' || key == 'S' || key == 'D' || key == 'Q' || key == 'E')) {
            if (IsProtectedEditorNode(static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Edit blocked: select an authored node, not World/rigs/helpers.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const float step = ApplyStepModifiers(0.08f, shiftHeld, altHeld);
            ri::math::Vec3 delta{0.0f, 0.0f, 0.0f};
            switch (key) {
                case 'W': delta.x += step; break;
                case 'S': delta.x -= step; break;
                case 'A': delta.y -= step; break;
                case 'D': delta.y += step; break;
                case 'Q': delta.z -= step; break;
                case 'E': delta.z += step; break;
            }
            if (selectedNode_ < starterScene_.scene.NodeCount()) {
                ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
                const ri::scene::Transform before = node.localTransform;
                node.localTransform.scale.x = std::max(0.01f, node.localTransform.scale.x + delta.x);
                node.localTransform.scale.y = std::max(0.01f, node.localTransform.scale.y + delta.y);
                node.localTransform.scale.z = std::max(0.01f, node.localTransform.scale.z + delta.z);
                const ri::scene::Transform after = node.localTransform;
                PushEditAction(TransformEditAction{selectedNode_, before, after});
            }
            lastIoStatus_ = shiftHeld ? "Scale: WASDQE (fine)." :
                (altHeld ? "Scale: WASDQE (coarse)." : "Scale: WASDQE.");
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 'W' || key == 'S') {
            if (IsProtectedEditorNode(static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Edit blocked: select an authored node, not World/rigs/helpers.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            const float step = ApplyStepModifiers(0.08f, shiftHeld, altHeld);
            const float direction = key == 'W' ? 1.0f : -1.0f;
            ApplySelectedNodeEdit(direction * step);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    LRESULT OnChar(WPARAM key) {
        if (uiWorkbenchTextEditActive_) {
            if (key == 27) {
                CancelUiWorkbenchTextEdit();
                return 0;
            }
            if (key == 13) {
                CommitUiWorkbenchTextEdit();
                return 0;
            }
            if (key == 8) {
                if (!uiWorkbenchTextEditDraft_.empty()) {
                    uiWorkbenchTextEditDraft_.pop_back();
                    lastIoStatus_ = "UI/VN edit: "
                        + (uiWorkbenchTextEditDraft_.empty() ? std::string("<empty>") : uiWorkbenchTextEditDraft_);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;
            }
            if (key >= 32 && key <= 126) {
                uiWorkbenchTextEditDraft_.push_back(static_cast<char>(key));
                lastIoStatus_ = "UI/VN edit: " + uiWorkbenchTextEditDraft_ + "  (Enter=apply Esc=cancel)";
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }
        if (nodeRenameTypingActive_) {
            if (key == 27) {
                nodeRenameTypingActive_ = false;
                nodeRenameDraft_.clear();
                lastIoStatus_ = "Rename cancelled.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (key == 13) {
                TryCommitNodeRename();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            if (key == 8) {
                if (!nodeRenameDraft_.empty()) {
                    nodeRenameDraft_.pop_back();
                    lastIoStatus_ = "Rename: " + (nodeRenameDraft_.empty() ? std::string("<empty>") : nodeRenameDraft_);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return 0;
            }
            if (key >= 32 && key <= 126) {
                nodeRenameDraft_.push_back(static_cast<char>(key));
                lastIoStatus_ = "Rename: " + nodeRenameDraft_ + "  (Enter=apply Esc=cancel)";
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }
        if (!resourceSearchActive_) {
            return 0;
        }
        if (key == 27) {
            resourceSearchActive_ = false;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key == 8) {
            if (!resourceSearchQuery_.empty()) {
                resourceSearchQuery_.pop_back();
                if (leftPanelMode_ == LeftPanelMode::Scene) {
                    RebuildFilteredHierarchyOrder();
                    lastIoStatus_ = "Scene filter: " + (resourceSearchQuery_.empty() ? std::string("<none>") : resourceSearchQuery_);
                } else {
                    RebuildFilteredResourceRows();
                    lastIoStatus_ = "Resource filter: " + (resourceSearchQuery_.empty() ? std::string("<none>") : resourceSearchQuery_);
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }
        if (key == 13) {
            resourceSearchActive_ = false;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (key < 32 || key > 126) {
            return 0;
        }
        resourceSearchQuery_.push_back(static_cast<char>(key));
        if (leftPanelMode_ == LeftPanelMode::Scene) {
            RebuildFilteredHierarchyOrder();
            lastIoStatus_ = "Scene filter: " + resourceSearchQuery_;
        } else {
            RebuildFilteredResourceRows();
            lastIoStatus_ = "Resource filter: " + resourceSearchQuery_;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    LRESULT OnLeftButtonDown(int x, int y) {
        const POINT pick{x, y};
#if defined(_WIN32)
        const HWND hitChild =
            ChildWindowFromPointEx(hwnd_, pick, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED);
        if (hitChild != nullptr && hitChild != hwnd_) {
            SetFocus(hitChild);
        } else {
            SetFocus(hwnd_);
        }
#else
        SetFocus(hwnd_);
#endif
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        POINT point{x, y};

        const auto hitRect = [&point](const RECT& rect) {
            return PtInRect(&rect, point) != FALSE;
        };

        if (resourceSearchActive_
            && (leftPanelMode_ != LeftPanelMode::Resources || PtInRect(&layout.hierarchyInner, point) == FALSE)) {
            resourceSearchActive_ = false;
        }

        if (HandleTopChromeClick(point)) {
            return 0;
        }

        if (ri::editor::DispatchEditorToolbarClick(
                layout.toolStrip,
                point,
                {
                    .onTranslate = [this]() {
                        editMode_ = EditMode::Translate;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onRotate = [this]() {
                        editMode_ = EditMode::Rotate;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onScale = [this]() {
                        editMode_ = EditMode::Scale;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAxisX = [this]() {
                        activeAxis_ = 0;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAxisY = [this]() {
                        activeAxis_ = 1;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAxisZ = [this]() {
                        activeAxis_ = 2;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSnapToggle = [this]() {
                        gridSnapEnabled_ = !gridSnapEnabled_;
                        lastIoStatus_ = std::string("Grid snap ") + (gridSnapEnabled_ ? "ON" : "OFF") + " (" + GridSnapLabel() + ").";
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSnapStepDown = [this]() {
                        CycleGridSnapStep(-1);
                        lastIoStatus_ = "Grid step: " + GridSnapLabel() + ".";
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSnapStepUp = [this]() {
                        CycleGridSnapStep(1);
                        lastIoStatus_ = "Grid step: " + GridSnapLabel() + ".";
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAddCube = [this]() {
                        AddAuthoringPrimitive(ri::scene::PrimitiveType::Cube);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAddPlane = [this]() {
                        AddAuthoringPrimitive(ri::scene::PrimitiveType::Plane);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onAddTrigger = [this]() {
                        AddTriggerVolumePrimitive();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onDuplicate = [this]() {
                        TryDuplicateSelectedNode();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onExportCsv = [this]() {
                        TryExportAssemblyPrimitivesCsv();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onPlay = [this]() {
                        TryLaunchPlayer();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                })) {
            return 0;
        }

        const ri::editor::EditorLeftPanelMode inputLeftPanelMode =
            leftPanelMode_ == LeftPanelMode::Resources
                ? ri::editor::EditorLeftPanelMode::Resources
                : ri::editor::EditorLeftPanelMode::Scene;
        if (ri::editor::DispatchEditorLeftPanelClick(
                {
                    .hierarchyInner = layout.hierarchyInner,
                    .mode = inputLeftPanelMode,
                    .hasWorkspaceGames = !workspaceGames_.empty(),
                    .point = point,
                    .hierarchyScrollTopRow = hierarchyScrollTopRow_,
                    .resourceCatalogScrollTopRow = resourceCatalogScrollTopRow_,
                    .workspaceGameCount = static_cast<int>(workspaceGames_.size()),
                    .focusedWorkspaceGameIndex = focusedWorkspaceGameIndex_,
                    .resourceCategoryMask = resourceCategoryMask_,
                    .filteredResourceRowCount = static_cast<int>(filteredResourceRows_.size()),
                    .hierarchyRowCount = static_cast<int>(HierarchyDrawOrder().size()),
                    .tryResolveDirtyResourceBeforeContextSwitch = [this](const char* reason) {
                        return ResolveDirtyResourceBeforeContextSwitch(reason);
                    },
                    .onFocusedWorkspaceGameIndexChanged = [this](int nextIndex) {
                        focusedWorkspaceGameIndex_ = nextIndex;
                        SwitchFocusedWorkspaceGame();
                    },
                    .onSceneTab = [this]() {
                        leftPanelMode_ = LeftPanelMode::Scene;
                        RebuildFilteredHierarchyOrder();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onResourcesTab = [this]() {
                        leftPanelMode_ = LeftPanelMode::Resources;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSearchActivate = [this]() {
                        resourceSearchActive_ = true;
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onSceneSearchClear = [this]() {
                        resourceSearchQuery_.clear();
                        resourceSearchActive_ = true;
                        RebuildFilteredHierarchyOrder();
                        lastIoStatus_ = "Scene filter cleared.";
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onResourceSearchClear = [this]() {
                        resourceSearchQuery_.clear();
                        resourceSearchActive_ = true;
                        RebuildFilteredResourceRows();
                        lastIoStatus_ = "Resource filter cleared.";
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onResourceCategoryMaskChanged = [this](std::uint32_t nextMask) {
                        resourceCategoryMask_ = nextMask;
                    },
                    .onSelectResourceRow = [this, &layout](int index) {
                        SelectResourceVisibleRow(index, layout.hierarchyInner);
                    },
                    .onSelectSceneRow = [this, &layout](int index) {
                        const std::vector<int>& order = HierarchyDrawOrder();
                        selectedNode_ = static_cast<std::size_t>(order[static_cast<std::size_t>(index)]);
                        EnsureHierarchySelectionVisible(layout.hierarchyInner);
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onStatus = [this](const std::string& status) {
                        lastIoStatus_ = status;
                    },
                    .onRebuildFilteredHierarchyOrder = [this]() {
                        RebuildFilteredHierarchyOrder();
                    },
                    .onRebuildFilteredResourceRows = [this]() {
                        RebuildFilteredResourceRows();
                    },
                    .onEnsureSelectedResourceVisible = [this, &layout]() {
                        EnsureSelectedResourceVisible(layout.hierarchyInner);
                    },
                    .onInvalidate = [this]() {
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                })) {
            return 0;
        }

        if (HandleInspectorTabClick(layout, point)) {
            return 0;
        }

        if (TryHandleUiWorkbenchInspectorSelectionClick(layout.inspectorInner, point)) {
            return 0;
        }

        if (HandleInspectorPanelClick(layout, point)) {
            return 0;
        }

        if (TryHandleUiWorkbenchViewportClick(layout.viewportInner, point)) {
            return 0;
        }

        if (!full3DViewport_ && TryRawIronOrthoSelectAt(x, y, layout)) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        const int splitterGrab = 6;
        if (std::abs(x - layout.hierarchySplitter.right) <= splitterGrab
            && y >= layout.hierarchy.top && y <= layout.hierarchy.bottom) {
            draggingHierarchySplitter_ = true;
            SetCapture(hwnd_);
            return 0;
        }
        if (std::abs(x - layout.inspectorSplitter.left) <= splitterGrab
            && y >= layout.inspector.top && y <= layout.inspector.bottom) {
            draggingInspectorSplitter_ = true;
            SetCapture(hwnd_);
            return 0;
        }

        if (hitRect(cameraPlotRect_) && !autoOrbitPreview_) {
            cameraDragActive_ = true;
            lastDragX_ = x;
            lastDragY_ = y;
            SetCapture(hwnd_);
            lastIoStatus_ = "Camera: drag to orbit (release to stop).";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        return 0;
    }

    LRESULT OnLeftButtonUp(int x, int y) {
        (void)x;
        (void)y;
        if (draggingHierarchySplitter_ || draggingInspectorSplitter_) {
            draggingHierarchySplitter_ = false;
            draggingInspectorSplitter_ = false;
            if (GetCapture() == hwnd_) {
                ReleaseCapture();
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (cameraDragActive_) {
            cameraDragActive_ = false;
            if (GetCapture() == hwnd_) {
                ReleaseCapture();
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    LRESULT OnMouseMove(int x, int y, WPARAM flags) {
        if (draggingHierarchySplitter_ && (flags & MK_LBUTTON) != 0) {
            RECT client{};
            GetClientRect(hwnd_, &client);
            const int minLeft = 240;
            const int maxLeft = std::max(minLeft, static_cast<int>(client.right) - 620);
            hierarchyPanelWidth_ = std::clamp(x - 10, minLeft, maxLeft);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (draggingInspectorSplitter_ && (flags & MK_LBUTTON) != 0) {
            RECT client{};
            GetClientRect(hwnd_, &client);
            const int minRight = 290;
            const int maxRight = std::max(minRight, static_cast<int>(client.right) - 620);
            inspectorPanelWidth_ = std::clamp(static_cast<int>(client.right) - x - 10, minRight, maxRight);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        if (cameraDragActive_ && (flags & MK_LBUTTON) != 0 && !autoOrbitPreview_) {
            const int dx = x - lastDragX_;
            const int dy = y - lastDragY_;
            lastDragX_ = x;
            lastDragY_ = y;
            editorOrbitState_.yawDegrees += static_cast<float>(dx) * 0.38f;
            editorOrbitState_.pitchDegrees -= static_cast<float>(dy) * 0.38f;
            ApplyEditorOrbitToScene();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    void OnCaptureLost() {
        cameraDragActive_ = false;
        draggingHierarchySplitter_ = false;
        draggingInspectorSplitter_ = false;
    }

    [[nodiscard]] bool TryRawIronOrthoSelectAt(int x, int y, const EditorLayout& layout) {
        constexpr int kBannerHeight = 24;
        constexpr int kMetaStrip = 26;
        const RECT menuBanner{layout.viewportInner.left + 4,
                              layout.viewportInner.top + 6,
                              layout.viewportInner.right - 4,
                              layout.viewportInner.top + 6 + kBannerHeight};
        const RECT quadArea{layout.viewportInner.left + 4,
                            menuBanner.bottom + 4,
                            layout.viewportInner.right - 4,
                            layout.viewportInner.bottom - 4 - kMetaStrip};
        if (quadArea.right <= quadArea.left + 32 || quadArea.bottom <= quadArea.top + 32) {
            return false;
        }

        const POINT point{x, y};
        const int midX = (quadArea.left + quadArea.right) / 2;
        const int midY = (quadArea.top + quadArea.bottom) / 2;
        const RECT cellTop{quadArea.left, quadArea.top, midX - 1, midY - 1};
        const RECT cellSide{midX + 1, quadArea.top, quadArea.right, midY - 1};
        const RECT cellFront{quadArea.left, midY + 1, midX - 1, quadArea.bottom};
        const auto plotRect = [](const RECT& cell) {
            const RECT inner{cell.left + 2, cell.top + 2, cell.right - 2, cell.bottom - 2};
            return RECT{inner.left + 4, inner.top + 24, inner.right - 4, inner.bottom - 4};
        };

        const RECT plotTop = plotRect(cellTop);
        const RECT plotSide = plotRect(cellSide);
        const RECT plotFront = plotRect(cellFront);

        if (PtInRect(&plotTop, point) != FALSE) {
            if (const auto handle =
                    PickRenderableInOrthoView(plotTop, RawIronFlatProjection::TopXz, x, y, starterScene_.scene)) {
                selectedNode_ = static_cast<std::size_t>(*handle);
                lastIoStatus_ = "TOP: selected renderable.";
                EnsureHierarchySelectionVisible(layout.hierarchyInner);
                return true;
            }
            return false;
        }
        if (PtInRect(&plotSide, point) != FALSE) {
            if (const auto handle =
                    PickRenderableInOrthoView(plotSide, RawIronFlatProjection::SideZy, x, y, starterScene_.scene)) {
                selectedNode_ = static_cast<std::size_t>(*handle);
                lastIoStatus_ = "SIDE: selected renderable.";
                EnsureHierarchySelectionVisible(layout.hierarchyInner);
                return true;
            }
            return false;
        }
        if (PtInRect(&plotFront, point) != FALSE) {
            if (const auto handle =
                    PickRenderableInOrthoView(plotFront, RawIronFlatProjection::FrontXy, x, y, starterScene_.scene)) {
                selectedNode_ = static_cast<std::size_t>(*handle);
                lastIoStatus_ = "FRONT: selected renderable.";
                EnsureHierarchySelectionVisible(layout.hierarchyInner);
                return true;
            }
            return false;
        }
        return false;
    }

    void TryLaunchPlayer() {
        std::optional<ri::content::GameManifest> targetManifest{};
        if (!workspaceGames_.empty() && focusedWorkspaceGameIndex_ >= 0 &&
            focusedWorkspaceGameIndex_ < static_cast<int>(workspaceGames_.size())) {
            targetManifest = ri::content::LoadGameManifest(
                workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)].rootPath / "manifest.json");
        }
        if (!targetManifest.has_value()) {
            targetManifest = sceneConfig_.gameManifest;
        }
        if (!targetManifest.has_value()) {
            lastIoStatus_ = "Play: no project manifest is active.";
            return;
        }
        const ri::editor::PlaytestLaunchResult launchResult =
            ri::editor::LaunchPlaytestForManifest(hwnd_, *targetManifest, sceneConfig_.workspaceRoot);
        lastIoStatus_ = launchResult.message;
    }

    [[nodiscard]] fs::path ResolveSceneStatePath() const {
        return sceneConfig_.sceneStatePath;
    }

    [[nodiscard]] fs::path ResolveAuthoredSceneStatePath(const fs::path& baseScenePath) const {
        return baseScenePath.parent_path() / "authored_scene.ri_editor";
    }

    [[nodiscard]] fs::path ResolveAuthoredSceneStatePath() const {
        return ResolveAuthoredSceneStatePath(ResolveSceneStatePath());
    }

    [[nodiscard]] bool SavePersistentEditorScene(const fs::path& baseScenePath,
                                                 const fs::path& orbitPath,
                                                 std::string* errorMessage) {
        const bool sceneSaved = ri::scene::SaveSceneNodeTransforms(starterScene_.scene, baseScenePath);
        const bool authoredSaved = SaveEditorAuthoredSceneState(
            starterScene_.scene,
            authoredNodeStart_,
            editorTrashFolderHandle_,
            ResolveAuthoredSceneStatePath(baseScenePath));
        const bool orbitSaved = SaveEditorOrbitStateToPath(orbitPath, editorOrbitState_);
        if (sceneSaved && authoredSaved && orbitSaved) {
            return true;
        }
        if (errorMessage != nullptr) {
            std::string message;
            if (!sceneSaved) {
                message += "transform state";
            }
            if (!authoredSaved) {
                if (!message.empty()) {
                    message += ", ";
                }
                message += "authored scene";
            }
            if (!orbitSaved) {
                if (!message.empty()) {
                    message += ", ";
                }
                message += "orbit sidecar";
            }
            *errorMessage = "failed to save " + message;
        }
        return false;
    }

    [[nodiscard]] bool TryLoadPersistentEditorScene(const fs::path& baseScenePath,
                                                    const bool loadAutosaveVariant) {
        starterScene_.scene = baselineStarterScene_;
        RebindEditorTrashFolderAfterSceneReplace();

        std::string authoredError;
        const bool authoredLoaded =
            LoadEditorAuthoredSceneState(starterScene_.scene,
                                         ResolveAuthoredSceneStatePath(baseScenePath),
                                         &authoredError);
        const bool sceneLoaded = ri::scene::LoadSceneNodeTransforms(starterScene_.scene, baseScenePath);
        const bool orbitLoaded = TryLoadEditorOrbitStateFromPath(
            loadAutosaveVariant ? ResolveAutosaveOrbitPath() : EditorOrbitSidecarPath(baseScenePath),
            editorOrbitState_);
        if (orbitLoaded) {
            ApplyEditorOrbitToScene();
        }
        RebindEditorTrashFolderAfterSceneReplace();

        if (sceneLoaded) {
            RebuildFilteredHierarchyOrder();
            autosavePending_ = false;
            lastAutosaveSteady_ = std::chrono::steady_clock::now();
            if (loadAutosaveVariant) {
                lastIoStatus_ = orbitLoaded
                    ? "Loaded autosave scene, authored nodes, and orbit."
                    : "Loaded autosave scene + authored nodes (orbit autosave missing).";
            } else {
                lastIoStatus_ = orbitLoaded
                    ? "Loaded scene transforms, authored nodes, and orbit camera."
                    : "Loaded scene transforms + authored nodes (no orbit sidecar).";
            }
            return true;
        }

        starterScene_.scene = baselineStarterScene_;
        RebindEditorTrashFolderAfterSceneReplace();
        if (!authoredLoaded && !fs::exists(baseScenePath)) {
            lastIoStatus_ = loadAutosaveVariant
                ? "No autosave state available to load."
                : "Failed to load editor scene state.";
        } else if (!authoredLoaded) {
            lastIoStatus_ = "Failed to load authored scene sidecar: " + authoredError + ".";
        } else {
            lastIoStatus_ = "Failed to load editor transform state.";
        }
        return false;
    }

    [[nodiscard]] std::string NextTriggerVolumeBasename() const {
        int maxIndex = 0;
        static constexpr std::string_view kPrefix = "Trigger_";
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            const std::string& name = node.name;
            if (name.size() <= kPrefix.size() || name.compare(0, kPrefix.size(), kPrefix) != 0) {
                continue;
            }
            int parsed = 0;
            bool anyDigit = false;
            bool bad = false;
            for (std::size_t i = kPrefix.size(); i < name.size(); ++i) {
                const char ch = name[i];
                if (ch < '0' || ch > '9') {
                    bad = true;
                    break;
                }
                anyDigit = true;
                parsed = parsed * 10 + static_cast<int>(ch - '0');
            }
            if (!bad && anyDigit) {
                maxIndex = std::max(maxIndex, parsed);
            }
        }
        return std::string(kPrefix) + std::to_string(maxIndex + 1);
    }

    void AddTriggerVolumePrimitive() {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot add trigger volume: scene has no world root.";
            return;
        }

        ri::scene::PrimitiveNodeOptions options{};
        options.parent = starterScene_.handles.root;
        options.primitive = ri::scene::PrimitiveType::Cube;
        options.shadingModel = ri::scene::ShadingModel::Unlit;
        options.nodeName = NextTriggerVolumeBasename();
        options.materialName = "author_trigger";
        options.baseColor = ri::math::Vec3{0.15f, 0.75f, 0.28f};
        options.transform.position = starterScene_.handles.orbitCamera.orbit.target;
        options.transform.scale = ri::math::Vec3{2.0f, 2.0f, 2.0f};

        const int newHandle = ri::scene::AddPrimitiveNode(starterScene_.scene, options);
        selectedNode_ = static_cast<std::size_t>(newHandle);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Added trigger volume '" + options.nodeName +
                        "'. Resize/move it, then Ctrl+E exports assembly.primitives.csv and assembly.triggers.csv.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool TryExportAssemblyTriggersCsv(const fs::path& outputPath, std::string* errorMessage) const {
        std::error_code ec{};
        fs::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            if (errorMessage != nullptr) {
                *errorMessage = "could not create trigger export folder";
            }
            return false;
        }

        std::ofstream stream(outputPath, std::ios::out | std::ios::trunc);
        if (!stream.is_open()) {
            if (errorMessage != nullptr) {
                *errorMessage = "could not open trigger export file";
            }
            return false;
        }

        stream << "trigger_id,event_type,min_x,min_y,min_z,max_x,max_y,max_z,param\n";
        std::size_t exportedCount = 0;
        for (std::size_t index = 0; index < starterScene_.scene.NodeCount(); ++index) {
            const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(index));
            if (node.name.rfind("Trigger_", 0) != 0) {
                continue;
            }
            if (node.mesh == ri::scene::kInvalidHandle) {
                continue;
            }
            const ri::scene::Mesh& mesh = starterScene_.scene.GetMesh(node.mesh);
            if (mesh.primitive != ri::scene::PrimitiveType::Cube) {
                continue;
            }
            const std::optional<ri::scene::WorldBounds> bounds =
                ri::scene::ComputeNodeWorldBounds(starterScene_.scene, static_cast<int>(index), false);
            if (!bounds.has_value()) {
                continue;
            }
            stream << node.name
                   << ",generic_trigger_volume,"
                   << bounds->min.x << "," << bounds->min.y << "," << bounds->min.z << ","
                   << bounds->max.x << "," << bounds->max.y << "," << bounds->max.z << ","
                   << "\n";
            exportedCount += 1U;
        }

        if (!stream.good()) {
            if (errorMessage != nullptr) {
                *errorMessage = "failed while writing trigger rows";
            }
            return false;
        }
        if (exportedCount == 0U && errorMessage != nullptr) {
            *errorMessage = "no Trigger_* cube nodes found to export";
        }
        return exportedCount > 0U;
    }

    void TryExportAssemblyPrimitivesCsv() {
        fs::path outputPath;
        fs::path triggersOutputPath;
        std::string destinationSummary;
        if (!workspaceGames_.empty() && focusedWorkspaceGameIndex_ >= 0 &&
            focusedWorkspaceGameIndex_ < static_cast<int>(workspaceGames_.size())) {
            const WorkspaceGameEntry& game = workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)];
            outputPath = game.rootPath / "levels" / "assembly.primitives.csv";
            triggersOutputPath = game.rootPath / "levels" / "assembly.triggers.csv";
            destinationSummary = game.displayName + " → levels/assembly.primitives.csv";
        } else {
            outputPath = ResolveSceneStatePath().parent_path() / "assembly.primitives.export.csv";
            triggersOutputPath = ResolveSceneStatePath().parent_path() / "assembly.triggers.export.csv";
            destinationSummary = "editor session folder (no workspace game focused)";
        }

        std::error_code ec{};
        fs::create_directories(outputPath.parent_path(), ec);
        if (ec) {
            lastIoStatus_ = "Export primitives: could not create folder " + outputPath.parent_path().string();
            return;
        }

        std::string error;
        if (!ri::scene::TryExportAssemblyPrimitivesCsv(starterScene_.scene, starterScene_.handles.root, outputPath,
                                                       &error)) {
            lastIoStatus_ = "Export primitives failed: " + error;
            return;
        }

        std::size_t customRenderableCount = 0;
        for (const int handle : ri::scene::CollectRenderableNodes(starterScene_.scene)) {
            const ri::scene::Node& node = starterScene_.scene.GetNode(handle);
            if (node.mesh == ri::scene::kInvalidHandle) {
                continue;
            }
            const ri::scene::Mesh& mesh = starterScene_.scene.GetMesh(node.mesh);
            if (mesh.primitive == ri::scene::PrimitiveType::Custom) {
                customRenderableCount += 1U;
            }
        }

        lastIoStatus_ = "Exported game-format assembly primitives (" + destinationSummary + "). Full path: " +
                        outputPath.string();
        if (customRenderableCount > 0U) {
            lastIoStatus_ += "  Skipped custom/brush meshes: " + std::to_string(customRenderableCount) +
                             " (CSV supports cube/plane rows only).";
        }

        std::string triggerError;
        if (TryExportAssemblyTriggersCsv(triggersOutputPath, &triggerError)) {
            lastIoStatus_ += "  Exported trigger volumes to " + triggersOutputPath.filename().string() + ".";
        } else if (!triggerError.empty() && triggerError != "no Trigger_* cube nodes found to export") {
            lastIoStatus_ += "  Trigger export failed: " + triggerError + ".";
        }
    }

    [[nodiscard]] std::string NextAuthoringPrimitiveBasename(const std::string_view prefix) const {
        int maxIndex = 0;
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            const std::string& name = node.name;
            if (name.size() <= prefix.size()) {
                continue;
            }
            if (name.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            int parsed = 0;
            bool anyDigit = false;
            bool bad = false;
            for (std::size_t i = prefix.size(); i < name.size(); ++i) {
                const char ch = name[i];
                if (ch < '0' || ch > '9') {
                    bad = true;
                    break;
                }
                anyDigit = true;
                parsed = parsed * 10 + static_cast<int>(ch - '0');
            }
            if (!bad && anyDigit) {
                maxIndex = std::max(maxIndex, parsed);
            }
        }
        return std::string(prefix) + std::to_string(maxIndex + 1);
    }

    [[nodiscard]] std::string NextStructuralBrushBasename(const std::string_view structuralType) const {
        const std::string prefix = std::string("Brush_") + std::string(structuralType) + "_";
        int maxIndex = 0;
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            const std::string& name = node.name;
            if (name.size() <= prefix.size()) {
                continue;
            }
            if (name.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            int parsed = 0;
            bool anyDigit = false;
            bool bad = false;
            for (std::size_t i = prefix.size(); i < name.size(); ++i) {
                const char ch = name[i];
                if (ch < '0' || ch > '9') {
                    bad = true;
                    break;
                }
                anyDigit = true;
                parsed = parsed * 10 + static_cast<int>(ch - '0');
            }
            if (!bad && anyDigit) {
                maxIndex = std::max(maxIndex, parsed);
            }
        }
        return prefix + std::to_string(maxIndex + 1);
    }

    [[nodiscard]] const ri::scene::StructuralPrimitivePreset& CurrentStructuralPrimitivePreset() const {
        return ri::scene::kStructuralPrimitivePresets[structuralBrushPresetIndex_ % ri::scene::kStructuralPrimitivePresets.size()];
    }

    void AddAuthoringPrimitive(const ri::scene::PrimitiveType primitive) {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot add primitive: scene has no world root.";
            return;
        }

        ri::scene::PrimitiveNodeOptions options{};
        options.parent = starterScene_.handles.root;
        options.primitive = primitive;
        options.shadingModel = ri::scene::ShadingModel::Lit;
        options.textureTiling = ri::math::Vec2{2.0f, 2.0f};
        options.baseColorTexture = "ri_psx_wall_vent.png";

        const ri::math::Vec3 focus = starterScene_.handles.orbitCamera.orbit.target;
        if (primitive == ri::scene::PrimitiveType::Cube) {
            options.nodeName = NextAuthoringPrimitiveBasename("Block_");
            options.materialName = "author_block";
            options.baseColor = ri::math::Vec3{0.62f, 0.66f, 0.72f};
            options.transform.position = ri::math::Vec3{focus.x, focus.y + 0.5f, focus.z};
            options.transform.scale = ri::math::Vec3{1.0f, 1.0f, 1.0f};
        } else if (primitive == ri::scene::PrimitiveType::Plane) {
            options.nodeName = NextAuthoringPrimitiveBasename("Slab_");
            options.materialName = "author_slab";
            options.baseColor = ri::math::Vec3{0.42f, 0.48f, 0.44f};
            options.transform.position = ri::math::Vec3{focus.x, 0.0f, focus.z};
            options.transform.scale = ri::math::Vec3{6.0f, 1.0f, 6.0f};
        } else {
            lastIoStatus_ = "Export pipeline currently authors cube/plane primitives only.";
            return;
        }

        const int newHandle = ri::scene::AddPrimitiveNode(starterScene_.scene, options);
        selectedNode_ = static_cast<std::size_t>(newHandle);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Added " + options.nodeName + " (" + ri::scene::ToString(primitive) +
                        ") under World. Adjust with WASDQE (Shift fine / Alt coarse) · Export with Ctrl+E.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void CycleStructuralPrimitivePreset(const int delta) {
        const int n = static_cast<int>(ri::scene::kStructuralPrimitivePresets.size());
        int idx = static_cast<int>(structuralBrushPresetIndex_);
        idx += delta;
        idx %= n;
        if (idx < 0) {
            idx += n;
        }
        structuralBrushPresetIndex_ = static_cast<std::size_t>(idx);
        const ri::scene::StructuralPrimitivePreset& preset = CurrentStructuralPrimitivePreset();
        lastIoStatus_ =
            "Structural brush preset [" + std::to_string(static_cast<int>(structuralBrushPresetIndex_) + 1) +
            "/" + std::to_string(ri::scene::kStructuralPrimitivePresets.size()) + "]: " + std::string(preset.label) +
            " (" + std::string(preset.structuralType) + ")  ([ / ] cycle · Ctrl+Shift+1..9 quick select · Ctrl+Shift+B place)";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void SelectStructuralPrimitivePresetByDigit(const int oneBasedDigit) {
        if (oneBasedDigit < 1 || oneBasedDigit > 9) {
            return;
        }
        const std::size_t idx = static_cast<std::size_t>(oneBasedDigit - 1);
        if (idx >= ri::scene::kStructuralPrimitivePresets.size()) {
            lastIoStatus_ = "No structural preset bound to Ctrl+Shift+" + std::to_string(oneBasedDigit) + ".";
            return;
        }
        structuralBrushPresetIndex_ = idx;
        const ri::scene::StructuralPrimitivePreset& preset = CurrentStructuralPrimitivePreset();
        lastIoStatus_ = "Selected structural preset " + std::string(preset.label) + " (" +
                        std::string(preset.structuralType) + ") via Ctrl+Shift+" + std::to_string(oneBasedDigit) + ".";
    }

    void SpawnStructuralBrushAtFocus() {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot spawn brush: scene has no world root.";
            return;
        }
        if (!SetInspectorPanel(InspectorPanel::Brush)) {
            return;
        }
        const ri::scene::StructuralPrimitivePreset& preset = CurrentStructuralPrimitivePreset();
        const std::string_view type = preset.structuralType;
        ri::scene::StructuralBrushSpawnOptions opt{};
        opt.structuralType = type;
        opt.shape = ri::scene::ShapeFromStructuralPreset(preset);
        opt.parent = starterScene_.handles.root;
        opt.nodeName = NextStructuralBrushBasename(SanitizeBrushLabelForName(preset.label));
        opt.transform.position =
            StructuralBrushSpawnPosition(type, starterScene_.handles.orbitCamera.orbit.target);
        opt.materialName = std::string("brush_") + SanitizeBrushLabelForName(preset.label);
        opt.baseColorTexture = "ri_psx_wall_vent.png";
        opt.textureTiling = ri::math::Vec2{2.0f, 2.0f};
        opt.baseColor = ri::math::Vec3{0.58f, 0.62f, 0.68f};

        const int newHandle = ri::scene::AddStructuralBrushNode(starterScene_.scene, opt);
        if (newHandle == ri::scene::kInvalidHandle) {
            lastIoStatus_ =
                "Structural brush '" + std::string(type) + "' produced no mesh (internal compiler issue).";
            return;
        }
        selectedNode_ = static_cast<std::size_t>(newHandle);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Placed structural brush '" + std::string(preset.label) + "' (" +
                        std::string(preset.structuralType) + ") as " + opt.nodeName +
                        " (Custom mesh). Game CSV export lists procedural cube/plane rows only.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void EnsureEditorTrashFolder() {
        if (editorTrashFolderHandle_ >= 0) {
            return;
        }
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            return;
        }
        editorTrashFolderHandle_ =
            starterScene_.scene.CreateNode("EditorTrash", starterScene_.handles.root);
    }

    [[nodiscard]] EditorSceneControllerContext BuildSceneControllerContext() const {
        return EditorSceneControllerContext{
            .handles = &starterScene_.handles,
            .editorTrashFolderHandle = editorTrashFolderHandle_,
        };
    }

    [[nodiscard]] bool IsProtectedEditorNode(const int handle) const {
        return ri::editor::IsProtectedEditorNode(BuildSceneControllerContext(), handle);
    }

    [[nodiscard]] std::string MakeUniqueNodeName(const std::string& baseName) const {
        return ri::editor::MakeUniqueNodeName(starterScene_.scene, baseName);
    }

    [[nodiscard]] bool TryAssignLocalTransformFromWorld(const int nodeHandle, const ri::math::Mat4& worldMatrix) {
        return ri::editor::TryAssignLocalTransformFromWorld(starterScene_.scene, nodeHandle, worldMatrix);
    }

    [[nodiscard]] float ApplyStepModifiers(const float baseStep, const bool fine, const bool coarse) const {
        if (fine) {
            return baseStep * 0.25f;
        }
        if (coarse) {
            return baseStep * 4.0f;
        }
        return baseStep;
    }

    [[nodiscard]] bool IsEditableAuthoredNode(const int handle) const {
        return ri::editor::IsEditableAuthoredNode(starterScene_.scene, BuildSceneControllerContext(), handle);
    }

    void TryResetSelectedTransform() {
        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        if (!ri::editor::TryResetSelectedTransform(
                starterScene_.scene, BuildSceneControllerContext(), selectedNode_, lastIoStatus_)) {
            return;
        }
        const ri::scene::Transform after = node.localTransform;
        PushEditAction(TransformEditAction{selectedNode_, before, after});
    }

    void TryFrameAllRenderables() {
        if (autoOrbitPreview_) {
            lastIoStatus_ = "Frame all unavailable while auto-orbit preview is running.";
            return;
        }
        const std::vector<int> handles = ri::scene::CollectRenderableNodes(starterScene_.scene);
        if (handles.empty()) {
            lastIoStatus_ = "Frame all: no renderable nodes.";
            return;
        }
        if (ri::scene::FrameNodesWithOrbitCamera(starterScene_.scene,
                                                 starterScene_.handles.orbitCamera,
                                                 handles,
                                                 1.25f)) {
            editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
            ApplyEditorOrbitToScene();
            lastIoStatus_ = "Framed all renderables in orbit camera.";
        } else {
            lastIoStatus_ = "Frame all failed.";
        }
    }

    void TryReparentSelectedToWorldRoot() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        if (!ri::editor::TryReparentSelectedToWorldRoot(
                starterScene_.scene, BuildSceneControllerContext(), selectedNode_, lastIoStatus_)) {
            return;
        }
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TrySelectAdjacentAuthoredNode(const int direction) {
        const std::vector<int> order = HierarchyDrawOrder();
        if (ri::editor::TrySelectAdjacentAuthoredNode(
                starterScene_.scene, BuildSceneControllerContext(), order, direction, selectedNode_, lastIoStatus_)) {
            const EditorLayout layout = ComputeLayout();
            EnsureHierarchySelectionVisible(layout.hierarchyInner);
        }
    }

    void TrySaveTimestampedSceneSnapshot() {
        const fs::path basePath = ResolveSceneStatePath();
        std::error_code ec{};
        fs::create_directories(basePath.parent_path(), ec);
        if (ec) {
            lastIoStatus_ = "Snapshot failed: could not create save folder.";
            return;
        }

        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm localTm{};
#if defined(_WIN32)
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        std::ostringstream stamp{};
        stamp << std::put_time(&localTm, "%Y%m%d_%H%M%S");
        const std::string token = stamp.str();

        const fs::path snapshotPath =
            basePath.parent_path() / ("scene_state_snapshot_" + token + ".ri_state");
        const fs::path orbitSnapshotPath =
            basePath.parent_path() / ("editor_orbit_snapshot_" + token + ".ri_cam");
        const bool sceneSaved = ri::scene::SaveSceneNodeTransforms(starterScene_.scene, snapshotPath);
        const bool authoredSaved = SaveEditorAuthoredSceneState(
            starterScene_.scene,
            authoredNodeStart_,
            editorTrashFolderHandle_,
            ResolveAuthoredSceneStatePath(snapshotPath));
        const bool orbitSaved = SaveEditorOrbitStateToPath(orbitSnapshotPath, editorOrbitState_);
        if (sceneSaved && authoredSaved && orbitSaved) {
            lastIoStatus_ = "Snapshot saved: " + snapshotPath.filename().string();
        } else if (sceneSaved && authoredSaved) {
            lastIoStatus_ = "Snapshot saved, but orbit snapshot failed.";
        } else {
            lastIoStatus_ = "Snapshot failed while writing scene state.";
        }
    }

    [[nodiscard]] fs::path ResolveAutosaveScenePath() const {
        return ResolveSceneStatePath().parent_path() / "autosave_scene_state.ri_state";
    }

    [[nodiscard]] fs::path ResolveAutosaveOrbitPath() const {
        return ResolveSceneStatePath().parent_path() / "autosave_editor_orbit.ri_cam";
    }

    void TryLoadAutosaveState() {
        (void)TryLoadPersistentEditorScene(ResolveAutosaveScenePath(), true);
    }

    void MaybeAutosaveState() {
        if (!autosavePending_) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - lastAutosaveSteady_ < kAutosaveInterval_) {
            return;
        }
        std::string error;
        if (SavePersistentEditorScene(ResolveAutosaveScenePath(), ResolveAutosaveOrbitPath(), &error)) {
            autosavePending_ = false;
            lastAutosaveSteady_ = now;
            if (elapsedSeconds_ - lastAutosaveStatusSeconds_ > 6.0) {
                lastIoStatus_ = "Autosaved scene state.";
                lastAutosaveStatusSeconds_ = elapsedSeconds_;
            }
        }
    }

    void PushEditAction(EditorEditAction action) {
        undoStack_.push_back(std::move(action));
        if (undoStack_.size() > kMaxUndoActions) {
            undoStack_.erase(undoStack_.begin());
        }
        redoStack_.clear();
        autosavePending_ = true;
    }

    void RecordSceneGraphEdit(const ri::scene::Scene& beforeScene, const std::size_t beforeSelectedNode) {
        PushEditAction(SceneGraphEditAction{
            .beforeScene = beforeScene,
            .afterScene = starterScene_.scene,
            .beforeSelectedNode = beforeSelectedNode,
            .afterSelectedNode = selectedNode_,
        });
        RebuildFilteredHierarchyOrder();
    }

    void RebindEditorTrashFolderAfterSceneReplace() {
        editorTrashFolderHandle_ = ri::scene::kInvalidHandle;
        const std::size_t nodeCount = starterScene_.scene.NodeCount();
        for (std::size_t index = 0; index < nodeCount; ++index) {
            const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(index));
            if (node.name == "EditorTrash") {
                editorTrashFolderHandle_ = static_cast<int>(index);
                break;
            }
        }
        if (editorTrashFolderHandle_ == ri::scene::kInvalidHandle) {
            EnsureEditorTrashFolder();
        }
        if (starterScene_.scene.NodeCount() == 0) {
            selectedNode_ = 0;
        } else if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            selectedNode_ = starterScene_.scene.NodeCount() - 1;
        }
    }

    void TryCreateGroupNode() {
        if (!SetInspectorPanel(InspectorPanel::Node)) {
            return;
        }
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        if (!ri::editor::TryCreateGroupNode(
                starterScene_.scene, BuildSceneControllerContext(), selectedNode_, lastIoStatus_)) {
            return;
        }
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TryGroupSelectedNode() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        if (!SetInspectorPanel(InspectorPanel::Node)) {
            return;
        }
        if (!ri::editor::TryGroupSelectedNode(
                starterScene_.scene, BuildSceneControllerContext(), selectedNode_, lastIoStatus_)) {
            return;
        }
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TryUngroupSelectedNode() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        EnsureEditorTrashFolder();
        if (!ri::editor::TryUngroupSelectedNode(
                starterScene_.scene, BuildSceneControllerContext(), selectedNode_, lastIoStatus_)) {
            return;
        }
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TryDeleteSelectedNode() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        EnsureEditorTrashFolder();
        if (!ri::editor::TryDeleteSelectedNode(
                starterScene_.scene, BuildSceneControllerContext(), selectedNode_, lastIoStatus_)) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void TryBeginNodeRename() {
        if (!IsEditableAuthoredNode(static_cast<int>(selectedNode_))) {
            lastIoStatus_ = "Rename: select an editable authored node (not World/rigs/helpers).";
            return;
        }
        nodeRenameDraft_ = starterScene_.scene.GetNode(static_cast<int>(selectedNode_)).name;
        nodeRenameTypingActive_ = true;
        lastIoStatus_ = "Rename: " + nodeRenameDraft_ + "  (type, Enter=apply, Esc=cancel)";
    }

    void TryCommitNodeRename() {
        if (!nodeRenameTypingActive_) {
            return;
        }
        nodeRenameTypingActive_ = false;
        const std::string trimmed = nodeRenameDraft_;
        nodeRenameDraft_.clear();
        if (trimmed.empty()) {
            lastIoStatus_ = "Rename cancelled (empty name).";
            return;
        }
        if (!IsEditableAuthoredNode(static_cast<int>(selectedNode_))) {
            lastIoStatus_ = "Rename failed: selection is no longer editable.";
            return;
        }
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        starterScene_.scene.GetNode(static_cast<int>(selectedNode_)).name = trimmed;
        lastIoStatus_ = "Renamed node to '" + trimmed + "'.";
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
    }

    void TryDuplicateSelectedNode() {
        const ri::scene::Scene beforeScene = starterScene_.scene;
        const std::size_t beforeSelectedNode = selectedNode_;
        if (!ri::editor::TryDuplicateSelectedNode(
                starterScene_.scene, BuildSceneControllerContext(), selectedNode_, lastIoStatus_)) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        RecordSceneGraphEdit(beforeScene, beforeSelectedNode);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void NudgeSelectedTransformComponent(const int component, const int axis, const float direction) {
        if (!IsEditableAuthoredNode(static_cast<int>(selectedNode_))) {
            lastIoStatus_ = "Inspector edit blocked: select an editable node (not World/rigs/helpers).";
            return;
        }
        const EditMode savedMode = editMode_;
        const int savedAxis = activeAxis_;
        if (component == 0) {
            editMode_ = EditMode::Translate;
        } else if (component == 1) {
            editMode_ = EditMode::Rotate;
        } else {
            editMode_ = EditMode::Scale;
        }
        activeAxis_ = std::clamp(axis, 0, 2);
        const float step = component == 1 ? 2.5f : (component == 0 ? ActiveGridSnapStep() : 0.08f);
        ApplySelectedNodeEdit(direction * step);
        editMode_ = savedMode;
        activeAxis_ = savedAxis;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool TryHandleInspectorNudgeClick(const POINT& point) {
        if (inspectorPanel_ != InspectorPanel::Node) {
            return false;
        }
        for (std::size_t index = 0; index < inspectorNudgeButtons_.size(); ++index) {
            const RECT& rect = inspectorNudgeButtons_[index];
            if (rect.left >= rect.right || rect.top >= rect.bottom) {
                continue;
            }
            if (point.x < rect.left || point.x > rect.right || point.y < rect.top || point.y > rect.bottom) {
                continue;
            }
            const int component = static_cast<int>(index / 6);
            const int axis = static_cast<int>((index / 2) % 3);
            const float direction = (index % 2) == 0 ? -1.0f : 1.0f;
            NudgeSelectedTransformComponent(component, axis, direction);
            return true;
        }
        return false;
    }

    void DrawInspectorNudgeRow(HDC dc,
                              int& top,
                              const RECT& inspectorInner,
                              const char* label,
                              const int componentIndex) {
        DrawTextLine(dc,
                     RECT{inspectorInner.left + 10, top, inspectorInner.left + 70, top + 18},
                     label,
                     RGB(200, 200, 200),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        const char* axisLabels[3] = {"X", "Y", "Z"};
        int x = inspectorInner.left + 72;
        for (int axis = 0; axis < 3; ++axis) {
            const RECT minusRect{x, top, x + 18, top + 18};
            const RECT plusRect{x + 20, top, x + 38, top + 18};
            const std::size_t minusIndex = static_cast<std::size_t>(componentIndex * 6 + axis * 2);
            const std::size_t plusIndex = minusIndex + 1;
            inspectorNudgeButtons_[minusIndex] = minusRect;
            inspectorNudgeButtons_[plusIndex] = plusRect;
            DrawToolbarButton(dc, minusRect, "-", false);
            DrawToolbarButton(dc, plusRect, "+", false);
            DrawTextLine(dc,
                         RECT{x + 40, top, x + 52, top + 18},
                         axisLabels[axis],
                         RGB(180, 186, 196),
                         smallFont_,
                         DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            x += 58;
        }
        top += 22;
    }

    void ApplySelectedNodeEdit(float delta) {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return;
        }

        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        if (editMode_ == EditMode::Translate) {
            if (activeAxis_ == 0) {
                node.localTransform.position.x += delta;
                if (gridSnapEnabled_) {
                    node.localTransform.position.x = SnapToGrid(node.localTransform.position.x);
                }
            } else if (activeAxis_ == 1) {
                node.localTransform.position.y += delta;
                if (gridSnapEnabled_) {
                    node.localTransform.position.y = SnapToGrid(node.localTransform.position.y);
                }
            } else {
                node.localTransform.position.z += delta;
                if (gridSnapEnabled_) {
                    node.localTransform.position.z = SnapToGrid(node.localTransform.position.z);
                }
            }
        } else if (editMode_ == EditMode::Rotate) {
            if (activeAxis_ == 0) {
                node.localTransform.rotationDegrees.x += delta;
            } else if (activeAxis_ == 1) {
                node.localTransform.rotationDegrees.y += delta;
            } else {
                node.localTransform.rotationDegrees.z += delta;
            }
        } else {
            auto clampScale = [](float value) {
                return std::max(0.01f, value);
            };
            if (activeAxis_ == 0) {
                node.localTransform.scale.x = clampScale(node.localTransform.scale.x + delta);
            } else if (activeAxis_ == 1) {
                node.localTransform.scale.y = clampScale(node.localTransform.scale.y + delta);
            } else {
                node.localTransform.scale.z = clampScale(node.localTransform.scale.z + delta);
            }
        }

        const ri::scene::Transform after = node.localTransform;
        if (before.position.x == after.position.x &&
            before.position.y == after.position.y &&
            before.position.z == after.position.z &&
            before.rotationDegrees.x == after.rotationDegrees.x &&
            before.rotationDegrees.y == after.rotationDegrees.y &&
            before.rotationDegrees.z == after.rotationDegrees.z &&
            before.scale.x == after.scale.x &&
            before.scale.y == after.scale.y &&
            before.scale.z == after.scale.z) {
            return;
        }

        PushEditAction(TransformEditAction{selectedNode_, before, after});
    }

    void ApplySelectedNodeTranslationDelta(const ri::math::Vec3& delta) {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return;
        }
        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) {
            return;
        }

        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        node.localTransform.position.x += delta.x;
        node.localTransform.position.y += delta.y;
        node.localTransform.position.z += delta.z;
        if (gridSnapEnabled_) {
            node.localTransform.position.x = SnapToGrid(node.localTransform.position.x);
            node.localTransform.position.y = SnapToGrid(node.localTransform.position.y);
            node.localTransform.position.z = SnapToGrid(node.localTransform.position.z);
        }
        const ri::scene::Transform after = node.localTransform;

        PushEditAction(TransformEditAction{selectedNode_, before, after});
    }

    bool SnapSelectedNodeToGridNow() {
        if (!gridSnapEnabled_) {
            lastIoStatus_ = "Grid snap is OFF. Press G to enable, then Ctrl+G to snap selection.";
            return false;
        }
        if (!IsEditableAuthoredNode(static_cast<int>(selectedNode_))) {
            lastIoStatus_ = "Cannot snap: select an editable authored node.";
            return false;
        }
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return false;
        }

        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        node.localTransform.position.x = SnapToGrid(node.localTransform.position.x);
        node.localTransform.position.y = SnapToGrid(node.localTransform.position.y);
        node.localTransform.position.z = SnapToGrid(node.localTransform.position.z);
        const ri::scene::Transform after = node.localTransform;
        if (before.position.x == after.position.x &&
            before.position.y == after.position.y &&
            before.position.z == after.position.z) {
            lastIoStatus_ = "Selection already aligned to grid (" + GridSnapLabel() + ").";
            return false;
        }

        PushEditAction(TransformEditAction{selectedNode_, before, after});
        lastIoStatus_ = "Snapped selection to grid (" + GridSnapLabel() + ").";
        return true;
    }

    void ApplySelectedNodeRotationDelta(const ri::math::Vec3& deltaDegrees) {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return;
        }
        if (deltaDegrees.x == 0.0f && deltaDegrees.y == 0.0f && deltaDegrees.z == 0.0f) {
            return;
        }

        ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        const ri::scene::Transform before = node.localTransform;
        node.localTransform.rotationDegrees.x += deltaDegrees.x;
        node.localTransform.rotationDegrees.y += deltaDegrees.y;
        node.localTransform.rotationDegrees.z += deltaDegrees.z;
        const ri::scene::Transform after = node.localTransform;

        PushEditAction(TransformEditAction{selectedNode_, before, after});
    }

    [[nodiscard]] bool UndoLastEdit() {
        if (undoStack_.empty()) {
            return false;
        }

        const EditorEditAction action = undoStack_.back();
        undoStack_.pop_back();
        if (std::holds_alternative<TransformEditAction>(action)) {
            const TransformEditAction& transformAction = std::get<TransformEditAction>(action);
            if (transformAction.nodeIndex >= starterScene_.scene.NodeCount()) {
                return false;
            }
            ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(transformAction.nodeIndex));
            node.localTransform = transformAction.before;
            selectedNode_ = transformAction.nodeIndex;
            redoStack_.push_back(action);
            return true;
        }

        const SceneGraphEditAction& sceneAction = std::get<SceneGraphEditAction>(action);
        starterScene_.scene = sceneAction.beforeScene;
        selectedNode_ = sceneAction.beforeSelectedNode;
        RebindEditorTrashFolderAfterSceneReplace();
        redoStack_.push_back(action);
        return true;
    }

    [[nodiscard]] bool RedoLastEdit() {
        if (redoStack_.empty()) {
            return false;
        }

        const EditorEditAction action = redoStack_.back();
        redoStack_.pop_back();
        if (std::holds_alternative<TransformEditAction>(action)) {
            const TransformEditAction& transformAction = std::get<TransformEditAction>(action);
            if (transformAction.nodeIndex >= starterScene_.scene.NodeCount()) {
                return false;
            }
            ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(transformAction.nodeIndex));
            node.localTransform = transformAction.after;
            selectedNode_ = transformAction.nodeIndex;
            undoStack_.push_back(action);
            return true;
        }

        const SceneGraphEditAction& sceneAction = std::get<SceneGraphEditAction>(action);
        starterScene_.scene = sceneAction.afterScene;
        selectedNode_ = sceneAction.afterSelectedNode;
        RebindEditorTrashFolderAfterSceneReplace();
        undoStack_.push_back(action);
        return true;
    }

    [[nodiscard]] std::string EditModeLabel() const {
        switch (editMode_) {
            case EditMode::Translate:
                return "Translate";
            case EditMode::Rotate:
                return "Rotate";
            case EditMode::Scale:
                return "Scale";
        }
        return "Translate";
    }

    [[nodiscard]] std::string AxisLabel() const {
        if (activeAxis_ == 0) {
            return "X";
        }
        if (activeAxis_ == 1) {
            return "Y";
        }
        return "Z";
    }

    [[nodiscard]] std::string InspectorPanelLabel() const {
        switch (inspectorPanel_) {
            case InspectorPanel::Node: return "Node";
            case InspectorPanel::Brush: return "Brush";
            case InspectorPanel::Gameplay: return "Gameplay";
            case InspectorPanel::Files: return "Files";
            case InspectorPanel::UiWorkbench: return "UI / VN";
        }
        return "Node";
    }

    [[nodiscard]] std::string EditStepLabel() const {
        if (editMode_ == EditMode::Translate) {
            return "0.10u";
        }
        if (editMode_ == EditMode::Rotate) {
            return "2.50deg";
        }
        return "0.08s";
    }

    [[nodiscard]] float ActiveGridSnapStep() const {
        static constexpr std::array<float, 6> kSnapSteps = {0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
        return kSnapSteps[gridSnapStepIndex_ % kSnapSteps.size()];
    }

    [[nodiscard]] std::string GridSnapLabel() const {
        std::ostringstream stream{};
        stream << std::fixed << std::setprecision(3) << ActiveGridSnapStep();
        return stream.str() + "u";
    }

    void CycleGridSnapStep(int direction) {
        static constexpr int kCount = 6;
        int next = gridSnapStepIndex_ + (direction >= 0 ? 1 : -1);
        while (next < 0) {
            next += kCount;
        }
        gridSnapStepIndex_ = next % kCount;
    }

    [[nodiscard]] float SnapToGrid(float value) const {
        const float step = std::max(0.0001f, ActiveGridSnapStep());
        return std::round(value / step) * step;
    }

    [[nodiscard]] std::string InventoryPresentationLabel() const {
        switch (creatorInventoryPolicy_.presentation) {
            case ri::world::InventoryPresentationMode::Disabled: return "disabled";
            case ri::world::InventoryPresentationMode::HiddenDataOnly: return "hidden_data_only";
            case ri::world::InventoryPresentationMode::Visible: return "visible";
        }
        return "visible";
    }

    [[nodiscard]] bool IsTriggerNode(const ri::scene::Node& node) const {
        return node.name.rfind("Trigger_", 0) == 0;
    }

    [[nodiscard]] std::size_t CountAuthoredNodes() const {
        std::size_t count = 0;
        for (std::size_t i = authoredNodeStart_; i < starterScene_.scene.NodeCount(); ++i) {
            if (!IsProtectedEditorNode(static_cast<int>(i))) {
                count += 1;
            }
        }
        return count;
    }

    [[nodiscard]] std::size_t CountTriggerNodes() const {
        std::size_t count = 0;
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            if (IsTriggerNode(node)) {
                count += 1;
            }
        }
        return count;
    }

    [[nodiscard]] std::string FocusedWorkspaceGameLabel() const {
        if (focusedWorkspaceGameIndex_ >= 0
            && focusedWorkspaceGameIndex_ < static_cast<int>(workspaceGames_.size())) {
            return workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)].displayName;
        }
        if (sceneConfig_.gameManifest.has_value()) {
            return sceneConfig_.gameManifest->id;
        }
        return "No game mounted";
    }

    [[nodiscard]] std::string SelectedNodeSummary() const {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return "No selection";
        }
        const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        return node.name + "  |  " + NodeKindLabel(node);
    }

    [[nodiscard]] std::string ResourceFocusSummary() const {
        if (selectedResourceRow_ >= 0
            && selectedResourceRow_ < static_cast<int>(resourceCatalogEntries_.size())) {
            return resourceCatalogEntries_[static_cast<std::size_t>(selectedResourceRow_)].relativePathUtf8;
        }
        return "No file selected";
    }

    [[nodiscard]] std::string TriggerSelectionSummary() const {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return "Selection is not a trigger volume.";
        }
        const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        if (!IsTriggerNode(node)) {
            return "Selection is not a trigger volume.";
        }
        const auto bounds =
            ri::scene::ComputeNodeWorldBounds(starterScene_.scene, static_cast<int>(selectedNode_), false);
        if (!bounds.has_value()) {
            return "Trigger volume bounds unavailable.";
        }
        const ri::math::Vec3 size = ri::scene::GetBoundsSize(*bounds);
        return "Trigger export: generic_trigger_volume  |  size " + ri::math::ToString(size);
    }

    void CycleInventoryPresentation() {
        switch (creatorInventoryPolicy_.presentation) {
        case ri::world::InventoryPresentationMode::Visible:
            creatorInventoryPolicy_.presentation = ri::world::InventoryPresentationMode::HiddenDataOnly;
            lastIoStatus_ = "Gameplay policy: inventory is now hidden_data_only.";
            break;
        case ri::world::InventoryPresentationMode::HiddenDataOnly:
            creatorInventoryPolicy_.presentation = ri::world::InventoryPresentationMode::Disabled;
            lastIoStatus_ = "Gameplay policy: inventory is now disabled.";
            break;
        case ri::world::InventoryPresentationMode::Disabled:
            creatorInventoryPolicy_.presentation = ri::world::InventoryPresentationMode::Visible;
            lastIoStatus_ = "Gameplay policy: inventory is now visible.";
            break;
        }
        SaveCreatorPolicyToDisk();
    }

    [[nodiscard]] std::string NodeKindLabel(const ri::scene::Node& node) const {
        if (node.camera != ri::scene::kInvalidHandle) {
            return "Camera";
        }
        if (node.light != ri::scene::kInvalidHandle) {
            return "Light";
        }
        if (node.mesh != ri::scene::kInvalidHandle) {
            return "Mesh";
        }
        return "Transform";
    }

    [[nodiscard]] EditorLayout ComputeLayout() const {
        RECT client{};
        GetClientRect(hwnd_, &client);
        EditorLayout layout{};
        const int clientRight = static_cast<int>(client.right);
        layout.toolStrip = RECT{10, 66, client.right - 10, 106};
        const int leftWidth = std::clamp(hierarchyPanelWidth_, 240, std::max(240, clientRight - 620));
        const int rightWidth = std::clamp(inspectorPanelWidth_, 290, std::max(290, clientRight - 620));
        layout.hierarchy = RECT{10, 116, 10 + leftWidth, client.bottom - 92};
        layout.inspector = RECT{clientRight - 10 - rightWidth, 116, clientRight - 10, client.bottom - 92};
        layout.viewport = RECT{layout.hierarchy.right + 10, 116, layout.inspector.left - 10, client.bottom - 92};
        layout.hierarchySplitter = RECT{layout.hierarchy.right + 2, 116, layout.hierarchy.right + 8, client.bottom - 92};
        layout.inspectorSplitter = RECT{layout.inspector.left - 8, 116, layout.inspector.left - 2, client.bottom - 92};
        layout.hierarchyInner = RECT{layout.hierarchy.left + 8, layout.hierarchy.top + 36, layout.hierarchy.right - 8, layout.hierarchy.bottom - 8};
        layout.viewportInner = RECT{layout.viewport.left + 8, layout.viewport.top + 36, layout.viewport.right - 8, layout.viewport.bottom - 8};
        layout.inspectorInner = RECT{layout.inspector.left + 8, layout.inspector.top + 36, layout.inspector.right - 8, layout.inspector.bottom - 8};
        return layout;
    }

    void DrawToolbarButton(HDC dc, const RECT& rect, const std::string& label, bool active) {
        EditorRenderer::DrawToolbarButton(dc, rect, label, active, smallFont_);
    }

    void DrawPanelHeader(HDC dc, const RECT& panelRect, const std::string& title, const std::string& meta = {}) {
        EditorRenderer::DrawPanelHeader(dc, panelRect, title, headerFont_, smallFont_, meta);
    }

    void DrawViewportPreview(HDC dc, const RECT& targetRect) {
        const int width = std::max(1L, targetRect.right - targetRect.left);
        const int height = std::max(1L, targetRect.bottom - targetRect.top);
        if (starterScene_.handles.orbitCamera.cameraNode == ri::scene::kInvalidHandle) {
            FillRectColor(dc, targetRect, RGB(32, 36, 42));
            return;
        }

        ri::render::software::ScenePreviewOptions options{};
        options.width = width;
        options.height = height;

        std::filesystem::path editorExe{};
        wchar_t moduleWide[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, moduleWide, MAX_PATH) > 0) {
            editorExe = std::filesystem::path(std::wstring(moduleWide));
        }
        const std::filesystem::path textureDir =
            ri::content::PickEngineTexturesDirectory(sceneConfig_.workspaceRoot, editorExe);
        if (!textureDir.empty()) {
            options.textureRoot = textureDir;
        }
        options.hiddenNodeHandles = {
            starterScene_.handles.grid,
            starterScene_.handles.axes.root,
            starterScene_.handles.axes.xAxis,
            starterScene_.handles.axes.yAxis,
            starterScene_.handles.axes.zAxis,
        };

        ri::editor::ConfigureEditorViewportForPreview(sceneConfig_.editorPreviewScene, options);

        const ri::render::software::SoftwareImage image =
            ri::render::software::RenderScenePreview(starterScene_.scene, starterScene_.handles.orbitCamera.cameraNode, options);
        if (image.pixels.empty()) {
            FillRectColor(dc, targetRect, RGB(32, 36, 42));
            return;
        }

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = image.width;
        bitmapInfo.bmiHeader.biHeight = -image.height;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 24;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;

        StretchDIBits(dc,
                      targetRect.left,
                      targetRect.top,
                      width,
                      height,
                      0,
                      0,
                      image.width,
                      image.height,
                      image.pixels.data(),
                      &bitmapInfo,
                      DIB_RGB_COLORS,
                      SRCCOPY);
    }

    [[nodiscard]] ri::world::RuntimeStatsOverlaySnapshot BuildRuntimeStatsOverlaySnapshot() const {
        ri::world::RuntimeStatsOverlaySnapshot snapshot{};
        snapshot.metrics = statsOverlayState_.GetMetrics();
        snapshot.sceneNodes = starterScene_.scene.NodeCount();
        snapshot.rootNodes = ri::scene::CollectRootNodes(starterScene_.scene).size();
        snapshot.renderables = ri::scene::CollectRenderableNodes(starterScene_.scene).size();
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            if (node.light != ri::scene::kInvalidHandle) {
                snapshot.lights += 1;
            }
            if (node.camera != ri::scene::kInvalidHandle) {
                snapshot.cameras += 1;
            }
        }
        snapshot.selectedNode = selectedNode_;
        snapshot.modeLabel = sceneConfig_.gameManifest.has_value() ? "project" : "starter";
        snapshot.sceneLabel = sceneConfig_.gameManifest.has_value()
            ? sceneConfig_.gameManifest->id
            : sceneConfig_.sceneName;
        return snapshot;
    }

    void DrawRuntimeStatsOverlay(HDC dc, const RECT& viewportRect) {
        if (!statsOverlayVisible_) {
            return;
        }

        const std::vector<std::string> lines =
            ri::world::FormatRuntimeStatsOverlayLines(BuildRuntimeStatsOverlaySnapshot(), 6);
        if (lines.empty()) {
            return;
        }

        const int lineHeight = 18;
        const int panelWidth = 296;
        const int panelHeight = 12 + static_cast<int>(lines.size()) * lineHeight;
        RECT panel{
            viewportRect.right - panelWidth - 12,
            viewportRect.top + 12,
            viewportRect.right - 12,
            viewportRect.top + 12 + panelHeight
        };
        DrawPanelFrame(dc, panel, RGB(58, 64, 74), RGB(214, 220, 228), RGB(20, 24, 28));

        int top = panel.top + 6;
        for (std::size_t index = 0; index < lines.size(); ++index) {
            DrawTextLine(dc,
                         RECT{panel.left + 10, top, panel.right - 10, top + lineHeight},
                         lines[index],
                         index == 0U ? RGB(255, 244, 195) : RGB(214, 222, 230),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            top += lineHeight;
        }
    }

    void DrawRawIronQuadViewportBlock(HDC dc, const RECT& viewportInner) {
        const ri::math::Vec3 orbitFocus = starterScene_.handles.orbitCamera.orbit.target;
        const std::string mode =
            autoOrbitPreview_ ? "auto-orbit demo" : "drag in CAMERA / wheel / Tab full 3D";
        ri::editor::RenderEditorViewportBlock(
            dc,
            viewportInner,
            ri::editor::EditorViewportBlockModel{
                .full3DViewport = full3DViewport_,
                .cameraPlotRect = cameraPlotRect_,
                .cameraSummaryLine =
                    "Camera "
                    + ri::math::ToString(
                        starterScene_.scene.ComputeWorldPosition(starterScene_.handles.orbitCamera.cameraNode))
                    + "  |  " + EditModeLabel() + "  |  Axis " + AxisLabel() + "  |  " + mode,
            },
            ri::editor::EditorViewportTheme{
                .titleFont = titleFont_,
                .headerFont = headerFont_,
                .bodyFont = bodyFont_,
                .smallFont = smallFont_,
            },
            ri::editor::EditorViewportBlockCallbacks{
                .drawViewportPreview = [this, dc](const RECT& rect) {
                    this->DrawViewportPreview(dc, rect);
                },
                .drawRuntimeStatsOverlay = [this, dc](const RECT& rect) {
                    this->DrawRuntimeStatsOverlay(dc, rect);
                },
                .drawTopView = [this, dc, orbitFocus](const RECT& rect) {
                    this->DrawRawIronFlatSceneView(
                        dc, rect, starterScene_.scene, selectedNode_, orbitFocus, RawIronFlatProjection::TopXz, "TOP (X / Z)", smallFont_);
                },
                .drawSideView = [this, dc, orbitFocus](const RECT& rect) {
                    this->DrawRawIronFlatSceneView(
                        dc, rect, starterScene_.scene, selectedNode_, orbitFocus, RawIronFlatProjection::SideZy, "SIDE (Z / Y)", smallFont_);
                },
                .drawFrontView = [this, dc, orbitFocus](const RECT& rect) {
                    this->DrawRawIronFlatSceneView(
                        dc, rect, starterScene_.scene, selectedNode_, orbitFocus, RawIronFlatProjection::FrontXy, "FRONT (X / Y)", smallFont_);
                },
            });
    }

    [[nodiscard]] std::vector<RECT> ComputeUiWorkbenchViewportBlockRects(const RECT& viewportInner,
                                                                         const UiWorkbenchPanelModel& model,
                                                                         RECT& stageRectOut) const {
        RECT headerRect{viewportInner.left + 18, viewportInner.top + 16, viewportInner.right - 18, viewportInner.top + 54};
        RECT stageCard{headerRect.right - std::max(320L, (viewportInner.right - viewportInner.left) - 254),
                       headerRect.bottom + 10,
                       viewportInner.right - 18,
                       viewportInner.bottom - 18};
        stageRectOut = RECT{stageCard.left + 18, stageCard.top + 52, stageCard.right - 18, stageCard.bottom - 18};

        std::vector<RECT> rects{};
        rects.reserve(model.previewBlocks.size());
        int blockTop = stageRectOut.top + 14;
        for (const UiWorkbenchPreviewBlock& block : model.previewBlocks) {
            const int blockHeight = block.detailLine.empty() ? 34 : 56;
            RECT blockRect{stageRectOut.left + 16, blockTop, stageRectOut.right - 16, blockTop + blockHeight};
            if (blockRect.bottom > stageRectOut.bottom - 10) {
                break;
            }
            rects.push_back(blockRect);
            blockTop += blockHeight + 12;
        }
        return rects;
    }

    [[nodiscard]] bool TryHandleUiWorkbenchViewportClick(const RECT& viewportInner, const POINT& point) {
        if (inspectorPanel_ != InspectorPanel::UiWorkbench) {
            return false;
        }
        UiWorkbenchPanelModel model = BuildUiWorkbenchPanelModel(ComputeLayout().inspectorInner);
        RECT stageRect{};
        const std::vector<RECT> blockRects = ComputeUiWorkbenchViewportBlockRects(viewportInner, model, stageRect);
        for (std::size_t i = 0; i < blockRects.size(); ++i) {
            if (PtInRect(&blockRects[i], point) != FALSE) {
                SelectUiWorkbenchBlock(static_cast<int>(i));
                return true;
            }
        }
        return false;
    }

    void DrawUiWorkbenchViewport(HDC dc, const RECT& viewportInner) {
        UiWorkbenchPanelModel model = BuildUiWorkbenchPanelModel(ComputeLayout().inspectorInner);

        const RECT headerRect{viewportInner.left + 18, viewportInner.top + 16, viewportInner.right - 18, viewportInner.top + 54};
        const RECT shelfRect{viewportInner.left + 18, headerRect.bottom + 10, viewportInner.left + 212, viewportInner.bottom - 18};
        const RECT stageCard{headerRect.right - std::max(320L, (viewportInner.right - viewportInner.left) - 254),
                             headerRect.bottom + 10,
                             viewportInner.right - 18,
                             viewportInner.bottom - 18};

        DrawInsetFrame(dc, headerRect, RGB(64, 70, 80), RGB(188, 194, 202), RGB(24, 28, 34));
        FillRectColor(dc, RECT{headerRect.left + 1, headerRect.top + 1, headerRect.right - 1, headerRect.top + 5}, RGB(214, 150, 56));
        DrawTextLine(dc,
                     RECT{headerRect.left + 14, headerRect.top + 8, headerRect.right - 14, headerRect.top + 28},
                     "UI / VN stage",
                     RGB(246, 242, 228),
                     headerFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc,
                     RECT{headerRect.left + 14, headerRect.top + 28, headerRect.right - 14, headerRect.bottom - 8},
                     model.sourceLine + "  |  " + model.previewMetaLine,
                     RGB(212, 218, 226),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        DrawInsetFrame(dc, shelfRect, RGB(34, 38, 46), RGB(108, 116, 130), RGB(18, 20, 26));
        FillRectColor(dc, RECT{shelfRect.left + 1, shelfRect.top + 1, shelfRect.right - 1, shelfRect.top + 4}, RGB(124, 150, 198));
        DrawTextLine(dc,
                     RECT{shelfRect.left + 12, shelfRect.top + 10, shelfRect.right - 12, shelfRect.top + 30},
                     "Screen rail",
                     RGB(232, 236, 242),
                     bodyFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawTextLine(dc,
                     RECT{shelfRect.left + 12, shelfRect.top + 28, shelfRect.right - 12, shelfRect.top + 46},
                     "Click a screen in the inspector. Click a block here or on stage.",
                     RGB(190, 198, 208),
                     smallFont_,
                     DT_LEFT | DT_WORDBREAK);

        int shelfTop = shelfRect.top + 58;
        for (const UiWorkbenchScreenSummary& screen : model.screens) {
            RECT rowRect{shelfRect.left + 10, shelfTop, shelfRect.right - 10, shelfTop + 28};
            if (rowRect.bottom > shelfRect.bottom - 12) {
                break;
            }
            FillRectColor(dc, rowRect, screen.selected ? RGB(118, 84, 42) : RGB(46, 50, 58));
            DrawInsetFrame(dc, rowRect, RGB(24, 26, 32), screen.selected ? RGB(236, 206, 142) : RGB(100, 108, 120), RGB(18, 20, 24));
            DrawTextLine(dc,
                         RECT{rowRect.left + 8, rowRect.top, rowRect.right - 8, rowRect.bottom},
                         screen.titleLine,
                         screen.selected ? RGB(255, 246, 218) : RGB(222, 228, 236),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            shelfTop += 34;
        }

        DrawInsetFrame(dc, stageCard, RGB(24, 28, 34), RGB(132, 140, 154), RGB(16, 18, 22));
        FillRectColor(dc, RECT{stageCard.left + 1, stageCard.top + 1, stageCard.right - 1, stageCard.top + 5}, RGB(204, 145, 60));
        DrawTextLine(dc,
                     RECT{stageCard.left + 16, stageCard.top + 10, stageCard.right - 16, stageCard.top + 28},
                     model.previewTitleLine.empty() ? std::string("No screen selected") : model.previewTitleLine,
                     RGB(248, 242, 220),
                     headerFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        DrawTextLine(dc,
                     RECT{stageCard.left + 16, stageCard.top + 30, stageCard.right - 16, stageCard.top + 48},
                     model.blockActionsHeaderLine,
                     RGB(194, 200, 210),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT stageRect{};
        const std::vector<RECT> blockRects = ComputeUiWorkbenchViewportBlockRects(viewportInner, model, stageRect);
        DrawInsetFrame(dc, stageRect, RGB(14, 16, 22), RGB(104, 112, 124), RGB(10, 12, 18));
        FillRectColor(dc, RECT{stageRect.left + 1, stageRect.top + 1, stageRect.right - 1, stageRect.bottom - 1}, RGB(18, 22, 30));
        FillRectColor(dc, RECT{stageRect.left + 1, stageRect.top + 1, stageRect.right - 1, stageRect.top + 6}, RGB(214, 150, 56));

        for (std::size_t i = 0; i < blockRects.size(); ++i) {
            const UiWorkbenchPreviewBlock& block = model.previewBlocks[i];
            RECT blockRect = blockRects[i];
            COLORREF fill = block.selected ? RGB(106, 78, 40) : RGB(38, 42, 52);
            COLORREF border = block.selected ? RGB(236, 206, 142) : RGB(112, 120, 134);
            COLORREF title = block.selected ? RGB(255, 246, 220) : RGB(232, 236, 242);
            COLORREF detail = block.selected ? RGB(255, 230, 180) : RGB(188, 196, 206);
            switch (block.tone) {
                case UiWorkbenchBlockTone::Heading:
                    if (!block.selected) {
                        fill = RGB(62, 48, 32);
                        border = RGB(198, 152, 82);
                    }
                    break;
                case UiWorkbenchBlockTone::Say:
                    if (!block.selected) {
                        fill = RGB(34, 46, 68);
                        border = RGB(110, 156, 214);
                    }
                    break;
                case UiWorkbenchBlockTone::Narration:
                    if (!block.selected) {
                        fill = RGB(48, 42, 60);
                        border = RGB(162, 136, 210);
                    }
                    break;
                case UiWorkbenchBlockTone::Choices:
                    if (!block.selected) {
                        fill = RGB(40, 56, 42);
                        border = RGB(118, 176, 128);
                    }
                    break;
                case UiWorkbenchBlockTone::Image:
                    if (!block.selected) {
                        fill = RGB(48, 48, 38);
                        border = RGB(178, 168, 116);
                    }
                    break;
                case UiWorkbenchBlockTone::Note:
                case UiWorkbenchBlockTone::Other:
                    break;
            }
            FillRectColor(dc, blockRect, fill);
            DrawInsetFrame(dc, blockRect, RGB(18, 20, 24), border, RGB(14, 16, 20));
            DrawTextLine(dc,
                         RECT{blockRect.left + 12, blockRect.top + 8, blockRect.right - 12, blockRect.top + 26},
                         std::to_string(static_cast<int>(i) + 1) + ". " + block.titleLine,
                         title,
                         bodyFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            if (!block.detailLine.empty()) {
                DrawTextLine(dc,
                             RECT{blockRect.left + 12, blockRect.top + 26, blockRect.right - 12, blockRect.bottom - 8},
                             block.detailLine,
                             detail,
                             smallFont_,
                             DT_LEFT | DT_WORDBREAK);
            }
        }

        if (blockRects.empty()) {
            DrawTextLine(dc,
                         RECT{stageRect.left + 16, stageRect.top + 28, stageRect.right - 16, stageRect.bottom - 16},
                         "No blocks on this screen yet. Use Add Dialogue, Add Narration, or Add Choices in the inspector.",
                         RGB(214, 218, 224),
                         bodyFont_,
                         DT_CENTER | DT_WORDBREAK | DT_VCENTER);
        }
    }

    void Paint() {
        PAINTSTRUCT paint{};
        HDC windowDc = BeginPaint(hwnd_, &paint);
        RECT client{};
        GetClientRect(hwnd_, &client);
        const int width = std::max(1L, client.right - client.left);
        const int height = std::max(1L, client.bottom - client.top);

        HDC dc = CreateCompatibleDC(windowDc);
        HBITMAP backBuffer = CreateCompatibleBitmap(windowDc, width, height);
        HGDIOBJ oldBitmap = SelectObject(dc, backBuffer);

        const COLORREF kWindowBg = RGB(52, 56, 62);
        const COLORREF kInsetFill = RGB(44, 50, 58);
        const COLORREF kViewportFill = RGB(38, 44, 52);
        FillRectColor(dc, client, kWindowBg);

        RECT topBar{0, 0, client.right, 56};
        const ri::editor::EditorViewportTheme viewportTheme{
            .titleFont = titleFont_,
            .headerFont = headerFont_,
            .bodyFont = bodyFont_,
            .smallFont = smallFont_,
        };
        ri::editor::RenderEditorTopChrome(
            dc,
            client,
            topBar,
            ri::editor::EditorViewportChromeModel{
                .title = "RawIron Editor",
                .subtitle = "Hammer discipline, Unity flow, and a tighter native workspace.",
                .focusedWorkspaceGameLabel = FocusedWorkspaceGameLabel(),
                .workspaceLabel = sceneConfig_.workspaceLabel,
                .resourcesModeActive = leftPanelMode_ == LeftPanelMode::Resources,
            },
            viewportTheme);

        const EditorLayout layout = ComputeLayout();
        RECT toolStrip = layout.toolStrip;
        ri::editor::RenderEditorToolStrip(
            dc,
            toolStrip,
            ri::editor::EditorViewportToolStripModel{
                .editModeLabel = EditModeLabel(),
                .axisLabel = AxisLabel(),
                .gridSnapEnabled = gridSnapEnabled_,
                .editStepLabel = EditStepLabel(),
                .gridSnapLabel = GridSnapLabel(),
                .undoDepth = undoStack_.size(),
                .authoredCount = CountAuthoredNodes(),
                .triggerCount = CountTriggerNodes(),
            },
            viewportTheme);

        RECT hierarchy = layout.hierarchy;
        RECT viewport = layout.viewport;
        RECT inspector = layout.inspector;
        RECT statusBar{10, client.bottom - 82, client.right - 10, client.bottom - 10};
        ri::editor::RenderEditorFramePanels(
            dc,
            hierarchy,
            viewport,
            inspector,
            layout.hierarchySplitter,
            layout.inspectorSplitter,
            statusBar);

        const std::string leftPanelMeta =
            leftPanelMode_ == LeftPanelMode::Scene
                ? (std::to_string(starterScene_.scene.NodeCount()) + " nodes")
                : (std::to_string(workspaceGames_.size()) + " games · " +
                   std::to_string(filteredResourceRows_.size()) + "/" +
                   std::to_string(resourceCatalogEntries_.size()) + " files");
        DrawPanelHeader(dc,
                        hierarchy,
                        leftPanelMode_ == LeftPanelMode::Scene ? "Hierarchy" : "Project Archive",
                        leftPanelMode_ == LeftPanelMode::Scene ? SelectedNodeSummary() : FocusedWorkspaceGameLabel());
        DrawPanelHeader(dc,
                        viewport,
                        "Viewport",
                        sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->id : "starter");
        DrawPanelHeader(dc, inspector, "Inspector", InspectorPanelLabel() + "  |  " + leftPanelMeta);

        RECT hierarchyInner = layout.hierarchyInner;
        RECT viewportInner = layout.viewportInner;
        RECT inspectorInner = layout.inspectorInner;
        DrawInsetFrame(dc, hierarchyInner, kInsetFill, RGB(154, 160, 170), RGB(26, 29, 35));
        DrawInsetFrame(dc, viewportInner, kViewportFill, RGB(154, 160, 170), RGB(24, 26, 30));
        DrawInsetFrame(dc, inspectorInner, kInsetFill, RGB(154, 160, 170), RGB(26, 29, 35));

        const auto& nodes = starterScene_.scene.Nodes();
        const std::vector<int> hierarchyOrder = HierarchyDrawOrder();

        DrawToolbarButton(dc,
                         RECT{hierarchyInner.left + 6,
                              hierarchyInner.top + 4,
                              hierarchyInner.left + 78,
                              hierarchyInner.top + 4 + kLeftPanelTabHeight_},
                         "Scene",
                         leftPanelMode_ == LeftPanelMode::Scene);
        DrawToolbarButton(dc,
                         RECT{hierarchyInner.left + 82,
                              hierarchyInner.top + 4,
                              hierarchyInner.left + 190,
                              hierarchyInner.top + 4 + kLeftPanelTabHeight_},
                         "Resources",
                         leftPanelMode_ == LeftPanelMode::Resources);

        const int tabBottom = hierarchyInner.top + 4 + kLeftPanelTabHeight_;
        if (leftPanelMode_ == LeftPanelMode::Resources && !workspaceGames_.empty()) {
            const WorkspaceGameEntry& focus =
                workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)];
            DrawToolbarButton(dc,
                             RECT{hierarchyInner.left + 6,
                                  tabBottom + 4,
                                  hierarchyInner.left + 34,
                                  tabBottom + 4 + kLeftPanelGameStripHeight_},
                             "<",
                             false);
            DrawToolbarButton(dc,
                             RECT{hierarchyInner.right - 34,
                                  tabBottom + 4,
                                  hierarchyInner.right - 6,
                                  tabBottom + 4 + kLeftPanelGameStripHeight_},
                             ">",
                             false);
        DrawTextLine(dc,
                     RECT{hierarchyInner.left + 40,
                          tabBottom + 6,
                          hierarchyInner.right - 40,
                          tabBottom + kLeftPanelGameStripHeight_},
                     focus.displayName,
                     RGB(255, 221, 154),
                     bodyFont_,
                     DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        if (leftPanelMode_ == LeftPanelMode::Resources) {
            const int filterTop = tabBottom + 4 + kLeftPanelGameStripHeight_ + 4;
            int fx = hierarchyInner.left + 6;
            const std::array<WorkspaceResourceCategory, 8> categories = {
                WorkspaceResourceCategory::Manifest,
                WorkspaceResourceCategory::Level,
                WorkspaceResourceCategory::Script,
                WorkspaceResourceCategory::Test,
                WorkspaceResourceCategory::UiScreen,
                WorkspaceResourceCategory::Menu,
                WorkspaceResourceCategory::Asset,
                WorkspaceResourceCategory::Other,
            };
            for (const WorkspaceResourceCategory category : categories) {
                const int width = 56;
                const RECT chip{fx, filterTop, fx + width, filterTop + kResourceFilterStripHeight_};
                fx += width + 4;
                const bool active = (resourceCategoryMask_ & WorkspaceCategoryBit(category)) != 0u;
                DrawToolbarButton(dc, chip, WorkspaceCategoryShortLabel(category), active);
            }
            const RECT searchRect = ResourceSearchBoxRect(hierarchyInner);
            const RECT clearRect = ResourceSearchClearRect(hierarchyInner);
            DrawInsetFrame(dc,
                           searchRect,
                           resourceSearchActive_ ? RGB(36, 54, 88) : RGB(62, 68, 78),
                           resourceSearchActive_ ? RGB(220, 230, 252) : RGB(162, 168, 178),
                           RGB(22, 24, 30));
            const std::string shownFilter = resourceSearchQuery_.empty()
                ? std::string("Find project files... (Ctrl+F)")
                : resourceSearchQuery_;
            DrawTextLine(dc,
                         RECT{searchRect.left + 8, searchRect.top + 2, searchRect.right - 8, searchRect.bottom - 2},
                         shownFilter,
                         resourceSearchQuery_.empty() ? RGB(188, 196, 208) : RGB(244, 248, 255),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            DrawToolbarButton(dc, clearRect, "x", !resourceSearchQuery_.empty());
        } else {
            const RECT searchRect = SceneSearchBoxRect(hierarchyInner);
            const RECT clearRect = SceneSearchClearRect(hierarchyInner);
            DrawInsetFrame(dc,
                           searchRect,
                           resourceSearchActive_ ? RGB(36, 54, 88) : RGB(62, 68, 78),
                           resourceSearchActive_ ? RGB(220, 230, 252) : RGB(162, 168, 178),
                           RGB(22, 24, 30));
            const std::string shownFilter = resourceSearchQuery_.empty()
                ? std::string("Filter hierarchy... (Ctrl+F)")
                : resourceSearchQuery_;
            DrawTextLine(dc,
                         RECT{searchRect.left + 8, searchRect.top + 2, searchRect.right - 8, searchRect.bottom - 2},
                         shownFilter,
                         resourceSearchQuery_.empty() ? RGB(188, 196, 208) : RGB(244, 248, 255),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            DrawToolbarButton(dc, clearRect, "x", !resourceSearchQuery_.empty());
        }

        const int listTop = LeftPanelContentTop(hierarchyInner);
        const int listBottom = LeftPanelSceneListBottom(hierarchyInner);
        const int listPixels = std::max(0, listBottom - listTop - 8);

        if (leftPanelMode_ == LeftPanelMode::Scene) {
            const int visibleHierarchyRows = std::max(0, listPixels / kHierarchyRowHeight_);
            const int maxHierarchyScroll =
                std::max(0, static_cast<int>(hierarchyOrder.size()) - visibleHierarchyRows);
            hierarchyScrollTopRow_ = std::clamp(hierarchyScrollTopRow_, 0, maxHierarchyScroll);

            if (!hierarchyOrder.empty() && visibleHierarchyRows > 0 && maxHierarchyScroll > 0) {
                const std::string scrollHint =
                    "Rows " + std::to_string(hierarchyScrollTopRow_ + 1) + "-" +
                    std::to_string(std::min(static_cast<int>(hierarchyOrder.size()),
                                            hierarchyScrollTopRow_ + visibleHierarchyRows)) +
                    " of " + std::to_string(hierarchyOrder.size()) + "  |  wheel / PgUp PgDn";
                DrawTextLine(dc,
                             RECT{hierarchyInner.left + 6,
                                  hierarchyInner.bottom - 22,
                                  hierarchyInner.right - 6,
                                  hierarchyInner.bottom - 4},
                             scrollHint,
                             RGB(36, 38, 42),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }

            int y = listTop;
            for (int row = 0; row < visibleHierarchyRows; ++row) {
                const int orderIndex = hierarchyScrollTopRow_ + row;
                if (orderIndex >= static_cast<int>(hierarchyOrder.size())) {
                    break;
                }
                const int nodeIndex = hierarchyOrder[static_cast<std::size_t>(orderIndex)];
                if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= nodes.size()) {
                    continue;
                }
                const ri::scene::Node& node = nodes[static_cast<std::size_t>(nodeIndex)];
                const int depth = ComputeNodeDepth(starterScene_.scene, nodeIndex);
                const int indent = 8 + depth * 14;
                RECT rowRect{hierarchyInner.left + 6, y, hierarchyInner.right - 6, y + 24};
                if (static_cast<std::size_t>(nodeIndex) == selectedNode_) {
                    FillRectColor(dc, rowRect, RGB(124, 89, 40));
                }
                DrawTextLine(dc,
                             RECT{rowRect.left + indent, rowRect.top, rowRect.right - 90, rowRect.bottom},
                             std::to_string(nodeIndex) + "  " + node.name,
                             static_cast<std::size_t>(nodeIndex) == selectedNode_ ? RGB(255, 255, 160)
                                                                                : RGB(236, 240, 244),
                             bodyFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                DrawTextLine(dc,
                             RECT{rowRect.right - 84, rowRect.top, rowRect.right - 8, rowRect.bottom},
                             NodeKindLabel(node),
                             static_cast<std::size_t>(nodeIndex) == selectedNode_ ? RGB(255, 255, 200)
                                                                                  : RGB(186, 194, 204),
                             smallFont_,
                             DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
                y += kHierarchyRowHeight_;
            }
        } else {
            const int visibleResourceRows = std::max(0, listPixels / kResourceListRowHeight_);
            const int maxResourceScroll =
                std::max(0, static_cast<int>(filteredResourceRows_.size()) - visibleResourceRows);
            resourceCatalogScrollTopRow_ =
                std::clamp(resourceCatalogScrollTopRow_, 0, maxResourceScroll);

            if (!filteredResourceRows_.empty() && visibleResourceRows > 0 && maxResourceScroll > 0) {
                const std::string scrollHint =
                    "Rows " + std::to_string(resourceCatalogScrollTopRow_ + 1) + "-" +
                    std::to_string(std::min(static_cast<int>(filteredResourceRows_.size()),
                                            resourceCatalogScrollTopRow_ + visibleResourceRows)) +
                    " of " + std::to_string(filteredResourceRows_.size()) + " (filtered)";
                DrawTextLine(dc,
                             RECT{hierarchyInner.left + 6,
                                  hierarchyInner.bottom - 22,
                                  hierarchyInner.right - 6,
                                  hierarchyInner.bottom - 4},
                             scrollHint,
                             RGB(36, 38, 42),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }

            if (workspaceGames_.empty()) {
                DrawTextLine(dc,
                             RECT{hierarchyInner.left + 8,
                                  listTop,
                                  hierarchyInner.right - 8,
                                  listTop + 48},
                             "No workspace game is mounted. Launch with --game=<id> or open a registered project.",
                             RGB(180, 90, 90),
                             smallFont_,
                             DT_LEFT | DT_WORDBREAK);
            } else {
                int ry = listTop;
                for (int row = 0; row < visibleResourceRows; ++row) {
                    const int visibleIdx = resourceCatalogScrollTopRow_ + row;
                    if (visibleIdx >= static_cast<int>(filteredResourceRows_.size())) {
                        break;
                    }
                    const int idx = filteredResourceRows_[static_cast<std::size_t>(visibleIdx)];
                    const WorkspaceResourceEntry& entry =
                        resourceCatalogEntries_[static_cast<std::size_t>(idx)];
                    RECT rowRect{hierarchyInner.left + 6,
                                 ry,
                                 hierarchyInner.right - 6,
                                 ry + kResourceListRowHeight_};
                    if (visibleIdx == selectedResourceVisibleRow_) {
                        FillRectColor(dc, rowRect, RGB(64, 84, 118));
                    }
                    DrawTextLine(dc,
                                 RECT{rowRect.left + 6, rowRect.top, rowRect.right - 110, rowRect.bottom},
                                 entry.relativePathUtf8,
                                 visibleIdx == selectedResourceVisibleRow_ ? RGB(255, 247, 220) : RGB(236, 240, 244),
                                 smallFont_,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    DrawTextLine(dc,
                                 RECT{rowRect.right - 102, rowRect.top, rowRect.right - 8, rowRect.bottom},
                                 WorkspaceCategoryLabel(entry.category),
                                 visibleIdx == selectedResourceVisibleRow_ ? RGB(214, 228, 248) : RGB(186, 194, 204),
                                 smallFont_,
                                 DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    ry += kResourceListRowHeight_;
                }
            }
        }

        const RECT inspectorTabShelf{inspectorInner.left + 6, inspectorInner.top + 6, inspectorInner.right - 6, inspectorInner.top + 36};
        DrawInsetFrame(dc, inspectorTabShelf, RGB(52, 58, 68), RGB(156, 162, 170), RGB(22, 24, 30));
        FillRectColor(dc,
                      RECT{inspectorTabShelf.left + 1, inspectorTabShelf.top + 1, inspectorTabShelf.right - 1, inspectorTabShelf.top + 5},
                      RGB(204, 145, 60));
        DrawToolbarButton(dc, RECT{inspectorInner.left + 12, inspectorInner.top + 10, inspectorInner.left + 78, inspectorInner.top + 34},
                          "Node", inspectorPanel_ == InspectorPanel::Node);
        DrawToolbarButton(dc, RECT{inspectorInner.left + 84, inspectorInner.top + 10, inspectorInner.left + 154, inspectorInner.top + 34},
                          "Brush", inspectorPanel_ == InspectorPanel::Brush);
        DrawToolbarButton(dc, RECT{inspectorInner.left + 160, inspectorInner.top + 10, inspectorInner.left + 244, inspectorInner.top + 34},
                          "Gameplay", inspectorPanel_ == InspectorPanel::Gameplay);
        DrawToolbarButton(dc, RECT{inspectorInner.left + 250, inspectorInner.top + 10, inspectorInner.left + 320, inspectorInner.top + 34},
                          "Files", inspectorPanel_ == InspectorPanel::Files);
        DrawToolbarButton(dc, RECT{inspectorInner.left + 326, inspectorInner.top + 10, inspectorInner.left + 404, inspectorInner.top + 34},
                          "UI / VN", inspectorPanel_ == InspectorPanel::UiWorkbench);

        int infoTop = inspectorInner.top + 48;
        if (inspectorPanel_ == InspectorPanel::Files) {
            const WorkspaceResourceEntry* selectedResourceEntry = nullptr;
            if (selectedResourceRow_ >= 0
                && selectedResourceRow_ < static_cast<int>(resourceCatalogEntries_.size())) {
                selectedResourceEntry = &resourceCatalogEntries_[static_cast<std::size_t>(selectedResourceRow_)];
            }
            const ri::editor::FilesInspectorPanelModel filesPanel = BuildFilesInspectorPanelModel(
                selectedResourceEntry,
                resourceManifestIssues_,
                resourceEditorAuxMessage_,
                resourceFileDirty_,
                ResourceFocusSummary());
            DrawTextLine(dc, RECT{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 22},
                         filesPanel.heading,
                         RGB(224, 224, 236),
                         headerFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            infoTop += 26;
            const ProjectShortcutLayout shortcuts = ComputeProjectShortcutLayout(inspectorInner);
            const RECT shortcutCard{inspectorInner.left + 10, infoTop + 2, inspectorInner.right - 10, shortcuts.plugins.bottom + 12};
            DrawInsetFrame(dc, shortcutCard, RGB(54, 60, 70), RGB(160, 166, 174), RGB(22, 24, 30));
            FillRectColor(dc,
                          RECT{shortcutCard.left + 1, shortcutCard.top + 1, shortcutCard.right - 1, shortcutCard.top + 5},
                          RGB(124, 150, 198));
            DrawTextLine(dc, RECT{inspectorInner.left + 20, infoTop + 10, inspectorInner.right - 20, infoTop + 28},
                         filesPanel.sectionLabel,
                         RGB(210, 214, 220),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            DrawToolbarButton(dc, shortcuts.manifest, "Manifest", false);
            DrawToolbarButton(dc, shortcuts.level, "Level", false);
            DrawToolbarButton(dc, shortcuts.gameplay, "Gameplay", false);
            DrawToolbarButton(dc, shortcuts.rendering, "Render", false);
            DrawToolbarButton(dc, shortcuts.uiLayout, "UI Flow", false);
            DrawToolbarButton(dc, shortcuts.uiStyle, "VN Flow", false);
            DrawToolbarButton(dc, shortcuts.menu, "Menu", false);
            DrawToolbarButton(dc, shortcuts.ai, "AI", false);
            DrawToolbarButton(dc, shortcuts.network, "Network", false);
            DrawToolbarButton(dc, shortcuts.plugins, "Plugins", false);
            infoTop = shortcuts.plugins.bottom + 18;
            if (filesPanel.hasSelection) {
                const RECT fileCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 112};
                DrawInsetFrame(dc, fileCard, RGB(54, 58, 68), RGB(160, 166, 176), RGB(20, 24, 30));
                FillRectColor(dc,
                              RECT{fileCard.left + 1, fileCard.top + 1, fileCard.right - 1, fileCard.top + 5},
                              RGB(106, 154, 122));
                DrawTextLine(dc, RECT{fileCard.left + 10, fileCard.top + 10, fileCard.right - 10, fileCard.top + 28},
                             filesPanel.selectedPath,
                             RGB(228, 236, 248),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                DrawTextLine(dc, RECT{fileCard.left + 10, fileCard.top + 34, fileCard.right - 10, fileCard.top + 52},
                             filesPanel.categoryLabel,
                             RGB(208, 212, 220),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                infoTop = fileCard.top + 58;
                if (!filesPanel.manifestStatus.empty()) {
                    DrawTextLine(dc,
                                 RECT{fileCard.left + 10, infoTop, fileCard.right - 10, infoTop + 18},
                                 filesPanel.manifestStatus,
                                 filesPanel.manifestOk ? RGB(160, 220, 170) : RGB(255, 180, 120),
                                 smallFont_,
                                 DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                    infoTop += 20;
                    for (const std::string& issue : filesPanel.manifestIssues) {
                        DrawTextLine(dc,
                                     RECT{fileCard.left + 14, infoTop, fileCard.right - 10, infoTop + 32},
                                     issue,
                                     RGB(230, 190, 150),
                                     smallFont_,
                                     DT_LEFT | DT_WORDBREAK);
                        infoTop += 34;
                    }
                    if (filesPanel.manifestOk) {
                        infoTop += 0;
                    }
                }
                infoTop = fileCard.bottom + 10;
            } else {
                const RECT emptyCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 68};
                DrawInsetFrame(dc, emptyCard, RGB(54, 58, 68), RGB(156, 162, 170), RGB(22, 24, 30));
                FillRectColor(dc,
                              RECT{emptyCard.left + 1, emptyCard.top + 1, emptyCard.right - 1, emptyCard.top + 5},
                              RGB(108, 116, 132));
                DrawTextLine(dc, RECT{emptyCard.left + 10, emptyCard.top + 12, emptyCard.right - 10, emptyCard.bottom - 10},
                             filesPanel.emptySelectionMessage,
                             RGB(180, 180, 190),
                             smallFont_,
                             DT_LEFT | DT_WORDBREAK);
                infoTop = emptyCard.bottom + 10;
            }
            if (!filesPanel.auxMessage.empty()) {
                const RECT auxCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 56};
                DrawInsetFrame(dc, auxCard, RGB(68, 60, 54), RGB(184, 162, 138), RGB(26, 22, 20));
                FillRectColor(dc,
                              RECT{auxCard.left + 1, auxCard.top + 1, auxCard.right - 1, auxCard.top + 5},
                              RGB(204, 145, 60));
                DrawTextLine(dc, RECT{auxCard.left + 10, auxCard.top + 10, auxCard.right - 10, auxCard.bottom - 10},
                             filesPanel.auxMessage,
                             RGB(220, 160, 120),
                             smallFont_,
                             DT_LEFT | DT_WORDBREAK);
                infoTop = auxCard.bottom + 10;
            }
            DrawToolbarButton(dc,
                             RECT{inspectorInner.left + 10, inspectorInner.top + 74, inspectorInner.left + 118, inspectorInner.top + 100},
                             filesPanel.saveLabel,
                             false);
            DrawToolbarButton(dc,
                             RECT{inspectorInner.left + 124, inspectorInner.top + 74, inspectorInner.right - 10, inspectorInner.top + 100},
                             "Explorer",
                             false);
            DrawTextLine(dc,
                         RECT{inspectorInner.left + 10, inspectorInner.top + 106, inspectorInner.right - 10, inspectorInner.top + 122},
                         filesPanel.footerHint,
                         RGB(200, 196, 160),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else if (inspectorPanel_ == InspectorPanel::UiWorkbench) {
            uiWorkbenchLayout_ = ComputeUiWorkbenchLayout(inspectorInner);
            UiWorkbenchPanelModel model = BuildUiWorkbenchPanelModel(inspectorInner);
            RenderUiWorkbenchPanel(
                dc,
                inspectorInner,
                model,
                headerFont_,
                bodyFont_,
                smallFont_,
                [this](HDC innerDc, const RECT& rect, const std::string& label, bool active) {
                    this->DrawToolbarButton(innerDc, rect, label, active);
                });
        } else if (!nodes.empty() && selectedNode_ < nodes.size()) {
            const ri::scene::Node& node = nodes[selectedNode_];
            const ri::math::Vec3 worldPos = starterScene_.scene.ComputeWorldPosition(static_cast<int>(selectedNode_));
            infoTop = inspectorInner.top + 42;
            if (inspectorPanel_ == InspectorPanel::Node) {
                inspectorNudgeButtons_.fill(RECT{});
                NodeInspectorPanelModel model{};
                model.renameTypingActive = nodeRenameTypingActive_;
                model.renameDraft = nodeRenameDraft_;
                model.nameLine = "Selection: " + node.name + "  |  F2 rename";
                model.pathLine = "Node path: " + ri::scene::DescribeNodePath(starterScene_.scene, static_cast<int>(selectedNode_));
                model.kindLine = "Type: " + NodeKindLabel(node);
                if (node.mesh != ri::scene::kInvalidHandle && node.mesh >= 0 &&
                    static_cast<std::size_t>(node.mesh) < starterScene_.scene.MeshCount()) {
                    const ri::scene::Mesh& mesh = starterScene_.scene.GetMesh(node.mesh);
                    model.meshPrimitiveLine = "Mesh: " + ri::scene::ToString(mesh.primitive);
                }
                model.localPosLine = "Local position: " + ri::math::ToString(node.localTransform.position);
                model.localRotLine = "Local rotation: " + ri::math::ToString(node.localTransform.rotationDegrees);
                model.localScaleLine = "Local scale: " + ri::math::ToString(node.localTransform.scale);
                model.editableAuthored = IsEditableAuthoredNode(static_cast<int>(selectedNode_));
                model.editModeLine = "Transform mode: " + EditModeLabel() + "  |  Axis " + AxisLabel();
                model.groupingLine = "Grouping: Ctrl+G group  |  Ctrl+Shift+G ungroup  |  Ctrl+Shift+N new group";
                model.opsLine = "Actions: Ctrl+R reset  |  Shift+F frame all  |  Ctrl+Shift+W parent to World";
                model.worldPosLine = "World position: " + ri::math::ToString(worldPos);
                RenderNodeInspectorPanel(
                    dc,
                    inspectorInner,
                    model,
                    headerFont_,
                    bodyFont_,
                    smallFont_,
                    [this](HDC innerDc, int& top, const RECT& innerRect, const char* label, int componentIndex) {
                        this->DrawInspectorNudgeRow(innerDc, top, innerRect, label, componentIndex);
                    });
            } else if (inspectorPanel_ == InspectorPanel::Brush) {
                const auto bounds =
                    ri::scene::ComputeNodeWorldBounds(starterScene_.scene, static_cast<int>(selectedNode_), false);
                BrushInspectorPanelModel model{};
                model.presetTitleLine =
                    std::string(CurrentStructuralPrimitivePreset().label)
                    + " (" + std::string(CurrentStructuralPrimitivePreset().structuralType) + ")";
                model.headingLine = "Brush / primitive authoring";
                model.helpLineA =
                    "[ / ] or click < > to cycle presets · Ctrl+Shift+1..9 quick preset · Ctrl+Shift+B spawn";
                model.helpLineB =
                    "Ctrl+Shift+T adds a trigger. Del removes authored mesh. Ctrl+D duplicates. Ctrl+G groups. CSV export emits cube/plane assembly rows only.";
                model.selectionLine = "Selection: " + node.name + "  |  " + NodeKindLabel(node);
                model.meshAttachedLine =
                    std::string("Mesh attached: ") + (node.mesh != ri::scene::kInvalidHandle ? "yes" : "no");
                model.boundsSizeLine = bounds.has_value()
                    ? "Bounds size: " + ri::math::ToString(ri::scene::GetBoundsSize(*bounds))
                    : "Bounds size: n/a";
                model.boundsCenterLine = bounds.has_value()
                    ? "Bounds center: " + ri::math::ToString(ri::scene::GetBoundsCenter(*bounds))
                    : "Bounds center: n/a";
                RenderBrushInspectorPanel(
                    dc,
                    inspectorInner,
                    model,
                    headerFont_,
                    bodyFont_,
                    smallFont_,
                    [this](HDC innerDc, const RECT& rect, const std::string& label, bool active) {
                        this->DrawToolbarButton(innerDc, rect, label, active);
                    });
            } else {
                gameplayPanelLayout_ = ComputeGameplayPanelLayout(inspectorInner);
                GameplayInspectorPanelModel model{};
                model.layout = gameplayPanelLayout_;
                model.headingLine = "Gameplay sandbox policy";
                model.summaryLine = TriggerSelectionSummary();
                model.inventoryModeLine = "Inventory mode: " + InventoryPresentationLabel() + "  (click to cycle)";
                model.offHandLine =
                    std::string("Off-hand slot: ")
                    + (creatorInventoryPolicy_.allowOffHand ? "enabled" : "disabled")
                    + "  (click to toggle)";
                model.gameplayStorageLine =
                    std::string("Gameplay storage: ")
                    + (creatorInventoryPolicy_.presentation == ri::world::InventoryPresentationMode::Disabled ? "off" : "on");
                model.hotbarLine =
                    std::string("Hotbar / backpack: ") + std::to_string(creatorInventoryPolicy_.hotbarSize) + " / "
                    + std::to_string(creatorInventoryPolicy_.backpackSize);
                model.controlsLine =
                    "Controls: 1/2/3/4 panels · I inventory · O off-hand · Ctrl+Shift+T trigger · Ctrl+Shift+I import primary level CSV · F5 reload preview · Ctrl+Shift+M scaffold project.";
                RenderGameplayInspectorPanel(
                    dc,
                    inspectorInner,
                    model,
                    headerFont_,
                    bodyFont_,
                    smallFont_,
                    [this](HDC innerDc, const RECT& rect, const std::string& label, bool active) {
                        this->DrawToolbarButton(innerDc, rect, label, active);
                    });
            }
        } else {
            infoTop = inspectorInner.top + 42;
            const RECT welcomeCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 62};
            DrawInsetFrame(dc, welcomeCard, RGB(58, 64, 74), RGB(170, 176, 184), RGB(22, 26, 32));
            DrawTextLine(dc, RECT{welcomeCard.left + 10, welcomeCard.top + 10, welcomeCard.right - 10, welcomeCard.bottom - 10},
                         "Choose a scene node to edit transforms and authored data, or switch to Project Archive / Files to work with game resources.",
                         RGB(200, 200, 200),
                         smallFont_,
                         DT_LEFT | DT_WORDBREAK);
        }

        {
            const int sessionTop = inspectorInner.bottom - 64;
            const RECT sessionCard{inspectorInner.left + 10, sessionTop - 8, inspectorInner.right - 10, inspectorInner.bottom - 8};
            DrawInsetFrame(dc, sessionCard, RGB(56, 61, 70), RGB(160, 166, 174), RGB(20, 24, 30));
            FillRectColor(dc,
                          RECT{sessionCard.left + 1, sessionCard.top + 1, sessionCard.right - 1, sessionCard.top + 5},
                          RGB(204, 145, 60));
            DrawTextLine(dc, RECT{inspectorInner.left + 18, sessionTop, inspectorInner.right - 18, sessionTop + 18},
                         "Session & Undo", RGB(240, 240, 236), headerFont_, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            DrawTextLine(dc,
                         RECT{inspectorInner.left + 18,
                              sessionTop + 22,
                              inspectorInner.right - 18,
                              sessionTop + 40},
                         "Undo: " + std::to_string(undoStack_.size()) + "  |  Redo: " +
                             std::to_string(redoStack_.size()) + "  |  Mounted Game: " + FocusedWorkspaceGameLabel(),
                         RGB(200, 200, 200),
                         smallFont_,
                         DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        if (inspectorPanel_ == InspectorPanel::UiWorkbench) {
            DrawUiWorkbenchViewport(dc, viewportInner);
        } else {
            UpdateCameraPlotRect(viewportInner);
            DrawRawIronQuadViewportBlock(dc, viewportInner);
        }

        const ri::scene::Node& camNode = starterScene_.scene.GetNode(starterScene_.handles.orbitCamera.cameraNode);
        const std::string consoleLine =
            "t=" + std::to_string(elapsedSeconds_) +
            " | camera=" + ri::math::ToString(starterScene_.scene.ComputeWorldPosition(starterScene_.handles.orbitCamera.cameraNode)) +
            " | yaw=" + std::to_string(camNode.localTransform.rotationDegrees.y) +
            " | nodeCount=" + std::to_string(starterScene_.scene.NodeCount());
        ri::editor::RenderEditorStatusBar(
            dc,
            statusBar,
            ri::editor::EditorViewportStatusModel{
                .consoleLine = consoleLine,
                .controlsLine =
                    "Esc root select · Ctrl+Shift+Q quit · Space orbit demo · Tab quad/perspective · T/R/U transform · WASDQE edit (Shift fine / Alt coarse) · G snap toggle · Ctrl+G snap selection · +/- grid step · Ctrl+R reset · Shift+F frame all · Ctrl+Shift+W to World · Ctrl+Shift+T trigger · ,/. authored cycle · Ctrl+Shift+S snapshot · Ctrl+Shift+L autosave load · Ctrl+Shift+M scaffold · Ctrl+E export · Ctrl+Z/Y · Ctrl+S persist/load · F5 reload · F6 stats",
                .stateLine = "Status: " + lastIoStatus_ + "  |  Scene state: " + ResolveSceneStatePath().string(),
            },
            ri::editor::EditorViewportTheme{
                .titleFont = titleFont_,
                .headerFont = headerFont_,
                .bodyFont = bodyFont_,
                .smallFont = smallFont_,
            });

        EnsureResourceTextEditorCreated();
        SyncResourceTextEditorContent();
        LayoutResourceTextEditorControl(inspectorInner);

        RECT excludedClientRect{};
        RECT* excludedClientRectPtr = nullptr;
#if defined(_WIN32)
        if (inspectorPanel_ == InspectorPanel::Files && resourceTextEditHwnd_ != nullptr
            && IsWindow(resourceTextEditHwnd_)) {
            RECT editWindowRect{};
            GetWindowRect(resourceTextEditHwnd_, &editWindowRect);
            POINT topLeft{editWindowRect.left, editWindowRect.top};
            POINT bottomRight{editWindowRect.right, editWindowRect.bottom};
            ScreenToClient(hwnd_, &topLeft);
            ScreenToClient(hwnd_, &bottomRight);
            excludedClientRect = RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
            excludedClientRectPtr = &excludedClientRect;
        }
#endif
        ri::editor::PresentEditorFrame(windowDc, dc, width, height, excludedClientRectPtr);
        SelectObject(dc, oldBitmap);
        DeleteObject(backBuffer);
        DeleteDC(dc);
        EndPaint(hwnd_, &paint);
    }

    HWND hwnd_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT headerFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    bool logEveryFrame_ = false;
    bool dumpScene_ = false;
    EditorSceneConfig sceneConfig_{};
    std::string lastIoStatus_ = "No scene I/O action yet.";
    static constexpr std::size_t kMaxUndoActions = 256;
    static constexpr std::chrono::seconds kAutosaveInterval_{90};
    std::vector<EditorEditAction> undoStack_;
    std::vector<EditorEditAction> redoStack_;
    EditMode editMode_ = EditMode::Translate;
    InspectorPanel inspectorPanel_ = InspectorPanel::Node;
    int activeAxis_ = 0;
    /// Preset into `ri::scene::kStructuralPrimitivePresets` (structural / brush spawn).
    std::size_t structuralBrushPresetIndex_ = 0;
    std::size_t selectedNode_ = 0;
    int hierarchyScrollTopRow_ = 0;
    double elapsedSeconds_ = 0.0;
    std::chrono::steady_clock::time_point lastTick_{};
    std::chrono::steady_clock::time_point lastAutosaveSteady_{};
    double lastAutosaveStatusSeconds_ = -999.0;
    bool autosavePending_ = false;
    ri::scene::StarterScene starterScene_{};
    ri::scene::Scene baselineStarterScene_{};
    std::size_t authoredNodeStart_ = 0;
    ri::world::InventoryPolicy creatorInventoryPolicy_{};
    bool statsOverlayVisible_ = false;
    ri::world::RuntimeStatsOverlayState statsOverlayState_{true};
    ri::scene::OrbitCameraState editorOrbitState_{};
    bool autoOrbitPreview_ = false;
    bool full3DViewport_ = false;
    bool cameraDragActive_ = false;
    bool draggingHierarchySplitter_ = false;
    bool draggingInspectorSplitter_ = false;
    int lastDragX_ = 0;
    int lastDragY_ = 0;
    RECT cameraPlotRect_{};
    int hierarchyPanelWidth_ = 304;
    int inspectorPanelWidth_ = 392;

    /// Deleted authored nodes move here (meshes stripped); subtree hidden from hierarchy list.
    int editorTrashFolderHandle_ = ri::scene::kInvalidHandle;

    LeftPanelMode leftPanelMode_ = LeftPanelMode::Scene;
    std::vector<WorkspaceGameEntry> workspaceGames_;
    int focusedWorkspaceGameIndex_ = 0;
    std::vector<WorkspaceResourceEntry> resourceCatalogEntries_;
    std::vector<int> filteredResourceRows_;
    std::vector<int> filteredHierarchyOrder_;
    GameplayPanelLayout gameplayPanelLayout_{};
    UiWorkbenchLayout uiWorkbenchLayout_{};
    std::uint32_t resourceCategoryMask_ = 0xFFu;
    std::string resourceSearchQuery_;
    bool resourceSearchActive_ = false;
    bool nodeRenameTypingActive_ = false;
    std::string nodeRenameDraft_;
    bool gridSnapEnabled_ = true;
    int gridSnapStepIndex_ = 2;
    int selectedResourceRow_ = -1;
    int selectedResourceVisibleRow_ = -1;
    int resourceCatalogScrollTopRow_ = 0;
    UiWorkbenchSource uiWorkbenchSource_ = UiWorkbenchSource::AutoSelection;
    int uiWorkbenchSelectedScreenIndex_ = 0;
    int uiWorkbenchSelectedBlockIndex_ = -1;
    bool uiWorkbenchTextEditActive_ = false;
    UiWorkbenchTextEditTarget uiWorkbenchTextEditTarget_ = UiWorkbenchTextEditTarget::None;
    std::string uiWorkbenchTextEditDraft_;
    std::string uiWorkbenchTextEditLabel_;
    std::vector<UiWorkbenchEditAction> uiWorkbenchUndoStack_;
    std::vector<UiWorkbenchEditAction> uiWorkbenchRedoStack_;

    fs::path loadedResourceAbsolutePath_;
    std::string loadedResourceUtf8_;
    std::string resourceEditorAuxMessage_;
    std::vector<std::string> resourceManifestIssues_;
    std::array<RECT, 18> inspectorNudgeButtons_{};
#if defined(_WIN32)
    HWND resourceTextEditHwnd_ = nullptr;
#endif
    bool resourceFileDirty_ = false;
};
#endif

} // namespace

int main(int argc, char** argv) {
    ri::core::InitializeCrashDiagnostics();
    try {
        ri::core::CommandLine commandLine(argc, argv);
        const bool hasFrameBudgetArg = commandLine.GetValue("--frames").has_value();

#if defined(_WIN32)
        const bool forceHeadless = commandLine.HasFlag("--headless") || commandLine.HasFlag("--cli-editor");
        const bool forceGui = commandLine.HasFlag("--editor-ui");
        if (!forceHeadless && (forceGui || !hasFrameBudgetArg)) {
            ri::runtime::RuntimeCore runtime(
                ri::runtime::RuntimeIdentity{
                    .id = "rawiron.editor",
                    .displayName = "RawIron.Editor",
                    .mode = "editor",
                    .instanceId = {},
                },
                ri::runtime::DetectRuntimePaths());
            if (!runtime.Startup(commandLine)) {
                return 1;
            }
            (void)runtime.Frame(ri::core::FrameContext{
                .frameIndex = 0,
                .deltaSeconds = 0.0,
                .elapsedSeconds = 0.0,
                .realtimeSeconds = 0.0,
                .realDeltaSeconds = 0.0,
            });
            RawIronEditorWindow window(commandLine);
            const int result = window.Run(GetModuleHandleW(nullptr));
            runtime.Shutdown();
            return result;
        }
#endif

        EditorHost host;
        ri::runtime::RuntimeCore runtime(
            ri::runtime::RuntimeIdentity{
                .id = "rawiron.editor",
                .displayName = "RawIron.Editor",
                .mode = "editor",
                .instanceId = {},
            },
            ri::runtime::DetectRuntimePaths());
        runtime.AddModule(std::make_unique<ri::runtime::RuntimeHostModule>(host));
        ri::runtime::RuntimeHostAdapter runtimeHost(runtime);

        ri::core::MainLoopOptions options;
        options.maxFrames = commandLine.GetIntOr("--frames", hasFrameBudgetArg ? 4 : 0);
        options.fixedDeltaSeconds = ResolveFixedDeltaSeconds(commandLine, 30);
        options.verboseFrames = commandLine.HasFlag("--verbose-frames");
        options.paceToFixedDelta = !commandLine.HasFlag("--unpaced");

        return ri::core::RunMainLoop(runtimeHost, commandLine, options);
    } catch (const std::exception&) {
        ri::core::LogCurrentExceptionWithStackTrace("Editor Failure");
        return 1;
    }
}
