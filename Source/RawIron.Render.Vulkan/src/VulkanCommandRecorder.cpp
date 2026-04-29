#include "RawIron/Render/VulkanCommandRecorder.h"

namespace ri::render::vulkan {

bool RecordVulkanCommandList(const std::vector<VulkanRenderOp>& operations, VulkanBackendRecorder& recorder) {
    for (const VulkanRenderOp& operation : operations) {
        switch (operation.type) {
        case VulkanRenderOpType::BeginBatch:
            if (!recorder.BeginBatch(operation.passIndex, operation.pipelineBucket)) {
                return false;
            }
            break;
        case VulkanRenderOpType::EndBatch:
            if (!recorder.EndBatch(operation.passIndex, operation.pipelineBucket)) {
                return false;
            }
            break;
        case VulkanRenderOpType::ClearColor:
            if (!recorder.ClearColor(operation.clearColor)) {
                return false;
            }
            break;
        case VulkanRenderOpType::SetViewProjection:
            if (!recorder.SetViewProjection(operation.viewProjection)) {
                return false;
            }
            break;
        case VulkanRenderOpType::DrawMesh:
            if (!recorder.DrawMesh(operation.meshHandle,
                                   operation.materialHandle,
                                   operation.firstIndex,
                                   operation.indexCount,
                                   operation.instanceCount,
                                   operation.model)) {
                return false;
            }
            break;
        case VulkanRenderOpType::Unknown:
        default:
            return false;
        }
    }
    return true;
}

} // namespace ri::render::vulkan
