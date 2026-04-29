#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

/// Data-driven description of a static model composition (mesh parts, transforms, materials).
struct DeclarativeVec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct DeclarativeQuat {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 1.0F;
};

struct DeclarativeModelPart {
    std::string name;
    /// Empty parent means root-level placement relative to the model frame.
    std::string parentName;
    std::string meshId;
    std::string materialId;
    DeclarativeVec3 translation{};
    DeclarativeQuat rotation{};
    DeclarativeVec3 scale{1.0F, 1.0F, 1.0F};
    std::vector<std::string> tags;
};

struct DeclarativeModelDefinition {
    static constexpr int kFormatVersion = 1;

    int formatVersion = kFormatVersion;
    std::string modelId;
    std::vector<DeclarativeModelPart> parts;
};

[[nodiscard]] std::string SerializeDeclarativeModelDefinition(const DeclarativeModelDefinition& model);

[[nodiscard]] std::optional<DeclarativeModelDefinition> ParseDeclarativeModelDefinition(std::string_view jsonText);

[[nodiscard]] std::optional<DeclarativeModelDefinition> LoadDeclarativeModelDefinition(const std::filesystem::path& path);

[[nodiscard]] bool SaveDeclarativeModelDefinition(const std::filesystem::path& path,
                                                  const DeclarativeModelDefinition& model);

} // namespace ri::content
