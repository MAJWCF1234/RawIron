#pragma once

#include "RawIron/Core/KeyboardFocus.h"
#include "RawIron/Runtime/RuntimeCore.h"

#include <memory>

namespace ri::runtime {

/// Engine-owned keyboard focus + window binding for standalone hosts.
///
/// Games and demos mount this through \ref HostInputRuntimeModule (included in
/// \ref RuntimeCore::AddDefaultModules) and then only Bind windows + query keys.
/// They never own a \ref ri::core::KeyboardFocusGate or call Update themselves.
class HostInputService {
public:
    /// Remember the host / optional overlay windows used for focus ownership.
    /// Safe to call every frame; Update runs from the runtime module and from \ref Sync.
    void Bind(void* hostWindowHandle, void* overlayWindowHandle = nullptr) noexcept;

    /// Bind then refresh focus immediately. Call at the start of a game tick when the
    /// window handle may have changed since the last runtime-module Update.
    void Sync(void* hostWindowHandle, void* overlayWindowHandle = nullptr) noexcept;

    /// Refresh focus from the last Bound windows. Invoked by HostInputRuntimeModule each frame.
    void Tick() noexcept;

    [[nodiscard]] ri::core::KeyboardFocusGate& Focus() noexcept { return focus_; }
    [[nodiscard]] const ri::core::KeyboardFocusGate& Focus() const noexcept { return focus_; }

    [[nodiscard]] bool Focused() const noexcept { return focus_.Focused(); }
    [[nodiscard]] bool JustRegainedFocus() const noexcept { return focus_.JustRegainedFocus(); }
    [[nodiscard]] bool IsKeyDown(int virtualKey) const noexcept { return focus_.IsKeyDown(virtualKey); }
    [[nodiscard]] bool IsKeyDownSettled(int virtualKey) const noexcept {
        return focus_.IsKeyDownSettled(virtualKey);
    }
    [[nodiscard]] bool ConsumeKeyPress(int virtualKey) const noexcept {
        return focus_.ConsumeKeyPress(virtualKey);
    }

    [[nodiscard]] void* HostWindowHandle() const noexcept { return hostWindow_; }
    [[nodiscard]] void* OverlayWindowHandle() const noexcept { return overlayWindow_; }

private:
    ri::core::KeyboardFocusGate focus_{};
    void* hostWindow_ = nullptr;
    void* overlayWindow_ = nullptr;
};

void EnsureHostInputService(RuntimeContext& context);
[[nodiscard]] HostInputService* TryGetHostInputService(RuntimeContext& context) noexcept;
[[nodiscard]] HostInputService& GetHostInputService(RuntimeContext& context);

/// Registers HostInputService when missing. Headless hosts stay inert (null windows).
class HostInputRuntimeModule final : public RuntimeModule {
public:
    [[nodiscard]] std::string_view Name() const noexcept override { return "HostInput"; }

    bool OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine& commandLine) override;
    bool OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame) override;
    void OnRuntimeShutdown(RuntimeContext& context) override;
};

[[nodiscard]] std::unique_ptr<RuntimeModule> MakeHostInputRuntimeModule();

} // namespace ri::runtime
