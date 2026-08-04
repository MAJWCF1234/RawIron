#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Scene/Helpers.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cstdint>
#include <iostream>
#include <string>

int main() {
#if !defined(_WIN32)
    std::cout << "VulkanNativeValidationProbe is Windows-only.\n";
    return 0;
#else
    ri::scene::Scene scene{"VulkanNativeValidationProbe"};
    const ri::scene::OrbitCameraHandles camera = ri::scene::AddOrbitCamera(
        scene,
        ri::scene::OrbitCameraOptions{
            .orbit = {
                .target = {0.0f, 0.0f, 0.0f},
                .distance = 5.0f,
                .yawDegrees = 180.0f,
                .pitchDegrees = -8.0f,
            },
        });
    ri::scene::AddPrimitiveNode(
        scene,
        ri::scene::PrimitiveNodeOptions{
            .nodeName = "OpaqueCube",
            .primitive = ri::scene::PrimitiveType::Cube,
            .baseColor = {0.70f, 0.35f, 0.18f},
        });
    ri::scene::AddPrimitiveNode(
        scene,
        ri::scene::PrimitiveNodeOptions{
            .nodeName = "TransparentCube",
            .primitive = ri::scene::PrimitiveType::Cube,
            .transform = {.position = {1.3f, 0.0f, 0.0f}},
            .baseColor = {0.18f, 0.45f, 0.85f},
            .opacity = 0.45f,
            .transparent = true,
        });

    HWND clientWindow = nullptr;
    ri::render::vulkan::VulkanPreviewWindowOptions options{};
    options.windowTitle = "Raw Iron Vulkan validation probe";
    options.showWindow = false;
    options.outClientHwnd = &clientWindow;
    options.enablePersistentPipelineWarmupCache = false;

    static const int firstSceneGeneration = 1;
    static const int secondSceneGeneration = 2;
    std::uint32_t frameCount = 0;
    const auto buildFrame = [&](ri::render::vulkan::VulkanNativeSceneFrame& frame, std::string*) {
        ++frameCount;
        frame.scene = &scene;
        // Change the identity only after submissions exist, exercising cache retirement.
        frame.sceneCacheIdentity = frameCount < 4 ? &firstSceneGeneration : &secondSceneGeneration;
        frame.frameSequence = frameCount;
        frame.suppressUnchangedFrames = false;
        frame.cameraNode = camera.cameraNode;
        frame.renderQualityTier = 0;
        if (frameCount >= 8 && clientWindow != nullptr) {
            PostMessageW(clientWindow, WM_CLOSE, 0, 0);
        }
        return true;
    };

    std::string error;
    if (!ri::render::vulkan::RunVulkanNativeSceneLoop(320, 200, buildFrame, options, &error)) {
        std::cerr << (error.empty() ? "Native Vulkan validation probe failed without a diagnostic." : error) << '\n';
        return 1;
    }
    if (frameCount < 8) {
        std::cerr << "Native Vulkan validation probe exited before eight frames.\n";
        return 1;
    }
    std::cout << "VulkanNativeValidationProbe frames=" << frameCount << "\n";
    return 0;
#endif
}
