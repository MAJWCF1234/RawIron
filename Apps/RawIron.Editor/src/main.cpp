#include "RawIron/Content/EngineAssets.h"
#include "RawIron/Content/GameManifest.h"
#include "RawIron/Content/AuthoringHandoff.h"
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
#include "EditorLevelExport.h"
#include "EditorPlaytestLauncher.h"
#include "EditorProjectHealth.h"
#include "RawIron/Editor/PreviewSceneRegistry.h"
#include "EditorHierarchy.h"
#include "EditorProjectScaffolding.h"
#include "EditorResourceBrowser.h"
#include "EditorResourceDocument.h"
#include "EditorRenderer.h"
#include "EditorResourceTextEditor.h"
#include "EditorSceneController.h"
#include "EditorViewportRenderer.h"
#include "EditorViewportPerformance.h"
#include "EditorVulkanViewport.h"
#include "EditorWorkspace.h"
#include "EditorCreatorPalette.h"
#include "EditorHelpDialog.h"
#include "EditorNewGame.h"
#include "EditorStructuralPicker.h"
#include "EditorAuthoringCatalog.h"
#include "EditorLogicLayer.h"
#include "EditorPluginManager.h"
#include "RawIron/Content/PluginProjectData.h"
#include "RawIron/Content/PluginRuntime.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/ScenePreviewPlacement.h"
#include "RawIron/Render/ScenePreviewRenderingScript.h"
#include "RawIron/Runtime/ExperiencePresets.h"
#include "RawIron/Runtime/RuntimeCore.h"
#include "RawIron/Scene/Components.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/PrimitivesCsvIO.h"
#include "RawIron/Scene/SceneStateIO.h"
#include "RawIron/Scene/SemanticStructuralPartition.h"
#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"
#include "RawIron/Scene/WorkspaceSandbox.h"
#include "RawIron/Scene/SceneUtils.h"
#include "RawIron/Scene/PhotoModeCamera.h"
#include "RawIron/Spatial/Aabb.h"
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
using ri::editor::RenderFilesInspectorPanel;
using ri::editor::ComputeVisibleHierarchyScrollTop;
using ri::editor::ComputeGameplayPanelLayout;
using ri::editor::ComputeUiWorkbenchLayout;
using ri::editor::InspectorTabLayout;
using ri::editor::UiWorkbenchInspectorLayout;
using ri::editor::UiWorkbenchViewportLayout;
using ri::editor::ComputeInspectorTabLayout;
using ri::editor::ComputePluginStoreLayout;
using ri::editor::ComputeUiWorkbenchInspectorLayout;
using ri::editor::ComputeUiWorkbenchScreenRowRects;
using ri::editor::ComputeUiWorkbenchInspectorPreviewBlockRects;
using ri::editor::ComputeUiWorkbenchViewportLayout;
using ri::editor::ComputeUiWorkbenchViewportBlockRects;
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
using ri::editor::PluginStorePanelModel;
using ri::editor::PluginStoreCardModel;
using ri::editor::PluginStoreLayout;
using ri::editor::PluginStorePackage;
using ri::editor::RenderPluginStorePanel;
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
using ri::editor::ApplyAtmospherePreset;
using ri::editor::AtmospherePresetLabel;
using ri::editor::CreateNewGameProject;
using ri::editor::CreatorAtmospherePreset;
using ri::editor::CreatorCameraPreset;
using ri::editor::CreatorInsertPreset;
using ri::editor::DefaultDisplayNameForTemplate;
using ri::editor::DispatchCreatorPanelClick;
using ri::editor::NewGameCreationResult;
using ri::editor::NewGameTemplate;
using ri::editor::NewGameTemplateLabel;
using ri::editor::RenderCreatorPanel;
using ri::editor::ComputeCreatorPanelLayout;
using ri::editor::CreateTabRect;
using ri::editor::HitTestViewportCreateMenu;
using ri::editor::HitTestViewportHelpMenu;
using ri::editor::SlugFromDisplayName;
using ri::editor::ComputeStructuralPickerLayout;
using ri::editor::ComputeStructuralPickerPanelHeight;
using ri::editor::AuthoringCatalogBottomInset;
using ri::editor::ComputeStructuralPickerCollapsedBarRect;
using ri::editor::HitTestStructuralPickerCollapsedBar;
using ri::editor::RenderStructuralPickerCollapsedBar;
using ri::editor::CycleCreatorAtmosphere;
using ri::editor::CycleCreatorCamera;
using ri::editor::CycleCreatorInsert;
using ri::editor::HitTestStructuralPicker;
using ri::editor::RenderStructuralPickerOverlay;
using ri::editor::StructuralPickerHitKind;
using ri::editor::StructuralPickerLayout;
using ri::editor::StructuralPresetDisplayLabel;
using ri::editor::StructuralThumbnailCache;
using ri::editor::TryCreateGroupNode;
using ri::editor::TryDeleteSelectedNode;
using ri::editor::TryDuplicateSelectedNode;
using ri::editor::TryGroupSelectedNode;
using ri::editor::TryReparentSelectedToWorldRoot;
using ri::editor::TryResetSelectedTransform;
using ri::editor::TrySelectAdjacentAuthoredNode;
using ri::editor::TrySnapSelectedNodeToGrid;
using ri::editor::TryUngroupSelectedNode;
using ri::editor::TryExportAssemblyCollidersCsv;
using ri::editor::TryExportAssemblyLightingCsv;
using ri::editor::TryImportAssemblyCollidersCsv;
using ri::editor::TryImportAssemblyLightingCsv;
using ri::editor::TryImportAssemblyTriggersCsv;
using ri::editor::TryExportAssemblyTriggersCsv;
using ri::editor::BuildProjectHealthReport;
using ri::editor::CanResolvePlaytestExecutable;
using ri::editor::ResolveDedicatedPlaytestExecutable;
using ri::editor::SummarizeProjectHealthForWelcome;
using ri::editor::SummarizeProjectHealthForFilesPanel;
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
    fs::path startupAssetPath;
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

