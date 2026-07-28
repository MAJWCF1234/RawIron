#pragma once

#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Structural/ConvexClipper.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ri::scene {

struct PrimitiveModelBakeResult {
    bool valid = false;
    ri::structural::CompiledMesh mesh{};
    /// Rigid bone binding for every baked vertex. Empty entries are unbound.
    /// Primitive Forge parts are rigidly weighted to one bone, inherited from their group when unset.
    std::vector<std::string> vertexBoneNames{};
    std::size_t inputPartCount = 0;
    std::size_t bakedPartCount = 0;
    std::size_t inputTriangleCount = 0;
    std::size_t outputTriangleCount = 0;
    std::size_t culledInternalTriangleCount = 0;
    std::size_t culledDuplicateTriangleCount = 0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// Bakes every enabled primitive through Raw Iron's native primitive library.
/// Internal-face culling is conservative: exact coplanar triangle pairs are removed after transform/weld quantization.
[[nodiscard]] PrimitiveModelBakeResult BakePrimitiveModel(
    const ri::content::PrimitiveModelDocument& document,
    const std::filesystem::path& documentDirectory = {});

/// Deterministic interchange output for the existing OBJ importer and external DCC inspection.
[[nodiscard]] bool SavePrimitiveModelBakeObj(const std::filesystem::path& path,
                                             const PrimitiveModelBakeResult& bake);
/// Saves compact rigid vertex-to-bone ranges beside an interchange mesh.
[[nodiscard]] bool SavePrimitiveModelBakeRigMap(const std::filesystem::path& path,
                                                const PrimitiveModelBakeResult& bake);

struct PrimitiveModelInstantiationResult {
    bool valid = false;
    int rootNode = kInvalidHandle;
    std::vector<int> groupNodes;
    std::vector<int> partNodes;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// Instantiates the editable group/part hierarchy without flattening it.
[[nodiscard]] PrimitiveModelInstantiationResult InstantiatePrimitiveModel(
    Scene& scene,
    int parentNode,
    const ri::content::PrimitiveModelDocument& document,
    const std::filesystem::path& documentDirectory = {});

} // namespace ri::scene
