#include "RawIron/Content/NativeAnimationDocument.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace ri::content {
namespace {

namespace detail_scan = ri::core::detail;

[[nodiscard]] bool IsFinite(const DeclarativeVec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] DeclarativeVec3 ReadVec3(const std::string_view objectText, const DeclarativeVec3 fallback = {}) {
    return DeclarativeVec3{
        .x = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "x").value_or(fallback.x)),
        .y = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "y").value_or(fallback.y)),
        .z = static_cast<float>(detail_scan::ExtractJsonDouble(objectText, "z").value_or(fallback.z)),
    };
}

void WriteVec3(std::ostringstream& json, const std::string_view key, const DeclarativeVec3& value, const int indent) {
    const std::string padding(static_cast<std::size_t>(indent), ' ');
    json << padding << "\"" << key << "\": {\"x\": " << std::setprecision(9) << value.x
         << ", \"y\": " << value.y << ", \"z\": " << value.z << "}";
}

[[nodiscard]] std::string Slugify(std::string value, const std::string_view fallback) {
    std::string slug{};
    bool pending = false;
    for (const unsigned char raw : value) {
        if ((raw >= 'a' && raw <= 'z') || (raw >= '0' && raw <= '9')) {
            if (pending && !slug.empty()) {
                slug.push_back('_');
            }
            pending = false;
            slug.push_back(static_cast<char>(raw));
        } else if (raw >= 'A' && raw <= 'Z') {
            if (pending && !slug.empty()) {
                slug.push_back('_');
            }
            pending = false;
            slug.push_back(static_cast<char>(raw - 'A' + 'a'));
        } else {
            pending = true;
        }
    }
    return slug.empty() ? std::string(fallback) : slug;
}

[[nodiscard]] std::string TrimEventName(std::string value) {
    const auto isSpace = [](const unsigned char character) {
        return character == ' ' || character == '\t' || character == '\n' || character == '\r';
    };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    if (value.size() > static_cast<std::size_t>(NativeAnimationDocument::kMaxEventName)) {
        value.resize(static_cast<std::size_t>(NativeAnimationDocument::kMaxEventName));
    }
    return value;
}

} // namespace

NativeAnimationDocument CreateNativeAnimationDocument(
    std::string id,
    std::string displayName,
    std::string rigPath) {
    NativeAnimationDocument document{};
    document.id = Slugify(std::move(id), "clip");
    document.displayName = displayName.empty() ? document.id : std::move(displayName);
    document.rigPath = std::move(rigPath);
    document.durationSeconds = 1.0;
    document.looping = true;
    document.rootMotion = true;
    return document;
}

