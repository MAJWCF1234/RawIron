#include "RawIron/Content/GameRuntimeSupport.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {
namespace {

[[nodiscard]] std::string Trim(const std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::string NormalizeKey(std::string text) {
    for (char& ch : text) {
        if (ch == '\\') {
            ch = '/';
        } else {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }
    return text;
}

[[nodiscard]] std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(line);
    while (std::getline(stream, current, ',')) {
        parts.push_back(Trim(current));
    }
    return parts;
}

/// Authoring convention: `# col1,col2,...` documents column names; treat as header row after `#`.
[[nodiscard]] std::string StripLeadingCsvHeaderComment(std::string_view line) {
    std::string s = Trim(line);
    if (!s.empty() && s.front() == '#') {
        s.erase(0, 1);
        return Trim(std::move(s));
    }
    return s;
}

[[nodiscard]] float ParseFloatLoose(std::string_view text, float fallback) {
    const std::string trimmed = Trim(text);
    if (trimmed.empty()) {
        return fallback;
    }
    try {
        return std::stof(trimmed);
    } catch (...) {
        return fallback;
    }
}

[[nodiscard]] int FindColumnIndex(const std::vector<std::string>& headerParts,
                                  std::initializer_list<std::string_view> aliases) {
    for (std::size_t index = 0; index < headerParts.size(); ++index) {
        const std::string normalizedHeader = NormalizeKey(headerParts[index]);
        for (const std::string_view alias : aliases) {
            if (normalizedHeader == NormalizeKey(std::string(alias))) {
                return static_cast<int>(index);
            }
        }
    }
    return -1;
}

void LoadStreamingManifest(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::string assetPath;
        int priority = 0;

        const std::size_t equals = trimmed.find('=');
        if (equals != std::string::npos) {
            assetPath = Trim(std::string_view(trimmed).substr(0, equals));
            const std::string value = Trim(std::string_view(trimmed).substr(equals + 1U));
            try {
                priority = std::stoi(value);
            } catch (...) {
                continue;
            }
        } else {
            const std::vector<std::string> parts = SplitCsvLine(trimmed);
            if (parts.size() < 2U) {
                continue;
            }
            assetPath = parts[0];
            try {
                priority = std::stoi(parts[1]);
            } catch (...) {
                continue;
            }
        }

        if (assetPath.empty()) {
            continue;
        }
        out.streamingPrioritiesByPath[NormalizeKey(assetPath)] = priority;
    }
}

void LoadDependencies(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    const std::string text = ri::core::detail::ReadTextFile(path);
    if (text.empty()) {
        return;
    }
    const std::vector<std::string_view> dependencyObjects = ri::core::detail::SplitJsonArrayObjects(text, "dependencies");
    for (const std::string_view objectText : dependencyObjects) {
        const std::string assetPath = NormalizeKey(ri::core::detail::ExtractJsonString(objectText, "asset").value_or(""));
        if (assetPath.empty()) {
            continue;
        }
        const std::vector<std::string> dependsOn = ri::core::detail::ExtractJsonStringArray(objectText, "dependsOn");
        for (const std::string& rawProvider : dependsOn) {
            const std::string providerPath = NormalizeKey(rawProvider);
            if (providerPath.empty() || providerPath == assetPath) {
                continue;
            }
            out.dependencyInboundCountByPath[providerPath] += 1;
        }
    }
}

void LoadZones(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }

    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    int levelColumn = -1;
    int priorityColumn = -1;
    for (std::size_t index = 0; index < headerParts.size(); ++index) {
        const std::string name = NormalizeKey(headerParts[index]);
        if (name == "target_level" || name == "level" || name == "level_id") {
            levelColumn = static_cast<int>(index);
        } else if (name == "priority" || name == "stream_priority") {
            priorityColumn = static_cast<int>(index);
        }
    }
    if (levelColumn < 0 || priorityColumn < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::vector<std::string> parts = SplitCsvLine(line);
        if (levelColumn >= static_cast<int>(parts.size()) || priorityColumn >= static_cast<int>(parts.size())) {
            continue;
        }
        const std::string level = NormalizeKey(parts[static_cast<std::size_t>(levelColumn)]);
        if (level.empty()) {
            continue;
        }
        int priority = 0;
        try {
            priority = std::stoi(parts[static_cast<std::size_t>(priorityColumn)]);
        } catch (...) {
            continue;
        }
        auto it = out.zonePriorityByLevelId.find(level);
        if (it == out.zonePriorityByLevelId.end() || priority > it->second) {
            out.zonePriorityByLevelId[level] = priority;
        }
    }
}

