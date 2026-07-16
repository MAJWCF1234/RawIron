#pragma once

#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/Helpers.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Scene/WorkspaceSandbox.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ri::editor {

struct EditorSceneControllerContext {
    const ri::scene::StarterSceneHandles* handles = nullptr;
    int editorTrashFolderHandle = ri::scene::kInvalidHandle;
};

[[nodiscard]] bool IsProtectedEditorNode(const EditorSceneControllerContext& context, int handle);
[[nodiscard]] bool IsEditableAuthoredNode(const ri::scene::Scene& scene,
                                          const EditorSceneControllerContext& context,
                                          int handle);
[[nodiscard]] std::string TrimNodeName(std::string_view name);
[[nodiscard]] bool IsNodeNameAvailable(const ri::scene::Scene& scene,
                                       std::string_view name,
                                       int ignoredHandle = ri::scene::kInvalidHandle);
[[nodiscard]] std::string MakeUniqueNodeName(const ri::scene::Scene& scene, const std::string& baseName);
[[nodiscard]] bool TryAssignLocalTransformFromWorld(ri::scene::Scene& scene,
                                                    int nodeHandle,
                                                    const ri::math::Mat4& worldMatrix);
[[nodiscard]] bool TryResetSelectedTransform(ri::scene::Scene& scene,
                                             const EditorSceneControllerContext& context,
                                             std::size_t selectedNode,
                                             std::string& message);
[[nodiscard]] bool TryReparentSelectedToWorldRoot(ri::scene::Scene& scene,
                                                  const EditorSceneControllerContext& context,
                                                  std::size_t selectedNode,
                                                  std::string& message);
[[nodiscard]] bool TrySelectAdjacentAuthoredNode(const ri::scene::Scene& scene,
                                                 const EditorSceneControllerContext& context,
                                                 const std::vector<int>& order,
                                                 int direction,
                                                 std::size_t& selectedNode,
                                                 std::string& message);
[[nodiscard]] bool TryCreateGroupNode(ri::scene::Scene& scene,
                                      const EditorSceneControllerContext& context,
                                      std::size_t& selectedNode,
                                      std::string& message);
[[nodiscard]] bool TryGroupSelectedNode(ri::scene::Scene& scene,
                                        const EditorSceneControllerContext& context,
                                        std::size_t& selectedNode,
                                        std::string& message);
[[nodiscard]] bool TryUngroupSelectedNode(ri::scene::Scene& scene,
                                          const EditorSceneControllerContext& context,
                                          std::size_t& selectedNode,
                                          std::string& message);
[[nodiscard]] bool TryDeleteSelectedNode(ri::scene::Scene& scene,
                                         const EditorSceneControllerContext& context,
                                         std::size_t& selectedNode,
                                         std::string& message);
[[nodiscard]] bool TryDuplicateSelectedNode(ri::scene::Scene& scene,
                                            const EditorSceneControllerContext& context,
                                            std::size_t& selectedNode,
                                            std::string& message);
[[nodiscard]] bool TrySnapSelectedNodeToGrid(ri::scene::Scene& scene,
                                             const EditorSceneControllerContext& context,
                                             std::size_t selectedNode,
                                             float gridStep,
                                             std::string& message);

} // namespace ri::editor