NativeAnimationValidationReport ValidateNativeAnimationDocument(const NativeAnimationDocument& document) {
    NativeAnimationValidationReport report{};
    if (document.formatVersion != NativeAnimationDocument::kFormatVersion) {
        report.errors.push_back("Animation formatVersion is unsupported.");
    }
    if (document.id.empty()) {
        report.errors.push_back("Animation id must be non-empty.");
    }
    if (document.displayName.empty()) {
        report.warnings.push_back("Animation display name is empty.");
    }
    if (document.rigPath.empty()) {
        report.warnings.push_back("Animation has no rig path; preview bind will wait for a selected skeleton.");
    } else if (document.rigPath.find("..") != std::string::npos) {
        report.errors.push_back("Animation rig path cannot traverse directories.");
    }
    if (!std::isfinite(document.durationSeconds) || document.durationSeconds <= 0.0
        || document.durationSeconds > 600.0) {
        report.errors.push_back("Animation duration must be finite and in (0, 600].");
    }
    if (document.tracks.size() > static_cast<std::size_t>(NativeAnimationDocument::kMaxTracks)) {
        report.errors.push_back("Animation has too many bone tracks.");
    }

    std::unordered_set<std::string> boneNames{};
    for (const NativeAnimationTrack& track : document.tracks) {
        if (track.boneName.empty()) {
            report.errors.push_back("Animation track is missing a bone name.");
            continue;
        }
        if (!boneNames.insert(track.boneName).second) {
            report.errors.push_back("Duplicate animation track: " + track.boneName);
        }
        if (track.keys.size() > static_cast<std::size_t>(NativeAnimationDocument::kMaxKeysPerTrack)) {
            report.errors.push_back("Animation track has too many keys: " + track.boneName);
        }
        report.keyCount += track.keys.size();
        for (const NativeAnimationKeyframe& key : track.keys) {
            if (!std::isfinite(key.timeSeconds) || key.timeSeconds < 0.0) {
                report.errors.push_back("Animation key time is invalid on " + track.boneName);
            }
            if (!IsFinite(key.translation) || !IsFinite(key.rotationDegrees) || !IsFinite(key.scale)) {
                report.errors.push_back("Animation key transform is non-finite on " + track.boneName);
            }
            if (std::abs(key.scale.x) <= 1.0e-6F || std::abs(key.scale.y) <= 1.0e-6F
                || std::abs(key.scale.z) <= 1.0e-6F) {
                report.errors.push_back("Animation key scale cannot contain zero on " + track.boneName);
            }
        }
    }
    report.trackCount = document.tracks.size();
    if (document.tracks.empty()) {
        report.warnings.push_back("Animation contains no bone tracks.");
    }
    if (document.events.size() > static_cast<std::size_t>(NativeAnimationDocument::kMaxEvents)) {
        report.errors.push_back("Animation has too many events.");
    }
    std::unordered_set<std::string> eventKeys{};
    for (const NativeAnimationEvent& event : document.events) {
        const std::string name = TrimEventName(event.name);
        if (name.empty()) {
            report.errors.push_back("Animation event is missing a name.");
            continue;
        }
        if (!std::isfinite(event.timeSeconds) || event.timeSeconds < 0.0) {
            report.errors.push_back("Animation event time is invalid on " + name);
        }
        if (event.timeSeconds > document.durationSeconds + 1.0e-6) {
            report.warnings.push_back("Animation event '" + name + "' is past clip duration.");
        }
        const std::string key = name + "@" + std::to_string(event.timeSeconds);
        if (!eventKeys.insert(key).second) {
            report.warnings.push_back("Duplicate animation event: " + name);
        }
        ++report.eventCount;
    }
    report.valid = report.errors.empty();
    return report;
}

std::string SerializeNativeAnimationDocument(const NativeAnimationDocument& document) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"formatVersion\": " << document.formatVersion << ",\n";
    json << "  \"id\": \"" << detail_scan::EscapeJsonString(document.id) << "\",\n";
    json << "  \"displayName\": \"" << detail_scan::EscapeJsonString(document.displayName) << "\",\n";
    json << "  \"rigPath\": \"" << detail_scan::EscapeJsonString(document.rigPath) << "\",\n";
    json << "  \"durationSeconds\": " << std::setprecision(9) << document.durationSeconds << ",\n";
    json << "  \"looping\": " << (document.looping ? "true" : "false") << ",\n";
    json << "  \"rootMotion\": " << (document.rootMotion ? "true" : "false") << ",\n";
    json << "  \"tracks\": [\n";
    for (std::size_t trackIndex = 0; trackIndex < document.tracks.size(); ++trackIndex) {
        const NativeAnimationTrack& track = document.tracks[trackIndex];
        json << "    {\n";
        json << "      \"boneName\": \"" << detail_scan::EscapeJsonString(track.boneName) << "\",\n";
        json << "      \"keys\": [\n";
        for (std::size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex) {
            const NativeAnimationKeyframe& key = track.keys[keyIndex];
            json << "        {\n";
            json << "          \"timeSeconds\": " << std::setprecision(9) << key.timeSeconds << ",\n";
            WriteVec3(json, "translation", key.translation, 10);
            json << ",\n";
            WriteVec3(json, "rotationDegrees", key.rotationDegrees, 10);
            json << ",\n";
            WriteVec3(json, "scale", key.scale, 10);
            json << "\n        }" << (keyIndex + 1U < track.keys.size() ? "," : "") << "\n";
        }
        json << "      ]\n";
        json << "    }" << (trackIndex + 1U < document.tracks.size() ? "," : "") << "\n";
    }
    json << "  ],\n";
    json << "  \"events\": [\n";
    for (std::size_t eventIndex = 0; eventIndex < document.events.size(); ++eventIndex) {
        const NativeAnimationEvent& event = document.events[eventIndex];
        json << "    {\n";
        json << "      \"timeSeconds\": " << std::setprecision(9) << event.timeSeconds << ",\n";
        json << "      \"name\": \"" << detail_scan::EscapeJsonString(event.name) << "\"\n";
        json << "    }" << (eventIndex + 1U < document.events.size() ? "," : "") << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    return json.str();
}

