#pragma once

#include "RawIron/Scene/Scene.h"

#include <filesystem>
#include <string>

namespace ri {
namespace scene {

struct GltfImportOptions {
    int parent = kInvalidHandle;
    /// If non-empty, inserted as a grouping node above each glTF scene root.
    std::string wrapperNodeName{};
    /// Zero-based scene index, or `-1` to use the file default scene (or the first scene if unset).
    int sceneIndex = -1;
    /// When set, attach glTF perspective cameras (`node.camera`) as `Camera` components.
    bool importCameras = false;
    /// When set, attach KHR punctual lights (`node.light`) as `Light` components.
    bool importLights = false;
};

/// Imports a glTF scene: mesh primitives become `Mesh` + `Material`, optional perspective cameras and
/// KHR punctual lights become `Camera` / `Light` when the corresponding flags are set. Returns the wrapper
/// node when `wrapperNodeName` is non-empty, otherwise the first imported root node. On failure returns
/// `kInvalidHandle` and sets `error`.
int ImportGltfToScene(Scene& targetScene,
                      const std::filesystem::path& path,
                      const GltfImportOptions& options,
                      std::string& error);

} // namespace scene
} // namespace ri
