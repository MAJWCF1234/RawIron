#include "RawIron/Runtime/LevelSchedulerBundle.h"

#include "RawIron/Runtime/RuntimeCore.h"

namespace ri::runtime {

void EnsureLevelSchedulerBundle(RuntimeContext& context) {
    if (context.Services().Contains<LevelSchedulerBundle>()) {
        return;
    }
    context.Services().Register<LevelSchedulerBundle>(std::make_shared<LevelSchedulerBundle>());
}

LevelSchedulerBundle* TryGetLevelSchedulerBundle(RuntimeContext& context) noexcept {
    const std::shared_ptr<LevelSchedulerBundle> bundle = context.Services().Resolve<LevelSchedulerBundle>();
    return bundle != nullptr ? bundle.get() : nullptr;
}

const LevelSchedulerBundle* TryGetLevelSchedulerBundle(const RuntimeContext& context) noexcept {
    const std::shared_ptr<LevelSchedulerBundle> bundle = context.Services().Resolve<LevelSchedulerBundle>();
    return bundle != nullptr ? bundle.get() : nullptr;
}

} // namespace ri::runtime
