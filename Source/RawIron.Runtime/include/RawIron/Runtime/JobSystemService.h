#pragma once

#include "RawIron/Core/JobSystem.h"
#include "RawIron/Runtime/RuntimeCore.h"

#include <memory>

namespace ri::runtime {

[[nodiscard]] ri::core::JobSystem* TryGetJobSystem(RuntimeContext& context) noexcept;
[[nodiscard]] const ri::core::JobSystem* TryGetJobSystem(const RuntimeContext& context) noexcept;
[[nodiscard]] ri::core::JobSystem& GetJobSystem(RuntimeContext& context);

/// Mounts the shared worker pool before gameplay modules and drains it after those modules stop.
/// `--job-workers N` selects an explicit count; zero/absent uses the hardware-aware default.
class JobSystemRuntimeModule final : public RuntimeModule {
public:
    [[nodiscard]] std::string_view Name() const noexcept override { return "Jobs"; }

    bool OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine& commandLine) override;
    void OnRuntimeShutdown(RuntimeContext& context) override;

private:
    std::shared_ptr<ri::core::JobSystem> ownedService_;
};

[[nodiscard]] std::unique_ptr<RuntimeModule> MakeJobSystemRuntimeModule();

} // namespace ri::runtime
