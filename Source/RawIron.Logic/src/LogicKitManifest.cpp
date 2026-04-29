#include "RawIron/Logic/LogicKitManifest.h"

#include "RawIron/Core/Detail/JsonScan.h"

namespace ri::logic {
namespace {

using ri::core::detail::ExtractJsonInt;
using ri::core::detail::ExtractJsonObject;
using ri::core::detail::ExtractJsonString;
using ri::core::detail::ExtractJsonStringArray;
using ri::core::detail::FindJsonKey;
using ri::core::detail::ParseQuotedString;
using ri::core::detail::ReadTextFile;
using ri::core::detail::SkipWhitespace;
using ri::core::detail::SplitJsonArrayObjects;

const LogicKitManifest* g_activeLogicKitManifest = nullptr;

[[nodiscard]] bool JsonValueIsNull(std::string_view text, const std::size_t index) {
    return index + 4U <= text.size() && text.compare(index, 4U, "null") == 0;
}

[[nodiscard]] std::unordered_map<std::string, std::string> ParseStringMapForKeys(std::string_view object,
                                                                                 const std::vector<std::string>& keys) {
    std::unordered_map<std::string, std::string> out;
    for (const std::string& key : keys) {
        if (const std::optional<std::string> value = ExtractJsonString(object, key)) {
            out[key] = *value;
        }
    }
    return out;
}

[[nodiscard]] std::optional<std::string> ExtractJsonStringOrNullMember(std::string_view text, std::string_view key) {
    const std::optional<std::size_t> valueIndex = FindJsonKey(text, key);
    if (!valueIndex.has_value()) {
        return std::nullopt;
    }
    std::size_t cursor = SkipWhitespace(text, *valueIndex);
    if (JsonValueIsNull(text, cursor)) {
        return std::string{};
    }
    return ParseQuotedString(text, cursor);
}

[[nodiscard]] std::vector<LogicKitPortBinding> ParsePortBindingObjects(std::string_view portSchemaObject,
                                                                      const std::string_view arrayKey) {
    std::vector<LogicKitPortBinding> bindings;
    const std::vector<std::string_view> objects = SplitJsonArrayObjects(portSchemaObject, arrayKey);
    bindings.reserve(objects.size());
    for (const std::string_view portObject : objects) {
        LogicKitPortBinding binding{};
        if (const std::optional<std::string> name = ExtractJsonString(portObject, "name")) {
            binding.name = *name;
        }
        if (const std::optional<std::string> scenePathHint = ExtractJsonString(portObject, "scenePathHint")) {
            binding.scenePathHint = *scenePathHint;
        }
        if (const std::optional<std::string> hitMeshNameHint = ExtractJsonString(portObject, "hitMeshNameHint")) {
            binding.hitMeshNameHint = *hitMeshNameHint;
        }
        if (const std::optional<std::string> portKind = ExtractJsonString(portObject, "portKind")) {
            binding.portKind = *portKind;
        }
        if (const std::optional<std::int32_t> portIndex = ExtractJsonInt(portObject, "portIndex")) {
            binding.portIndex = *portIndex;
        }
        if (!binding.name.empty()) {
            bindings.push_back(std::move(binding));
        }
    }
    return bindings;
}

[[nodiscard]] LogicKitPortSchemaSpec ParsePortSchema(std::string_view nodeObject) {
    LogicKitPortSchemaSpec spec{};
    const std::optional<std::string_view> portSchemaObject = ExtractJsonObject(nodeObject, "portSchema");
    if (!portSchemaObject.has_value()) {
        return spec;
    }
    spec.inputs = ParsePortBindingObjects(*portSchemaObject, "inputs");
    spec.outputs = ParsePortBindingObjects(*portSchemaObject, "outputs");
    return spec;
}

[[nodiscard]] std::optional<LogicKitEmbeddedScreen> ParseEmbeddedScreen(std::string_view nodeObject) {
    const std::optional<std::string_view> embeddedObject = ExtractJsonObject(nodeObject, "embeddedScreen");
    if (!embeddedObject.has_value()) {
        return std::nullopt;
    }
    LogicKitEmbeddedScreen screen{};
    if (const std::optional<std::string> profile = ExtractJsonString(*embeddedObject, "profile")) {
        screen.profile = *profile;
    }
    if (const std::optional<std::string> state = ExtractJsonString(*embeddedObject, "state")) {
        screen.state = *state;
    }
    if (const std::optional<std::int32_t> px = ExtractJsonInt(*embeddedObject, "px")) {
        screen.px = *px;
    }
    return screen;
}

[[nodiscard]] LogicKitNodeManifestEntry ParseNodeObject(const LogicKitManifest& partialManifestForKeys,
                                                        std::string_view nodeObject) {
    LogicKitNodeManifestEntry entry{};
    if (const std::optional<std::string> id = ExtractJsonString(nodeObject, "id")) {
        entry.id = *id;
    }
    if (const std::optional<std::string> glb = ExtractJsonString(nodeObject, "glb")) {
        entry.glbRelative = *glb;
    }
    if (const std::optional<std::string> color = ExtractJsonString(nodeObject, "color")) {
        entry.colorHex = *color;
    }
    entry.summaryInputs = ExtractJsonStringArray(nodeObject, "inputs");
    entry.summaryOutputs = ExtractJsonStringArray(nodeObject, "outputs");

    if (const std::optional<std::string_view> screenStatesObj = ExtractJsonObject(nodeObject, "screenStates")) {
        entry.screenStates = ParseStringMapForKeys(*screenStatesObj, partialManifestForKeys.screenStateKeys);
    }
    if (const std::optional<std::string_view> byProfile = ExtractJsonObject(nodeObject, "screenTexturesByProfile")) {
        for (const LogicKitScreenTextureProfile& profile : partialManifestForKeys.screenTextureProfiles) {
            const std::optional<std::string_view> profileObject = ExtractJsonObject(*byProfile, profile.key);
            if (!profileObject.has_value()) {
                continue;
            }
            entry.screenTexturesByProfile[profile.key] =
                ParseStringMapForKeys(*profileObject, partialManifestForKeys.screenStateKeys);
        }
    }
    if (const std::optional<std::string_view> r128 = ExtractJsonObject(nodeObject, "screenTexturesR128")) {
        entry.screenTexturesR128 = ParseStringMapForKeys(*r128, partialManifestForKeys.screenStateKeys);
    }
    if (const std::optional<std::string_view> r512 = ExtractJsonObject(nodeObject, "screenTexturesR512")) {
        entry.screenTexturesR512 = ParseStringMapForKeys(*r512, partialManifestForKeys.screenStateKeys);
    }
    if (const std::optional<std::string_view> remap = ExtractJsonObject(nodeObject, "remapPreviewIdle")) {
        entry.remapPreviewIdle = ParseStringMapForKeys(*remap, partialManifestForKeys.screenRemapPreviewModes);
    }
    entry.embeddedScreen = ParseEmbeddedScreen(nodeObject);
    if (const std::optional<std::string> defaultState = ExtractJsonString(nodeObject, "defaultScreenState")) {
        entry.defaultScreenState = *defaultState;
    }
    entry.screenMaterialIndex = ExtractJsonInt(nodeObject, "screenMaterialIndex");
    entry.portSchema = ParsePortSchema(nodeObject);
    return entry;
}

} // namespace

void SetActiveLogicKitManifest(const LogicKitManifest* manifest) {
    g_activeLogicKitManifest = manifest;
}

const LogicKitManifest* ActiveLogicKitManifest() {
    return g_activeLogicKitManifest;
}

std::filesystem::path LogicKitRootDirectory(const std::filesystem::path& manifestPath) {
    return manifestPath.parent_path();
}

std::filesystem::path ResolveLogicKitGlbPath(const std::filesystem::path& manifestPath,
                                             const std::string_view glbRelative) {
    return LogicKitRootDirectory(manifestPath) / glbRelative;
}

std::filesystem::path ResolveLogicKitAssetPath(const std::filesystem::path& manifestPath,
                                               const std::string_view relativePath) {
    return LogicKitRootDirectory(manifestPath) / relativePath;
}

LogicNodePortSchema BuildLogicNodePortSchemaFromKitEntry(const LogicKitNodeManifestEntry& entry) {
    LogicNodePortSchema schema{};
    schema.kind = entry.id;
    if (!entry.portSchema.inputs.empty() || !entry.portSchema.outputs.empty()) {
        schema.inputs.reserve(entry.portSchema.inputs.size());
        for (const LogicKitPortBinding& binding : entry.portSchema.inputs) {
            schema.inputs.push_back(LogicPortDescriptor{.name = binding.name, .carriesNumericParameter = false});
        }
        schema.outputs.reserve(entry.portSchema.outputs.size());
        for (const LogicKitPortBinding& binding : entry.portSchema.outputs) {
            schema.outputs.push_back(LogicPortDescriptor{.name = binding.name, .carriesNumericParameter = false});
        }
        return schema;
    }
    schema.inputs.reserve(entry.summaryInputs.size());
    for (const std::string& name : entry.summaryInputs) {
        schema.inputs.push_back(LogicPortDescriptor{.name = name, .carriesNumericParameter = false});
    }
    schema.outputs.reserve(entry.summaryOutputs.size());
    for (const std::string& name : entry.summaryOutputs) {
        schema.outputs.push_back(LogicPortDescriptor{.name = name, .carriesNumericParameter = false});
    }
    return schema;
}

std::optional<LogicKitManifest> LoadLogicKitManifest(const std::filesystem::path& manifestPath) {
    const std::string text = ReadTextFile(manifestPath);
    if (text.empty()) {
        return std::nullopt;
    }
    LogicKitManifest manifest{};
    if (const std::optional<std::int32_t> version = ExtractJsonInt(text, "kitVersion")) {
        manifest.kitVersion = *version;
    }
    if (const std::optional<std::string> remapMode = ExtractJsonString(text, "screenPixelRemapMode")) {
        manifest.screenPixelRemapMode = *remapMode;
    }
    manifest.screenStateKeys = ExtractJsonStringArray(text, "screenStateKeys");
    manifest.screenRemapPreviewModes = ExtractJsonStringArray(text, "screenRemapPreviewModes");

    const std::vector<std::string_view> profileObjects = SplitJsonArrayObjects(text, "screenTextureProfiles");
    manifest.screenTextureProfiles.reserve(profileObjects.size());
    for (const std::string_view profileObject : profileObjects) {
        LogicKitScreenTextureProfile profile{};
        if (const std::optional<std::string> key = ExtractJsonString(profileObject, "key")) {
            profile.key = *key;
        }
        if (const std::optional<std::int32_t> px = ExtractJsonInt(profileObject, "px")) {
            profile.px = *px;
        }
        if (const std::optional<std::string> subdirString = ExtractJsonStringOrNullMember(profileObject, "subdir")) {
            if (!subdirString->empty()) {
                profile.subdir = *subdirString;
            }
        }
        if (!profile.key.empty()) {
            manifest.screenTextureProfiles.push_back(std::move(profile));
        }
    }

    const std::vector<std::string_view> nodeObjects = SplitJsonArrayObjects(text, "nodes");
    manifest.nodes.reserve(nodeObjects.size());
    for (const std::string_view nodeObject : nodeObjects) {
        LogicKitNodeManifestEntry entry = ParseNodeObject(manifest, nodeObject);
        if (entry.id.empty() || entry.glbRelative.empty()) {
            continue;
        }
        manifest.nodes.push_back(std::move(entry));
    }
    if (manifest.nodes.empty()) {
        return std::nullopt;
    }
    return manifest;
}

std::optional<std::vector<LogicKitNodeManifestEntry>> LoadLogicKitNodeManifestEntries(
    const std::filesystem::path& manifestPath) {
    const std::optional<LogicKitManifest> manifest = LoadLogicKitManifest(manifestPath);
    if (!manifest.has_value()) {
        return std::nullopt;
    }
    return manifest->nodes;
}

const LogicKitNodeManifestEntry* FindLogicKitNodeManifestEntry(const std::vector<LogicKitNodeManifestEntry>& entries,
                                                               const std::string_view nodeId) {
    for (const LogicKitNodeManifestEntry& entry : entries) {
        if (entry.id == nodeId) {
            return &entry;
        }
    }
    return nullptr;
}

const LogicKitNodeManifestEntry* FindLogicKitNodeManifestEntry(const LogicKitManifest& manifest,
                                                               const std::string_view nodeId) {
    return FindLogicKitNodeManifestEntry(manifest.nodes, nodeId);
}

} // namespace ri::logic
