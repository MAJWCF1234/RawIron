#include "EditorSceneController.h"

#include <cmath>

namespace ri::editor {

namespace {

[[nodiscard]] ri::math::Vec3 EulerDegreesFromRotation3x3(const ri::math::Mat4& rotationOnly) {
    const float sy = std::sqrt(rotationOnly.m[0][0] * rotationOnly.m[0][0] +
                               rotationOnly.m[1][0] * rotationOnly.m[1][0]);
    constexpr float kRadToDeg = 180.0f / ri::math::kPi;
    if (sy > 1.0e-6f) {
        const float x = std::atan2(rotationOnly.m[2][1], rotationOnly.m[2][2]);
        const float y = std::atan2(-rotationOnly.m[2][0], sy);
        const float z = std::atan2(rotationOnly.m[1][0], rotationOnly.m[0][0]);
        return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, z * kRadToDeg};
    }
    const float x = std::atan2(-rotationOnly.m[1][2], rotationOnly.m[1][1]);
    const float y = std::atan2(-rotationOnly.m[2][0], sy);
    return ri::math::Vec3{x * kRadToDeg, y * kRadToDeg, 0.0f};
}

[[nodiscard]] ri::scene::Transform TransformFromMatrix(const ri::math::Mat4& matrix) {
    ri::scene::Transform transform{};
    transform.position = ri::math::ExtractTranslation(matrix);
    transform.scale = ri::math::ExtractScale(matrix);

    ri::math::Mat4 rotationOnly = matrix;
    for (int column = 0; column < 3; ++column) {
        const float scaleValue = column == 0 ? transform.scale.x : (column == 1 ? transform.scale.y : transform.scale.z);
        const float inverseScale = std::fabs(scaleValue) > 1.0e-8f ? 1.0f / scaleValue : 0.0f;
        rotationOnly.m[0][column] *= inverseScale;
        rotationOnly.m[1][column] *= inverseScale;
        rotationOnly.m[2][column] *= inverseScale;
    }
    transform.rotationDegrees = EulerDegreesFromRotation3x3(rotationOnly);
    return transform;
}

[[nodiscard]] float SnapToGrid(const float value, const float step) {
    const float safeStep = std::max(0.0001f, step);
    return std::round(value / safeStep) * safeStep;
}

} // namespace

bool IsProtectedEditorNode(const EditorSceneControllerContext& context, const int handle) {
    if (handle < 0 || context.handles == nullptr) {
        return true;
    }
    const ri::scene::StarterSceneHandles& handles = *context.handles;
    if (handle == handles.root || handle == handles.sun || handle == handles.grid || handle == handles.floor) {
        return true;
    }
    if (handle == handles.orbitCamera.root || handle == handles.orbitCamera.swivel || handle == handles.orbitCamera.cameraNode) {
        return true;
    }
    if (handle == handles.axes.root || handle == handles.axes.xAxis || handle == handles.axes.yAxis || handle == handles.axes.zAxis) {
        return true;
    }
    return handle == context.editorTrashFolderHandle;
}

bool IsEditableAuthoredNode(const ri::scene::Scene& scene,
                            const EditorSceneControllerContext& context,
                            const int handle) {
    return handle >= 0 && static_cast<std::size_t>(handle) < scene.NodeCount() && !IsProtectedEditorNode(context, handle);
}

std::string MakeUniqueNodeName(const ri::scene::Scene& scene, const std::string& baseName) {
    const auto nameExists = [&scene](const std::string& candidate) {
        for (const ri::scene::Node& node : scene.Nodes()) {
            if (node.name == candidate) {
                return true;
            }
        }
        return false;
    };

    std::string candidate = baseName;
    if (!nameExists(candidate)) {
        return candidate;
    }
    for (int suffix = 1; suffix < 10000; ++suffix) {
        candidate = baseName + "_" + std::to_string(suffix);
        if (!nameExists(candidate)) {
            return candidate;
        }
    }
    return baseName + "_node";
}

bool TryAssignLocalTransformFromWorld(ri::scene::Scene& scene, const int nodeHandle, const ri::math::Mat4& worldMatrix) {
    if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
        return false;
    }
    const int parent = scene.GetNode(nodeHandle).parent;
    if (parent == ri::scene::kInvalidHandle) {
        scene.GetNode(nodeHandle).localTransform = TransformFromMatrix(worldMatrix);
        return true;
    }
    ri::math::Mat4 parentWorldInverse{};
    if (!ri::math::TryInvertAffineMat4(scene.ComputeWorldMatrix(parent), parentWorldInverse)) {
        return false;
    }
    scene.GetNode(nodeHandle).localTransform = TransformFromMatrix(ri::math::Multiply(parentWorldInverse, worldMatrix));
    return true;
}

