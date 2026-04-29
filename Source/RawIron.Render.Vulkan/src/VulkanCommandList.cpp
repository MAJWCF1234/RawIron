#include "RawIron/Render/VulkanCommandList.h"

#include <cmath>
#include <cstring>

namespace ri::render::vulkan {
namespace {

[[nodiscard]] float FiniteOrZero(float value) noexcept {
    return std::isfinite(value) ? value : 0.0f;
}

} // namespace

VulkanCommandListSink::VulkanCommandListSink(bool allowUnknownCommands)
    : allowUnknownCommands_(allowUnknownCommands) {}

bool VulkanCommandListSink::BeginBatch(const ri::core::RenderSubmissionBatch& batch) {
    activePass_ = batch.passIndex;
    activePipeline_ = batch.pipelineBucket;
    operations_.push_back(VulkanRenderOp{
        .type = VulkanRenderOpType::BeginBatch,
        .passIndex = batch.passIndex,
        .pipelineBucket = batch.pipelineBucket,
    });
    return true;
}

void VulkanCommandListSink::EndBatch(const ri::core::RenderSubmissionBatch& batch) {
    operations_.push_back(VulkanRenderOp{
        .type = VulkanRenderOpType::EndBatch,
        .passIndex = batch.passIndex,
        .pipelineBucket = batch.pipelineBucket,
    });
}

bool VulkanCommandListSink::RecordClearColor(const ri::core::ClearColorCommand& command,
                                             const ri::core::RenderCommandHeader&) {
    VulkanRenderOp operation{};
    operation.type = VulkanRenderOpType::ClearColor;
    operation.passIndex = activePass_;
    operation.pipelineBucket = activePipeline_;
    operation.clearColor[0] = FiniteOrZero(command.r);
    operation.clearColor[1] = FiniteOrZero(command.g);
    operation.clearColor[2] = FiniteOrZero(command.b);
    operation.clearColor[3] = FiniteOrZero(command.a);
    operations_.push_back(operation);
    return true;
}

bool VulkanCommandListSink::RecordSetViewProjection(const ri::core::SetViewProjectionCommand& command,
                                                    const ri::core::RenderCommandHeader&) {
    VulkanRenderOp operation{
        .type = VulkanRenderOpType::SetViewProjection,
        .passIndex = activePass_,
        .pipelineBucket = activePipeline_,
    };
    std::memcpy(operation.viewProjection, command.viewProjection, sizeof(operation.viewProjection));
    operations_.push_back(operation);
    return true;
}

bool VulkanCommandListSink::RecordDrawMesh(const ri::core::DrawMeshCommand& command,
                                           const ri::core::RenderCommandHeader&) {
    VulkanRenderOp operation{
        .type = VulkanRenderOpType::DrawMesh,
        .passIndex = activePass_,
        .pipelineBucket = activePipeline_,
        .meshHandle = command.meshHandle,
        .materialHandle = command.materialHandle,
        .firstIndex = command.firstIndex,
        .indexCount = command.indexCount,
        .instanceCount = command.instanceCount,
    };
    std::memcpy(operation.model, command.model, sizeof(operation.model));
    operations_.push_back(operation);
    return true;
}

bool VulkanCommandListSink::RecordUnknown(const ri::core::RenderCommandView&) {
    operations_.push_back(VulkanRenderOp{
        .type = VulkanRenderOpType::Unknown,
        .passIndex = activePass_,
        .pipelineBucket = activePipeline_,
    });
    return allowUnknownCommands_;
}

const std::vector<VulkanRenderOp>& VulkanCommandListSink::Operations() const noexcept {
    return operations_;
}

void VulkanCommandListSink::Clear() noexcept {
    operations_.clear();
}

} // namespace ri::render::vulkan