void LoadLookupIndex(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = NormalizeKey(Trim(std::string_view(trimmed).substr(0, equals)));
        const std::string value = Trim(std::string_view(trimmed).substr(equals + 1U));
        if (!key.empty() && !value.empty()) {
            out.lookupIndex[key] = value;
        }
    }
}

void LoadKeyValueScalars(const std::filesystem::path& path, std::unordered_map<std::string, std::string>& outMap) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        std::string key = Trim(std::string_view(trimmed).substr(0, equals));
        std::string value = Trim(std::string_view(trimmed).substr(equals + 1U));
        if (key.empty()) {
            continue;
        }
        key = NormalizeKey(key);
        if (!value.empty()) {
            outMap[std::move(key)] = std::move(value);
        }
    }
}

void LoadLevelTriggersCsv(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    headerLine = StripLeadingCsvHeaderComment(headerLine);
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    const int idCol = FindColumnIndex(headerParts, {"trigger_id", "id"});
    const int eventCol = FindColumnIndex(headerParts, {"event_type", "event"});
    const int minXCol = FindColumnIndex(headerParts, {"min_x"});
    const int minYCol = FindColumnIndex(headerParts, {"min_y"});
    const int minZCol = FindColumnIndex(headerParts, {"min_z"});
    const int maxXCol = FindColumnIndex(headerParts, {"max_x"});
    const int maxYCol = FindColumnIndex(headerParts, {"max_y"});
    const int maxZCol = FindColumnIndex(headerParts, {"max_z"});
    const int paramCol = FindColumnIndex(headerParts, {"param", "payload"});
    if (idCol < 0 || eventCol < 0 || minXCol < 0 || minYCol < 0 || minZCol < 0 || maxXCol < 0 || maxYCol < 0 || maxZCol < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::vector<std::string> parts = SplitCsvLine(trimmed);
        const auto need = static_cast<std::size_t>(
            std::max({idCol, eventCol, minXCol, minYCol, minZCol, maxXCol, maxYCol, maxZCol}));
        if (parts.size() <= need) {
            continue;
        }
        LevelTriggerVolume row{};
        row.id = parts[static_cast<std::size_t>(idCol)];
        row.eventType = parts[static_cast<std::size_t>(eventCol)];
        row.bounds.min[0] = ParseFloatLoose(parts[static_cast<std::size_t>(minXCol)], 0.0f);
        row.bounds.min[1] = ParseFloatLoose(parts[static_cast<std::size_t>(minYCol)], 0.0f);
        row.bounds.min[2] = ParseFloatLoose(parts[static_cast<std::size_t>(minZCol)], 0.0f);
        row.bounds.max[0] = ParseFloatLoose(parts[static_cast<std::size_t>(maxXCol)], 0.0f);
        row.bounds.max[1] = ParseFloatLoose(parts[static_cast<std::size_t>(maxYCol)], 0.0f);
        row.bounds.max[2] = ParseFloatLoose(parts[static_cast<std::size_t>(maxZCol)], 0.0f);
        if (paramCol >= 0 && paramCol < static_cast<int>(parts.size())) {
            row.param = parts[static_cast<std::size_t>(paramCol)];
        }
        if (!row.id.empty()) {
            out.levelTriggers.push_back(std::move(row));
        }
    }
}

