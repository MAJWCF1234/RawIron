#include "RawIron/Content/PluginPackage.h"

#include "RawIron/Content/PluginProjectData.h"
#include "RawIron/Core/Detail/JsonScan.h"

#include <cctype>
#include <system_error>
#include <unordered_set>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace ri::content {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] bool IsSemanticVersionTriplet(const std::string_view value) {
    std::size_t index = 0U;
    for (int part = 0; part < 3; ++part) {
        if (index >= value.size() || !std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
        while (index < value.size() && std::isdigit(static_cast<unsigned char>(value[index]))) {
            ++index;
        }
        if (part < 2) {
            if (index >= value.size() || value[index] != '.') {
                return false;
            }
            ++index;
        }
    }
    return index == value.size();
}

[[nodiscard]] std::string Trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

[[nodiscard]] std::string FirstCsvField(std::string_view line) {
    const std::size_t comma = line.find(',');
    return Trim(std::string(comma == std::string_view::npos ? line : line.substr(0, comma)));
}

[[nodiscard]] bool IsRegularNonLinkFile(const fs::path& path, std::string& issue) {
    issue.clear();
    std::error_code error;
    const fs::file_status linkStatus = fs::symlink_status(path, error);
    if (error) {
        issue = "cannot inspect '" + path.generic_string() + "': " + error.message();
        return false;
    }
    if (fs::is_symlink(linkStatus)) {
        issue = "'" + path.generic_string() + "' is a symlink and cannot be archived.";
        return false;
    }
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        issue = "'" + path.generic_string() + "' attributes cannot be inspected.";
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
        issue = "'" + path.generic_string() + "' is a reparse point and cannot be archived.";
        return false;
    }
#endif
    if (!fs::is_regular_file(linkStatus)) {
        issue = "'" + path.generic_string() + "' is not a regular file.";
        return false;
    }
    return true;
}

[[nodiscard]] bool IsNonLinkDirectory(const fs::path& path) {
    std::error_code error;
    const fs::file_status linkStatus = fs::symlink_status(path, error);
    if (error || fs::is_symlink(linkStatus) || !fs::is_directory(linkStatus)) {
        return false;
    }
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES
        || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
        || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U) {
        return false;
    }
#endif
    return true;
}

} // namespace

std::optional<fs::path> ResolvePluginPackageRoot(const fs::path& packageDirOrDescriptor) {
    if (packageDirOrDescriptor.empty()) {
        return std::nullopt;
    }
    std::string issue;
    if (packageDirOrDescriptor.filename() == "package.riplugin.json"
        && IsRegularNonLinkFile(packageDirOrDescriptor, issue)) {
        return packageDirOrDescriptor.parent_path();
    }
    if (IsNonLinkDirectory(packageDirOrDescriptor)) {
        const fs::path descriptor = packageDirOrDescriptor / "package.riplugin.json";
        if (IsRegularNonLinkFile(descriptor, issue)) {
            return packageDirOrDescriptor;
        }
    }
    return std::nullopt;
}

std::optional<PluginPackageDescriptor> ParsePluginPackageDescriptor(const fs::path& packageRoot) {
    const fs::path packageJson = packageRoot / "package.riplugin.json";
    const std::string text = ri::core::detail::ReadTextFile(packageJson);
    if (text.empty()) {
        return std::nullopt;
    }

    PluginPackageDescriptor package{};
    package.id = ri::core::detail::ExtractJsonString(text, "id").value_or("");
    package.name = ri::core::detail::ExtractJsonString(text, "name").value_or(package.id);
    package.version = ri::core::detail::ExtractJsonString(text, "version").value_or("");
    package.author = ri::core::detail::ExtractJsonString(text, "author").value_or("");
    package.category = ri::core::detail::ExtractJsonString(text, "category").value_or("utility");
    package.description = ri::core::detail::ExtractJsonString(text, "description").value_or("");
    package.tagLine = ri::core::detail::ExtractJsonString(text, "tagLine").value_or("");
    package.badge = ri::core::detail::ExtractJsonString(text, "badge").value_or("");
    package.manifestLine = ri::core::detail::ExtractJsonString(text, "manifestLine").value_or("");
    package.hookGroup = ri::core::detail::ExtractJsonString(text, "hookGroup").value_or("runtime.mod");
    package.loadOrder = static_cast<int>(ri::core::detail::ExtractJsonInt(text, "loadOrder").value_or(100));
    package.tags = ri::core::detail::ExtractJsonStringArray(text, "tags");
    package.hooks = ri::core::detail::ExtractJsonStringArray(text, "hooks");
    if (const std::optional<ExtensionDescriptor> extension = ExtractExtensionDescriptor(text)) {
        package.extension = *extension;
    } else {
        package.extension.id = package.id;
        package.extension.displayName = package.name;
        package.extension.version = package.version;
        package.extension.kind = ExtensionKind::Plugin;
        package.extension.scope = ExtensionScope::Shared;
        package.extension.host = ExtensionHost::Runtime;
        package.extension.entry = "plugins/hooks.riplugin";
        package.extension.description = package.description;
        package.extension.capabilities = package.tags;
        package.extension.tags = package.tags;
    }
    return package;
}

