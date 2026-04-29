#include "RawIron/Core/RenderRecorder.h"

namespace ri::core {

bool ExecuteRenderSubmissionPlan(const RenderCommandStream& stream,
                                 const RenderSubmissionPlan& plan,
                                 RenderCommandSink& sink,
                                 RenderRecorderStats* outStats) {
    RenderRecorderStats stats{};
    const auto flushStats = [&]() {
        if (outStats != nullptr) {
            *outStats = stats;
        }
    };
    for (std::size_t batchIndex = 0; batchIndex < plan.batches.size(); ++batchIndex) {
        const RenderSubmissionBatch& batch = plan.batches[batchIndex];
        if (!sink.BeginBatch(batch)) {
            flushStats();
            return false;
        }
        stats.batchesVisited += 1;

        bool sinkCommandFailed = false;
        bool recorderFailed = false;
        const bool iterated = ForEachSubmissionBatchCommand(
            stream,
            plan,
            batchIndex,
            [&](const RenderCommandView& view, std::size_t, std::size_t) {
                stats.commandsVisited += 1;
                switch (view.header.type) {
                case RenderCommandType::ClearColor: {
                    ClearColorCommand command{};
                    if (!view.ReadPayload(command)) {
                        recorderFailed = true;
                        return false;
                    }
                    if (!sink.RecordClearColor(command, view.header)) {
                        sinkCommandFailed = true;
                        return false;
                    }
                    stats.clearCommands += 1;
                    return true;
                }
                case RenderCommandType::SetViewProjection: {
                    SetViewProjectionCommand command{};
                    if (!view.ReadPayload(command)) {
                        recorderFailed = true;
                        return false;
                    }
                    if (!sink.RecordSetViewProjection(command, view.header)) {
                        sinkCommandFailed = true;
                        return false;
                    }
                    stats.setViewProjectionCommands += 1;
                    return true;
                }
                case RenderCommandType::DrawMesh: {
                    DrawMeshCommand command{};
                    if (!view.ReadPayload(command)) {
                        recorderFailed = true;
                        return false;
                    }
                    if (!sink.RecordDrawMesh(command, view.header)) {
                        sinkCommandFailed = true;
                        return false;
                    }
                    stats.drawCommands += 1;
                    return true;
                }
                default:
                    if (!sink.RecordUnknown(view)) {
                        sinkCommandFailed = true;
                        return false;
                    }
                    stats.unknownCommands += 1;
                    return true;
                }
            });

        sink.EndBatch(batch);
        if (!iterated || sinkCommandFailed || recorderFailed) {
            flushStats();
            return false;
        }
    }

    flushStats();
    return true;
}

} // namespace ri::core