void LoadOcclusionCsv(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    headerLine = StripLeadingCsvHeaderComment(headerLine);
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    const int idCol = FindColumnIndex(headerParts, {"volume_id", "id"});
    const int groupCol = FindColumnIndex(headerParts, {"cull_group", "group"});
    const int minXCol = FindColumnIndex(headerParts, {"min_x"});
    const int minYCol = FindColumnIndex(headerParts, {"min_y"});
    const int minZCol = FindColumnIndex(headerParts, {"min_z"});
    const int maxXCol = FindColumnIndex(headerParts, {"max_x"});
    const int maxYCol = FindColumnIndex(headerParts, {"max_y"});
    const int maxZCol = FindColumnIndex(headerParts, {"max_z"});
    if (idCol < 0 || groupCol < 0 || minXCol < 0 || minYCol < 0 || minZCol < 0 || maxXCol < 0 || maxYCol < 0 || maxZCol < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::vector<std::string> parts = SplitCsvLine(trimmed);
        const auto need = static_cast<std::size_t>(
            std::max({idCol, groupCol, minXCol, minYCol, minZCol, maxXCol, maxYCol, maxZCol}));
        if (parts.size() <= need) {
            continue;
        }
        OcclusionVolume row{};
        row.id = parts[static_cast<std::size_t>(idCol)];
        row.cullGroup = parts[static_cast<std::size_t>(groupCol)];
        row.bounds.min[0] = ParseFloatLoose(parts[static_cast<std::size_t>(minXCol)], 0.0f);
        row.bounds.min[1] = ParseFloatLoose(parts[static_cast<std::size_t>(minYCol)], 0.0f);
        row.bounds.min[2] = ParseFloatLoose(parts[static_cast<std::size_t>(minZCol)], 0.0f);
        row.bounds.max[0] = ParseFloatLoose(parts[static_cast<std::size_t>(maxXCol)], 0.0f);
        row.bounds.max[1] = ParseFloatLoose(parts[static_cast<std::size_t>(maxYCol)], 0.0f);
        row.bounds.max[2] = ParseFloatLoose(parts[static_cast<std::size_t>(maxZCol)], 0.0f);
        if (!row.id.empty()) {
            out.occlusionVolumes.push_back(std::move(row));
        }
    }
}

void LoadAudioZonesCsv(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    headerLine = StripLeadingCsvHeaderComment(headerLine);
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    const int idCol = FindColumnIndex(headerParts, {"zone_id", "audio_zone_id", "id"});
    const int reverbCol = FindColumnIndex(headerParts, {"reverb", "reverb_preset", "reverb_node"});
    const int dampeningCol = FindColumnIndex(headerParts, {"occlusion_dampening", "dampening", "occlusion"});
    const int minXCol = FindColumnIndex(headerParts, {"min_x"});
    const int minYCol = FindColumnIndex(headerParts, {"min_y"});
    const int minZCol = FindColumnIndex(headerParts, {"min_z"});
    const int maxXCol = FindColumnIndex(headerParts, {"max_x"});
    const int maxYCol = FindColumnIndex(headerParts, {"max_y"});
    const int maxZCol = FindColumnIndex(headerParts, {"max_z"});
    if (idCol < 0 || reverbCol < 0 || dampeningCol < 0 || minXCol < 0 || minYCol < 0 || minZCol < 0 || maxXCol < 0
        || maxYCol < 0 || maxZCol < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::vector<std::string> parts = SplitCsvLine(trimmed);
        const auto need = static_cast<std::size_t>(std::max(
            {idCol, reverbCol, dampeningCol, minXCol, minYCol, minZCol, maxXCol, maxYCol, maxZCol}));
        if (parts.size() <= need) {
            continue;
        }
        AudioZoneRow row{};
        row.id = parts[static_cast<std::size_t>(idCol)];
        row.reverbPreset = parts[static_cast<std::size_t>(reverbCol)];
        row.occlusionDampening = ParseFloatLoose(parts[static_cast<std::size_t>(dampeningCol)], 0.0f);
        row.bounds.min[0] = ParseFloatLoose(parts[static_cast<std::size_t>(minXCol)], 0.0f);
        row.bounds.min[1] = ParseFloatLoose(parts[static_cast<std::size_t>(minYCol)], 0.0f);
        row.bounds.min[2] = ParseFloatLoose(parts[static_cast<std::size_t>(minZCol)], 0.0f);
        row.bounds.max[0] = ParseFloatLoose(parts[static_cast<std::size_t>(maxXCol)], 0.0f);
        row.bounds.max[1] = ParseFloatLoose(parts[static_cast<std::size_t>(maxYCol)], 0.0f);
        row.bounds.max[2] = ParseFloatLoose(parts[static_cast<std::size_t>(maxZCol)], 0.0f);
        if (!row.id.empty()) {
            out.audioZones.push_back(std::move(row));
        }
    }
}

