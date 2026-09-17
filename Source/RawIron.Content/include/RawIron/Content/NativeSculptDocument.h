#pragma once

#include "RawIron/Scene/Components.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

struct NativeSculptVertexInfluence {
    std::string boneName;
    float weight = 0.0f;

    friend bool operator==(const NativeSculptVertexInfluence& lhs, const NativeSculptVertexInfluence& rhs) {
        return lhs.boneName == rhs.boneName && lhs.weight == rhs.weight;
    }
};

/// Native Forge sculpt mesh. This is the in-engine clay/hard-surface source, not an imported DCC file.
struct NativeSculptDocument {
    static constexpr int kFormatVersion = 1;
    static constexpr int kMaxVertices = 200000;
    static constexpr int kMaxIndices = 1200000;
    static constexpr int kMaxInfluences = 4;

    int formatVersion = kFormatVersion;
    std::string id{};
    std::string displayName{};
    /// `sphere` (clay ball) or `cube` (hard-surface cage).
    std::string cage{"sphere"};
    int segmentsAround = 32;
    int segmentsDown = 16;
    /// Workspace-relative or filename path to a `.ri_rig.json`. Empty means unbound clay.
    std::string rigPath{};
    /// Dominant bone per vertex, parallel to `mesh.positions`. Empty means not yet bound.
    std::vector<std::string> vertexBoneNames{};
    /// Up to four blended influences per vertex. Empty means rigid `vertexBoneNames` at weight 1.
    std::vector<std::vector<NativeSculptVertexInfluence>> vertexInfluences{};
    ri::scene::Mesh mesh{};
};

struct NativeSculptValidationReport {
    bool valid = false;
    std::vector<std::string> errors{};
    std::vector<std::string> warnings{};
    std::size_t vertexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t unboundVertexCount = 0;
    std::size_t blendedVertexCount = 0;
};

[[nodiscard]] NativeSculptValidationReport ValidateNativeSculptDocument(const NativeSculptDocument& document);
[[nodiscard]] std::string SerializeNativeSculptDocument(const NativeSculptDocument& document);
[[nodiscard]] std::optional<NativeSculptDocument> ParseNativeSculptDocument(std::string_view jsonText);
[[nodiscard]] std::optional<NativeSculptDocument> LoadNativeSculptDocument(const std::filesystem::path& path);
[[nodiscard]] bool SaveNativeSculptDocument(const std::filesystem::path& path, const NativeSculptDocument& document);

} // namespace ri::content
