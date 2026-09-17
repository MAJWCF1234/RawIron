#pragma once

#include "RawIron/Content/PrimitiveModelDocument.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/Scene.h"
#include "RawIron/Structural/ConvexClipper.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
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
    std::vector<std::string> groupIds;
    std::vector<int> partNodes;
    std::vector<std::string> partIds;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// Instantiates the editable group/part hierarchy without flattening it.
[[nodiscard]] PrimitiveModelInstantiationResult InstantiatePrimitiveModel(
    Scene& scene,
    int parentNode,
    const ri::content::PrimitiveModelDocument& document,
    const std::filesystem::path& documentDirectory = {});

/// Direct part bone, otherwise the inherited group binding.
[[nodiscard]] std::string EffectivePrimitivePartBone(
    const ri::content::PrimitiveModelDocument& document,
    std::string_view partId);

struct BoundPrimitivePartRest {
    int node = kInvalidHandle;
    std::string boneName{};
    ri::math::Mat4 restNodeWorld = ri::math::IdentityMatrix();
    std::vector<ri::math::Vec3> restPositions{};
    std::vector<ri::math::Vec3> restNormals{};
};

/// Captures rest meshes/worlds for parts that have an effective bone binding.
[[nodiscard]] std::vector<BoundPrimitivePartRest> CaptureBoundPrimitivePartRest(
    const Scene& scene,
    const std::vector<int>& partNodes,
    const std::vector<std::string>& partIds,
    const ri::content::PrimitiveModelDocument& document);

/// Rigid-skins captured part meshes so they follow posed bones. Identity pose restores rest.
[[nodiscard]] std::size_t PoseBoundPrimitiveParts(
    Scene& scene,
    const std::vector<BoundPrimitivePartRest>& parts,
    const std::unordered_map<std::string, ri::math::Mat4>& restBoneWorld,
    const std::unordered_map<std::string, ri::math::Mat4>& posedBoneWorld);

void RestoreBoundPrimitiveParts(Scene& scene, const std::vector<BoundPrimitivePartRest>& parts);

} // namespace ri::scene
