#pragma once

#include "RawIron/Render/VulkanCommandList.h"

namespace ri::render::vulkan {

class VulkanBackendRecorder {
public:
    virtual ~VulkanBackendRecorder() = default;

    virtual bool BeginBatch(std::uint8_t passIndex, std::uint16_t pipelineBucket) = 0;
    virtual bool EndBatch(std::uint8_t passIndex, std::uint16_t pipelineBucket) = 0;
    virtual bool ClearColor(const float rgba[4]) = 0;
    virtual bool SetViewProjection(const float viewProjection[16]) = 0;
    virtual bool DrawMesh(std::int32_t meshHandle,
                          std::int32_t materialHandle,
                          std::uint32_t firstIndex,
                          std::uint32_t indexCount,
                          std::uint32_t instanceCount,
                          const float model[16]) = 0;
};

bool RecordVulkanCommandList(const std::vector<VulkanRenderOp>& operations, VulkanBackendRecorder& recorder);

} // namespace ri::render::vulkan
