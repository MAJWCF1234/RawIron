#include "RawIron/Runtime/LevelSchedulerRuntimeModule.h"

#include "RawIron/Runtime/LevelSchedulerBundle.h"

namespace ri::runtime {

bool LevelSchedulerRuntimeModule::OnRuntimeStartup(RuntimeContext& context,
                                                   const ri::core::CommandLine& commandLine) {
    (void)commandLine;
    EnsureLevelSchedulerBundle(context);
    return true;
}

bool LevelSchedulerRuntimeModule::OnRuntimeFrame(RuntimeContext& context,
                                                 const ri::core::FrameContext& frame) {
    LevelSchedulerBundle* bundle = TryGetLevelSchedulerBundle(context);
    if (bundle == nullptr) {
        return true;
    }
    bundle->timeouts.Tick(frame.elapsedSeconds);
    bundle->intervals.Tick(frame.elapsedSeconds, frame.deltaSeconds);
    return true;
}

void LevelSchedulerRuntimeModule::OnRuntimeShutdown(RuntimeContext& context) {
    if (LevelSchedulerBundle* bundle = TryGetLevelSchedulerBundle(context); bundle != nullptr) {
        bundle->timeouts.Clear();
        bundle->intervals.Clear();
    }
    context.Services().Unregister<LevelSchedulerBundle>();
}

} // namespace ri::runtime