void DetachMeshesInSubtree(ri::scene::Scene& scene, const int nodeHandle) {
    if (nodeHandle < 0 || static_cast<std::size_t>(nodeHandle) >= scene.NodeCount()) {
        return;
    }
    ri::scene::Node& node = scene.GetNode(nodeHandle);
    if (node.mesh != ri::scene::kInvalidHandle) {
        scene.AttachMesh(nodeHandle, ri::scene::kInvalidHandle, ri::scene::kInvalidHandle);
    }
    for (const int child : node.children) {
        DetachMeshesInSubtree(scene, child);
    }
}

bool TryResetSelectedTransform(ri::scene::Scene& scene,
                               const EditorSceneControllerContext& context,
                               const std::size_t selectedNode,
                               std::string& message) {
    if (!IsEditableAuthoredNode(scene, context, static_cast<int>(selectedNode))) {
        message = "Reset blocked: select an authored node, not World/rigs/helpers.";
        return false;
    }
    scene.GetNode(static_cast<int>(selectedNode)).localTransform = ri::scene::Transform{};
    message = "Reset selected node transform.";
    return true;
}

bool TryReparentSelectedToWorldRoot(ri::scene::Scene& scene,
                                    const EditorSceneControllerContext& context,
                                    const std::size_t selectedNode,
                                    std::string& message) {
    if (!IsEditableAuthoredNode(scene, context, static_cast<int>(selectedNode))) {
        message = "Reparent blocked: select an authored node.";
        return false;
    }
    if (context.handles == nullptr || context.handles->root == ri::scene::kInvalidHandle) {
        message = "Reparent failed: scene has no world root.";
        return false;
    }
    const int nodeHandle = static_cast<int>(selectedNode);
    if (scene.GetNode(nodeHandle).parent == context.handles->root) {
        message = "Reparent: node is already under World.";
        return false;
    }
    const ri::math::Mat4 world = scene.ComputeWorldMatrix(nodeHandle);
    if (!scene.SetParent(nodeHandle, context.handles->root)) {
        message = "Reparent failed: could not attach to World.";
        return false;
    }
    if (!TryAssignLocalTransformFromWorld(scene, nodeHandle, world)) {
        message = "Reparent warning: parent changed, but world transform was not preserved.";
        return false;
    }
    message = "Reparented selected node under World (world transform preserved).";
    return true;
}

bool TrySelectAdjacentAuthoredNode(const ri::scene::Scene& scene,
                                   const EditorSceneControllerContext& context,
                                   const std::vector<int>& order,
                                   const int direction,
                                   std::size_t& selectedNode,
                                   std::string& message) {
    if (order.empty()) {
        return false;
    }
    int startIndex = -1;
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == static_cast<int>(selectedNode)) {
            startIndex = static_cast<int>(i);
            break;
        }
    }
    if (startIndex < 0) {
        startIndex = 0;
    }
    const int n = static_cast<int>(order.size());
    for (int step = 1; step <= n; ++step) {
        int candidate = startIndex + (direction > 0 ? step : -step);
        candidate %= n;
        if (candidate < 0) {
            candidate += n;
        }
        const int handle = order[static_cast<std::size_t>(candidate)];
        if (!IsProtectedEditorNode(context, handle)) {
            selectedNode = static_cast<std::size_t>(handle);
            message = "Selection: " + scene.GetNode(handle).name;
            return true;
        }
    }
    message = "No authored node available in hierarchy.";
    return false;
}

bool TryCreateGroupNode(ri::scene::Scene& scene,
                        const EditorSceneControllerContext& context,
                        std::size_t& selectedNode,
                        std::string& message) {
    int parent = context.handles == nullptr ? ri::scene::kInvalidHandle : context.handles->root;
    const int selected = static_cast<int>(selectedNode);
    if (selected >= 0 && static_cast<std::size_t>(selected) < scene.NodeCount()) {
        parent = selected;
    }
    const std::string name = MakeUniqueNodeName(scene, "Group");
    selectedNode = static_cast<std::size_t>(scene.CreateNode(name, parent));
    message = "Created group node '" + name + "'.";
    return true;
}

bool TryGroupSelectedNode(ri::scene::Scene& scene,
                          const EditorSceneControllerContext& context,
                          std::size_t& selectedNode,
                          std::string& message) {
    const int selected = static_cast<int>(selectedNode);
    if (selected < 0 || static_cast<std::size_t>(selected) >= scene.NodeCount() || IsProtectedEditorNode(context, selected)) {
        message = "Group: select an authored node (not editor rigs/helpers).";
        return false;
    }

    const ri::scene::Node nodeSnapshot = scene.GetNode(selected);
    if (nodeSnapshot.parent == ri::scene::kInvalidHandle) {
        message = "Group: selected node has no parent.";
        return false;
    }

    const std::string groupName = MakeUniqueNodeName(scene, nodeSnapshot.name + "_group");
    const int group = scene.CreateNode(groupName, nodeSnapshot.parent);
    scene.GetNode(group).localTransform = nodeSnapshot.localTransform;
    if (!scene.SetParent(selected, group)) {
        message = "Group failed: could not re-parent selected node.";
        return false;
    }
    scene.GetNode(selected).localTransform = ri::scene::Transform{};
    selectedNode = static_cast<std::size_t>(group);
    message = "Grouped node under '" + groupName + "' (pivot group).";
    return true;
}

