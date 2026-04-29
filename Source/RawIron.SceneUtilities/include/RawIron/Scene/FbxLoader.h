#pragma once

#include "RawIron/Scene/Scene.h"

#include <filesystem>
#include <string>

namespace ri::scene {

struct FbxImportOptions {
    int parent = kInvalidHandle;
    /// If non-empty, inserted as a grouping node above each imported FBX root.
    std::string wrapperNodeName{};
};

/// Imports an FBX scene through `ufbx`: mesh parts become `Mesh` + `Material`.
/// Returns the wrapper node when `wrapperNodeName` is non-empty, otherwise the
/// first imported root node. On failure returns `kInvalidHandle` and sets `error`.
int ImportFbxToScene(Scene& targetScene,
                     const std::filesystem::path& path,
                     const FbxImportOptions& options,
                     std::string& error);

} // namespace ri::scene
