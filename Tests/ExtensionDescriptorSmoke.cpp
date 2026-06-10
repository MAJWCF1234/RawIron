#include "RawIron/Content/ExtensionDescriptor.h"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

bool Expect(bool condition) {
    return condition;
}

} // namespace

int main() {
    const std::string text =
        "{\n"
        "  \"formatVersion\": 1,\n"
        "  \"id\": \"rawiron.blender.pipe\",\n"
        "  \"displayName\": \"Blender Pipe\",\n"
        "  \"version\": \"1.2.0\",\n"
        "  \"kind\": \"pipe\",\n"
        "  \"scope\": \"editor\",\n"
        "  \"host\": \"external\",\n"
        "  \"entry\": \"tools/blender-bridge.exe\",\n"
        "  \"description\": \"Routes import jobs into Blender.\",\n"
        "  \"capabilities\": [\"editor.import_export\", \"external.rpc\"],\n"
        "  \"tags\": [\"blender\", \"pipeline\"]\n"
        "}\n";

    const std::optional<ri::content::ExtensionDescriptor> parsed = ri::content::ParseExtensionDescriptor(text);
    if (!Expect(parsed.has_value())) {
        return EXIT_FAILURE;
    }
    if (!Expect(parsed->kind == ri::content::ExtensionKind::Pipe)) {
        return EXIT_FAILURE;
    }
    if (!Expect(parsed->scope == ri::content::ExtensionScope::Editor)) {
        return EXIT_FAILURE;
    }
    if (!Expect(parsed->host == ri::content::ExtensionHost::External)) {
        return EXIT_FAILURE;
    }
    if (!Expect(parsed->capabilities.size() == 2U)) {
        return EXIT_FAILURE;
    }

    const ri::content::ExtensionValidationReport valid = ri::content::ValidateExtensionDescriptor(*parsed);
    if (!Expect(valid.valid) || !Expect(valid.issues.empty())) {
        return EXIT_FAILURE;
    }

    const std::string invalidText =
        "{\n"
        "  \"id\": \"\",\n"
        "  \"displayName\": \"Broken\",\n"
        "  \"version\": \"1\",\n"
        "  \"kind\": \"plugin\",\n"
        "  \"scope\": \"shared\",\n"
        "  \"host\": \"runtime\",\n"
        "  \"entry\": \"\",\n"
        "  \"capabilities\": [\"render.pipeline\", \"render.pipeline\", \"\"],\n"
        "  \"tags\": []\n"
        "}\n";

    const std::optional<ri::content::ExtensionDescriptor> invalidParsed =
        ri::content::ParseExtensionDescriptor(invalidText);
    if (!Expect(invalidParsed.has_value())) {
        return EXIT_FAILURE;
    }
    const ri::content::ExtensionValidationReport invalid =
        ri::content::ValidateExtensionDescriptor(*invalidParsed);
    if (!Expect(!invalid.valid)) {
        return EXIT_FAILURE;
    }

    bool sawEmptyId = false;
    bool sawVersion = false;
    bool sawEntry = false;
    bool sawDuplicateCapability = false;
    bool sawEmptyCapability = false;
    for (const std::string& issue : invalid.issues) {
        sawEmptyId = sawEmptyId || issue.find("id must be non-empty") != std::string::npos;
        sawVersion = sawVersion || issue.find("version must use semantic triplet format") != std::string::npos;
        sawEntry = sawEntry || issue.find("entry must be non-empty") != std::string::npos;
        sawDuplicateCapability = sawDuplicateCapability || issue.find("capabilities cannot contain duplicates") != std::string::npos;
        sawEmptyCapability = sawEmptyCapability || issue.find("capabilities cannot contain empty strings") != std::string::npos;
    }

    if (!Expect(sawEmptyId) || !Expect(sawVersion) || !Expect(sawEntry)
        || !Expect(sawDuplicateCapability) || !Expect(sawEmptyCapability)) {
        return EXIT_FAILURE;
    }

    const std::string wrapper =
        "{\n"
        "  \"id\": \"rawiron.quest-beacons\",\n"
        "  \"name\": \"Quest Beacons\",\n"
        "  \"extension\": {\n"
        "    \"id\": \"rawiron.quest-beacons\",\n"
        "    \"displayName\": \"Quest Beacons\",\n"
        "    \"version\": \"1.0.0\",\n"
        "    \"kind\": \"plugin\",\n"
        "    \"scope\": \"game\",\n"
        "    \"host\": \"runtime\",\n"
        "    \"entry\": \"plugins/hooks.riplugin\",\n"
        "    \"capabilities\": [\"gameplay.events\"],\n"
        "    \"tags\": [\"gameplay\"]\n"
        "  }\n"
        "}\n";
    const std::optional<ri::content::ExtensionDescriptor> extracted =
        ri::content::ExtractExtensionDescriptor(wrapper);
    if (!Expect(extracted.has_value())) {
        return EXIT_FAILURE;
    }
    if (!Expect(extracted->kind == ri::content::ExtensionKind::Plugin)
        || !Expect(extracted->scope == ri::content::ExtensionScope::Game)
        || !Expect(extracted->host == ri::content::ExtensionHost::Runtime)
        || !Expect(extracted->entry == "plugins/hooks.riplugin")) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
