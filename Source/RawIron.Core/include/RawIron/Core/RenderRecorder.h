#pragma once

#include "RawIron/Core/RenderCommandStream.h"
#include "RawIron/Core/RenderSubmissionPlan.h"

#include <cstddef>

namespace ri::core {

struct RenderRecorderStats {
    std::size_t batchesVisited = 0;
    std::size_t commandsVisited = 0;
    std::size_t clearCommands = 0;
    std::size_t setViewProjectionCommands = 0;
    std::size_t drawCommands = 0;
    std::size_t unknownCommands = 0;
};

class RenderCommandSink {
public:
    virtual ~RenderCommandSink() = default;

    virtual bool BeginBatch(const RenderSubmissionBatch& batch) = 0;
    virtual void EndBatch(const RenderSubmissionBatch& batch) = 0;
    virtual bool RecordClearColor(const ClearColorCommand& command, const RenderCommandHeader& header) = 0;
    virtual bool RecordSetViewProjection(const SetViewProjectionCommand& command, const RenderCommandHeader& header) = 0;
    virtual bool RecordDrawMesh(const DrawMeshCommand& command, const RenderCommandHeader& header) = 0;
    virtual bool RecordUnknown(const RenderCommandView& view) = 0;
};

[[nodiscard]] bool ExecuteRenderSubmissionPlan(const RenderCommandStream& stream,
                                               const RenderSubmissionPlan& plan,
                                               RenderCommandSink& sink,
                                               RenderRecorderStats* outStats = nullptr);

} // namespace ri::core
