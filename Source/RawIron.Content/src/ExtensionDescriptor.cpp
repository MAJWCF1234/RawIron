#include "RawIron/Content/ExtensionDescriptor.h"

#include "RawIron/Core/Detail/JsonScan.h"

#include <iomanip>
#include <set>
#include <sstream>

namespace ri::content {
namespace {

namespace detail_scan = ri::core::detail;

[[nodiscard]] bool IsSemanticVersionTriplet(std::string_view value) {
    int componentCount = 0;
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const std::size_t nextDot = value.find('.', cursor);
        const std::size_t end = nextDot == std::string_view::npos ? value.size() : nextDot;
        if (end == cursor) {
            return false;
        }
        for (std::size_t index = cursor; index < end; ++index) {
            if (value[index] < '0' || value[index] > '9') {
                return false;
            }
        }
        ++componentCount;
        if (nextDot == std::string_view::npos) {
            break;
        }
        cursor = nextDot + 1U;
    }
    return componentCount == 3;
}

void WriteStringArray(std::ostringstream& json, const char* key, const std::vector<std::string>& values) {
    json << "  \"" << key << "\": [";
    for (std::size_t index = 0; index < values.size(); ++index) {
        json << "\"" << detail_scan::EscapeJsonString(values[index]) << "\"";
        if (index + 1U < values.size()) {
            json << ", ";
        }
    }
    json << "]";
}

} // namespace

std::optional<ExtensionKind> ParseExtensionKind(const std::string_view value) {
    if (value == "mod") {
        return ExtensionKind::Mod;
    }
    if (value == "plugin") {
        return ExtensionKind::Plugin;
    }
    if (value == "data-pack") {
        return ExtensionKind::DataPack;
    }
    if (value == "pipe") {
        return ExtensionKind::Pipe;
    }
    return std::nullopt;
}

std::optional<ExtensionScope> ParseExtensionScope(const std::string_view value) {
    if (value == "game") {
        return ExtensionScope::Game;
    }
    if (value == "editor") {
        return ExtensionScope::Editor;
    }
    if (value == "engine") {
        return ExtensionScope::Engine;
    }
    if (value == "shared") {
        return ExtensionScope::Shared;
    }
    return std::nullopt;
}

std::optional<ExtensionHost> ParseExtensionHost(const std::string_view value) {
    if (value == "runtime") {
        return ExtensionHost::Runtime;
    }
    if (value == "editor") {
        return ExtensionHost::Editor;
    }
    if (value == "external") {
        return ExtensionHost::External;
    }
    if (value == "hybrid") {
        return ExtensionHost::Hybrid;
    }
    return std::nullopt;
}

std::string_view ToString(const ExtensionKind value) noexcept {
    switch (value) {
    case ExtensionKind::Mod:
        return "mod";
    case ExtensionKind::Plugin:
        return "plugin";
    case ExtensionKind::DataPack:
        return "data-pack";
    case ExtensionKind::Pipe:
        return "pipe";
    }
    return "plugin";
}

std::string_view ToString(const ExtensionScope value) noexcept {
    switch (value) {
    case ExtensionScope::Game:
        return "game";
    case ExtensionScope::Editor:
        return "editor";
    case ExtensionScope::Engine:
        return "engine";
    case ExtensionScope::Shared:
        return "shared";
    }
    return "shared";
}

std::string_view ToString(const ExtensionHost value) noexcept {
    switch (value) {
    case ExtensionHost::Runtime:
        return "runtime";
    case ExtensionHost::Editor:
        return "editor";
    case ExtensionHost::External:
        return "external";
    case ExtensionHost::Hybrid:
        return "hybrid";
    }
    return "runtime";
}

std::string SerializeExtensionDescriptor(const ExtensionDescriptor& descriptor) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"formatVersion\": " << descriptor.formatVersion << ",\n";
    json << "  \"id\": \"" << detail_scan::EscapeJsonString(descriptor.id) << "\",\n";
    json << "  \"displayName\": \"" << detail_scan::EscapeJsonString(descriptor.displayName) << "\",\n";
    json << "  \"version\": \"" << detail_scan::EscapeJsonString(descriptor.version) << "\",\n";
    json << "  \"kind\": \"" << ToString(descriptor.kind) << "\",\n";
    json << "  \"scope\": \"" << ToString(descriptor.scope) << "\",\n";
    json << "  \"host\": \"" << ToString(descriptor.host) << "\",\n";
    json << "  \"entry\": \"" << detail_scan::EscapeJsonString(descriptor.entry) << "\",\n";
    json << "  \"description\": \"" << detail_scan::EscapeJsonString(descriptor.description) << "\",\n";
    WriteStringArray(json, "capabilities", descriptor.capabilities);
    json << ",\n";
    WriteStringArray(json, "tags", descriptor.tags);
    json << "\n}\n";
    return json.str();
}

