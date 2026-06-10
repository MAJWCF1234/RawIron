#pragma once

#include "RawIron/Logic/LogicAuthoring.h"
#include "RawIron/Logic/LogicAuthoringSenseRuntime.h"
#include "RawIron/Logic/LogicGraph.h"
#include "RawIron/Logic/LogicKitManifest.h"
#include "RawIron/Logic/LogicVisualPrimitives.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Trace/TraceScene.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ri::editor {

struct EditorLogicWireRecord {
    std::string wireId;
    std::string sourceLogicId;
    std::string outputName;
    std::string targetLogicId;
    std::string inputName;
    std::vector<int> sceneHandles;
};

struct EditorLogicPlacedNode {
    std::string logicNodeId;
    std::string kitId;
    std::vector<int> sceneHandles;
    std::unordered_map<std::string, ri::math::Vec3> inputPortWorld{};
    std::unordered_map<std::string, ri::math::Vec3> outputPortWorld{};
    ri::math::Vec3 position{};
    ri::math::Vec3 visibleScale{1.0f, 1.0f, 1.0f};
    bool executable = true;
};

struct EditorLogicPlaceResult {
    bool placed = false;
    bool wiredToPrevious = false;
    std::string logicNodeId;
    std::string message;
};

struct EditorLogicWirePickState {
    bool armed = false;
    std::string sourceId;
    std::string outputName;
    bool worldActorSource = false;
};

struct EditorLogicWirePickResult {
    bool handled = false;
    bool completedWire = false;
    std::string message;
};

class EditorLogicLayer {
public:
    void Reset();
    void EnsureKitLoaded(const std::filesystem::path& workspaceRoot);
    void EnsureGameColliderTrace(const std::filesystem::path& gameRoot);

    [[nodiscard]] std::size_t LogicCatalogCount() const;
    [[nodiscard]] const ri::logic::LogicKitNodeManifestEntry& LogicCatalogEntry(std::size_t index) const;
    [[nodiscard]] bool LogicCatalogEntryExecutable(std::size_t index) const;

    [[nodiscard]] EditorLogicPlaceResult PlaceKitNode(ri::scene::Scene& scene,
                                                    int worldRoot,
                                                    std::string_view kitId,
                                                    const ri::math::Vec3& position,
                                                    bool wireToPrevious,
                                                    std::optional<std::string_view> forcedLogicNodeId = std::nullopt);

    void AddWire(ri::scene::Scene& scene,
                 std::string_view sourceLogicId,
                 std::string_view outputName,
                 std::string_view targetLogicId,
                 std::string_view inputName);

    void SetCreatorLayerVisible(ri::scene::Scene& scene, bool visible);
    void SetPlayerPreviewHidden(ri::scene::Scene& scene, bool hidden);
    [[nodiscard]] bool IsCreatorLayerVisible() const { return creatorLayerVisible_; }
    [[nodiscard]] bool IsPlayerPreviewHidden() const { return playerPreviewHidden_; }

    [[nodiscard]] ri::logic::LogicAuthoringGraph BuildAuthoringGraph() const;
    [[nodiscard]] ri::logic::LogicAuthoringCompileResult Recompile(ri::scene::Scene& scene);
    void ApplyCircuitProbeColors(ri::scene::Scene& scene);
    void TickSenseProbes(const ri::math::Vec3& probeWorldPosition);

    void PulseTestInput(std::string_view logicNodeId, std::string_view inputName);
    bool PulseMostRecentNode(ri::scene::Scene& scene, std::size_t inputIndex = 0);
    bool PulseLogicNodeAtSceneHandle(ri::scene::Scene& scene, int sceneHandle, std::size_t inputIndex = 0);

    [[nodiscard]] EditorLogicWirePickResult HandleWirePickAtSceneNode(ri::scene::Scene& scene, int sceneHandle);
    void ClearWirePick();
    void CycleWirePickPort(int delta);
    bool PulseSelectedTrigger(ri::scene::Scene& scene, int sceneHandle);
    [[nodiscard]] const EditorLogicWirePickState& WirePickState() const { return wirePickState_; }

