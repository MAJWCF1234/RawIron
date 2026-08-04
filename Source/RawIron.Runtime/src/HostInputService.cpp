#include "RawIron/Runtime/HostInputService.h"

namespace ri::runtime {

void HostInputService::Bind(void* hostWindowHandle, void* overlayWindowHandle) noexcept {
    hostWindow_ = hostWindowHandle;
    overlayWindow_ = overlayWindowHandle;
}

void HostInputService::Sync(void* hostWindowHandle, void* overlayWindowHandle) noexcept {
    Bind(hostWindowHandle, overlayWindowHandle);
    Tick();
}

void HostInputService::Tick() noexcept {
    focus_.Update(hostWindow_, overlayWindow_);
}

void EnsureHostInputService(RuntimeContext& context) {
    if (context.Services().Contains<HostInputService>()) {
        return;
    }
    context.Services().Register<HostInputService>(std::make_shared<HostInputService>());
}

HostInputService* TryGetHostInputService(RuntimeContext& context) noexcept {
    return context.Services().Resolve<HostInputService>().get();
}

HostInputService& GetHostInputService(RuntimeContext& context) {
    EnsureHostInputService(context);
    return *context.Services().Resolve<HostInputService>();
}

bool HostInputRuntimeModule::OnRuntimeStartup(RuntimeContext& context,
                                              const ri::core::CommandLine& commandLine) {
    (void)commandLine;
    EnsureHostInputService(context);
    return true;
}

bool HostInputRuntimeModule::OnRuntimeFrame(RuntimeContext& context,
                                            const ri::core::FrameContext& frame) {
    (void)frame;
    if (HostInputService* input = TryGetHostInputService(context); input != nullptr) {
        input->Tick();
    }
    return true;
}

void HostInputRuntimeModule::OnRuntimeShutdown(RuntimeContext& context) {
    context.Services().Unregister<HostInputService>();
}

std::unique_ptr<RuntimeModule> MakeHostInputRuntimeModule() {
    return std::make_unique<HostInputRuntimeModule>();
}

} // namespace ri::runtime