void LoadLodRangesCsv(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    headerLine = StripLeadingCsvHeaderComment(headerLine);
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    const int idCol = FindColumnIndex(headerParts, {"lod_id", "id"});
    const int nearCol = FindColumnIndex(headerParts, {"near_distance_m", "near_m", "distance_near"});
    const int farCol = FindColumnIndex(headerParts, {"far_distance_m", "far_m", "distance_far"});
    const int minScaleCol = FindColumnIndex(headerParts, {"min_scale"});
    const int maxScaleCol = FindColumnIndex(headerParts, {"max_scale"});
    if (idCol < 0 || nearCol < 0 || farCol < 0 || minScaleCol < 0 || maxScaleCol < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::vector<std::string> parts = SplitCsvLine(trimmed);
        const auto need = static_cast<std::size_t>(std::max({idCol, nearCol, farCol, minScaleCol, maxScaleCol}));
        if (parts.size() <= need) {
            continue;
        }
        LodRangeRow row{};
        row.id = parts[static_cast<std::size_t>(idCol)];
        row.nearDistanceMeters = ParseFloatLoose(parts[static_cast<std::size_t>(nearCol)], 0.0f);
        row.farDistanceMeters = ParseFloatLoose(parts[static_cast<std::size_t>(farCol)], 0.0f);
        row.minScale = ParseFloatLoose(parts[static_cast<std::size_t>(minScaleCol)], 1.0f);
        row.maxScale = ParseFloatLoose(parts[static_cast<std::size_t>(maxScaleCol)], 1.0f);
        if (!row.id.empty()) {
            out.lodRanges.push_back(std::move(row));
        }
    }
}

void LoadMaterialsManifestCsv(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    headerLine = StripLeadingCsvHeaderComment(headerLine);
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    const int idCol = FindColumnIndex(headerParts, {"material_id", "id"});
    const int bindingCol = FindColumnIndex(headerParts, {"binding", "shader_binding"});
    const int sourceCol = FindColumnIndex(headerParts, {"source", "path"});
    if (idCol < 0 || bindingCol < 0 || sourceCol < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::vector<std::string> parts = SplitCsvLine(trimmed);
        if (static_cast<int>(parts.size()) <= std::max({idCol, bindingCol, sourceCol})) {
            continue;
        }
        const std::string& id = parts[static_cast<std::size_t>(idCol)];
        if (id.empty()) {
            continue;
        }
        MaterialManifestEntry entry{};
        entry.binding = parts[static_cast<std::size_t>(bindingCol)];
        entry.source = parts[static_cast<std::size_t>(sourceCol)];
        out.materialsById[id] = std::move(entry);
    }
}

void LoadAudioBanksCsv(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    headerLine = StripLeadingCsvHeaderComment(headerLine);
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    const int idCol = FindColumnIndex(headerParts, {"bank_id", "id"});
    const int pathCol = FindColumnIndex(headerParts, {"path"});
    if (idCol < 0 || pathCol < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::vector<std::string> parts = SplitCsvLine(trimmed);
        if (static_cast<int>(parts.size()) <= std::max(idCol, pathCol)) {
            continue;
        }
        const std::string& id = parts[static_cast<std::size_t>(idCol)];
        const std::string& bankPath = parts[static_cast<std::size_t>(pathCol)];
        if (!id.empty() && !bankPath.empty()) {
            out.audioBankPathById[id] = bankPath;
        }
    }
}

void LoadFontsManifestCsv(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    headerLine = StripLeadingCsvHeaderComment(headerLine);
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    const int idCol = FindColumnIndex(headerParts, {"font_id", "id"});
    const int weightCol = FindColumnIndex(headerParts, {"weight"});
    const int pathCol = FindColumnIndex(headerParts, {"path"});
    if (idCol < 0 || weightCol < 0 || pathCol < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::vector<std::string> parts = SplitCsvLine(trimmed);
        if (static_cast<int>(parts.size()) <= std::max({idCol, weightCol, pathCol})) {
            continue;
        }
        const std::string& fontId = parts[static_cast<std::size_t>(idCol)];
        const std::string& weight = parts[static_cast<std::size_t>(weightCol)];
        const std::string& fontPath = parts[static_cast<std::size_t>(pathCol)];
        if (fontId.empty() || fontPath.empty()) {
            continue;
        }
        const std::string key = NormalizeKey(fontId) + "|" + NormalizeKey(weight);
        out.fontPathByFontKey[key] = fontPath;
    }
}

