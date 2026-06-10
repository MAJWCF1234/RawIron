#pragma once

#include "RawIron/Render/PostProcessProfiles.h"
#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Render/VulkanPreviewPresenter.h"

namespace ri::render::vulkan {

/// Copies software preview atmosphere (`rendering.riscript` keys) into a native Vulkan frame.
void ApplyScenePreviewAtmosphereToVulkanFrame(const ri::render::software::ScenePreviewOptions& preview,
                                              VulkanNativeSceneFrame& frame);

/// Merges lightweight editor preview post scalars into native composite parameters.
void OverlayScenePreviewPostProcessOnParameters(const ri::render::software::ScenePreviewOptions& preview,
                                                ri::render::PostProcessParameters& postProcess);

} // namespace ri::render::vulkan
