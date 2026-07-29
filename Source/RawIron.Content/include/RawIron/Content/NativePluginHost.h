#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ri::content {

struct NativePluginLoadOptions {
    /// When non-empty, the canonical plugin path must remain below this directory.
    std::filesystem::path allowedRoot;
    /// Optional host-level allowlist layered above project capability grants.
    std::vector<std::string> allowedCapabilities;
    bool enforceAllowedCapabilities = false;
};

struct NativePluginLoadResult {
    bool loaded = false;
    std::string pluginId;
    std::string pluginVersion;
    std::size_t registeredHookCount = 0;
    std::string diagnostic;
};

/// Owns versioned native plugin modules and keeps their code resident while registered hooks execute.
class NativePluginHost {
public:
    NativePluginHost();
    ~NativePluginHost();

    NativePluginHost(const NativePluginHost&) = delete;
    NativePluginHost& operator=(const NativePluginHost&) = delete;
    NativePluginHost(NativePluginHost&&) noexcept;
    NativePluginHost& operator=(NativePluginHost&&) noexcept;

    [[nodiscard]] NativePluginLoadResult Load(
        const std::filesystem::path& modulePath,
        const NativePluginLoadOptions& options = {});

    void UnloadAll();

    [[nodiscard]] std::size_t LoadedPluginCount() const noexcept;
    [[nodiscard]] std::vector<std::string> LoadedPluginIds() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ri::content
