#include "RawIron/Content/PluginRuntime.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <filesystem>
#include <mutex>
#include <set>
#include <unordered_map>

namespace ri::content {

namespace {

struct PluginHookRegistration {
    PluginHookHandler handler;
    std::string requiredCapability;
};

std::unordered_map<std::string, PluginHookRegistration, std::hash<std::string>, std::equal_to<>> g_handlers{};
std::mutex g_handlersMutex{};
bool g_builtinsRegistered = false;

void RegisterPluginHookHandlerUnsafe(const std::string_view eventName,
                                     const std::string_view requiredCapability,
                                     PluginHookHandler handler) {
    g_handlers[std::string(eventName)] = PluginHookRegistration{
        .handler = std::move(handler),
        .requiredCapability = std::string(requiredCapability),
    };
}

void EnsureBuiltinHandlers() {
    std::lock_guard lock(g_handlersMutex);
    if (g_builtinsRegistered) {
        return;
    }
    g_builtinsRegistered = true;

    RegisterPluginHookHandlerUnsafe("bootstrap", "", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.runtimeScalars["plugin." + invocation.pluginId + ".bootstrapped"] = 1.0f;
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Bootstrap complete for " + invocation.pluginId,
        });
        return true;
    });

    RegisterPluginHookHandlerUnsafe("frame_sample", "telemetry.runtime", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Frame sample at t=" + std::to_string(context.elapsedSeconds),
        });
        return true;
    });

    RegisterPluginHookHandlerUnsafe("audio_zones_bind", "audio.runtime", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.runtimeScalars["plugin." + invocation.pluginId + ".audio_bound"] = 1.0f;
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Audio zones bound from authored CSV",
        });
        return true;
    });

    RegisterPluginHookHandlerUnsafe("ambient_tick", "audio.runtime", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        const float gain = ScriptScalarOr(context.runtimeScalars, "ambient_presence_gain", 0.85f);
        context.runtimeScalars["plugin." + invocation.pluginId + ".ambient_gain"] = gain;
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Ambient tick gain=" + std::to_string(gain),
        });
        return true;
    });

    RegisterPluginHookHandlerUnsafe("quest_marker_refresh", "ui.overlay", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Quest markers refreshed",
        });
        return true;
    });

    RegisterPluginHookHandlerUnsafe("ai_policy_bind", "gameplay.ai", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "AI policy bridge bound",
        });
        return true;
    });

    RegisterPluginHookHandlerUnsafe("timeline_register", "gameplay.cinematics", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Cinematic timeline registered",
        });
        return true;
    });

    RegisterPluginHookHandlerUnsafe("mode_update", "gameplay.ai", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "AI mode update tick",
        });
        return true;
    });

    RegisterPluginHookHandlerUnsafe("zone_cutscene_trigger", "gameplay.cinematics", [](PluginHookContext& context, const PluginHookInvocation& invocation) {
        context.results.push_back(PluginHookResult{
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .handled = true,
            .message = "Zone cutscene trigger evaluated",
        });
        return true;
    });
}

[[nodiscard]] const PluginManifestEntry* FindManifest(const PluginProjectData& data, const std::string& pluginId) {
    for (const PluginManifestEntry& entry : data.manifestEntries) {
        if (entry.id == pluginId) {
            return &entry;
        }
    }
    return nullptr;
}

[[nodiscard]] const PluginRegistryEntry* FindRegistry(const PluginProjectData& data, const std::string& pluginId) {
    for (const PluginRegistryEntry& entry : data.registryEntries) {
        if (entry.id == pluginId) {
            return &entry;
        }
    }
    return nullptr;
}

void EmitPluginRuntimeEvent(PluginHookContext& context,
                            const PluginHookInvocation& invocation,
                            const PluginHookResult& result) noexcept {
    if (!context.eventSink) {
        return;
    }
    try {
        context.eventSink(PluginRuntimeEvent{
            .type = "plugin.hook." + invocation.eventName,
            .pluginId = invocation.pluginId,
            .eventName = invocation.eventName,
            .hookPhase = std::string(context.hookPhase),
            .handled = result.handled,
            .capabilityDenied = result.capabilityDenied,
            .requiredCapability = result.requiredCapability,
            .message = result.message,
        });
    } catch (...) {
        context.eventSinkFailures += 1U;
    }
}

[[nodiscard]] bool IsNativeModulePath(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
#if defined(_WIN32)
    return extension == ".dll";
#elif defined(__APPLE__)
    return extension == ".dylib" || extension == ".so";
#else
    return extension == ".so";
#endif
}

