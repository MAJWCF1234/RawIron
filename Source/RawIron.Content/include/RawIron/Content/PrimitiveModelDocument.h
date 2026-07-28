#pragma once

#include "RawIron/Content/DeclarativeModelDefinition.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

struct PrimitiveModelTransform {
    DeclarativeVec3 translation{};
    DeclarativeVec3 rotationDegrees{};
    DeclarativeVec3 scale{1.0F, 1.0F, 1.0F};
};

/// Hierarchical transform/pivot shared by multiple primitive parts.
struct PrimitiveModelGroup {
    std::string id;
    std::string name;
    std::string parentId;
    std::string boneName;
    PrimitiveModelTransform transform{};
};

struct PrimitiveModelPart {
    std::string id;
    std::string name;
    std::string groupId;
    /// Label from Raw Iron's complete StructuralPrimitivePresets catalog.
    std::string primitivePreset{"box"};
    std::string materialId{"default"};
    /// Optional direct bone binding. Empty inherits the nearest group's bone binding.
    std::string boneName;
    PrimitiveModelTransform transform{};
    bool enabled = true;
};

struct PrimitiveModelBakeSettings {
    bool cullInternalFaces = true;
    float weldEpsilon = 0.0001F;
};

/// Editable Forge model composed from Raw Iron native primitives.
struct PrimitiveModelDocument {
    static constexpr int kFormatVersion = 1;

    int formatVersion = kFormatVersion;
    std::string modelId;
    std::string displayName;
    /// Optional path relative to this document. Bone bindings are validated against it during bake.
    std::string rigPath;
    std::vector<PrimitiveModelGroup> groups;
    std::vector<PrimitiveModelPart> parts;
    PrimitiveModelBakeSettings bake{};
};

struct PrimitiveModelValidationReport {
    bool valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::size_t rootGroupCount = 0;
    std::size_t enabledPartCount = 0;
};

[[nodiscard]] PrimitiveModelDocument CreatePrimitiveModelDocument(std::string modelId,
                                                                  std::string displayName = {});
[[nodiscard]] PrimitiveModelValidationReport ValidatePrimitiveModelDocument(
    const PrimitiveModelDocument& document);

[[nodiscard]] std::string SerializePrimitiveModelDocument(const PrimitiveModelDocument& document);
[[nodiscard]] std::optional<PrimitiveModelDocument> ParsePrimitiveModelDocument(std::string_view jsonText);
[[nodiscard]] std::optional<PrimitiveModelDocument> LoadPrimitiveModelDocument(
    const std::filesystem::path& path);
[[nodiscard]] bool SavePrimitiveModelDocument(const std::filesystem::path& path,
                                              const PrimitiveModelDocument& document);

/// Adds a collision-safe group or part id and returns the inserted id. Empty means validation failed.
[[nodiscard]] std::string AddPrimitiveModelGroup(PrimitiveModelDocument& document,
                                                 std::string name,
                                                 std::string parentId = {},
                                                 std::string boneName = {});
[[nodiscard]] std::string AddPrimitiveModelPart(PrimitiveModelDocument& document,
                                                std::string primitivePreset,
                                                std::string groupId = {},
                                                std::string name = {});

} // namespace ri::content
