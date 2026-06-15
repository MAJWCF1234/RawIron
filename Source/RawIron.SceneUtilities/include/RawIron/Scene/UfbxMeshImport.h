#pragma once

#include "RawIron/Scene/Scene.h"

#include <filesystem>
#include <string>

namespace ri::scene {

struct UfbxSceneImportOptions {
    int parent = kInvalidHandle;
    /// If non-empty, inserted as a grouping node above imported roots.
    std::string wrapperNodeName{};
    enum class FileFormat {
        Auto,
        Fbx,
        Obj,
    } fileFormat = FileFormat::Auto;
};

/// Imports geometry from FBX/OBJ through `ufbx`: mesh material parts become child nodes with materials.
/// Returns the wrapper node when `wrapperNodeName` is non-empty, otherwise the first imported root.
int ImportUfbxSceneFile(Scene& targetScene,
                        const std::filesystem::path& path,
                        const UfbxSceneImportOptions& options,
                        std::string& error);

} // namespace ri::scene