std::optional<ExtensionDescriptor> ParseExtensionDescriptor(const std::string_view jsonText) {
    ExtensionDescriptor descriptor{};
    descriptor.formatVersion =
        detail_scan::ExtractJsonInt(jsonText, "formatVersion").value_or(ExtensionDescriptor::kFormatVersion);
    descriptor.id = detail_scan::ExtractJsonString(jsonText, "id").value_or("");
    descriptor.displayName = detail_scan::ExtractJsonString(jsonText, "displayName").value_or("");
    descriptor.version = detail_scan::ExtractJsonString(jsonText, "version").value_or("0.1.0");
    descriptor.entry = detail_scan::ExtractJsonString(jsonText, "entry").value_or("");
    descriptor.description = detail_scan::ExtractJsonString(jsonText, "description").value_or("");
    descriptor.capabilities = detail_scan::ExtractJsonStringArray(jsonText, "capabilities");
    descriptor.tags = detail_scan::ExtractJsonStringArray(jsonText, "tags");

    if (const std::optional<std::string> kind = detail_scan::ExtractJsonString(jsonText, "kind"); kind.has_value()) {
        descriptor.kind = ParseExtensionKind(*kind).value_or(ExtensionKind::Plugin);
    }
    if (const std::optional<std::string> scope = detail_scan::ExtractJsonString(jsonText, "scope"); scope.has_value()) {
        descriptor.scope = ParseExtensionScope(*scope).value_or(ExtensionScope::Shared);
    }
    if (const std::optional<std::string> host = detail_scan::ExtractJsonString(jsonText, "host"); host.has_value()) {
        descriptor.host = ParseExtensionHost(*host).value_or(ExtensionHost::Runtime);
    }

    if (descriptor.displayName.empty()) {
        descriptor.displayName = descriptor.id;
    }
    if (descriptor.id.empty() && descriptor.displayName.empty()) {
        return std::nullopt;
    }
    return descriptor;
}

std::optional<ExtensionDescriptor> ExtractExtensionDescriptor(const std::string_view jsonText, const std::string_view key) {
    const std::optional<std::string_view> object = detail_scan::ExtractJsonObject(jsonText, key);
    if (!object.has_value()) {
        return std::nullopt;
    }
    return ParseExtensionDescriptor(*object);
}

ExtensionValidationReport ValidateExtensionDescriptor(const ExtensionDescriptor& descriptor) {
    ExtensionValidationReport report{};
    if (descriptor.formatVersion != ExtensionDescriptor::kFormatVersion) {
        report.issues.push_back("extension formatVersion is unsupported.");
    }
    if (descriptor.id.empty()) {
        report.issues.push_back("extension id must be non-empty.");
    }
    if (descriptor.displayName.empty()) {
        report.issues.push_back("extension displayName must be non-empty.");
    }
    if (descriptor.version.empty()) {
        report.issues.push_back("extension version must be non-empty.");
    } else if (!IsSemanticVersionTriplet(descriptor.version)) {
        report.issues.push_back("extension version must use semantic triplet format (e.g. \"1.0.0\").");
    }
    if (descriptor.entry.empty()) {
        report.issues.push_back("extension entry must be non-empty.");
    }

    std::set<std::string> seenCapabilities;
    for (const std::string& capability : descriptor.capabilities) {
        if (capability.empty()) {
            report.issues.push_back("extension capabilities cannot contain empty strings.");
            continue;
        }
        if (!seenCapabilities.insert(capability).second) {
            report.issues.push_back("extension capabilities cannot contain duplicates.");
        }
    }
    std::set<std::string> seenTags;
    for (const std::string& tag : descriptor.tags) {
        if (tag.empty()) {
            report.issues.push_back("extension tags cannot contain empty strings.");
            continue;
        }
        if (!seenTags.insert(tag).second) {
            report.issues.push_back("extension tags cannot contain duplicates.");
        }
    }

    report.valid = report.issues.empty();
    return report;
}

} // namespace ri::content