PluginPackageValidationReport ValidatePluginPackage(const fs::path& packageDirOrDescriptor) {
    PluginPackageValidationReport report{};
    const std::optional<fs::path> root = ResolvePluginPackageRoot(packageDirOrDescriptor);
    if (!root.has_value()) {
        report.issues.push_back(
            "plugin package root could not be resolved (expected a directory containing package.riplugin.json).");
        return report;
    }
    report.packageRoot = *root;

    const std::optional<PluginPackageDescriptor> parsed = ParsePluginPackageDescriptor(*root);
    if (!parsed.has_value()) {
        report.issues.push_back("package.riplugin.json could not be read or parsed.");
        return report;
    }
    report.descriptor = *parsed;

    if (report.descriptor.id.empty()) {
        report.issues.push_back("package id must be non-empty.");
    }
    if (report.descriptor.name.empty()) {
        report.issues.push_back("package name must be non-empty.");
    }
    if (report.descriptor.version.empty()) {
        report.issues.push_back("package version must be non-empty.");
    } else if (!IsSemanticVersionTriplet(report.descriptor.version)) {
        report.issues.push_back("package version must use semantic triplet format (e.g. \"1.0.0\").");
    }
    if (report.descriptor.manifestLine.empty()) {
        report.issues.push_back("package manifestLine must be non-empty.");
    } else if (!report.descriptor.id.empty()
               && FirstCsvField(report.descriptor.manifestLine) != report.descriptor.id) {
        report.issues.push_back("package manifestLine id does not match package id.");
    }

    const ExtensionValidationReport extensionReport = ValidateExtensionDescriptor(report.descriptor.extension);
    if (!extensionReport.valid) {
        for (const std::string& issue : extensionReport.issues) {
            report.issues.push_back("extension: " + issue);
        }
    } else if (!report.descriptor.id.empty() && report.descriptor.extension.id != report.descriptor.id) {
        report.issues.push_back("extension id must match package id.");
    }

    const PluginProjectData project = LoadPluginProjectData(*root);
    for (const PluginValidationIssue& issue : project.issues) {
        report.issues.push_back("control-plane: " + issue.message);
    }

    bool foundManifestId = false;
    for (const PluginManifestEntry& entry : project.manifestEntries) {
        if (entry.id == report.descriptor.id) {
            foundManifestId = true;
            if (!entry.entryPathValid) {
                report.issues.push_back("manifest entry path is unsafe for package id '" + entry.id + "'.");
            }
            if (!entry.entryExists && !entry.entryIsRemote) {
                report.issues.push_back("manifest entry file is missing for package id '" + entry.id + "'.");
            }
        }
    }
    if (!report.descriptor.id.empty() && !foundManifestId) {
        report.issues.push_back("plugins/manifest.plugins does not declare package id '"
                                + report.descriptor.id + "'.");
    }

    bool foundRegistryId = false;
    for (const PluginRegistryEntry& entry : project.registryEntries) {
        if (entry.id == report.descriptor.id) {
            foundRegistryId = true;
            break;
        }
    }
    if (!report.descriptor.id.empty() && !foundRegistryId) {
        report.issues.push_back("plugins/registry.json does not declare package id '"
                                + report.descriptor.id + "'.");
    }

    bool foundLoadOrder = false;
    for (const ActivePlugin& active : project.activePlugins) {
        if (active.manifest.id == report.descriptor.id) {
            foundLoadOrder = true;
            break;
        }
    }
    if (!report.descriptor.id.empty() && foundManifestId && !foundLoadOrder) {
        // Active plugins require registry enable + load order; surface a clearer message.
        std::error_code existsError;
        const bool loadOrderExists = fs::is_regular_file(*root / "plugins" / "load_order.cfg", existsError);
        if (!loadOrderExists) {
            report.issues.push_back("plugins/load_order.cfg is missing.");
        } else {
            report.issues.push_back("package id '" + report.descriptor.id
                                    + "' is not active (check registry enablement and load_order.cfg).");
        }
    }

    std::unordered_set<std::string> hookPluginIds;
    for (const PluginHookBinding& binding : project.hookBindings) {
        hookPluginIds.insert(binding.pluginId);
    }
    if (!report.descriptor.id.empty() && !hookPluginIds.empty()
        && hookPluginIds.find(report.descriptor.id) == hookPluginIds.end()) {
        report.issues.push_back("plugins/hooks.riplugin does not bind package id '"
                                + report.descriptor.id + "'.");
    }

    report.valid = report.issues.empty();
    return report;
}

