#pragma once

#include "RawIron/Render/VulkanCommandRecorder.h"

#include <cstdint>
#include <vector>

namespace ri::render::vulkan {

enum class VulkanCommandIntentType : std::uint8_t {
    BeginBatch = 0,
    EndBatch = 1,
    ClearColor = 2,
    SetViewProjection = 3,
    DrawMesh = 4,
};

struct VulkanCommandIntent {
    VulkanCommandIntentType type = VulkanCommandIntentType::SetViewProjection;
    std::uint8_t passIndex = 0;
    std::uint16_t pipelineBucket = 0;
    std::int32_t meshHandle = -1;
    std::int32_t materialHandle = -1;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t instanceCount = 0;
    float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float viewProjection[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    float model[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

class VulkanCommandBufferRecorder final : public VulkanBackendRecorder {
public:
    bool BeginBatch(std::uint8_t passIndex, std::uint16_t pipelineBucket) override;
    bool EndBatch(std::uint8_t passIndex, std::uint16_t pipelineBucket) override;
    bool ClearColor(const float rgba[4]) override;
    bool SetViewProjection(const float viewProjection[16]) override;
    bool DrawMesh(std::int32_t meshHandle,
                  std::int32_t materialHandle,
                  std::uint32_t firstIndex,
                  std::uint32_t indexCount,
                  std::uint32_t instanceCount,
                  const float model[16]) override;

    [[nodiscard]] const std::vector<VulkanCommandIntent>& Intents() const noexcept;
    [[nodiscard]] bool InBatch() const noexcept;
    void Reset() noexcept;

private:
    bool inBatch_ = false;
    std::uint8_t activePass_ = 0;
    std::uint16_t activePipeline_ = 0;
    std::vector<VulkanCommandIntent> intents_{};
};

} // namespace ri::render::vulkan
