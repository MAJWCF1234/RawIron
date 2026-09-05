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
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
#if !defined(_WIN32)
    std::cout << "VulkanNativeValidationProbe is Windows-only.\n";
    return 0;
#else
    const bool lumaCurveProbe = argc >= 3 && std::string_view(argv[2]) == "--luma-curve";
    const bool normalFrameProbe = argc >= 5 && std::string_view(argv[2]) == "--normal-frame";
    const int probeWidth=normalFrameProbe && argc>=7 ? std::stoi(argv[6]) : 320;
    const int probeHeight=probeWidth*5/8;
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
            .shadingModel = lumaCurveProbe ? ri::scene::ShadingModel::Unlit : ri::scene::ShadingModel::Lit,
            .baseColor = lumaCurveProbe ? ri::math::Vec3{0,0,0} : ri::math::Vec3{0.70f,0.35f,0.18f},
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

    int cameraNode = camera.cameraNode;
    if (normalFrameProbe) {
        scene = ri::scene::Scene("NormalFrameProbe");
        cameraNode = scene.CreateNode("Camera");
        scene.GetNode(cameraNode).localTransform.position={0,0,-5};
        scene.AttachCamera(cameraNode,scene.AddCamera({}));
        const std::string_view mode=argv[3];
        const bool mirrorU=mode=="mirror-u", mirrorV=mode=="mirror-v";
        const bool degenerate=mode=="degenerate";
        const bool analytic=argc>=6 && std::string_view(argv[5])=="--analytic";
        auto mesh=ri::scene::MakeBillboardQuadMesh("NormalFramePanel");
        for (auto& uv:mesh.texCoords) { if(mirrorU) uv.x=1-uv.x; if(mirrorV) uv.y=1-uv.y; if(degenerate) uv={0,0}; }
        // Analytic world normal is independent of the shader tangent reconstruction.
        const auto expected=ri::math::Normalize(ri::math::Vec3{
            (166.f/255*2-1)*(mirrorU ? -1.f : 1.f),
            (191.f/255*2-1)*(mirrorV ? -1.f : 1.f),-(230.f/255*2-1)});
        mesh.normals.assign(4,analytic && !degenerate ? expected : ri::math::Vec3{0,0,-1});
        const int panel=scene.CreateNode("Panel");
        for (auto& position : mesh.positions) position=position*4;
        ri::scene::Material material{};
        material.baseColor={.45f,.45f,.45f}; material.roughness=1; material.doubleSided=true;
        if (!analytic) material.normalTexture=argv[4];
        scene.AttachMesh(panel,scene.AddMesh(mesh),scene.AddMaterial(material));
        ri::scene::LightNodeOptions light{};
        light.transform.rotationDegrees={35,-25,0}; light.light.intensity=.7f;
        ri::scene::AddLightNode(scene,light);
    }

    HWND clientWindow = nullptr;
    ri::render::vulkan::VulkanPreviewWindowOptions options{};
    if (argc >= 2) options.captureFirstFramePath = argv[1];
    options.enableHybridHdrPresentation = argc >= 4 && std::string_view(argv[3]) == "--hybrid-hdr";
    options.enableExtendedPostProcessShader = argc >= 5 && std::string_view(argv[4]) == "--extended-post";
    if (normalFrameProbe) options.textureRoot=std::filesystem::path(argv[4]).parent_path();
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
        frame.cameraNode = cameraNode;
        frame.renderQualityTier = normalFrameProbe && argc>=8 ? std::stoi(argv[7]) : 0;
        if (normalFrameProbe) {
            frame.renderFogDensity=frame.renderFogStrength=0;
            frame.nativeAmbientLight={0,0,0};
        }
        if (lumaCurveProbe) {
            // Negative RGB after contrast must remain black through the luma curve,
            // in the direct shader and both hybrid composite shader variants.
            frame.renderContrast = 1.08f;
            frame.renderFogDensity = frame.renderFogStrength = 0.0f;
            frame.postProcess.toneCurveStrength = 0.06f;
        }
        if (frameCount >= 8 && clientWindow != nullptr) {
            PostMessageW(clientWindow, WM_CLOSE, 0, 0);
        }
        return true;
    };

    std::string error;
    if (!ri::render::vulkan::RunVulkanNativeSceneLoop(probeWidth, probeHeight, buildFrame, options, &error)) {
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
