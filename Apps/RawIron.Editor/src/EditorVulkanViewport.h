#pragma once

#include "RawIron/Render/ScenePreview.h"
#include "RawIron/Scene/Scene.h"

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ri::editor {

#if defined(_WIN32)
struct EditorVulkanResourceStats {
    std::size_t descriptorPoolCount = 0;
    std::size_t allocatedDescriptorSetCount = 0;
    std::size_t cachedDescriptorCount = 0;
    std::size_t uploadedTextureCount = 0;
    std::size_t missingTextureFallbackCount = 0;
    std::size_t descriptorAllocationFailureCount = 0;
};

class EditorVulkanViewport {
public:
    EditorVulkanViewport() = default;
    ~EditorVulkanViewport();

    EditorVulkanViewport(const EditorVulkanViewport&) = delete;
    EditorVulkanViewport& operator=(const EditorVulkanViewport&) = delete;

    bool Start(HWND parent, const RECT& bounds);
    void RestartAsync(HWND parent, const RECT& bounds);
    void Stop();
    void SetBounds(const RECT& bounds);
    void SetVisible(bool visible);
    void Publish(const ri::scene::Scene& scene,
                 int cameraNode,
                 const ri::render::software::ScenePreviewOptions& previewOptions,
                 double animationTimeSeconds,
                 bool sceneContentDirty);

    [[nodiscard]] bool Running() const noexcept;
    [[nodiscard]] bool RestartInFlight() const noexcept;
    [[nodiscard]] HWND ChildHwnd() const noexcept;
    [[nodiscard]] std::string LastError() const;
    [[nodiscard]] EditorVulkanResourceStats ResourceStats() const;
    static LRESULT CALLBACK HostWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    struct Snapshot;
    bool StartRenderLoop(HWND parent, const RECT& bounds);
    void StopRenderLoop();
    void RunRestartWorker(std::stop_token stopToken);
    bool EnsureHostWindow(HWND parent);
    void DestroyHostWindow();

    static void ForwardChildMessage(void* user,
                                    void* hwnd,
                                    unsigned int message,
                                    std::uint64_t wParam,
                                    std::int64_t lParam);

    HWND parent_ = nullptr;
    int surfaceWidth_ = 0;
    int surfaceHeight_ = 0;
    std::atomic<HWND> child_{nullptr};
    std::atomic<bool> running_{false};
    std::atomic<bool> restartWorkerRunning_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<std::uint64_t> publishSequence_{1};
    std::atomic<std::shared_ptr<const Snapshot>> snapshot_{};
    std::jthread renderThread_{};
    std::jthread restartThread_{};
    std::mutex restartMutex_{};
    std::condition_variable_any restartCv_{};
    bool restartQueued_ = false;
    HWND restartParent_ = nullptr;
    RECT restartBounds_{};
    std::mutex boundsMutex_{};
    RECT lastBounds_{};
    bool lastBoundsValid_ = false;
    mutable std::mutex errorMutex_{};
    std::string lastError_{};
    mutable std::mutex resourceStatsMutex_{};
    EditorVulkanResourceStats resourceStats_{};
};
#endif

} // namespace ri::editor
