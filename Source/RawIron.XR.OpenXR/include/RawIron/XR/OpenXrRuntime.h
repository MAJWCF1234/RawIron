#pragma once

#include "RawIron/XR/HapticPolicy.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ri::xr {

enum class RuntimeAvailability {
    Uninitialized,
    LoaderUnavailable,
    RuntimeUnavailable,
    SystemReady,
};

struct ViewConfiguration {
    std::uint32_t recommendedWidth = 0;
    std::uint32_t recommendedHeight = 0;
    std::uint32_t recommendedSamples = 1;
};

struct RuntimeInfo {
    RuntimeAvailability availability = RuntimeAvailability::Uninitialized;
    std::string runtimeName{};
    std::uint64_t runtimeVersion = 0;
    std::string systemName{};
    std::uint32_t vendorId = 0;
    bool vulkanEnable2 = false;
    bool handTracking = false;
    bool stageSpace = false;
    std::uint64_t minimumVulkanApiVersion = 0;
    std::uint64_t maximumVulkanApiVersion = 0;
    std::vector<ViewConfiguration> stereoViews{};
    std::vector<std::string> warnings{};
};

struct VulkanSessionRunReport {
    std::uint32_t submittedFrames = 0;
    std::uint32_t validStereoFrames = 0;
    std::uint32_t leftControllerTrackedFrames = 0;
    std::uint32_t rightControllerTrackedFrames = 0;
    std::uint32_t focusedFrames = 0;
    std::uint32_t leftPoseActionActiveFrames = 0;
    std::uint32_t rightPoseActionActiveFrames = 0;
    std::uint32_t aimPoseBoundSourceCount = 0;
    std::uint32_t leftArticulatedHandFrames = 0;
    std::uint32_t rightArticulatedHandFrames = 0;
    std::uint32_t locomotionInputFrames = 0;
    std::uint32_t snapTurnCount = 0;
    std::uint32_t selectPressCount = 0;
    std::uint32_t dynamicSceneFrames = 0;
    std::uint32_t hapticPulseCount = 0;
    std::uint32_t jumpPressCount = 0;
    std::uint32_t teleportCount = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::int64_t colorFormat = 0;
    std::string leftInteractionProfile{};
    std::string rightInteractionProfile{};
};

struct HardwareSceneVertex {
    float position[3]{};
    /// World-space geometric normal. This is intentionally part of the shared scene stream so
    /// desktop and stereo hosts shade the same authored mesh rather than baking light into color.
    float normal[3]{0.0f, 1.0f, 0.0f};
    float color[3]{1.0f, 1.0f, 1.0f};
    float texCoord[2]{};
    /// Normalized UV rectangle in the shared material atlas. An empty rectangle means no albedo map.
    float atlasRect[4]{};
    /// Normalized UV rectangle for a tangent-space normal map packed into the same atlas.
    float normalAtlasRect[4]{};
    /// x=metallic, y=roughness, z=normal-scale-x, w=normal-scale-y.
    float materialParams[4]{0.0f, 1.0f, 1.0f, 1.0f};
};

struct HardwareLocomotionInput {
    float moveRight = 0.0f;
    float moveForward = 0.0f;
    float deltaSeconds = 0.0f;
    float viewForward[3]{0.0f, 0.0f, 1.0f};
    float viewRight[3]{1.0f, 0.0f, 0.0f};
    bool jumpPressed = false;
};

struct HardwareInteractionHandInput {
    bool tracked = false;
    bool selectHeld = false;
    bool selectPressed = false;
    bool selectReleased = false;
    bool grabHeld = false;
    bool grabPressed = false;
    bool grabReleased = false;
    bool teleportHeld = false;
    bool teleportPressed = false;
    bool teleportReleased = false;
    float aimOrigin[3]{};
    float aimDirection[3]{0.0f, 0.0f, 1.0f};
};