namespace {

[[nodiscard]] bool TryAddArchiveEntry(
    const fs::path& packageRoot,
    const std::string_view relativeEntry,
    const bool required,
    PluginPackageArchivePlan& plan) {
    const fs::path absolute = packageRoot / fs::path(relativeEntry);
    std::string issue;
    std::error_code error;
    if (!fs::exists(absolute, error) || error) {
        if (required) {
            plan.issues.push_back(
                "required archive entry '" + std::string(relativeEntry) + "' is missing.");
        }
        return false;
    }
    if (!IsRegularNonLinkFile(absolute, issue)) {
        if (required || !issue.empty()) {
            plan.issues.push_back("archive entry " + issue);
        }
        return false;
    }
    plan.relativeEntries.emplace_back(relativeEntry);
    return true;
}

} // namespace

PluginPackageArchivePlan PlanPluginPackageArchive(const fs::path& packageDirOrDescriptor) {
    PluginPackageArchivePlan plan{};
    const PluginPackageValidationReport validation = ValidatePluginPackage(packageDirOrDescriptor);
    plan.packageRoot = validation.packageRoot;
    plan.descriptor = validation.descriptor;
    plan.issues = validation.issues;
    if (!validation.valid) {
        return plan;
    }

    static constexpr std::string_view kRequiredEntries[] = {
        "package.riplugin.json",
        "plugins/manifest.plugins",
        "plugins/registry.json",
        "plugins/hooks.riplugin",
        "plugins/load_order.cfg",
    };
    static constexpr std::string_view kOptionalEntries[] = {
        "README.md",
        "LICENSE.md",
        "LICENSE",
        "LICENSE.txt",
        "icon.png",
        "scripts/plugins.riscript",
    };

    for (const std::string_view entry : kRequiredEntries) {
        (void)TryAddArchiveEntry(plan.packageRoot, entry, true, plan);
    }
    for (const std::string_view entry : kOptionalEntries) {
        (void)TryAddArchiveEntry(plan.packageRoot, entry, false, plan);
    }

    plan.valid = plan.issues.empty() && !plan.relativeEntries.empty();
    return plan;
}

bool StagePluginPackageArchive(
    const PluginPackageArchivePlan& plan,
    const fs::path& stagingRoot,
    std::vector<std::string>& issues) {
    issues.clear();
    if (!plan.valid) {
        issues.push_back("plugin package archive plan is not valid.");
        return false;
    }
    if (stagingRoot.empty()) {
        issues.push_back("plugin package staging root must be non-empty.");
        return false;
    }

    std::error_code error;
    fs::create_directories(stagingRoot, error);
    if (error) {
        issues.push_back("could not create plugin package staging root: " + error.message());
        return false;
    }

    for (const std::string& relativeEntry : plan.relativeEntries) {
        const fs::path source = plan.packageRoot / fs::path(relativeEntry);
        std::string inspectIssue;
        if (!IsRegularNonLinkFile(source, inspectIssue)) {
            issues.push_back("staging refused source " + inspectIssue);
            return false;
        }

        const fs::path destination = stagingRoot / fs::path(relativeEntry);
        error.clear();
        fs::create_directories(destination.parent_path(), error);
        if (error) {
            issues.push_back(
                "could not create staging parent for '" + relativeEntry + "': " + error.message());
            return false;
        }
        error.clear();
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
        if (error) {
            issues.push_back(
                "could not stage '" + relativeEntry + "': " + error.message());
            return false;
        }
        if (!IsRegularNonLinkFile(destination, inspectIssue)) {
            issues.push_back("staged entry became unsafe: " + inspectIssue);
            return false;
        }
    }
    return true;
}

} // namespace ri::content
