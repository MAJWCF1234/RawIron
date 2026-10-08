#pragma once

#include "RawIron/Content/DeclarativeModelDefinition.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

/// One rigid colored block on a humanoid bone (CLI/Forge authoring output, not engine content).
struct BlockCharacterPart {
    std::string id{};
    std::string name{};
    std::string boneName{};
    /// box | prism | dome | wedge | cylinder | spike | slab | bevel | capsule
    std::string shape{"box"};
    DeclarativeVec3 center{};
    DeclarativeVec3 halfExtent{0.1F, 0.1F, 0.1F};
    /// Zero = no taper (same as halfExtent).
    DeclarativeVec3 halfExtentTop{};
    DeclarativeVec3 rotationDegrees{};
    DeclarativeVec3 albedoColor{0.62F, 0.55F, 0.46F};
    float roughness = 0.88F;
    float metallic = 0.0F;
    /// Ring facets for prism/cylinder/dome/capsule (0 = shape default).
    int sides = 0;
    /// Edge chamfer fraction of min half-extent for bevel/slab (0..0.45).
    float bevel = 0.0F;
    /// Workspace-relative albedo map (e.g. characters/psx_scout/fatigue.tga).
    std::string albedoTexture{};
};

/// Authored PSX/block humanoid: parts + colors + textures. Previewed by Forge; emitted by ri_tool.
struct BlockCharacterDocument {
    static constexpr int kFormatVersion = 1;

    int formatVersion = kFormatVersion;
    std::string id{};
    std::string displayName{};
    std::string rigPath{};
    std::vector<BlockCharacterPart> parts{};
};

struct BlockCharacterValidationReport {
    bool valid = false;
    std::vector<std::string> errors{};
    std::vector<std::string> warnings{};
    std::size_t partCount = 0;
    std::size_t enabledPartCount = 0;
};

[[nodiscard]] BlockCharacterValidationReport ValidateBlockCharacterDocument(
    const BlockCharacterDocument& document);
[[nodiscard]] std::string SerializeBlockCharacterDocument(const BlockCharacterDocument& document);
[[nodiscard]] std::optional<BlockCharacterDocument> ParseBlockCharacterDocument(std::string_view jsonText);
[[nodiscard]] std::optional<BlockCharacterDocument> LoadBlockCharacterDocument(
    const std::filesystem::path& path);
[[nodiscard]] bool SaveBlockCharacterDocument(
    const std::filesystem::path& path,
    const BlockCharacterDocument& document);

[[nodiscard]] bool IsBlockCharacterPath(const std::filesystem::path& path);

} // namespace ri::content
