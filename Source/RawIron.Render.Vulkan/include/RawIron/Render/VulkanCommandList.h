#pragma once

#include "RawIron/Core/RenderRecorder.h"

#include <cstdint>
#include <vector>

namespace ri::render::vulkan {

enum class VulkanRenderOpType : std::uint8_t {
    BeginBatch = 0,
    EndBatch = 1,
    ClearColor = 2,
    SetViewProjection = 3,
    DrawMesh = 4,
    Unknown = 5,
};

struct VulkanRenderOp {
    VulkanRenderOpType type = VulkanRenderOpType::Unknown;
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

class VulkanCommandListSink final : public ri::core::RenderCommandSink {
public:
    explicit VulkanCommandListSink(bool allowUnknownCommands = false);

    bool BeginBatch(const ri::core::RenderSubmissionBatch& batch) override;
    void EndBatch(const ri::core::RenderSubmissionBatch& batch) override;
    bool RecordClearColor(const ri::core::ClearColorCommand& command, const ri::core::RenderCommandHeader& header) override;
    bool RecordSetViewProjection(const ri::core::SetViewProjectionCommand& command,
                                 const ri::core::RenderCommandHeader& header) override;
    bool RecordDrawMesh(const ri::core::DrawMeshCommand& command, const ri::core::RenderCommandHeader& header) override;
    bool RecordUnknown(const ri::core::RenderCommandView& view) override;

    [[nodiscard]] const std::vector<VulkanRenderOp>& Operations() const noexcept;
    void Clear() noexcept;

private:
    bool allowUnknownCommands_ = false;
    std::uint8_t activePass_ = 0;
    std::uint16_t activePipeline_ = 0;
    std::vector<VulkanRenderOp> operations_{};
};

} // namespace ri::render::vulkan
