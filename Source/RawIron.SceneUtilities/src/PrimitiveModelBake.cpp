#include "RawIron/Scene/PrimitiveModelBake.h"

#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Math/Mat4.h"
#include "RawIron/Scene/RigAuthoring.h"
#include "RawIron/Scene/StructuralBrush.h"
#include "RawIron/Scene/StructuralPrimitivePresets.h"
#include "RawIron/Structural/StructuralPrimitives.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <fstream>
#include <functional>
#include <limits>
#include <set>
#include <unordered_map>

namespace ri::scene {
namespace {

struct QuantizedPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    auto operator<=>(const QuantizedPoint&) const = default;
};

struct TriangleKey {
    std::array<QuantizedPoint, 3> points{};

    bool operator==(const TriangleKey&) const = default;
};

struct TriangleKeyHash {
    std::size_t operator()(const TriangleKey& key) const noexcept {
        std::size_t hash = 1469598103934665603ULL;
        for (const QuantizedPoint& point : key.points) {
            for (const std::int64_t value : {point.x, point.y, point.z}) {
                hash ^= std::hash<std::int64_t>{}(value);
                hash *= 1099511628211ULL;
            }
        }
        return hash;
    }
};

struct TriangleRecord {
    std::array<ri::math::Vec3, 3> positions{};
    ri::math::Vec3 normal{};
    std::size_t partIndex = 0;
    std::string boneName{};
    bool removed = false;
};

struct PlaneKey {
    std::int64_t nx = 0;
    std::int64_t ny = 0;
    std::int64_t nz = 0;
    std::int64_t distance = 0;