[[nodiscard]] std::vector<NativePluginLoadResult> LoadNativeProjectPlugins(
    PluginProjectData& projectData,
    NativePluginHost& host) {
    std::vector<NativePluginLoadResult> results;
    for (const ActivePlugin& plugin : projectData.activePlugins) {
        if (!IsNativeModulePath(plugin.manifest.resolvedEntryPath)) {
            continue;
        }
        NativePluginLoadOptions options{};
        options.allowedRoot = plugin.manifest.sourceKind == PluginSourceKind::External
            ? plugin.manifest.resolvedEntryPath.parent_path()
            : projectData.gameRoot;
        options.allowedCapabilities = plugin.registry.capabilities;
        options.enforceAllowedCapabilities = true;
        NativePluginLoadResult result = host.Load(plugin.manifest.resolvedEntryPath, options);
        if (!result.loaded) {
            projectData.issues.push_back({
                .message = "Native plugin load failed: " + plugin.manifest.id + " (" + result.diagnostic + ")",
            });
        }
        results.push_back(std::move(result));
    }
    return results;
}

} // namespace

void RegisterBuiltinPluginHookHandlers() {
    EnsureBuiltinHandlers();
}

void RegisterPluginHookHandler(const std::string_view eventName, PluginHookHandler handler) {
    RegisterPluginHookHandler(eventName, "", std::move(handler));
}

void RegisterPluginHookHandler(const std::string_view eventName,
                               const std::string_view requiredCapability,
                               PluginHookHandler handler) {
    if (eventName.empty() || !handler) {
        return;
    }
    std::lock_guard lock(g_handlersMutex);
    RegisterPluginHookHandlerUnsafe(eventName, requiredCapability, std::move(handler));
}

void ClearPluginHookHandlers() {
    std::lock_guard lock(g_handlersMutex);
    g_handlers.clear();
    g_builtinsRegistered = false;
}

bool UnregisterPluginHookHandler(const std::string_view eventName) {
    std::lock_guard lock(g_handlersMutex);
    return g_handlers.erase(std::string(eventName)) > 0U;
}

bool IsPluginHookHandlerRegistered(const std::string_view eventName) {
    EnsureBuiltinHandlers();
    std::lock_guard lock(g_handlersMutex);
    const auto handler = g_handlers.find(std::string(eventName));
    return handler != g_handlers.end() && static_cast<bool>(handler->second.handler);
}

