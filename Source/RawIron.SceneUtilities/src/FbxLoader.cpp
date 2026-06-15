#include "RawIron/Scene/FbxLoader.h"

#include "RawIron/Scene/UfbxMeshImport.h"

namespace ri::scene {

int ImportFbxToScene(Scene& targetScene,
                     const std::filesystem::path& path,
                     const FbxImportOptions& options,
                     std::string& error) {
    return ImportUfbxSceneFile(
        targetScene,
        path,
        UfbxSceneImportOptions{
            .parent = options.parent,
            .wrapperNodeName = options.wrapperNodeName,
            .fileFormat = UfbxSceneImportOptions::FileFormat::Fbx,
        },
        error);
}

} // namespace ri::scene
