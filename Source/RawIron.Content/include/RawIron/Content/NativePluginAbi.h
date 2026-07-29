#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RAWIRON_NATIVE_PLUGIN_ABI_V1 = 1U,
};

typedef struct RawIronNativePluginInvocationV1 {
    uint32_t structSize;
    const char* pluginId;
    const char* hookPhase;
    const char* eventName;
    const char* hookGroup;
    const char* category;
    double elapsedSeconds;
    int32_t frameIndex;
} RawIronNativePluginInvocationV1;

typedef int32_t (*RawIronNativePluginHookFnV1)(
    void* userData,
    const RawIronNativePluginInvocationV1* invocation);

typedef struct RawIronNativePluginHookV1 {
    uint32_t structSize;
    const char* eventName;
    const char* requiredCapability;
    void* userData;
    RawIronNativePluginHookFnV1 handler;
} RawIronNativePluginHookV1;

typedef int32_t (*RawIronNativePluginInitializeFnV1)(void);
typedef void (*RawIronNativePluginShutdownFnV1)(void);

typedef struct RawIronNativePluginDescriptorV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    const char* pluginId;
    const char* pluginVersion;
    uint32_t hookCount;
    const RawIronNativePluginHookV1* hooks;
    RawIronNativePluginInitializeFnV1 initialize;
    RawIronNativePluginShutdownFnV1 shutdown;
} RawIronNativePluginDescriptorV1;

typedef const RawIronNativePluginDescriptorV1* (*RawIronPluginGetDescriptorV1Fn)(void);

#ifdef __cplusplus
}
#endif
