#pragma once

#include "RawIron/Scene/Scene.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::scene {

struct GltfExportOptions {
    bool includeCameras = true;
    bool includeLights = true;
    bool includeInstanceBatches = true;
    bool copyExternalTextures = true;
};

struct GltfExportReport {
    std::size_t nodeCount = 0;
    std::size_t meshCount = 0;
    std::size_t materialCount = 0;
    std::size_t cameraCount = 0;
    std::size_t lightCount = 0;
    std::size_t instanceCount = 0;
    std::size_t textureCount = 0;
    std::filesystem::path jsonPath{};
    std::filesystem::path binaryPath{};
    std::vector<std::string> warnings{};
};

/// Writes glTF 2.0 JSON plus an adjacent binary buffer. Triangle meshes, normals, UV0, indices,
/// hierarchy, PBR material factors, texture references, cameras, punctual lights, and expanded
/// mesh-instance batches are supported. The output can be loaded back through `ImportGltfToScene`.
[[nodiscard]] bool ExportSceneToGltf(const Scene& scene,
                                     const std::filesystem::path& outputPath,
                                     const GltfExportOptions& options,
                                     GltfExportReport& report,
                                     std::string& error);

} // namespace ri::scene