void LoadSaveSchemaJson(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    const std::string text = ri::core::detail::ReadTextFile(path);
    if (text.empty()) {
        return;
    }
    out.saveSchemaVersion = ri::core::detail::ExtractJsonInt(text, "schemaVersion");
}

void LoadAchievementsRegistryJson(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    const std::string text = ri::core::detail::ReadTextFile(path);
    if (text.empty()) {
        return;
    }
    const std::vector<std::string_view> rows = ri::core::detail::SplitJsonArrayObjects(text, "achievements");
    static constexpr std::array<std::string_view, 4> kPlatformKeys = {"steam", "xbox", "psn", "generic"};
    for (const std::string_view objectText : rows) {
        const std::string id = ri::core::detail::ExtractJsonString(objectText, "id").value_or("");
        if (id.empty()) {
            continue;
        }
        auto& platMap = out.achievementIdsByPlatform[id];
        for (const std::string_view platformKey : kPlatformKeys) {
            if (const std::optional<std::string> external = ri::core::detail::ExtractJsonString(objectText, platformKey);
                external.has_value() && !external->empty()) {
                platMap[std::string(platformKey)] = *external;
            }
        }
    }
}

void LoadSquadTacticsCsv(const std::filesystem::path& path, GameRuntimeSupportData& out) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return;
    }
    std::string headerLine;
    if (!std::getline(stream, headerLine)) {
        return;
    }
    headerLine = StripLeadingCsvHeaderComment(headerLine);
    const std::vector<std::string> headerParts = SplitCsvLine(headerLine);
    const int squadCol = FindColumnIndex(headerParts, {"squad_id", "id"});
    const int formationCol = FindColumnIndex(headerParts, {"formation"});
    const int spacingCol = FindColumnIndex(headerParts, {"spacing_m", "spacing"});
    if (squadCol < 0 || formationCol < 0 || spacingCol < 0) {
        return;
    }

    std::string line;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }
        const std::vector<std::string> parts = SplitCsvLine(trimmed);
        if (static_cast<int>(parts.size()) <= std::max({squadCol, formationCol, spacingCol})) {
            continue;
        }
        SquadTacticRow row{};
        row.squadId = parts[static_cast<std::size_t>(squadCol)];
        row.formation = parts[static_cast<std::size_t>(formationCol)];
        row.spacingMeters = ParseFloatLoose(parts[static_cast<std::size_t>(spacingCol)], 0.0f);
        if (!row.squadId.empty()) {
            out.squadTactics.push_back(std::move(row));
        }
    }
}

} // namespace

GameRuntimeSupportData LoadGameRuntimeSupportData(const std::filesystem::path& gameRoot) {
    GameRuntimeSupportData out{};
    LoadStreamingManifest(gameRoot / "assets" / "streaming.manifest", out);
    LoadDependencies(gameRoot / "assets" / "dependencies.json", out);
    LoadZones(gameRoot / "levels" / "assembly.zones.csv", out);
    LoadLookupIndex(gameRoot / "data" / "lookup.index", out);
    LoadLevelTriggersCsv(gameRoot / "levels" / "assembly.triggers.csv", out);
    LoadOcclusionCsv(gameRoot / "levels" / "assembly.occlusion.csv", out);
    LoadAudioZonesCsv(gameRoot / "levels" / "assembly.audio.zones", out);
    LoadLodRangesCsv(gameRoot / "levels" / "assembly.lods.csv", out);
    LoadMaterialsManifestCsv(gameRoot / "assets" / "materials.manifest", out);
    LoadAudioBanksCsv(gameRoot / "assets" / "audio.banks", out);
    LoadFontsManifestCsv(gameRoot / "assets" / "fonts.manifest", out);
    LoadSaveSchemaJson(gameRoot / "data" / "save.schema", out);
    LoadAchievementsRegistryJson(gameRoot / "data" / "achievements.registry", out);
    LoadKeyValueScalars(gameRoot / "ai" / "perception.cfg", out.perceptionScalars);
    LoadSquadTacticsCsv(gameRoot / "ai" / "squad.tactics", out);
    return out;
}