fs::path BuildEditorLogicAuthoringPath(const fs::path& sceneStatePath) {
    return sceneStatePath.parent_path() / "logic_authoring.ri_logic";
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
    if (const std::optional<std::string> startupAsset = commandLine.GetValue("--open-asset");
        startupAsset.has_value() && !startupAsset->empty()) {
        config.startupAssetPath = NormalizePathForConfig(*startupAsset);
    }
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
        if (!sceneConfig_.startupAssetPath.empty()) {
            const ri::content::AuthoringHandoffReport handoff = ri::content::BuildAuthoringHandoff({
                .workspaceRoot = sceneConfig_.workspaceRoot,
                .assetPath = sceneConfig_.startupAssetPath,
                .gameId = sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->id : std::string{},
            });
            ri::core::LogInfo(std::string("Authoring handoff: ") + (handoff.valid ? "ready" : "rejected"));
            if (handoff.valid) {
                ri::core::LogInfo("Handoff asset kind: " + std::string(ri::content::ToString(handoff.assetKind)));
                ri::core::LogInfo("Handoff asset: " + handoff.assetPath.string());
            } else if (!handoff.issues.empty()) {
                ri::core::LogInfo("Handoff issue: " + handoff.issues.front());
            }
        }
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
        ri::editor::AnimateEditorWorkspaceScene(
            sceneConfig_.editorPreviewScene, starterScene_, frame.elapsedSeconds, true);

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

HFONT CreateUiFont(int height, int weight, const wchar_t* faceName = L"Tahoma") {
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
        DEFAULT_QUALITY,
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

[[nodiscard]] std::optional<int> PickRenderableInCameraView(const RECT& plot,
                                                              const int mx,
                                                              const int my,
                                                              const ri::scene::Scene& scene,
                                                              const int cameraNodeHandle) {
    if (plot.right <= plot.left + 4 || plot.bottom <= plot.top + 4
        || cameraNodeHandle == ri::scene::kInvalidHandle
        || cameraNodeHandle >= static_cast<int>(scene.NodeCount())) {
        return std::nullopt;
    }

    const float plotWidth = static_cast<float>(plot.right - plot.left);
    const float plotHeight = static_cast<float>(plot.bottom - plot.top);
    const float aspectRatio = plotWidth / std::max(plotHeight, 1.0f);
    const ri::scene::Node& cameraNode = scene.GetNode(cameraNodeHandle);
    if (cameraNode.camera == ri::scene::kInvalidHandle) {
        return std::nullopt;
    }
    const ri::scene::Camera& camera = scene.GetCamera(cameraNode.camera);
    const float fieldOfViewDegrees =
        ri::scene::ResolvePhotoModeFieldOfViewDegrees(camera.fieldOfViewDegrees, {}, aspectRatio);
    const ri::math::Mat4 worldMatrix = scene.ComputeWorldMatrix(cameraNodeHandle);
    const ri::math::Vec3 cameraPosition = ri::math::ExtractTranslation(worldMatrix);
    const ri::math::Vec3 cameraRight = ri::math::ExtractRight(worldMatrix);
    const ri::math::Vec3 cameraUp = ri::math::ExtractUp(worldMatrix);
    const ri::math::Vec3 cameraForward = ri::math::ExtractForward(worldMatrix);

    const float ndcX = ((static_cast<float>(mx - plot.left) / plotWidth) * 2.0f) - 1.0f;
    const float ndcY = 1.0f - ((static_cast<float>(my - plot.top) / plotHeight) * 2.0f);
    const float tanHalfFovY = std::tan(ri::math::DegreesToRadians(fieldOfViewDegrees * 0.5f));
    const float tanHalfFovX = tanHalfFovY * aspectRatio;
    const ri::math::Vec3 rayDirection = ri::math::Normalize(
        cameraForward + (cameraRight * (ndcX * tanHalfFovX)) + (cameraUp * (ndcY * tanHalfFovY)));
    const ri::spatial::Ray ray{.origin = cameraPosition, .direction = rayDirection};

    float bestDistance = std::numeric_limits<float>::infinity();
    int best = ri::scene::kInvalidHandle;
    const float farClip = std::max(camera.nearClip + 0.01f, camera.farClip);
    for (const int handle : ri::scene::CollectRenderableNodes(scene)) {
        const std::optional<ri::scene::WorldBounds> bounds =
            ri::scene::ComputeNodeWorldBounds(scene, handle, true);
        if (!bounds.has_value()) {
            continue;
        }
        const ri::spatial::Aabb box{
            .min = bounds->min,
            .max = bounds->max,
        };
        float hitDistance = 0.0f;
        if (!ri::spatial::IntersectRayAabb(ray, box, farClip, &hitDistance)) {
            continue;
        }
        if (hitDistance < bestDistance) {
            bestDistance = hitDistance;
            best = handle;
        }
    }

    if (best == ri::scene::kInvalidHandle) {
        return std::nullopt;
    }
    return best;
}

[[nodiscard]] ri::render::software::CameraViewRect CameraViewRectFrom(const RECT& plot) {
    return ri::render::software::CameraViewRect{
        .left = plot.left,
        .top = plot.top,
        .right = plot.right,
        .bottom = plot.bottom,
    };
}

[[nodiscard]] ri::math::Vec3 StructuralBrushSpawnPositionAtPoint(const std::string_view structuralType,
                                                                   const ri::math::Vec3& hitPoint) {
    if (structuralType == "plane") {
        return {hitPoint.x, 0.0f, hitPoint.z};
    }
    return {hitPoint.x, hitPoint.y + 0.5f, hitPoint.z};
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
    const COLORREF lineA = ri::editor::EditorUiTheme::kOrthoGridA;
    const COLORREF lineB = ri::editor::EditorUiTheme::kOrthoGridB;
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
    DrawInsetFrame(dc,
                   cell,
                   ri::editor::EditorUiTheme::kViewportWellFill,
                   ri::editor::EditorUiTheme::kWellHi,
                   ri::editor::EditorUiTheme::kWellShadow);
    RECT inner{cell.left + 2, cell.top + 2, cell.right - 2, cell.bottom - 2};
    FillRectColor(dc, inner, ri::editor::EditorUiTheme::kOrthoCellFill);

    DrawTextLine(dc,
                 RECT{inner.left + 6, inner.top + 4, inner.right - 6, inner.top + 22},
                 std::string(title),
                 ri::editor::EditorUiTheme::kOrthoTitle,
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
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, ri::editor::EditorUiTheme::kOrthoCrosshair, 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, ri::editor::EditorUiTheme::kOrthoCrosshair, 1);
    } else if (projection == RawIronFlatProjection::FrontXy) {
        cxA = center.x;
        cxB = center.y;
        halfSpan = std::max(std::max(size.x, size.y) * 0.5f + margin, 6.0f);
        DrawRawIronOrthoGrid(dc, plot, projection, cxA, cxB, halfSpan);
        LONG fx = 0;
        LONG fy = 0;
        ProjectRawIronFront(plot, cxA, cxB, halfSpan, orbitFocus.x, orbitFocus.y, fx, fy);
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, ri::editor::EditorUiTheme::kOrthoCrosshair, 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, ri::editor::EditorUiTheme::kOrthoCrosshair, 1);
    } else {
        cxA = center.z;
        cxB = center.y;
        halfSpan = std::max(std::max(size.z, size.y) * 0.5f + margin, 6.0f);
        DrawRawIronOrthoGrid(dc, plot, projection, cxA, cxB, halfSpan);
        LONG fx = 0;
        LONG fy = 0;
        ProjectRawIronSide(plot, cxA, cxB, halfSpan, orbitFocus.z, orbitFocus.y, fx, fy);
        DcStrokeLine(dc, fx - 6, fy, fx + 6, fy, ri::editor::EditorUiTheme::kOrthoCrosshair, 1);
        DcStrokeLine(dc, fx, fy - 6, fx, fy + 6, ri::editor::EditorUiTheme::kOrthoCrosshair, 1);
    }

    const std::vector<int> renderables = ri::scene::CollectRenderableNodes(scene);
    for (const int handle : renderables) {
        const std::optional<ri::scene::WorldBounds> nb = ri::scene::ComputeNodeWorldBounds(scene, handle, true);
        if (!nb.has_value()) {
            continue;
        }
        const bool isSelected = static_cast<std::size_t>(handle) == selectedNode;
        const COLORREF stroke =
            isSelected ? ri::editor::EditorUiTheme::kOrthoSelBox : ri::editor::EditorUiTheme::kTextMuted;
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
          autoOrbitPreview_(commandLine.HasFlag("--auto-orbit")),
          viewportRayTracePreview_(commandLine.HasFlag("--viewport-ray-trace")),
          viewportGpuAllowed_(commandLine.HasFlag("--gpu-viewport")) {
        ri::editor::RegisterBundledGameEditorPreviews();
        starterScene_ = ri::editor::BuildEditorWorkspaceScene(
            sceneConfig_.editorPreviewScene,
            sceneConfig_.sceneName,
            sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->rootPath : fs::path{});
        editorTextureRoot_ = DiscoverEditorTextureRoot();
        RefreshViewportPreviewConfiguration(true);
        editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
        (void)TryLoadEditorOrbitSidecar(sceneConfig_.sceneStatePath, editorOrbitState_);
        ApplyEditorOrbitToScene();
        statsOverlayState_.SetAttached(true);
        statsOverlayState_.SetVisible(statsOverlayVisible_);
        if (!sceneConfig_.statusMessage.empty()) {
            lastIoStatus_ = sceneConfig_.statusMessage + "  ";
        }
        lastIoStatus_ += "Camera: drag in CAMERA, wheel zooms. Tab: full 3D / quad. Alt+LMB/MMB orbit on geometry.";
        lastIoStatus_ +=
            "  Authoring: +Light cycles point/spot/directional, Ctrl+Shift+I import level CSVs, Ctrl+E export, Playtest.";
        if (autoOrbitPreview_) {
            lastIoStatus_ += "  (--auto-orbit: demo camera motion on)";
        }
        if (viewportRayTracePreview_) {
            lastIoStatus_ += "  (--viewport-ray-trace: quality preview on; Ctrl+Shift+R toggles)";
        } else {
            lastIoStatus_ += "  (Ctrl+Shift+R: ray-traced viewport preview)";
        }
        if (viewportGpuAllowed_) {
            lastIoStatus_ += "  (--gpu-viewport: experimental Vulkan viewport enabled)";
        }
        RefreshWorkspaceGamesAndResources();
        OpenStartupAssetHandoff();
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
        logicLayer_.EnsureKitLoaded(sceneConfig_.workspaceRoot);
        if (sceneConfig_.gameManifest.has_value()) {
            logicLayer_.EnsureGameColliderTrace(sceneConfig_.gameManifest->rootPath);
        }
        ri::editor::BindAuthoringLogicCatalog(&logicLayer_);
        // Catalog thumbnails load lazily via PrewarmVisible during paint.
        RefreshPluginStoreState();
        if (!fs::exists(ResolveSceneStatePath(), loadEc) && !fs::exists(ResolveAuthoredSceneStatePath(), loadEc)) {
            (void)logicLayer_.Load(
                ResolveLogicAuthoringPath(), starterScene_.scene, starterScene_.handles.root);
        }
        if (sceneConfig_.gameManifest.has_value()) {
            leftPanelMode_ = LeftPanelMode::Scene;
            full3DViewport_ = true;
            rightPanelCollapsed_ = true;
            toolMode_ = ri::editor::EditorToolMode::Create;
            authoringCatalogExpanded_ = true;
        }
        lastIoStatus_ += "  S select · C create · click viewport to stamp · Sky bar cycles atmosphere · F1 help.";
    }

    int Run(HINSTANCE instance) {
        const wchar_t* className = L"RawIronEditorWindow";
        WNDCLASSW windowClass{};
        windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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
            ri::editor::DefaultEditorStartupWidth(),
            ri::editor::DefaultEditorStartupHeight(),
            nullptr,
            nullptr,
            instance,
            this);
        if (hwnd_ == nullptr) {
            return 1;
        }

        titleFont_ = CreateUiFont(-20, FW_BOLD, L"Bahnschrift SemiBold");
        headerFont_ = CreateUiFont(-15, FW_SEMIBOLD, L"Bahnschrift SemiBold");
        bodyFont_ = CreateUiFont(-14, FW_SEMIBOLD, L"Segoe UI");
        smallFont_ = CreateUiFont(-12, FW_NORMAL, L"Segoe UI");
        monoFont_ = CreateUiFont(-12, FW_NORMAL, L"Consolas");

        if (viewportGpuAllowed_) {
            StartVulkanViewport();
        }
        SetTimer(hwnd_, 1, 33, nullptr);
        lastTick_ = std::chrono::steady_clock::now();

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        vulkanViewport_.Stop();
        DestroyEditorBackBuffer();
        DeleteObject(titleFont_);
        DeleteObject(headerFont_);
        DeleteObject(bodyFont_);
        DeleteObject(smallFont_);
        DeleteObject(monoFont_);
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
        Create,
        Resources,
    };
    enum class CameraDragMode {
        None,
        ViewOrbit,
        ViewPan,
        RailTrackball,
        RailPan,
        RailDepth,
    };
    enum class InspectorPanel {
        Node,
        Brush,
        Gameplay,
        Files,
        PluginStore,
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
        RECT cameraRail{};
        RECT hierarchyInner{};
        RECT cameraRailInner{};
        RECT viewport{};
        RECT inspector{};
        RECT viewportInner{};
        RECT inspectorInner{};
        RECT hierarchySplitter{};
        RECT inspectorSplitter{};
    };
    struct RailPadState {
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float springStartX = 0.0f;
        float springStartY = 0.0f;
        bool springing = false;
        std::chrono::steady_clock::time_point springStart{};
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
            case WM_LBUTTONDBLCLK:
                return self->OnLeftButtonDoubleClick(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_LBUTTONUP:
                return self->OnLeftButtonUp(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_MBUTTONDOWN:
                return self->OnMiddleButtonDown(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_MBUTTONUP:
                return self->OnMiddleButtonUp(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_RBUTTONDOWN:
                return self->OnRightButtonDown(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_RBUTTONUP:
                return self->OnRightButtonUp(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            case WM_MOUSEMOVE:
                return self->OnMouseMove(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)), wParam);
            case WM_SETCURSOR:
                if (LOWORD(lParam) == HTCLIENT) {
                    POINT pt{};
                    GetCursorPos(&pt);
                    ScreenToClient(hwnd, &pt);
                    if (self->UpdateInteractiveCursor(pt.x, pt.y)) {
                        return TRUE;
                    }
                }
                break;
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
                self->DestroyEditorBackBuffer();
                self->MarkViewportPreviewDirty();
                self->viewportRestartPending_ = self->viewportGpuEnabled_;
                self->lastViewportResizeSteady_ = std::chrono::steady_clock::now();
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case WM_MOVE:
                if (self->viewportGpuEnabled_) {
                    self->MarkViewportPreviewDirty();
                    self->InvalidateViewportAndRail();
                }
                break;
            case WM_ENTERSIZEMOVE:
                self->liveWindowMoveSize_ = true;
                self->MarkViewportPreviewDirty();
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case WM_EXITSIZEMOVE:
                self->liveWindowMoveSize_ = false;
                self->MarkViewportPreviewDirty();
                self->viewportRestartPending_ = self->viewportGpuEnabled_;
                self->lastViewportResizeSteady_ =
                    std::chrono::steady_clock::now() - std::chrono::milliseconds(250);
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case WM_GETMINMAXINFO: {
                auto* minMax = reinterpret_cast<MINMAXINFO*>(lParam);
                minMax->ptMinTrackSize.x = 680;
                minMax->ptMinTrackSize.y = 520;
                return 0;
            }
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
        const bool windowMinimized = IsIconic(hwnd_) != FALSE;
        if (windowMinimized) {
            AdaptEditorTimerInterval();
            return;
        }
        statsOverlayState_.RecordFrameDeltaSeconds(delta.count());
        statsOverlayState_.SetAttached(true);
        statsOverlayState_.SetVisible(statsOverlayVisible_);
        const ri::editor::ViewportCameraState previousCamera = SnapshotViewportCameraState();
        if (ri::editor::ShouldRunEditorPreviewAnimation(windowMinimized)) {
            ri::editor::AnimateEditorWorkspaceScene(
                sceneConfig_.editorPreviewScene, starterScene_, elapsedSeconds_, true);
        }
        const bool logicLivePreview = ri::editor::ShouldRunLogicLivePreview(
            logicLayer_.IsCreatorLayerVisible(),
            inspectorPanel_ == InspectorPanel::Gameplay,
            authoringCatalogExpanded_
                && authoringCatalogSection_ == ri::editor::AuthoringCatalogSection::Logic);
        if (logicLivePreview && !logicLayer_.PlacedNodes().empty()) {
            const bool forceLogicTick = cameraDragMode_ != CameraDragMode::None || autoOrbitPreview_;
            if (ri::editor::ShouldTickLogicPreview(logicPreviewFrameCounter_++, forceLogicTick)) {
                logicLayer_.TickSenseProbes(editorOrbitState_.target);
                logicLayer_.ApplyCircuitProbeColors(starterScene_.scene);
            }
        } else {
            logicPreviewFrameCounter_ = 0;
        }
        if (!autoOrbitPreview_) {
            ApplyEditorOrbitToScene();
        } else {
            editorOrbitState_.yawDegrees =
                ri::editor::AdvanceAutoOrbitYaw(editorOrbitState_.yawDegrees, delta.count());
            ApplyEditorOrbitToScene();
        }
        UpdateCameraRailSpringStates(now);
        ApplyCameraRailVelocity(delta.count());
        const bool cameraChanged =
            ri::editor::HasCameraStateChanged(previousCamera, SnapshotViewportCameraState());
        const bool viewportInteractiveMotion = ri::editor::IsViewportInteractiveMotion(
            cameraChanged,
            cameraDragMode_ != CameraDragMode::None,
            autoOrbitPreview_,
            IsCameraRailAnimating());
        viewportPreviewDirty_ = viewportPreviewDirty_ || cameraChanged;
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        if (viewportGpuEnabled_) {
            vulkanViewport_.SetBounds(cameraPlotRect_);
        }
        if (ri::editor::ShouldPollGamePreviewScripts(
                sceneConfig_.gameManifest.has_value(), now - lastGamePreviewScriptPoll_)) {
            lastGamePreviewScriptPoll_ = now;
            if (ri::render::software::DidGamePreviewScriptsChange(
                    sceneConfig_.gameManifest->rootPath, gamePreviewScriptTimestamps_)) {
                RefreshViewportPreviewConfiguration(false);
                ClearViewportPreviewCache();
                lastIoStatus_ = "Rendering/postprocess scripts changed — viewport atmosphere reloaded.";
            }
        }
        if (viewportRestartPending_ && viewportGpuEnabled_ && !vulkanViewport_.RestartInFlight()
            && now - lastViewportResizeSteady_ >= std::chrono::milliseconds(120)) {
            RestartVulkanViewport();
            viewportRestartPending_ = false;
        }
        bool viewportRendered = false;
        if (viewportGpuEnabled_ && vulkanViewport_.Running()) {
            vulkanViewport_.SetVisible(true);
            PublishVulkanViewportFrame(cameraPlotRect_, viewportInteractiveMotion);
            viewportPreviewDirty_ = false;
        } else {
            if (viewportGpuEnabled_) {
                vulkanViewport_.SetVisible(false);
            }
            if (viewportGpuEnabled_ && !vulkanViewport_.RestartInFlight()) {
                const std::string error = vulkanViewport_.LastError();
                const bool recoverableSurfaceIssue =
                    error.find("surface") != std::string::npos || error.find("swapchain") != std::string::npos
                    || error.find("VK_ERROR_OUT_OF_DATE_KHR") != std::string::npos;
                if (recoverableSurfaceIssue) {
                    viewportRestartPending_ = true;
                    lastViewportResizeSteady_ = now;
                    lastIoStatus_ = "Viewport: recovering Vulkan surface; CPU fallback active.";
                } else {
                    viewportGpuEnabled_ = false;
                    lastIoStatus_ = "Viewport: Vulkan stopped; CPU fallback active."
                        + (error.empty() ? std::string{} : " " + error);
                }
            }
            const ri::editor::ViewportRenderPolicy renderPolicy{
                .plotWidth = static_cast<int>(cameraPlotRect_.right - cameraPlotRect_.left),
                .plotHeight = static_cast<int>(cameraPlotRect_.bottom - cameraPlotRect_.top),
                .cameraMoving = viewportInteractiveMotion,
                .resolutionScalingEnabled = viewportResolutionScalingEnabled_,
                .previewDirty = viewportPreviewDirty_,
            };
            if (ri::editor::ShouldRenderViewportPreview(renderPolicy)) {
                RebuildViewportPreviewBitmap();
                viewportRendered = true;
            }
        }
        AdaptEditorTimerInterval();
        MaybeAutosaveState();
        if (IsCameraRailAnimating()) {
            InvalidateRect(hwnd_, &layout.cameraRail, FALSE);
        }
        if (viewportRendered) {
            InvalidateRect(hwnd_, &cameraPlotRect_, FALSE);
        }
        if (liveWindowMoveSize_) {
            InvalidateViewportAndRail();
        }
    }

    void ClearViewportPreviewCache() {
        viewportPreviewCache_ = {};
        viewportPreviewScratch_ = {};
        viewportPreviewReady_ = false;
        ++viewportPreviewBlitGeneration_;
        ri::editor::EditorRenderer::ReleaseSoftwareImageBlitCache(viewportPreviewBlitCache_);
        MarkViewportPreviewDirty();
    }

    void MarkViewportPreviewDirty() {
        viewportPreviewDirty_ = true;
        viewportSceneSnapshotDirty_ = true;
    }

    void AdaptEditorTimerInterval() {
        const bool viewportInteractiveMotion = ri::editor::IsViewportInteractiveMotion(
            false,
            cameraDragMode_ != CameraDragMode::None,
            autoOrbitPreview_,
            IsCameraRailAnimating());
        const UINT intervalMs = ri::editor::ComputeViewportTimerIntervalMs(
            viewportInteractiveMotion,
            lastViewportPreviewMs_);
        SetTimer(hwnd_, 1, intervalMs, nullptr);
    }

    [[nodiscard]] fs::path DiscoverEditorTextureRoot() const {
        fs::path editorExe{};
        wchar_t moduleWide[MAX_PATH]{};
        if (GetModuleFileNameW(nullptr, moduleWide, MAX_PATH) > 0) {
            editorExe = fs::path(std::wstring(moduleWide));
        }
        return ri::content::PickEngineTexturesDirectory(sceneConfig_.workspaceRoot, editorExe);
    }

    void RefreshViewportPreviewConfiguration(const bool snapshotScriptTimestamps) {
        viewportPreviewOptionsTemplate_ = {};
        if (!editorTextureRoot_.empty()) {
            viewportPreviewOptionsTemplate_.textureRoot = editorTextureRoot_;
        }
        ri::editor::ConfigureEditorViewportForPreview(
            sceneConfig_.editorPreviewScene,
            viewportPreviewOptionsTemplate_,
            sceneConfig_.gameManifest.has_value() ? &sceneConfig_.gameManifest->rootPath : nullptr);
        if (snapshotScriptTimestamps && sceneConfig_.gameManifest.has_value()) {
            ri::render::software::SnapshotGamePreviewScriptTimestamps(
                sceneConfig_.gameManifest->rootPath, gamePreviewScriptTimestamps_);
        }
        lastGamePreviewScriptPoll_ = std::chrono::steady_clock::now();
    }

    [[nodiscard]] ri::render::software::ScenePreviewOptions BuildViewportPreviewOptions(
        const int width,
        const int height) const {
        ri::render::software::ScenePreviewOptions options = viewportPreviewOptionsTemplate_;
        options.width = width;
        options.height = height;
        options.hiddenNodeHandles = {
            starterScene_.handles.grid,
            starterScene_.handles.axes.root,
            starterScene_.handles.axes.xAxis,
            starterScene_.handles.axes.yAxis,
            starterScene_.handles.axes.zAxis,
        };
        return options;
    }

    void PublishVulkanViewportFrame(const RECT& cameraRect, const bool viewportInteractiveMotion) {
        if (!viewportGpuEnabled_ || !vulkanViewport_.Running()) {
            return;
        }
        const bool needsPublish = viewportSceneSnapshotDirty_ || viewportInteractiveMotion;
        vulkanViewport_.SetBounds(cameraRect);
        if (!needsPublish) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto publishInterval =
            viewportInteractiveMotion ? std::chrono::milliseconds(11) : std::chrono::milliseconds(33);
        if (lastVulkanViewportPublish_.time_since_epoch().count() != 0
            && now - lastVulkanViewportPublish_ < publishInterval) {
            return;
        }
        const int width = std::max(64L, cameraRect.right - cameraRect.left);
        const int height = std::max(64L, cameraRect.bottom - cameraRect.top);
        ri::render::software::ScenePreviewOptions options = BuildViewportPreviewOptions(width, height);
        vulkanViewport_.Publish(starterScene_.scene,
                                starterScene_.handles.orbitCamera.cameraNode,
                                options,
                                elapsedSeconds_,
                                viewportSceneSnapshotDirty_);
        lastVulkanViewportPublish_ = now;
        viewportSceneSnapshotDirty_ = false;
    }

    void StartVulkanViewport() {
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        if (cameraPlotRect_.right <= cameraPlotRect_.left || cameraPlotRect_.bottom <= cameraPlotRect_.top) {
            return;
        }
        const int width = std::max(64L, cameraPlotRect_.right - cameraPlotRect_.left);
        const int height = std::max(64L, cameraPlotRect_.bottom - cameraPlotRect_.top);
        vulkanViewport_.Publish(starterScene_.scene,
                                starterScene_.handles.orbitCamera.cameraNode,
                                BuildViewportPreviewOptions(width, height),
                                elapsedSeconds_,
                                true);
        viewportGpuEnabled_ = vulkanViewport_.Start(hwnd_, cameraPlotRect_);
        if (viewportGpuEnabled_) {
            lastIoStatus_ = "Viewport: native Vulkan GPU renderer active.";
        } else {
            lastIoStatus_ = "Viewport: Vulkan unavailable; CPU software fallback active. "
                + vulkanViewport_.LastError();
        }
    }

    void RestartVulkanViewport() {
        if (!viewportGpuEnabled_) {
            return;
        }
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        viewportSceneSnapshotDirty_ = true;
        vulkanViewport_.RestartAsync(hwnd_, cameraPlotRect_);
    }

    void RebuildViewportPreviewBitmap() {
        if (starterScene_.handles.orbitCamera.cameraNode == ri::scene::kInvalidHandle) {
            viewportPreviewReady_ = false;
            return;
        }
        if (cameraPlotRect_.right <= cameraPlotRect_.left + 8
            || cameraPlotRect_.bottom <= cameraPlotRect_.top + 8) {
            viewportPreviewReady_ = false;
            return;
        }

        const int plotWidth = std::max(1, static_cast<int>(cameraPlotRect_.right - cameraPlotRect_.left));
        const int plotHeight = std::max(1, static_cast<int>(cameraPlotRect_.bottom - cameraPlotRect_.top));
        const bool viewportInteractiveMotion = ri::editor::IsViewportInteractiveMotion(
            false,
            cameraDragMode_ != CameraDragMode::None,
            autoOrbitPreview_,
            IsCameraRailAnimating());
        const ri::editor::ViewportRenderSize renderSize =
            ri::editor::ComputeViewportRenderSize(ri::editor::ViewportRenderPolicy{
                .plotWidth = plotWidth,
                .plotHeight = plotHeight,
                .cameraMoving = viewportInteractiveMotion,
                .resolutionScalingEnabled = viewportResolutionScalingEnabled_,
                .previewDirty = viewportPreviewDirty_,
            });
        const int renderWidth = renderSize.width;
        const int renderHeight = renderSize.height;

        ri::render::software::ScenePreviewOptions options = BuildViewportPreviewOptions(renderWidth, renderHeight);
        if (viewportRayTracePreview_) {
            options.renderer = ri::render::software::ScenePreviewRenderer::RayTrace;
            options.rayTracingResolutionScale = 0.68f;
            options.rayTracingShadowRays = 4;
            options.rayTracingMaxBounces = 2;
            options.rayTracingReflections = true;
        } else {
            options.renderer = ri::render::software::ScenePreviewRenderer::Raster;
            options.pointSampleTextures = viewportInteractiveMotion;
            options.adaptiveTextureSampling = true;
            // Keep perspective-correct UVs on hall-scale quads; lowSpecMode enables affine mapping
            // which warps large floors/walls in the editor viewport.
            options.affineTextureMapping = false;
            options.farHorizonMaxNodeStride = std::max(options.farHorizonMaxNodeStride, 6U);
            options.farHorizonMaxInstanceStride = std::max(options.farHorizonMaxInstanceStride, 10U);
        }

        const auto renderStart = std::chrono::steady_clock::now();
        ri::render::software::RenderScenePreviewInto(starterScene_.scene,
                                                      starterScene_.handles.orbitCamera.cameraNode,
                                                      options,
                                                      viewportPreviewScratch_,
                                                      &viewportPreviewCache_);
        lastViewportRenderWidth_ = renderWidth;
        lastViewportRenderHeight_ = renderHeight;
        lastViewportPreviewMs_ =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - renderStart).count();
        viewportPreviewReady_ = !viewportPreviewScratch_.pixels.empty();
        ++viewportPreviewBlitGeneration_;
        viewportPreviewDirty_ = false;
    }

    [[nodiscard]] ri::editor::ViewportCameraState SnapshotViewportCameraState() const {
        if (starterScene_.handles.orbitCamera.cameraNode != ri::scene::kInvalidHandle) {
            const ri::scene::Node& cameraNode =
                starterScene_.scene.GetNode(starterScene_.handles.orbitCamera.cameraNode);
            const ri::math::Vec3 position =
                starterScene_.scene.ComputeWorldPosition(starterScene_.handles.orbitCamera.cameraNode);
            return {
                .targetX = position.x,
                .targetY = position.y,
                .targetZ = position.z,
                .distance = cameraNode.localTransform.rotationDegrees.x,
                .yawDegrees = cameraNode.localTransform.rotationDegrees.y,
                .pitchDegrees = cameraNode.localTransform.rotationDegrees.z,
            };
        }
        return {
            .targetX = editorOrbitState_.target.x,
            .targetY = editorOrbitState_.target.y,
            .targetZ = editorOrbitState_.target.z,
            .distance = editorOrbitState_.distance,
            .yawDegrees = editorOrbitState_.yawDegrees,
            .pitchDegrees = editorOrbitState_.pitchDegrees,
        };
    }

    void ApplyEditorOrbitToScene() {
        editorOrbitState_.pitchDegrees = std::clamp(editorOrbitState_.pitchDegrees, -85.0f, 85.0f);
        editorOrbitState_.distance = std::clamp(editorOrbitState_.distance, 0.75f, 180.0f);
        ri::scene::SetOrbitCameraState(starterScene_.scene, starterScene_.handles.orbitCamera, editorOrbitState_);
    }

    void PanEditorOrbitCamera(const int dx, const int dy) {
        const float yawRad = editorOrbitState_.yawDegrees * (3.14159265f / 180.0f);
        const float pitchRad = editorOrbitState_.pitchDegrees * (3.14159265f / 180.0f);
        const float cosPitch = std::cos(pitchRad);
        const float sinPitch = std::sin(pitchRad);
        const float sinYaw = std::sin(yawRad);
        const float cosYaw = std::cos(yawRad);
        const ri::math::Vec3 forward{cosPitch * sinYaw, sinPitch, cosPitch * cosYaw};
        const ri::math::Vec3 worldUp{0.0f, 1.0f, 0.0f};
        ri::math::Vec3 right = ri::math::Cross(forward, worldUp);
        if (ri::math::LengthSquared(right) < 1.0e-6f) {
            right = ri::math::Vec3{1.0f, 0.0f, 0.0f};
        } else {
            right = ri::math::Normalize(right);
        }
        const ri::math::Vec3 up = ri::math::Normalize(ri::math::Cross(right, forward));
        const float scale = std::max(0.0015f, editorOrbitState_.distance * 0.0018f);
        editorOrbitState_.target =
            editorOrbitState_.target + right * (-static_cast<float>(dx) * scale) + up * (static_cast<float>(dy) * scale);
    }

    [[nodiscard]] bool IsRailDragMode() const {
        return cameraDragMode_ == CameraDragMode::RailTrackball
            || cameraDragMode_ == CameraDragMode::RailPan
            || cameraDragMode_ == CameraDragMode::RailDepth;
    }

    [[nodiscard]] bool IsViewDragMode() const {
        return cameraDragMode_ == CameraDragMode::ViewOrbit || cameraDragMode_ == CameraDragMode::ViewPan;
    }

    [[nodiscard]] bool IsCameraRailAnimating() const {
        return IsRailDragMode()
            || orbitRailPad_.springing
            || panRailPad_.springing
            || depthRailPad_.springing
            || std::abs(orbitRailPad_.offsetX) > 0.01f
            || std::abs(orbitRailPad_.offsetY) > 0.01f
            || std::abs(panRailPad_.offsetX) > 0.01f
            || std::abs(panRailPad_.offsetY) > 0.01f
            || std::abs(depthRailPad_.offsetX) > 0.01f
            || std::abs(depthRailPad_.offsetY) > 0.01f;
    }

    [[nodiscard]] static float ApplyRailDeadzone(const float value) {
        constexpr float kDeadzone = 4.0f;
        if (std::abs(value) <= kDeadzone) {
            return 0.0f;
        }
        return value - (value > 0.0f ? kDeadzone : -kDeadzone);
    }

    void UpdateRailPadSpring(RailPadState& state, const std::chrono::steady_clock::time_point now) {
        if (!state.springing) {
            return;
        }
        ri::editor::AdvanceRailPadSpring(state, std::chrono::duration<double>(now - state.springStart).count());
    }

    void UpdateCameraRailSpringStates(const std::chrono::steady_clock::time_point now) {
        UpdateRailPadSpring(orbitRailPad_, now);
        UpdateRailPadSpring(panRailPad_, now);
        UpdateRailPadSpring(depthRailPad_, now);
    }

    void ApplyCameraRailVelocity(const double deltaSeconds) {
        if (cameraDragMode_ == CameraDragMode::RailTrackball
            || cameraDragMode_ == CameraDragMode::RailPan
            || cameraDragMode_ == CameraDragMode::RailDepth) {
            return;
        }
        const float frameScale = static_cast<float>(std::clamp(deltaSeconds * 60.0, 0.2, 2.0));
        const float orbitX = ApplyRailDeadzone(orbitRailPad_.offsetX);
        const float orbitY = ApplyRailDeadzone(orbitRailPad_.offsetY);
        const float panX = ApplyRailDeadzone(panRailPad_.offsetX);
        const float panY = ApplyRailDeadzone(panRailPad_.offsetY);
        const float depthX = ApplyRailDeadzone(depthRailPad_.offsetX);
        const float depthY = ApplyRailDeadzone(depthRailPad_.offsetY);
        if (orbitX == 0.0f && orbitY == 0.0f
            && panX == 0.0f && panY == 0.0f
            && depthX == 0.0f && depthY == 0.0f) {
            return;
        }

        if (orbitX != 0.0f || orbitY != 0.0f) {
            editorOrbitState_.yawDegrees += orbitX * 0.090f * frameScale;
            editorOrbitState_.pitchDegrees -= orbitY * 0.090f * frameScale;
            lastIoStatus_ = "View rig: orbit.";
        }
        if (panX != 0.0f || panY != 0.0f) {
            PanEditorOrbitCamera(static_cast<int>(std::lround(panX * 0.95f * frameScale)),
                                 static_cast<int>(std::lround(panY * 0.95f * frameScale)));
            lastIoStatus_ = "View rig: pan.";
        }
        if (depthX != 0.0f || depthY != 0.0f) {
            editorOrbitState_.distance *= std::exp(depthY * 0.00145f * frameScale);
            PanEditorOrbitCamera(static_cast<int>(std::lround(depthX * 0.50f * frameScale)), 0);
            lastIoStatus_ = depthY < 0.0f ? "View rig: zoom in." : "View rig: zoom out.";
        }
        ApplyEditorOrbitToScene();
        viewportPreviewDirty_ = true;
    }

    void ApplyDirectCameraRailDrag(const int dx, const int dy) {
        if (cameraDragMode_ == CameraDragMode::RailTrackball) {
            editorOrbitState_.yawDegrees += static_cast<float>(dx) * 0.70f;
            editorOrbitState_.pitchDegrees -= static_cast<float>(dy) * 0.70f;
            lastIoStatus_ = "View rig: orbit.";
        } else if (cameraDragMode_ == CameraDragMode::RailPan) {
            PanEditorOrbitCamera(static_cast<int>(std::lround(static_cast<float>(dx) * 1.6f)),
                                 static_cast<int>(std::lround(static_cast<float>(dy) * 1.6f)));
            lastIoStatus_ = "View rig: pan.";
        } else if (cameraDragMode_ == CameraDragMode::RailDepth) {
            editorOrbitState_.distance *= std::exp(static_cast<float>(dy) * 0.0105f);
            PanEditorOrbitCamera(static_cast<int>(std::lround(static_cast<float>(dx) * 0.95f)), 0);
            lastIoStatus_ = dy < 0 ? "View rig: zoom in." : (dy > 0 ? "View rig: zoom out." : "View rig: truck.");
        } else {
            return;
        }
        ApplyEditorOrbitToScene();
        viewportPreviewDirty_ = true;
    }

    void UpdateCameraRailPadFromPoint(const POINT& point, const RECT& railRect) {
        const ri::editor::CameraRailLayout layout = ri::editor::ComputeCameraRailLayout(railRect);
        auto clampCircle = [](const float dx, const float dy, const float radius) {
            const float length = std::sqrt((dx * dx) + (dy * dy));
            if (length <= radius || length <= 0.001f) {
                return std::pair<float, float>{dx, dy};
            }
            const float scale = radius / length;
            return std::pair<float, float>{dx * scale, dy * scale};
        };
        if (cameraDragMode_ == CameraDragMode::RailTrackball) {
            const auto [x, y] = clampCircle(static_cast<float>(point.x - layout.trackballCenter.x),
                                            static_cast<float>(point.y - layout.trackballCenter.y),
                                            static_cast<float>(layout.orbitRadius));
            orbitRailPad_.offsetX = x;
            orbitRailPad_.offsetY = y;
            orbitRailPad_.springing = false;
        } else if (cameraDragMode_ == CameraDragMode::RailPan) {
            const float dx = static_cast<float>(point.x - layout.panCenter.x);
            const float dy = static_cast<float>(point.y - layout.panCenter.y);
            if (std::abs(dx) >= std::abs(dy)) {
                panRailPad_.offsetX = std::clamp(dx,
                                                 -static_cast<float>(layout.panRadius),
                                                 static_cast<float>(layout.panRadius));
                panRailPad_.offsetY = 0.0f;
            } else {
                panRailPad_.offsetX = 0.0f;
                panRailPad_.offsetY = std::clamp(dy,
                                                 -static_cast<float>(layout.panRadius),
                                                 static_cast<float>(layout.panRadius));
            }
            panRailPad_.springing = false;
        } else if (cameraDragMode_ == CameraDragMode::RailDepth) {
            depthRailPad_.offsetX = std::clamp(static_cast<float>(point.x - layout.depthCenter.x),
                                               -static_cast<float>(layout.depthHalfWidth),
                                               static_cast<float>(layout.depthHalfWidth));
            depthRailPad_.offsetY = std::clamp(static_cast<float>(point.y - layout.depthCenter.y),
                                               -static_cast<float>(layout.depthHalfHeight),
                                               static_cast<float>(layout.depthHalfHeight));
            depthRailPad_.springing = false;
        }
    }

    void ApplyCameraDragDelta(const int dx, const int dy) {
        if (cameraDragMode_ == CameraDragMode::ViewOrbit) {
            editorOrbitState_.yawDegrees += static_cast<float>(dx) * 0.38f;
            editorOrbitState_.pitchDegrees -= static_cast<float>(dy) * 0.38f;
        } else if (cameraDragMode_ == CameraDragMode::ViewPan) {
            PanEditorOrbitCamera(dx, dy);
        } else if (IsRailDragMode()) {
            (void)dx;
            (void)dy;
            return;
        } else {
            return;
        }
        ApplyEditorOrbitToScene();
        viewportPreviewDirty_ = true;
        AdaptEditorTimerInterval();
    }

    [[nodiscard]] bool CameraRailInteractionBlocked() {
        if (autoOrbitPreview_) {
            lastIoStatus_ = "Camera rail disabled while auto-orbit preview is running.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        return false;
    }

    void ResetViewportCameraHome() {
        if (CameraRailInteractionBlocked()) {
            return;
        }
        editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
        ApplyEditorOrbitToScene();
        ClearViewportPreviewCache();
        lastIoStatus_ = "Camera rail: reset to home view.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool TryBeginCameraRailDrag(const POINT& point, const RECT& railRect) {
        if (CameraRailInteractionBlocked()) {
            return false;
        }
        const ri::editor::CameraRailHit hit = ri::editor::HitTestCameraRail(railRect, point);
        if (hit == ri::editor::CameraRailHit::None) {
            return false;
        }
        switch (hit) {
            case ri::editor::CameraRailHit::HomeButton:
                ResetViewportCameraHome();
                return true;
            case ri::editor::CameraRailHit::FrameSelectionButton:
                TryFrameSelection();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            case ri::editor::CameraRailHit::FrameAllButton:
                TryFrameAllRenderables();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            case ri::editor::CameraRailHit::ResolutionScaleButton:
                viewportResolutionScalingEnabled_ = !viewportResolutionScalingEnabled_;
                viewportPreviewDirty_ = true;
                lastIoStatus_ = viewportResolutionScalingEnabled_
                    ? "Viewport motion scaling: 1/2 resolution while moving."
                    : "Viewport motion scaling: full resolution.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            case ri::editor::CameraRailHit::Trackball:
                cameraDragMode_ = CameraDragMode::RailTrackball;
                orbitRailPad_.springing = false;
                break;
            case ri::editor::CameraRailHit::PanCross:
                cameraDragMode_ = CameraDragMode::RailPan;
                panRailPad_.springing = false;
                break;
            case ri::editor::CameraRailHit::DepthCross:
                cameraDragMode_ = CameraDragMode::RailDepth;
                depthRailPad_.springing = false;
                break;
            case ri::editor::CameraRailHit::TrackballCenter:
            case ri::editor::CameraRailHit::PanCenter:
            case ri::editor::CameraRailHit::None:
                return false;
        }
        lastDragX_ = point.x;
        lastDragY_ = point.y;
        lastRailInputSteady_ = std::chrono::steady_clock::now();
        UpdateCameraRailPadFromPoint(point, railRect);
        SetCapture(hwnd_);
        viewportPreviewDirty_ = true;
        AdaptEditorTimerInterval();
        InvalidateViewportAndRail();
        return true;
    }

    [[nodiscard]] bool IsKeyDown(const int virtualKey) const {
        return (GetKeyState(virtualKey) & 0x8000) != 0;
    }

    [[nodiscard]] bool HitCameraPlot(const POINT& point) const {
        return PtInRect(&cameraPlotRect_, point) != FALSE;
    }

    bool TryBeginCameraDrag(const POINT& point, const CameraDragMode mode) {
        if (autoOrbitPreview_ || mode == CameraDragMode::None || !HitCameraPlot(point)) {
            return false;
        }
        cameraDragMode_ = mode;
        lastDragX_ = point.x;
        lastDragY_ = point.y;
        SetCapture(hwnd_);
        lastIoStatus_ = mode == CameraDragMode::ViewPan ? "Camera: panning (release to stop)."
                                                    : "Camera: orbiting (release to stop).";
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    void EndCameraDrag() {
        if (cameraDragMode_ == CameraDragMode::None) {
            return;
        }
        if (cameraDragMode_ == CameraDragMode::RailTrackball) {
            ri::editor::BeginRailPadSpring(orbitRailPad_);
            orbitRailPad_.springStart = std::chrono::steady_clock::now();
        } else if (cameraDragMode_ == CameraDragMode::RailPan) {
            ri::editor::BeginRailPadSpring(panRailPad_);
            panRailPad_.springStart = std::chrono::steady_clock::now();
        } else if (cameraDragMode_ == CameraDragMode::RailDepth) {
            ri::editor::BeginRailPadSpring(depthRailPad_);
            depthRailPad_.springStart = std::chrono::steady_clock::now();
        }
        cameraDragMode_ = CameraDragMode::None;
        viewportPreviewDirty_ = true;
        if (GetCapture() == hwnd_) {
            ReleaseCapture();
        }
        InvalidateViewportAndRail();
    }

    void TryFrameSelection() {
        if (autoOrbitPreview_) {
            lastIoStatus_ = "Frame selection unavailable while auto-orbit preview is running.";
            return;
        }
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            lastIoStatus_ = "Frame selection: pick a node first.";
            return;
        }
        const std::vector<int> handles = {static_cast<int>(selectedNode_)};
        if (ri::scene::FrameNodesWithOrbitCamera(starterScene_.scene,
                                                 starterScene_.handles.orbitCamera,
                                                 handles,
                                                 1.35f)) {
            editorOrbitState_ = starterScene_.handles.orbitCamera.orbit;
            ApplyEditorOrbitToScene();
            ClearViewportPreviewCache();
            lastIoStatus_ = "Framed selection (F or double-click hierarchy). Ctrl+S saves orbit sidecar.";
        } else {
            lastIoStatus_ = "Could not frame selection.";
        }
    }

    [[nodiscard]] bool UpdateInteractiveCursor(const int x, const int y) {
        if (cameraDragMode_ == CameraDragMode::ViewPan) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            return true;
        }
        if (cameraDragMode_ == CameraDragMode::ViewOrbit) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return true;
        }
        if (IsRailDragMode()) {
            return true;
        }
        if (draggingHierarchySplitter_ || draggingInspectorSplitter_) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            return true;
        }
        const EditorLayout layout = ComputeLayout();
        const POINT point{x, y};
        const int splitterGrab = 6;
        if (!leftPanelCollapsed_
            && std::abs(x - layout.hierarchySplitter.right) <= splitterGrab
            && y >= layout.hierarchy.top && y <= layout.hierarchy.bottom) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            return true;
        }
        if (!rightPanelCollapsed_
            && std::abs(x - layout.inspectorSplitter.left) <= splitterGrab
            && y >= layout.inspector.top && y <= layout.inspector.bottom) {
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            return true;
        }
        UpdateCameraPlotRect(layout.viewportInner);
        if (full3DViewport_ && PtInRect(&cameraPlotRect_, point) != FALSE && !autoOrbitPreview_) {
            const bool shiftHeld = IsKeyDown(VK_SHIFT);
            const bool altHeld = IsKeyDown(VK_MENU);
            if (shiftHeld) {
                SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            } else if (altHeld) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
            } else {
                SetCursor(LoadCursor(nullptr, IDC_CROSS));
            }
            return true;
        }
        return false;
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

    void TryImportLevelCsvSupplement(std::string* statusOut = nullptr) {
        if (!sceneConfig_.gameManifest.has_value()) {
            return;
        }
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            return;
        }
        const fs::path gameRoot = sceneConfig_.gameManifest->rootPath;
        const std::size_t nodeCountBefore = starterScene_.scene.NodeCount();
        auto appendStatus = [statusOut](const std::string& text) {
            if (statusOut != nullptr) {
                *statusOut += text;
            }
        };

        const ri::editor::LevelImportResult lightingImport = TryImportAssemblyLightingCsv(
            starterScene_.scene, starterScene_.handles.root, gameRoot / "levels" / "assembly.lighting.csv");
        if (lightingImport.success && lightingImport.importedCount > 0U) {
            appendStatus("  Imported " + std::to_string(lightingImport.importedCount) + " light(s).");
        } else if (!lightingImport.error.empty() && lightingImport.error != "lighting CSV not found") {
            appendStatus("  Lighting import: " + lightingImport.error + ".");
        }

        const ri::editor::LevelImportResult colliderImport = TryImportAssemblyCollidersCsv(
            starterScene_.scene, starterScene_.handles.root, gameRoot / "levels" / "assembly.colliders.csv");
        if (colliderImport.success && colliderImport.importedCount > 0U) {
            appendStatus("  Imported " + std::to_string(colliderImport.importedCount) + " collider proxy cube(s).");
        } else if (!colliderImport.error.empty() && colliderImport.error != "colliders CSV not found") {
            appendStatus("  Collider import: " + colliderImport.error + ".");
        }

        const ri::editor::LevelImportResult triggerImport = TryImportAssemblyTriggersCsv(
            starterScene_.scene, starterScene_.handles.root, gameRoot / "levels" / "assembly.triggers.csv");
        if (triggerImport.success && triggerImport.importedCount > 0U) {
            appendStatus("  Imported " + std::to_string(triggerImport.importedCount) + " trigger volume(s).");
        } else if (!triggerImport.error.empty() && triggerImport.error != "triggers CSV not found") {
            appendStatus("  Trigger import: " + triggerImport.error + ".");
        }

        if (starterScene_.scene.NodeCount() > nodeCountBefore) {
            authoredNodeStart_ = std::min(authoredNodeStart_, nodeCountBefore);
            autosavePending_ = true;
        }
    }

    void TryImportPrimaryLevelCsv() {
        if (!sceneConfig_.gameManifest.has_value()) {
            lastIoStatus_ = "Import skipped: no game manifest mounted.";
            return;
        }
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Import skipped: scene has no world root.";
            return;
        }
        const fs::path gameRoot = sceneConfig_.gameManifest->rootPath;
        const std::string primaryLevel = sceneConfig_.gameManifest->primaryLevel.empty()
            ? std::string("levels/assembly.primitives.csv")
            : sceneConfig_.gameManifest->primaryLevel;
        const fs::path levelPath = gameRoot / primaryLevel;
        std::error_code ec{};
        if (!fs::exists(levelPath, ec)) {
            lastIoStatus_ = "Import skipped: primary level CSV not found at " + primaryLevel + ".";
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
            lastIoStatus_ = "Level CSV import failed: " + importError;
            return;
        }
        if (importStart < starterScene_.scene.NodeCount()) {
            authoredNodeStart_ = std::min(authoredNodeStart_, importStart);
        }
        lastIoStatus_ = "Imported " + std::to_string(importResult.spawnedCount) + " primitives from "
            + primaryLevel + ".";
        TryImportLevelCsvSupplement(&lastIoStatus_);
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
        std::string levelImportNote;
        if (importCsv) {
            TryImportPrimaryLevelCsv();
        } else {
            TryImportLevelCsvSupplement(&levelImportNote);
        }
        logicLayer_.EnsureKitLoaded(sceneConfig_.workspaceRoot);
        logicLayer_.EnsureGameColliderTrace(manifest->rootPath);
        ri::editor::BindAuthoringLogicCatalog(&logicLayer_);
        structuralThumbnailCache_.Clear();
        RefreshViewportPreviewConfiguration(true);
        ClearViewportPreviewCache();

        if (hwnd_ != nullptr) {
            SetWindowTextW(hwnd_, Widen(sceneConfig_.windowTitle).c_str());
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        lastIoStatus_ = "Switched project: " + game.displayName + " (preview reloaded). Use Ctrl+S to persist edits.";
        if (!levelImportNote.empty()) {
            lastIoStatus_ += levelImportNote;
        }
        RefreshPluginStoreState();
    }

    void RefreshPluginStoreState() {
        pluginStorePackages_ = ri::editor::ListPluginStorePackages(sceneConfig_.workspaceRoot);
        ClampPluginStoreScrollRow();
        if (sceneConfig_.gameManifest.has_value()) {
            const ri::content::GamePluginBootstrap bootstrap =
                ri::content::BootstrapGamePlugins(sceneConfig_.gameManifest->rootPath);
            pluginProjectData_ = bootstrap.projectData;
            pluginStoreStatusLine_.clear();
            if (bootstrap.startupHooksExecuted > 0U) {
                pluginStoreStatusLine_ = std::to_string(bootstrap.startupHooksExecuted) + " startup hook(s) dispatched.";
            }
            if (!pluginProjectData_.issues.empty()) {
                if (!pluginStoreStatusLine_.empty()) {
                    pluginStoreStatusLine_ += " ";
                }
                pluginStoreStatusLine_ += std::to_string(pluginProjectData_.issues.size()) + " validation issue(s).";
            }
            for (const ri::content::PluginHookResult& result : bootstrap.startupResults) {
                if (result.handled && pluginStoreStatusLine_.size() < 180U) {
                    pluginStoreStatusLine_ += " [" + result.pluginId + "]";
                }
            }
        } else {
            pluginProjectData_ = {};
            pluginStoreStatusLine_.clear();
        }
    }

    [[nodiscard]] PluginStorePanelModel BuildPluginStorePanelModel() const {
        PluginStorePanelModel model{};
        model.headingLine = "Plugin Store";
        model.hasMountedGame = sceneConfig_.gameManifest.has_value();
        model.summaryLine = model.hasMountedGame
            ? ri::content::SummarizePluginProjectData(pluginProjectData_)
            : "No game mounted — pick a project from the workspace strip.";
        model.modelHelpLine = "Policy → manifest → hooks → runtime handlers (declarative mod pipeline).";
        if (model.hasMountedGame && !pluginProjectData_.activePlugins.empty()) {
            model.summaryLine += " · active:";
            for (const ri::content::ActivePlugin& active : pluginProjectData_.activePlugins) {
                model.summaryLine += " " + active.manifest.id;
            }
        }
        model.storePathLine = "Store: " + ri::editor::ResolvePluginStoreRoot(sceneConfig_.workspaceRoot).string();
        model.statusLine = pluginStoreStatusLine_;
        model.scrollTopRow = pluginStoreScrollRow_;

        for (std::size_t index = 0; index < pluginStorePackages_.size(); ++index) {
            const PluginStorePackage& package = pluginStorePackages_[index];
            PluginStoreCardModel card{};
            card.packageIndex = static_cast<int>(index);
            card.titleLine = (package.badge.empty() ? "" : package.badge + "  ") + package.name + "  v" + package.version;
            card.metaLine = package.author + " · " + package.category + " · source: project";
            card.tagLine = package.tagLine.empty() ? package.id : package.tagLine;
            card.descriptionLine = package.description;
            card.installed = model.hasMountedGame && ri::editor::IsPluginInstalled(pluginProjectData_, package.id);
            card.enabled = false;
            if (card.installed) {
                for (const ri::content::PluginRegistryEntry& entry : pluginProjectData_.registryEntries) {
                    if (entry.id == package.id) {
                        card.enabled = entry.enabled;
                        break;
                    }
                }
                if (const ri::content::PluginManifestEntry* manifest =
                        ri::content::FindPluginManifestEntry(pluginProjectData_, package.id)) {
                    card.policyLine = "Policy: " + std::string(ri::content::ToString(manifest->sourceKind));
                    if (manifest->blockedByPolicy) {
                        card.blocked = true;
                        card.statusLine = "Blocked by policy";
                        card.policyLine += " · " + manifest->policyBlockReason;
                    }
                }
                if (!card.blocked) {
                    card.statusLine = card.enabled ? "Installed · enabled" : "Installed · disabled";
                }
                card.actionLabel = card.blocked ? "Blocked" : (card.enabled ? "Disable" : "Enable");
                card.secondaryActionLabel = "Remove";
            } else {
                if (model.hasMountedGame && !pluginProjectData_.policy.allowProjectPlugins) {
                    card.blocked = true;
                    card.policyLine = "Policy: project · project plugins disabled";
                    card.statusLine = "Install blocked by project policy";
                    card.actionLabel = "Blocked";
                } else {
                    card.policyLine = "Policy: project";
                    card.actionLabel = "Install";
                }
            }
            model.cards.push_back(std::move(card));
        }
        if (!pluginStorePackages_.empty()) {
            model.scrollLine = "Showing " + std::to_string(std::min(pluginStorePackages_.size(),
                                                                      static_cast<std::size_t>(pluginStoreScrollRow_ + 3)))
                + " of " + std::to_string(pluginStorePackages_.size()) + " packages";
        }
        return model;
    }

    void ClampPluginStoreScrollRow() {
        pluginStoreScrollRow_ = std::max(0, pluginStoreScrollRow_);
        if (pluginStorePackages_.size() <= 3U) {
            pluginStoreScrollRow_ = 0;
        } else if (pluginStoreScrollRow_ > static_cast<int>(pluginStorePackages_.size()) - 1) {
            pluginStoreScrollRow_ = std::max(0, static_cast<int>(pluginStorePackages_.size()) - 3);
        }
    }

    void TryInstallStorePlugin(const int cardIndex) {
        if (!sceneConfig_.gameManifest.has_value()) {
            pluginStoreStatusLine_ = "Mount a game project before installing plugins.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        if (cardIndex < 0 || cardIndex >= static_cast<int>(pluginStorePackages_.size())) {
            return;
        }
        if (!pluginProjectData_.policy.allowProjectPlugins) {
            pluginStoreStatusLine_ = "Install blocked: project plugins are disabled by policy.";
            lastIoStatus_ = pluginStoreStatusLine_;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        const ri::editor::PluginInstallResult result = ri::editor::InstallPluginStorePackage(
            sceneConfig_.gameManifest->rootPath,
            pluginStorePackages_[static_cast<std::size_t>(cardIndex)]);
        pluginStoreStatusLine_ = result.message;
        lastIoStatus_ = result.message;
        RefreshPluginStoreState();
        RefreshWorkspaceResourceRows();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void TryToggleStorePlugin(const int cardIndex) {
        if (!sceneConfig_.gameManifest.has_value()) {
            return;
        }
        if (cardIndex < 0 || cardIndex >= static_cast<int>(pluginStorePackages_.size())) {
            return;
        }
        const PluginStorePackage& package = pluginStorePackages_[static_cast<std::size_t>(cardIndex)];
        if (const ri::content::PluginManifestEntry* manifest =
                ri::content::FindPluginManifestEntry(pluginProjectData_, package.id);
            manifest != nullptr && manifest->blockedByPolicy) {
            pluginStoreStatusLine_ = "Toggle blocked: " + manifest->policyBlockReason + ".";
            lastIoStatus_ = pluginStoreStatusLine_;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        bool enabled = false;
        for (const ri::content::PluginRegistryEntry& entry : pluginProjectData_.registryEntries) {
            if (entry.id == package.id) {
                enabled = entry.enabled;
                break;
            }
        }
        const ri::editor::PluginInstallResult result = ri::editor::SetPluginEnabled(
            sceneConfig_.gameManifest->rootPath,
            package.id,
            !enabled);
        pluginStoreStatusLine_ = result.message;
        lastIoStatus_ = result.message;
        RefreshPluginStoreState();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void TryPluginStoreCardAction(const int cardIndex) {
        if (cardIndex < 0 || cardIndex >= static_cast<int>(pluginStorePackages_.size())) {
            return;
        }
        const PluginStorePackage& package = pluginStorePackages_[static_cast<std::size_t>(cardIndex)];
        if (sceneConfig_.gameManifest.has_value()
            && ri::editor::IsPluginInstalled(pluginProjectData_, package.id)) {
            TryToggleStorePlugin(cardIndex);
        } else {
            TryInstallStorePlugin(cardIndex);
        }
    }

    void TryUninstallStorePlugin(const int cardIndex) {
        if (!sceneConfig_.gameManifest.has_value()) {
            return;
        }
        if (cardIndex < 0 || cardIndex >= static_cast<int>(pluginStorePackages_.size())) {
            return;
        }
        const PluginStorePackage& package = pluginStorePackages_[static_cast<std::size_t>(cardIndex)];
        const ri::editor::PluginInstallResult result = ri::editor::UninstallPluginStorePackage(
            sceneConfig_.gameManifest->rootPath,
            package.id);
        pluginStoreStatusLine_ = result.message;
        lastIoStatus_ = result.message;
        RefreshPluginStoreState();
        ClampPluginStoreScrollRow();
        RefreshWorkspaceResourceRows();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void OpenPluginStoreFolder() const {
#if defined(_WIN32)
        const fs::path storeRoot = ri::editor::ResolvePluginStoreRoot(sceneConfig_.workspaceRoot);
        std::error_code ec{};
        fs::create_directories(storeRoot, ec);
        const std::wstring path = storeRoot.wstring();
        ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    }

    void SwitchFocusedWorkspaceGame() {
        ReloadEditorSceneForFocusedGame(true);
        RefreshWorkspaceResourceRows();
        RebuildFilteredHierarchyOrder();
    }

    void TryScaffoldMountedGame() {
        if (!sceneConfig_.gameManifest.has_value()) {
            lastIoStatus_ = "Setup Files needs an open game manifest.";
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
        RefreshPluginStoreState();
        RebuildFilteredResourceRows();
        if (createdCount == 0) {
            lastIoStatus_ = "Project scaffold already present. Resources refreshed.";
            return;
        }
        lastIoStatus_ = "Created " + std::to_string(createdCount) + " missing authoring files.";
    }

    [[nodiscard]] std::string CurrentNewGameDisplayName() const {
        std::string name = DefaultDisplayNameForTemplate(newGameTemplate_);
        if (newGameNameVariant_ > 0) {
            name += " " + std::to_string(newGameNameVariant_ + 1);
        }
        return name;
    }

    void CycleNewGameNameVariant(const int delta) {
        newGameNameVariant_ = std::max(0, newGameNameVariant_ + delta);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] ri::editor::CreatorPanelModel BuildCreatorPanelModel() const {
        const std::string displayName = CurrentNewGameDisplayName();
        return ri::editor::CreatorPanelModel{
            .selectedTemplate = newGameTemplate_,
            .displayNameDraft = displayName,
            .slugPreview = SlugFromDisplayName(displayName),
            .mountedGameLabel = FocusedWorkspaceGameLabel(),
            .hasMountedGame = sceneConfig_.gameManifest.has_value(),
            .selectedAtmosphere = creatorAtmospherePreset_,
            .selectedInsert = creatorInsertPreset_,
            .selectedCamera = creatorCameraPreset_,
        };
    }

    void MountWorkspaceGameById(const std::string& gameId) {
        for (std::size_t i = 0; i < workspaceGames_.size(); ++i) {
            if (workspaceGames_[i].id == gameId) {
                focusedWorkspaceGameIndex_ = static_cast<int>(i);
                SwitchFocusedWorkspaceGame();
                return;
            }
        }
    }

    void TryCreateNewGameFromWizard() {
        const NewGameCreationResult created = CreateNewGameProject(
            sceneConfig_.workspaceRoot, newGameTemplate_, CurrentNewGameDisplayName());
        if (!created.ok) {
            lastIoStatus_ = created.error.empty() ? "Could not create new game project." : created.error;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        RefreshWorkspaceGamesAndResources();
        MountWorkspaceGameById(created.manifest.id);
        leftPanelMode_ = LeftPanelMode::Scene;
        newGameNameVariant_ = 0;

        CreatorAtmospherePreset atmosphere = CreatorAtmospherePreset::ClearDay;
        if (newGameTemplate_ == NewGameTemplate::OutdoorScene) {
            atmosphere = CreatorAtmospherePreset::GoldenHour;
        } else if (newGameTemplate_ == NewGameTemplate::InteriorRoom) {
            atmosphere = CreatorAtmospherePreset::NightStudio;
        }
        std::string atmosphereError;
        if (!ApplyAtmospherePreset(created.projectRoot, atmosphere, &atmosphereError)) {
            lastIoStatus_ = "Created " + created.manifest.name + ", but atmosphere setup failed: " + atmosphereError;
        } else {
            lastIoStatus_ = "Created game '" + created.manifest.name + "' under Games/. Scene and atmosphere are ready.";
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void ApplyCreatorAtmosphere(const CreatorAtmospherePreset preset) {
        if (!sceneConfig_.gameManifest.has_value()) {
            lastIoStatus_ = "Mount a game before applying atmosphere presets.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        std::string error;
        if (!ApplyAtmospherePreset(sceneConfig_.gameManifest->rootPath, preset, &error)) {
            lastIoStatus_ = error.empty() ? "Atmosphere preset failed." : error;
        } else {
            lastIoStatus_ = "Applied atmosphere preset: " + AtmospherePresetLabel(preset) + " (viewport sky updated).";
            RefreshWorkspaceResourceRows();
            RefreshViewportPreviewConfiguration(true);
            ClearViewportPreviewCache();
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void AddCreatorPrimitive(const ri::scene::PrimitiveType primitive,
                             const std::string& basename,
                             const ri::math::Vec3& position,
                             const ri::math::Vec3& scale,
                             const ri::math::Vec3& baseColor,
                             const std::string& materialName) {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot insert object: scene has no world root.";
            return;
        }
        ri::scene::PrimitiveNodeOptions options{};
        options.parent = starterScene_.handles.root;
        options.primitive = primitive;
        options.nodeName = NextAuthoringPrimitiveBasename(basename);
        options.materialName = materialName;
        options.shadingModel = ri::scene::ShadingModel::Lit;
        options.baseColor = baseColor;
        options.baseColorTexture = ri::scene::DefaultStructuralBrushAlbedoTexture();
        options.baseColor = ri::scene::DefaultStructuralBrushBaseColor();
        options.textureTiling = ri::math::Vec2{2.0f, 2.0f};
        options.transform.position = position;
        options.transform.scale = scale;
        const int newHandle = ri::scene::AddPrimitiveNode(starterScene_.scene, options);
        selectedNode_ = static_cast<std::size_t>(newHandle);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
    }

    void ApplyCreatorInsert(const CreatorInsertPreset preset) {
        const ri::math::Vec3 focus = starterScene_.handles.orbitCamera.orbit.target;
        switch (preset) {
            case CreatorInsertPreset::GroundPlate:
                AddCreatorPrimitive(ri::scene::PrimitiveType::Plane,
                                    "Ground_",
                                    ri::math::Vec3{focus.x, 0.0f, focus.z},
                                    ri::math::Vec3{18.0f, 1.0f, 18.0f},
                                    ri::math::Vec3{0.52f, 0.56f, 0.48f},
                                    "creator_ground");
                lastIoStatus_ = "Inserted ground plate at world origin height.";
                break;
            case CreatorInsertPreset::RockCluster:
                AddCreatorPrimitive(ri::scene::PrimitiveType::Cube,
                                    "Rock_",
                                    ri::math::Vec3{focus.x + 2.0f, 0.8f, focus.z + 1.5f},
                                    ri::math::Vec3{1.4f, 1.1f, 1.2f},
                                    ri::math::Vec3{0.44f, 0.42f, 0.40f},
                                    "creator_rock");
                AddCreatorPrimitive(ri::scene::PrimitiveType::Cube,
                                    "Rock_",
                                    ri::math::Vec3{focus.x - 1.5f, 0.6f, focus.z - 2.0f},
                                    ri::math::Vec3{1.8f, 0.9f, 1.5f},
                                    ri::math::Vec3{0.48f, 0.46f, 0.44f},
                                    "creator_rock");
                AddCreatorPrimitive(ri::scene::PrimitiveType::Cube,
                                    "Rock_",
                                    ri::math::Vec3{focus.x + 3.0f, 0.5f, focus.z - 1.0f},
                                    ri::math::Vec3{1.0f, 0.8f, 0.9f},
                                    ri::math::Vec3{0.40f, 0.38f, 0.36f},
                                    "creator_rock");
                lastIoStatus_ = "Inserted rock cluster near camera focus.";
                break;
            case CreatorInsertPreset::WaterSurface:
                AddCreatorPrimitive(ri::scene::PrimitiveType::Plane,
                                    "Water_",
                                    ri::math::Vec3{focus.x, -0.05f, focus.z},
                                    ri::math::Vec3{20.0f, 1.0f, 20.0f},
                                    ri::math::Vec3{0.18f, 0.34f, 0.52f},
                                    "creator_water");
                lastIoStatus_ = "Inserted water surface plane.";
                break;
            case CreatorInsertPreset::SkyBackdrop:
                AddCreatorPrimitive(ri::scene::PrimitiveType::Plane,
                                    "Sky_",
                                    ri::math::Vec3{focus.x, 16.0f, focus.z - 12.0f},
                                    ri::math::Vec3{28.0f, 1.0f, 18.0f},
                                    ri::math::Vec3{0.58f, 0.72f, 0.92f},
                                    "creator_sky");
                lastIoStatus_ = "Inserted sky backdrop slab.";
                break;
            case CreatorInsertPreset::PortalArch:
                AddCreatorPrimitive(ri::scene::PrimitiveType::Cube,
                                    "Arch_",
                                    ri::math::Vec3{focus.x - 1.2f, 1.5f, focus.z},
                                    ri::math::Vec3{0.4f, 3.0f, 0.4f},
                                    ri::math::Vec3{0.62f, 0.64f, 0.68f},
                                    "creator_arch");
                AddCreatorPrimitive(ri::scene::PrimitiveType::Cube,
                                    "Arch_",
                                    ri::math::Vec3{focus.x + 1.2f, 1.5f, focus.z},
                                    ri::math::Vec3{0.4f, 3.0f, 0.4f},
                                    ri::math::Vec3{0.62f, 0.64f, 0.68f},
                                    "creator_arch");
                AddCreatorPrimitive(ri::scene::PrimitiveType::Cube,
                                    "Arch_",
                                    ri::math::Vec3{focus.x, 2.8f, focus.z},
                                    ri::math::Vec3{2.8f, 0.4f, 0.5f},
                                    ri::math::Vec3{0.66f, 0.68f, 0.72f},
                                    "creator_arch");
                lastIoStatus_ = "Inserted portal arch near camera focus.";
                break;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void ApplyCreatorCamera(const CreatorCameraPreset preset) {
        switch (preset) {
            case CreatorCameraPreset::Hero:
                editorOrbitState_.yawDegrees = 28.0f;
                editorOrbitState_.pitchDegrees = -16.0f;
                editorOrbitState_.distance = 11.0f;
                editorOrbitState_.target = ri::math::Vec3{0.0f, 1.0f, 0.0f};
                lastIoStatus_ = "Camera preset: Hero framing.";
                break;
            case CreatorCameraPreset::TopDown:
                editorOrbitState_.yawDegrees = 0.0f;
                editorOrbitState_.pitchDegrees = -78.0f;
                editorOrbitState_.distance = 20.0f;
                editorOrbitState_.target = ri::math::Vec3{0.0f, 0.0f, 0.0f};
                lastIoStatus_ = "Camera preset: Top-down overview.";
                break;
            case CreatorCameraPreset::LowAngle:
                editorOrbitState_.yawDegrees = -18.0f;
                editorOrbitState_.pitchDegrees = -7.0f;
                editorOrbitState_.distance = 6.5f;
                editorOrbitState_.target = ri::math::Vec3{0.0f, 0.6f, 0.0f};
                lastIoStatus_ = "Camera preset: Low dramatic angle.";
                break;
        }
        ApplyEditorOrbitToScene();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool ShouldShowAuthoringCatalogChrome() const {
        return leftPanelMode_ == LeftPanelMode::Create || inspectorPanel_ == InspectorPanel::Brush
            || toolMode_ == ri::editor::EditorToolMode::Create;
    }

    [[nodiscard]] bool ShouldShowViewportWorldBar() const {
        return sceneConfig_.gameManifest.has_value();
    }

    [[nodiscard]] std::string ArmedCatalogPresetSummary() const {
        return ri::editor::ActiveCatalogPresetLabel(authoringCatalogSection_,
                                                    SelectedCatalogPresetIndex(authoringCatalogSection_));
    }

    [[nodiscard]] std::string CreateModeHintLine() const {
        if (toolMode_ != ri::editor::EditorToolMode::Create) {
            return {};
        }
        return "Stamp: " + ArmedCatalogPresetSummary() + "  |  click scene  |  Place / dbl-click preset";
    }

    void SetToolMode(const ri::editor::EditorToolMode mode) {
        toolMode_ = mode;
        if (mode == ri::editor::EditorToolMode::Create) {
            leftPanelMode_ = LeftPanelMode::Scene;
            authoringCatalogExpanded_ = true;
            (void)SetInspectorPanel(InspectorPanel::Brush);
            lastIoStatus_ = "Create mode (Bryce-style): pick a preset below, then click the scene to stamp it.";
        } else if (mode == ri::editor::EditorToolMode::Select) {
            lastIoStatus_ = "Select mode: click objects to edit transforms and properties.";
        } else {
            lastIoStatus_ = "Camera mode: drag to orbit and pan without stamping objects.";
        }
        ClearViewportPreviewCache();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool ShouldShowStructuralPicker() const {
        return ShouldShowAuthoringCatalogChrome() && authoringCatalogExpanded_;
    }

    [[nodiscard]] int AuthoringCatalogBottomChromeInset() const {
        if (!ShouldShowAuthoringCatalogChrome()) {
            return 0;
        }
        return AuthoringCatalogBottomInset(authoringCatalogExpanded_);
    }

    [[nodiscard]] bool TryHandleAuthoringCatalogClick(const POINT& point, const RECT& viewportInner) {
        if (!ShouldShowAuthoringCatalogChrome()) {
            return false;
        }
        if (!authoringCatalogExpanded_) {
            const RECT barRect = ComputeStructuralPickerCollapsedBarRect(viewportInner);
            if (HitTestStructuralPickerCollapsedBar(barRect, point).kind == StructuralPickerHitKind::ToggleExpand) {
                authoringCatalogExpanded_ = true;
                ClearViewportPreviewCache();
                lastIoStatus_ = "Authoring catalog expanded (Ctrl+2 toggles).";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            }
            return false;
        }
        return HandleStructuralPickerClick(point, viewportInner);
    }

    [[nodiscard]] const fs::path& ResolveEditorTextureRoot() const {
        return editorTextureRoot_;
    }

    [[nodiscard]] StructuralPickerLayout CurrentStructuralPickerLayout(const RECT& viewportInner) const {
        return ComputeStructuralPickerLayout(viewportInner, authoringCatalogSection_, structuralPickerScrollRow_);
    }

    [[nodiscard]] std::size_t& SelectedCatalogPresetIndex(const ri::editor::AuthoringCatalogSection section) {
        switch (section) {
            case ri::editor::AuthoringCatalogSection::Volumes:
                return volumeCatalogPresetIndex_;
            case ri::editor::AuthoringCatalogSection::Logic:
                return logicCatalogPresetIndex_;
            case ri::editor::AuthoringCatalogSection::Structural:
                break;
        }
        return structuralBrushPresetIndex_;
    }

    [[nodiscard]] std::size_t SelectedCatalogPresetIndex(const ri::editor::AuthoringCatalogSection section) const {
        switch (section) {
            case ri::editor::AuthoringCatalogSection::Volumes:
                return volumeCatalogPresetIndex_;
            case ri::editor::AuthoringCatalogSection::Logic:
                return logicCatalogPresetIndex_;
            case ri::editor::AuthoringCatalogSection::Structural:
                break;
        }
        return structuralBrushPresetIndex_;
    }

    void SwitchAuthoringCatalogSection(const ri::editor::AuthoringCatalogSection section) {
        if (authoringCatalogSection_ == section) {
            return;
        }
        authoringCatalogSection_ = section;
        structuralPickerScrollRow_ = 0;
        lastIoStatus_ = std::string("Authoring catalog: ") + std::string(ri::editor::AuthoringCatalogSectionLabel(section))
            + " tab.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] bool HandleStructuralPickerClick(const POINT& point, const RECT& viewportInner) {
        if (!ShouldShowStructuralPicker()) {
            return false;
        }
        const StructuralPickerLayout layout = CurrentStructuralPickerLayout(viewportInner);
        const ri::editor::StructuralPickerHit hit = HitTestStructuralPicker(layout, point);
        switch (hit.kind) {
            case StructuralPickerHitKind::Preset:
                SelectedCatalogPresetIndex(authoringCatalogSection_) = hit.presetIndex;
                lastIoStatus_ = "Selected " + std::string(ri::editor::AuthoringCatalogSectionLabel(authoringCatalogSection_))
                    + " preset: " + ri::editor::ActiveCatalogPresetLabel(authoringCatalogSection_, hit.presetIndex)
                    + "  |  click scene to stamp · Place · dbl-click preset.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            case StructuralPickerHitKind::SectionStructural:
                SwitchAuthoringCatalogSection(ri::editor::AuthoringCatalogSection::Structural);
                return true;
            case StructuralPickerHitKind::SectionVolumes:
                SwitchAuthoringCatalogSection(ri::editor::AuthoringCatalogSection::Volumes);
                return true;
            case StructuralPickerHitKind::SectionLogic:
                SwitchAuthoringCatalogSection(ri::editor::AuthoringCatalogSection::Logic);
                return true;
            case StructuralPickerHitKind::PrevPage:
                structuralPickerScrollRow_ = std::max(0, structuralPickerScrollRow_ - 1);
                lastIoStatus_ = "Authoring catalog: previous page.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            case StructuralPickerHitKind::NextPage:
                structuralPickerScrollRow_ =
                    std::min(std::max(0, layout.totalRows - layout.visibleRows), structuralPickerScrollRow_ + 1);
                lastIoStatus_ = "Authoring catalog: next page.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            case StructuralPickerHitKind::Place:
                SpawnAuthoringCatalogAtFocus();
                return true;
            case StructuralPickerHitKind::ToggleExpand:
                authoringCatalogExpanded_ = false;
                ClearViewportPreviewCache();
                lastIoStatus_ = "Authoring catalog collapsed — viewport enlarged (Ctrl+2 toggles).";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            case StructuralPickerHitKind::None:
                break;
        }
        return false;
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

    void OpenStartupAssetHandoff() {
        if (sceneConfig_.startupAssetPath.empty()) {
            return;
        }
        const ri::content::AuthoringHandoffReport handoff = ri::content::BuildAuthoringHandoff({
            .workspaceRoot = sceneConfig_.workspaceRoot,
            .assetPath = sceneConfig_.startupAssetPath,
            .gameId = sceneConfig_.gameManifest.has_value() ? sceneConfig_.gameManifest->id : std::string{},
        });
        if (!handoff.valid) {
            lastIoStatus_ += "  Forge handoff rejected: "
                + (handoff.issues.empty() ? std::string("unknown validation error") : handoff.issues.front());
            return;
        }

        int rowIndex = -1;
        for (int index = 0; index < static_cast<int>(resourceCatalogEntries_.size()); ++index) {
            if (resourceCatalogEntries_[static_cast<std::size_t>(index)].absolutePath == handoff.assetPath) {
                rowIndex = index;
                break;
            }
        }
        if (rowIndex < 0) {
            resourceCatalogEntries_.insert(resourceCatalogEntries_.begin(), ri::editor::WorkspaceResourceEntry{
                .absolutePath = handoff.assetPath,
                .relativePathUtf8 = handoff.workspaceRelativePath.generic_string(),
                .category = ri::editor::WorkspaceResourceCategory::Asset,
            });
            rowIndex = 0;
            RebuildFilteredResourceRows();
        }
        leftPanelMode_ = LeftPanelMode::Resources;
        SelectWorkspaceResourceRow(rowIndex);
        inspectorPanel_ = InspectorPanel::Files;
        lastIoStatus_ = "Forge handoff opened " + handoff.workspaceRelativePath.generic_string()
            + " (" + std::string(ri::content::ToString(handoff.assetKind)) + ").";
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
        model.headingLine = "Menu / UI / VN workbench";
        model.usingAutoSource = uiWorkbenchSource_ == UiWorkbenchSource::AutoSelection;
        model.usingMenuSample = uiWorkbenchSource_ == UiWorkbenchSource::MenuSample;
        model.usingVnSample = uiWorkbenchSource_ == UiWorkbenchSource::VnSample;
        model.hintLine = "Menu blocks: + Button / Heading / Paragraph / Spacer. Auto mode stays inside the mounted game.";
        model.actionsHeaderLine = "Authoring actions write directly into the selected game-local manifest.";
        model.blockActionsHeaderLine = "Use New Menu for a title screen template. F2 edits labels and text in-place.";
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
                    preview.preferredHeight = 40;
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
                    preview.preferredHeight = std::clamp(static_cast<int>(block.spacerHeight), 12, 64);
                    break;
                case ri::ui::UiBlockKind::Separator:
                    preview.tone = UiWorkbenchBlockTone::Other;
                    preview.titleLine = "Separator";
                    break;
                case ri::ui::UiBlockKind::Button:
                    preview.tone = UiWorkbenchBlockTone::Button;
                    preview.titleLine = "Button";
                    preview.detailLine = block.label.empty() ? "Untitled button" : block.label;
                    preview.preferredHeight = 44;
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
        const UiWorkbenchInspectorLayout cards = ComputeUiWorkbenchInspectorLayout(inspectorInner);
        const std::vector<RECT> screenRows =
            ComputeUiWorkbenchScreenRowRects(cards, static_cast<int>(model.screens.size()));
        for (std::size_t i = 0; i < screenRows.size(); ++i) {
            if (PtInRect(&screenRows[i], point) != FALSE) {
                SelectUiWorkbenchScreen(static_cast<int>(i));
                return true;
            }
        }
        const std::vector<RECT> blockRows = ComputeUiWorkbenchInspectorPreviewBlockRects(cards, model.previewBlocks);
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

    [[nodiscard]] static ri::ui::UiBlock MakeMenuButtonBlock(std::string label, ri::ui::UiAction action) {
        ri::ui::UiBlock block{};
        block.kind = ri::ui::UiBlockKind::Button;
        block.label = std::move(label);
        block.action = std::move(action);
        return block;
    }

    void UiWorkbenchCreateMenuScreen() {
        ApplyUiWorkbenchMutation("Menu workbench: created menu screen.", [this](ri::ui::UiManifest& manifest) {
            ri::ui::UiScreen screen{};
            const int nextIndex = static_cast<int>(manifest.screens.size()) + 1;
            screen.id = "menu_" + std::to_string(nextIndex);
            screen.title = "Menu " + std::to_string(nextIndex);
            screen.backgroundRgba = {0.04f, 0.05f, 0.10f, 0.98f};
            screen.blocks.push_back(ri::ui::UiBlock{
                .kind = ri::ui::UiBlockKind::Heading,
                .text = screen.title,
                .align = "center",
            });
            screen.blocks.push_back(ri::ui::UiBlock{
                .kind = ri::ui::UiBlockKind::Spacer,
                .spacerHeight = 24.0f,
            });
            screen.blocks.push_back(ri::ui::UiBlock{
                .kind = ri::ui::UiBlockKind::Paragraph,
                .text = "Replace this subtitle with your game pitch or chapter title.",
                .align = "center",
            });
            screen.blocks.push_back(ri::ui::UiBlock{
                .kind = ri::ui::UiBlockKind::Spacer,
                .spacerHeight = 32.0f,
            });
            screen.blocks.push_back(MakeMenuButtonBlock(
                "Play",
                ri::ui::UiAction{.kind = ri::ui::UiActionKind::Emit, .target = "game.start"}));
            screen.blocks.push_back(MakeMenuButtonBlock(
                "Settings",
                ri::ui::UiAction{.kind = ri::ui::UiActionKind::Navigate, .target = screen.id}));
            screen.blocks.push_back(MakeMenuButtonBlock(
                "Quit",
                ri::ui::UiAction{.kind = ri::ui::UiActionKind::Emit, .target = "app.quit"}));
            manifest.screens.push_back(std::move(screen));
            uiWorkbenchSelectedScreenIndex_ = static_cast<int>(manifest.screens.size()) - 1;
            uiWorkbenchSelectedBlockIndex_ = 0;
            if (manifest.startScreenId.empty()) {
                manifest.startScreenId = manifest.screens.back().id;
            }
            return true;
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

    void UiWorkbenchAddButtonBlock() {
        ApplyUiWorkbenchMutation("Menu workbench: added button block.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "Menu workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            blocks.push_back(MakeMenuButtonBlock(
                "New Button",
                ri::ui::UiAction{.kind = ri::ui::UiActionKind::Emit, .target = "game.start"}));
            uiWorkbenchSelectedBlockIndex_ = static_cast<int>(blocks.size()) - 1;
            return true;
        });
    }

    void UiWorkbenchAddHeadingBlock() {
        ApplyUiWorkbenchMutation("Menu workbench: added heading block.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "Menu workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            ri::ui::UiBlock block{};
            block.kind = ri::ui::UiBlockKind::Heading;
            block.text = "New Heading";
            block.align = "center";
            blocks.push_back(std::move(block));
            uiWorkbenchSelectedBlockIndex_ = static_cast<int>(blocks.size()) - 1;
            return true;
        });
    }

    void UiWorkbenchAddParagraphBlock() {
        ApplyUiWorkbenchMutation("Menu workbench: added paragraph block.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "Menu workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            ri::ui::UiBlock block{};
            block.kind = ri::ui::UiBlockKind::Paragraph;
            block.text = "New paragraph text.";
            block.align = "center";
            blocks.push_back(std::move(block));
            uiWorkbenchSelectedBlockIndex_ = static_cast<int>(blocks.size()) - 1;
            return true;
        });
    }

    void UiWorkbenchAddSpacerBlock() {
        ApplyUiWorkbenchMutation("Menu workbench: added spacer block.", [this](ri::ui::UiManifest& manifest) {
            if (manifest.screens.empty()) {
                lastIoStatus_ = "Menu workbench: no screen available.";
                return false;
            }
            uiWorkbenchSelectedScreenIndex_ =
                std::clamp(uiWorkbenchSelectedScreenIndex_, 0, static_cast<int>(manifest.screens.size()) - 1);
            auto& blocks = manifest.screens[static_cast<std::size_t>(uiWorkbenchSelectedScreenIndex_)].blocks;
            ri::ui::UiBlock block{};
            block.kind = ri::ui::UiBlockKind::Spacer;
            block.spacerHeight = 24.0f;
            blocks.push_back(std::move(block));
            uiWorkbenchSelectedBlockIndex_ = static_cast<int>(blocks.size()) - 1;
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
        const WorkspaceResourceEntry* selectedResourceEntry = nullptr;
        if (selectedResourceRow_ >= 0 && selectedResourceRow_ < static_cast<int>(resourceCatalogEntries_.size())) {
            selectedResourceEntry = &resourceCatalogEntries_[static_cast<std::size_t>(selectedResourceRow_)];
        }
        const ri::editor::FilesInspectorPanelModel filesPanel = BuildFilesInspectorPanelModelForWindow(
            selectedResourceEntry);
        ri::editor::LayoutResourceTextEditorControl(hwnd_,
                                                    resourceTextEditHwnd_,
                                                    inspectorPanel_ == InspectorPanel::Files,
                                                    loadedResourceAbsolutePath_,
                                                    resourceEditorAuxMessage_,
                                                    inspectorInner,
                                                    filesPanel);
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
        const std::string savedName = loadedResourceAbsolutePath_.filename().string();
        lastIoStatus_ = "Saved resource: " + savedName;
        if (savedName == "rendering.riscript" || savedName == "postprocess.riscript") {
            RefreshViewportPreviewConfiguration(true);
            ClearViewportPreviewCache();
            lastIoStatus_ += " (viewport atmosphere reloaded).";
        }
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

    [[nodiscard]] ri::editor::EditorLeftPanelMode EditorLeftPanelModeForPaint() const {
        if (leftPanelMode_ == LeftPanelMode::Resources) {
            return ri::editor::EditorLeftPanelMode::Resources;
        }
        if (leftPanelMode_ == LeftPanelMode::Create) {
            return ri::editor::EditorLeftPanelMode::Create;
        }
        return ri::editor::EditorLeftPanelMode::Scene;
    }

    [[nodiscard]] int LeftPanelContentTop(const RECT& hierarchyInner) const {
        return LeftPanelContentTop(hierarchyInner, EditorLeftPanelModeForPaint());
    }

    [[nodiscard]] int LeftPanelContentTop(const RECT& hierarchyInner, const ri::editor::EditorLeftPanelMode mode) const {
        return ri::editor::LeftPanelContentTop(hierarchyInner, mode);
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
        return ri::editor::ResourceSearchBoxRect(hierarchyInner);
    }

    [[nodiscard]] RECT ResourceSearchClearRect(const RECT& hierarchyInner) const {
        return ri::editor::ResourceSearchClearRect(hierarchyInner);
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
        const int worldBarHeight = ShouldShowViewportWorldBar() ? kViewportWorldBarHeight_ : 0;
        const int bottomInset = AuthoringCatalogBottomChromeInset();
        const RECT menuBanner{viewportInner.left + 4,
                              viewportInner.top + 6,
                              viewportInner.right - 4,
                              viewportInner.top + 6 + kBannerHeight};
        const RECT quadArea{viewportInner.left + 4,
                            menuBanner.bottom + 4 + worldBarHeight,
                            viewportInner.right - 4,
                            viewportInner.bottom - 4 - kMetaStrip - bottomInset};

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

    void InvalidateViewportAndRail() {
        const EditorLayout layout = ComputeLayout();
        if (layout.cameraRail.right > layout.cameraRail.left) {
            InvalidateRect(hwnd_, &layout.cameraRail, FALSE);
        }
        UpdateCameraPlotRect(layout.viewportInner);
        if (cameraPlotRect_.right > cameraPlotRect_.left && cameraPlotRect_.bottom > cameraPlotRect_.top) {
            InvalidateRect(hwnd_, &cameraPlotRect_, FALSE);
        } else {
            InvalidateRect(hwnd_, &layout.viewport, FALSE);
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
        if (ShouldShowAuthoringCatalogChrome()) {
            const StructuralPickerLayout pickerLayout = CurrentStructuralPickerLayout(layout.viewportInner);
            if (PtInRect(&pickerLayout.panelRect, point) != FALSE) {
                const int rowDelta = wheelDelta > 0 ? -1 : 1;
                structuralPickerScrollRow_ = std::clamp(
                    structuralPickerScrollRow_ + rowDelta,
                    0,
                    std::max(0, pickerLayout.totalRows - pickerLayout.visibleRows));
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            }
        }
        if (PtInRect(&cameraPlotRect_, point) != FALSE) {
            if (!autoOrbitPreview_) {
                const float steps = static_cast<float>(wheelDelta) / static_cast<float>(WHEEL_DELTA);
                const float factor = std::exp(-steps * 0.14f);
                editorOrbitState_.distance *= factor;
                ApplyEditorOrbitToScene();
                viewportPreviewDirty_ = true;
                lastIoStatus_ = "Camera: zoom.";
            }
            InvalidateViewportAndRail();
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
                .onNewGame = [this]() {
                    leftPanelMode_ = LeftPanelMode::Create;
                    lastIoStatus_ = "Creator Lab: pick a template and click Create Game Project.";
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
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
                .onStore = [this]() {
                    (void)SetInspectorPanel(InspectorPanel::PluginStore);
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
        gameplayPanelLayout_ = ComputeGameplayPanelLayout(layout.inspectorInner);
        uiWorkbenchLayout_ = ComputeUiWorkbenchLayout(layout.inspectorInner);
        pluginStoreLayout_ = ComputePluginStoreLayout(
            layout.inspectorInner,
            static_cast<int>(pluginStorePackages_.size()),
            pluginStoreScrollRow_);
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
                .pluginStoreLayout = pluginStoreLayout_,
                .projectShortcuts = shortcuts,
                .primaryLevelShortcutPath = primaryLevelShortcutPath,
                .brushPanelActive = inspectorPanel_ == InspectorPanel::Brush,
                .filesPanelActive = inspectorPanel_ == InspectorPanel::Files,
                .gameplayPanelActive = inspectorPanel_ == InspectorPanel::Gameplay,
                .pluginStoreActive = inspectorPanel_ == InspectorPanel::PluginStore,
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
                .onRefreshPluginStore = [this]() {
                    RefreshPluginStoreState();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onOpenPluginStoreFolder = [this]() {
                    OpenPluginStoreFolder();
                },
                .onPluginStoreScrollPrev = [this]() {
                    pluginStoreScrollRow_ = std::max(0, pluginStoreScrollRow_ - 1);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onPluginStoreScrollNext = [this]() {
                    pluginStoreScrollRow_ += 1;
                    ClampPluginStoreScrollRow();
                    InvalidateRect(hwnd_, nullptr, FALSE);
                },
                .onPluginStoreAction = [this](const int cardIndex) {
                    TryPluginStoreCardAction(cardIndex);
                },
                .onPluginStoreUninstall = [this](const int cardIndex) {
                    TryUninstallStorePlugin(cardIndex);
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
                .onUiWorkbenchNewMenuScreen = [this]() {
                    UiWorkbenchCreateMenuScreen();
                },
                .onUiWorkbenchDuplicateScreen = [this]() {
                    UiWorkbenchDuplicateScreen();
                },
                .onUiWorkbenchAddButtonBlock = [this]() {
                    UiWorkbenchAddButtonBlock();
                },
                .onUiWorkbenchAddHeadingBlock = [this]() {
                    UiWorkbenchAddHeadingBlock();
                },
                .onUiWorkbenchAddParagraphBlock = [this]() {
                    UiWorkbenchAddParagraphBlock();
                },
                .onUiWorkbenchAddSpacerBlock = [this]() {
                    UiWorkbenchAddSpacerBlock();
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

    void ShowHelpGuide() {
        ri::editor::ShowEditorHelpDialog(hwnd_);
    }

    [[nodiscard]] bool HandleGlobalEditorHotkeys(WPARAM key, const bool controlHeld, const bool shiftHeld, const bool altHeld) {
        if (key == VK_F1) {
            ShowHelpGuide();
            return true;
        }
        if (key == 'S' && !controlHeld && !shiftHeld && !altHeld && !resourceSearchActive_) {
            SetToolMode(ri::editor::EditorToolMode::Select);
            return true;
        }
        if (key == 'C' && !controlHeld && !shiftHeld && !altHeld && !resourceSearchActive_) {
            SetToolMode(ri::editor::EditorToolMode::Create);
            return true;
        }
        if (key == VK_TAB) {
            full3DViewport_ = !full3DViewport_;
            ClearViewportPreviewCache();
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
        if (controlHeld && shiftHeld && key == 'R') {
            viewportRayTracePreview_ = !viewportRayTracePreview_;
            ClearViewportPreviewCache();
            lastIoStatus_ = viewportRayTracePreview_
                ? "Viewport: ray-traced quality preview ON (slower). Ctrl+Shift+R toggles."
                : "Viewport: fast raster preview ON. Ctrl+Shift+R toggles ray trace.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (controlHeld && !shiftHeld && !altHeld && key == VK_OEM_4) {
            leftPanelCollapsed_ = !leftPanelCollapsed_;
            ClearViewportPreviewCache();
            lastIoStatus_ = leftPanelCollapsed_ ? "Left panel collapsed." : "Left panel expanded.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (controlHeld && !shiftHeld && !altHeld && key == VK_OEM_6) {
            rightPanelCollapsed_ = !rightPanelCollapsed_;
            ClearViewportPreviewCache();
            lastIoStatus_ = rightPanelCollapsed_ ? "Right panel collapsed." : "Right panel expanded.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (controlHeld && !shiftHeld && !altHeld && key == '2') {
            if (ShouldShowAuthoringCatalogChrome() || leftPanelMode_ == LeftPanelMode::Scene) {
                if (!ShouldShowAuthoringCatalogChrome()) {
                    inspectorPanel_ = InspectorPanel::Brush;
                }
                authoringCatalogExpanded_ = !authoringCatalogExpanded_;
                ClearViewportPreviewCache();
                lastIoStatus_ = authoringCatalogExpanded_ ? "Authoring catalog shown (Ctrl+2 toggles)."
                                                          : "Authoring catalog hidden — viewport enlarged.";
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return true;
        }
        if (key == VK_HOME && !controlHeld) {
            TryFrameAllRenderables();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (controlHeld && shiftHeld && key == 'Q') {
            DestroyWindow(hwnd_);
            return true;
        }
        if (key == VK_ESCAPE) {
            if (cameraDragMode_ != CameraDragMode::None) {
                EndCameraDrag();
                lastIoStatus_ = "Camera drag cancelled.";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return true;
            }
            if (logicLayer_.WirePickState().armed) {
                logicLayer_.ClearWirePick();
                lastIoStatus_ = "Wire pick cleared.";
            } else if (static_cast<int>(selectedNode_) != starterScene_.handles.root &&
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
        if (controlHeld && altHeld && key == 'L') {
            logicLayer_.SetPlayerPreviewHidden(starterScene_.scene, !logicLayer_.IsPlayerPreviewHidden());
            lastIoStatus_ = logicLayer_.IsPlayerPreviewHidden()
                ? "Logic layer: hidden from player preview (creator/debug view)."
                : "Logic layer: visible in player preview.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_F7) {
            const std::size_t inputIndex = shiftHeld ? 1U : 0U;
            bool pulsed = logicLayer_.PulseLogicNodeAtSceneHandle(
                starterScene_.scene, static_cast<int>(selectedNode_), inputIndex);
            if (!pulsed) {
                pulsed = logicLayer_.PulseMostRecentNode(starterScene_.scene, inputIndex);
            }
            if (pulsed) {
                lastIoStatus_ = std::string(shiftHeld ? "Logic test pulse (Shift+F7) input #2 on selected/recent node.  "
                                                     : "Logic test pulse (F7) input #1 on selected/recent node.  ")
                    + logicLayer_.LastCompileSummary();
            } else {
                lastIoStatus_ = shiftHeld
                    ? "Logic test pulse: select a logic node with a second input (e.g. mem_flipflop Clock)."
                    : "Logic test pulse: select a logic node with inputs first.";
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (altHeld && key == 'W') {
            const ri::editor::EditorLogicWirePickResult pick = logicLayer_.HandleWirePickAtSceneNode(
                starterScene_.scene, static_cast<int>(selectedNode_));
            if (pick.handled) {
                lastIoStatus_ = pick.message + "  Escape clears wire pick.";
            } else {
                lastIoStatus_ = pick.message;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (altHeld && (key == VK_OEM_4 || key == VK_OEM_6)) {
            logicLayer_.CycleWirePickPort(key == VK_OEM_4 ? -1 : 1);
            if (logicLayer_.WirePickState().armed) {
                lastIoStatus_ = "Wire port: " + logicLayer_.WirePickState().sourceId + "."
                    + logicLayer_.WirePickState().outputName;
            } else {
                lastIoStatus_ = "Alt+[ / Alt+] cycles armed output port.";
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (key == VK_F8) {
            if (logicLayer_.PulseSelectedTrigger(starterScene_.scene, static_cast<int>(selectedNode_))) {
                lastIoStatus_ = "Trigger test pulse (F8): OnStartTouch fired into logic graph.";
            } else {
                lastIoStatus_ = "Trigger test: select a Trigger_* volume and ensure logic graph compiles.";
            }
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
        if (HandleGlobalEditorHotkeys(key, controlHeld, shiftHeld, altHeld)) {
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
                    .onAddLight = [this]() {
                        AddLightAtFocus();
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
                        TryFrameSelection();
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
                    .onSelectInspectorStore = [this]() {
                        (void)SetInspectorPanel(InspectorPanel::PluginStore);
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

        if (TryHandlePanelCollapseClick(point)) {
            return 0;
        }

        if (HitTestViewportCreateMenu(layout.viewportInner, point)) {
            leftPanelMode_ = LeftPanelMode::Create;
            lastIoStatus_ = "Creator Lab opened from Viewport > Create menu.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        if (HitTestViewportHelpMenu(layout.viewportInner, point)) {
            ShowHelpGuide();
            return 0;
        }

        if (ShouldShowViewportWorldBar()) {
            const ri::editor::EditorViewportWorldBarHit worldBarHit = ri::editor::HitTestViewportWorldBar(
                layout.viewportInner, point, true, kViewportWorldBarHeight_);
            if (worldBarHit.hitAtmosphereCycle) {
                creatorAtmospherePreset_ = CycleCreatorAtmosphere(creatorAtmospherePreset_);
                ApplyCreatorAtmosphere(creatorAtmospherePreset_);
                ClearViewportPreviewCache();
                lastIoStatus_ = "Sky preset: " + AtmospherePresetLabel(creatorAtmospherePreset_) + " (click Sky bar to cycle).";
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
        }

        if (TryHandleAuthoringCatalogClick(point, layout.viewportInner)) {
            return 0;
        }

        if (TryBeginCameraRailDrag(point, layout.cameraRailInner)) {
            return 0;
        }

        if (ri::editor::DispatchEditorToolbarClick(
                layout.toolStrip,
                point,
                {
                    .onSelectMode = [this]() {
                        SetToolMode(ri::editor::EditorToolMode::Select);
                    },
                    .onCreateMode = [this]() {
                        SetToolMode(ri::editor::EditorToolMode::Create);
                    },
                    .onCameraMode = [this]() {
                        SetToolMode(ri::editor::EditorToolMode::Camera);
                    },
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
                    .onResolutionScaleToggle = [this]() {
                        viewportResolutionScalingEnabled_ = !viewportResolutionScalingEnabled_;
                        viewportPreviewDirty_ = true;
                        lastIoStatus_ = viewportResolutionScalingEnabled_
                            ? "Viewport motion scaling: 1/2 resolution while moving."
                            : "Viewport motion scaling: full resolution.";
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
                    .onAddLight = [this]() {
                        AddLightAtFocus();
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
                : (leftPanelMode_ == LeftPanelMode::Create ? ri::editor::EditorLeftPanelMode::Create
                                                           : ri::editor::EditorLeftPanelMode::Scene);

        if (leftPanelMode_ == LeftPanelMode::Create
            && DispatchCreatorPanelClick(
                layout.hierarchyInner,
                point,
                {
                    .onSelectTemplate = [this](const NewGameTemplate templateKind) {
                        newGameTemplate_ = templateKind;
                        newGameNameVariant_ = 0;
                        lastIoStatus_ = "Template: " + NewGameTemplateLabel(templateKind) + ".";
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onCycleNameVariant = [this](const int delta) { CycleNewGameNameVariant(delta); },
                    .onCreateProject = [this]() { TryCreateNewGameFromWizard(); },
                    .onApplyAtmosphere = [this](const CreatorAtmospherePreset preset) {
                        ApplyCreatorAtmosphere(preset);
                    },
                    .onInsertPreset = [this](const CreatorInsertPreset preset) { ApplyCreatorInsert(preset); },
                    .onApplyCamera = [this](const CreatorCameraPreset preset) { ApplyCreatorCamera(preset); },
                    .onSetupFiles = [this]() {
                        TryScaffoldMountedGame();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onCycleAtmosphereMenu = [this]() {
                        creatorAtmospherePreset_ = CycleCreatorAtmosphere(creatorAtmospherePreset_);
                        ApplyCreatorAtmosphere(creatorAtmospherePreset_);
                    },
                    .onCycleInsertMenu = [this]() {
                        creatorInsertPreset_ = CycleCreatorInsert(creatorInsertPreset_);
                        ApplyCreatorInsert(creatorInsertPreset_);
                    },
                    .onCycleCameraMenu = [this]() {
                        creatorCameraPreset_ = CycleCreatorCamera(creatorCameraPreset_);
                        ApplyCreatorCamera(creatorCameraPreset_);
                    },
                })) {
            return 0;
        }

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
                        if (sceneConfig_.gameManifest.has_value()) {
                            authoringCatalogExpanded_ = false;
                            ClearViewportPreviewCache();
                        }
                        RebuildFilteredHierarchyOrder();
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    },
                    .onCreateTab = [this]() {
                        leftPanelMode_ = LeftPanelMode::Create;
                        lastIoStatus_ = "Creator Lab opened. Templates, atmosphere, inserts, and cameras live here.";
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

        if (HitCameraPlot(point) && starterScene_.handles.orbitCamera.cameraNode != ri::scene::kInvalidHandle) {
            const bool altHeld = IsKeyDown(VK_MENU);
            const bool shiftHeld = IsKeyDown(VK_SHIFT);
            if (toolMode_ == ri::editor::EditorToolMode::Create && !altHeld && !shiftHeld) {
                if (TryPlaceAuthoringCatalogInViewport(x, y)) {
                    return 0;
                }
            }
            if (toolMode_ == ri::editor::EditorToolMode::Select && !altHeld && !shiftHeld) {
                if (const std::optional<int> picked = PickRenderableInCameraView(
                        cameraPlotRect_,
                        x,
                        y,
                        starterScene_.scene,
                        starterScene_.handles.orbitCamera.cameraNode)) {
                    selectedNode_ = static_cast<std::size_t>(*picked);
                    lastIoStatus_ = "3D viewport: selected renderable (F to frame · Alt+drag to orbit).";
                    EnsureHierarchySelectionVisible(layout.hierarchyInner);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return 0;
                }
            }
            if (TryBeginCameraDrag(point, shiftHeld ? CameraDragMode::ViewPan : CameraDragMode::ViewOrbit)) {
                return 0;
            }
        }

        const int splitterGrab = 6;
        if (!leftPanelCollapsed_
            && std::abs(x - layout.hierarchySplitter.right) <= splitterGrab
            && y >= layout.hierarchy.top && y <= layout.hierarchy.bottom) {
            draggingHierarchySplitter_ = true;
            SetCapture(hwnd_);
            return 0;
        }
        if (!rightPanelCollapsed_
            && std::abs(x - layout.inspectorSplitter.left) <= splitterGrab
            && y >= layout.inspector.top && y <= layout.inspector.bottom) {
            draggingInspectorSplitter_ = true;
            SetCapture(hwnd_);
            return 0;
        }

        return 0;
    }

    LRESULT OnLeftButtonDoubleClick(const int x, const int y) {
        const EditorLayout layout = ComputeLayout();
        const POINT point{x, y};
        if (ShouldShowStructuralPicker() && HitCameraPlot(point) == FALSE) {
            const StructuralPickerLayout pickerLayout = CurrentStructuralPickerLayout(layout.viewportInner);
            const ri::editor::StructuralPickerHit hit = HitTestStructuralPicker(pickerLayout, point);
            if (hit.kind == StructuralPickerHitKind::Preset) {
                SelectedCatalogPresetIndex(authoringCatalogSection_) = hit.presetIndex;
                SpawnAuthoringCatalogAtFocus();
                return 0;
            }
        }
        if (leftPanelCollapsed_ || leftPanelMode_ != LeftPanelMode::Scene) {
            return 0;
        }
        if (PtInRect(&layout.hierarchyInner, point) == FALSE) {
            return 0;
        }
        const int row = ri::editor::HitTestSceneRow(layout.hierarchyInner, point.y, hierarchyScrollTopRow_);
        if (row < 0) {
            return 0;
        }
        const std::vector<int>& order = HierarchyDrawOrder();
        if (row >= static_cast<int>(order.size())) {
            return 0;
        }
        selectedNode_ = static_cast<std::size_t>(order[static_cast<std::size_t>(row)]);
        TryFrameSelection();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    }

    LRESULT OnMiddleButtonDown(int x, int y) {
        POINT point{x, y};
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        if (TryBeginCameraDrag(point, CameraDragMode::ViewOrbit)) {
            return 0;
        }
        return 0;
    }

    LRESULT OnMiddleButtonUp(int x, int y) {
        (void)x;
        (void)y;
        if (cameraDragMode_ != CameraDragMode::None) {
            EndCameraDrag();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    LRESULT OnRightButtonDown(int x, int y) {
        POINT point{x, y};
        const EditorLayout layout = ComputeLayout();
        UpdateCameraPlotRect(layout.viewportInner);
        if (TryBeginCameraDrag(point, CameraDragMode::ViewPan)) {
            return 0;
        }
        return 0;
    }

    LRESULT OnRightButtonUp(int x, int y) {
        (void)x;
        (void)y;
        if (cameraDragMode_ != CameraDragMode::None) {
            EndCameraDrag();
            InvalidateRect(hwnd_, nullptr, FALSE);
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
        if (cameraDragMode_ != CameraDragMode::None) {
            EndCameraDrag();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    LRESULT OnMouseMove(int x, int y, WPARAM flags) {
        if (draggingHierarchySplitter_ && (flags & MK_LBUTTON) != 0) {
            RECT client{};
            GetClientRect(hwnd_, &client);
            const int minLeft = 124;
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
        if (cameraDragMode_ != CameraDragMode::None && !autoOrbitPreview_) {
            const bool leftHeld = (flags & MK_LBUTTON) != 0;
            const bool middleHeld = (flags & MK_MBUTTON) != 0;
            const bool rightHeld = (flags & MK_RBUTTON) != 0;
            const bool dragButtonHeld =
                (cameraDragMode_ == CameraDragMode::ViewOrbit && (leftHeld || middleHeld))
                || (cameraDragMode_ == CameraDragMode::ViewPan && (leftHeld || rightHeld))
                || IsRailDragMode();
            if (dragButtonHeld) {
                const int dx = x - lastDragX_;
                const int dy = y - lastDragY_;
                lastDragX_ = x;
                lastDragY_ = y;
                if (IsRailDragMode()) {
                    const EditorLayout layout = ComputeLayout();
                    UpdateCameraRailPadFromPoint(POINT{x, y}, layout.cameraRailInner);
                    lastRailInputSteady_ = std::chrono::steady_clock::now();
                    ApplyDirectCameraRailDrag(dx, dy);
                    viewportPreviewDirty_ = true;
                }
                ApplyCameraDragDelta(dx, dy);
                InvalidateViewportAndRail();
            }
        } else {
            (void)UpdateInteractiveCursor(x, y);
            lastMouseClientPos_ = POINT{x, y};
            hasLastMouseClientPos_ = true;
            const EditorLayout layout = ComputeLayout();
            const std::string toolbarTooltip =
                ri::editor::EditorToolbarTooltipAtPoint(layout.toolStrip, lastMouseClientPos_);
            if (toolbarTooltip != hoveredToolbarTooltip_) {
                hoveredToolbarTooltip_ = toolbarTooltip;
                RECT tooltipDirty = layout.toolStrip;
                tooltipDirty.bottom += 32;
                InvalidateRect(hwnd_, &tooltipDirty, FALSE);
            }
            if (toolMode_ == ri::editor::EditorToolMode::Create && HitCameraPlot(lastMouseClientPos_)) {
                const std::optional<ri::math::Vec3> nextPlacement =
                    ri::render::software::PickPlacementPointInCameraView(
                        CameraViewRectFrom(cameraPlotRect_),
                        x,
                        y,
                        starterScene_.scene,
                        starterScene_.handles.orbitCamera.cameraNode);
                const bool placementChanged = createModePlacementPoint_.has_value() != nextPlacement.has_value()
                    || (createModePlacementPoint_.has_value() && nextPlacement.has_value()
                        && (std::abs(createModePlacementPoint_->x - nextPlacement->x) > 0.04f
                            || std::abs(createModePlacementPoint_->y - nextPlacement->y) > 0.04f
                            || std::abs(createModePlacementPoint_->z - nextPlacement->z) > 0.04f));
                createModePlacementPoint_ = nextPlacement;
                if (placementChanged) {
                    InvalidateViewportAndRail();
                }
            } else if (createModePlacementPoint_.has_value()) {
                createModePlacementPoint_.reset();
                InvalidateViewportAndRail();
            }
        }
        return 0;
    }

    [[nodiscard]] std::optional<ri::math::Vec3> ResolveCreateModeGhostCenter() const {
        if (!createModePlacementPoint_.has_value()) {
            return std::nullopt;
        }
        if (authoringCatalogSection_ == ri::editor::AuthoringCatalogSection::Structural) {
            return StructuralBrushSpawnPositionAtPoint(CurrentStructuralPrimitivePreset().structuralType,
                                                       *createModePlacementPoint_);
        }
        return *createModePlacementPoint_;
    }

    [[nodiscard]] ri::math::Vec3 ResolveCreateModeGhostHalfExtents() const {
        if (authoringCatalogSection_ == ri::editor::AuthoringCatalogSection::Structural) {
            const ri::scene::StructuralPrimitivePreset& preset = CurrentStructuralPrimitivePreset();
            ri::scene::StructuralBrushSpawnOptions brush{};
            brush.structuralType = preset.structuralType;
            brush.shape = ri::scene::ShapeFromStructuralPreset(preset);
            return ri::scene::EstimateStructuralBrushHalfExtents(brush);
        }
        const std::size_t presetIndex = SelectedCatalogPresetIndex(authoringCatalogSection_);
        const ri::editor::AuthoringCatalogPreset& preset =
            ri::editor::AuthoringCatalogPresetAt(authoringCatalogSection_, presetIndex);
        if (preset.spawnKind == ri::editor::AuthoringCatalogSpawnKind::VolumeMarker) {
            return {1.0f, 1.0f, 1.0f};
        }
        return {0.5f, 0.5f, 0.5f};
    }

    void DrawCreateModeStampCursor(HDC dc) const {
        if (toolMode_ != ri::editor::EditorToolMode::Create || !hasLastMouseClientPos_) {
            return;
        }
        if (!HitCameraPlot(lastMouseClientPos_)) {
            return;
        }
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 196, 88));
        HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
        const int cx = lastMouseClientPos_.x;
        const int cy = lastMouseClientPos_.y;
        MoveToEx(dc, cx - 10, cy, nullptr);
        LineTo(dc, cx + 10, cy);
        MoveToEx(dc, cx, cy - 10, nullptr);
        LineTo(dc, cx, cy + 10);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        HPEN ringPen = CreatePen(PS_DOT, 1, RGB(255, 220, 140));
        oldPen = static_cast<HPEN>(SelectObject(dc, ringPen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, GetStockObject(HOLLOW_BRUSH)));
        Ellipse(dc, cx - 14, cy - 14, cx + 14, cy + 14);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(ringPen);
    }

    void OnCaptureLost() {
        if (cameraDragMode_ == CameraDragMode::RailTrackball) {
            ri::editor::BeginRailPadSpring(orbitRailPad_);
            orbitRailPad_.springStart = std::chrono::steady_clock::now();
            viewportPreviewDirty_ = true;
        } else if (cameraDragMode_ == CameraDragMode::RailPan) {
            ri::editor::BeginRailPadSpring(panRailPad_);
            panRailPad_.springStart = std::chrono::steady_clock::now();
            viewportPreviewDirty_ = true;
        } else if (cameraDragMode_ == CameraDragMode::RailDepth) {
            ri::editor::BeginRailPadSpring(depthRailPad_);
            depthRailPad_.springStart = std::chrono::steady_clock::now();
            viewportPreviewDirty_ = true;
        }
        cameraDragMode_ = CameraDragMode::None;
        draggingHierarchySplitter_ = false;
        draggingInspectorSplitter_ = false;
        InvalidateViewportAndRail();
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
        const fs::path logicPath = ResolveLogicAuthoringPath();
        if (!logicLayer_.PlacedNodes().empty()) {
            (void)logicLayer_.Save(logicPath, starterScene_.scene);
            (void)logicLayer_.Recompile(starterScene_.scene);
        }
        const ri::editor::PlaytestLaunchResult launchResult = ri::editor::LaunchPlaytestForManifest(
            hwnd_, *targetManifest, sceneConfig_.workspaceRoot, logicPath);
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

    [[nodiscard]] fs::path ResolveLogicAuthoringPath() const {
        return BuildEditorLogicAuthoringPath(ResolveSceneStatePath());
    }

    [[nodiscard]] fs::path ResolveLogicAuthoringPath(const fs::path& baseScenePath) const {
        return BuildEditorLogicAuthoringPath(baseScenePath);
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
        const bool logicSaved = logicLayer_.Save(ResolveLogicAuthoringPath(baseScenePath), starterScene_.scene);
        if (sceneSaved && authoredSaved && orbitSaved && logicSaved) {
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
            if (!logicSaved) {
                if (!message.empty()) {
                    message += ", ";
                }
                message += "logic authoring";
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

        logicLayer_.Reset();
        ri::editor::BindAuthoringLogicCatalog(&logicLayer_);
        logicLayer_.EnsureKitLoaded(sceneConfig_.workspaceRoot);
        if (sceneConfig_.gameManifest.has_value()) {
            logicLayer_.EnsureGameColliderTrace(sceneConfig_.gameManifest->rootPath);
        }
        (void)logicLayer_.Load(ResolveLogicAuthoringPath(baseScenePath), starterScene_.scene, starterScene_.handles.root);
        structuralThumbnailCache_.Clear();

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
                        "'. Resize/move it, then Ctrl+E exports assembly.primitives.csv, lighting, colliders, and triggers.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] std::string NextLightBasename() const {
        static constexpr std::string_view kPrefix = "Light_";
        int maxIndex = 0;
        for (const ri::scene::Node& node : starterScene_.scene.Nodes()) {
            const std::string& name = node.name;
            if (name.rfind(kPrefix, 0) != 0) {
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

    void AddLightAtFocus() {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot add light: scene has no world root.";
            return;
        }

        static constexpr std::array<ri::scene::LightType, 3> kSpawnCycle{
            ri::scene::LightType::Point,
            ri::scene::LightType::Spot,
            ri::scene::LightType::Directional,
        };
        static constexpr std::array<const char*, 3> kSpawnLabels{"point", "spot", "directional"};
        const ri::scene::LightType lightType = kSpawnCycle[lightSpawnTypeIndex_ % kSpawnCycle.size()];
        const char* typeLabel = kSpawnLabels[lightSpawnTypeIndex_ % kSpawnLabels.size()];
        lightSpawnTypeIndex_ = (lightSpawnTypeIndex_ + 1U) % kSpawnCycle.size();
        const char* nextLabel = kSpawnLabels[lightSpawnTypeIndex_ % kSpawnLabels.size()];

        const std::string lightName = NextLightBasename();
        ri::scene::LightNodeOptions options{};
        options.parent = starterScene_.handles.root;
        options.nodeName = lightName;
        options.transform.position = starterScene_.handles.orbitCamera.orbit.target + ri::math::Vec3{0.0f, 1.5f, 0.0f};
        options.light = ri::scene::Light{
            .name = lightName,
            .type = lightType,
            .color = ri::math::Vec3{0.96f, 0.92f, 0.84f},
            .intensity = 2.4f,
            .range = 10.0f,
            .spotAngleDegrees = 42.0f,
        };
        if (lightType == ri::scene::LightType::Spot) {
            options.light.intensity = 3.2f;
            options.light.range = 14.0f;
            options.transform.rotationDegrees = ri::math::Vec3{-35.0f, 25.0f, 0.0f};
        } else if (lightType == ri::scene::LightType::Directional) {
            options.light.intensity = 1.6f;
            options.light.range = 0.0f;
            options.transform.rotationDegrees = ri::math::Vec3{-42.0f, 34.0f, 0.0f};
        }

        const int newHandle = ri::scene::AddLightNode(starterScene_.scene, options);
        selectedNode_ = static_cast<std::size_t>(newHandle);
        autosavePending_ = true;
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Added " + std::string(typeLabel) + " light '" + lightName
            + "'. Next + Light spawns " + nextLabel + ". Ctrl+E exports assembly.lighting.csv.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    [[nodiscard]] std::string NextVolumeMarkerBasename(const std::string_view volumeType) const {
        const std::string prefix = std::string("Volume_") + SanitizeBrushLabelForName(volumeType) + "_";
        std::size_t maxIndex = 0;
        for (std::size_t index = 0; index < starterScene_.scene.NodeCount(); ++index) {
            const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(index));
            if (node.name.rfind(prefix, 0) != 0) {
                continue;
            }
            const std::string suffix = node.name.substr(prefix.size());
            try {
                const std::size_t parsed = static_cast<std::size_t>(std::stoul(suffix));
                maxIndex = std::max(maxIndex, parsed);
            } catch (...) {
            }
        }
        return prefix + std::to_string(maxIndex + 1);
    }

    [[nodiscard]] std::string NextLogicPortBasename(const std::string_view logicType) const {
        const std::string prefix = std::string("LogicPort_") + SanitizeBrushLabelForName(logicType) + "_";
        std::size_t maxIndex = 0;
        for (std::size_t index = 0; index < starterScene_.scene.NodeCount(); ++index) {
            const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(index));
            if (node.name.rfind(prefix, 0) != 0) {
                continue;
            }
            const std::string suffix = node.name.substr(prefix.size());
            try {
                const std::size_t parsed = static_cast<std::size_t>(std::stoul(suffix));
                maxIndex = std::max(maxIndex, parsed);
            } catch (...) {
            }
        }
        return prefix + std::to_string(maxIndex);
    }

    void SpawnStructuralBrushAt(const std::optional<ri::math::Vec3>& clickPoint) {
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
        opt.transform.position = clickPoint.has_value()
            ? StructuralBrushSpawnPositionAtPoint(type, *clickPoint)
            : StructuralBrushSpawnPosition(type, starterScene_.handles.orbitCamera.orbit.target);
        opt.materialName = std::string("brush_") + SanitizeBrushLabelForName(preset.label);
        if (ri::editor::IsGuideStructuralPreset(type)) {
            const ri::math::Vec3 guideColor = ri::editor::GuideStructuralWireColor(type);
            opt.shadingModel = ri::scene::ShadingModel::Unlit;
            opt.baseColor = guideColor * 0.35f + ri::math::Vec3{0.08f, 0.08f, 0.10f};
            opt.emissiveColor = guideColor * 0.85f;
            opt.baseColorTexture.clear();
            opt.roughness = 1.0f;
        } else {
            opt.baseColorTexture = ri::scene::DefaultStructuralBrushAlbedoTexture();
            opt.textureTiling = ri::math::Vec2{2.0f, 2.0f};
            opt.baseColor = ri::scene::DefaultStructuralBrushBaseColor();
        }

        const int newHandle = ri::scene::AddStructuralBrushNode(starterScene_.scene, opt);
        if (newHandle == ri::scene::kInvalidHandle) {
            lastIoStatus_ =
                "Structural brush '" + std::string(type) + "' produced no mesh (internal compiler issue).";
            return;
        }
        selectedNode_ = static_cast<std::size_t>(newHandle);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Stamped '" + std::string(preset.label) + "' as " + opt.nodeName + ".";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void SpawnStructuralBrushAtFocus() {
        SpawnStructuralBrushAt(std::nullopt);
    }

    void SpawnVolumeMarkerAt(const ri::editor::AuthoringCatalogPreset& preset,
                             const std::optional<ri::math::Vec3>& clickPoint) {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot spawn volume: scene has no world root.";
            return;
        }

        const ri::math::Vec3 wireColor = ri::editor::AuthoringCatalogWireColor(
            ri::editor::AuthoringCatalogSection::Volumes, preset.typeId);
        const ri::math::Vec3 anchor =
            clickPoint.value_or(starterScene_.handles.orbitCamera.orbit.target);
        ri::scene::PrimitiveNodeOptions options{};
        options.parent = starterScene_.handles.root;
        options.primitive = ri::scene::PrimitiveType::Cube;
        options.shadingModel = ri::scene::ShadingModel::Unlit;
        options.nodeName = NextVolumeMarkerBasename(preset.typeId);
        options.materialName = std::string("volume_") + SanitizeBrushLabelForName(preset.typeId);
        options.baseColor = wireColor * 0.30f + ri::math::Vec3{0.06f, 0.06f, 0.08f};
        options.emissiveColor = wireColor * 0.85f;
        options.transform.position = anchor;
        options.transform.scale = ri::math::Vec3{2.0f, 2.0f, 2.0f};

        const int newHandle = ri::scene::AddPrimitiveNode(starterScene_.scene, options);
        selectedNode_ = static_cast<std::size_t>(newHandle);
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = "Placed volume marker '" + options.nodeName + "' (" + std::string(preset.typeId) + ").";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void SpawnVolumeMarkerAtFocus(const ri::editor::AuthoringCatalogPreset& preset) {
        SpawnVolumeMarkerAt(preset, std::nullopt);
    }

    void SpawnLogicKitNodeAtFocus(const ri::editor::AuthoringCatalogPreset& preset) {
        if (starterScene_.handles.root == ri::scene::kInvalidHandle) {
            lastIoStatus_ = "Cannot spawn logic node: scene has no world root.";
            return;
        }

        const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const ri::editor::EditorLogicPlaceResult result = logicLayer_.PlaceKitNode(
            starterScene_.scene,
            starterScene_.handles.root,
            preset.typeId,
            starterScene_.handles.orbitCamera.orbit.target,
            shiftHeld);
        if (!result.placed) {
            lastIoStatus_ = result.message.empty() ? "Logic node placement failed." : result.message;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }

        if (!logicLayer_.PlacedNodes().empty()) {
            const std::vector<int>& handles = logicLayer_.PlacedNodes().back().sceneHandles;
            if (!handles.empty()) {
                selectedNode_ = static_cast<std::size_t>(handles.front());
            }
        }
        const EditorLayout layout = ComputeLayout();
        EnsureHierarchySelectionVisible(layout.hierarchyInner);
        lastIoStatus_ = result.message + "  " + logicLayer_.LastCompileSummary()
            + "  Shift+Place auto-wire · Alt+W manual wire · Ctrl+Alt+L player-preview hide.";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void SpawnAuthoringCatalogAt(const std::optional<ri::math::Vec3>& clickPoint) {
        if (authoringCatalogSection_ == ri::editor::AuthoringCatalogSection::Structural) {
            SpawnStructuralBrushAt(clickPoint);
            return;
        }

        const std::size_t presetIndex = SelectedCatalogPresetIndex(authoringCatalogSection_);
        const ri::editor::AuthoringCatalogPreset& preset =
            ri::editor::AuthoringCatalogPresetAt(authoringCatalogSection_, presetIndex);
        switch (preset.spawnKind) {
            case ri::editor::AuthoringCatalogSpawnKind::TriggerVolume:
                AddTriggerVolumePrimitive();
                break;
            case ri::editor::AuthoringCatalogSpawnKind::VolumeMarker:
                SpawnVolumeMarkerAt(preset, clickPoint);
                break;
            case ri::editor::AuthoringCatalogSpawnKind::LogicPort:
                SpawnLogicKitNodeAtFocus(preset);
                break;
            case ri::editor::AuthoringCatalogSpawnKind::StructuralBrush:
                SpawnStructuralBrushAt(clickPoint);
                break;
        }
    }

    void SpawnAuthoringCatalogAtFocus() {
        SpawnAuthoringCatalogAt(std::nullopt);
    }

    [[nodiscard]] bool TryPlaceAuthoringCatalogInViewport(const int x, const int y) {
        if (toolMode_ != ri::editor::EditorToolMode::Create) {
            return false;
        }
        const POINT point{x, y};
        if (!HitCameraPlot(point)) {
            return false;
        }
        std::optional<ri::math::Vec3> placement = ri::render::software::PickPlacementPointInCameraView(
            CameraViewRectFrom(cameraPlotRect_),
            x,
            y,
            starterScene_.scene,
            starterScene_.handles.orbitCamera.cameraNode);
        // Structural stamps target the authored Q-mesh, not arbitrary render geometry. The cached
        // partition keeps this precise click path inexpensive while scene edits still invalidate it.
        if (authoringCatalogSection_ == ri::editor::AuthoringCatalogSection::Structural) {
            const std::optional<ri::render::software::CameraViewRay> cameraRay =
                ri::render::software::BuildCameraViewRay(
                    CameraViewRectFrom(cameraPlotRect_),
                    x,
                    y,
                    starterScene_.scene,
                    starterScene_.handles.orbitCamera.cameraNode);
            if (cameraRay.has_value()) {
                const ri::scene::SemanticStructuralPartition& partition =
                    structuralPlacementPartitionCache_.GetOrRebuild(starterScene_.scene);
                const std::optional<ri::scene::SemanticStructuralRaycastHit> structuralHit =
                    ri::scene::RaycastSemanticStructuralPartition(
                        starterScene_.scene,
                        partition,
                        cameraRay->ray,
                        cameraRay->farClip,
                        {
                            .channel = ri::scene::StructuralBrushChannel::QueryMesh,
                            .queryPurpose = ri::scene::StructuralBrushQueryPurpose::Placement,
                        });
                if (structuralHit.has_value()) {
                    placement = structuralHit->hit.position;
                }
            }
        }
        if (!placement.has_value()) {
            return false;
        }
        SpawnAuthoringCatalogAt(placement);
        lastIoStatus_ = "Stamped " + ArmedCatalogPresetSummary() + " at click.";
        InvalidateRect(hwnd_, nullptr, FALSE);
        return true;
    }

    void TryExportAssemblyPrimitivesCsv() {
        fs::path outputPath;
        fs::path triggersOutputPath;
        fs::path lightingOutputPath;
        fs::path collidersOutputPath;
        std::string destinationSummary;
        if (!workspaceGames_.empty() && focusedWorkspaceGameIndex_ >= 0 &&
            focusedWorkspaceGameIndex_ < static_cast<int>(workspaceGames_.size())) {
            const WorkspaceGameEntry& game = workspaceGames_[static_cast<std::size_t>(focusedWorkspaceGameIndex_)];
            outputPath = game.rootPath / "levels" / "assembly.primitives.csv";
            triggersOutputPath = game.rootPath / "levels" / "assembly.triggers.csv";
            lightingOutputPath = game.rootPath / "levels" / "assembly.lighting.csv";
            collidersOutputPath = game.rootPath / "levels" / "assembly.colliders.csv";
            destinationSummary = game.displayName + " → levels/assembly.primitives.csv";
        } else {
            outputPath = ResolveSceneStatePath().parent_path() / "assembly.primitives.export.csv";
            triggersOutputPath = ResolveSceneStatePath().parent_path() / "assembly.triggers.export.csv";
            lightingOutputPath = ResolveSceneStatePath().parent_path() / "assembly.lighting.export.csv";
            collidersOutputPath = ResolveSceneStatePath().parent_path() / "assembly.colliders.export.csv";
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
        const ri::editor::LevelExportResult triggerResult =
            TryExportAssemblyTriggersCsv(starterScene_.scene, triggersOutputPath);
        if (triggerResult.success) {
            lastIoStatus_ += "  Exported " + std::to_string(triggerResult.rowCount) + " trigger(s) to "
                + triggersOutputPath.filename().string() + ".";
        } else if (!triggerResult.error.empty() && triggerResult.error != "no Trigger_* cube nodes found to export") {
            lastIoStatus_ += "  Trigger export failed: " + triggerResult.error + ".";
        }

        const ri::editor::LevelExportResult lightingResult =
            TryExportAssemblyLightingCsv(starterScene_.scene, lightingOutputPath);
        if (lightingResult.success) {
            lastIoStatus_ += "  Exported " + std::to_string(lightingResult.rowCount) + " light(s) to "
                + lightingOutputPath.filename().string() + ".";
        } else if (!lightingResult.error.empty()) {
            lastIoStatus_ += "  Lighting export failed: " + lightingResult.error + ".";
        }

        const ri::editor::LevelExportResult colliderResult =
            TryExportAssemblyCollidersCsv(starterScene_.scene, collidersOutputPath);
        if (colliderResult.success) {
            lastIoStatus_ += "  Exported " + std::to_string(colliderResult.rowCount) + " collider(s) to "
                + collidersOutputPath.filename().string() + ".";
        } else if (!colliderResult.error.empty()) {
            lastIoStatus_ += "  Collider export failed: " + colliderResult.error + ".";
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
        options.baseColorTexture = ri::scene::DefaultStructuralBrushAlbedoTexture();
        options.baseColor = ri::scene::DefaultStructuralBrushBaseColor();

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
            ClearViewportPreviewCache();
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
        viewportSceneSnapshotDirty_ = true;
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
        for (std::size_t index = 0; index < materialNudgeButtons_.size(); ++index) {
            const RECT& rect = materialNudgeButtons_[index];
            if (rect.left >= rect.right || rect.top >= rect.bottom) {
                continue;
            }
            if (point.x < rect.left || point.x > rect.right || point.y < rect.top || point.y > rect.bottom) {
                continue;
            }
            const int field = static_cast<int>(index / 2);
            const float direction = (index % 2) == 0 ? -1.0f : 1.0f;
            NudgeSelectedMaterialProperty(field, direction);
            return true;
        }
        for (std::size_t index = 0; index < lightNudgeButtons_.size(); ++index) {
            const RECT& rect = lightNudgeButtons_[index];
            if (rect.left >= rect.right || rect.top >= rect.bottom) {
                continue;
            }
            if (point.x < rect.left || point.x > rect.right || point.y < rect.top || point.y > rect.bottom) {
                continue;
            }
            const float direction = (index % 2) == 0 ? -1.0f : 1.0f;
            NudgeSelectedLightProperty(direction);
            return true;
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

    void DrawMaterialNudgeRow(HDC dc,
                              int& top,
                              const RECT& inspectorInner,
                              const char* label,
                              const int fieldIndex) {
        DrawTextLine(dc,
                     RECT{inspectorInner.left + 10, top, inspectorInner.left + 70, top + 18},
                     label,
                     RGB(200, 200, 200),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        const RECT minusRect{inspectorInner.left + 72, top, inspectorInner.left + 90, top + 18};
        const RECT plusRect{inspectorInner.left + 92, top, inspectorInner.left + 110, top + 18};
        const std::size_t minusIndex = static_cast<std::size_t>(fieldIndex * 2);
        materialNudgeButtons_[minusIndex] = minusRect;
        materialNudgeButtons_[minusIndex + 1U] = plusRect;
        DrawToolbarButton(dc, minusRect, "-", false);
        DrawToolbarButton(dc, plusRect, "+", false);
        top += 22;
    }

    void DrawLightNudgeRow(HDC dc,
                           int& top,
                           const RECT& inspectorInner,
                           const char* label,
                           const int /*fieldIndex*/) {
        DrawTextLine(dc,
                     RECT{inspectorInner.left + 10, top, inspectorInner.left + 70, top + 18},
                     label,
                     RGB(200, 200, 200),
                     smallFont_,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        const RECT minusRect{inspectorInner.left + 72, top, inspectorInner.left + 90, top + 18};
        const RECT plusRect{inspectorInner.left + 92, top, inspectorInner.left + 110, top + 18};
        lightNudgeButtons_[0] = minusRect;
        lightNudgeButtons_[1] = plusRect;
        DrawToolbarButton(dc, minusRect, "-", false);
        DrawToolbarButton(dc, plusRect, "+", false);
        top += 22;
    }

    void NudgeSelectedMaterialProperty(const int field, const float direction) {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return;
        }
        const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        if (node.material == ri::scene::kInvalidHandle
            || static_cast<std::size_t>(node.material) >= starterScene_.scene.MaterialCount()) {
            lastIoStatus_ = "Material edit blocked: selection has no material.";
            return;
        }
        ri::scene::Material& material = starterScene_.scene.GetMaterial(node.material);
        switch (field) {
        case 0:
            material.roughness = std::clamp(material.roughness + direction * 0.05f, 0.04f, 1.0f);
            break;
        case 1:
            material.metallic = std::clamp(material.metallic + direction * 0.05f, 0.0f, 1.0f);
            break;
        default:
            material.opacity = std::clamp(material.opacity + direction * 0.05f, 0.05f, 1.0f);
            if (material.opacity < 0.98f) {
                material.transparent = true;
            }
            break;
        }
        autosavePending_ = true;
        viewportSceneSnapshotDirty_ = true;
        lastIoStatus_ = "Material updated for " + node.name + ".";
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void NudgeSelectedLightProperty(const float direction) {
        if (selectedNode_ >= starterScene_.scene.NodeCount()) {
            return;
        }
        const ri::scene::Node& node = starterScene_.scene.GetNode(static_cast<int>(selectedNode_));
        if (node.light == ri::scene::kInvalidHandle
            || static_cast<std::size_t>(node.light) >= starterScene_.scene.LightCount()) {
            lastIoStatus_ = "Light edit blocked: selection has no light.";
            return;
        }
        ri::scene::Light& light = starterScene_.scene.GetLight(node.light);
        light.intensity = std::clamp(light.intensity + direction * 0.15f, 0.05f, 8.0f);
        autosavePending_ = true;
        viewportSceneSnapshotDirty_ = true;
        lastIoStatus_ = "Light intensity updated for " + node.name + ".";
        InvalidateRect(hwnd_, nullptr, FALSE);
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
            viewportSceneSnapshotDirty_ = true;
            return true;
        }

        const SceneGraphEditAction& sceneAction = std::get<SceneGraphEditAction>(action);
        starterScene_.scene = sceneAction.beforeScene;
        selectedNode_ = sceneAction.beforeSelectedNode;
        RebindEditorTrashFolderAfterSceneReplace();
        redoStack_.push_back(action);
        viewportSceneSnapshotDirty_ = true;
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
            viewportSceneSnapshotDirty_ = true;
            return true;
        }

        const SceneGraphEditAction& sceneAction = std::get<SceneGraphEditAction>(action);
        starterScene_.scene = sceneAction.afterScene;
        selectedNode_ = sceneAction.afterSelectedNode;
        RebindEditorTrashFolderAfterSceneReplace();
        undoStack_.push_back(action);
        viewportSceneSnapshotDirty_ = true;
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
            case InspectorPanel::PluginStore: return "Store";
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

    [[nodiscard]] ri::editor::FilesInspectorPanelModel BuildFilesInspectorPanelModelForWindow(
        const WorkspaceResourceEntry* selectedResourceEntry) const {
        ri::editor::FilesInspectorPanelModel model = BuildFilesInspectorPanelModel(
            selectedResourceEntry,
            resourceManifestIssues_,
            resourceEditorAuxMessage_,
            resourceFileDirty_,
            ResourceFocusSummary());
        if (sceneConfig_.gameManifest.has_value()) {
            const ri::editor::ProjectHealthReport health = BuildProjectHealthReport(
                *sceneConfig_.gameManifest,
                ResolveDedicatedPlaytestExecutable(*sceneConfig_.gameManifest),
                CanResolvePlaytestExecutable(*sceneConfig_.gameManifest));
            const ri::editor::FilesPanelHealthSummary healthSummary = SummarizeProjectHealthForFilesPanel(health);
            model.showProjectHealth = true;
            model.projectHealthReadyLine = healthSummary.readyLine;
            model.projectHealthWarnings = healthSummary.warnings;
        }
        return model;
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
        constexpr int kMinViewportWidth = 160;
        constexpr int kMinLeftWidth = 124;
        constexpr int kMinRightWidth = 260;
        constexpr int kCollapsedPanelRailWidth = 36;
        constexpr int kOuterMargin = 20;
        constexpr int kRegionGutter = 20;

        int leftWidth = leftPanelCollapsed_
            ? kCollapsedPanelRailWidth
            : std::clamp(hierarchyPanelWidth_, kMinLeftWidth, std::max(kMinLeftWidth, clientRight - kOuterMargin));
        int rightWidth = rightPanelCollapsed_
            ? kCollapsedPanelRailWidth
            : std::clamp(inspectorPanelWidth_, kMinRightWidth, std::max(kMinRightWidth, clientRight - kOuterMargin));
        const int maxCombined =
            std::max(kMinLeftWidth + kMinRightWidth,
                     clientRight - kOuterMargin - (kRegionGutter * 2) - kMinViewportWidth);
        if (leftWidth + rightWidth > maxCombined) {
            const int excess = leftWidth + rightWidth - maxCombined;
            const int leftShrink = std::min(excess, leftWidth - kMinLeftWidth);
            leftWidth -= leftShrink;
            rightWidth -= std::min(excess - leftShrink, rightWidth - kMinRightWidth);
        }

        layout.toolStrip = RECT{10, 66, client.right - 10, 106};
        const RECT leftColumn = RECT{10, 116, 10 + leftWidth, client.bottom - 92};
        if (leftPanelCollapsed_) {
            layout.hierarchy = leftColumn;
        } else {
            constexpr int kLeftPanelSplitGap = 10;
            constexpr int kMinCameraRailHeight = 300;
            const int leftColumnHeight = leftColumn.bottom - leftColumn.top;
            const int preferredHierarchyHeight = std::max(196, (leftColumnHeight * 42) / 100);
            const int hierarchyHeight = std::clamp(
                preferredHierarchyHeight,
                196,
                std::max(196, leftColumnHeight - kMinCameraRailHeight - kLeftPanelSplitGap));
            layout.hierarchy = RECT{leftColumn.left, leftColumn.top, leftColumn.right, leftColumn.top + hierarchyHeight};
            if (inspectorPanel_ != InspectorPanel::UiWorkbench) {
                layout.cameraRail = RECT{
                    leftColumn.left,
                    layout.hierarchy.bottom + kLeftPanelSplitGap,
                    leftColumn.right,
                    leftColumn.bottom};
            }
        }
        layout.inspector = RECT{clientRight - 10 - rightWidth, 116, clientRight - 10, client.bottom - 92};
        const int viewportLeft = leftColumn.right + 10;
        int viewportRight = layout.inspector.left - 10;
        if (viewportRight < viewportLeft + kMinViewportWidth) {
            viewportRight = viewportLeft + kMinViewportWidth;
        }
        layout.viewport = RECT{viewportLeft, 116, viewportRight, client.bottom - 92};
        layout.hierarchySplitter = RECT{leftColumn.right + 2, 116, leftColumn.right + 8, client.bottom - 92};
        layout.inspectorSplitter = RECT{layout.inspector.left - 8, 116, layout.inspector.left - 2, client.bottom - 92};
        layout.hierarchyInner = RECT{layout.hierarchy.left + 8, layout.hierarchy.top + 36, layout.hierarchy.right - 8, layout.hierarchy.bottom - 8};
        layout.cameraRailInner = RECT{layout.cameraRail.left + 6, layout.cameraRail.top + 6, layout.cameraRail.right - 6, layout.cameraRail.bottom - 6};
        layout.viewportInner = RECT{layout.viewport.left + 8, layout.viewport.top + 36, layout.viewport.right - 8, layout.viewport.bottom - 8};
        layout.inspectorInner = RECT{layout.inspector.left + 8, layout.inspector.top + 36, layout.inspector.right - 8, layout.inspector.bottom - 8};
        return layout;
    }

    void DrawToolbarButton(HDC dc,
                           const RECT& rect,
                           const std::string& label,
                           bool active,
                           ri::editor::EditorToolbarStyle style = ri::editor::EditorToolbarStyle::Dark) {
        EditorRenderer::DrawToolbarButton(dc, rect, label, active, smallFont_, style);
    }

    void DrawPanelHeader(HDC dc,
                         const RECT& panelRect,
                         const std::string& title,
                         const std::string& meta = {},
                         const bool showCollapseToggle = false,
                         const bool collapsed = false,
                         RECT* collapseToggleRectOut = nullptr) {
        EditorRenderer::DrawPanelHeader(
            dc, panelRect, title, headerFont_, smallFont_, meta, showCollapseToggle, collapsed, collapseToggleRectOut);
    }

    [[nodiscard]] bool TryHandlePanelCollapseClick(const POINT& point) {
        if (PtInRect(&leftPanelCollapseToggleRect_, point) != FALSE) {
            leftPanelCollapsed_ = !leftPanelCollapsed_;
            MarkViewportPreviewDirty();
            lastIoStatus_ = leftPanelCollapsed_ ? "Left panel collapsed (Ctrl+[)." : "Left panel expanded.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        if (PtInRect(&rightPanelCollapseToggleRect_, point) != FALSE) {
            rightPanelCollapsed_ = !rightPanelCollapsed_;
            MarkViewportPreviewDirty();
            lastIoStatus_ = rightPanelCollapsed_ ? "Right panel collapsed (Ctrl+])." : "Right panel expanded.";
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        return false;
    }

    void DrawViewportPreview(HDC dc, const RECT& targetRect) {
        if (viewportGpuEnabled_ && vulkanViewport_.Running()) {
            FillRectColor(dc, targetRect, RGB(10, 12, 15));
            return;
        }
        if (starterScene_.handles.orbitCamera.cameraNode == ri::scene::kInvalidHandle) {
            FillRectColor(dc, targetRect, RGB(32, 36, 42));
            return;
        }
        if (!viewportPreviewReady_ || viewportPreviewScratch_.pixels.empty()) {
            FillRectColor(dc, targetRect, RGB(32, 36, 42));
            return;
        }

        const bool compositeGhost = ri::editor::ShouldCompositeCreateModeGhost(
            toolMode_ == ri::editor::EditorToolMode::Create,
            createModePlacementPoint_.has_value(),
            !viewportPreviewCache_.depthBuffer.empty());
        if (compositeGhost) {
            if (viewportPreviewDisplayScratch_.width != viewportPreviewScratch_.width
                || viewportPreviewDisplayScratch_.height != viewportPreviewScratch_.height
                || viewportPreviewDisplayScratch_.pixels.size() != viewportPreviewScratch_.pixels.size()) {
                viewportPreviewDisplayScratch_.width = viewportPreviewScratch_.width;
                viewportPreviewDisplayScratch_.height = viewportPreviewScratch_.height;
                viewportPreviewDisplayScratch_.pixels = viewportPreviewScratch_.pixels;
            } else {
                viewportPreviewDisplayScratch_.pixels = viewportPreviewScratch_.pixels;
            }
            ri::render::software::ScenePreviewOptions options =
                BuildViewportPreviewOptions(lastViewportRenderWidth_, lastViewportRenderHeight_);
            if (const std::optional<ri::math::Vec3> ghostCenter = ResolveCreateModeGhostCenter();
                ghostCenter.has_value()) {
                ri::render::software::DrawWireBoxOverlayIntoScenePreview(
                    viewportPreviewDisplayScratch_,
                    viewportPreviewCache_.depthBuffer,
                    starterScene_.scene,
                    starterScene_.handles.orbitCamera.cameraNode,
                    options,
                    *ghostCenter,
                    ResolveCreateModeGhostHalfExtents());
            }
            ri::editor::EditorRenderer::BlitSoftwareImage(dc, targetRect, viewportPreviewDisplayScratch_);
            return;
        }

        ri::editor::EditorRenderer::BlitSoftwareImageCached(
            dc, targetRect, viewportPreviewScratch_, viewportPreviewBlitCache_, viewportPreviewBlitGeneration_);
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
        const std::string mode = autoOrbitPreview_
            ? "auto-orbit demo"
            : (toolMode_ == ri::editor::EditorToolMode::Create
                   ? "Create: click to stamp preset"
                   : toolMode_ == ri::editor::EditorToolMode::Camera
                         ? "Camera: drag to move view"
                         : "Select: click objects · F frame");
        ri::editor::RenderEditorViewportBlock(
            dc,
            viewportInner,
            ri::editor::EditorViewportBlockModel{
                .full3DViewport = full3DViewport_,
                .createMenuActive = leftPanelMode_ == LeftPanelMode::Create,
                .showWorldBar = ShouldShowViewportWorldBar(),
                .atmosphereLabel = AtmospherePresetLabel(creatorAtmospherePreset_),
                .createHintLine = CreateModeHintLine(),
                .bottomChromeInset = AuthoringCatalogBottomChromeInset(),
                .worldBarHeight = kViewportWorldBarHeight_,
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
                .monoFont = monoFont_,
            },
            ri::editor::EditorViewportBlockCallbacks{
                .drawViewportPreview = [this, dc](const RECT& rect) {
                    this->DrawViewportPreview(dc, rect);
                },
                .drawRuntimeStatsOverlay = [this, dc](const RECT& rect) {
                    this->DrawRuntimeStatsOverlay(dc, rect);
                },
                .drawTopView = [this, dc, orbitFocus](const RECT& rect) {
                    DrawRawIronFlatSceneView(
                        dc, rect, starterScene_.scene, selectedNode_, orbitFocus, RawIronFlatProjection::TopXz, "TOP (X / Z)", smallFont_);
                },
                .drawSideView = [this, dc, orbitFocus](const RECT& rect) {
                    DrawRawIronFlatSceneView(
                        dc, rect, starterScene_.scene, selectedNode_, orbitFocus, RawIronFlatProjection::SideZy, "SIDE (Z / Y)", smallFont_);
                },
                .drawFrontView = [this, dc, orbitFocus](const RECT& rect) {
                    DrawRawIronFlatSceneView(
                        dc, rect, starterScene_.scene, selectedNode_, orbitFocus, RawIronFlatProjection::FrontXy, "FRONT (X / Y)", smallFont_);
                },
            });
    }

    [[nodiscard]] bool TryHandleUiWorkbenchViewportClick(const RECT& viewportInner, const POINT& point) {
        if (inspectorPanel_ != InspectorPanel::UiWorkbench) {
            return false;
        }
        UiWorkbenchPanelModel model = BuildUiWorkbenchPanelModel(ComputeLayout().inspectorInner);
        const UiWorkbenchViewportLayout viewportLayout = ComputeUiWorkbenchViewportLayout(viewportInner);
        const std::vector<RECT> blockRects = ComputeUiWorkbenchViewportBlockRects(viewportLayout, model.previewBlocks);
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
        const UiWorkbenchViewportLayout viewportLayout = ComputeUiWorkbenchViewportLayout(viewportInner);
        const RECT headerRect = viewportLayout.headerRect;
        const RECT shelfRect = viewportLayout.shelfRect;
        const RECT stageCard = viewportLayout.stageCard;

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

        const RECT stageRect = viewportLayout.stageRect;
        const std::vector<RECT> blockRects = ComputeUiWorkbenchViewportBlockRects(viewportLayout, model.previewBlocks);
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
                case UiWorkbenchBlockTone::Button:
                    if (!block.selected) {
                        fill = RGB(34, 58, 44);
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
            if (block.tone == UiWorkbenchBlockTone::Button) {
                const int buttonWidth = std::min(220, std::max(120, static_cast<int>(blockRect.right - blockRect.left - 80)));
                const int buttonLeft = blockRect.left + ((blockRect.right - blockRect.left) - buttonWidth) / 2;
                RECT buttonRect{buttonLeft, blockRect.top + 4, buttonLeft + buttonWidth, blockRect.bottom - 4};
                FillRectColor(dc, buttonRect, fill);
                DrawInsetFrame(dc, buttonRect, RGB(18, 20, 24), border, RGB(14, 16, 20));
                FillRectColor(dc,
                              RECT{buttonRect.left + 1, buttonRect.top + 1, buttonRect.right - 1, buttonRect.top + 4},
                              RGB(214, 150, 56));
                DrawTextLine(dc,
                             RECT{buttonRect.left + 10, buttonRect.top, buttonRect.right - 10, buttonRect.bottom},
                             block.detailLine.empty() ? block.titleLine : block.detailLine,
                             title,
                             bodyFont_,
                             DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                continue;
            }

            FillRectColor(dc, blockRect, fill);
            DrawInsetFrame(dc, blockRect, RGB(18, 20, 24), border, RGB(14, 16, 20));
            if (block.tone == UiWorkbenchBlockTone::Heading) {
                DrawTextLine(dc,
                             RECT{blockRect.left + 12, blockRect.top, blockRect.right - 12, blockRect.bottom},
                             block.detailLine.empty() ? block.titleLine : block.detailLine,
                             title,
                             headerFont_,
                             DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                continue;
            }
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
                         "No blocks on this screen yet. Use + Button, New Menu, Add Dialogue, or Add Choices in the inspector.",
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

        if (!EnsureEditorBackBuffer(windowDc, width, height)) {
            EndPaint(hwnd_, &paint);
            return;
        }
        HDC dc = backBufferDc_;

        FillRectColor(dc, client, ri::editor::EditorUiTheme::kWindowBg);

        RECT topBar{0, 0, client.right, 56};
        const ri::editor::EditorViewportTheme viewportTheme{
            .titleFont = titleFont_,
            .headerFont = headerFont_,
            .bodyFont = bodyFont_,
            .smallFont = smallFont_,
            .monoFont = monoFont_,
        };
        ri::editor::RenderEditorTopChrome(
            dc,
            client,
            topBar,
            ri::editor::EditorViewportChromeModel{
                .title = "RawIron Editor",
                .subtitle = "Quad views · project archive · creator lab · native workspace.",
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
                .toolMode = toolMode_,
                .armedPresetLabel = toolMode_ == ri::editor::EditorToolMode::Create ? ArmedCatalogPresetSummary() : std::string{},
                .editModeLabel = EditModeLabel(),
                .axisLabel = AxisLabel(),
                .gridSnapEnabled = gridSnapEnabled_,
                .editStepLabel = EditStepLabel(),
                .gridSnapLabel = GridSnapLabel(),
                .undoDepth = undoStack_.size(),
                .authoredCount = CountAuthoredNodes(),
                .triggerCount = CountTriggerNodes(),
                .resolutionScalingEnabled = viewportResolutionScalingEnabled_,
            },
            viewportTheme);

        RECT hierarchy = layout.hierarchy;
        RECT cameraRail = layout.cameraRail;
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
        if (cameraRail.right > cameraRail.left) {
            EditorRenderer::DrawPanelFrame(dc,
                                           cameraRail,
                                           ri::editor::EditorUiTheme::kPanelRaisedFill,
                                           ri::editor::EditorUiTheme::kPanelRaisedHi,
                                           ri::editor::EditorUiTheme::kPanelRaisedShadow);
        }

        const std::string leftPanelMeta =
            leftPanelMode_ == LeftPanelMode::Scene
                ? (std::to_string(starterScene_.scene.NodeCount()) + " nodes")
                : (leftPanelMode_ == LeftPanelMode::Create
                       ? "Creator Lab · quick-start templates"
                       : (std::to_string(workspaceGames_.size()) + " games · " +
                          std::to_string(filteredResourceRows_.size()) + "/" +
                          std::to_string(resourceCatalogEntries_.size()) + " files"));
        DrawPanelHeader(dc,
                        hierarchy,
                        leftPanelMode_ == LeftPanelMode::Scene
                            ? "Hierarchy"
                            : (leftPanelMode_ == LeftPanelMode::Create ? "Create" : "Project Archive"),
                        leftPanelCollapsed_ ? "collapsed" : (leftPanelMode_ == LeftPanelMode::Scene ? SelectedNodeSummary()
                        : (leftPanelMode_ == LeftPanelMode::Create ? CurrentNewGameDisplayName()
                                                                   : FocusedWorkspaceGameLabel())),
                        true,
                        leftPanelCollapsed_,
                        &leftPanelCollapseToggleRect_);
        DrawPanelHeader(dc,
                        viewport,
                        "Viewport",
                        (full3DViewport_ ? "full 3D" : "quad")
                            + std::string("  |  Tab layout  |  ")
                            + (authoringCatalogExpanded_ ? "catalog shown" : "catalog hidden"));
        DrawPanelHeader(dc,
                        inspector,
                        "Inspector",
                        rightPanelCollapsed_ ? "collapsed" : InspectorPanelLabel() + "  |  " + leftPanelMeta,
                        true,
                        rightPanelCollapsed_,
                        &rightPanelCollapseToggleRect_);
        if (cameraRail.right > cameraRail.left) {
            DrawPanelHeader(dc,
                            cameraRail,
                            "View Rig",
                            "orbit  |  pan  |  zoom",
                            false,
                            false,
                            nullptr);
        }

        RECT hierarchyInner = layout.hierarchyInner;
        RECT cameraRailInner = layout.cameraRailInner;
        RECT viewportInner = layout.viewportInner;
        RECT inspectorInner = layout.inspectorInner;
        const auto& nodes = starterScene_.scene.Nodes();
        const std::vector<int> hierarchyOrder = HierarchyDrawOrder();
        DrawInsetFrame(dc,
                       hierarchyInner,
                       ri::editor::EditorUiTheme::kWellFill,
                       ri::editor::EditorUiTheme::kWellHi,
                       ri::editor::EditorUiTheme::kWellShadow);
        DrawInsetFrame(dc,
                       viewportInner,
                       ri::editor::EditorUiTheme::kViewportWellFill,
                       ri::editor::EditorUiTheme::kWellHi,
                       ri::editor::EditorUiTheme::kWellShadow);
        DrawInsetFrame(dc,
                       inspectorInner,
                       ri::editor::EditorUiTheme::kWellFill,
                       ri::editor::EditorUiTheme::kWellHi,
                       ri::editor::EditorUiTheme::kWellShadow);
        if (cameraRail.right > cameraRail.left) {
            DrawInsetFrame(dc,
                           cameraRailInner,
                           ri::editor::EditorUiTheme::kWellFill,
                           ri::editor::EditorUiTheme::kWellHi,
                           ri::editor::EditorUiTheme::kWellShadow);
        }

        const int leftPanelClip = SaveDC(dc);
        IntersectClipRect(dc, hierarchyInner.left, hierarchyInner.top, hierarchyInner.right, hierarchyInner.bottom);

        if (leftPanelCollapsed_) {
            DrawTextLine(dc,
                         RECT{hierarchyInner.left + 4, hierarchyInner.top + 40, hierarchyInner.right - 4, hierarchyInner.bottom - 8},
                         "Left panel collapsed. Click » in the header or press Ctrl+[.",
                         RGB(170, 176, 186),
                         smallFont_,
                         DT_LEFT | DT_WORDBREAK);
        } else {
        DrawToolbarButton(dc,
                         RECT{hierarchyInner.left + 6,
                              hierarchyInner.top + 4,
                              hierarchyInner.left + 68,
                              hierarchyInner.top + 4 + kLeftPanelTabHeight_},
                         "Scene",
                         leftPanelMode_ == LeftPanelMode::Scene,
                         ri::editor::EditorToolbarStyle::Light);
        DrawToolbarButton(dc,
                         CreateTabRect(hierarchyInner),
                         "Create",
                         leftPanelMode_ == LeftPanelMode::Create,
                         ri::editor::EditorToolbarStyle::Light);
        DrawToolbarButton(dc,
                         RECT{hierarchyInner.left + 140,
                              hierarchyInner.top + 4,
                              hierarchyInner.left + 248,
                              hierarchyInner.top + 4 + kLeftPanelTabHeight_},
                         "Resources",
                         leftPanelMode_ == LeftPanelMode::Resources,
                         ri::editor::EditorToolbarStyle::Light);

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
            const ri::editor::ResourceCategoryChipLayout chipLayout =
                ri::editor::ComputeResourceCategoryChipLayout(hierarchyInner);
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
            for (int index = 0; index < static_cast<int>(categories.size()); ++index) {
                const WorkspaceResourceCategory category = categories[static_cast<std::size_t>(index)];
                const bool active = (resourceCategoryMask_ & WorkspaceCategoryBit(category)) != 0u;
                DrawToolbarButton(dc,
                                  ri::editor::ResourceCategoryChipRect(hierarchyInner, index, chipLayout),
                                  WorkspaceCategoryShortLabel(category),
                                  active);
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
        } else if (leftPanelMode_ == LeftPanelMode::Scene) {
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

        if (leftPanelMode_ == LeftPanelMode::Create) {
            RenderCreatorPanel(
                dc,
                hierarchyInner,
                ComputeCreatorPanelLayout(hierarchyInner),
                BuildCreatorPanelModel(),
                headerFont_,
                bodyFont_,
                smallFont_,
                [this](HDC paintDc, const RECT& rect, const std::string& label, const bool active) {
                    DrawToolbarButton(paintDc, rect, label, active, ri::editor::EditorToolbarStyle::Creator);
                });
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
                    " of " + std::to_string(hierarchyOrder.size()) +
                    "  |  wheel scroll  |  dbl-click / F frame";
                DrawTextLine(dc,
                             RECT{hierarchyInner.left + 6,
                                  hierarchyInner.bottom - 22,
                                  hierarchyInner.right - 6,
                                  hierarchyInner.bottom - 4},
                             scrollHint,
                             RGB(36, 38, 42),
                             smallFont_,
                             DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            } else if (!hierarchyOrder.empty()) {
                DrawTextLine(dc,
                             RECT{hierarchyInner.left + 6,
                                  hierarchyInner.bottom - 22,
                                  hierarchyInner.right - 6,
                                  hierarchyInner.bottom - 4},
                             std::to_string(hierarchyOrder.size()) + " nodes  |  dbl-click / F frame  |  Home frame all",
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
                RECT rowRect{hierarchyInner.left + 6, y, hierarchyInner.right - 6, y + kHierarchyRowHeight_};
                if (static_cast<std::size_t>(nodeIndex) == selectedNode_) {
                    FillRectColor(dc, rowRect, ri::editor::EditorUiTheme::kSelSceneFill);
                }
                DrawTextLine(dc,
                             RECT{rowRect.left + indent, rowRect.top, rowRect.right - 90, rowRect.bottom},
                             std::to_string(nodeIndex) + "  " + node.name,
                             static_cast<std::size_t>(nodeIndex) == selectedNode_ ? ri::editor::EditorUiTheme::kSelSceneText
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
        } else if (leftPanelMode_ == LeftPanelMode::Resources) {
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
                        FillRectColor(dc, rowRect, ri::editor::EditorUiTheme::kSelResourceFill);
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

        }

        RestoreDC(dc, leftPanelClip);

        if (!rightPanelCollapsed_) {
        const RECT inspectorTabShelf{inspectorInner.left + 6, inspectorInner.top + 6, inspectorInner.right - 6, inspectorInner.top + 36};
        DrawInsetFrame(dc,
                       inspectorTabShelf,
                       ri::editor::EditorUiTheme::kWellFill,
                       ri::editor::EditorUiTheme::kWellHi,
                       ri::editor::EditorUiTheme::kWellShadow);
        FillRectColor(dc,
                      RECT{inspectorTabShelf.left + 1, inspectorTabShelf.top + 1, inspectorTabShelf.right - 1, inspectorTabShelf.top + 4},
                      ri::editor::EditorUiTheme::kHeaderAccent);
        const InspectorTabLayout inspectorTabs = ComputeInspectorTabLayout(inspectorInner);
        DrawToolbarButton(dc,
                         inspectorTabs.nodeTab,
                         "Node",
                         inspectorPanel_ == InspectorPanel::Node,
                         ri::editor::EditorToolbarStyle::Light);
        DrawToolbarButton(dc,
                         inspectorTabs.brushTab,
                         "Brush",
                         inspectorPanel_ == InspectorPanel::Brush,
                         ri::editor::EditorToolbarStyle::Light);
        DrawToolbarButton(dc,
                         inspectorTabs.gameplayTab,
                         "Game",
                         inspectorPanel_ == InspectorPanel::Gameplay,
                         ri::editor::EditorToolbarStyle::Light);
        DrawToolbarButton(dc,
                         inspectorTabs.filesTab,
                         "Files",
                         inspectorPanel_ == InspectorPanel::Files,
                         ri::editor::EditorToolbarStyle::Light);
        DrawToolbarButton(dc,
                         inspectorTabs.storeTab,
                         "Store",
                         inspectorPanel_ == InspectorPanel::PluginStore,
                         ri::editor::EditorToolbarStyle::Light);
        DrawToolbarButton(dc,
                         inspectorTabs.uiWorkbenchTab,
                         "UI/VN",
                         inspectorPanel_ == InspectorPanel::UiWorkbench,
                         ri::editor::EditorToolbarStyle::Light);

        const int inspectorContentClip = SaveDC(dc);
        IntersectClipRect(dc,
                          inspectorInner.left,
                          inspectorInner.top + 44,
                          inspectorInner.right,
                          ri::editor::InspectorContentBottom(inspectorInner));

        int infoTop = inspectorInner.top + 48;
        if (inspectorPanel_ == InspectorPanel::Files) {
            const WorkspaceResourceEntry* selectedResourceEntry = nullptr;
            if (selectedResourceRow_ >= 0
                && selectedResourceRow_ < static_cast<int>(resourceCatalogEntries_.size())) {
                selectedResourceEntry = &resourceCatalogEntries_[static_cast<std::size_t>(selectedResourceRow_)];
            }
            const ri::editor::FilesInspectorPanelModel filesPanel =
                BuildFilesInspectorPanelModelForWindow(selectedResourceEntry);
            RenderFilesInspectorPanel(
                dc,
                inspectorInner,
                filesPanel,
                headerFont_,
                smallFont_,
                [this](HDC innerDc, const RECT& rect, const std::string& label, const bool active) {
                    DrawToolbarButton(innerDc, rect, label, active);
                });
        } else if (inspectorPanel_ == InspectorPanel::PluginStore) {
            PluginStorePanelModel storeModel = BuildPluginStorePanelModel();
            RenderPluginStorePanel(
                dc,
                inspectorInner,
                storeModel,
                headerFont_,
                bodyFont_,
                smallFont_,
                [this](HDC innerDc, const RECT& rect, const std::string& label, bool active) {
                    this->DrawToolbarButton(innerDc, rect, label, active);
                });
            pluginStoreLayout_ = storeModel.layout;
            pluginStoreScrollRow_ = storeModel.layout.scrollTopRow;
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
                materialNudgeButtons_.fill(RECT{});
                lightNudgeButtons_.fill(RECT{});
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
                if (node.material != ri::scene::kInvalidHandle
                    && static_cast<std::size_t>(node.material) < starterScene_.scene.MaterialCount()) {
                    const ri::scene::Material& material = starterScene_.scene.GetMaterial(node.material);
                    model.hasMaterial = true;
                    model.materialEditable = IsEditableAuthoredNode(static_cast<int>(selectedNode_));
                    model.materialNameLine = "Name: " + material.name;
                    model.materialColorLine =
                        "Base color: " + ri::math::ToString(material.baseColor);
                    {
                        std::ostringstream roughStream;
                        roughStream.setf(std::ios::fixed);
                        roughStream.precision(2);
                        roughStream << material.roughness;
                        model.materialRoughnessLine = "Roughness: " + roughStream.str();
                        roughStream.str({});
                        roughStream << material.metallic;
                        model.materialMetallicLine = "Metallic: " + roughStream.str();
                        roughStream.str({});
                        roughStream << material.opacity;
                        model.materialOpacityLine = "Opacity: " + roughStream.str();
                    }
                    model.materialTextureLine = material.baseColorTexture.empty()
                        ? "Albedo texture: (none)"
                        : ("Albedo texture: " + material.baseColorTexture);
                    model.materialFlagsLine =
                        std::string(material.transparent ? "transparent" : "opaque")
                        + (material.doubleSided ? " · double-sided" : "")
                        + (material.additiveBlend ? " · additive" : "");
                }
                if (node.light != ri::scene::kInvalidHandle
                    && static_cast<std::size_t>(node.light) < starterScene_.scene.LightCount()) {
                    const ri::scene::Light& light = starterScene_.scene.GetLight(node.light);
                    model.hasLight = true;
                    model.lightEditable = true;
                    model.lightTypeLine = "Type: " + ri::scene::ToString(light.type);
                    model.lightColorLine = "Color: " + ri::math::ToString(light.color);
                    {
                        std::ostringstream lightStream;
                        lightStream.setf(std::ios::fixed);
                        lightStream.precision(2);
                        lightStream << light.intensity;
                        model.lightIntensityLine = "Intensity: " + lightStream.str();
                        lightStream.str({});
                        lightStream << light.range;
                        model.lightRangeLine = "Range: " + lightStream.str();
                    }
                }
                if (IsTriggerNode(node)) {
                    const std::optional<ri::scene::WorldBounds> triggerBounds =
                        ri::scene::ComputeNodeWorldBounds(starterScene_.scene, static_cast<int>(selectedNode_), false);
                    model.hasTrigger = true;
                    if (triggerBounds.has_value()) {
                        model.triggerBoundsLine =
                            "World AABB: min " + ri::math::ToString(triggerBounds->min)
                            + "  max " + ri::math::ToString(triggerBounds->max);
                    } else {
                        model.triggerBoundsLine = "World AABB: unavailable (add/resize mesh)";
                    }
                    model.triggerHelpLine =
                        "Gameplay trigger volume. Wire in Logic tab · Ctrl+Shift+I import · Ctrl+E exports triggers CSV.";
                }
                RenderNodeInspectorPanel(
                    dc,
                    inspectorInner,
                    model,
                    headerFont_,
                    bodyFont_,
                    smallFont_,
                    [this](HDC innerDc, int& top, const RECT& innerRect, const char* label, int componentIndex) {
                        this->DrawInspectorNudgeRow(innerDc, top, innerRect, label, componentIndex);
                    },
                    [this](HDC innerDc, int& top, const RECT& innerRect, const char* label, int fieldIndex) {
                        this->DrawMaterialNudgeRow(innerDc, top, innerRect, label, fieldIndex);
                    },
                    [this](HDC innerDc, int& top, const RECT& innerRect, const char* label, int fieldIndex) {
                        this->DrawLightNudgeRow(innerDc, top, innerRect, label, fieldIndex);
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
                if (!node.structuralBrush.brushId.empty()) {
                    const ri::scene::StructuralBrushMetadata& metadata = node.structuralBrush;
                    const ri::scene::StructuralBrushValidationReport validation =
                        ri::scene::ValidateStructuralBrushMetadata(metadata);
                    model.hasStructuralMetadata = true;
                    model.structuralMetadataValid = validation.valid && validation.warnings.empty();
                    model.semanticIdentityLine = "Id: " + metadata.brushId + "  |  role "
                        + ri::scene::ToString(metadata.role) + "  |  region "
                        + (metadata.region.empty() ? "(none)" : metadata.region);
                    model.semanticPolicyLine = "Op " + ri::scene::ToString(metadata.operation)
                        + "  |  collision " + ri::scene::ToString(metadata.collision)
                        + "  |  nav " + ri::scene::ToString(metadata.navigation);
                    model.semanticChannelsLine = "Channels: "
                        + std::string(ri::scene::StructuralBrushParticipatesInChannel(
                                          metadata, ri::scene::StructuralBrushChannel::VisualMesh)
                                          ? "M"
                                          : "-")
                        + (ri::scene::StructuralBrushParticipatesInChannel(
                               metadata, ri::scene::StructuralBrushChannel::PhysicsMesh)
                               ? "P"
                               : "-")
                        + (ri::scene::StructuralBrushParticipatesInChannel(
                               metadata, ri::scene::StructuralBrushChannel::QueryMesh)
                               ? "Q"
                               : "-")
                        + (ri::scene::StructuralBrushParticipatesInChannel(
                               metadata, ri::scene::StructuralBrushChannel::InformationLayer)
                               ? "I"
                               : "-")
                        + "  |  rebuild " + ri::scene::ToString(metadata.rebuildScope);
                    if (!validation.errors.empty()) {
                        model.semanticValidationLine = "Invalid: " + validation.errors.front();
                    } else if (!validation.warnings.empty()) {
                        model.semanticValidationLine = "Warning: " + validation.warnings.front();
                    } else {
                        model.semanticValidationLine = "Validated: structural ownership is complete.";
                    }
                }
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
            const RECT welcomeCard{inspectorInner.left + 10, infoTop, inspectorInner.right - 10, infoTop + 96};
            DrawInsetFrame(dc, welcomeCard, RGB(58, 64, 74), RGB(170, 176, 184), RGB(22, 26, 32));
            std::string welcomeText =
                "Choose a scene node to edit transforms, materials, and lights. Use Project Archive / Files for game resources.";
            if (sceneConfig_.gameManifest.has_value()) {
                const ri::editor::ProjectHealthReport health = BuildProjectHealthReport(
                    *sceneConfig_.gameManifest,
                    ResolveDedicatedPlaytestExecutable(*sceneConfig_.gameManifest),
                    CanResolvePlaytestExecutable(*sceneConfig_.gameManifest));
                welcomeText = SummarizeProjectHealthForWelcome(health) + "\n\n" + welcomeText;
            }
            DrawTextLine(dc,
                         RECT{welcomeCard.left + 10, welcomeCard.top + 10, welcomeCard.right - 10, welcomeCard.bottom - 10},
                         welcomeText,
                         RGB(200, 200, 200),
                         smallFont_,
                         DT_LEFT | DT_WORDBREAK);
        }

        RestoreDC(dc, inspectorContentClip);

        {
            const int sessionTop = ri::editor::InspectorContentBottom(inspectorInner) + 8;
            const RECT sessionCard{inspectorInner.left + 10, sessionTop, inspectorInner.right - 10, inspectorInner.bottom - 8};
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

        } else {
            DrawTextLine(dc,
                         RECT{inspectorInner.left + 8, inspectorInner.top + 40, inspectorInner.right - 8, inspectorInner.bottom - 8},
                         "Inspector collapsed. Click » in the header or press Ctrl+].",
                         RGB(170, 176, 186),
                         smallFont_,
                         DT_LEFT | DT_WORDBREAK);
        }

        if (cameraRail.right > cameraRail.left) {
            ri::editor::RenderEditorCameraRail(
                dc,
                cameraRailInner,
                viewportTheme,
                ri::editor::CameraRailVisualModel{
                    .orbitOffsetX = orbitRailPad_.offsetX,
                    .orbitOffsetY = orbitRailPad_.offsetY,
                    .panOffsetX = panRailPad_.offsetX,
                    .panOffsetY = panRailPad_.offsetY,
                    .depthOffsetX = depthRailPad_.offsetX,
                    .depthOffsetY = depthRailPad_.offsetY,
                    .orbitActive = cameraDragMode_ == CameraDragMode::RailTrackball,
                    .panActive = cameraDragMode_ == CameraDragMode::RailPan,
                    .depthActive = cameraDragMode_ == CameraDragMode::RailDepth,
                    .resolutionScalingEnabled = viewportResolutionScalingEnabled_,
                });
        }

        if (inspectorPanel_ == InspectorPanel::UiWorkbench) {
            DrawUiWorkbenchViewport(dc, viewportInner);
        } else {
            UpdateCameraPlotRect(viewportInner);
            DrawRawIronQuadViewportBlock(dc, viewportInner);
            DrawCreateModeStampCursor(dc);
            if (ShouldShowStructuralPicker()) {
                const StructuralPickerLayout pickerLayout = CurrentStructuralPickerLayout(viewportInner);
                RenderStructuralPickerOverlay(
                    dc,
                    pickerLayout,
                    ri::editor::StructuralPickerModel{
                        .visible = true,
                        .section = authoringCatalogSection_,
                        .selectedPresetIndex = SelectedCatalogPresetIndex(authoringCatalogSection_),
                        .hoveredPresetIndex = structuralPickerHovered_,
                        .scrollTopRow = structuralPickerScrollRow_,
                    },
                    structuralThumbnailCache_,
                    ResolveEditorTextureRoot(),
                    ri::editor::StructuralPickerTheme{
                        .headerFont = headerFont_,
                        .bodyFont = bodyFont_,
                        .smallFont = smallFont_,
                    },
                    [this](HDC paintDc, const RECT& rect, const std::string& label, const bool active) {
                        DrawToolbarButton(paintDc, rect, label, active);
                    });
            } else if (ShouldShowAuthoringCatalogChrome()) {
                RenderStructuralPickerCollapsedBar(
                    dc,
                    ComputeStructuralPickerCollapsedBarRect(viewportInner),
                    ri::editor::StructuralPickerTheme{
                        .headerFont = headerFont_,
                        .bodyFont = bodyFont_,
                        .smallFont = smallFont_,
                    },
                    [this](HDC paintDc, const RECT& rect, const std::string& label, const bool active) {
                        DrawToolbarButton(paintDc, rect, label, active);
                    });
            }
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
                    "S select · C create · click scene to stamp · Sky bar cycles atmosphere · Tab layout · F1 help",
                .stateLine = "Status: " + lastIoStatus_ + "  |  Scene state: " + ResolveSceneStatePath().string(),
            },
            ri::editor::EditorViewportTheme{
                .titleFont = titleFont_,
                .headerFont = headerFont_,
                .bodyFont = bodyFont_,
                .smallFont = smallFont_,
                .monoFont = monoFont_,
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
        if (hasLastMouseClientPos_) {
            ri::editor::RenderEditorToolbarTooltip(dc, toolStrip, lastMouseClientPos_, viewportTheme);
        }
        ri::editor::PresentEditorFrame(windowDc, dc, width, height, excludedClientRectPtr, &paint.rcPaint);
        EndPaint(hwnd_, &paint);
    }

    bool EnsureEditorBackBuffer(HDC windowDc, const int width, const int height) {
        if (backBufferDc_ == nullptr) {
            backBufferDc_ = CreateCompatibleDC(windowDc);
            if (backBufferDc_ == nullptr) {
                return false;
            }
        }
        if (!ri::editor::NeedsEditorBackBufferResize(backBufferWidth_, backBufferHeight_, width, height)
            && backBufferBitmap_ != nullptr) {
            return true;
        }
        if (backBufferBitmap_ != nullptr) {
            SelectObject(backBufferDc_, backBufferDefaultBitmap_);
            DeleteObject(backBufferBitmap_);
            backBufferBitmap_ = nullptr;
        }
        backBufferBitmap_ = CreateCompatibleBitmap(windowDc, width, height);
        if (backBufferBitmap_ == nullptr) {
            backBufferWidth_ = 0;
            backBufferHeight_ = 0;
            return false;
        }
        backBufferDefaultBitmap_ = SelectObject(backBufferDc_, backBufferBitmap_);
        backBufferWidth_ = width;
        backBufferHeight_ = height;
        return true;
    }

    void DestroyEditorBackBuffer() {
        ri::editor::EditorRenderer::ReleaseSoftwareImageBlitCache(viewportPreviewBlitCache_);
        if (backBufferDc_ != nullptr && backBufferBitmap_ != nullptr) {
            SelectObject(backBufferDc_, backBufferDefaultBitmap_);
            DeleteObject(backBufferBitmap_);
        }
        if (backBufferDc_ != nullptr) {
            DeleteDC(backBufferDc_);
        }
        backBufferDc_ = nullptr;
        backBufferBitmap_ = nullptr;
        backBufferDefaultBitmap_ = nullptr;
        backBufferWidth_ = 0;
        backBufferHeight_ = 0;
    }

    HWND hwnd_ = nullptr;
    HDC backBufferDc_ = nullptr;
    HBITMAP backBufferBitmap_ = nullptr;
    HGDIOBJ backBufferDefaultBitmap_ = nullptr;
    int backBufferWidth_ = 0;
    int backBufferHeight_ = 0;
    HFONT titleFont_ = nullptr;
    HFONT headerFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    HFONT monoFont_ = nullptr;
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
    std::size_t volumeCatalogPresetIndex_ = 0;
    std::size_t logicCatalogPresetIndex_ = 0;
    ri::editor::AuthoringCatalogSection authoringCatalogSection_ = ri::editor::AuthoringCatalogSection::Structural;
    std::uint32_t lightSpawnTypeIndex_ = 0U;
    ri::editor::EditorLogicLayer logicLayer_{};
    std::size_t selectedNode_ = 0;
    int hierarchyScrollTopRow_ = 0;
    double elapsedSeconds_ = 0.0;
    std::chrono::steady_clock::time_point lastTick_{};
    std::chrono::steady_clock::time_point lastAutosaveSteady_{};
    std::chrono::steady_clock::time_point lastVulkanViewportPublish_{};
    std::chrono::steady_clock::time_point lastRailInputSteady_{};
    std::chrono::steady_clock::time_point lastViewportResizeSteady_{};
    double lastAutosaveStatusSeconds_ = -999.0;
    bool autosavePending_ = false;
    ri::scene::StarterScene starterScene_{};
    ri::scene::Scene baselineStarterScene_{};
    ri::scene::SemanticStructuralPartitionCache structuralPlacementPartitionCache_{};
    std::size_t authoredNodeStart_ = 0;
    ri::world::InventoryPolicy creatorInventoryPolicy_{};
    bool statsOverlayVisible_ = false;
    ri::world::RuntimeStatsOverlayState statsOverlayState_{true};
    ri::scene::OrbitCameraState editorOrbitState_{};
    bool autoOrbitPreview_ = false;
    bool viewportRayTracePreview_ = false;
    bool viewportGpuAllowed_ = false;
    bool viewportResolutionScalingEnabled_ = true;
    bool viewportGpuEnabled_ = false;
    ri::editor::EditorVulkanViewport vulkanViewport_{};
    bool viewportPreviewReady_ = false;
    double lastViewportPreviewMs_ = 0.0;
    bool viewportPreviewDirty_ = true;
    bool viewportSceneSnapshotDirty_ = true;
    bool viewportRestartPending_ = false;
    bool liveWindowMoveSize_ = false;
    ri::render::software::ScenePreviewCache viewportPreviewCache_{};
    ri::render::software::SoftwareImage viewportPreviewScratch_{};
    ri::render::software::SoftwareImage viewportPreviewDisplayScratch_{};
    int lastViewportRenderWidth_ = 0;
    int lastViewportRenderHeight_ = 0;
    ri::editor::SoftwareImageBlitCache viewportPreviewBlitCache_{};
    std::uint64_t viewportPreviewBlitGeneration_ = 0;
    std::uint32_t logicPreviewFrameCounter_ = 0;
    bool full3DViewport_ = false;
    ri::editor::EditorToolMode toolMode_ = ri::editor::EditorToolMode::Select;
    static constexpr int kViewportWorldBarHeight_ = 28;
    POINT lastMouseClientPos_{};
    bool hasLastMouseClientPos_ = false;
    std::string hoveredToolbarTooltip_;
    std::optional<ri::math::Vec3> createModePlacementPoint_{};
    CameraDragMode cameraDragMode_ = CameraDragMode::None;
    RailPadState orbitRailPad_{};
    RailPadState panRailPad_{};
    RailPadState depthRailPad_{};
    bool draggingHierarchySplitter_ = false;
    bool draggingInspectorSplitter_ = false;
    int lastDragX_ = 0;
    int lastDragY_ = 0;
    RECT cameraPlotRect_{};
    int hierarchyPanelWidth_ = ri::editor::DefaultHierarchyPanelWidth();
    int inspectorPanelWidth_ = 320;
    bool leftPanelCollapsed_ = false;
    bool rightPanelCollapsed_ = false;
    bool authoringCatalogExpanded_ = false;
    RECT leftPanelCollapseToggleRect_{};
    RECT rightPanelCollapseToggleRect_{};
    ri::render::software::GamePreviewScriptTimestamps gamePreviewScriptTimestamps_{};
    std::chrono::steady_clock::time_point lastGamePreviewScriptPoll_{};
    fs::path editorTextureRoot_{};
    ri::render::software::ScenePreviewOptions viewportPreviewOptionsTemplate_{};
    CreatorAtmospherePreset creatorAtmospherePreset_ = CreatorAtmospherePreset::ClearDay;
    CreatorInsertPreset creatorInsertPreset_ = CreatorInsertPreset::GroundPlate;
    CreatorCameraPreset creatorCameraPreset_ = CreatorCameraPreset::Hero;

    /// Deleted authored nodes move here (meshes stripped); subtree hidden from hierarchy list.
    int editorTrashFolderHandle_ = ri::scene::kInvalidHandle;

    LeftPanelMode leftPanelMode_ = LeftPanelMode::Scene;
    NewGameTemplate newGameTemplate_ = NewGameTemplate::EmptyStudio;
    int newGameNameVariant_ = 0;
    int structuralPickerScrollRow_ = 0;
    std::size_t structuralPickerHovered_ = SIZE_MAX;
    StructuralThumbnailCache structuralThumbnailCache_{};
    std::vector<WorkspaceGameEntry> workspaceGames_;
    int focusedWorkspaceGameIndex_ = 0;
    std::vector<WorkspaceResourceEntry> resourceCatalogEntries_;
    std::vector<int> filteredResourceRows_;
    std::vector<int> filteredHierarchyOrder_;
    GameplayPanelLayout gameplayPanelLayout_{};
    UiWorkbenchLayout uiWorkbenchLayout_{};
    PluginStoreLayout pluginStoreLayout_{};
    ri::content::PluginProjectData pluginProjectData_{};
    std::vector<PluginStorePackage> pluginStorePackages_{};
    int pluginStoreScrollRow_ = 0;
    std::string pluginStoreStatusLine_;
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
    std::array<RECT, 6> materialNudgeButtons_{};
    std::array<RECT, 2> lightNudgeButtons_{};
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
