#include "RawIron/XR/OpenXrRuntime.h"

#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

namespace ri::xr {
namespace {

constexpr const char* kVulkanEnable2Extension = "XR_KHR_vulkan_enable2";
constexpr const char* kHandTrackingExtension = "XR_EXT_hand_tracking";

using Matrix4 = std::array<float, 16>;

/// Per-eye data shared by the vertex transform and the forward material pass. Keeping the camera
/// position in the same push block avoids a second per-eye descriptor update in the VR hot path.
struct alignas(16) XrEyePushConstants {
    Matrix4 viewProjection{};
    std::array<float, 4> cameraWorldPosition{};
};
static_assert(sizeof(XrEyePushConstants) == 80U, "XrScene EyeData must remain std430-compatible.");

Matrix4 IdentityMatrix() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

Matrix4 MultiplyMatrix(const Matrix4& left, const Matrix4& right) {
    Matrix4 result{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t inner = 0; inner < 4; ++inner) {
                result[row * 4 + column] += left[row * 4 + inner] * right[inner * 4 + column];
            }
        }
    }
    return result;
}

std::array<float, 16> ToShaderColumnMajor(const Matrix4& matrix) {
    std::array<float, 16> output{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            output[column * 4 + row] = matrix[row * 4 + column];
        }
    }
    return output;
}

Matrix4 BuildWorldToAppMatrix(const std::array<float, 3>& origin, const float yawDegrees) {
    Matrix4 matrix = IdentityMatrix();
    matrix[2 * 4 + 2] = -1.0f;
    matrix[0 * 4 + 3] = -origin[0];
    matrix[1 * 4 + 3] = -origin[1];
    matrix[2 * 4 + 3] = origin[2];
    const float radians = yawDegrees * 0.01745329251994329577f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    Matrix4 rotation = IdentityMatrix();
    rotation[0] = cosine;
    rotation[2] = sine;
    rotation[8] = -sine;
    rotation[10] = cosine;
    return MultiplyMatrix(rotation, matrix);
}

std::array<float, 3> RotateVector(const XrQuaternionf& q, const std::array<float, 3>& vector) {
    const std::array<float, 3> u{q.x, q.y, q.z};
    const float dotUv = u[0] * vector[0] + u[1] * vector[1] + u[2] * vector[2];
    const float dotUu = u[0] * u[0] + u[1] * u[1] + u[2] * u[2];
    const std::array<float, 3> cross{
        u[1] * vector[2] - u[2] * vector[1],
        u[2] * vector[0] - u[0] * vector[2],
        u[0] * vector[1] - u[1] * vector[0]};
    return {
        2.0f * dotUv * u[0] + (q.w * q.w - dotUu) * vector[0] + 2.0f * q.w * cross[0],
        2.0f * dotUv * u[1] + (q.w * q.w - dotUu) * vector[1] + 2.0f * q.w * cross[1],
        2.0f * dotUv * u[2] + (q.w * q.w - dotUu) * vector[2] + 2.0f * q.w * cross[2]};
}

std::array<float, 3> AppToWorldPoint(const std::array<float, 3>& appPoint,
                                     const std::array<float, 3>& origin,
                                     const float yawDegrees) {
    const float radians = -yawDegrees * 0.01745329251994329577f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float rotatedX = cosine * appPoint[0] + sine * appPoint[2];
    const float rotatedZ = -sine * appPoint[0] + cosine * appPoint[2];
    return {origin[0] + rotatedX, origin[1] + appPoint[1], origin[2] - rotatedZ};
}

std::array<float, 3> AppToWorldVector(const std::array<float, 3>& appVector,
                                      const float yawDegrees) {
    const float radians = -yawDegrees * 0.01745329251994329577f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float rotatedX = cosine * appVector[0] + sine * appVector[2];
    const float rotatedZ = -sine * appVector[0] + cosine * appVector[2];
    return {rotatedX, appVector[1], -rotatedZ};
}

std::uint32_t BuildTrackedControllerVertices(
    const XrSpaceLocation& location,
    const std::array<float, 3>& origin,
    const float yawDegrees,
    const std::array<float, 3>& color,
    HardwareSceneVertex* output) {
    constexpr std::array<std::array<float, 3>, 8> corners{{
        {-0.035f, -0.025f, 0.025f}, {0.035f, -0.025f, 0.025f},
        {0.035f, 0.025f, 0.025f}, {-0.035f, 0.025f, 0.025f},
        {-0.035f, -0.025f, -0.16f}, {0.035f, -0.025f, -0.16f},
        {0.035f, 0.025f, -0.16f}, {-0.035f, 0.025f, -0.16f}}};
    constexpr std::array<std::array<std::uint32_t, 3>, 12> triangles{{
        {4, 5, 6}, {4, 6, 7}, {1, 0, 3}, {1, 3, 2},
        {0, 4, 7}, {0, 7, 3}, {5, 1, 2}, {5, 2, 6},
        {3, 7, 6}, {3, 6, 2}, {0, 1, 5}, {0, 5, 4}}};
    std::array<std::array<float, 3>, 8> worldCorners{};
    for (std::size_t index = 0; index < corners.size(); ++index) {
        const std::array<float, 3> rotated = RotateVector(location.pose.orientation, corners[index]);
        const std::array<float, 3> appPoint{
            location.pose.position.x + rotated[0],
            location.pose.position.y + rotated[1],
            location.pose.position.z + rotated[2]};
        worldCorners[index] = AppToWorldPoint(appPoint, origin, yawDegrees);
    }
    std::uint32_t vertexCount = 0;
    for (const auto& triangle : triangles) {
        const auto& a = worldCorners[triangle[0]];
        const auto& b = worldCorners[triangle[1]];
        const auto& c = worldCorners[triangle[2]];
        const std::array<float, 3> edgeOne{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const std::array<float, 3> edgeTwo{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        std::array<float, 3> normal{
            edgeOne[1] * edgeTwo[2] - edgeOne[2] * edgeTwo[1],
            edgeOne[2] * edgeTwo[0] - edgeOne[0] * edgeTwo[2],
            edgeOne[0] * edgeTwo[1] - edgeOne[1] * edgeTwo[0]};
        const float normalLengthSquared = normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2];
        if (normalLengthSquared > 1.0e-10f) {
            const float inverseLength = 1.0f / std::sqrt(normalLengthSquared);
            normal[0] *= inverseLength;
            normal[1] *= inverseLength;
            normal[2] *= inverseLength;
        } else {
            normal = {0.0f, 1.0f, 0.0f};
        }
        for (const std::uint32_t corner : triangle) {
            HardwareSceneVertex& vertex = output[vertexCount++];
            vertex.position[0] = worldCorners[corner][0];
            vertex.position[1] = worldCorners[corner][1];
            vertex.position[2] = worldCorners[corner][2];
            vertex.normal[0] = normal[0];
            vertex.normal[1] = normal[1];
            vertex.normal[2] = normal[2];
            vertex.color[0] = color[0];
            vertex.color[1] = color[1];
            vertex.color[2] = color[2];
            vertex.materialParams[0] = 0.0f;
            vertex.materialParams[1] = 0.28f;
            vertex.materialParams[2] = 1.0f;
            vertex.materialParams[3] = 1.0f;
        }
    }
    return vertexCount;
}

Matrix4 BuildInversePoseMatrix(const XrPosef& pose) {
    const float x = pose.orientation.x;
    const float y = pose.orientation.y;
    const float z = pose.orientation.z;
    const float w = pose.orientation.w;
    Matrix4 rotation = IdentityMatrix();
    rotation[0] = 1.0f - 2.0f * (y * y + z * z);
    rotation[1] = 2.0f * (x * y + z * w);
    rotation[2] = 2.0f * (x * z - y * w);
    rotation[4] = 2.0f * (x * y - z * w);
    rotation[5] = 1.0f - 2.0f * (x * x + z * z);
    rotation[6] = 2.0f * (y * z + x * w);
    rotation[8] = 2.0f * (x * z + y * w);
    rotation[9] = 2.0f * (y * z - x * w);
    rotation[10] = 1.0f - 2.0f * (x * x + y * y);
    rotation[3] = -(rotation[0] * pose.position.x + rotation[1] * pose.position.y
                    + rotation[2] * pose.position.z);
    rotation[7] = -(rotation[4] * pose.position.x + rotation[5] * pose.position.y
                    + rotation[6] * pose.position.z);
    rotation[11] = -(rotation[8] * pose.position.x + rotation[9] * pose.position.y
                     + rotation[10] * pose.position.z);
    return rotation;
}

Matrix4 BuildProjectionMatrix(const XrFovf& fov, const float nearClip, const float farClip) {
    const float tanLeft = std::tan(fov.angleLeft);
    const float tanRight = std::tan(fov.angleRight);
    const float tanDown = std::tan(fov.angleDown);
    const float tanUp = std::tan(fov.angleUp);
    const float width = tanRight - tanLeft;
    const float height = tanUp - tanDown;
    Matrix4 projection{};
    projection[0] = 2.0f / width;
    projection[2] = (tanRight + tanLeft) / width;
    projection[5] = -2.0f / height;
    projection[6] = -(tanUp + tanDown) / height;
    projection[10] = -farClip / (farClip - nearClip);
    projection[11] = -(farClip * nearClip) / (farClip - nearClip);
    projection[14] = -1.0f;
    return projection;
}

std::optional<std::uint32_t> FindMemoryType(const VkPhysicalDevice physicalDevice,
                                            const std::uint32_t typeBits,
                                            const VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
        if ((typeBits & (1U << index)) != 0U
            && (properties.memoryTypes[index].propertyFlags & flags) == flags) {
            return index;
        }
    }
    return std::nullopt;
}

std::vector<std::uint32_t> ReadSpirv(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return {};
    const std::streamsize byteCount = stream.tellg();
    if (byteCount <= 0 || (byteCount % 4) != 0) return {};
    std::vector<std::uint32_t> words(static_cast<std::size_t>(byteCount) / 4U);
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(words.data()), byteCount);
    return stream ? words : std::vector<std::uint32_t>{};
}

template <std::size_t Size>
void CopyOpenXrName(char (&destination)[Size], const std::string_view source) {
    static_assert(Size > 0U);
    const std::size_t count = std::min(source.size(), Size - 1U);
    std::memcpy(destination, source.data(), count);
    destination[count] = '\0';
}

std::string ResultText(const XrInstance instance, const XrResult result) {
    std::array<char, XR_MAX_RESULT_STRING_SIZE> text{};
    if (instance != XR_NULL_HANDLE && XR_SUCCEEDED(xrResultToString(instance, result, text.data()))) {
        return text.data();
    }
    return "OpenXR result " + std::to_string(static_cast<int>(result));
}

