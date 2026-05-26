#pragma once

#include "RawIron/Runtime/RuntimeCore.h"

namespace ri::runtime {

/// Ticks \ref LevelSchedulerBundle each runtime frame using the authoritative frame clock.
class LevelSchedulerRuntimeModule final : public RuntimeModule {
public:
    [[nodiscard]] std::string_view Name() const noexcept override {
        return "LevelSchedulerRuntime";
    }

    bool OnRuntimeStartup(RuntimeContext& context, const ri::core::CommandLine& commandLine) override;
    bool OnRuntimeFrame(RuntimeContext& context, const ri::core::FrameContext& frame) override;
    void OnRuntimeShutdown(RuntimeContext& context) override;
};

} // namespace ri::runtime
