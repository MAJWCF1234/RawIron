#include "RawIron/Scene/FbxLoader.h"

#include "RawIron/Scene/UfbxMeshImport.h"

namespace ri::scene {

int ImportFbxToScene(Scene& targetScene,
                     const std::filesystem::path& path,
                     const FbxImportOptions& options,
                     std::string& error) {
    const UfbxSceneImportOptions importOptions{
        .parent = options.parent,
        .wrapperNodeName = options.wrapperNodeName,
        .fileFormat = UfbxSceneImportOptions::FileFormat::Fbx,
    };
#if defined(_WIN32)
    return ImportUfbxSceneFileSeh(targetScene, path, importOptions, error);
#else
    return ImportUfbxSceneFile(targetScene, path, importOptions, error);
#endif
}

} // namespace ri::scene
