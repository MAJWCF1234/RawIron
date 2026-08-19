#include "EditorLogicLayer.h"

#include "RawIron/Logic/LogicAuthoringSenseRuntime.h"
#include "RawIron/Logic/LogicKitNodeFactory.h"
#include "RawIron/Logic/LogicPortSchema.h"
#include "RawIron/Logic/LogicVisualPrimitives.h"
#include "RawIron/Logic/WorldActorPorts.h"
#include "RawIron/Spatial/Aabb.h"
#include "RawIron/Trace/TraceScene.h"
#include "RawIron/World/WorldLogicBridge.h"
#include "RawIron/Scene/ModelLoader.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ri::editor {
namespace {

namespace fs = std::filesystem;

using ri::scene::AddGltfModelNode;
using ri::scene::AddPrimitiveNode;
using ri::scene::GltfModelOptions;
using ri::scene::kInvalidHandle;
using ri::scene::PrimitiveNodeOptions;
using ri::scene::PrimitiveType;
using ri::scene::Scene;
using ri::scene::ShadingModel;

[[nodiscard]] ri::math::Vec3 ParseHexColor(std::string_view hex, const ri::math::Vec3& fallback) {
    if (hex.empty() || hex[0] != '#') {
        return fallback;
    }
    std::string digits(hex.substr(1));
    if (digits.size() != 6U) {
        return fallback;
    }
    auto parseByte = [](const char hi, const char lo) -> int {
        auto nybble = [](const char ch) -> int {
            if (ch >= '0' && ch <= '9') {
                return ch - '0';
            }
            if (ch >= 'a' && ch <= 'f') {
                return 10 + (ch - 'a');
            }
            if (ch >= 'A' && ch <= 'F') {
                return 10 + (ch - 'A');
            }
            return -1;
        };
        const int high = nybble(hi);
        const int low = nybble(lo);
        if (high < 0 || low < 0) {
            return -1;
        }
        return high * 16 + low;
    };
    const int r = parseByte(digits[0], digits[1]);
    const int g = parseByte(digits[2], digits[3]);
    const int b = parseByte(digits[4], digits[5]);
    if (r < 0 || g < 0 || b < 0) {
        return fallback;
    }
    return ri::math::Vec3{static_cast<float>(r) / 255.0f,
                          static_cast<float>(g) / 255.0f,
                          static_cast<float>(b) / 255.0f};
}

[[nodiscard]] std::string SanitizeToken(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
            out.push_back(ch);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

[[nodiscard]] std::optional<std::string> FirstPortName(const ri::logic::LogicNodePortSchema& schema, const bool input) {
    const std::vector<ri::logic::LogicPortDescriptor>& ports = input ? schema.inputs : schema.outputs;
    if (ports.empty()) {
        return std::nullopt;
    }
    return ports.front().name;
}

[[nodiscard]] ri::math::Vec3 QuadraticBezier(const ri::math::Vec3& p0,
                                               const ri::math::Vec3& p1,
                                               const ri::math::Vec3& p2,
                                               const float t) {
    const float u = 1.0f - t;
    return p0 * (u * u) + p1 * (2.0f * u * t) + p2 * (t * t);
}

[[nodiscard]] std::string TrimCopy(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::vector<std::string> SplitCsvLine(std::string_view line) {
    std::vector<std::string> tokens{};
    std::string current{};
    for (char ch : line) {
        if (ch == ',') {
            tokens.push_back(TrimCopy(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    tokens.push_back(TrimCopy(current));
    return tokens;
}

[[nodiscard]] ri::math::Vec3 ParseCsvVec3(const std::vector<std::string>& tokens,
                                            const std::size_t offset,
                                            const ri::math::Vec3& fallback) {
    if (tokens.size() < offset + 3U) {
        return fallback;
    }
    try {
        return ri::math::Vec3{std::stof(tokens[offset]),
                               std::stof(tokens[offset + 1U]),
                               std::stof(tokens[offset + 2U])};
    } catch (...) {
        return fallback;
    }
}

[[nodiscard]] std::vector<ri::trace::TraceCollider> LoadGameColliderCsv(const fs::path& csvPath) {
    std::vector<ri::trace::TraceCollider> colliders{};
    std::ifstream input(csvPath);
    if (!input.is_open()) {
        return colliders;
    }
    std::string line{};
    while (std::getline(input, line)) {
        line = TrimCopy(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> tokens = SplitCsvLine(line);
        if (tokens.size() < 7U) {
            continue;
        }
        const ri::math::Vec3 center = ParseCsvVec3(tokens, 1U, {});
        const ri::math::Vec3 extents = ParseCsvVec3(tokens, 4U, ri::math::Vec3{1.0f, 1.0f, 1.0f});
        colliders.push_back(ri::trace::TraceCollider{
            .id = tokens[0],
            .bounds = ri::spatial::Aabb{
                .min = center - extents,
                .max = center + extents,
            },
            .structural = true,
        });
    }
    return colliders;
}

[[nodiscard]] std::string NormalizeInputName(std::string_view inputName) {
    std::string out;
    out.reserve(inputName.size());
    for (char ch : inputName) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

} // namespace

void EditorLogicLayer::Reset() {
    placedNodes_.clear();
    wires_.clear();
    runtimeGraph_.reset();
    senseRuntimeState_ = {};
    editorTraceScene_.reset();
    editorTraceGameRoot_.clear();
    lastCompileSummary_.clear();
    lastPlacedLogicNodeId_.clear();
    logicFolderHandle_ = kInvalidHandle;
    logicWireFolderHandle_ = kInvalidHandle;
    wireVisualSerial_ = 0;
    wirePickState_ = {};
}

void EditorLogicLayer::EnsureKitLoaded(const fs::path& workspaceRoot) {
    workspaceRoot_ = workspaceRoot;
    if (kitManifest_) {
        return;
    }
    const fs::path manifestPath = workspaceRoot / std::string(ri::logic::kLogicKitNodesJsonRelative);
    if (std::optional<ri::logic::LogicKitManifest> loaded = ri::logic::LoadLogicKitManifest(manifestPath)) {
        kitManifest_ = std::make_unique<ri::logic::LogicKitManifest>(std::move(*loaded));
        ri::logic::SetActiveLogicKitManifest(kitManifest_.get());
    }
}

void EditorLogicLayer::EnsureGameColliderTrace(const fs::path& gameRoot) {
    if (gameRoot.empty() || gameRoot == editorTraceGameRoot_) {
        return;
    }
    editorTraceGameRoot_ = gameRoot;
    const fs::path colliderCsv = gameRoot / "levels" / "assembly.colliders.csv";
    std::vector<ri::trace::TraceCollider> colliders = LoadGameColliderCsv(colliderCsv);
    if (colliders.empty()) {
        editorTraceScene_.reset();
        return;
    }
    editorTraceScene_.emplace(std::move(colliders));
}

std::size_t EditorLogicLayer::LogicCatalogCount() const {
    return kitManifest_ ? kitManifest_->nodes.size() : 0U;
}

const ri::logic::LogicKitNodeManifestEntry& EditorLogicLayer::LogicCatalogEntry(const std::size_t index) const {
    static const ri::logic::LogicKitNodeManifestEntry kFallback{};
    if (!kitManifest_ || index >= kitManifest_->nodes.size()) {
        return kFallback;
    }
    return kitManifest_->nodes[index];
}

bool EditorLogicLayer::LogicCatalogEntryExecutable(const std::size_t index) const {
    if (!kitManifest_ || index >= kitManifest_->nodes.size()) {
        return false;
    }
    return ri::logic::LogicKitIdIsExecutable(kitManifest_->nodes[index].id);
}

std::string EditorLogicLayer::NextLogicNodeId(const std::string_view kitId) {
    const std::string prefix = std::string("LogicNode_") + SanitizeToken(kitId) + "_";
    std::size_t maxIndex = 0;
    for (const EditorLogicPlacedNode& node : placedNodes_) {
        if (node.logicNodeId.rfind(prefix, 0) != 0) {
            continue;
        }
        try {
            maxIndex = std::max(maxIndex, static_cast<std::size_t>(std::stoul(node.logicNodeId.substr(prefix.size()))));
        } catch (...) {
        }
    }
    return prefix + std::to_string(maxIndex + 1U);
}

std::string EditorLogicLayer::NextWireId() {
    return std::string("LogicWire_") + std::to_string(wires_.size() + 1U);
}

void EditorLogicLayer::RegisterSceneHandle(EditorLogicPlacedNode& node,
                                             const int handle,
                                             const ri::math::Vec3& visibleScale) {
    if (handle == kInvalidHandle) {
        return;
    }
    node.sceneHandles.push_back(handle);
    if (node.visibleScale.x <= 0.0f) {
        node.visibleScale = visibleScale;
    }
}

void EditorLogicLayer::ExtractPortAnchors(
    EditorLogicPlacedNode& node,
    const std::string_view kitId,
    const std::vector<ri::logic::LogicVisualPrimitiveInstance>& instances) {
    const ri::logic::LogicNodePortSchema schema = ri::logic::GetLogicNodePortSchema(kitId);
    for (const ri::logic::LogicVisualPrimitiveInstance& instance : instances) {
        const ri::math::Vec3 world{instance.worldPosition[0], instance.worldPosition[1], instance.worldPosition[2]};
        const std::string& instanceId = instance.id;
        const std::size_t colon = instanceId.rfind(':');
        const std::string stubId = colon == std::string::npos ? instanceId : instanceId.substr(colon + 1);
        if (instance.kind == ri::logic::LogicVisualPrimitiveKind::InputStub) {
            if (stubId.rfind("in_stub_", 0) == 0) {
                try {
                    const std::size_t index = static_cast<std::size_t>(std::stoul(stubId.substr(8)));
                    if (index < schema.inputs.size()) {
                        node.inputPortWorld[schema.inputs[index].name] = world;
                    }
                } catch (...) {
                }
            }
        } else if (instance.kind == ri::logic::LogicVisualPrimitiveKind::OutputStub) {
            if (stubId.rfind("out_stub_", 0) == 0) {
                try {
                    const std::size_t index = static_cast<std::size_t>(std::stoul(stubId.substr(9)));
                    if (index < schema.outputs.size()) {
                        node.outputPortWorld[schema.outputs[index].name] = world;
                    }
                } catch (...) {
                }
            }
        }
    }
}

const EditorLogicPlacedNode* EditorLogicLayer::FindPlacedNode(const std::string_view logicNodeId) const {
    for (const EditorLogicPlacedNode& node : placedNodes_) {
        if (node.logicNodeId == logicNodeId) {
            return &node;
        }
    }
    return nullptr;
}

std::optional<ri::math::Vec3> EditorLogicLayer::ResolvePortWorld(const EditorLogicPlacedNode& node,
                                                                 const std::string_view portName,
                                                                 const bool input) const {
    if (input) {
        const auto it = node.inputPortWorld.find(std::string(portName));
        if (it != node.inputPortWorld.end()) {
            return it->second;
        }
        return node.position + ri::math::Vec3{-1.0f, 0.0f, 0.0f};
    }
    const auto it = node.outputPortWorld.find(std::string(portName));
    if (it != node.outputPortWorld.end()) {
        return it->second;
    }
    return node.position + ri::math::Vec3{1.0f, 0.0f, 0.0f};
}

void EditorLogicLayer::HideWireVisuals(Scene& scene, const EditorLogicWireRecord& wire) const {
    for (const int handle : wire.sceneHandles) {
        if (handle == kInvalidHandle || handle >= static_cast<int>(scene.NodeCount())) {
            continue;
        }
        scene.GetNode(handle).localTransform.scale = ri::math::Vec3{0.001f, 0.001f, 0.001f};
    }
}

void EditorLogicLayer::ApplyVisibilityToWire(Scene& scene, const EditorLogicWireRecord& wire) const {
    constexpr float kBeadRadius = 0.10f;
    const ri::math::Vec3 visibleScale{kBeadRadius, kBeadRadius, kBeadRadius};
    const ri::math::Vec3 scale =
        playerPreviewHidden_ ? ri::math::Vec3{0.01f, 0.01f, 0.01f}
                             : (creatorLayerVisible_ ? visibleScale : ri::math::Vec3{0.01f, 0.01f, 0.01f});
    for (const int handle : wire.sceneHandles) {
        if (handle == kInvalidHandle || handle >= static_cast<int>(scene.NodeCount())) {
            continue;
        }
        scene.GetNode(handle).localTransform.scale = scale;
    }
}

void EditorLogicLayer::SpawnWireVisual(Scene& scene, EditorLogicWireRecord& wire) {
    const std::optional<ri::math::Vec3> from =
        ResolveWireEndpointWorld(scene, wire.sourceLogicId, wire.outputName, false);
    const std::optional<ri::math::Vec3> to =
        ResolveWireEndpointWorld(scene, wire.targetLogicId, wire.inputName, true);
    if (!from.has_value() || !to.has_value()) {
        return;
    }
    if (logicWireFolderHandle_ == kInvalidHandle || logicWireFolderHandle_ >= static_cast<int>(scene.NodeCount())) {
        if (logicFolderHandle_ == kInvalidHandle || logicFolderHandle_ >= static_cast<int>(scene.NodeCount())) {
            return;
        }
        logicWireFolderHandle_ = scene.CreateNode("LogicWires", logicFolderHandle_);
    }

    HideWireVisuals(scene, wire);
    wire.sceneHandles.clear();

    const ri::math::Vec3 delta = *to - *from;
    const float span = ri::math::Length(delta);
    if (span < 0.04f) {
        return;
    }
    const float lift = std::clamp(0.24f * span, 0.28f, 1.85f);
    const ri::math::Vec3 p0 = *from;
    const ri::math::Vec3 p2 = *to;
    const ri::math::Vec3 p1 = (p0 + p2) * 0.5f + ri::math::Vec3{0.0f, lift, 0.0f};
    const int beads = std::clamp(static_cast<int>(span / 0.18f), 10, 48);
    const float beadRadius = 0.10f;
    for (int i = 0; i <= beads; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(beads);
        const ri::math::Vec3 center = QuadraticBezier(p0, p1, p2, t);
        PrimitiveNodeOptions options{};
        options.parent = logicWireFolderHandle_;
        options.primitive = PrimitiveType::Sphere;
        options.shadingModel = ShadingModel::Unlit;
        options.nodeName = wire.wireId + "_bead_" + std::to_string(wireVisualSerial_++);
        options.materialName = "logic_wire_bead";
        options.baseColor = ri::math::Vec3{0.55f, 0.42f, 0.08f};
        options.emissiveColor = ri::math::Vec3{0.95f, 0.78f, 0.18f};
        options.transform.position = center;
        options.transform.scale = ri::math::Vec3{beadRadius, beadRadius, beadRadius};
        const int handle = AddPrimitiveNode(scene, options);
        if (handle != kInvalidHandle) {
            wire.sceneHandles.push_back(handle);
        }
    }
    ApplyVisibilityToWire(scene, wire);
}

void EditorLogicLayer::RebuildAllWireVisuals(Scene& scene) {
    for (EditorLogicWireRecord& wire : wires_) {
        SpawnWireVisual(scene, wire);
    }
}

void EditorLogicLayer::ApplyVisibilityToNode(Scene& scene, const EditorLogicPlacedNode& node) const {
    const ri::math::Vec3 scale =
        playerPreviewHidden_ ? ri::math::Vec3{0.01f, 0.01f, 0.01f}
                             : (creatorLayerVisible_ ? node.visibleScale : ri::math::Vec3{0.01f, 0.01f, 0.01f});
    for (const int handle : node.sceneHandles) {
        if (handle == kInvalidHandle || handle >= static_cast<int>(scene.NodeCount())) {
            continue;
        }
        scene.GetNode(handle).localTransform.scale = scale;
    }
}

EditorLogicPlaceResult EditorLogicLayer::PlaceKitNode(Scene& scene,
                                                      const int worldRoot,
                                                      const std::string_view kitId,
                                                      const ri::math::Vec3& position,
                                                      const bool wireToPrevious,
                                                      const std::optional<std::string_view> forcedLogicNodeId) {
    EditorLogicPlaceResult result{};
    if (worldRoot == kInvalidHandle) {
        result.message = "Scene has no world root.";
        return result;
    }
    if (logicFolderHandle_ == kInvalidHandle || logicFolderHandle_ >= static_cast<int>(scene.NodeCount())) {
        logicFolderHandle_ = scene.CreateNode("LogicLayer", worldRoot);
    }

    const std::string logicNodeId =
        forcedLogicNodeId.has_value() ? std::string(*forcedLogicNodeId) : NextLogicNodeId(kitId);
    const std::optional<ri::logic::LogicKitNodeFactoryResult> factory =
        ri::logic::CreateLogicNodeFromKitId(kitId, logicNodeId);

    const ri::logic::LogicKitNodeManifestEntry* kitEntry =
        kitManifest_ ? ri::logic::FindLogicKitNodeManifestEntry(*kitManifest_, kitId) : nullptr;
    const ri::math::Vec3 kitColor =
        kitEntry != nullptr ? ParseHexColor(kitEntry->colorHex, ri::math::Vec3{0.55f, 0.82f, 0.95f})
                            : ri::math::Vec3{0.55f, 0.82f, 0.95f};

    EditorLogicPlacedNode placed{
        .logicNodeId = logicNodeId,
        .kitId = std::string(kitId),
        .position = position,
        .visibleScale = ri::math::Vec3{1.0f, 1.0f, 1.0f},
        .executable = factory.has_value(),
    };

    bool importedKitMesh = false;
    if (kitEntry != nullptr && kitManifest_ && !workspaceRoot_.empty()) {
        const fs::path manifestPath = workspaceRoot_ / std::string(ri::logic::kLogicKitNodesJsonRelative);
        const fs::path glbPath = ri::logic::ResolveLogicKitGlbPath(manifestPath, kitEntry->glbRelative);
        std::error_code ec{};
        if (fs::is_regular_file(glbPath, ec)) {
            GltfModelOptions glb{};
            glb.sourcePath = glbPath;
            glb.wrapperNodeName = logicNodeId + "_Kit";
            glb.parent = logicFolderHandle_;
            glb.transform.position = position;
            const int glbRoot = AddGltfModelNode(scene, glb);
            if (glbRoot != kInvalidHandle) {
                RegisterSceneHandle(placed, glbRoot, ri::math::Vec3{1.0f, 1.0f, 1.0f});
                importedKitMesh = true;
            }
        }
    }

    const ri::logic::LogicVisualLibrary library = ri::logic::BuildDefaultLogicVisualLibrary();
    const std::array<float, 3> worldPos{position.x, position.y, position.z};
    const std::vector<ri::logic::LogicVisualPrimitiveInstance> layoutInstances =
        ri::logic::BuildLogicVisualNodeInstances(library, std::string(kitId), logicNodeId, worldPos, false);

    for (const ri::logic::LogicVisualPrimitiveInstance& instance : layoutInstances) {
        if (importedKitMesh && instance.kind == ri::logic::LogicVisualPrimitiveKind::NodeBody) {
            continue;
        }
        const ri::math::Vec3 instancePos{instance.worldPosition[0], instance.worldPosition[1], instance.worldPosition[2]};
        const ri::math::Vec3 instanceScale{instance.worldScale[0], instance.worldScale[1], instance.worldScale[2]};
        const ri::math::Vec3 instanceColor{instance.color[0], instance.color[1], instance.color[2]};
        const ri::math::Vec3 instanceEmissive{instance.emissive[0], instance.emissive[1], instance.emissive[2]};
        PrimitiveNodeOptions options{};
        options.parent = logicFolderHandle_;
        options.primitive = PrimitiveType::Cube;
        options.shadingModel = ShadingModel::Unlit;
        options.nodeName = instance.id;
        options.materialName = std::string("logic_") + SanitizeToken(kitId);
        options.baseColor = instanceColor * 0.35f + kitColor * 0.25f;
        options.emissiveColor = instanceEmissive + kitColor * 0.45f;
        options.transform.position = instancePos;
        options.transform.scale = instanceScale;
        const int handle = AddPrimitiveNode(scene, options);
        RegisterSceneHandle(placed, handle, instanceScale);
    }

    if (placed.sceneHandles.empty()) {
        PrimitiveNodeOptions fallback{};
        fallback.parent = logicFolderHandle_;
        fallback.primitive = PrimitiveType::Cube;
        fallback.shadingModel = ShadingModel::Unlit;
        fallback.nodeName = logicNodeId + "_Body";
        fallback.materialName = std::string("logic_") + SanitizeToken(kitId);
        fallback.baseColor = kitColor * 0.35f;
        fallback.emissiveColor = kitColor * 0.85f;
        fallback.transform.position = position;
        fallback.transform.scale = ri::math::Vec3{1.2f, 0.9f, 1.0f};
        RegisterSceneHandle(placed, AddPrimitiveNode(scene, fallback), fallback.transform.scale);
    }

    ExtractPortAnchors(placed, kitId, layoutInstances);
    ApplyVisibilityToNode(scene, placed);
    placedNodes_.push_back(placed);

    if (wireToPrevious && !lastPlacedLogicNodeId_.empty()) {
        const EditorLogicPlacedNode* previous = nullptr;
        for (const EditorLogicPlacedNode& candidate : placedNodes_) {
            if (candidate.logicNodeId == lastPlacedLogicNodeId_) {
                previous = &candidate;
                break;
            }
        }
        if (previous != nullptr && TryAutoWire(scene, *previous, placed)) {
            result.wiredToPrevious = true;
        }
    }

    lastPlacedLogicNodeId_ = logicNodeId;
    result.placed = true;
    result.logicNodeId = logicNodeId;
    if (!factory.has_value()) {
        result.message = "Placed visual-only LogicKit node '" + std::string(kitId)
            + "' (executor pending for this kit id).";
    } else if (result.wiredToPrevious) {
        result.message = "Placed and wired '" + logicNodeId + "' (" + std::string(kitId) + ").";
    } else {
        result.message = "Placed logic node '" + logicNodeId + "' (" + std::string(kitId) + "). Shift+Place to wire.";
    }

    (void)Recompile(scene);
    return result;
}

bool EditorLogicLayer::TryAutoWire(Scene& scene,
                                   const EditorLogicPlacedNode& source,
                                   const EditorLogicPlacedNode& target) {
    const ri::logic::LogicNodePortSchema sourceSchema = ri::logic::GetLogicNodePortSchema(source.kitId);
    const ri::logic::LogicNodePortSchema targetSchema = ri::logic::GetLogicNodePortSchema(target.kitId);
    const std::optional<std::string> outputName = FirstPortName(sourceSchema, false);
    const std::optional<std::string> inputName = FirstPortName(targetSchema, true);
    if (!outputName.has_value() || !inputName.has_value()) {
        return false;
    }
    AddWire(scene, source.logicNodeId, *outputName, target.logicNodeId, *inputName);
    return true;
}

void EditorLogicLayer::AddWire(Scene& scene,
                               const std::string_view sourceLogicId,
                               const std::string_view outputName,
                               const std::string_view targetLogicId,
                               const std::string_view inputName) {
    wires_.push_back(EditorLogicWireRecord{
        .wireId = NextWireId(),
        .sourceLogicId = std::string(sourceLogicId),
        .outputName = std::string(outputName),
        .targetLogicId = std::string(targetLogicId),
        .inputName = std::string(inputName),
    });
    SpawnWireVisual(scene, wires_.back());
    (void)Recompile(scene);
}

void EditorLogicLayer::SetCreatorLayerVisible(Scene& scene, const bool visible) {
    creatorLayerVisible_ = visible;
    for (const EditorLogicPlacedNode& node : placedNodes_) {
        ApplyVisibilityToNode(scene, node);
    }
    for (const EditorLogicWireRecord& wire : wires_) {
        ApplyVisibilityToWire(scene, wire);
    }
}

void EditorLogicLayer::SetPlayerPreviewHidden(Scene& scene, const bool hidden) {
    playerPreviewHidden_ = hidden;
    for (const EditorLogicPlacedNode& node : placedNodes_) {
        ApplyVisibilityToNode(scene, node);
    }
    for (const EditorLogicWireRecord& wire : wires_) {
        ApplyVisibilityToWire(scene, wire);
    }
}

ri::logic::LogicAuthoringGraph EditorLogicLayer::BuildAuthoringGraph() const {
    ri::logic::LogicAuthoringGraph graph{};
    graph.nodes.reserve(placedNodes_.size());
    for (const EditorLogicPlacedNode& placed : placedNodes_) {
        const std::optional<ri::logic::LogicKitNodeFactoryResult> factory =
            ri::logic::CreateLogicNodeFromKitId(placed.kitId, placed.logicNodeId);
        if (!factory.has_value()) {
            continue;
        }
        ri::logic::LogicNodeInstance instance{};
        instance.definition = factory->definition;
        instance.sourceKitId = placed.kitId;
        instance.placement.position = {placed.position.x, placed.position.y, placed.position.z};
        instance.placement.layer = "logic";
        instance.placement.debugVisible = creatorLayerVisible_;
        graph.nodes.push_back(std::move(instance));
    }

    graph.wires.reserve(wires_.size());
    for (const EditorLogicWireRecord& wire : wires_) {
        ri::logic::LogicAuthoringWire authoringWire{};
        authoringWire.id = wire.wireId;
        authoringWire.sourceId = wire.sourceLogicId;
        authoringWire.outputName = wire.outputName;
        ri::logic::LogicRouteTarget target{};
        target.targetId = wire.targetLogicId;
        target.inputName = wire.inputName;
        authoringWire.targets.push_back(target);
        graph.wires.push_back(std::move(authoringWire));
    }
    return graph;
}

ri::logic::LogicAuthoringCompileOptions EditorLogicLayer::BuildCompileOptionsFromScene(const Scene& scene) const {
    ri::logic::LogicAuthoringCompileOptions options{};
    for (std::size_t index = 0; index < scene.NodeCount(); ++index) {
        const ri::scene::Node& node = scene.GetNode(static_cast<int>(index));
        if (node.name.rfind("Trigger_", 0) != 0) {
            continue;
        }
        options.knownWorldActorIds.insert(node.name);
        options.knownWorldActorKinds[node.name] = "trigger_volume";
    }
    return options;
}

std::optional<std::string> EditorLogicLayer::FindLogicNodeIdForSceneHandle(const int sceneHandle) const {
    for (const EditorLogicPlacedNode& node : placedNodes_) {
        if (std::find(node.sceneHandles.begin(), node.sceneHandles.end(), sceneHandle) != node.sceneHandles.end()) {
            return node.logicNodeId;
        }
    }
    return std::nullopt;
}

std::optional<std::string> EditorLogicLayer::ResolvePortNameFromStubSceneNode(const std::string_view nodeName,
                                                                                const std::string_view kitId,
                                                                                const bool input) const {
    const std::size_t colon = nodeName.rfind(':');
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    const std::string stubId(nodeName.substr(colon + 1));
    const ri::logic::LogicNodePortSchema schema = ri::logic::GetLogicNodePortSchema(kitId);
    if (input) {
        if (stubId.rfind("in_stub_", 0) != 0) {
            return std::nullopt;
        }
        try {
            const std::size_t index = static_cast<std::size_t>(std::stoul(stubId.substr(8)));
            if (index < schema.inputs.size()) {
                return schema.inputs[index].name;
            }
        } catch (...) {
        }
        return std::nullopt;
    }
    if (stubId.rfind("out_stub_", 0) != 0) {
        return std::nullopt;
    }
    try {
        const std::size_t index = static_cast<std::size_t>(std::stoul(stubId.substr(9)));
        if (index < schema.outputs.size()) {
            return schema.outputs[index].name;
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<ri::math::Vec3> EditorLogicLayer::ResolveWireEndpointWorld(Scene& scene,
                                                                         const std::string_view actorId,
                                                                         const std::string_view portName,
                                                                         const bool input) const {
    if (const EditorLogicPlacedNode* placed = FindPlacedNode(actorId)) {
        return ResolvePortWorld(*placed, portName, input);
    }
    for (std::size_t index = 0; index < scene.NodeCount(); ++index) {
        const ri::scene::Node& node = scene.GetNode(static_cast<int>(index));
        if (node.name != actorId) {
            continue;
        }
        const ri::math::Vec3 worldCenter = scene.ComputeWorldPosition(static_cast<int>(index));
        if (node.name.rfind("Trigger_", 0) == 0) {
            return worldCenter + (input ? ri::math::Vec3{-0.6f, 0.0f, 0.0f} : ri::math::Vec3{0.6f, 0.0f, 0.0f});
        }
        return worldCenter;
    }
    return std::nullopt;
}

void EditorLogicLayer::ClearWirePick() {
    wirePickState_ = {};
}

EditorLogicWirePickResult EditorLogicLayer::HandleWirePickAtSceneNode(Scene& scene, const int sceneHandle) {
    EditorLogicWirePickResult result{};
    if (sceneHandle == kInvalidHandle || sceneHandle >= static_cast<int>(scene.NodeCount())) {
        result.message = "Wire pick: invalid scene node.";
        return result;
    }

    const ri::scene::Node& node = scene.GetNode(sceneHandle);
    std::optional<std::string> logicNodeId = FindLogicNodeIdForSceneHandle(sceneHandle);
    if (!logicNodeId.has_value()) {
        const std::size_t colon = node.name.rfind(':');
        if (colon != std::string::npos) {
            const std::string prefix = node.name.substr(0, colon);
            if (FindPlacedNode(prefix) != nullptr) {
                logicNodeId = prefix;
            }
        }
    }
    const bool isTrigger = node.name.rfind("Trigger_", 0) == 0;
    if (!logicNodeId.has_value() && !isTrigger) {
        result.message = "Wire pick: select a logic node, port stub, or Trigger_* volume.";
        return result;
    }

    result.handled = true;
    const std::string targetActorId = logicNodeId.has_value() ? *logicNodeId : node.name;

    if (wirePickState_.armed) {
        if (wirePickState_.sourceId == targetActorId) {
            ClearWirePick();
            result.message = "Wire pick cancelled.";
            return result;
        }
        if (!logicNodeId.has_value()) {
            result.message = "Wire pick: armed — select a logic node input to complete.";
            return result;
        }
        const EditorLogicPlacedNode* targetPlaced = FindPlacedNode(*logicNodeId);
        if (targetPlaced == nullptr) {
            result.message = "Wire pick: target logic node not found.";
            return result;
        }
        std::optional<std::string> inputName = ResolvePortNameFromStubSceneNode(node.name, targetPlaced->kitId, true);
        if (!inputName.has_value()) {
            const ri::logic::LogicNodePortSchema targetSchema = ri::logic::GetLogicNodePortSchema(targetPlaced->kitId);
            inputName = FirstPortName(targetSchema, true);
        }
        if (!inputName.has_value()) {
            result.message = "Wire pick: target has no inputs.";
            ClearWirePick();
            return result;
        }
        AddWire(scene,
                wirePickState_.sourceId,
                wirePickState_.outputName,
                *logicNodeId,
                *inputName);
        result.completedWire = true;
        result.message = "Wired " + wirePickState_.sourceId + "." + wirePickState_.outputName + " -> "
            + *logicNodeId + "." + *inputName;
        ClearWirePick();
        return result;
    }

    if (isTrigger) {
        wirePickState_ = EditorLogicWirePickState{
            .armed = true,
            .sourceId = node.name,
            .outputName = std::string(ri::logic::ports::kTriggerOnStartTouch),
            .worldActorSource = true,
        };
        result.message = "Wire armed: " + node.name + ".OnStartTouch -> select logic input, Alt+W to connect.";
        return result;
    }

    const EditorLogicPlacedNode* placed = FindPlacedNode(*logicNodeId);
    if (placed == nullptr) {
        result.message = "Wire pick: logic node not found.";
        return result;
    }
    std::optional<std::string> outputName = ResolvePortNameFromStubSceneNode(node.name, placed->kitId, false);
    if (!outputName.has_value()) {
        const ri::logic::LogicNodePortSchema sourceSchema = ri::logic::GetLogicNodePortSchema(placed->kitId);
        outputName = FirstPortName(sourceSchema, false);
    }
    if (!outputName.has_value()) {
        result.message = "Wire pick: node has no outputs.";
        return result;
    }
    wirePickState_ = EditorLogicWirePickState{
        .armed = true,
        .sourceId = *logicNodeId,
        .outputName = *outputName,
        .worldActorSource = false,
    };
    result.message = "Wire armed: " + *logicNodeId + "." + *outputName
        + " -> select input stub or node, Alt+W to connect (Alt+[ ] cycles ports).";
    return result;
}

void EditorLogicLayer::CycleWirePickPort(const int delta) {
    if (!wirePickState_.armed || wirePickState_.worldActorSource || delta == 0) {
        return;
    }
    const EditorLogicPlacedNode* placed = FindPlacedNode(wirePickState_.sourceId);
    if (placed == nullptr) {
        return;
    }
    const ri::logic::LogicNodePortSchema schema = ri::logic::GetLogicNodePortSchema(placed->kitId);
    if (schema.outputs.empty()) {
        return;
    }
    std::size_t index = 0;
    for (std::size_t i = 0; i < schema.outputs.size(); ++i) {
        if (schema.outputs[i].name == wirePickState_.outputName) {
            index = i;
            break;
        }
    }
    const int count = static_cast<int>(schema.outputs.size());
    index = static_cast<std::size_t>((static_cast<int>(index) + delta + count) % count);
    wirePickState_.outputName = schema.outputs[index].name;
}

bool EditorLogicLayer::PulseSelectedTrigger(Scene& scene, const int sceneHandle) {
    if (!runtimeGraph_ || sceneHandle == kInvalidHandle || sceneHandle >= static_cast<int>(scene.NodeCount())) {
        return false;
    }
    const ri::scene::Node& node = scene.GetNode(sceneHandle);
    if (node.name.rfind("Trigger_", 0) != 0) {
        return false;
    }
    ri::logic::LogicContext ctx = ri::world::MakePlayerTriggerContext("editor_test");
    ctx.sourceId = node.name;
    runtimeGraph_->EmitWorldOutput(node.name, ri::logic::ports::kTriggerOnStartTouch, std::move(ctx));
    ApplyCircuitProbeColors(scene);
    for (const EditorLogicWireRecord& wire : wires_) {
        if (wire.sourceLogicId != node.name) {
            continue;
        }
        for (const int handle : wire.sceneHandles) {
            if (handle == kInvalidHandle || handle >= static_cast<int>(scene.NodeCount())) {
                continue;
            }
            ri::scene::Node& bead = scene.GetNode(handle);
            if (bead.material == kInvalidHandle) {
                continue;
            }
            scene.GetMaterial(bead.material).emissiveColor = ri::math::Vec3{0.95f, 0.82f, 0.22f};
        }
    }
    return true;
}

void EditorLogicLayer::ApplyWireProbeColors(Scene& scene,
                                              const std::vector<ri::logic::LogicCircuitNodeProbe>& probes) {
    for (const EditorLogicWireRecord& wire : wires_) {
        bool powered = false;
        if (const EditorLogicPlacedNode* source = FindPlacedNode(wire.sourceLogicId)) {
            const auto probeIt = std::find_if(probes.begin(), probes.end(), [&](const ri::logic::LogicCircuitNodeProbe& probe) {
                return probe.id == source->logicNodeId;
            });
            powered = probeIt != probes.end() && probeIt->powered;
        }
        for (const int handle : wire.sceneHandles) {
            if (handle == kInvalidHandle || handle >= static_cast<int>(scene.NodeCount())) {
                continue;
            }
            ri::scene::Node& bead = scene.GetNode(handle);
            if (bead.material == kInvalidHandle) {
                continue;
            }
            ri::scene::Material& material = scene.GetMaterial(bead.material);
            material.emissiveColor = powered ? ri::math::Vec3{0.95f, 0.82f, 0.22f}
                                             : ri::math::Vec3{0.12f, 0.10f, 0.03f};
        }
    }
}

ri::logic::LogicAuthoringCompileResult EditorLogicLayer::Recompile(Scene& scene) {
    const ri::logic::LogicAuthoringGraph graph = BuildAuthoringGraph();
    const ri::logic::LogicAuthoringCompileOptions options = BuildCompileOptionsFromScene(scene);
    ri::logic::LogicAuthoringCompileResult result = ri::logic::CompileLogicAuthoringGraphWithReport(graph, options);
    if (ri::logic::LogicAuthoringCompileSucceeded(result) && !result.spec.nodes.empty()) {
        runtimeGraph_ = std::make_unique<ri::logic::LogicGraph>(result.spec);
        runtimeGraph_->SetOutputHandler([this](const ri::logic::LogicOutputEvent& event) {
            lastCompileSummary_ = "Logic pulse: " + event.sourceId + "." + event.outputName;
            (void)event;
        });
        std::unordered_map<std::string, std::string> kitIdByLogicNodeId{};
        kitIdByLogicNodeId.reserve(placedNodes_.size());
        for (const EditorLogicPlacedNode& placed : placedNodes_) {
            kitIdByLogicNodeId[placed.logicNodeId] = placed.kitId;
        }
        ri::logic::BindLogicSenseInputDispatchHandler(*runtimeGraph_, senseRuntimeState_, kitIdByLogicNodeId);
    } else if (result.spec.nodes.empty()) {
        runtimeGraph_.reset();
    }

    lastCompileSummary_ = "Logic compile: " + std::to_string(result.spec.nodes.size()) + " nodes, "
        + std::to_string(result.spec.routes.size()) + " routes, " + std::to_string(result.summary.errorCount)
        + " errors, " + std::to_string(result.summary.warningCount) + " warnings.";
    return result;
}

void EditorLogicLayer::TickSenseProbes(const ri::math::Vec3& probeWorldPosition) {
    if (!runtimeGraph_) {
        return;
    }
    std::vector<ri::logic::LogicAuthoringSenseProbeRecord> probes{};
    probes.reserve(placedNodes_.size());
    for (const EditorLogicPlacedNode& placed : placedNodes_) {
        if (placed.kitId.rfind("sense_", 0) != 0 || placed.kitId == "sense_tick") {
            continue;
        }
        ri::logic::LogicAuthoringSenseProbeRecord record{};
        record.logicNodeId = placed.logicNodeId;
        record.kitId = placed.kitId;
        record.position = {placed.position.x, placed.position.y, placed.position.z};
        probes.push_back(std::move(record));
    }
    if (probes.empty()) {
        return;
    }
    const std::array<float, 3> probePosition{probeWorldPosition.x, probeWorldPosition.y, probeWorldPosition.z};
    ri::logic::LogicAuthoringSenseRuntimeOptions senseOptions{};
    senseOptions.probeInstigatorTag = "player";
    if (editorTraceScene_.has_value()) {
        senseOptions.raycast = [this](const ri::logic::LogicSenseRaycastRequest& request)
            -> std::optional<ri::logic::LogicSenseRaycastHit> {
            const ri::math::Vec3 origin{request.origin[0], request.origin[1], request.origin[2]};
            const ri::math::Vec3 direction{request.direction[0], request.direction[1], request.direction[2]};
            if (const std::optional<ri::trace::TraceHit> hit =
                    editorTraceScene_->TraceRay(origin, direction, request.maxDistance)) {
                return ri::logic::LogicSenseRaycastHit{hit->time * request.maxDistance};
            }
            return std::nullopt;
        };
    }
    ri::logic::TickLogicAuthoringSenseNodes(
        *runtimeGraph_, probes, probePosition, senseRuntimeState_, &senseOptions);
}

void EditorLogicLayer::ApplyCircuitProbeColors(Scene& scene) {
    if (!runtimeGraph_) {
        return;
    }
    const std::vector<ri::logic::LogicCircuitNodeProbe> probes = runtimeGraph_->ProbeCircuitNodes();
    ApplyWireProbeColors(scene, probes);
    for (const EditorLogicPlacedNode& placed : placedNodes_) {
        const auto probeIt = std::find_if(probes.begin(), probes.end(), [&](const ri::logic::LogicCircuitNodeProbe& probe) {
            return probe.id == placed.logicNodeId;
        });
        const bool powered = probeIt != probes.end() && probeIt->powered;
        for (const int handle : placed.sceneHandles) {
            if (handle == kInvalidHandle || handle >= static_cast<int>(scene.NodeCount())) {
                continue;
            }
            ri::scene::Node& node = scene.GetNode(handle);
            if (node.material == kInvalidHandle) {
                continue;
            }
            ri::scene::Material& material = scene.GetMaterial(node.material);
            material.emissiveColor = powered ? ri::math::Vec3{0.35f, 0.95f, 0.45f} : ri::math::Vec3{0.05f, 0.06f, 0.08f};
        }
    }
}

void EditorLogicLayer::PulseTestInput(const std::string_view logicNodeId, const std::string_view inputName) {
    if (!runtimeGraph_) {
        return;
    }
    ri::logic::LogicContext ctx{};
    ctx.sourceId = "editor_test";
    ctx.parameter = 1.0;
    ctx.analogSignal = 1.0;
    const std::string normalized = NormalizeInputName(inputName);
    if (normalized == "tag") {
        ctx.fields["tag"] = "player";
    } else if (normalized == "key") {
        ctx.fields["key"] = "distance";
    } else if (normalized == "poll") {
        ctx.fields["poll"] = "1";
    }
    runtimeGraph_->DispatchInput(logicNodeId, inputName, ctx);
}

bool EditorLogicLayer::PulseLogicNodeAtSceneHandle(Scene& scene, const int sceneHandle, const std::size_t inputIndex) {
    if (!runtimeGraph_ || sceneHandle == kInvalidHandle || sceneHandle >= static_cast<int>(scene.NodeCount())) {
        return false;
    }
    std::optional<std::string> logicNodeId = FindLogicNodeIdForSceneHandle(sceneHandle);
    if (!logicNodeId.has_value()) {
        const ri::scene::Node& node = scene.GetNode(sceneHandle);
        const std::size_t colon = node.name.rfind(':');
        if (colon != std::string::npos) {
            const std::string prefix = node.name.substr(0, colon);
            if (FindPlacedNode(prefix) != nullptr) {
                logicNodeId = prefix;
            }
        }
    }
    if (!logicNodeId.has_value()) {
        return false;
    }
    const EditorLogicPlacedNode* placed = FindPlacedNode(*logicNodeId);
    if (placed == nullptr) {
        return false;
    }
    const ri::logic::LogicNodePortSchema schema = ri::logic::GetLogicNodePortSchema(placed->kitId);
    if (schema.inputs.empty() || inputIndex >= schema.inputs.size()) {
        return false;
    }
    PulseTestInput(placed->logicNodeId, schema.inputs[inputIndex].name);
    ApplyCircuitProbeColors(scene);
    return true;
}

bool EditorLogicLayer::PulseMostRecentNode(Scene& scene, const std::size_t inputIndex) {
    if (placedNodes_.empty() || !runtimeGraph_) {
        return false;
    }
    const EditorLogicPlacedNode& node = placedNodes_.back();
    const ri::logic::LogicNodePortSchema schema = ri::logic::GetLogicNodePortSchema(node.kitId);
    if (schema.inputs.empty() || inputIndex >= schema.inputs.size()) {
        return false;
    }
    PulseTestInput(node.logicNodeId, schema.inputs[inputIndex].name);
    ApplyCircuitProbeColors(scene);
    return true;
}

std::string EditorLogicLayer::Serialize(const Scene& scene) const {
    std::ostringstream stream;
    stream << "# RawIron logic authoring v1\n";
    stream << "creator_visible=" << (creatorLayerVisible_ ? "1" : "0") << "\n";
    stream << "player_hidden=" << (playerPreviewHidden_ ? "1" : "0") << "\n";
    for (const EditorLogicPlacedNode& node : placedNodes_) {
        stream << "node," << node.logicNodeId << "," << node.kitId << "," << node.position.x << "," << node.position.y
               << "," << node.position.z << "," << (node.executable ? "1" : "0") << "\n";
    }
    for (std::size_t index = 0; index < scene.NodeCount(); ++index) {
        const ri::scene::Node& node = scene.GetNode(static_cast<int>(index));
        if (node.name.rfind("Trigger_", 0) != 0) {
            continue;
        }
        const ri::math::Vec3 worldCenter = scene.ComputeWorldPosition(static_cast<int>(index));
        stream << "trigger," << node.name << "," << worldCenter.x << "," << worldCenter.y << "," << worldCenter.z
               << "\n";
    }
    for (const EditorLogicWireRecord& wire : wires_) {
        stream << "wire," << wire.wireId << "," << wire.sourceLogicId << "," << wire.outputName << ","
               << wire.targetLogicId << "," << wire.inputName << "\n";
    }
    return stream.str();
}

bool EditorLogicLayer::Save(const fs::path& path, const Scene& scene) const {
    if (path.empty() || path.filename().empty()) {
        return false;
    }

    const auto pathPrefixHasIndirection = [](const fs::path& candidate) -> bool {
        if (candidate.empty()) {
            return false;
        }
        std::error_code absoluteError;
        fs::path current = fs::absolute(candidate, absoluteError);
        if (absoluteError) {
            return true;
        }
        std::vector<fs::path> chain;
        for (;;) {
            chain.push_back(current);
            if (!current.has_relative_path()) {
                break;
            }
            const fs::path parent = current.parent_path();
            if (parent.empty() || parent == current) {
                break;
            }
            current = parent;
        }
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            std::error_code statusError;
            const fs::file_status status = fs::symlink_status(*it, statusError);
            if (statusError == std::errc::no_such_file_or_directory
                || status.type() == fs::file_type::not_found) {
                continue;
            }
            if (statusError || fs::is_symlink(status)) {
                return true;
            }
#if defined(_WIN32)
            const DWORD attributes = GetFileAttributesW(it->c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES) {
                const DWORD lastError = GetLastError();
                if (lastError == ERROR_FILE_NOT_FOUND || lastError == ERROR_PATH_NOT_FOUND) {
                    continue;
                }
                return true;
            }
            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
                return true;
            }
#endif
        }
        return false;
    };

    const fs::path parentPath = path.parent_path();
    if (!parentPath.empty()) {
        if (pathPrefixHasIndirection(parentPath)) {
            return false;
        }
        std::error_code ec{};
        fs::create_directories(parentPath, ec);
        if (ec) {
            return false;
        }
    }

    // Reject leaf symlink/reparse destinations (ofstream trunc would follow them).
#if defined(_WIN32)
    const DWORD leafAttributes = GetFileAttributesW(path.c_str());
    if (leafAttributes != INVALID_FILE_ATTRIBUTES
        && ((leafAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
            || (leafAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U)) {
        return false;
    }
#else
    {
        std::error_code statusError;
        const fs::file_status leafStatus = fs::symlink_status(path, statusError);
        if (!statusError
            && (fs::is_symlink(leafStatus) || fs::is_directory(leafStatus))) {
            return false;
        }
        if (statusError && statusError != std::errc::no_such_file_or_directory) {
            return false;
        }
    }
#endif

    const std::string serialized = Serialize(scene);
    constexpr std::size_t kCollisionRetries = 8U;
    static std::atomic<std::uint64_t> sequence{0U};
    for (std::size_t attempt = 0U; attempt < kCollisionRetries; ++attempt) {
        fs::path temporary = path;
        const std::uint64_t stamp = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        temporary += ".tmp." + std::to_string(stamp) + "."
            + std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed));
#if defined(_WIN32)
        const HANDLE handle = CreateFileW(temporary.c_str(),
                                          GENERIC_WRITE,
                                          0U,
                                          nullptr,
                                          CREATE_NEW,
                                          FILE_ATTRIBUTE_NORMAL,
                                          nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            const DWORD lastError = GetLastError();
            if (lastError == ERROR_FILE_EXISTS || lastError == ERROR_ALREADY_EXISTS) {
                continue;
            }
            return false;
        }
        DWORD written = 0U;
        const BOOL wrote = serialized.empty()
            || (WriteFile(handle,
                          serialized.data(),
                          static_cast<DWORD>(serialized.size()),
                          &written,
                          nullptr)
                && written == serialized.size());
        const BOOL flushed = wrote ? FlushFileBuffers(handle) : FALSE;
        CloseHandle(handle);
        if (!wrote || !flushed) {
            std::error_code cleanupError;
            fs::remove(temporary, cleanupError);
            return false;
        }
        if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
            == FALSE) {
            std::error_code cleanupError;
            fs::remove(temporary, cleanupError);
            return false;
        }
        return true;
#else
        int flags = O_WRONLY | O_CREAT | O_EXCL;
#if defined(O_CLOEXEC)
        flags |= O_CLOEXEC;
#endif
#if defined(O_NOFOLLOW)
        flags |= O_NOFOLLOW;
#endif
        const int descriptor = ::open(temporary.c_str(), flags, 0600);
        if (descriptor < 0) {
            if (errno == EEXIST) {
                continue;
            }
            return false;
        }
        bool writeOk = true;
        std::size_t offset = 0U;
        while (offset < serialized.size()) {
            const ssize_t chunk = ::write(descriptor, serialized.data() + offset, serialized.size() - offset);
            if (chunk < 0) {
                if (errno == EINTR) {
                    continue;
                }
                writeOk = false;
                break;
            }
            if (chunk == 0) {
                writeOk = false;
                break;
            }
            offset += static_cast<std::size_t>(chunk);
        }
        const bool synced = writeOk && ::fsync(descriptor) == 0;
        ::close(descriptor);
        if (!writeOk || !synced) {
            std::error_code cleanupError;
            fs::remove(temporary, cleanupError);
            return false;
        }
        std::error_code renameError;
        fs::rename(temporary, path, renameError);
        if (renameError) {
            std::error_code cleanupError;
            fs::remove(temporary, cleanupError);
            return false;
        }
        return true;
#endif
    }
    return false;
}

bool EditorLogicLayer::Load(const fs::path& path,
                            Scene& scene,
                            const int worldRoot,
                            std::string* errorMessage) {
    constexpr std::uintmax_t kMaxLogicAuthoringBytes = 64U * 1024U * 1024U;
    std::error_code sizeError;
    const std::uintmax_t fileSize = fs::file_size(path, sizeError);
    if (sizeError || fileSize > kMaxLogicAuthoringBytes) {
        if (errorMessage != nullptr) {
            *errorMessage = sizeError ? "logic authoring file could not be inspected"
                                      : "logic authoring file exceeds safety limit";
        }
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "logic authoring file missing";
        }
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad() || static_cast<std::uintmax_t>(buffer.tellp()) != fileSize) {
        if (errorMessage != nullptr) {
            *errorMessage = "logic authoring file read was incomplete";
        }
        return false;
    }
    std::istringstream stream(buffer.str());
    std::string magic;
    if (!std::getline(stream, magic)) {
        if (errorMessage != nullptr) {
            *errorMessage = "logic authoring file is empty";
        }
        return false;
    }
    if (!magic.empty() && magic.back() == '\r') {
        magic.pop_back();
    }
    if (magic != "# RawIron logic authoring v1") {
        if (errorMessage != nullptr) {
            *errorMessage = "logic authoring file has an unsupported header";
        }
        return false;
    }

    // Parse fully into staging state before mutating this layer or the scene.
    struct PendingNode {
        std::string logicNodeId;
        std::string kitId;
        ri::math::Vec3 position{};
        bool executable = true;
    };
    bool stagedCreatorVisible = true;
    bool stagedPlayerHidden = false;
    std::vector<EditorLogicWireRecord> stagedWires;
    std::vector<PendingNode> stagedNodes;

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (const std::size_t eq = line.find('='); eq != std::string::npos) {
            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            if (key == "creator_visible") {
                if (value != "0" && value != "1") {
                    if (errorMessage != nullptr) {
                        *errorMessage = "logic creator_visible flag is malformed";
                    }
                    return false;
                }
                stagedCreatorVisible = value == "1";
                continue;
            }
            if (key == "player_hidden") {
                if (value != "0" && value != "1") {
                    if (errorMessage != nullptr) {
                        *errorMessage = "logic player_hidden flag is malformed";
                    }
                    return false;
                }
                stagedPlayerHidden = value == "1";
                continue;
            }
            if (errorMessage != nullptr) {
                *errorMessage = "unknown logic authoring setting '" + key + "'";
            }
            return false;
        }
        const std::vector<std::string> tokens = SplitCsvLine(line);
        if (tokens.empty()) {
            continue;
        }
        const std::string& kind = tokens[0];
        if (kind == "wire") {
            if (tokens.size() != 6U || tokens[1].empty() || tokens[2].empty() || tokens[3].empty()
                || tokens[4].empty() || tokens[5].empty()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "malformed logic wire record";
                }
                return false;
            }
            EditorLogicWireRecord wire{};
            wire.wireId = tokens[1];
            wire.sourceLogicId = tokens[2];
            wire.outputName = tokens[3];
            wire.targetLogicId = tokens[4];
            wire.inputName = tokens[5];
            stagedWires.push_back(std::move(wire));
            continue;
        }
        if (kind == "node") {
            if (tokens.size() != 7U || tokens[1].empty() || tokens[2].empty()
                || (tokens[6] != "0" && tokens[6] != "1")) {
                if (errorMessage != nullptr) {
                    *errorMessage = "malformed logic node record";
                }
                return false;
            }
            ri::math::Vec3 position{};
            try {
                position = ri::math::Vec3{std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5])};
            } catch (...) {
                if (errorMessage != nullptr) {
                    *errorMessage = "logic node position is malformed";
                }
                return false;
            }
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                if (errorMessage != nullptr) {
                    *errorMessage = "logic node position is non-finite";
                }
                return false;
            }
            stagedNodes.push_back(PendingNode{
                .logicNodeId = tokens[1],
                .kitId = tokens[2],
                .position = position,
                .executable = tokens[6] == "1",
            });
            continue;
        }
        if (kind == "trigger") {
            if (tokens.size() != 5U || tokens[1].empty()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "malformed logic trigger record";
                }
                return false;
            }
            try {
                const float x = std::stof(tokens[2]);
                const float y = std::stof(tokens[3]);
                const float z = std::stof(tokens[4]);
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
                    throw std::out_of_range("non-finite trigger");
                }
            } catch (...) {
                if (errorMessage != nullptr) {
                    *errorMessage = "logic trigger position is malformed";
                }
                return false;
            }
            continue;
        }
        if (errorMessage != nullptr) {
            *errorMessage = "unknown logic authoring record '" + kind + "'";
        }
        return false;
    }

    // Apply only after the full file validated. Placement still mutates `scene`; callers that
    // need all-or-nothing scene mutation (bundle load) should pass a candidate scene.
    Reset();
    creatorLayerVisible_ = stagedCreatorVisible;
    playerPreviewHidden_ = stagedPlayerHidden;
    wires_ = std::move(stagedWires);
    for (const PendingNode& pending : stagedNodes) {
        const EditorLogicPlaceResult placed =
            PlaceKitNode(scene, worldRoot, pending.kitId, pending.position, false, pending.logicNodeId);
        if (!placed.placed) {
            Reset();
            if (errorMessage != nullptr) {
                *errorMessage = placed.message.empty() ? "logic node could not be instantiated" : placed.message;
            }
            return false;
        }
        if (!placedNodes_.empty()) {
            placedNodes_.back().executable = pending.executable;
        }
        lastPlacedLogicNodeId_ = pending.logicNodeId;
    }
    RebuildAllWireVisuals(scene);
    (void)Recompile(scene);
    SetCreatorLayerVisible(scene, creatorLayerVisible_);
    SetPlayerPreviewHidden(scene, playerPreviewHidden_);
    return true;
}

} // namespace ri::editor