int ComputeResourcePriorityScore(const GameRuntimeSupportData& supportData, std::string_view relativeResourcePath) {
    const std::string normalized = NormalizeKey(std::string(relativeResourcePath));
    int score = 0;

    if (const auto it = supportData.streamingPrioritiesByPath.find(normalized);
        it != supportData.streamingPrioritiesByPath.end()) {
        score += it->second * 1000;
    }
    if (const auto it = supportData.dependencyInboundCountByPath.find(normalized);
        it != supportData.dependencyInboundCountByPath.end()) {
        score += it->second * 10;
    }

    constexpr std::string_view kLevelsPrefix = "levels/";
    if (normalized.rfind(kLevelsPrefix, 0) == 0U) {
        std::string levelId = std::string(normalized.substr(kLevelsPrefix.size()));
        const std::size_t slash = levelId.find('/');
        if (slash != std::string::npos) {
            levelId = levelId.substr(0, slash);
        }
        const std::size_t dot = levelId.find('.');
        if (dot != std::string::npos) {
            levelId = levelId.substr(0, dot);
        }
        levelId = NormalizeKey(levelId);
        if (const auto it = supportData.zonePriorityByLevelId.find(levelId);
            it != supportData.zonePriorityByLevelId.end()) {
            score += it->second * 100;
        }
    }

    return score;
}

std::string ResolveLookupValueOr(std::string_view key,
                                 std::string_view fallback,
                                 const GameRuntimeSupportData& supportData) {
    const auto it = supportData.lookupIndex.find(NormalizeKey(std::string(key)));
    if (it == supportData.lookupIndex.end() || it->second.empty()) {
        return std::string(fallback);
    }
    return it->second;
}

bool PointInsideAxisAlignedBounds(float x, float y, float z, const AxisAlignedBounds& bounds) {
    return x >= bounds.min[0] && x <= bounds.max[0] && y >= bounds.min[1] && y <= bounds.max[1] && z >= bounds.min[2]
        && z <= bounds.max[2];
}

std::optional<std::string> ResolveAchievementExternalId(std::string_view achievementId,
                                                       std::string_view platformKey,
                                                       const GameRuntimeSupportData& data) {
    const auto outer = data.achievementIdsByPlatform.find(std::string(achievementId));
    if (outer == data.achievementIdsByPlatform.end()) {
        return std::nullopt;
    }
    const std::string plat(NormalizeKey(std::string(platformKey)));
    const auto inner = outer->second.find(plat);
    if (inner == outer->second.end()) {
        return std::nullopt;
    }
    return inner->second;
}

std::optional<std::string_view> TryGetPerceptionScalar(std::string_view key,
                                                       const GameRuntimeSupportData& data) {
    const auto it = data.perceptionScalars.find(NormalizeKey(std::string(key)));
    if (it == data.perceptionScalars.end()) {
        return std::nullopt;
    }
    return std::string_view(it->second);
}

const MaterialManifestEntry* TryGetMaterialEntry(std::string_view materialId,
                                                 const GameRuntimeSupportData& data) {
    const auto it = data.materialsById.find(std::string(materialId));
    if (it == data.materialsById.end()) {
        return nullptr;
    }
    return &it->second;
}

std::optional<std::string_view> TryGetAudioBankPath(std::string_view bankId,
                                                    const GameRuntimeSupportData& data) {
    const auto it = data.audioBankPathById.find(std::string(bankId));
    if (it == data.audioBankPathById.end()) {
        return std::nullopt;
    }
    return std::string_view(it->second);
}