enum class HardwareTurnMode : std::uint8_t {
    Smooth = 0,
    Snap,
};

struct HardwareInteractionFrameInput {
    float deltaSeconds = 0.0f;
    HardwareInteractionHandInput hands[2]{};
};

struct HardwareInteractionFrameOutput {
    const HardwareSceneVertex* vertices = nullptr;
    std::size_t vertexCount = 0;
    float hapticAmplitude[2]{};
    float hapticDurationSeconds[2]{};
    HapticEvent hapticEvent[2]{};
    bool teleportRequested = false;
    float teleportDestinationFeet[3]{};
};

using HardwareLocomotionResolver = void (*)(
    void* user,
    const HardwareLocomotionInput& input,
    float origin[3],
    float& yawDegrees);

using HardwareInteractionUpdater = HardwareInteractionFrameOutput (*)(
    void* user,
    const HardwareInteractionFrameInput& input);

/// Immutable world-space triangle stream consumed directly by the OpenXR Vulkan bridge.
/// Coordinates use Raw Iron's +X right, +Y up, +Z forward convention; `origin` places
/// the tracked stage inside that world without changing authored scene data.
struct HardwareSceneView {
    const HardwareSceneVertex* vertices = nullptr;
    std::size_t vertexCount = 0;
    float origin[3]{0.0f, 0.0f, 0.0f};
    float nearClip = 0.05f;
    float farClip = 500.0f;
    const std::uint8_t* textureRgba = nullptr;
    std::uint32_t textureWidth = 0;
    std::uint32_t textureHeight = 0;
    void* locomotionUser = nullptr;
    HardwareLocomotionResolver resolveLocomotion = nullptr;
    std::size_t dynamicVertexCapacity = 0;
    void* interactionUser = nullptr;
    HardwareInteractionUpdater updateInteraction = nullptr;
    HardwareTurnMode turnMode = HardwareTurnMode::Smooth;
    float smoothTurnDegreesPerSecond = 120.0f;
    float snapTurnDegrees = 30.0f;
};

class OpenXrVulkanSession;

/// Owns OpenXR loader/instance/system discovery and Raw Iron's standard VR action schema.
/// Graphics-session and swapchain creation intentionally live in the Vulkan XR bridge, not here.
class OpenXrRuntime final {
public:
    OpenXrRuntime();
    ~OpenXrRuntime();
    OpenXrRuntime(OpenXrRuntime&&) noexcept;
    OpenXrRuntime& operator=(OpenXrRuntime&&) noexcept;
    OpenXrRuntime(const OpenXrRuntime&) = delete;
    OpenXrRuntime& operator=(const OpenXrRuntime&) = delete;

    /// Initializes the loader, HMD system, stereo-view contract, and standard hand actions.
    /// RuntimeUnavailable is a normal result on machines without an active OpenXR runtime.
    [[nodiscard]] bool Initialize(std::string_view applicationName, std::string& error);
    void Shutdown() noexcept;
    void PollEvents();

    [[nodiscard]] const RuntimeInfo& Info() const noexcept;
    [[nodiscard]] bool IsSystemReady() const noexcept;

private:
    friend class OpenXrVulkanSession;
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Vulkan/OpenXR session bridge used by headset hosts. It creates graphics through
/// XR_KHR_vulkan_enable2, owns stereo swapchains, and submits projection frames.
class OpenXrVulkanSession final {
public:
    explicit OpenXrVulkanSession(OpenXrRuntime& runtime) noexcept;

    /// Runs a bounded live headset frame loop. Swapchain images are cleared by Vulkan;
    /// native scene rendering can replace this clear pass without changing XR ownership.
    [[nodiscard]] bool RunFrames(std::uint32_t maximumFrames,
                                 VulkanSessionRunReport& report,
                                 std::string& error,
                                 const HardwareSceneView* scene = nullptr);

private:
    OpenXrRuntime* runtime_ = nullptr;
};

} // namespace ri::xr
