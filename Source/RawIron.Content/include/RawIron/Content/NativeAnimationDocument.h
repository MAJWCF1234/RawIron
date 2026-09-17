#pragma once

#include "RawIron/Content/DeclarativeModelDefinition.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

struct NativeAnimationKeyframe {
    double timeSeconds = 0.0;
    DeclarativeVec3 translation{};
    DeclarativeVec3 rotationDegrees{};
    DeclarativeVec3 scale{1.0F, 1.0F, 1.0F};
};

struct NativeAnimationTrack {
    std::string boneName;
    std::vector<NativeAnimationKeyframe> keys;
};

struct NativeAnimationEvent {
    double timeSeconds = 0.0;
    std::string name;
};

/// Portable bone-name animation clip. Node handles are bound at preview/runtime, never stored.
struct NativeAnimationDocument {
    static constexpr int kFormatVersion = 1;
    static constexpr int kMaxTracks = 256;
    static constexpr int kMaxKeysPerTrack = 4096;
    static constexpr int kMaxEvents = 256;
    static constexpr int kMaxEventName = 64;

    int formatVersion = kFormatVersion;
    std::string id;
    std::string displayName;
    /// Workspace-relative or document-adjacent `.ri_rig.json`.
    std::string rigPath;
    double durationSeconds = 1.0;
    bool looping = true;
    /// When false, preview holds the root bone's translation at rest so cycles stay in place.
    bool rootMotion = true;
    std::vector<NativeAnimationTrack> tracks;
    std::vector<NativeAnimationEvent> events;
};

struct NativeAnimationValidationReport {
    bool valid = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::size_t trackCount = 0;
    std::size_t keyCount = 0;
    std::size_t eventCount = 0;
};

[[nodiscard]] NativeAnimationDocument CreateNativeAnimationDocument(
    std::string id,
    std::string displayName = {},
    std::string rigPath = {});

[[nodiscard]] NativeAnimationValidationReport ValidateNativeAnimationDocument(
    const NativeAnimationDocument& document);

[[nodiscard]] std::string SerializeNativeAnimationDocument(const NativeAnimationDocument& document);
[[nodiscard]] std::optional<NativeAnimationDocument> ParseNativeAnimationDocument(std::string_view jsonText);
[[nodiscard]] std::optional<NativeAnimationDocument> LoadNativeAnimationDocument(
    const std::filesystem::path& path);
[[nodiscard]] bool SaveNativeAnimationDocument(
    const std::filesystem::path& path,
    const NativeAnimationDocument& document);

} // namespace ri::content
