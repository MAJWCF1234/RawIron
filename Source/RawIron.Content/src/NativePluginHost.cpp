#include "RawIron/Content/NativePluginHost.h"

#include "RawIron/Content/NativePluginAbi.h"
#include "RawIron/Content/PluginRuntime.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace ri::content {
namespace {

constexpr std::uint32_t kMaxNativePluginHooks = 1024U;
constexpr const char* kDescriptorSymbol = "RawIronPluginGetDescriptorV1";

#if defined(_WIN32)
using ModuleHandle = HMODULE;

[[nodiscard]] ModuleHandle OpenModule(const std::filesystem::path& path) {
    return LoadLibraryW(path.c_str());
}

[[nodiscard]] void* FindModuleSymbol(const ModuleHandle module, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(module, name));
}

void CloseModule(const ModuleHandle module) {
    if (module != nullptr) {
        FreeLibrary(module);
    }
}
#else
using ModuleHandle = void*;

[[nodiscard]] ModuleHandle OpenModule(const std::filesystem::path& path) {
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

[[nodiscard]] void* FindModuleSymbol(const ModuleHandle module, const char* name) {
    return dlsym(module, name);
}

void CloseModule(const ModuleHandle module) {
    if (module != nullptr) {
        dlclose(module);
    }
}
#endif

[[nodiscard]] bool IsPathWithin(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto rootIt = root.begin();
    auto candidateIt = candidate.begin();
    for (; rootIt != root.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == candidate.end() || *rootIt != *candidateIt) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool CapabilityAllowed(
    const std::vector<std::string>& allowed,
    const std::string_view capability) {
    return capability.empty()
        || std::find(allowed.begin(), allowed.end(), capability) != allowed.end();
}

struct ModuleLease {
    ModuleHandle handle = nullptr;
    RawIronNativePluginShutdownFnV1 shutdown = nullptr;

    ~ModuleLease() {
        if (shutdown != nullptr) {
            try {
                shutdown();
            } catch (...) {
                // Native modules cross a C ABI and must not throw. Keep teardown fail-safe if one does.
            }
        }
        CloseModule(handle);
    }
};

struct LoadedPlugin {
    std::string id;
    std::string version;
    std::vector<std::string> eventNames;
    std::shared_ptr<ModuleLease> lease;
};

} // namespace

struct NativePluginHost::Impl {
    std::vector<LoadedPlugin> plugins;
};

NativePluginHost::NativePluginHost()
    : impl_(std::make_unique<Impl>()) {}

NativePluginHost::~NativePluginHost() {
    UnloadAll();
}

NativePluginHost::NativePluginHost(NativePluginHost&&) noexcept = default;
NativePluginHost& NativePluginHost::operator=(NativePluginHost&&) noexcept = default;

NativePluginLoadResult NativePluginHost::Load(
    const std::filesystem::path& modulePath,
    const NativePluginLoadOptions& options) {
    NativePluginLoadResult result{};
    std::error_code error;
    const std::filesystem::path canonicalModule = std::filesystem::weakly_canonical(modulePath, error);
    if (error || !std::filesystem::is_regular_file(canonicalModule, error) || error) {
        result.diagnostic = "Native plugin module does not exist or is not a regular file.";
        return result;
    }
    if (!options.allowedRoot.empty()) {
        const std::filesystem::path canonicalRoot =
            std::filesystem::weakly_canonical(options.allowedRoot, error);
        if (error || !IsPathWithin(canonicalRoot, canonicalModule)) {
            result.diagnostic = "Native plugin path escapes the configured plugin root.";
            return result;
        }
    }

    const ModuleHandle rawHandle = OpenModule(canonicalModule);
    if (rawHandle == nullptr) {
        result.diagnostic = "Operating system loader rejected the native plugin module.";
        return result;
    }
    auto lease = std::make_shared<ModuleLease>();
    lease->handle = rawHandle;

    const auto getDescriptor = reinterpret_cast<RawIronPluginGetDescriptorV1Fn>(
        FindModuleSymbol(rawHandle, kDescriptorSymbol));
    if (getDescriptor == nullptr) {
        result.diagnostic = "Native plugin is missing RawIronPluginGetDescriptorV1.";
        return result;
    }
    const RawIronNativePluginDescriptorV1* descriptor = nullptr;
    try {
        descriptor = getDescriptor();
    } catch (...) {
        result.diagnostic = "Native plugin descriptor entry point threw an exception.";
        return result;
    }
    if (descriptor == nullptr
        || descriptor->structSize < sizeof(RawIronNativePluginDescriptorV1)
        || descriptor->abiVersion != RAWIRON_NATIVE_PLUGIN_ABI_V1
        || descriptor->pluginId == nullptr
        || descriptor->pluginId[0] == '\0') {
        result.diagnostic = "Native plugin descriptor or ABI version is invalid.";
        return result;
    }
    result.pluginId = descriptor->pluginId;
    result.pluginVersion = descriptor->pluginVersion != nullptr ? descriptor->pluginVersion : "";
    if (descriptor->hookCount > kMaxNativePluginHooks
        || (descriptor->hookCount > 0U && descriptor->hooks == nullptr)) {
        result.diagnostic = "Native plugin hook table is invalid or exceeds the safety limit.";
        return result;
    }
    if (std::any_of(
            impl_->plugins.begin(),
            impl_->plugins.end(),
            [&](const LoadedPlugin& loaded) { return loaded.id == result.pluginId; })) {
        result.diagnostic = "A native plugin with this id is already loaded.";
        return result;
    }

    std::vector<std::string> eventNames;
    eventNames.reserve(descriptor->hookCount);
    for (std::uint32_t index = 0; index < descriptor->hookCount; ++index) {
        const RawIronNativePluginHookV1& hook = descriptor->hooks[index];
        if (hook.structSize < sizeof(RawIronNativePluginHookV1)
            || hook.eventName == nullptr
            || hook.eventName[0] == '\0'
            || hook.handler == nullptr) {
            result.diagnostic = "Native plugin contains an invalid hook descriptor.";
            return result;
        }
        const std::string eventName = hook.eventName;
        if (IsPluginHookHandlerRegistered(eventName)
            || std::find(eventNames.begin(), eventNames.end(), eventName) != eventNames.end()) {
            result.diagnostic = "Native plugin hook event is already registered: " + eventName;
            return result;
        }
        const std::string_view required =
            hook.requiredCapability != nullptr ? std::string_view(hook.requiredCapability) : std::string_view{};
        if (options.enforceAllowedCapabilities
            && !CapabilityAllowed(options.allowedCapabilities, required)) {
            result.diagnostic = "Native plugin requires a host capability that is not allowed: "
                + std::string(required);
            return result;
        }
        eventNames.push_back(eventName);
    }

    if (descriptor->initialize != nullptr) {
        try {
            if (descriptor->initialize() == 0) {
                result.diagnostic = "Native plugin initialization failed.";
                return result;
            }
        } catch (...) {
            result.diagnostic = "Native plugin initialization threw an exception.";
            return result;
        }
    }
    lease->shutdown = descriptor->shutdown;

    std::size_t registeredCount = 0U;
    try {
        for (std::uint32_t index = 0; index < descriptor->hookCount; ++index) {
            const RawIronNativePluginHookV1 hook = descriptor->hooks[index];
            const std::string required =
                hook.requiredCapability != nullptr ? hook.requiredCapability : "";
            RegisterPluginHookHandler(
                eventNames[index],
                required,
                [hook, lease](PluginHookContext& context, const PluginHookInvocation& invocation) {
                    const RawIronNativePluginInvocationV1 nativeInvocation{
                        .structSize = sizeof(RawIronNativePluginInvocationV1),
                        .pluginId = invocation.pluginId.c_str(),
                        .hookPhase = invocation.hookPhase.c_str(),
                        .eventName = invocation.eventName.c_str(),
                        .hookGroup = invocation.hookGroup.c_str(),
                        .category = invocation.category.c_str(),
                        .elapsedSeconds = context.elapsedSeconds,
                        .frameIndex = context.frameIndex,
                    };
                    return hook.handler(hook.userData, &nativeInvocation) != 0;
                });
            ++registeredCount;
        }
    } catch (...) {
        for (std::size_t index = 0; index < registeredCount; ++index) {
            (void)UnregisterPluginHookHandler(eventNames[index]);
        }
        result.diagnostic = "Native plugin hook registration failed.";
        return result;
    }

    impl_->plugins.push_back(LoadedPlugin{
        .id = result.pluginId,
        .version = result.pluginVersion,
        .eventNames = std::move(eventNames),
        .lease = std::move(lease),
    });
    result.loaded = true;
    result.registeredHookCount = descriptor->hookCount;
    result.diagnostic = "Native plugin loaded.";
    return result;
}

void NativePluginHost::UnloadAll() {
    if (!impl_) {
        return;
    }
    for (auto plugin = impl_->plugins.rbegin(); plugin != impl_->plugins.rend(); ++plugin) {
        for (const std::string& eventName : plugin->eventNames) {
            (void)UnregisterPluginHookHandler(eventName);
        }
    }
    impl_->plugins.clear();
}

std::size_t NativePluginHost::LoadedPluginCount() const noexcept {
    return impl_ ? impl_->plugins.size() : 0U;
}

std::vector<std::string> NativePluginHost::LoadedPluginIds() const {
    std::vector<std::string> ids;
    if (!impl_) {
        return ids;
    }
    ids.reserve(impl_->plugins.size());
    for (const LoadedPlugin& plugin : impl_->plugins) {
        ids.push_back(plugin.id);
    }
    return ids;
}

} // namespace ri::content