std::vector<std::string> RegisteredPluginHookHandlerNames() {
    EnsureBuiltinHandlers();
    std::lock_guard lock(g_handlersMutex);
    std::vector<std::string> names{};
    names.reserve(g_handlers.size());
    for (const auto& [name, registration] : g_handlers) {
        if (registration.handler) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<PluginHookHandlerInfo> RegisteredPluginHookHandlers() {
    EnsureBuiltinHandlers();
    std::lock_guard lock(g_handlersMutex);
    std::vector<PluginHookHandlerInfo> info{};
    info.reserve(g_handlers.size());
    for (const auto& [name, registration] : g_handlers) {
        if (registration.handler) {
            info.push_back(PluginHookHandlerInfo{
                .eventName = name,
                .requiredCapability = registration.requiredCapability,
            });
        }
    }
    std::sort(info.begin(), info.end(), [](const PluginHookHandlerInfo& a, const PluginHookHandlerInfo& b) {
        return a.eventName < b.eventName;
    });
    return info;
}

void AppendPluginHookHandlerIssues(const PluginProjectData& data, std::vector<PluginValidationIssue>& issues) {
    EnsureBuiltinHandlers();
    std::set<std::string> missingHandlersReported{};
    std::set<std::string> capabilityDenialsReported{};
    for (const PluginHookBinding& hook : data.hookBindings) {
        if (!IsPluginHookHandlerRegistered(hook.eventName)) {
            const PluginManifestEntry* manifest = FindManifest(data, hook.pluginId);
            if (manifest != nullptr && IsNativeModulePath(manifest->resolvedEntryPath)) {
                // The versioned module registers this event after project policy/path validation.
                continue;
            }
            if (missingHandlersReported.insert(hook.eventName).second) {
                issues.push_back(PluginValidationIssue{
                    .message = "Hook event has no engine handler: " + hook.eventName + " (plugin " + hook.pluginId + ")",
                });
            }
            continue;
        }
        if (!data.policy.enforceDeclaredCapabilities) {
            continue;
        }

        std::string requiredCapability;
        {
            std::lock_guard lock(g_handlersMutex);
            const auto handler = g_handlers.find(hook.eventName);
            if (handler != g_handlers.end()) {
                requiredCapability = handler->second.requiredCapability;
            }
        }
        if (requiredCapability.empty()) {
            continue;
        }
        const PluginRegistryEntry* registry = FindRegistry(data, hook.pluginId);
        const bool granted = registry != nullptr
            && std::find(registry->capabilities.begin(), registry->capabilities.end(), requiredCapability)
                != registry->capabilities.end();
        const std::string denialKey = hook.pluginId + '\n' + requiredCapability;
        if (!granted && capabilityDenialsReported.insert(denialKey).second) {
            issues.push_back(PluginValidationIssue{
                .message = "Plugin capability not granted: " + hook.pluginId + " requires "
                    + requiredCapability + " for event " + hook.eventName,
            });
        }
    }
}

std::size_t DispatchPluginHooks(PluginHookContext& context, const std::string_view hookPhase) {
    EnsureBuiltinHandlers();
    if (context.projectData == nullptr) {
        return 0U;
    }

    const PluginProjectData& data = *context.projectData;
    context.hookPhase = hookPhase;
    ScriptScalarMap mergedScalars = context.runtimeScalars;
    if (data.policy.allowRuntimeOverrides) {
        for (const auto& [key, value] : data.tuningScalars) {
            mergedScalars.try_emplace(key, value);
        }
    } else {
        for (const auto& [key, value] : data.tuningScalars) {
            mergedScalars[key] = value;
        }
    }
    context.runtimeScalars = std::move(mergedScalars);

    const std::vector<PluginHookBinding> hooks = CollectHooksForPhase(data, hookPhase);
    const int maxChain = std::clamp(data.policy.maxHookChain, 0, 128);
    const bool enforceStartupBudget = hookPhase == "startup";
    const double budgetMs = static_cast<double>(std::clamp(data.policy.startupTimeoutMs, 1, 10000));
    const auto dispatchStarted = std::chrono::steady_clock::now();
    std::size_t executed = 0U;
    for (std::size_t hookIndex = 0U; hookIndex < hooks.size(); ++hookIndex) {
        const PluginHookBinding& hook = hooks[hookIndex];
        context.hookElapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - dispatchStarted).count();
        if (enforceStartupBudget && context.hookElapsedMs >= budgetMs) {
            context.hookBudgetExceeded = true;
            context.hooksSkippedByBudget += hooks.size() - hookIndex;
            break;
        }
        if (executed >= static_cast<std::size_t>(maxChain)) {
            break;
        }
        const PluginRegistryEntry* registry = FindRegistry(data, hook.pluginId);
        if (registry != nullptr && !registry->enabled) {
            continue;
        }

        PluginHookInvocation invocation{
            .pluginId = hook.pluginId,
            .hookPhase = hook.hookPhase,
            .eventName = hook.eventName,
            .priority = hook.priority,
        };
        if (registry != nullptr) {
            invocation.hookGroup = registry->hookGroup;
            invocation.grantedCapabilities = registry->capabilities;
        }
        if (const PluginManifestEntry* manifest = FindManifest(data, hook.pluginId)) {
            invocation.category = manifest->category;
        }

        PluginHookRegistration registration{};
        {
            std::lock_guard lock(g_handlersMutex);
            const auto handlerIt = g_handlers.find(hook.eventName);
            if (handlerIt != g_handlers.end()) {
                registration = handlerIt->second;
            }
        }
        PluginHookResult result{
            .pluginId = hook.pluginId,
            .eventName = hook.eventName,
            .handled = false,
            .message = "No handler registered for event '" + hook.eventName + "'",
        };
        const bool capabilityGranted = registration.requiredCapability.empty()
            || !data.policy.enforceDeclaredCapabilities
            || std::find(invocation.grantedCapabilities.begin(),
                         invocation.grantedCapabilities.end(),
                         registration.requiredCapability) != invocation.grantedCapabilities.end();
        if (registration.handler && !capabilityGranted) {
            result.capabilityDenied = true;
            result.requiredCapability = registration.requiredCapability;
            result.message = "Capability denied: plugin '" + hook.pluginId + "' requires '"
                + registration.requiredCapability + "' for event '" + hook.eventName + "'";
            context.results.push_back(result);
            context.capabilityDenials += 1U;
        } else if (registration.handler) {
            const std::size_t resultsBefore = context.results.size();
            try {
                const bool handled = registration.handler(context, invocation);
                if (context.results.size() > resultsBefore) {
                    result = context.results.back();
                } else {
                    result.handled = handled;
                    result.message = handled
                        ? "Handler completed for " + hook.pluginId
                        : "Handler declined event '" + hook.eventName + "' for " + hook.pluginId;
                    context.results.push_back(result);
                }
            } catch (const std::exception& error) {
                context.results.resize(resultsBefore);
                result.message = "Handler threw for event '" + hook.eventName + "': " + error.what();
                context.results.push_back(result);
            } catch (...) {
                context.results.resize(resultsBefore);
                result.message = "Handler threw for event '" + hook.eventName + "'.";
                context.results.push_back(result);
            }
        } else {
            context.results.push_back(result);
        }
        EmitPluginRuntimeEvent(context, invocation, result);
        executed += 1U;
    }
    context.hookElapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - dispatchStarted).count();
    if (enforceStartupBudget && context.hookElapsedMs >= budgetMs) {
        context.hookBudgetExceeded = true;
    }
    return executed;
}