    bool operator==(const PlaneKey&) const = default;
};

struct PlaneKeyHash {
    std::size_t operator()(const PlaneKey& key) const noexcept {
        std::size_t hash = 1469598103934665603ULL;
        for (const std::int64_t value : {key.nx, key.ny, key.nz, key.distance}) {
            hash ^= std::hash<std::int64_t>{}(value);
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

struct FacePatch {
    std::size_t partIndex = 0;
    int orientation = 1;
    std::vector<std::size_t> triangles{};
    std::set<QuantizedPoint> points{};
};

[[nodiscard]] ri::math::Vec3 ToVec3(const ri::content::DeclarativeVec3& value) {
    return {value.x, value.y, value.z};
}

[[nodiscard]] ri::math::Mat4 TransformMatrix(const ri::content::PrimitiveModelTransform& transform) {
    return ri::math::TRS(
        ToVec3(transform.translation),
        ToVec3(transform.rotationDegrees),
        ToVec3(transform.scale));
}

[[nodiscard]] Transform SceneTransform(const ri::content::PrimitiveModelTransform& transform) {
    return Transform{
        .position = ToVec3(transform.translation),
        .rotationDegrees = ToVec3(transform.rotationDegrees),
        .scale = ToVec3(transform.scale),
    };
}

[[nodiscard]] QuantizedPoint Quantize(const ri::math::Vec3& point, const double epsilon) {
    const auto component = [epsilon](const float value) {
        const double quantized = std::round(static_cast<double>(value) / epsilon);
        const double bounded = std::clamp(
            quantized,
            static_cast<double>((std::numeric_limits<std::int64_t>::min)()),
            static_cast<double>((std::numeric_limits<std::int64_t>::max)()));
        return static_cast<std::int64_t>(bounded);
    };
    return {component(point.x), component(point.y), component(point.z)};
}

[[nodiscard]] TriangleKey MakeTriangleKey(const TriangleRecord& triangle, const double epsilon) {
    TriangleKey key{
        .points = {
            Quantize(triangle.positions[0], epsilon),
            Quantize(triangle.positions[1], epsilon),
            Quantize(triangle.positions[2], epsilon),
        },
    };
    std::sort(key.points.begin(), key.points.end());
    return key;
}

[[nodiscard]] PlaneKey MakePlaneKey(const TriangleRecord& triangle, const double epsilon) {
    ri::math::Vec3 canonicalNormal = triangle.normal;
    int orientation = 1;
    if (canonicalNormal.x < -1.0e-6F
        || (std::abs(canonicalNormal.x) <= 1.0e-6F && canonicalNormal.y < -1.0e-6F)
        || (std::abs(canonicalNormal.x) <= 1.0e-6F
            && std::abs(canonicalNormal.y) <= 1.0e-6F
            && canonicalNormal.z < 0.0F)) {
        canonicalNormal = canonicalNormal * -1.0F;
        orientation = -1;
    }
    constexpr double normalEpsilon = 1.0e-5;
    const auto quantizeScalar = [](const double value, const double step) {
        return static_cast<std::int64_t>(std::llround(value / step));
    };
    const double distance = static_cast<double>(
        ri::math::Dot(canonicalNormal, triangle.positions[0]));
    (void)orientation;
    return {
        quantizeScalar(canonicalNormal.x, normalEpsilon),
        quantizeScalar(canonicalNormal.y, normalEpsilon),
        quantizeScalar(canonicalNormal.z, normalEpsilon),
        quantizeScalar(distance, epsilon),
    };
}

[[nodiscard]] int PlaneOrientation(const ri::math::Vec3& normal) {
    if (normal.x < -1.0e-6F
        || (std::abs(normal.x) <= 1.0e-6F && normal.y < -1.0e-6F)
        || (std::abs(normal.x) <= 1.0e-6F
            && std::abs(normal.y) <= 1.0e-6F
            && normal.z < 0.0F)) {
        return -1;
    }
    return 1;
}

void AppendBounds(ri::structural::CompiledMesh& mesh, const ri::math::Vec3& point) {
    if (!mesh.hasBounds) {
        mesh.boundsMin = point;
        mesh.boundsMax = point;
        mesh.hasBounds = true;
        return;
    }
    mesh.boundsMin.x = std::min(mesh.boundsMin.x, point.x);
    mesh.boundsMin.y = std::min(mesh.boundsMin.y, point.y);
    mesh.boundsMin.z = std::min(mesh.boundsMin.z, point.z);
    mesh.boundsMax.x = std::max(mesh.boundsMax.x, point.x);
    mesh.boundsMax.y = std::max(mesh.boundsMax.y, point.y);
    mesh.boundsMax.z = std::max(mesh.boundsMax.z, point.z);
}

[[nodiscard]] std::unordered_map<std::string, ri::math::Mat4> BuildGroupWorldMatrices(
    const ri::content::PrimitiveModelDocument& document,
    std::vector<std::string>& errors) {
    std::unordered_map<std::string, const ri::content::PrimitiveModelGroup*> groups{};
    for (const auto& group : document.groups) {
        groups[group.id] = &group;
    }
    std::unordered_map<std::string, ri::math::Mat4> world{};
    std::set<std::string> visiting{};
    std::function<bool(const ri::content::PrimitiveModelGroup&)> resolve =
        [&](const ri::content::PrimitiveModelGroup& group) {
            if (world.contains(group.id)) {
                return true;
            }
            if (!visiting.insert(group.id).second) {
                errors.push_back("Primitive model group transform cycle: " + group.id);
                return false;
            }
            ri::math::Mat4 parent = ri::math::IdentityMatrix();
            if (!group.parentId.empty()) {
                const auto found = groups.find(group.parentId);
                if (found == groups.end() || !resolve(*found->second)) {
                    visiting.erase(group.id);
                    return false;
                }
                parent = world[group.parentId];
            }
            world[group.id] = ri::math::Multiply(parent, TransformMatrix(group.transform));
            visiting.erase(group.id);
            return true;
        };
    for (const auto& group : document.groups) {
        (void)resolve(group);
    }
    return world;
}

[[nodiscard]] std::unordered_map<std::string, std::string> BuildGroupBoneBindings(
    const ri::content::PrimitiveModelDocument& document,
    std::vector<std::string>& errors) {
    std::unordered_map<std::string, const ri::content::PrimitiveModelGroup*> groups{};
    for (const auto& group : document.groups) {
        groups[group.id] = &group;
    }
    std::unordered_map<std::string, std::string> bindings{};
    std::set<std::string> visiting{};
    std::function<bool(const ri::content::PrimitiveModelGroup&)> resolve =
        [&](const ri::content::PrimitiveModelGroup& group) {
            if (bindings.contains(group.id)) {
                return true;
            }
            if (!visiting.insert(group.id).second) {
                errors.push_back("Primitive model group bone-binding cycle: " + group.id);
                return false;
            }
            std::string binding = group.boneName;
            if (binding.empty() && !group.parentId.empty()) {
                const auto parent = groups.find(group.parentId);
                if (parent == groups.end() || !resolve(*parent->second)) {
                    visiting.erase(group.id);
                    return false;
                }
                binding = bindings[group.parentId];
            }
            bindings[group.id] = std::move(binding);
            visiting.erase(group.id);
            return true;
        };
    for (const auto& group : document.groups) {
        (void)resolve(group);
    }
    return bindings;
}

void ValidateRigBindings(const ri::content::PrimitiveModelDocument& document,
                         const std::filesystem::path& documentDirectory,
                         PrimitiveModelBakeResult& result) {
    std::set<std::string> bindings{};
    for (const auto& group : document.groups) {
        if (!group.boneName.empty()) {
            bindings.insert(group.boneName);
        }
    }
    for (const auto& part : document.parts) {
        if (!part.boneName.empty()) {
            bindings.insert(part.boneName);
        }
    }
    if (bindings.empty()) {
        return;
    }
    if (document.rigPath.empty()) {
        result.errors.push_back("Primitive model has bone bindings but no rigPath.");
        return;
    }
    std::filesystem::path rigPath(document.rigPath);
    if (rigPath.is_relative()) {
        rigPath = documentDirectory / rigPath;
    }
    const auto rig = LoadRigDefinition(rigPath);
    if (!rig.has_value()) {
        result.errors.push_back("Primitive model rig could not be loaded: " + rigPath.string());
        return;
    }
    const RigValidationReport validation = ValidateRigDefinition(*rig);
    if (!validation.valid) {
        result.errors.push_back("Primitive model rig is invalid: " + rigPath.string());
        return;
    }
    std::set<std::string> bones{};
    for (const RigBone& bone : rig->bones) {
        bones.insert(bone.name);
    }
    for (const std::string& binding : bindings) {
        if (!bones.contains(binding)) {
            result.errors.push_back("Primitive model binding references missing bone: " + binding);
        }
    }
}

} // namespace

PrimitiveModelBakeResult BakePrimitiveModel(
    const ri::content::PrimitiveModelDocument& document,
    const std::filesystem::path& documentDirectory) {
    PrimitiveModelBakeResult result{};
    result.inputPartCount = document.parts.size();
    const ri::content::PrimitiveModelValidationReport validation =
        ri::content::ValidatePrimitiveModelDocument(document);
    result.errors = validation.errors;
    result.warnings = validation.warnings;
    ValidateRigBindings(document, documentDirectory, result);
    if (!result.errors.empty()) {
        return result;
    }

    std::vector<std::string> transformErrors{};
    const auto groupWorld = BuildGroupWorldMatrices(document, transformErrors);
    const auto groupBoneBindings = BuildGroupBoneBindings(document, transformErrors);
    result.errors.insert(result.errors.end(), transformErrors.begin(), transformErrors.end());
    if (!result.errors.empty()) {
        return result;
    }

    std::vector<TriangleRecord> triangles{};
    for (std::size_t partIndex = 0; partIndex < document.parts.size(); ++partIndex) {
        const ri::content::PrimitiveModelPart& part = document.parts[partIndex];
        if (!part.enabled) {
            continue;
        }
        const auto preset = FindStructuralPreset(part.primitivePreset);
        if (!preset.has_value()) {
            result.errors.push_back("Unknown Raw Iron primitive preset: " + part.primitivePreset
                                    + " (part " + part.id + ")");
            continue;
        }
        ri::math::Mat4 matrix = TransformMatrix(part.transform);
        if (!part.groupId.empty()) {
            const auto group = groupWorld.find(part.groupId);
            if (group == groupWorld.end()) {
                result.errors.push_back("Missing group transform for part: " + part.id);
                continue;
            }
            matrix = ri::math::Multiply(group->second, matrix);
        }
        const ri::structural::CompiledMesh primitive = ri::structural::BuildPrimitiveMesh(
            preset->structuralType,
            ShapeFromStructuralPreset(*preset));
        if (primitive.positions.size() < 3U || primitive.positions.size() % 3U != 0U) {
            result.warnings.push_back("Primitive produced no bakeable triangles: " + part.id);
            continue;
        }
        std::string effectiveBone = part.boneName;
        if (effectiveBone.empty() && !part.groupId.empty()) {
            if (const auto binding = groupBoneBindings.find(part.groupId);
                binding != groupBoneBindings.end()) {
                effectiveBone = binding->second;
            }
        }
        for (std::size_t offset = 0; offset + 2U < primitive.positions.size(); offset += 3U) {
            TriangleRecord triangle{
                .positions = {
                    ri::math::TransformPoint(matrix, primitive.positions[offset]),
                    ri::math::TransformPoint(matrix, primitive.positions[offset + 1U]),
                    ri::math::TransformPoint(matrix, primitive.positions[offset + 2U]),
                },
                .partIndex = partIndex,
                .boneName = effectiveBone,
            };
            triangle.normal = ri::math::Normalize(
                ri::math::Cross(
                    triangle.positions[1] - triangle.positions[0],
                    triangle.positions[2] - triangle.positions[0]));
            if (ri::math::LengthSquared(triangle.normal) <= 1.0e-12F) {
                result.warnings.push_back("Degenerate triangle omitted from part: " + part.id);
                continue;
            }
            triangles.push_back(triangle);
        }
        ++result.bakedPartCount;
    }
    result.inputTriangleCount = triangles.size();
    if (!result.errors.empty()) {
        return result;
    }

    if (document.bake.cullInternalFaces) {
        const double epsilon = std::clamp(
            static_cast<double>(document.bake.weldEpsilon),
            1.0e-8,
            0.1);
        // First remove complete coplanar patches. This handles adjacent primitives whose
        // shared quad faces use opposite triangle diagonals.
        std::unordered_map<PlaneKey, std::vector<FacePatch>, PlaneKeyHash> planeBuckets{};
        planeBuckets.reserve(triangles.size() / 2U);
        for (std::size_t index = 0; index < triangles.size(); ++index) {
            const TriangleRecord& triangle = triangles[index];
            auto& patches = planeBuckets[MakePlaneKey(triangle, epsilon)];
            const int orientation = PlaneOrientation(triangle.normal);
            auto patch = std::find_if(
                patches.begin(),
                patches.end(),
                [&](const FacePatch& value) {
                    return value.partIndex == triangle.partIndex
                        && value.orientation == orientation;
                });
            if (patch == patches.end()) {
                patches.push_back(FacePatch{
                    .partIndex = triangle.partIndex,
                    .orientation = orientation,
                });
                patch = std::prev(patches.end());
            }
            patch->triangles.push_back(index);
            for (const ri::math::Vec3& point : triangle.positions) {
                patch->points.insert(Quantize(point, epsilon));
            }
        }
        for (auto& [plane, patches] : planeBuckets) {
            (void)plane;
            for (std::size_t left = 0; left < patches.size(); ++left) {
                for (std::size_t right = left + 1U; right < patches.size(); ++right) {
                    FacePatch& a = patches[left];
                    FacePatch& b = patches[right];
                    if (a.partIndex == b.partIndex || a.orientation == b.orientation
                        || a.points != b.points) {
                        continue;
                    }
                    for (const std::size_t index : a.triangles) {
                        if (!triangles[index].removed) {
                            triangles[index].removed = true;
                            ++result.culledInternalTriangleCount;
                        }
                    }
                    for (const std::size_t index : b.triangles) {
                        if (!triangles[index].removed) {
                            triangles[index].removed = true;
                            ++result.culledInternalTriangleCount;
                        }
                    }
                }
            }
        }

        // Then collapse exact same-facing triangle duplicates that remain.
        std::unordered_map<TriangleKey, std::vector<std::size_t>, TriangleKeyHash> buckets{};
        buckets.reserve(triangles.size());
        for (std::size_t index = 0; index < triangles.size(); ++index) {
            if (triangles[index].removed) {
                continue;
            }
            buckets[MakeTriangleKey(triangles[index], epsilon)].push_back(index);
        }
        for (auto& [key, bucket] : buckets) {
            (void)key;
            for (std::size_t left = 0; left < bucket.size(); ++left) {
                TriangleRecord& a = triangles[bucket[left]];
                if (a.removed) {
                    continue;
                }
                for (std::size_t right = left + 1U; right < bucket.size(); ++right) {
                    TriangleRecord& b = triangles[bucket[right]];
                    if (b.removed) {
                        continue;
                    }
                    const float alignment = ri::math::Dot(a.normal, b.normal);
                    if (alignment > 0.999F) {
                        b.removed = true;
                        ++result.culledDuplicateTriangleCount;
                    }
                }
            }
        }
    }

    const std::size_t removedTriangleCount =
        result.culledInternalTriangleCount + result.culledDuplicateTriangleCount;
    result.mesh.positions.reserve((triangles.size() - removedTriangleCount) * 3U);
    result.mesh.normals.reserve(result.mesh.positions.capacity());
    result.vertexBoneNames.reserve(result.mesh.positions.capacity());
    for (const TriangleRecord& triangle : triangles) {
        if (triangle.removed) {
            continue;
        }
        for (const ri::math::Vec3& position : triangle.positions) {
            result.mesh.positions.push_back(position);
            result.mesh.normals.push_back(triangle.normal);
            result.vertexBoneNames.push_back(triangle.boneName);
            AppendBounds(result.mesh, position);
        }
        ++result.mesh.triangleCount;
    }
    result.outputTriangleCount = result.mesh.triangleCount;
    result.valid = result.errors.empty() && result.bakedPartCount > 0U && result.mesh.triangleCount > 0U;
    return result;
}

bool SavePrimitiveModelBakeObj(const std::filesystem::path& path,
                               const PrimitiveModelBakeResult& bake) {
    if (!bake.valid || bake.mesh.positions.size() != bake.mesh.normals.size()
        || bake.mesh.positions.size() % 3U != 0U) {
        return false;
    }
    std::error_code error{};
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << "# Raw Iron Forge primitive model bake\n";
    output << "# triangles " << bake.outputTriangleCount << "\n";
    for (const ri::math::Vec3& position : bake.mesh.positions) {
        output << "v " << position.x << ' ' << position.y << ' ' << position.z << '\n';
    }
    for (const ri::math::Vec3& normal : bake.mesh.normals) {
        output << "vn " << normal.x << ' ' << normal.y << ' ' << normal.z << '\n';
    }
    for (std::size_t offset = 0; offset < bake.mesh.positions.size(); offset += 3U) {
        const std::size_t a = offset + 1U;
        const std::size_t b = offset + 2U;
        const std::size_t c = offset + 3U;
        output << "f " << a << "//" << a << ' ' << b << "//" << b << ' ' << c << "//" << c << '\n';
    }
    return output.good();
}

bool SavePrimitiveModelBakeRigMap(const std::filesystem::path& path,
                                  const PrimitiveModelBakeResult& bake) {
    if (!bake.valid || bake.vertexBoneNames.size() != bake.mesh.positions.size()) {
        return false;
    }
    std::error_code error{};
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << "{\n"
           << "  \"formatVersion\": 1,\n"
           << "  \"bindingMode\": \"rigid-part\",\n"
           << "  \"vertexCount\": " << bake.vertexBoneNames.size() << ",\n"
           << "  \"ranges\": [\n";
    bool wroteRange = false;
    for (std::size_t begin = 0; begin < bake.vertexBoneNames.size();) {
        const std::string& bone = bake.vertexBoneNames[begin];
        std::size_t end = begin + 1U;
        while (end < bake.vertexBoneNames.size() && bake.vertexBoneNames[end] == bone) {
            ++end;
        }
        if (!bone.empty()) {
            if (wroteRange) {
                output << ",\n";
            }
            output << "    {\"firstVertex\": " << begin
                   << ", \"vertexCount\": " << (end - begin)
                   << ", \"bone\": \""
                   << ri::core::detail::EscapeJsonString(bone)
                   << "\"}";
            wroteRange = true;
        }
        begin = end;
    }
    if (wroteRange) {
        output << '\n';
    }
    output << "  ]\n"
           << "}\n";
    return output.good();
}

PrimitiveModelInstantiationResult InstantiatePrimitiveModel(
    Scene& scene,
    const int parentNode,
    const ri::content::PrimitiveModelDocument& document,
    const std::filesystem::path& documentDirectory) {
    PrimitiveModelInstantiationResult result{};
    const ri::content::PrimitiveModelValidationReport documentValidation =
        ri::content::ValidatePrimitiveModelDocument(document);
    result.errors = documentValidation.errors;
    result.warnings = documentValidation.warnings;
    PrimitiveModelBakeResult rigValidation{};
    ValidateRigBindings(document, documentDirectory, rigValidation);
    result.errors.insert(
        result.errors.end(),
        rigValidation.errors.begin(),
        rigValidation.errors.end());
    result.warnings.insert(
        result.warnings.end(),
        rigValidation.warnings.begin(),
        rigValidation.warnings.end());
    if (!result.errors.empty()) {
        return result;
    }
    std::vector<std::string> bindingErrors{};
    const auto groupBoneBindings = BuildGroupBoneBindings(document, bindingErrors);
    result.errors.insert(result.errors.end(), bindingErrors.begin(), bindingErrors.end());
    if (!result.errors.empty()) {
        return result;
    }

    result.rootNode = scene.CreateNode(
        document.displayName.empty() ? document.modelId : document.displayName,
        parentNode);
    std::unordered_map<std::string, int> groupHandles{};
    std::size_t remaining = document.groups.size();
    while (remaining > 0U) {
        bool progressed = false;
        for (const auto& group : document.groups) {
            if (groupHandles.contains(group.id)) {
                continue;
            }
            int parent = result.rootNode;
            if (!group.parentId.empty()) {
                const auto found = groupHandles.find(group.parentId);
                if (found == groupHandles.end()) {
                    continue;
                }
                parent = found->second;
            }
            const int handle = scene.CreateNode(group.name.empty() ? group.id : group.name, parent);
            scene.GetNode(handle).localTransform = SceneTransform(group.transform);
            groupHandles[group.id] = handle;
            result.groupNodes.push_back(handle);
            --remaining;
            progressed = true;
        }
        if (!progressed) {
            result.errors.push_back("Primitive model group hierarchy could not be instantiated.");
            return result;
        }
    }

    for (const auto& part : document.parts) {
        if (!part.enabled) {
            continue;
        }
        const auto preset = FindStructuralPreset(part.primitivePreset);
        if (!preset.has_value()) {
            result.errors.push_back("Unknown Raw Iron primitive preset: " + part.primitivePreset);
            continue;
        }
        int parent = result.rootNode;
        if (!part.groupId.empty()) {
            const auto found = groupHandles.find(part.groupId);
            if (found == groupHandles.end()) {
                result.errors.push_back("Primitive part group was not instantiated: " + part.id);
                continue;
            }
            parent = found->second;
        }
        StructuralBrushSpawnOptions options{};
        options.nodeName = part.name.empty() ? part.id : part.name;
        options.structuralType = preset->structuralType;
        options.shape = ShapeFromStructuralPreset(*preset);
        options.parent = parent;
        options.transform = SceneTransform(part.transform);
        options.materialName = part.materialId.empty() ? "default" : part.materialId;
        options.metadata.brushId = document.modelId + "/" + part.id;
        options.metadata.region = "forge-model";
        std::string effectiveBone = part.boneName;
        if (effectiveBone.empty() && !part.groupId.empty()) {
            if (const auto binding = groupBoneBindings.find(part.groupId);
                binding != groupBoneBindings.end()) {
                effectiveBone = binding->second;
            }
        }
        options.metadata.informationLayer.gameplayMeaning =
            effectiveBone.empty() ? "primitive-model-part" : "bone:" + effectiveBone;
        result.partNodes.push_back(AddStructuralBrushNode(scene, options));
    }
    result.valid = result.errors.empty();
    return result;
}

} // namespace ri::scene
