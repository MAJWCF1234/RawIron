#include "EditorVulkanViewport.h"

#include "RawIron/Render/VulkanPreviewPresenter.h"
#include "RawIron/Render/VulkanScenePreviewBridge.h"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <windowsx.h>

namespace ri::editor {

#if defined(_WIN32)
namespace {

constexpr wchar_t kEditorVulkanHostClassName[] = L"RawIronEditorVulkanViewportHost";

void EnsureHostClassRegistered() {
    static std::once_flag once{};
    std::call_once(once, []() {
        WNDCLASSW wc{};
        wc.lpfnWndProc = &EditorVulkanViewport::HostWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kEditorVulkanHostClassName;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        RegisterClassW(&wc);
    });
}

}

struct EditorVulkanViewport::Snapshot {
    std::uint64_t publishSequence = 0;
    std::shared_ptr<const ri::scene::Scene> scene{};
    const void* sceneCacheIdentity = nullptr;
    int cameraNode = ri::scene::kInvalidHandle;
    ri::render::software::ScenePreviewOptions previewOptions{};
    double animationTimeSeconds = 0.0;
};

EditorVulkanViewport::~EditorVulkanViewport() {
    Stop();
    DestroyHostWindow();
}

bool EditorVulkanViewport::Start(const HWND parent, const RECT& bounds) {
    Stop();
    if (!StartRenderLoop(parent, bounds)) {
        return false;
    }
    restartThread_ = std::jthread([this](const std::stop_token stopToken) {
        RunRestartWorker(stopToken);
    });
    return true;
}

bool EditorVulkanViewport::StartRenderLoop(const HWND parent, const RECT& bounds) {
    const int width = std::max(64L, bounds.right - bounds.left);
    const int height = std::max(64L, bounds.bottom - bounds.top);
    parent_ = parent;
    if (!EnsureHostWindow(parent_)) {
        return false;
    }
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    SetBounds(bounds);
    stopRequested_.store(false);
    running_.store(true);
    {
        std::scoped_lock lock(errorMutex_);
        lastError_.clear();
    }

    renderThread_ = std::jthread([this, width, height]() {
        ri::render::vulkan::VulkanPreviewWindowOptions options{};
        options.windowTitle = "RawIron Editor Vulkan Viewport";
        options.clientHwnd = child_.load();
        options.messageUserData = this;
        options.presentModePreference = ri::render::vulkan::VulkanPresentModePreference::Mailbox;
        options.enableHybridHdrPresentation = true;
        options.initialRenderQualityTier = 1;

        std::string error;
        const bool ok = ri::render::vulkan::RunVulkanNativeSceneLoop(
            width,
            height,
            [this](ri::render::vulkan::VulkanNativeSceneFrame& frame, std::string* frameError) {
                if (stopRequested_.load()) {
                    if (frameError != nullptr) {
                        *frameError = "Editor Vulkan viewport stopped.";
                    }
                    return false;
                }
                const std::shared_ptr<const Snapshot> snapshot = snapshot_.load();
                if (!snapshot || !snapshot->scene) {
                    if (frameError != nullptr) {
                        *frameError = "Editor Vulkan viewport is waiting for a scene snapshot.";
                    }
                    return false;
                }
                frame.sceneOwner = snapshot->scene;
                frame.scene = snapshot->scene.get();
                frame.sceneCacheIdentity = snapshot->sceneCacheIdentity;
                frame.frameSequence = snapshot->publishSequence;
                frame.cameraNode = snapshot->cameraNode;
                frame.textureRoot = snapshot->previewOptions.textureRoot.value_or(std::filesystem::path{});
                frame.animationTimeSeconds = snapshot->animationTimeSeconds;
                frame.renderQualityTier = 1;
                ri::render::vulkan::ApplyScenePreviewAtmosphereToVulkanFrame(snapshot->previewOptions, frame);
                ri::render::vulkan::OverlayScenePreviewPostProcessOnParameters(
                    snapshot->previewOptions,
                    frame.postProcess);
                return true;
            },
            options,
            &error);

        if (!ok && !stopRequested_.load()) {
            std::scoped_lock lock(errorMutex_);
            lastError_ = error;
            std::ofstream("Saved/EditorVulkanViewport.log", std::ios::trunc)
                << error << '\n';
        }
        running_.store(false);
    });

    return renderThread_.joinable();
}

void EditorVulkanViewport::RestartAsync(const HWND parent, const RECT& bounds) {
    {
        std::scoped_lock lock(restartMutex_);
        restartQueued_ = true;
        restartParent_ = parent;
        restartBounds_ = bounds;
    }
    restartCv_.notify_one();
}

void EditorVulkanViewport::Stop() {
    {
        std::scoped_lock lock(restartMutex_);
        restartQueued_ = false;
    }
    if (restartThread_.joinable()) {
        restartThread_.request_stop();
        restartCv_.notify_all();
        restartThread_.join();
    }
    StopRenderLoop();
    restartWorkerRunning_.store(false);
}

void EditorVulkanViewport::StopRenderLoop() {
    stopRequested_.store(true);
    if (renderThread_.joinable()) {
        renderThread_.join();
    }
    running_.store(false);
}

void EditorVulkanViewport::RunRestartWorker(const std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        HWND parent = nullptr;
        RECT bounds{};
        {
            std::unique_lock lock(restartMutex_);
            restartCv_.wait(lock, stopToken, [this]() { return restartQueued_; });
            if (stopToken.stop_requested()) {
                break;
            }
            restartQueued_ = false;
            parent = restartParent_;
            bounds = restartBounds_;
        }

        restartWorkerRunning_.store(true);
        StopRenderLoop();
        if (!stopToken.stop_requested()) {
            (void)StartRenderLoop(parent, bounds);
        }
        restartWorkerRunning_.store(false);
    }
}