std::optional<NativeAnimationDocument> ParseNativeAnimationDocument(const std::string_view jsonText) {
    NativeAnimationDocument document{};
    document.formatVersion = detail_scan::ExtractJsonInt(jsonText, "formatVersion")
                                 .value_or(NativeAnimationDocument::kFormatVersion);
    document.id = detail_scan::ExtractJsonString(jsonText, "id").value_or("");
    document.displayName = detail_scan::ExtractJsonString(jsonText, "displayName").value_or(document.id);
    document.rigPath = detail_scan::ExtractJsonString(jsonText, "rigPath").value_or("");
    document.durationSeconds = detail_scan::ExtractJsonDouble(jsonText, "durationSeconds").value_or(1.0);
    document.looping = detail_scan::ExtractJsonBool(jsonText, "looping").value_or(true);
    document.rootMotion = detail_scan::ExtractJsonBool(jsonText, "rootMotion").value_or(true);
    for (const std::string_view trackObject : detail_scan::SplitJsonArrayObjects(jsonText, "tracks")) {
        NativeAnimationTrack track{};
        track.boneName = detail_scan::ExtractJsonString(trackObject, "boneName").value_or("");
        for (const std::string_view keyObject : detail_scan::SplitJsonArrayObjects(trackObject, "keys")) {
            NativeAnimationKeyframe key{};
            key.timeSeconds = detail_scan::ExtractJsonDouble(keyObject, "timeSeconds").value_or(0.0);
            if (const auto translation = detail_scan::ExtractJsonObject(keyObject, "translation")) {
                key.translation = ReadVec3(*translation);
            }
            if (const auto rotation = detail_scan::ExtractJsonObject(keyObject, "rotationDegrees")) {
                key.rotationDegrees = ReadVec3(*rotation);
            }
            if (const auto scale = detail_scan::ExtractJsonObject(keyObject, "scale")) {
                key.scale = ReadVec3(*scale, {1.0F, 1.0F, 1.0F});
            }
            track.keys.push_back(key);
        }
        document.tracks.push_back(std::move(track));
    }
    for (const std::string_view eventObject : detail_scan::SplitJsonArrayObjects(jsonText, "events")) {
        NativeAnimationEvent event{};
        event.timeSeconds = detail_scan::ExtractJsonDouble(eventObject, "timeSeconds").value_or(0.0);
        event.name = detail_scan::ExtractJsonString(eventObject, "name").value_or("");
        document.events.push_back(std::move(event));
    }
    if (document.id.empty()) {
        return std::nullopt;
    }
    return document;
}

std::optional<NativeAnimationDocument> LoadNativeAnimationDocument(const std::filesystem::path& path) {
    const std::string json = detail_scan::ReadTextFile(path);
    return json.empty() ? std::nullopt : ParseNativeAnimationDocument(json);
}

bool SaveNativeAnimationDocument(const std::filesystem::path& path, const NativeAnimationDocument& document) {
    if (!ValidateNativeAnimationDocument(document).valid) {
        return false;
    }
    std::error_code error{};
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }
    return detail_scan::WriteTextFile(path, SerializeNativeAnimationDocument(document));
}

} // namespace ri::content
