#include "RawIron/Render/VulkanCommandBufferRecorder.h"

#include <cstring>

namespace ri::render::vulkan {

bool VulkanCommandBufferRecorder::BeginBatch(std::uint8_t passIndex, std::uint16_t pipelineBucket) {
    if (inBatch_) {
        return false;
    }
    inBatch_ = true;
    activePass_ = passIndex;
    activePipeline_ = pipelineBucket;
    intents_.push_back(VulkanCommandIntent{
        .type = VulkanCommandIntentType::BeginBatch,
        .passIndex = passIndex,
        .pipelineBucket = pipelineBucket,
    });
    return true;
}

bool VulkanCommandBufferRecorder::EndBatch(std::uint8_t passIndex, std::uint16_t pipelineBucket) {
    if (!inBatch_) {
        return false;
    }
    if (passIndex != activePass_ || pipelineBucket != activePipeline_) {
        return false;
    }
    intents_.push_back(VulkanCommandIntent{
        .type = VulkanCommandIntentType::EndBatch,
        .passIndex = passIndex,
        .pipelineBucket = pipelineBucket,
    });
    inBatch_ = false;
    return true;
}

bool VulkanCommandBufferRecorder::ClearColor(const float rgba[4]) {
    if (!inBatch_ || rgba == nullptr) {
        return false;
    }

    VulkanCommandIntent intent{};
    intent.type = VulkanCommandIntentType::ClearColor;
    intent.passIndex = activePass_;
    intent.pipelineBucket = activePipeline_;
    intent.clearColor[0] = rgba[0];
    intent.clearColor[1] = rgba[1];
    intent.clearColor[2] = rgba[2];
    intent.clearColor[3] = rgba[3];
    intents_.push_back(intent);
    return true;
}

bool VulkanCommandBufferRecorder::SetViewProjection(const float viewProjection[16]) {
    if (!inBatch_ || viewProjection == nullptr) {
        return false;
    }
    VulkanCommandIntent intent{
        .type = VulkanCommandIntentType::SetViewProjection,
        .passIndex = activePass_,
        .pipelineBucket = activePipeline_,
    };
    std::memcpy(intent.viewProjection, viewProjection, sizeof(intent.viewProjection));
    intents_.push_back(intent);
    return true;
}

bool VulkanCommandBufferRecorder::DrawMesh(std::int32_t meshHandle,
                                           std::int32_t materialHandle,
                                           std::uint32_t firstIndex,
                                           std::uint32_t indexCount,
                                           std::uint32_t instanceCount,
                                           const float model[16]) {
    if (!inBatch_ || model == nullptr) {
        return false;
    }
    VulkanCommandIntent intent{
        .type = VulkanCommandIntentType::DrawMesh,
        .passIndex = activePass_,
        .pipelineBucket = activePipeline_,
        .meshHandle = meshHandle,
        .materialHandle = materialHandle,
        .firstIndex = firstIndex,
        .indexCount = indexCount,
        .instanceCount = instanceCount,
    };
    std::memcpy(intent.model, model, sizeof(intent.model));
    intents_.push_back(intent);
    return true;
}

const std::vector<VulkanCommandIntent>& VulkanCommandBufferRecorder::Intents() const noexcept {
    return intents_;
}

bool VulkanCommandBufferRecorder::InBatch() const noexcept {
    return inBatch_;
}

void VulkanCommandBufferRecorder::Reset() noexcept {
    inBatch_ = false;
    activePass_ = 0;
    activePipeline_ = 0;
    intents_.clear();
}

} // namespace ri::render::vulkan