void EditorVulkanViewport::SetBounds(const RECT& bounds) {
    const HWND child = child_.load();
    if (child == nullptr || !IsWindow(child)) {
        return;
    }
    SetWindowPos(child,
                 nullptr,
                 bounds.left,
                 bounds.top,
                 std::max(1L, bounds.right - bounds.left),
                 std::max(1L, bounds.bottom - bounds.top),
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
}

void EditorVulkanViewport::SetVisible(const bool visible) {
    const HWND child = child_.load();
    if (child == nullptr || !IsWindow(child)) {
        return;
    }
    ShowWindow(child, visible ? SW_SHOW : SW_HIDE);
}

void EditorVulkanViewport::Publish(
    const ri::scene::Scene& scene,
    const int cameraNode,
    const ri::render::software::ScenePreviewOptions& previewOptions,
    const double animationTimeSeconds,
    const bool sceneContentDirty) {
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->publishSequence = publishSequence_.fetch_add(1);
    snapshot->sceneCacheIdentity = &scene;
    if (!sceneContentDirty) {
        const std::shared_ptr<const Snapshot> previous = snapshot_.load();
        if (previous && previous->scene && previous->sceneCacheIdentity == snapshot->sceneCacheIdentity) {
            snapshot->scene = previous->scene;
        }
    }
    if (!snapshot->scene) {
        snapshot->scene = std::make_shared<ri::scene::Scene>(scene);
    }
    snapshot->cameraNode = cameraNode;
    snapshot->previewOptions = previewOptions;
    snapshot->animationTimeSeconds = animationTimeSeconds;
    snapshot_.store(std::move(snapshot));
}

bool EditorVulkanViewport::Running() const noexcept {
    return running_.load();
}

bool EditorVulkanViewport::RestartInFlight() const noexcept {
    return restartWorkerRunning_.load();
}

HWND EditorVulkanViewport::ChildHwnd() const noexcept {
    return child_.load();
}

std::string EditorVulkanViewport::LastError() const {
    std::scoped_lock lock(errorMutex_);
    return lastError_;
}

bool EditorVulkanViewport::EnsureHostWindow(const HWND parent) {
    EnsureHostClassRegistered();
    HWND child = child_.load();
    if (child != nullptr && IsWindow(child)) {
        return true;
    }
    child = CreateWindowExW(0,
                            kEditorVulkanHostClassName,
                            L"",
                            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                            0,
                            0,
                            64,
                            64,
                            parent,
                            nullptr,
                            GetModuleHandleW(nullptr),
                            this);
    if (child == nullptr) {
        std::scoped_lock lock(errorMutex_);
        lastError_ = "CreateWindowExW failed for editor Vulkan host window.";
        return false;
    }
    child_.store(child);
    return true;
}

void EditorVulkanViewport::DestroyHostWindow() {
    const HWND child = child_.load();
    if (child != nullptr && IsWindow(child)) {
        DestroyWindow(child);
    }
    child_.store(nullptr);
}

LRESULT CALLBACK EditorVulkanViewport::HostWindowProc(
    const HWND hwnd,
    const UINT message,
    const WPARAM wParam,
    const LPARAM lParam) {
    auto* self = reinterpret_cast<EditorVulkanViewport*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<LPCREATESTRUCTW>(lParam);
        self = static_cast<EditorVulkanViewport*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self != nullptr) {
        switch (message) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
            ForwardChildMessage(self, hwnd, message, static_cast<std::uint64_t>(wParam), static_cast<std::int64_t>(lParam));
            break;
        default:
            break;
        }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void EditorVulkanViewport::ForwardChildMessage(
    void* user,
    void* hwndVoid,
    const unsigned int message,
    const std::uint64_t wParam,
    const std::int64_t lParam) {
    auto* self = static_cast<EditorVulkanViewport*>(user);
    const HWND child = static_cast<HWND>(hwndVoid);
    if (self == nullptr || self->parent_ == nullptr) {
        return;
    }
    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MOUSEMOVE: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        MapWindowPoints(child, self->parent_, &point, 1);
        PostMessageW(self->parent_, message, static_cast<WPARAM>(wParam), MAKELPARAM(point.x, point.y));
        break;
    }
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
        PostMessageW(self->parent_, message, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
        break;
    default:
        break;
    }
}
#endif

} // namespace ri::editor