GamePluginBootstrap BootstrapGamePlugins(const std::filesystem::path& gameRoot) {
    GamePluginBootstrap bootstrap{};
    bootstrap.projectData = LoadPluginProjectData(gameRoot);
    RegisterBuiltinPluginHookHandlers();
    NativePluginHost nativeHost;
    bootstrap.nativePluginLoads = LoadNativeProjectPlugins(bootstrap.projectData, nativeHost);

    PluginHookContext context{
        .gameRoot = gameRoot,
        .projectData = &bootstrap.projectData,
        .hookPhase = "startup",
    };
    bootstrap.startupHooksExecuted = DispatchPluginHooks(context, "startup");
    bootstrap.startupBudgetExceeded = context.hookBudgetExceeded;
    bootstrap.startupHooksSkippedByBudget = context.hooksSkippedByBudget;
    bootstrap.startupElapsedMs = context.hookElapsedMs;
    bootstrap.runtimeScalars = context.runtimeScalars;
    bootstrap.startupResults = std::move(context.results);
    return bootstrap;
}

void GamePluginRuntimeSession::Bootstrap() {
    if (packageMountRegistry) {
        ReleaseDeclaredGamePackages(*packageMountRegistry, packageMountReport);
    }
    if (nativePluginHost) {
        nativePluginHost->UnloadAll();
    }
    projectData = LoadPluginProjectData(gameRoot);
    RegisterBuiltinPluginHookHandlers();
    packageMountRegistry = std::make_shared<PackageMountRegistry>();
    packageMountReport = MountDeclaredGamePackages(*packageMountRegistry, gameRoot);
    nativePluginHost = std::make_shared<NativePluginHost>();
    nativePluginLoads = LoadNativeProjectPlugins(projectData, *nativePluginHost);
    PluginHookContext context{
        .gameRoot = gameRoot,
        .projectData = &projectData,
        .hookPhase = "startup",
        .eventSink = eventSink,
    };
    startupHooksExecuted = DispatchPluginHooks(context, "startup");
    startupBudgetExceeded = context.hookBudgetExceeded;
    startupHooksSkippedByBudget = context.hooksSkippedByBudget;
    startupElapsedMs = context.hookElapsedMs;
    runtimeScalars = context.runtimeScalars;
    startupResults = std::move(context.results);
    frameCounter = 0;
}

std::size_t GamePluginRuntimeSession::TickRuntime(const double elapsedSeconds) {
    if (projectData.activePlugins.empty()) {
        return 0U;
    }
    frameCounter += 1;
    const int batchSize = ScriptScalarOrIntClamped(
        projectData.tuningScalars, "plugin_hook_batch_size", 4, 1, 120);
    if (frameCounter % batchSize != 0) {
        return 0U;
    }

    PluginHookContext context{
        .gameRoot = gameRoot,
        .projectData = &projectData,
        .hookPhase = "runtime",
        .elapsedSeconds = elapsedSeconds,
        .frameIndex = frameCounter,
        .runtimeScalars = runtimeScalars,
        .eventSink = eventSink,
    };
    const std::size_t executed = DispatchPluginHooks(context, "runtime");
    runtimeScalars = context.runtimeScalars;
    return executed;
}

std::string DescribePluginModificationModel() {
    return
        "RawIron plugins are policy-gated project packages. Manifest + registry "
        "declare inventory and capability grants; load_order.cfg defines precedence; hooks.riplugin binds "
        "startup/runtime events. At runtime DispatchPluginHooks capability-checks and executes event handlers "
        "that may tune scalars, bind authored CSV "
        "surfaces, and emit RuntimeEventBus topics. Versioned native modules may additionally register C-ABI "
        "handlers through NativePluginHost; their paths and requested capabilities are validated before load.";
}

} // namespace ri::content