bool TryUngroupSelectedNode(ri::scene::Scene& scene,
                            const EditorSceneControllerContext& context,
                            std::size_t& selectedNode,
                            std::string& message) {
    const int selected = static_cast<int>(selectedNode);
    if (selected < 0 || static_cast<std::size_t>(selected) >= scene.NodeCount() || IsProtectedEditorNode(context, selected)) {
        message = "Ungroup: select a regular authored group node.";
        return false;
    }

    ri::scene::Node& group = scene.GetNode(selected);
    if (group.parent == ri::scene::kInvalidHandle) {
        message = "Ungroup failed: group has no parent.";
        return false;
    }
    if (group.mesh != ri::scene::kInvalidHandle || group.camera != ri::scene::kInvalidHandle || group.light != ri::scene::kInvalidHandle) {
        message = "Ungroup only supports transform-only group nodes.";
        return false;
    }
    if (group.children.empty()) {
        message = "Ungroup: selected group has no children.";
        return false;
    }

    const int parent = group.parent;
    const std::vector<int> children = group.children;
    for (const int child : children) {
        const ri::math::Mat4 childWorld = scene.ComputeWorldMatrix(child);
        if (!scene.SetParent(child, parent)) {
            message = "Ungroup failed: could not move all children.";
            return false;
        }
        if (!TryAssignLocalTransformFromWorld(scene, child, childWorld)) {
            message = "Ungroup failed: could not preserve child transform.";
            return false;
        }
    }

    if (!scene.SetParent(selected, context.editorTrashFolderHandle)) {
        message = "Ungroup warning: children moved, but could not hide old group node.";
        return false;
    }

    selectedNode = static_cast<std::size_t>(children.front());
    message = "Ungrouped children and moved empty group to EditorTrash.";
    return true;
}

bool TryDeleteSelectedNode(ri::scene::Scene& scene,
                           const EditorSceneControllerContext& context,
                           std::size_t& selectedNode,
                           std::string& message) {
    const int sel = static_cast<int>(selectedNode);
    if (IsProtectedEditorNode(context, sel)) {
        message = "Cannot delete World, rigs, orbit camera, helpers, or trash.";
        return false;
    }
    DetachMeshesInSubtree(scene, sel);
    if (!scene.SetParent(sel, context.editorTrashFolderHandle)) {
        message = "Delete failed — could not re-parent node.";
        return false;
    }
    selectedNode = static_cast<std::size_t>(context.handles == nullptr ? 0 : context.handles->root);
    message = "Removed geometry from the working scene (hidden EditorTrash folder).";
    return true;
}

bool TryDuplicateSelectedNode(ri::scene::Scene& scene,
                              const EditorSceneControllerContext& context,
                              std::size_t& selectedNode,
                              std::string& message) {
    const int sel = static_cast<int>(selectedNode);
    if (sel < 0 || IsProtectedEditorNode(context, sel)) {
        message = "Duplicate: pick an authored mesh node (not rigs or helpers).";
        return false;
    }
    const ri::scene::Node src = scene.GetNode(sel);
    if (src.mesh == ri::scene::kInvalidHandle) {
        message = "Duplicate requires a mesh on the selected node.";
        return false;
    }
    const std::string dupName = src.name + "_copy";
    const int dup = scene.CreateNode(dupName, src.parent);
    scene.GetNode(dup).localTransform = src.localTransform;
    scene.GetNode(dup).localTransform.position.x += 1.0f;
    scene.AttachMesh(dup, src.mesh, src.material);
    selectedNode = static_cast<std::size_t>(dup);
    message = "Duplicated mesh node as " + dupName + " (offset +1.0 X, shared mesh/material handles).";
    return true;
}

bool TrySnapSelectedNodeToGrid(ri::scene::Scene& scene,
                               const EditorSceneControllerContext& context,
                               const std::size_t selectedNode,
                               const float gridStep,
                               const std::string& gridLabel,
                               std::string& message) {
    if (!IsEditableAuthoredNode(scene, context, static_cast<int>(selectedNode))) {
        message = "Cannot snap: select an editable authored node.";
        return false;
    }
    if (selectedNode >= scene.NodeCount()) {
        return false;
    }

    ri::scene::Node& node = scene.GetNode(static_cast<int>(selectedNode));
    const ri::scene::Transform before = node.localTransform;
    node.localTransform.position.x = SnapToGrid(node.localTransform.position.x, gridStep);
    node.localTransform.position.y = SnapToGrid(node.localTransform.position.y, gridStep);
    node.localTransform.position.z = SnapToGrid(node.localTransform.position.z, gridStep);
    const ri::scene::Transform after = node.localTransform;
    if (before.position.x == after.position.x &&
        before.position.y == after.position.y &&
        before.position.z == after.position.z) {
        message = "Selection already aligned to grid (" + gridLabel + ").";
        return false;
    }

    message = "Snapped selection to grid (" + gridLabel + ").";
    return true;
}

} // namespace ri::editor
