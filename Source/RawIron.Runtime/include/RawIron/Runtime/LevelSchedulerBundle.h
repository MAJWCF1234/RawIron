#pragma once

#include "RawIron/Runtime/LevelScopedSchedulers.h"

#include <memory>

namespace ri::runtime {

class RuntimeContext;

/// Level-scoped timeout/interval schedulers owned by the runtime service registry.
struct LevelSchedulerBundle {
    LevelScopedTimeoutScheduler timeouts{};
    LevelScopedIntervalScheduler intervals{};
};

/// Registers \ref LevelSchedulerBundle on the context if missing.
void EnsureLevelSchedulerBundle(RuntimeContext& context);

[[nodiscard]] LevelSchedulerBundle* TryGetLevelSchedulerBundle(RuntimeContext& context) noexcept;
[[nodiscard]] const LevelSchedulerBundle* TryGetLevelSchedulerBundle(const RuntimeContext& context) noexcept;

} // namespace ri::runtime