    [[nodiscard]] bool Save(const std::filesystem::path& path, const ri::scene::Scene& scene) const;
    [[nodiscard]] bool Load(const std::filesystem::path& path, ri::scene::Scene& scene, int worldRoot);

    [[nodiscard]] const std::vector<EditorLogicPlacedNode>& PlacedNodes() const { return placedNodes_; }
    [[nodiscard]] const std::vector<EditorLogicWireRecord>& Wires() const { return wires_; }
    [[nodiscard]] const std::string& LastCompileSummary() const { return lastCompileSummary_; }

    void RebuildAllWireVisuals(ri::scene::Scene& scene);

private:
    [[nodiscard]] std::string NextLogicNodeId(std::string_view kitId);
    [[nodiscard]] std::string NextWireId();
    void RegisterSceneHandle(EditorLogicPlacedNode& node, int handle, const ri::math::Vec3& visibleScale);
    void ApplyVisibilityToNode(ri::scene::Scene& scene, const EditorLogicPlacedNode& node) const;
    [[nodiscard]] bool TryAutoWire(ri::scene::Scene& scene,
                                   const EditorLogicPlacedNode& source,
                                   const EditorLogicPlacedNode& target);
    void ExtractPortAnchors(EditorLogicPlacedNode& node,
                            std::string_view kitId,
                            const std::vector<ri::logic::LogicVisualPrimitiveInstance>& instances);
    [[nodiscard]] std::optional<ri::math::Vec3> ResolvePortWorld(const EditorLogicPlacedNode& node,
                                                                 std::string_view portName,
                                                                 bool input) const;
    void SpawnWireVisual(ri::scene::Scene& scene, EditorLogicWireRecord& wire);
    void HideWireVisuals(ri::scene::Scene& scene, const EditorLogicWireRecord& wire) const;
    void ApplyVisibilityToWire(ri::scene::Scene& scene, const EditorLogicWireRecord& wire) const;
    [[nodiscard]] const EditorLogicPlacedNode* FindPlacedNode(std::string_view logicNodeId) const;
    [[nodiscard]] std::optional<std::string> FindLogicNodeIdForSceneHandle(int sceneHandle) const;
    [[nodiscard]] std::optional<std::string> ResolvePortNameFromStubSceneNode(std::string_view nodeName,
                                                                              std::string_view kitId,
                                                                              bool input) const;
    [[nodiscard]] std::optional<ri::math::Vec3> ResolveWireEndpointWorld(ri::scene::Scene& scene,
                                                                         std::string_view actorId,
                                                                         std::string_view portName,
                                                                         bool input) const;
    [[nodiscard]] ri::logic::LogicAuthoringCompileOptions BuildCompileOptionsFromScene(
        const ri::scene::Scene& scene) const;
    void ApplyWireProbeColors(ri::scene::Scene& scene,
                              const std::vector<ri::logic::LogicCircuitNodeProbe>& probes);

    std::filesystem::path workspaceRoot_{};
    std::unique_ptr<ri::logic::LogicKitManifest> kitManifest_{};
    std::unique_ptr<ri::logic::LogicGraph> runtimeGraph_{};
    std::vector<EditorLogicPlacedNode> placedNodes_{};
    std::vector<EditorLogicWireRecord> wires_{};
    std::string lastCompileSummary_{};
    std::string lastPlacedLogicNodeId_{};
    bool creatorLayerVisible_ = true;
    bool playerPreviewHidden_ = false;
    int logicFolderHandle_ = ri::scene::kInvalidHandle;
    int logicWireFolderHandle_ = ri::scene::kInvalidHandle;
    int wireVisualSerial_ = 0;
    EditorLogicWirePickState wirePickState_{};
    ri::logic::LogicAuthoringSenseRuntimeState senseRuntimeState_{};
    std::optional<ri::trace::TraceScene> editorTraceScene_{};
    std::filesystem::path editorTraceGameRoot_{};
};

} // namespace ri::editor