bool HasExtension(const std::vector<XrExtensionProperties>& extensions, const char* name) {
    return std::ranges::any_of(extensions, [name](const XrExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

} // namespace

class OpenXrRuntime::Impl {
public:
    ~Impl() { Reset(); }

    RuntimeInfo info{};
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrActionSet gameplayActions = XR_NULL_HANDLE;
    XrAction gripPose = XR_NULL_HANDLE;
    XrAction aimPose = XR_NULL_HANDLE;
    XrAction select = XR_NULL_HANDLE;
    XrAction squeeze = XR_NULL_HANDLE;
    XrAction thumbstick = XR_NULL_HANDLE;
    XrAction teleport = XR_NULL_HANDLE;
    XrAction jump = XR_NULL_HANDLE;
    XrAction haptic = XR_NULL_HANDLE;

    void Reset() noexcept {
        if (gameplayActions != XR_NULL_HANDLE) {
            xrDestroyActionSet(gameplayActions);
        }
        if (instance != XR_NULL_HANDLE) {
            xrDestroyInstance(instance);
        }
        instance = XR_NULL_HANDLE;
        system = XR_NULL_SYSTEM_ID;
        gameplayActions = XR_NULL_HANDLE;
        gripPose = aimPose = XR_NULL_HANDLE;
        select = squeeze = thumbstick = teleport = jump = haptic = XR_NULL_HANDLE;
        info = {};
    }

    bool CreateAction(const char* name,
                      const char* localizedName,
                      const XrActionType type,
                      const std::vector<XrPath>& subactions,
                      XrAction& output,
                      std::string& error) {
        XrActionCreateInfo createInfo{XR_TYPE_ACTION_CREATE_INFO};
        CopyOpenXrName(createInfo.actionName, name);
        CopyOpenXrName(createInfo.localizedActionName, localizedName);
        createInfo.actionType = type;
        createInfo.countSubactionPaths = static_cast<std::uint32_t>(subactions.size());
        createInfo.subactionPaths = subactions.empty() ? nullptr : subactions.data();
        const XrResult result = xrCreateAction(gameplayActions, &createInfo, &output);
        if (XR_FAILED(result)) {
            error = "xrCreateAction(" + std::string(name) + ") failed: " + ResultText(instance, result);
            return false;
        }
        return true;
    }
};

OpenXrRuntime::OpenXrRuntime() : impl_(std::make_unique<Impl>()) {}
OpenXrRuntime::~OpenXrRuntime() { Shutdown(); }
OpenXrRuntime::OpenXrRuntime(OpenXrRuntime&&) noexcept = default;
OpenXrRuntime& OpenXrRuntime::operator=(OpenXrRuntime&&) noexcept = default;

bool OpenXrRuntime::Initialize(const std::string_view applicationName, std::string& error) {
    Shutdown();

    std::uint32_t extensionCount = 0;
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    if (result == XR_ERROR_RUNTIME_UNAVAILABLE || result == XR_ERROR_INITIALIZATION_FAILED) {
        impl_->info.availability = RuntimeAvailability::RuntimeUnavailable;
        error = "No active OpenXR runtime was found. Set SteamVR as the current OpenXR runtime and retry.";
        return false;
    }
    if (XR_FAILED(result)) {
        impl_->info.availability = RuntimeAvailability::LoaderUnavailable;
        error = "OpenXR loader discovery failed: " + ResultText(XR_NULL_HANDLE, result);
        return false;
    }

    std::vector<XrExtensionProperties> extensions(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
    result = xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.data());
    if (XR_FAILED(result)) {
        impl_->info.availability = RuntimeAvailability::LoaderUnavailable;
        error = "OpenXR extension enumeration failed: " + ResultText(XR_NULL_HANDLE, result);
        return false;
    }
    impl_->info.vulkanEnable2 = HasExtension(extensions, kVulkanEnable2Extension);
    impl_->info.handTracking = HasExtension(extensions, kHandTrackingExtension);
    if (!impl_->info.vulkanEnable2) {
        impl_->info.availability = RuntimeAvailability::RuntimeUnavailable;
        error = "The active OpenXR runtime does not expose XR_KHR_vulkan_enable2, required by Raw Iron.";
        return false;
    }

    std::vector<const char*> enabledExtensions{kVulkanEnable2Extension};
    if (impl_->info.handTracking) {
        enabledExtensions.push_back(kHandTrackingExtension);
    }
    XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    const std::string safeName = applicationName.empty() ? "Raw Iron" : std::string(applicationName);
    CopyOpenXrName(instanceInfo.applicationInfo.applicationName, safeName);
    CopyOpenXrName(instanceInfo.applicationInfo.engineName, "Raw Iron");
    instanceInfo.applicationInfo.applicationVersion = 1;
    instanceInfo.applicationInfo.engineVersion = 1;
    // Raw Iron currently uses the OpenXR 1.0 core plus explicitly negotiated extensions.
    // Requesting the header's newest version unnecessarily rejects otherwise conformant
    // SteamVR runtimes before extension discovery can do its job.
    instanceInfo.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    instanceInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledExtensions.size());
    instanceInfo.enabledExtensionNames = enabledExtensions.data();
    result = xrCreateInstance(&instanceInfo, &impl_->instance);
    if (XR_FAILED(result)) {
        impl_->info.availability = RuntimeAvailability::RuntimeUnavailable;
        error = "xrCreateInstance failed: " + ResultText(XR_NULL_HANDLE, result);
        return false;
    }

    XrInstanceProperties runtimeProperties{XR_TYPE_INSTANCE_PROPERTIES};
    if (XR_SUCCEEDED(xrGetInstanceProperties(impl_->instance, &runtimeProperties))) {
        impl_->info.runtimeName = runtimeProperties.runtimeName;
        impl_->info.runtimeVersion = runtimeProperties.runtimeVersion;
    }

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    result = xrGetSystem(impl_->instance, &systemInfo, &impl_->system);
    if (result == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
        impl_->info.availability = RuntimeAvailability::RuntimeUnavailable;
        error = "The OpenXR runtime is active, but no head-mounted display is available.";
        return false;
    }
    if (XR_FAILED(result)) {
        error = "xrGetSystem failed: " + ResultText(impl_->instance, result);
        return false;
    }

    XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
    result = xrGetSystemProperties(impl_->instance, impl_->system, &properties);
    if (XR_FAILED(result)) {
        error = "xrGetSystemProperties failed: " + ResultText(impl_->instance, result);
        return false;
    }
    impl_->info.systemName = properties.systemName;
    impl_->info.vendorId = properties.vendorId;

    PFN_xrGetVulkanGraphicsRequirements2KHR getVulkanRequirements = nullptr;
    result = xrGetInstanceProcAddr(
        impl_->instance,
        "xrGetVulkanGraphicsRequirements2KHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&getVulkanRequirements));
    if (XR_FAILED(result) || getVulkanRequirements == nullptr) {
        error = "The OpenXR runtime exposed Vulkan enable2 but not xrGetVulkanGraphicsRequirements2KHR.";
        return false;
    }
    XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    result = getVulkanRequirements(impl_->instance, impl_->system, &graphicsRequirements);
    if (XR_FAILED(result)) {
        error = "OpenXR Vulkan requirements query failed: " + ResultText(impl_->instance, result);
        return false;
    }
    impl_->info.minimumVulkanApiVersion = graphicsRequirements.minApiVersionSupported;
    impl_->info.maximumVulkanApiVersion = graphicsRequirements.maxApiVersionSupported;

    std::uint32_t viewCount = 0;
    result = xrEnumerateViewConfigurationViews(
        impl_->instance, impl_->system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    if (XR_FAILED(result) || viewCount != 2U) {
        error = XR_FAILED(result)
            ? "Stereo view discovery failed: " + ResultText(impl_->instance, result)
            : "Raw Iron requires a two-view primary stereo configuration.";
        return false;
    }
    std::vector<XrViewConfigurationView> views(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    result = xrEnumerateViewConfigurationViews(
        impl_->instance, impl_->system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        viewCount, &viewCount, views.data());
    if (XR_FAILED(result)) {
        error = "Stereo view query failed: " + ResultText(impl_->instance, result);
        return false;
    }
    for (const XrViewConfigurationView& view : views) {
        impl_->info.stereoViews.push_back({
            view.recommendedImageRectWidth,
            view.recommendedImageRectHeight,
            view.recommendedSwapchainSampleCount});
    }

    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    CopyOpenXrName(actionSetInfo.actionSetName, "raw_iron_gameplay");
    CopyOpenXrName(actionSetInfo.localizedActionSetName, "Raw Iron Gameplay");
    actionSetInfo.priority = 0;
    result = xrCreateActionSet(impl_->instance, &actionSetInfo, &impl_->gameplayActions);
    if (XR_FAILED(result)) {
        error = "xrCreateActionSet failed: " + ResultText(impl_->instance, result);
        return false;
    }

    XrPath left = XR_NULL_PATH;
    XrPath right = XR_NULL_PATH;
    result = xrStringToPath(impl_->instance, "/user/hand/left", &left);
    if (XR_FAILED(result)) {
        error = "Could not create the left-hand OpenXR subaction path: " + ResultText(impl_->instance, result);
        return false;
    }
    result = xrStringToPath(impl_->instance, "/user/hand/right", &right);
    if (XR_FAILED(result)) {
        error = "Could not create the right-hand OpenXR subaction path: " + ResultText(impl_->instance, result);
        return false;
    }
    const std::vector<XrPath> bothHands{left, right};
    if (!impl_->CreateAction("grip_pose", "Grip Pose", XR_ACTION_TYPE_POSE_INPUT, bothHands, impl_->gripPose, error)
        || !impl_->CreateAction("aim_pose", "Aim Pose", XR_ACTION_TYPE_POSE_INPUT, bothHands, impl_->aimPose, error)
        || !impl_->CreateAction("select", "Select", XR_ACTION_TYPE_BOOLEAN_INPUT, bothHands, impl_->select, error)
        || !impl_->CreateAction("squeeze", "Squeeze", XR_ACTION_TYPE_FLOAT_INPUT, bothHands, impl_->squeeze, error)
        || !impl_->CreateAction("thumbstick", "Move and Turn", XR_ACTION_TYPE_VECTOR2F_INPUT, bothHands, impl_->thumbstick, error)
        || !impl_->CreateAction("teleport", "Teleport", XR_ACTION_TYPE_BOOLEAN_INPUT, bothHands, impl_->teleport, error)
        || !impl_->CreateAction("jump", "Jump", XR_ACTION_TYPE_BOOLEAN_INPUT, bothHands, impl_->jump, error)
        || !impl_->CreateAction("haptic", "Haptic Output", XR_ACTION_TYPE_VIBRATION_OUTPUT, bothHands, impl_->haptic, error)) {
        return false;
    }

    const auto xrPath = [&](const char* path, XrPath& output) {
        const XrResult pathResult = xrStringToPath(impl_->instance, path, &output);
        if (XR_FAILED(pathResult)) {
            impl_->info.warnings.push_back(
                "OpenXR rejected binding path " + std::string(path) + ": "
                + ResultText(impl_->instance, pathResult));
            return false;
        }
        return true;
    };
    const auto suggestProfile = [&](const char* profile,
                                    const std::vector<std::pair<XrAction, const char*>>& requestedBindings) {
        XrPath profilePath = XR_NULL_PATH;
        if (!xrPath(profile, profilePath)) {
            return;
        }
        std::vector<XrActionSuggestedBinding> bindings;
        bindings.reserve(requestedBindings.size());
        for (const auto& [action, bindingName] : requestedBindings) {
            XrPath bindingPath = XR_NULL_PATH;
            if (xrPath(bindingName, bindingPath)) {
                bindings.push_back({action, bindingPath});
            }
        }
        const XrInteractionProfileSuggestedBinding suggestion{
            XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
            nullptr,
            profilePath,
            static_cast<std::uint32_t>(bindings.size()),
            bindings.data()};
        const XrResult suggestResult = xrSuggestInteractionProfileBindings(impl_->instance, &suggestion);
        if (XR_FAILED(suggestResult)) {
            impl_->info.warnings.push_back(
                "Interaction profile " + std::string(profile) + " was not accepted: "
                + ResultText(impl_->instance, suggestResult));
        }
    };

    suggestProfile("/interaction_profiles/valve/index_controller", {
        {impl_->gripPose, "/user/hand/left/input/grip/pose"},
        {impl_->gripPose, "/user/hand/right/input/grip/pose"},
        {impl_->aimPose, "/user/hand/left/input/aim/pose"},
        {impl_->aimPose, "/user/hand/right/input/aim/pose"},
        {impl_->select, "/user/hand/left/input/trigger/click"},
        {impl_->select, "/user/hand/right/input/trigger/click"},
        {impl_->squeeze, "/user/hand/left/input/squeeze/value"},
        {impl_->squeeze, "/user/hand/right/input/squeeze/value"},
        {impl_->thumbstick, "/user/hand/left/input/thumbstick"},
        {impl_->thumbstick, "/user/hand/right/input/thumbstick"},
        {impl_->teleport, "/user/hand/left/input/thumbstick/click"},
        {impl_->teleport, "/user/hand/right/input/thumbstick/click"},
        {impl_->jump, "/user/hand/right/input/a/click"},
        {impl_->haptic, "/user/hand/left/output/haptic"},
        {impl_->haptic, "/user/hand/right/output/haptic"},
    });
    suggestProfile("/interaction_profiles/khr/simple_controller", {
        {impl_->gripPose, "/user/hand/left/input/grip/pose"},
        {impl_->gripPose, "/user/hand/right/input/grip/pose"},
        {impl_->aimPose, "/user/hand/left/input/aim/pose"},
        {impl_->aimPose, "/user/hand/right/input/aim/pose"},
        {impl_->select, "/user/hand/left/input/select/click"},
        {impl_->select, "/user/hand/right/input/select/click"},
        {impl_->teleport, "/user/hand/left/input/menu/click"},
        {impl_->teleport, "/user/hand/right/input/menu/click"},
        {impl_->jump, "/user/hand/right/input/menu/click"},
        {impl_->haptic, "/user/hand/left/output/haptic"},
        {impl_->haptic, "/user/hand/right/output/haptic"},
    });
    suggestProfile("/interaction_profiles/oculus/touch_controller", {
        {impl_->gripPose, "/user/hand/left/input/grip/pose"},
        {impl_->gripPose, "/user/hand/right/input/grip/pose"},
        {impl_->aimPose, "/user/hand/left/input/aim/pose"},
        {impl_->aimPose, "/user/hand/right/input/aim/pose"},
        {impl_->select, "/user/hand/left/input/trigger/value"},
        {impl_->select, "/user/hand/right/input/trigger/value"},
        {impl_->squeeze, "/user/hand/left/input/squeeze/value"},
        {impl_->squeeze, "/user/hand/right/input/squeeze/value"},
        {impl_->thumbstick, "/user/hand/left/input/thumbstick"},
        {impl_->thumbstick, "/user/hand/right/input/thumbstick"},
        {impl_->teleport, "/user/hand/left/input/thumbstick/click"},
        {impl_->teleport, "/user/hand/right/input/thumbstick/click"},
        {impl_->jump, "/user/hand/right/input/a/click"},
        {impl_->haptic, "/user/hand/left/output/haptic"},
        {impl_->haptic, "/user/hand/right/output/haptic"},
    });

    impl_->info.availability = RuntimeAvailability::SystemReady;
    error.clear();
    return true;
}

void OpenXrRuntime::Shutdown() noexcept {
    if (impl_) {
        impl_->Reset();
    }
}

void OpenXrRuntime::PollEvents() {
    if (!impl_ || impl_->instance == XR_NULL_HANDLE) {
        return;
    }
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(impl_->instance, &event) == XR_SUCCESS) {
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

const RuntimeInfo& OpenXrRuntime::Info() const noexcept { return impl_->info; }
bool OpenXrRuntime::IsSystemReady() const noexcept {
    return impl_->info.availability == RuntimeAvailability::SystemReady;
}

OpenXrVulkanSession::OpenXrVulkanSession(OpenXrRuntime& runtime) noexcept : runtime_(&runtime) {}

bool OpenXrVulkanSession::RunFrames(const std::uint32_t maximumFrames,
                                    VulkanSessionRunReport& report,
                                    std::string& error,
                                    const HardwareSceneView* scene) {
    report = {};
    if (runtime_ == nullptr || !runtime_->impl_ || !runtime_->IsSystemReady()) {
        error = "OpenXR system discovery must succeed before creating a Vulkan session.";
        return false;
    }
    if (maximumFrames == 0U) {
        error = "OpenXR session frame count must be greater than zero.";
        return false;
    }
    const bool renderHardwareScene = scene != nullptr && scene->vertices != nullptr
        && scene->vertexCount >= 3U;
    if (scene != nullptr && (!renderHardwareScene || (scene->vertexCount % 3U) != 0U)) {
        error = "OpenXR hardware scene must contain a non-empty triangle-list vertex stream.";
        return false;
    }
    if (scene != nullptr && scene->dynamicVertexCapacity > 0U
        && scene->updateInteraction == nullptr) {
        error = "OpenXR dynamic vertex capacity requires an interaction update callback.";
        return false;
    }
#if !RAWIRON_XR_HARDWARE_SCENE_ENABLED
    if (renderHardwareScene) {
        error = "OpenXR hardware scene shaders were not built because glslangValidator was unavailable.";
        return false;
    }
#endif
    OpenXrRuntime::Impl& runtime = *runtime_->impl_;

    PFN_xrCreateVulkanInstanceKHR createVulkanInstance = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR getVulkanGraphicsDevice = nullptr;
    PFN_xrCreateVulkanDeviceKHR createVulkanDevice = nullptr;
    const auto loadFunction = [&](const char* name, auto& function) {
        PFN_xrVoidFunction raw = nullptr;
        const XrResult loadResult = xrGetInstanceProcAddr(runtime.instance, name, &raw);
        function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(raw);
        if (XR_FAILED(loadResult) || function == nullptr) {
            error = "Could not load " + std::string(name) + ": " + ResultText(runtime.instance, loadResult);
            return false;
        }
        return true;
    };
    if (!loadFunction("xrCreateVulkanInstanceKHR", createVulkanInstance)
        || !loadFunction("xrGetVulkanGraphicsDevice2KHR", getVulkanGraphicsDevice)
        || !loadFunction("xrCreateVulkanDeviceKHR", createVulkanDevice)) {
        return false;
    }

    struct Resources {
        XrSession session = XR_NULL_HANDLE;
        XrSpace appSpace = XR_NULL_HANDLE;
        XrSpace leftAimSpace = XR_NULL_HANDLE;
        XrSpace rightAimSpace = XR_NULL_HANDLE;
        XrSwapchain swapchain = XR_NULL_HANDLE;
        XrHandTrackerEXT leftHandTracker = XR_NULL_HANDLE;
        XrHandTrackerEXT rightHandTracker = XR_NULL_HANDLE;
        PFN_xrDestroyHandTrackerEXT destroyHandTracker = nullptr;
        VkInstance instance = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkBuffer sceneVertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory sceneVertexMemory = VK_NULL_HANDLE;
        VkBuffer controllerVertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory controllerVertexMemory = VK_NULL_HANDLE;
        void* mappedControllerVertices = nullptr;
        VkBuffer dynamicVertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory dynamicVertexMemory = VK_NULL_HANDLE;
        void* mappedDynamicVertices = nullptr;
        VkImage depthImage = VK_NULL_HANDLE;
        VkDeviceMemory depthMemory = VK_NULL_HANDLE;
        std::array<VkImageView, 2> depthViews{};
        std::vector<VkImageView> colorViews{};
        std::vector<VkFramebuffer> framebuffers{};
        VkRenderPass sceneRenderPass = VK_NULL_HANDLE;
        VkPipelineLayout scenePipelineLayout = VK_NULL_HANDLE;
        VkPipeline scenePipeline = VK_NULL_HANDLE;
        VkImage sceneTexture = VK_NULL_HANDLE;
        VkDeviceMemory sceneTextureMemory = VK_NULL_HANDLE;
        VkImageView sceneTextureView = VK_NULL_HANDLE;
        VkSampler sceneSampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout sceneDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool sceneDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet sceneDescriptorSet = VK_NULL_HANDLE;
        ~Resources() {
            if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
            if (scenePipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, scenePipeline, nullptr);
            if (scenePipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, scenePipelineLayout, nullptr);
            if (sceneDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, sceneDescriptorPool, nullptr);
            if (sceneDescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, sceneDescriptorSetLayout, nullptr);
            if (sceneSampler != VK_NULL_HANDLE) vkDestroySampler(device, sceneSampler, nullptr);
            if (sceneTextureView != VK_NULL_HANDLE) vkDestroyImageView(device, sceneTextureView, nullptr);
            if (sceneTexture != VK_NULL_HANDLE) vkDestroyImage(device, sceneTexture, nullptr);
            if (sceneTextureMemory != VK_NULL_HANDLE) vkFreeMemory(device, sceneTextureMemory, nullptr);
            for (VkFramebuffer framebuffer : framebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
            if (sceneRenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, sceneRenderPass, nullptr);
            for (VkImageView view : colorViews) vkDestroyImageView(device, view, nullptr);
            for (VkImageView view : depthViews) if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
            if (depthImage != VK_NULL_HANDLE) vkDestroyImage(device, depthImage, nullptr);
            if (depthMemory != VK_NULL_HANDLE) vkFreeMemory(device, depthMemory, nullptr);
            if (sceneVertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, sceneVertexBuffer, nullptr);
            if (sceneVertexMemory != VK_NULL_HANDLE) vkFreeMemory(device, sceneVertexMemory, nullptr);
            if (mappedControllerVertices != nullptr) vkUnmapMemory(device, controllerVertexMemory);
            if (controllerVertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, controllerVertexBuffer, nullptr);
            if (controllerVertexMemory != VK_NULL_HANDLE) vkFreeMemory(device, controllerVertexMemory, nullptr);
            if (mappedDynamicVertices != nullptr) vkUnmapMemory(device, dynamicVertexMemory);
            if (dynamicVertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, dynamicVertexBuffer, nullptr);
            if (dynamicVertexMemory != VK_NULL_HANDLE) vkFreeMemory(device, dynamicVertexMemory, nullptr);
            if (destroyHandTracker != nullptr) {
                if (leftHandTracker != XR_NULL_HANDLE) destroyHandTracker(leftHandTracker);
                if (rightHandTracker != XR_NULL_HANDLE) destroyHandTracker(rightHandTracker);
            }
            if (swapchain != XR_NULL_HANDLE) xrDestroySwapchain(swapchain);
            if (leftAimSpace != XR_NULL_HANDLE) xrDestroySpace(leftAimSpace);
            if (rightAimSpace != XR_NULL_HANDLE) xrDestroySpace(rightAimSpace);
            if (appSpace != XR_NULL_HANDLE) xrDestroySpace(appSpace);
            if (session != XR_NULL_HANDLE) xrDestroySession(session);
            if (fence != VK_NULL_HANDLE && device != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
            if (commandPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) vkDestroyCommandPool(device, commandPool, nullptr);
            if (device != VK_NULL_HANDLE) vkDestroyDevice(device, nullptr);
            if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, nullptr);
        }
    } resources;

    const std::uint32_t minimumMajor = XR_VERSION_MAJOR(runtime.info.minimumVulkanApiVersion);
    const std::uint32_t minimumMinor = XR_VERSION_MINOR(runtime.info.minimumVulkanApiVersion);
    const std::uint32_t maximumMajor = XR_VERSION_MAJOR(runtime.info.maximumVulkanApiVersion);
    const std::uint32_t maximumMinor = XR_VERSION_MINOR(runtime.info.maximumVulkanApiVersion);
    std::uint32_t vulkanApiVersion = VK_API_VERSION_1_1;
    if (maximumMajor < 1U || (maximumMajor == 1U && maximumMinor < 1U)) {
        vulkanApiVersion = VK_API_VERSION_1_0;
    }
    if (minimumMajor > 1U || (minimumMajor == 1U && minimumMinor > VK_API_VERSION_MINOR(vulkanApiVersion))) {
        vulkanApiVersion = VK_MAKE_API_VERSION(0, minimumMajor, minimumMinor, 0);
    }

    const VkApplicationInfo applicationInfo{
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        nullptr,
        "Raw Iron VR Showcase",
        1,
        "Raw Iron",
        1,
        vulkanApiVersion};
    const VkInstanceCreateInfo vulkanInstanceInfo{
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &applicationInfo, 0, nullptr, 0, nullptr};
    const XrVulkanInstanceCreateInfoKHR xrInstanceInfo{
        XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
        nullptr,
        runtime.system,
        0,
        vkGetInstanceProcAddr,
        &vulkanInstanceInfo,
        nullptr};
    VkResult vkResult = VK_SUCCESS;
    XrResult xrResult = createVulkanInstance(
        runtime.instance, &xrInstanceInfo, &resources.instance, &vkResult);
    if (XR_FAILED(xrResult) || vkResult != VK_SUCCESS) {
        error = "OpenXR Vulkan instance creation failed: " + ResultText(runtime.instance, xrResult)
            + " | VkResult=" + std::to_string(static_cast<int>(vkResult));
        return false;
    }

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    const XrVulkanGraphicsDeviceGetInfoKHR graphicsDeviceInfo{
        XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR, nullptr, runtime.system, resources.instance};
    xrResult = getVulkanGraphicsDevice(runtime.instance, &graphicsDeviceInfo, &physicalDevice);
    if (XR_FAILED(xrResult) || physicalDevice == VK_NULL_HANDLE) {
        error = "OpenXR Vulkan physical-device selection failed: " + ResultText(runtime.instance, xrResult);
        return false;
    }

    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    std::uint32_t queueFamily = queueFamilyCount;
    for (std::uint32_t index = 0; index < queueFamilyCount; ++index) {
        if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            queueFamily = index;
            break;
        }
    }
    if (queueFamily == queueFamilyCount) {
        error = "OpenXR-selected Vulkan device has no graphics queue family.";
        return false;
    }

    constexpr float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo queueInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, queueFamily, 1, &queuePriority};
    const VkDeviceCreateInfo deviceInfo{
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0, 1, &queueInfo, 0, nullptr, 0, nullptr, nullptr};
    const XrVulkanDeviceCreateInfoKHR xrDeviceInfo{
        XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
        nullptr,
        runtime.system,
        0,
        vkGetInstanceProcAddr,
        physicalDevice,
        &deviceInfo,
        nullptr};
    xrResult = createVulkanDevice(runtime.instance, &xrDeviceInfo, &resources.device, &vkResult);
    if (XR_FAILED(xrResult) || vkResult != VK_SUCCESS) {
        error = "OpenXR Vulkan device creation failed: " + ResultText(runtime.instance, xrResult)
            + " | VkResult=" + std::to_string(static_cast<int>(vkResult));
        return false;
    }
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(resources.device, queueFamily, 0, &queue);

    const XrGraphicsBindingVulkan2KHR graphicsBinding{
        XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
        nullptr,
        resources.instance,
        physicalDevice,
        resources.device,
        queueFamily,
        0};
    const XrSessionCreateInfo sessionInfo{
        XR_TYPE_SESSION_CREATE_INFO, &graphicsBinding, 0, runtime.system};
    xrResult = xrCreateSession(runtime.instance, &sessionInfo, &resources.session);
    if (XR_FAILED(xrResult)) {
        error = "xrCreateSession failed: " + ResultText(runtime.instance, xrResult);
        return false;
    }

    const XrSessionActionSetsAttachInfo attachInfo{
        XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO, nullptr, 1, &runtime.gameplayActions};
    xrResult = xrAttachSessionActionSets(resources.session, &attachInfo);
    if (XR_FAILED(xrResult)) {
        error = "xrAttachSessionActionSets failed: " + ResultText(runtime.instance, xrResult);
        return false;
    }

    std::uint32_t referenceSpaceCount = 0;
    xrEnumerateReferenceSpaces(resources.session, 0, &referenceSpaceCount, nullptr);
    std::vector<XrReferenceSpaceType> referenceSpaces(referenceSpaceCount);
    xrEnumerateReferenceSpaces(
        resources.session, referenceSpaceCount, &referenceSpaceCount, referenceSpaces.data());
    const bool supportsStage = std::ranges::find(referenceSpaces, XR_REFERENCE_SPACE_TYPE_STAGE)
        != referenceSpaces.end();
    runtime.info.stageSpace = supportsStage;
    const XrPosef identityPose{{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    const XrReferenceSpaceCreateInfo spaceInfo{
        XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        nullptr,
        supportsStage ? XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL,
        identityPose};
    xrResult = xrCreateReferenceSpace(resources.session, &spaceInfo, &resources.appSpace);
    if (XR_FAILED(xrResult)) {
        error = "xrCreateReferenceSpace failed: " + ResultText(runtime.instance, xrResult);
        return false;
    }

    const auto createActionSpace = [&](const XrAction action, const XrPath subaction, XrSpace& output) {
        const XrActionSpaceCreateInfo actionSpaceInfo{
            XR_TYPE_ACTION_SPACE_CREATE_INFO, nullptr, action, subaction, identityPose};
        return xrCreateActionSpace(resources.session, &actionSpaceInfo, &output);
    };
    XrPath leftHand = XR_NULL_PATH;
    XrPath rightHand = XR_NULL_PATH;
    xrStringToPath(runtime.instance, "/user/hand/left", &leftHand);
    xrStringToPath(runtime.instance, "/user/hand/right", &rightHand);
    if (XR_FAILED(createActionSpace(runtime.aimPose, leftHand, resources.leftAimSpace))
        || XR_FAILED(createActionSpace(runtime.aimPose, rightHand, resources.rightAimSpace))) {
        error = "Could not create controller aim spaces.";
        return false;
    }

    PFN_xrLocateHandJointsEXT locateHandJoints = nullptr;
    if (runtime.info.handTracking) {
        PFN_xrCreateHandTrackerEXT createHandTracker = nullptr;
        if (!loadFunction("xrCreateHandTrackerEXT", createHandTracker)
            || !loadFunction("xrDestroyHandTrackerEXT", resources.destroyHandTracker)
            || !loadFunction("xrLocateHandJointsEXT", locateHandJoints)) {
            runtime.info.warnings.push_back("XR_EXT_hand_tracking was advertised but its functions could not be loaded.");
            runtime.info.handTracking = false;
            error.clear();
        } else {
            const XrHandTrackerCreateInfoEXT leftTrackerInfo{
                XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
                nullptr,
                XR_HAND_LEFT_EXT,
                XR_HAND_JOINT_SET_DEFAULT_EXT};
            const XrHandTrackerCreateInfoEXT rightTrackerInfo{
                XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
                nullptr,
                XR_HAND_RIGHT_EXT,
                XR_HAND_JOINT_SET_DEFAULT_EXT};
            if (XR_FAILED(createHandTracker(
                    resources.session, &leftTrackerInfo, &resources.leftHandTracker))
                || XR_FAILED(createHandTracker(
                    resources.session, &rightTrackerInfo, &resources.rightHandTracker))) {
                runtime.info.warnings.push_back("OpenXR hand trackers could not be created; controller poses remain available.");
                if (resources.leftHandTracker != XR_NULL_HANDLE) {
                    resources.destroyHandTracker(resources.leftHandTracker);
                    resources.leftHandTracker = XR_NULL_HANDLE;
                }
                if (resources.rightHandTracker != XR_NULL_HANDLE) {
                    resources.destroyHandTracker(resources.rightHandTracker);
                    resources.rightHandTracker = XR_NULL_HANDLE;
                }
                runtime.info.handTracking = false;
            }
        }
    }

    std::uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(resources.session, 0, &formatCount, nullptr);
    std::vector<std::int64_t> formats(formatCount);
    xrResult = xrEnumerateSwapchainFormats(
        resources.session, formatCount, &formatCount, formats.data());
    if (XR_FAILED(xrResult) || formats.empty()) {
        error = "OpenXR runtime returned no Vulkan swapchain formats.";
        return false;
    }
    constexpr std::array<VkFormat, 4> preferredFormats{
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM};
    std::int64_t selectedFormat = formats.front();
    for (const VkFormat preferred : preferredFormats) {
        if (std::ranges::find(formats, static_cast<std::int64_t>(preferred)) != formats.end()) {
            selectedFormat = preferred;
            break;
        }
    }
    const std::uint32_t width = std::max(
        runtime.info.stereoViews[0].recommendedWidth, runtime.info.stereoViews[1].recommendedWidth);
    const std::uint32_t height = std::max(
        runtime.info.stereoViews[0].recommendedHeight, runtime.info.stereoViews[1].recommendedHeight);
    const XrSwapchainCreateInfo swapchainInfo{
        XR_TYPE_SWAPCHAIN_CREATE_INFO,
        nullptr,
        0,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
        selectedFormat,
        1,
        width,
        height,
        1,
        2,
        1};
    xrResult = xrCreateSwapchain(resources.session, &swapchainInfo, &resources.swapchain);
    if (XR_FAILED(xrResult)) {
        error = "xrCreateSwapchain failed: " + ResultText(runtime.instance, xrResult);
        return false;
    }
    report.width = width;
    report.height = height;
    report.colorFormat = selectedFormat;

    std::uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(resources.swapchain, 0, &imageCount, nullptr);
    std::vector<XrSwapchainImageVulkan2KHR> images(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    xrResult = xrEnumerateSwapchainImages(
        resources.swapchain,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
    if (XR_FAILED(xrResult) || images.empty()) {
        error = "OpenXR swapchain returned no Vulkan images.";
        return false;
    }
    std::vector<bool> initialized(images.size(), false);

    const VkCommandPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        queueFamily};
    if (vkCreateCommandPool(resources.device, &poolInfo, nullptr, &resources.commandPool) != VK_SUCCESS) {
        error = "Could not create the OpenXR Vulkan command pool.";
        return false;
    }
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo commandInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        resources.commandPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1};
    if (vkAllocateCommandBuffers(resources.device, &commandInfo, &commandBuffer) != VK_SUCCESS) {
        error = "Could not allocate the OpenXR Vulkan command buffer.";
        return false;
    }
    const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(resources.device, &fenceInfo, nullptr, &resources.fence) != VK_SUCCESS) {
        error = "Could not create the OpenXR Vulkan frame fence.";
        return false;
    }

    if (renderHardwareScene) {
#if RAWIRON_XR_HARDWARE_SCENE_ENABLED
        const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(
            scene->vertexCount * sizeof(HardwareSceneVertex));
        const VkBufferCreateInfo vertexBufferInfo{
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, vertexBytes,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};
        if (vkCreateBuffer(resources.device, &vertexBufferInfo, nullptr, &resources.sceneVertexBuffer)
            != VK_SUCCESS) {
            error = "Could not create the OpenXR hardware-scene vertex buffer.";
            return false;
        }
        VkMemoryRequirements vertexRequirements{};
        vkGetBufferMemoryRequirements(resources.device, resources.sceneVertexBuffer, &vertexRequirements);
        const auto vertexMemoryType = FindMemoryType(
            physicalDevice,
            vertexRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!vertexMemoryType.has_value()) {
            error = "OpenXR hardware scene requires host-visible coherent Vulkan memory.";
            return false;
        }
        const VkMemoryAllocateInfo vertexAllocation{
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, vertexRequirements.size, *vertexMemoryType};
        if (vkAllocateMemory(resources.device, &vertexAllocation, nullptr, &resources.sceneVertexMemory)
                != VK_SUCCESS
            || vkBindBufferMemory(resources.device, resources.sceneVertexBuffer, resources.sceneVertexMemory, 0)
                != VK_SUCCESS) {
            error = "Could not allocate the OpenXR hardware-scene vertex buffer.";
            return false;
        }
        void* mappedVertices = nullptr;
        if (vkMapMemory(resources.device, resources.sceneVertexMemory, 0, vertexBytes, 0, &mappedVertices)
            != VK_SUCCESS) {
            error = "Could not map the OpenXR hardware-scene vertex buffer.";
            return false;
        }
        std::memcpy(mappedVertices, scene->vertices, static_cast<std::size_t>(vertexBytes));
        vkUnmapMemory(resources.device, resources.sceneVertexMemory);

        constexpr VkDeviceSize controllerVertexBytes = 72U * sizeof(HardwareSceneVertex);
        const VkBufferCreateInfo controllerBufferInfo{
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, controllerVertexBytes,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};
        if (vkCreateBuffer(
                resources.device, &controllerBufferInfo, nullptr, &resources.controllerVertexBuffer)
            != VK_SUCCESS) {
            error = "Could not create the tracked-controller vertex buffer.";
            return false;
        }
        VkMemoryRequirements controllerRequirements{};
        vkGetBufferMemoryRequirements(
            resources.device, resources.controllerVertexBuffer, &controllerRequirements);
        const auto controllerMemoryType = FindMemoryType(
            physicalDevice,
            controllerRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!controllerMemoryType.has_value()) {
            error = "Tracked controllers require host-visible coherent Vulkan memory.";
            return false;
        }
        const VkMemoryAllocateInfo controllerAllocation{
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            controllerRequirements.size,
            *controllerMemoryType};
        if (vkAllocateMemory(
                resources.device,
                &controllerAllocation,
                nullptr,
                &resources.controllerVertexMemory) != VK_SUCCESS
            || vkBindBufferMemory(
                resources.device,
                resources.controllerVertexBuffer,
                resources.controllerVertexMemory,
                0) != VK_SUCCESS
            || vkMapMemory(
                resources.device,
                resources.controllerVertexMemory,
                0,
                controllerVertexBytes,
                0,
                &resources.mappedControllerVertices) != VK_SUCCESS) {
            error = "Could not allocate the tracked-controller vertex buffer.";
            return false;
        }

        if (scene->dynamicVertexCapacity > 0U) {
            const VkDeviceSize dynamicVertexBytes = static_cast<VkDeviceSize>(
                scene->dynamicVertexCapacity * sizeof(HardwareSceneVertex));
            const VkBufferCreateInfo dynamicBufferInfo{
                VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, dynamicVertexBytes,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};
            if (vkCreateBuffer(
                    resources.device, &dynamicBufferInfo, nullptr, &resources.dynamicVertexBuffer)
                != VK_SUCCESS) {
                error = "Could not create the dynamic OpenXR scene vertex buffer.";
                return false;
            }
            VkMemoryRequirements dynamicRequirements{};
            vkGetBufferMemoryRequirements(
                resources.device, resources.dynamicVertexBuffer, &dynamicRequirements);
            const auto dynamicMemoryType = FindMemoryType(
                physicalDevice,
                dynamicRequirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (!dynamicMemoryType.has_value()) {
                error = "Dynamic OpenXR geometry requires host-visible coherent Vulkan memory.";
                return false;
            }
            const VkMemoryAllocateInfo dynamicAllocation{
                VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                nullptr,
                dynamicRequirements.size,
                *dynamicMemoryType};
            if (vkAllocateMemory(
                    resources.device, &dynamicAllocation, nullptr, &resources.dynamicVertexMemory)
                    != VK_SUCCESS
                || vkBindBufferMemory(
                    resources.device,
                    resources.dynamicVertexBuffer,
                    resources.dynamicVertexMemory,
                    0) != VK_SUCCESS
                || vkMapMemory(
                    resources.device,
                    resources.dynamicVertexMemory,
                    0,
                    dynamicVertexBytes,
                    0,
                    &resources.mappedDynamicVertices) != VK_SUCCESS) {
                error = "Could not allocate the dynamic OpenXR scene vertex buffer.";
                return false;
            }
        }

        const bool hasTexture = scene->textureRgba != nullptr
            && scene->textureWidth > 0U && scene->textureHeight > 0U;
        const std::array<std::uint8_t, 4> whitePixel{255U, 255U, 255U, 255U};
        const std::uint8_t* texturePixels = hasTexture ? scene->textureRgba : whitePixel.data();
        const std::uint32_t textureWidth = hasTexture ? scene->textureWidth : 1U;
        const std::uint32_t textureHeight = hasTexture ? scene->textureHeight : 1U;
        const VkDeviceSize textureBytes = static_cast<VkDeviceSize>(textureWidth)
            * static_cast<VkDeviceSize>(textureHeight) * 4U;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        const VkBufferCreateInfo stagingInfo{
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, textureBytes,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};
        if (vkCreateBuffer(resources.device, &stagingInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            error = "Could not create the OpenXR texture staging buffer.";
            return false;
        }
        VkMemoryRequirements stagingRequirements{};
        vkGetBufferMemoryRequirements(resources.device, stagingBuffer, &stagingRequirements);
        const auto stagingMemoryType = FindMemoryType(
            physicalDevice,
            stagingRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!stagingMemoryType.has_value()) {
            vkDestroyBuffer(resources.device, stagingBuffer, nullptr);
            error = "OpenXR texture upload could not find host-visible Vulkan memory.";
            return false;
        }
        const VkMemoryAllocateInfo stagingAllocation{
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, stagingRequirements.size, *stagingMemoryType};
        if (vkAllocateMemory(resources.device, &stagingAllocation, nullptr, &stagingMemory) != VK_SUCCESS
            || vkBindBufferMemory(resources.device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
            if (stagingMemory != VK_NULL_HANDLE) vkFreeMemory(resources.device, stagingMemory, nullptr);
            vkDestroyBuffer(resources.device, stagingBuffer, nullptr);
            error = "Could not allocate the OpenXR texture staging buffer.";
            return false;
        }
        void* mappedTexture = nullptr;
        if (vkMapMemory(resources.device, stagingMemory, 0, textureBytes, 0, &mappedTexture) != VK_SUCCESS) {
            vkFreeMemory(resources.device, stagingMemory, nullptr);
            vkDestroyBuffer(resources.device, stagingBuffer, nullptr);
            error = "Could not map the OpenXR texture staging buffer.";
            return false;
        }
        std::memcpy(mappedTexture, texturePixels, static_cast<std::size_t>(textureBytes));
        vkUnmapMemory(resources.device, stagingMemory);

        const VkImageCreateInfo textureImageInfo{
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_SRGB,
            {textureWidth, textureHeight, 1},
            1,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED};
        if (vkCreateImage(resources.device, &textureImageInfo, nullptr, &resources.sceneTexture)
            != VK_SUCCESS) {
            vkFreeMemory(resources.device, stagingMemory, nullptr);
            vkDestroyBuffer(resources.device, stagingBuffer, nullptr);
            error = "Could not create the OpenXR scene texture atlas.";
            return false;
        }
        VkMemoryRequirements textureRequirements{};
        vkGetImageMemoryRequirements(resources.device, resources.sceneTexture, &textureRequirements);
        const auto textureMemoryType = FindMemoryType(
            physicalDevice, textureRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!textureMemoryType.has_value()) {
            vkFreeMemory(resources.device, stagingMemory, nullptr);
            vkDestroyBuffer(resources.device, stagingBuffer, nullptr);
            error = "OpenXR texture atlas could not find device-local memory.";
            return false;
        }
        const VkMemoryAllocateInfo textureAllocation{
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, textureRequirements.size, *textureMemoryType};
        if (vkAllocateMemory(resources.device, &textureAllocation, nullptr, &resources.sceneTextureMemory)
                != VK_SUCCESS
            || vkBindImageMemory(resources.device, resources.sceneTexture, resources.sceneTextureMemory, 0)
                != VK_SUCCESS) {
            vkFreeMemory(resources.device, stagingMemory, nullptr);
            vkDestroyBuffer(resources.device, stagingBuffer, nullptr);
            error = "Could not allocate the OpenXR scene texture atlas.";
            return false;
        }
        VkCommandBuffer uploadCommand = VK_NULL_HANDLE;
        const VkCommandBufferAllocateInfo uploadAllocate{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, resources.commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
        vkAllocateCommandBuffers(resources.device, &uploadAllocate, &uploadCommand);
        const VkCommandBufferBeginInfo uploadBegin{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
        vkBeginCommandBuffer(uploadCommand, &uploadBegin);
        VkImageMemoryBarrier atlasToTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        atlasToTransfer.srcAccessMask = 0;
        atlasToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        atlasToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        atlasToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        atlasToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        atlasToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        atlasToTransfer.image = resources.sceneTexture;
        atlasToTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(
            uploadCommand, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &atlasToTransfer);
        const VkBufferImageCopy atlasCopy{
            0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {textureWidth, textureHeight, 1}};
        vkCmdCopyBufferToImage(
            uploadCommand, stagingBuffer, resources.sceneTexture,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &atlasCopy);
        VkImageMemoryBarrier atlasToSample = atlasToTransfer;
        atlasToSample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        atlasToSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        atlasToSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        atlasToSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(
            uploadCommand, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &atlasToSample);
        vkEndCommandBuffer(uploadCommand);
        const VkSubmitInfo uploadSubmit{
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &uploadCommand, 0, nullptr};
        if (vkQueueSubmit(queue, 1, &uploadSubmit, VK_NULL_HANDLE) != VK_SUCCESS
            || vkQueueWaitIdle(queue) != VK_SUCCESS) {
            error = "Vulkan failed while uploading the OpenXR scene texture atlas.";
            return false;
        }
        vkFreeCommandBuffers(resources.device, resources.commandPool, 1, &uploadCommand);
        vkFreeMemory(resources.device, stagingMemory, nullptr);
        vkDestroyBuffer(resources.device, stagingBuffer, nullptr);

        const VkImageViewCreateInfo textureViewInfo{
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            resources.sceneTexture,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_R8G8B8A8_SRGB,
            {},
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        if (vkCreateImageView(
                resources.device, &textureViewInfo, nullptr, &resources.sceneTextureView) != VK_SUCCESS) {
            error = "Could not create the OpenXR scene texture view.";
            return false;
        }
        const VkSamplerCreateInfo samplerInfo{
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            nullptr,
            0,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            0.0f,
            VK_FALSE,
            1.0f,
            VK_FALSE,
            VK_COMPARE_OP_ALWAYS,
            0.0f,
            0.0f,
            VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            VK_FALSE};
        if (vkCreateSampler(resources.device, &samplerInfo, nullptr, &resources.sceneSampler)
            != VK_SUCCESS) {
            error = "Could not create the OpenXR scene texture sampler.";
            return false;
        }

        constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        const VkImageCreateInfo depthImageInfo{
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0,
            VK_IMAGE_TYPE_2D,
            depthFormat,
            {width, height, 1},
            1,
            2,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED};
        if (vkCreateImage(resources.device, &depthImageInfo, nullptr, &resources.depthImage) != VK_SUCCESS) {
            error = "Could not create the OpenXR stereo depth image.";
            return false;
        }
        VkMemoryRequirements depthRequirements{};
        vkGetImageMemoryRequirements(resources.device, resources.depthImage, &depthRequirements);
        const auto depthMemoryType = FindMemoryType(
            physicalDevice, depthRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!depthMemoryType.has_value()) {
            error = "OpenXR hardware scene could not find device-local depth memory.";
            return false;
        }
        const VkMemoryAllocateInfo depthAllocation{
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, depthRequirements.size, *depthMemoryType};
        if (vkAllocateMemory(resources.device, &depthAllocation, nullptr, &resources.depthMemory) != VK_SUCCESS
            || vkBindImageMemory(resources.device, resources.depthImage, resources.depthMemory, 0) != VK_SUCCESS) {
            error = "Could not allocate the OpenXR stereo depth image.";
            return false;
        }

        for (std::uint32_t eye = 0; eye < 2U; ++eye) {
            const VkImageViewCreateInfo depthViewInfo{
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                nullptr,
                0,
                resources.depthImage,
                VK_IMAGE_VIEW_TYPE_2D,
                depthFormat,
                {},
                {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, eye, 1}};
            if (vkCreateImageView(resources.device, &depthViewInfo, nullptr, &resources.depthViews[eye])
                != VK_SUCCESS) {
                error = "Could not create an OpenXR eye depth view.";
                return false;
            }
        }

        const VkAttachmentDescription colorAttachment{
            0,
            static_cast<VkFormat>(selectedFormat),
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        const VkAttachmentDescription depthAttachment{
            0,
            depthFormat,
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        const std::array<VkAttachmentDescription, 2> attachments{colorAttachment, depthAttachment};
        const VkAttachmentReference colorReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        const VkAttachmentReference depthReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        const VkSubpassDescription subpass{
            0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, &colorReference, nullptr, &depthReference, 0, nullptr};
        const VkSubpassDependency dependency{
            VK_SUBPASS_EXTERNAL,
            0,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            0};
        const VkRenderPassCreateInfo renderPassInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            nullptr,
            0,
            static_cast<std::uint32_t>(attachments.size()),
            attachments.data(),
            1,
            &subpass,
            1,
            &dependency};
        if (vkCreateRenderPass(resources.device, &renderPassInfo, nullptr, &resources.sceneRenderPass)
            != VK_SUCCESS) {
            error = "Could not create the OpenXR hardware-scene render pass.";
            return false;
        }

        resources.colorViews.reserve(images.size() * 2U);
        resources.framebuffers.reserve(images.size() * 2U);
        for (const XrSwapchainImageVulkan2KHR& image : images) {
            for (std::uint32_t eye = 0; eye < 2U; ++eye) {
                VkImageView colorView = VK_NULL_HANDLE;
                const VkImageViewCreateInfo colorViewInfo{
                    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    nullptr,
                    0,
                    image.image,
                    VK_IMAGE_VIEW_TYPE_2D,
                    static_cast<VkFormat>(selectedFormat),
                    {},
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, eye, 1}};
                if (vkCreateImageView(resources.device, &colorViewInfo, nullptr, &colorView) != VK_SUCCESS) {
                    error = "Could not create an OpenXR swapchain eye view.";
                    return false;
                }
                resources.colorViews.push_back(colorView);
                const std::array<VkImageView, 2> framebufferAttachments{
                    colorView, resources.depthViews[eye]};
                VkFramebuffer framebuffer = VK_NULL_HANDLE;
                const VkFramebufferCreateInfo framebufferInfo{
                    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    nullptr,
                    0,
                    resources.sceneRenderPass,
                    static_cast<std::uint32_t>(framebufferAttachments.size()),
                    framebufferAttachments.data(),
                    width,
                    height,
                    1};
                if (vkCreateFramebuffer(resources.device, &framebufferInfo, nullptr, &framebuffer)
                    != VK_SUCCESS) {
                    error = "Could not create an OpenXR eye framebuffer.";
                    return false;
                }
                resources.framebuffers.push_back(framebuffer);
            }
        }

        const std::vector<std::uint32_t> vertexSpirv =
            ReadSpirv(std::filesystem::path(RAWIRON_XR_SHADER_DIR) / "XrScene.vert.spv");
        const std::vector<std::uint32_t> fragmentSpirv =
            ReadSpirv(std::filesystem::path(RAWIRON_XR_SHADER_DIR) / "XrScene.frag.spv");
        if (vertexSpirv.empty() || fragmentSpirv.empty()) {
            error = "OpenXR hardware-scene SPIR-V shaders are missing.";
            return false;
        }
        const auto createShader = [&](const std::vector<std::uint32_t>& words, VkShaderModule& shader) {
            const VkShaderModuleCreateInfo shaderInfo{
                VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                nullptr,
                0,
                words.size() * sizeof(std::uint32_t),
                words.data()};
            return vkCreateShaderModule(resources.device, &shaderInfo, nullptr, &shader) == VK_SUCCESS;
        };
        VkShaderModule vertexShader = VK_NULL_HANDLE;
        VkShaderModule fragmentShader = VK_NULL_HANDLE;
        if (!createShader(vertexSpirv, vertexShader) || !createShader(fragmentSpirv, fragmentShader)) {
            if (vertexShader != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, vertexShader, nullptr);
            if (fragmentShader != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, fragmentShader, nullptr);
            error = "Could not create OpenXR hardware-scene shader modules.";
            return false;
        }
        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{{
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
             VK_SHADER_STAGE_VERTEX_BIT, vertexShader, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
             VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader, "main", nullptr}}};
        const VkVertexInputBindingDescription vertexBinding{
            0, sizeof(HardwareSceneVertex), VK_VERTEX_INPUT_RATE_VERTEX};
        const std::array<VkVertexInputAttributeDescription, 7> vertexAttributes{{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(HardwareSceneVertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(HardwareSceneVertex, normal)},
            {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(HardwareSceneVertex, color)},
            {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(HardwareSceneVertex, texCoord)},
            {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HardwareSceneVertex, atlasRect)},
            {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HardwareSceneVertex, normalAtlasRect)},
            {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HardwareSceneVertex, materialParams)}}};
        const VkPipelineVertexInputStateCreateInfo vertexInput{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            nullptr,
            0,
            1,
            &vertexBinding,
            static_cast<std::uint32_t>(vertexAttributes.size()),
            vertexAttributes.data()};
        const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            VK_FALSE};
        const VkPipelineViewportStateCreateInfo viewportState{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
        const VkPipelineRasterizationStateCreateInfo rasterization{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_FALSE,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_NONE,
            VK_FRONT_FACE_COUNTER_CLOCKWISE,
            VK_FALSE,
            0,
            0,
            0,
            1.0f};
        const VkPipelineMultisampleStateCreateInfo multisample{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            0,
            nullptr,
            VK_FALSE,
            VK_FALSE};
        const VkPipelineDepthStencilStateCreateInfo depthState{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_TRUE,
            VK_TRUE,
            VK_COMPARE_OP_LESS,
            VK_FALSE,
            VK_FALSE,
            {},
            {},
            0,
            1};
        const VkPipelineColorBlendAttachmentState colorBlendAttachment{
            VK_FALSE,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ONE,
            VK_BLEND_FACTOR_ZERO,
            VK_BLEND_OP_ADD,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT};
        const VkPipelineColorBlendStateCreateInfo colorBlend{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_FALSE,
            VK_LOGIC_OP_COPY,
            1,
            &colorBlendAttachment,
            {0, 0, 0, 0}};
        constexpr std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const VkPipelineDynamicStateCreateInfo dynamicState{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,
            0,
            static_cast<std::uint32_t>(dynamicStates.size()),
            dynamicStates.data()};
        const VkDescriptorSetLayoutBinding atlasBinding{
            0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        const VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &atlasBinding};
        if (vkCreateDescriptorSetLayout(
                resources.device,
                &descriptorLayoutInfo,
                nullptr,
                &resources.sceneDescriptorSetLayout) != VK_SUCCESS) {
            vkDestroyShaderModule(resources.device, vertexShader, nullptr);
            vkDestroyShaderModule(resources.device, fragmentShader, nullptr);
            error = "Could not create the OpenXR scene texture descriptor layout.";
            return false;
        }
        const VkDescriptorPoolSize descriptorPoolSize{
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        const VkDescriptorPoolCreateInfo descriptorPoolInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 1, &descriptorPoolSize};
        if (vkCreateDescriptorPool(
                resources.device, &descriptorPoolInfo, nullptr, &resources.sceneDescriptorPool)
            != VK_SUCCESS) {
            vkDestroyShaderModule(resources.device, vertexShader, nullptr);
            vkDestroyShaderModule(resources.device, fragmentShader, nullptr);
            error = "Could not create the OpenXR scene texture descriptor pool.";
            return false;
        }
        const VkDescriptorSetAllocateInfo descriptorAllocateInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr,
            resources.sceneDescriptorPool,
            1,
            &resources.sceneDescriptorSetLayout};
        if (vkAllocateDescriptorSets(
                resources.device, &descriptorAllocateInfo, &resources.sceneDescriptorSet) != VK_SUCCESS) {
            vkDestroyShaderModule(resources.device, vertexShader, nullptr);
            vkDestroyShaderModule(resources.device, fragmentShader, nullptr);
            error = "Could not allocate the OpenXR scene texture descriptor set.";
            return false;
        }
        const VkDescriptorImageInfo atlasImageInfo{
            resources.sceneSampler, resources.sceneTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkWriteDescriptorSet descriptorWrite{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            nullptr,
            resources.sceneDescriptorSet,
            0,
            0,
            1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &atlasImageInfo,
            nullptr,
            nullptr};
        vkUpdateDescriptorSets(resources.device, 1, &descriptorWrite, 0, nullptr);
        const VkPushConstantRange pushConstantRange{
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(XrEyePushConstants)};
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,
            0,
            1,
            &resources.sceneDescriptorSetLayout,
            1,
            &pushConstantRange};
        if (vkCreatePipelineLayout(
                resources.device, &pipelineLayoutInfo, nullptr, &resources.scenePipelineLayout)
            != VK_SUCCESS) {
            vkDestroyShaderModule(resources.device, vertexShader, nullptr);
            vkDestroyShaderModule(resources.device, fragmentShader, nullptr);
            error = "Could not create the OpenXR hardware-scene pipeline layout.";
            return false;
        }
        const VkGraphicsPipelineCreateInfo pipelineInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,
            0,
            static_cast<std::uint32_t>(shaderStages.size()),
            shaderStages.data(),
            &vertexInput,
            &inputAssembly,
            nullptr,
            &viewportState,
            &rasterization,
            &multisample,
            &depthState,
            &colorBlend,
            &dynamicState,
            resources.scenePipelineLayout,
            resources.sceneRenderPass,
            0,
            VK_NULL_HANDLE,
            -1};
        const VkResult pipelineResult = vkCreateGraphicsPipelines(
            resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &resources.scenePipeline);
        vkDestroyShaderModule(resources.device, vertexShader, nullptr);
        vkDestroyShaderModule(resources.device, fragmentShader, nullptr);
        if (pipelineResult != VK_SUCCESS) {
            error = "Could not create the OpenXR hardware-scene graphics pipeline: VkResult="
                + std::to_string(static_cast<int>(pipelineResult));
            return false;
        }
#endif
    }

    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    bool sessionRunning = false;
    // Haptic requests are edge feedback, not a per-render-frame effect.
    std::array<XrTime, 2> nextHapticTime{};
    bool exitRequested = false;
    std::uint32_t idlePolls = 0;
    std::vector<XrView> locatedViews(2, {XR_TYPE_VIEW});
    std::array<float, 3> locomotionOrigin{
        renderHardwareScene ? scene->origin[0] : 0.0f,
        renderHardwareScene ? scene->origin[1] : 0.0f,
        renderHardwareScene ? scene->origin[2] : 0.0f};
    XrTime previousDisplayTime = 0;
    float locomotionYawDegrees = 0.0f;
    bool snapTurnLatched = false;
    std::array<bool, 2> squeezeHeld{};

    while (!exitRequested && report.submittedFrames < maximumFrames) {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(runtime.instance, &event) == XR_SUCCESS) {
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* changed = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                sessionState = changed->state;
                if (sessionState == XR_SESSION_STATE_READY && !sessionRunning) {
                    const XrSessionBeginInfo beginInfo{
                        XR_TYPE_SESSION_BEGIN_INFO, nullptr, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
                    xrResult = xrBeginSession(resources.session, &beginInfo);
                    if (XR_FAILED(xrResult)) {
                        error = "xrBeginSession failed: " + ResultText(runtime.instance, xrResult);
                        return false;
                    }
                    sessionRunning = true;
                } else if (sessionState == XR_SESSION_STATE_STOPPING && sessionRunning) {
                    xrEndSession(resources.session);
                    sessionRunning = false;
                    exitRequested = true;
                } else if (sessionState == XR_SESSION_STATE_EXITING
                           || sessionState == XR_SESSION_STATE_LOSS_PENDING) {
                    exitRequested = true;
                }
            } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                exitRequested = true;
            }
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        if (!sessionRunning) {
            if (++idlePolls > 1000U) {
                error = "OpenXR session did not enter READY state within ten seconds.";
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (sessionState == XR_SESSION_STATE_FOCUSED) {
            ++report.focusedFrames;
        }

        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        const XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        xrResult = xrWaitFrame(resources.session, &waitInfo, &frameState);
        if (XR_FAILED(xrResult)) {
            error = "xrWaitFrame failed: " + ResultText(runtime.instance, xrResult);
            return false;
        }
        const XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        xrResult = xrBeginFrame(resources.session, &frameBeginInfo);
        if (XR_FAILED(xrResult)) {
            error = "xrBeginFrame failed: " + ResultText(runtime.instance, xrResult);
            return false;
        }
        const float frameDeltaSeconds = previousDisplayTime == 0
            ? 0.0f
            : std::clamp(
                static_cast<float>(frameState.predictedDisplayTime - previousDisplayTime) * 1.0e-9f,
                0.0f,
                0.05f);

        XrViewState viewState{XR_TYPE_VIEW_STATE};
        std::uint32_t locatedViewCount = 0;
        const XrViewLocateInfo locateInfo{
            XR_TYPE_VIEW_LOCATE_INFO,
            nullptr,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            frameState.predictedDisplayTime,
            resources.appSpace};
        xrResult = xrLocateViews(
            resources.session, &locateInfo, &viewState, 2, &locatedViewCount, locatedViews.data());
        const bool stereoValid = XR_SUCCEEDED(xrResult) && locatedViewCount == 2U
            && (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0U
            && (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0U;

        const XrActiveActionSet activeActionSet{runtime.gameplayActions, XR_NULL_PATH};
        const XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr, 1, &activeActionSet};
        const XrResult syncResult = xrSyncActions(resources.session, &syncInfo);
        if (XR_FAILED(syncResult) && syncResult != XR_SESSION_NOT_FOCUSED) {
            error = "xrSyncActions failed: " + ResultText(runtime.instance, syncResult);
            return false;
        }
        std::array<XrActionStateBoolean, 2> selectStates{{
            {XR_TYPE_ACTION_STATE_BOOLEAN},
            {XR_TYPE_ACTION_STATE_BOOLEAN}}};
        const std::array<XrPath, 2> handPaths{leftHand, rightHand};
        for (std::size_t handIndex = 0; handIndex < handPaths.size(); ++handIndex) {
            const XrActionStateGetInfo selectInfo{
                XR_TYPE_ACTION_STATE_GET_INFO,
                nullptr,
                runtime.select,
                handPaths[handIndex]};
            if (XR_FAILED(xrGetActionStateBoolean(
                    resources.session, &selectInfo, &selectStates[handIndex]))) {
                selectStates[handIndex] = {XR_TYPE_ACTION_STATE_BOOLEAN};
            }
            if (selectStates[handIndex].isActive != 0
                && selectStates[handIndex].changedSinceLastSync != 0
                && selectStates[handIndex].currentState != 0) {
                ++report.selectPressCount;
            }
        }
        std::array<bool, 2> grabPressed{};
        std::array<bool, 2> grabReleased{};
        std::array<bool, 2> grabActionActive{};
        for (std::size_t handIndex = 0; handIndex < handPaths.size(); ++handIndex) {
            XrActionStateFloat squeezeState{XR_TYPE_ACTION_STATE_FLOAT};
            const XrActionStateGetInfo squeezeInfo{
                XR_TYPE_ACTION_STATE_GET_INFO,
                nullptr,
                runtime.squeeze,
                handPaths[handIndex]};
            const bool active = XR_SUCCEEDED(xrGetActionStateFloat(
                resources.session, &squeezeInfo, &squeezeState)) && squeezeState.isActive != 0;
            grabActionActive[handIndex] = active;
            const bool previousHeld = squeezeHeld[handIndex];
            if (!active || squeezeState.currentState <= 0.35f) squeezeHeld[handIndex] = false;
            else if (squeezeState.currentState >= 0.55f) squeezeHeld[handIndex] = true;
            grabPressed[handIndex] = !previousHeld && squeezeHeld[handIndex];
            grabReleased[handIndex] = previousHeld && !squeezeHeld[handIndex];
        }
        XrActionStateBoolean jumpState{XR_TYPE_ACTION_STATE_BOOLEAN};
        const XrActionStateGetInfo jumpInfo{
            XR_TYPE_ACTION_STATE_GET_INFO, nullptr, runtime.jump, rightHand};
        const bool jumpPressed = XR_SUCCEEDED(xrGetActionStateBoolean(
            resources.session, &jumpInfo, &jumpState))
            && jumpState.isActive != 0 && jumpState.changedSinceLastSync != 0
            && jumpState.currentState != 0;
        if (jumpPressed) ++report.jumpPressCount;
        std::array<XrActionStateBoolean, 2> teleportStates{{
            {XR_TYPE_ACTION_STATE_BOOLEAN},
            {XR_TYPE_ACTION_STATE_BOOLEAN}}};
        for (std::size_t handIndex = 0; handIndex < handPaths.size(); ++handIndex) {
            const XrActionStateGetInfo teleportInfo{
                XR_TYPE_ACTION_STATE_GET_INFO,
                nullptr,
                runtime.teleport,
                handPaths[handIndex]};
            if (XR_FAILED(xrGetActionStateBoolean(
                    resources.session, &teleportInfo, &teleportStates[handIndex]))) {
                teleportStates[handIndex] = {XR_TYPE_ACTION_STATE_BOOLEAN};
            }
        }
        XrActionStateVector2f turnState{XR_TYPE_ACTION_STATE_VECTOR2F};
        const XrActionStateGetInfo turnStateInfo{
            XR_TYPE_ACTION_STATE_GET_INFO, nullptr, runtime.thumbstick, rightHand};
        if (renderHardwareScene
            && XR_SUCCEEDED(xrGetActionStateVector2f(resources.session, &turnStateInfo, &turnState))
            && turnState.isActive != 0) {
            const float horizontal = turnState.currentState.x;
            const auto preserveHeadPivotTurn = [&](const float turnDegrees) {
                const auto appToWorldHorizontal = [](const XrVector3f& appPosition,
                                                     const float yawDegrees) {
                    const float radians = -yawDegrees * 0.01745329251994329577f;
                    const float cosine = std::cos(radians);
                    const float sine = std::sin(radians);
                    const float rotatedX = cosine * appPosition.x + sine * appPosition.z;
                    const float rotatedZ = -sine * appPosition.x + cosine * appPosition.z;
                    return std::array<float, 2>{rotatedX, -rotatedZ};
                };
                const std::array<float, 2> oldHeadOffset = appToWorldHorizontal(
                    locatedViews[0].pose.position, locomotionYawDegrees);
                locomotionYawDegrees += turnDegrees;
                if (locomotionYawDegrees > 180.0f) locomotionYawDegrees -= 360.0f;
                if (locomotionYawDegrees < -180.0f) locomotionYawDegrees += 360.0f;
                const std::array<float, 2> newHeadOffset = appToWorldHorizontal(
                    locatedViews[0].pose.position, locomotionYawDegrees);
                locomotionOrigin[0] += oldHeadOffset[0] - newHeadOffset[0];
                locomotionOrigin[2] += oldHeadOffset[1] - newHeadOffset[1];
            };
            if (scene->turnMode == HardwareTurnMode::Smooth) {
                if (std::fabs(horizontal) >= 0.18f && frameDeltaSeconds > 0.0f) {
                    const float scaled = std::copysign(
                        (std::fabs(horizontal) - 0.18f) / 0.82f, horizontal);
                    preserveHeadPivotTurn(
                        scaled * std::clamp(scene->smoothTurnDegreesPerSecond, 15.0f, 360.0f)
                            * frameDeltaSeconds);
                }
                snapTurnLatched = false;
            } else if (!snapTurnLatched && std::fabs(horizontal) >= 0.70f) {
                preserveHeadPivotTurn(
                    horizontal > 0.0f
                        ? std::clamp(scene->snapTurnDegrees, 10.0f, 90.0f)
                        : -std::clamp(scene->snapTurnDegrees, 10.0f, 90.0f));
                snapTurnLatched = true;
                ++report.snapTurnCount;
            } else if (std::fabs(horizontal) <= 0.30f) {
                snapTurnLatched = false;
            }
        }
        XrActionStateVector2f moveState{XR_TYPE_ACTION_STATE_VECTOR2F};
        const XrActionStateGetInfo moveStateInfo{
            XR_TYPE_ACTION_STATE_GET_INFO, nullptr, runtime.thumbstick, leftHand};
        const bool moveActionActive = XR_SUCCEEDED(xrGetActionStateVector2f(
            resources.session, &moveStateInfo, &moveState)) && moveState.isActive != 0;
        if (renderHardwareScene) {
            const float inputLengthSquared = moveState.currentState.x * moveState.currentState.x
                + moveState.currentState.y * moveState.currentState.y;
            if (((moveActionActive && inputLengthSquared > 0.0225f) || jumpPressed)
                && frameDeltaSeconds > 0.0f) {
                std::array<float, 3> forward = RotateVector(
                    locatedViews[0].pose.orientation, {0.0f, 0.0f, -1.0f});
                std::array<float, 3> right = RotateVector(
                    locatedViews[0].pose.orientation, {1.0f, 0.0f, 0.0f});
                forward = {forward[0], 0.0f, -forward[2]};
                right = {right[0], 0.0f, -right[2]};
                const float yawRadians = locomotionYawDegrees * 0.01745329251994329577f;
                const float yawCosine = std::cos(yawRadians);
                const float yawSine = std::sin(yawRadians);
                const auto applyArtificialYaw = [&](std::array<float, 3>& value) {
                    const float x = yawCosine * value[0] + yawSine * value[2];
                    const float z = -yawSine * value[0] + yawCosine * value[2];
                    value[0] = x;
                    value[2] = z;
                };
                applyArtificialYaw(forward);
                applyArtificialYaw(right);
                const auto normalizeHorizontal = [](std::array<float, 3>& value) {
                    const float length = std::sqrt(value[0] * value[0] + value[2] * value[2]);
                    if (length > 1.0e-5f) {
                        value[0] /= length;
                        value[2] /= length;
                    }
                };
                normalizeHorizontal(forward);
                normalizeHorizontal(right);
                if (scene->resolveLocomotion != nullptr) {
                    const HardwareLocomotionInput locomotionInput{
                        moveState.currentState.x,
                        moveState.currentState.y,
                        frameDeltaSeconds,
                        {forward[0], forward[1], forward[2]},
                        {right[0], right[1], right[2]},
                        jumpPressed};
                    scene->resolveLocomotion(
                        scene->locomotionUser,
                        locomotionInput,
                        locomotionOrigin.data(),
                        locomotionYawDegrees);
                } else {
                    constexpr float movementSpeed = 3.2f;
                    locomotionOrigin[0] += (right[0] * moveState.currentState.x
                        + forward[0] * moveState.currentState.y) * movementSpeed * frameDeltaSeconds;
                    locomotionOrigin[2] += (right[2] * moveState.currentState.x
                        + forward[2] * moveState.currentState.y) * movementSpeed * frameDeltaSeconds;
                }
                ++report.locomotionInputFrames;
            }
        }
        previousDisplayTime = frameState.predictedDisplayTime;
        const auto resolveProfile = [&](const XrPath hand, std::string& output) {
            XrInteractionProfileState profileState{XR_TYPE_INTERACTION_PROFILE_STATE};
            if (XR_FAILED(xrGetCurrentInteractionProfile(resources.session, hand, &profileState))
                || profileState.interactionProfile == XR_NULL_PATH) {
                return;
            }
            std::uint32_t required = 0;
            if (XR_FAILED(xrPathToString(runtime.instance, profileState.interactionProfile, 0, &required, nullptr))
                || required == 0U) {
                return;
            }
            std::vector<char> text(required);
            if (XR_SUCCEEDED(xrPathToString(
                    runtime.instance, profileState.interactionProfile, required, &required, text.data()))) {
                output = text.data();
            }
        };
        if (report.leftInteractionProfile.empty()) resolveProfile(leftHand, report.leftInteractionProfile);
        if (report.rightInteractionProfile.empty()) resolveProfile(rightHand, report.rightInteractionProfile);
        const auto countActivePose = [&](const XrAction action,
                                         const XrPath hand,
                                         std::uint32_t& counter) {
            const XrActionStateGetInfo stateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, action, hand};
            XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};
            if (XR_SUCCEEDED(xrGetActionStatePose(resources.session, &stateInfo, &state))
                && state.isActive != 0) {
                ++counter;
            }
        };
        countActivePose(runtime.aimPose, leftHand, report.leftPoseActionActiveFrames);
        countActivePose(runtime.aimPose, rightHand, report.rightPoseActionActiveFrames);
        if (report.aimPoseBoundSourceCount == 0U) {
            const XrBoundSourcesForActionEnumerateInfo sourceInfo{
                XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO, nullptr, runtime.aimPose};
            std::uint32_t sourceCount = 0;
            if (XR_SUCCEEDED(xrEnumerateBoundSourcesForAction(
                    resources.session, &sourceInfo, 0, &sourceCount, nullptr))) {
                report.aimPoseBoundSourceCount = sourceCount;
            }
        }
        XrSpaceLocation leftAimLocation{XR_TYPE_SPACE_LOCATION};
        XrSpaceLocation rightAimLocation{XR_TYPE_SPACE_LOCATION};
        const auto countTracked = [&](const XrSpace space,
                                      std::uint32_t& counter,
                                      XrSpaceLocation& location) {
            if (XR_SUCCEEDED(xrLocateSpace(
                    space, resources.appSpace, frameState.predictedDisplayTime, &location))
                && (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0U
                && (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0U) {
                ++counter;
            }
        };
        countTracked(resources.leftAimSpace, report.leftControllerTrackedFrames, leftAimLocation);
        countTracked(resources.rightAimSpace, report.rightControllerTrackedFrames, rightAimLocation);
        std::uint32_t dynamicVertexCount = 0;
        if (renderHardwareScene && scene->updateInteraction != nullptr) {
            HardwareInteractionFrameInput interactionInput{};
            interactionInput.deltaSeconds = frameDeltaSeconds;
            const std::array<const XrSpaceLocation*, 2> aimLocations{
                &leftAimLocation, &rightAimLocation};
            for (std::size_t handIndex = 0; handIndex < aimLocations.size(); ++handIndex) {
                HardwareInteractionHandInput& handInput = interactionInput.hands[handIndex];
                const XrSpaceLocation& location = *aimLocations[handIndex];
                constexpr XrSpaceLocationFlags required = XR_SPACE_LOCATION_POSITION_VALID_BIT
                    | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                handInput.tracked = (location.locationFlags & required) == required;
                const XrActionStateBoolean& selectState = selectStates[handIndex];
                handInput.selectHeld = selectState.isActive != 0 && selectState.currentState != 0;
                handInput.selectPressed = selectState.isActive != 0
                    && selectState.changedSinceLastSync != 0 && selectState.currentState != 0;
                handInput.selectReleased = selectState.changedSinceLastSync != 0
                    && selectState.currentState == 0;
                handInput.grabHeld = grabActionActive[handIndex]
                    ? squeezeHeld[handIndex] : handInput.selectHeld;
                handInput.grabPressed = grabActionActive[handIndex]
                    ? grabPressed[handIndex] : handInput.selectPressed;
                handInput.grabReleased = grabActionActive[handIndex]
                    ? grabReleased[handIndex] : handInput.selectReleased;
                const XrActionStateBoolean& teleportState = teleportStates[handIndex];
                handInput.teleportHeld = teleportState.isActive != 0
                    && teleportState.currentState != 0;
                handInput.teleportPressed = teleportState.isActive != 0
                    && teleportState.changedSinceLastSync != 0
                    && teleportState.currentState != 0;
                handInput.teleportReleased = teleportState.changedSinceLastSync != 0
                    && teleportState.currentState == 0;
                if (!handInput.tracked) continue;
                const std::array<float, 3> worldOrigin = AppToWorldPoint(
                    {location.pose.position.x, location.pose.position.y, location.pose.position.z},
                    locomotionOrigin,
                    locomotionYawDegrees);
                const std::array<float, 3> appDirection = RotateVector(
                    location.pose.orientation, {0.0f, 0.0f, -1.0f});
                const std::array<float, 3> worldDirection = AppToWorldVector(
                    appDirection, locomotionYawDegrees);
                std::copy(worldOrigin.begin(), worldOrigin.end(), handInput.aimOrigin);
                std::copy(worldDirection.begin(), worldDirection.end(), handInput.aimDirection);
            }
            const HardwareInteractionFrameOutput interactionOutput = scene->updateInteraction(
                scene->interactionUser, interactionInput);
            if (interactionOutput.vertexCount > scene->dynamicVertexCapacity
                || (interactionOutput.vertexCount > 0U
                    && (interactionOutput.vertices == nullptr
                        || resources.mappedDynamicVertices == nullptr))) {
                error = "OpenXR interaction callback exceeded its dynamic vertex contract.";
                return false;
            }
            dynamicVertexCount = static_cast<std::uint32_t>(interactionOutput.vertexCount);
            if (dynamicVertexCount > 0U) {
                std::memcpy(
                    resources.mappedDynamicVertices,
                    interactionOutput.vertices,
                    interactionOutput.vertexCount * sizeof(HardwareSceneVertex));
                ++report.dynamicSceneFrames;
            }
            if (interactionOutput.teleportRequested
                && std::isfinite(interactionOutput.teleportDestinationFeet[0])
                && std::isfinite(interactionOutput.teleportDestinationFeet[1])
                && std::isfinite(interactionOutput.teleportDestinationFeet[2])) {
                std::copy_n(
                    interactionOutput.teleportDestinationFeet,
                    3,
                    locomotionOrigin.begin());
                ++report.teleportCount;
            }
            for (std::size_t handIndex = 0; handIndex < handPaths.size(); ++handIndex) {
                const float amplitude = std::clamp(
                    interactionOutput.hapticAmplitude[handIndex], 0.0f, 1.0f);
                const float durationSeconds = std::clamp(
                    interactionOutput.hapticDurationSeconds[handIndex], 0.0f, 1.0f);
                if (!(amplitude > 0.0f) || !(durationSeconds > 0.0f)
                    || sessionState != XR_SESSION_STATE_FOCUSED
                    || frameState.predictedDisplayTime < nextHapticTime[handIndex]) continue;
                const XrHapticActionInfo hapticInfo{
                    XR_TYPE_HAPTIC_ACTION_INFO, nullptr, runtime.haptic, handPaths[handIndex]};
                const XrHapticVibration vibration{
                    XR_TYPE_HAPTIC_VIBRATION,
                    nullptr,
                    static_cast<XrDuration>(durationSeconds * 1.0e9f),
                    XR_FREQUENCY_UNSPECIFIED,
                    amplitude};
                if (XR_SUCCEEDED(xrApplyHapticFeedback(
                        resources.session,
                        &hapticInfo,
                        reinterpret_cast<const XrHapticBaseHeader*>(&vibration)))) {
                    ++report.hapticPulseCount;
                    const float cooldownSeconds = std::max(0.060f, durationSeconds);
                    nextHapticTime[handIndex] = frameState.predictedDisplayTime
                        + static_cast<XrDuration>(cooldownSeconds * 1.0e9f);
                }
            }
        }
        if (locateHandJoints != nullptr && runtime.info.handTracking) {
            const XrHandJointsLocateInfoEXT handLocateInfo{
                XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
                nullptr,
                resources.appSpace,
                frameState.predictedDisplayTime};
            const auto countArticulatedHand = [&](const XrHandTrackerEXT tracker, std::uint32_t& counter) {
                std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT> joints{};
                XrHandJointLocationsEXT locations{
                    XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
                    nullptr,
                    0,
                    static_cast<std::uint32_t>(joints.size()),
                    joints.data()};
                if (tracker != XR_NULL_HANDLE
                    && XR_SUCCEEDED(locateHandJoints(tracker, &handLocateInfo, &locations))
                    && locations.isActive != 0) {
                    const XrSpaceLocationFlags wristFlags = joints[XR_HAND_JOINT_WRIST_EXT].locationFlags;
                    if ((wristFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0U
                        && (wristFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0U) {
                        ++counter;
                    }
                }
            };
            countArticulatedHand(resources.leftHandTracker, report.leftArticulatedHandFrames);
            countArticulatedHand(resources.rightHandTracker, report.rightArticulatedHandFrames);
        }

        std::array<XrCompositionLayerProjectionView, 2> projectionViews{};
        std::array<const XrCompositionLayerBaseHeader*, 1> layers{};
        XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        bool submittedProjection = false;
        if (frameState.shouldRender != 0 && stereoValid) {
            std::uint32_t controllerVertexCount = 0;
            const auto appendTrackedController = [&](const XrSpaceLocation& location,
                                                     const std::array<float, 3>& color) {
                constexpr XrSpaceLocationFlags required = XR_SPACE_LOCATION_POSITION_VALID_BIT
                    | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                if ((location.locationFlags & required) != required) return;
                auto* vertices = static_cast<HardwareSceneVertex*>(resources.mappedControllerVertices);
                controllerVertexCount += BuildTrackedControllerVertices(
                    location,
                    locomotionOrigin,
                    locomotionYawDegrees,
                    color,
                    vertices + controllerVertexCount);
            };
            if (renderHardwareScene && resources.mappedControllerVertices != nullptr) {
                appendTrackedController(leftAimLocation, {0.10f, 0.82f, 1.0f});
                appendTrackedController(rightAimLocation, {1.0f, 0.34f, 0.12f});
            }
            std::uint32_t imageIndex = 0;
            const XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrResult = xrAcquireSwapchainImage(resources.swapchain, &acquireInfo, &imageIndex);
            const XrSwapchainImageWaitInfo imageWaitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, nullptr, XR_INFINITE_DURATION};
            if (XR_SUCCEEDED(xrResult)) xrResult = xrWaitSwapchainImage(resources.swapchain, &imageWaitInfo);
            if (XR_FAILED(xrResult) || imageIndex >= images.size()) {
                error = "OpenXR swapchain image acquisition failed: " + ResultText(runtime.instance, xrResult);
                return false;
            }

            vkResetFences(resources.device, 1, &resources.fence);
            vkResetCommandBuffer(commandBuffer, 0);
            const VkCommandBufferBeginInfo commandBegin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vkBeginCommandBuffer(commandBuffer, &commandBegin);
            if (renderHardwareScene) {
                if (!initialized[imageIndex]) {
                    VkImageMemoryBarrier toColor{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    toColor.srcAccessMask = 0;
                    toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    toColor.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    toColor.image = images[imageIndex].image;
                    toColor.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2};
                    vkCmdPipelineBarrier(
                        commandBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &toColor);
                }
                const VkViewport viewport{
                    0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
                const VkRect2D scissor{{0, 0}, {width, height}};
                const std::array<VkClearValue, 2> clears{{
                    {.color = {{0.025f, 0.032f, 0.045f, 1.0f}}},
                    {.depthStencil = {1.0f, 0}}}};
                const VkDeviceSize vertexOffset = 0;
                const Matrix4 worldToApp = BuildWorldToAppMatrix(
                    locomotionOrigin, locomotionYawDegrees);
                for (std::uint32_t eye = 0; eye < 2U; ++eye) {
                    const VkRenderPassBeginInfo renderPassBegin{
                        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                        nullptr,
                        resources.sceneRenderPass,
                        resources.framebuffers[imageIndex * 2U + eye],
                        {{0, 0}, {width, height}},
                        static_cast<std::uint32_t>(clears.size()),
                        clears.data()};
                    vkCmdBeginRenderPass(commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
                    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.scenePipeline);
                    vkCmdBindDescriptorSets(
                        commandBuffer,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        resources.scenePipelineLayout,
                        0,
                        1,
                        &resources.sceneDescriptorSet,
                        0,
                        nullptr);
                    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
                    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
                    vkCmdBindVertexBuffers(
                        commandBuffer, 0, 1, &resources.sceneVertexBuffer, &vertexOffset);
                    const Matrix4 view = BuildInversePoseMatrix(locatedViews[eye].pose);
                    const Matrix4 projection = BuildProjectionMatrix(
                        locatedViews[eye].fov,
                        std::max(scene->nearClip, 0.001f),
                        std::max(scene->farClip, scene->nearClip + 0.01f));
                    XrEyePushConstants eyeData{};
                    eyeData.viewProjection = ToShaderColumnMajor(
                        MultiplyMatrix(projection, MultiplyMatrix(view, worldToApp)));
                    const std::array<float, 3> eyeWorld = AppToWorldPoint(
                        {locatedViews[eye].pose.position.x,
                         locatedViews[eye].pose.position.y,
                         locatedViews[eye].pose.position.z},
                        locomotionOrigin,
                        locomotionYawDegrees);
                    eyeData.cameraWorldPosition = {eyeWorld[0], eyeWorld[1], eyeWorld[2], 1.0f};
                    vkCmdPushConstants(
                        commandBuffer,
                        resources.scenePipelineLayout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0,
                        sizeof(eyeData),
                        &eyeData);
                    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(scene->vertexCount), 1, 0, 0);
                    if (dynamicVertexCount > 0U) {
                        vkCmdBindVertexBuffers(
                            commandBuffer,
                            0,
                            1,
                            &resources.dynamicVertexBuffer,
                            &vertexOffset);
                        vkCmdDraw(commandBuffer, dynamicVertexCount, 1, 0, 0);
                    }
                    if (controllerVertexCount > 0U) {
                        vkCmdBindVertexBuffers(
                            commandBuffer,
                            0,
                            1,
                            &resources.controllerVertexBuffer,
                            &vertexOffset);
                        vkCmdDraw(commandBuffer, controllerVertexCount, 1, 0, 0);
                    }
                    vkCmdEndRenderPass(commandBuffer);
                }
            } else {
                VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                toTransfer.srcAccessMask = initialized[imageIndex] ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
                toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                toTransfer.oldLayout = initialized[imageIndex]
                    ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    : VK_IMAGE_LAYOUT_UNDEFINED;
                toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                toTransfer.image = images[imageIndex].image;
                toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2};
                vkCmdPipelineBarrier(
                    commandBuffer,
                    initialized[imageIndex]
                        ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                        : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &toTransfer);
                const float pulse = static_cast<float>(report.submittedFrames % 180U) / 179.0f;
                const VkClearColorValue clearColor{{
                    0.035f + pulse * 0.08f,
                    0.012f,
                    0.018f + (1.0f - pulse) * 0.05f,
                    1.0f}};
                vkCmdClearColorImage(
                    commandBuffer,
                    images[imageIndex].image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &clearColor,
                    1,
                    &toTransfer.subresourceRange);
                VkImageMemoryBarrier toPresent = toTransfer;
                toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                toPresent.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                toPresent.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &toPresent);
            }
            vkEndCommandBuffer(commandBuffer);
            const VkSubmitInfo submitInfo{
                VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &commandBuffer, 0, nullptr};
            if (vkQueueSubmit(queue, 1, &submitInfo, resources.fence) != VK_SUCCESS
                || vkWaitForFences(resources.device, 1, &resources.fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
                error = "Vulkan failed while clearing an OpenXR swapchain image.";
                return false;
            }
            initialized[imageIndex] = true;
            const XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrResult = xrReleaseSwapchainImage(resources.swapchain, &releaseInfo);
            if (XR_FAILED(xrResult)) {
                error = "xrReleaseSwapchainImage failed: " + ResultText(runtime.instance, xrResult);
                return false;
            }

            for (std::uint32_t eye = 0; eye < 2U; ++eye) {
                projectionViews[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                projectionViews[eye].pose = locatedViews[eye].pose;
                projectionViews[eye].fov = locatedViews[eye].fov;
                projectionViews[eye].subImage = {
                    resources.swapchain,
                    {{0, 0}, {static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)}},
                    eye};
            }
            projectionLayer.space = resources.appSpace;
            projectionLayer.viewCount = 2;
            projectionLayer.views = projectionViews.data();
            layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer);
            submittedProjection = true;
            ++report.validStereoFrames;
        }

        const XrFrameEndInfo endInfo{
            XR_TYPE_FRAME_END_INFO,
            nullptr,
            frameState.predictedDisplayTime,
            XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
            submittedProjection ? 1U : 0U,
            submittedProjection ? layers.data() : nullptr};
        xrResult = xrEndFrame(resources.session, &endInfo);
        if (XR_FAILED(xrResult)) {
            error = "xrEndFrame failed: " + ResultText(runtime.instance, xrResult);
            return false;
        }
        ++report.submittedFrames;
    }

    if (sessionRunning) {
        xrRequestExitSession(resources.session);
    }
    if (report.submittedFrames != maximumFrames) {
        error = "OpenXR session stopped after " + std::to_string(report.submittedFrames)
            + " of " + std::to_string(maximumFrames) + " requested frames.";
        return false;
    }
    if (report.validStereoFrames == 0U) {
        error = "OpenXR completed without producing a valid tracked stereo frame.";
        return false;
    }
    error.clear();
    return true;
}

} // namespace ri::xr
