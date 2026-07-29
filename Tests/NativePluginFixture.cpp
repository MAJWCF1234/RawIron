#include "RawIron/Content/NativePluginAbi.h"

#include <cstdint>

namespace {

bool g_initialized = false;

int32_t InitializeFixture() {
    g_initialized = true;
    return 1;
}

void ShutdownFixture() {
    g_initialized = false;
}

int32_t HandleFixtureTick(
    void*,
    const RawIronNativePluginInvocationV1* invocation) {
    return g_initialized
        && invocation != nullptr
        && invocation->structSize >= sizeof(RawIronNativePluginInvocationV1)
        && invocation->eventName != nullptr
        && invocation->frameIndex == 7
        ? 1
        : 0;
}

const RawIronNativePluginHookV1 kHooks[] = {
    {
        .structSize = sizeof(RawIronNativePluginHookV1),
        .eventName = "native_fixture_tick",
        .requiredCapability = "native.fixture",
        .userData = nullptr,
        .handler = HandleFixtureTick,
    },
};

const RawIronNativePluginDescriptorV1 kDescriptor{
    .structSize = sizeof(RawIronNativePluginDescriptorV1),
    .abiVersion = RAWIRON_NATIVE_PLUGIN_ABI_V1,
    .pluginId = "rawiron.test.native-fixture",
    .pluginVersion = "1.0.0",
    .hookCount = 1U,
    .hooks = kHooks,
    .initialize = InitializeFixture,
    .shutdown = ShutdownFixture,
};

} // namespace

#if defined(_WIN32)
#define RAWIRON_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define RAWIRON_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

RAWIRON_PLUGIN_EXPORT const RawIronNativePluginDescriptorV1* RawIronPluginGetDescriptorV1() {
    return &kDescriptor;
}
