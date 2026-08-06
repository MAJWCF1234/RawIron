#include "RawIron/Content/NativePluginHost.h"

#include "RawIron/Content/NativePluginAbi.h"
#include "RawIron/Content/PluginRuntime.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <limits.h>
#endif
#endif

namespace ri::content {
namespace {

constexpr std::uint32_t kMaxNativePluginHooks = 1024U;
constexpr const char* kDescriptorSymbol = "RawIronPluginGetDescriptorV1";

#if defined(_WIN32)
using ModuleHandle = HMODULE;

/// Open reparse-safe, then LoadLibraryW by resolved path, then verify the loaded
/// image still refers to the same NTFS file identity as the vetted handle.
/// Windows cannot fd-bind LoadLibrary; same-inode in-place overwrite remains a
/// residual when the attacker already has write access to the module file.
[[nodiscard]] ModuleHandle OpenModule(const std::filesystem::path& path) {
    const HANDLE fileHandle = CreateFileW(path.c_str(),
                                          GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                          nullptr,
                                          OPEN_EXISTING,
                                          FILE_FLAG_OPEN_REPARSE_POINT,
                                          nullptr);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    BY_HANDLE_FILE_INFORMATION openedInfo{};
    if (!GetFileInformationByHandle(fileHandle, &openedInfo)
        || (openedInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U
        || (openedInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        CloseHandle(fileHandle);
        return nullptr;
    }
    std::wstring loaderPath;
    constexpr DWORD kMaxFinalPathChars = 32768U;
    for (DWORD capacity = MAX_PATH;; ) {
        loaderPath.assign(capacity, L'\0');
        const DWORD length =
            GetFinalPathNameByHandleW(fileHandle, loaderPath.data(), capacity, FILE_NAME_NORMALIZED);
        if (length == 0U) {
            CloseHandle(fileHandle);
            return nullptr;
        }
        if (length < capacity) {
            loaderPath.resize(length);
            break;
        }
        if (capacity >= kMaxFinalPathChars) {
            CloseHandle(fileHandle);
            return nullptr;
        }
        const DWORD nextCapacity = capacity * 2U;
        capacity = nextCapacity < capacity || nextCapacity > kMaxFinalPathChars
            ? kMaxFinalPathChars
            : nextCapacity;
    }
    // GetFinalPathNameByHandleW returns \\?\ / \\?\UNC\ forms. LoadLibraryW is unreliable
    // with those prefixes; strip them the same way PackageMountRegistry does.
    if (loaderPath.rfind(L"\\\\?\\UNC\\", 0) == 0) {
        loaderPath = L"\\\\" + loaderPath.substr(8);
    } else if (loaderPath.rfind(L"\\\\?\\", 0) == 0) {
        loaderPath = loaderPath.substr(4);
    }
    const ModuleHandle module = LoadLibraryW(loaderPath.c_str());
    if (module == nullptr) {
        CloseHandle(fileHandle);
        return nullptr;
    }
    wchar_t loadedPath[4096]{};
    const DWORD loadedLength =
        GetModuleFileNameW(module, loadedPath, static_cast<DWORD>(std::size(loadedPath)));
    if (loadedLength == 0U || loadedLength >= static_cast<DWORD>(std::size(loadedPath))) {
        FreeLibrary(module);
        CloseHandle(fileHandle);
        return nullptr;
    }
    const HANDLE loadedHandle = CreateFileW(loadedPath,
                                            GENERIC_READ,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                            nullptr,
                                            OPEN_EXISTING,
                                            FILE_FLAG_OPEN_REPARSE_POINT,
                                            nullptr);
    if (loadedHandle == INVALID_HANDLE_VALUE) {
        FreeLibrary(module);
        CloseHandle(fileHandle);
        return nullptr;
    }
    BY_HANDLE_FILE_INFORMATION loadedInfo{};
    const BOOL gotLoadedInfo = GetFileInformationByHandle(loadedHandle, &loadedInfo);
    CloseHandle(loadedHandle);
    const bool sameIdentity = gotLoadedInfo
        && openedInfo.dwVolumeSerialNumber == loadedInfo.dwVolumeSerialNumber
        && openedInfo.nFileIndexHigh == loadedInfo.nFileIndexHigh
        && openedInfo.nFileIndexLow == loadedInfo.nFileIndexLow
        && (loadedInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
    CloseHandle(fileHandle);
    if (!sameIdentity) {
        FreeLibrary(module);
        return nullptr;
    }
    return module;
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

/// Open with O_NOFOLLOW and load via an fd-bound path so a symlink swap cannot
/// redirect dlopen to an attacker-controlled target.
[[nodiscard]] ModuleHandle OpenModule(const std::filesystem::path& path) {
    int flags = O_RDONLY;
#if defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif
#if defined(O_NOFOLLOW)
    flags |= O_NOFOLLOW;
#endif
    const int fd = ::open(path.c_str(), flags);
    if (fd < 0) {
        return nullptr;
    }
    struct stat status {};
    if (::fstat(fd, &status) != 0 || !S_ISREG(status.st_mode)) {
        ::close(fd);
        return nullptr;
    }
#if defined(__linux__)
    char fdPath[64];
    std::snprintf(fdPath, sizeof(fdPath), "/proc/self/fd/%d", fd);
    ModuleHandle module = dlopen(fdPath, RTLD_NOW | RTLD_LOCAL);
#elif defined(__APPLE__)
    char resolved[PATH_MAX];
    if (::fcntl(fd, F_GETPATH, resolved) == -1) {
        ::close(fd);
        return nullptr;
    }
    ModuleHandle module = dlopen(resolved, RTLD_NOW | RTLD_LOCAL);
#else
    // Fail closed: path-only dlopen after open() is a TOCTOU hole on platforms
    // without an fd-bound loader path.
    ::close(fd);
    return nullptr;
#endif
    ::close(fd);
    return module;
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
        if (candidateIt == candidate.end()) {
            return false;
        }
#if defined(_WIN32)
        if (CompareStringOrdinal(rootIt->c_str(), -1, candidateIt->c_str(), -1, TRUE) != CSTR_EQUAL) {
            return false;
        }
#else
        if (*rootIt != *candidateIt) {
            return false;
        }
#endif
    }
    return true;
}

[[nodiscard]] bool IsUnsafeNativePluginPath(const std::filesystem::path& path) {
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
        return true;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        return true;
    }
    return false;
#else
    std::error_code statusError;
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, statusError);
    if (statusError || std::filesystem::is_symlink(status) || std::filesystem::is_directory(status)) {
        return true;
    }
    return false;
#endif
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
    if (IsUnsafeNativePluginPath(modulePath)) {
        result.diagnostic = "Native plugin module must be a regular non-symlink file.";
        return result;
    }
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

    // OpenModule rejects reparse points and (on Win32) verifies loaded file identity.
    const ModuleHandle rawHandle = OpenModule(canonicalModule);
    if (rawHandle == nullptr) {
        result.diagnostic =
            "Operating system loader rejected the native plugin module (missing, unsafe reparse, or load failure).";
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