std::optional<std::string_view> TryGetFontPath(std::string_view fontId,
                                              std::string_view weight,
                                              const GameRuntimeSupportData& data) {
    const std::string key = NormalizeKey(std::string(fontId)) + "|" + NormalizeKey(std::string(weight));
    const auto it = data.fontPathByFontKey.find(key);
    if (it == data.fontPathByFontKey.end()) {
        return std::nullopt;
    }
    return std::string_view(it->second);
}

const AudioZoneRow* FindAudioZoneAtPoint(float x,
                                         float y,
                                         float z,
                                         const GameRuntimeSupportData& data) {
    const AudioZoneRow* best = nullptr;
    float bestVolume = 0.0f;
    for (const AudioZoneRow& row : data.audioZones) {
        if (!PointInsideAxisAlignedBounds(x, y, z, row.bounds)) {
            continue;
        }
        const float sizeX = std::max(0.0f, row.bounds.max[0] - row.bounds.min[0]);
        const float sizeY = std::max(0.0f, row.bounds.max[1] - row.bounds.min[1]);
        const float sizeZ = std::max(0.0f, row.bounds.max[2] - row.bounds.min[2]);
        const float volume = sizeX * sizeY * sizeZ;
        if (best == nullptr || volume < bestVolume) {
            best = &row;
            bestVolume = volume;
        }
    }
    return best;
}

float ComputeLodScaleForDistance(std::string_view lodId,
                                 float distanceMeters,
                                 const GameRuntimeSupportData& data) {
    if (lodId.empty()) {
        return 1.0f;
    }
    for (const LodRangeRow& row : data.lodRanges) {
        if (row.id != lodId) {
            continue;
        }
        const float nearD = std::min(row.nearDistanceMeters, row.farDistanceMeters);
        const float farD = std::max(row.nearDistanceMeters, row.farDistanceMeters);
        const float minScale = std::min(row.minScale, row.maxScale);
        const float maxScale = std::max(row.minScale, row.maxScale);
        if (farD <= nearD) {
            return std::clamp(maxScale, 0.01f, 16.0f);
        }
        const float t = std::clamp((distanceMeters - nearD) / (farD - nearD), 0.0f, 1.0f);
        const float scale = maxScale + (minScale - maxScale) * t;
        return std::clamp(scale, 0.01f, 16.0f);
    }
    return 1.0f;
}

std::vector<std::string> ExpandPreloadLevelPayloads(std::span<const std::string_view> levelIds,
                                                    const GameRuntimeSupportData& data) {
    std::vector<std::string> payloads;
    payloads.reserve(levelIds.size());
    for (const std::string_view rawLevelId : levelIds) {
        const std::string levelId = NormalizeKey(std::string(rawLevelId));
        if (levelId.empty()) {
            continue;
        }
        const std::string key = "levels." + levelId;
        payloads.push_back(ResolveLookupValueOr(key, "levels/" + levelId + ".primitives.csv", data));
    }
    return payloads;
}

LevelPreloadPlan BuildLevelPreloadPlan(std::span<const std::string_view> levelIds,
                                       const GameRuntimeSupportData& data) {
    LevelPreloadPlan plan{};
    const std::vector<std::string> payloads = ExpandPreloadLevelPayloads(levelIds, data);
    plan.entries.reserve(payloads.size());
    for (const std::string& payload : payloads) {
        const std::string normalized = NormalizeKey(payload);
        const int score = ComputeResourcePriorityScore(data, normalized);
        plan.entries.push_back(LevelPreloadEntry{
            .payloadPath = payload,
            .priorityScore = score,
        });
        plan.pendingPayloads.insert(normalized);
        plan.cachedPriorityScores[normalized] = score;
    }

    std::sort(plan.entries.begin(), plan.entries.end(), [](const LevelPreloadEntry& lhs, const LevelPreloadEntry& rhs) {
        if (lhs.priorityScore != rhs.priorityScore) {
            return lhs.priorityScore > rhs.priorityScore;
        }
        return lhs.payloadPath < rhs.payloadPath;
    });
    return plan;
}

void MarkPreloadPayloadReady(LevelPreloadPlan& plan, std::string_view payloadPath) {
    if (payloadPath.empty()) {
        return;
    }
    const std::string normalized = NormalizeKey(std::string(payloadPath));
    plan.pendingPayloads.erase(normalized);
}

} // namespace ri::content
