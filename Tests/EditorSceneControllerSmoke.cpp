#include "Apps/RawIron.Editor/src/EditorSceneController.h"

#include <cstdlib>
#include <string>
#include <unordered_set>

int main() {
    ri::scene::Scene scene{"EditorSceneControllerSmoke"};
    ri::scene::StarterSceneHandles handles{};
    handles.root = scene.CreateNode("World");

    const int mesh = scene.AddMesh({});
    const int material = scene.AddMaterial({});
    const int source = scene.CreateNode("Crate", handles.root);
    scene.AttachMesh(source, mesh, material);

    const ri::editor::EditorSceneControllerContext context{
        .handles = &handles,
        .editorTrashFolderHandle = ri::scene::kInvalidHandle,
    };
    std::string message;
    std::size_t selected = static_cast<std::size_t>(source);
    if (!ri::editor::TryDuplicateSelectedNode(scene, context, selected, message)
        || scene.GetNode(static_cast<int>(selected)).name != "Crate_copy") {
        return EXIT_FAILURE;
    }

    selected = static_cast<std::size_t>(source);
    if (!ri::editor::TryDuplicateSelectedNode(scene, context, selected, message)
        || scene.GetNode(static_cast<int>(selected)).name != "Crate_copy_1") {
        return EXIT_FAILURE;
    }

    std::unordered_set<std::string> names;
    for (const ri::scene::Node& node : scene.Nodes()) {
        if (!names.insert(node.name).second) {
            return EXIT_FAILURE;
        }
    }

    if (ri::editor::TrimNodeName(" \t Renamed Crate \r\n") != "Renamed Crate"
        || ri::editor::IsNodeNameAvailable(scene, "Crate")
        || !ri::editor::IsNodeNameAvailable(scene, "Crate", source)
        || ri::editor::MakeUniqueNodeName(scene, "  Crate  ") != "Crate_1"
        || ri::editor::MakeUniqueNodeName(scene, "   ") != "Node") {
        return EXIT_FAILURE;
    }

    selected = scene.NodeCount() + 100U;
    if (ri::editor::TryDuplicateSelectedNode(scene, context, selected, message)) {
        return EXIT_FAILURE;
    }

    const std::vector<int> corruptOrder{999999, source};
    selected = static_cast<std::size_t>(source);
    if (!ri::editor::TrySelectAdjacentAuthoredNode(scene, context, corruptOrder, 1, selected, message)
        || selected != static_cast<std::size_t>(source)) {
        return EXIT_FAILURE;
    }

    const int doomed = scene.CreateNode("Doomed", handles.root);
    scene.AttachMesh(doomed, mesh, material);
    selected = static_cast<std::size_t>(doomed);
    if (ri::editor::TryDeleteSelectedNode(scene, context, selected, message)
        || scene.GetNode(doomed).mesh != mesh
        || scene.GetNode(doomed).parent != handles.root) {
        return EXIT_FAILURE;
    }

    const int otherParent = scene.CreateNode("OtherParent", handles.root);
    const int reparentCandidate = scene.CreateNode("ReparentCandidate", otherParent);
    scene.GetNode(handles.root).localTransform.scale = {0.0f, 1.0f, 1.0f};
    selected = static_cast<std::size_t>(reparentCandidate);
    if (ri::editor::TryReparentSelectedToWorldRoot(scene, context, selected, message)
        || scene.GetNode(reparentCandidate).parent != otherParent) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
