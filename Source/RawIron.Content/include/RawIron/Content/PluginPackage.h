#pragma once

#include "RawIron/Content/ExtensionDescriptor.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ri::content {

struct PluginPackageDescriptor {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string category;
    std::string description;
    std::string tagLine;
    std::string badge;
    std::string manifestLine;
    std::string hookGroup;
    int loadOrder = 100;
    std::vector<std::string> tags;
    std::vector<std::string> hooks;
    ExtensionDescriptor extension{};
};

struct PluginPackageValidationReport {
    bool valid = false;
    std::filesystem::path packageRoot{};
    PluginPackageDescriptor descriptor{};
    std::vector<std::string> issues{};
};

/// Resolves a package directory or `package.riplugin.json` path to the package root.
/// Rejects symlink/reparse directories and a directory decoy named `package.riplugin.json`.
/// Rejects symlinks and Windows reparse points for both the root and the descriptor.
[[nodiscard]] std::optional<std::filesystem::path> ResolvePluginPackageRoot(
    const std::filesystem::path& packageDirOrDescriptor);

/// Parses `package.riplugin.json` without validating control-plane files.
[[nodiscard]] std::optional<PluginPackageDescriptor> ParsePluginPackageDescriptor(
    const std::filesystem::path& packageRoot);

/// Validates a public plugin-store package folder (descriptor + plugins/ control plane).
[[nodiscard]] PluginPackageValidationReport ValidatePluginPackage(
    const std::filesystem::path& packageDirOrDescriptor);

struct PluginPackageArchivePlan {
    bool valid = false;
    std::filesystem::path packageRoot{};
    PluginPackageDescriptor descriptor{};
    /// Portable forward-slash package-relative entries included in the archive.
    std::vector<std::string> relativeEntries{};
    std::vector<std::string> issues{};
};

/// Validates a plugin package and lists the allowlisted files that may be archived.
[[nodiscard]] PluginPackageArchivePlan PlanPluginPackageArchive(
    const std::filesystem::path& packageDirOrDescriptor);

/// Copies `plan.relativeEntries` into `stagingRoot` as regular files only (no links).
[[nodiscard]] bool StagePluginPackageArchive(
    const PluginPackageArchivePlan& plan,
    const std::filesystem::path& stagingRoot,
    std::vector<std::string>& issues);

} // namespace ri::content
