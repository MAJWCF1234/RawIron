#include "RawIron/Runtime/DesktopMouseLook.h"
#include <algorithm>
#include <cmath>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ri::runtime {
DesktopMouseLook::~DesktopMouseLook() { Release(); }
void DesktopMouseLook::Release() {
#if defined(_WIN32)
    if (captured_) ClipCursor(nullptr);
    if (hidden_) ShowCursor(TRUE);
#endif
    captured_ = hidden_ = false;
    x_ = y_ = 0;
}
void DesktopMouseLook::HandleMessage(void* window, unsigned message, std::uint64_t wParam, std::int64_t lParam) {
#if defined(_WIN32)
    const auto hwnd = static_cast<HWND>(window);
    if (!hwnd) return;
    if (message == WM_CREATE) {
        RAWINPUTDEVICE device{0x01,0x02,0,hwnd};
        RegisterRawInputDevices(&device,1,sizeof(device));
    } else if (message == WM_KILLFOCUS || message == WM_CANCELMODE || message == WM_DESTROY
        || (message == WM_ACTIVATEAPP && !wParam)) {
        Release();
    } else if (message == WM_INPUT && GetForegroundWindow() == hwnd && captured_) {
        const auto handle = reinterpret_cast<HRAWINPUT>(lParam);
        UINT size = 0;
        if (GetRawInputData(handle,RID_INPUT,nullptr,&size,sizeof(RAWINPUTHEADER)) == UINT(-1)
            || size < sizeof(RAWINPUT) || size > 4096) return;
        std::vector<std::uint8_t> bytes(size);
        if (GetRawInputData(handle,RID_INPUT,bytes.data(),&size,sizeof(RAWINPUTHEADER)) == UINT(-1)) return;
        const auto& raw = *reinterpret_cast<const RAWINPUT*>(bytes.data());
        if (raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
            x_ += static_cast<float>(raw.data.mouse.lLastX);
            y_ += static_cast<float>(raw.data.mouse.lLastY);
        }
    }
#else
    (void)window; (void)message; (void)wParam; (void)lParam;
#endif
}
void DesktopMouseLook::Update(void* window, float& yaw, float& pitch, float sensitivity) {
#if defined(_WIN32)
    const auto hwnd = static_cast<HWND>(window);
    if (!hwnd || GetForegroundWindow()!=hwnd || IsIconic(hwnd)) { Release(); return; }
    RECT client{};
    if (!GetClientRect(hwnd,&client)) { Release(); return; }
    POINT top{client.left,client.top}, bottom{client.right,client.bottom};
    ClientToScreen(hwnd,&top); ClientToScreen(hwnd,&bottom);
    const RECT clip{top.x,top.y,bottom.x,bottom.y};
    if (!captured_) {
        captured_ = ClipCursor(&clip) != FALSE;
        x_ = y_ = 0;
        if (captured_) { ShowCursor(FALSE); hidden_=true; }
        return;
    }
    ClipCursor(&clip); // Follow window movement/resizing while captured.
    if (std::isfinite(x_) && std::isfinite(y_) && std::isfinite(sensitivity)) {
        yaw = std::remainder(yaw + x_*sensitivity,360.0f);
        pitch = std::clamp(pitch + y_*sensitivity*.84f,-80.0f,78.0f);
    }
    x_=y_=0;
#else
    (void)window; (void)yaw; (void)pitch; (void)sensitivity;
#endif
}
} // namespace ri::runtime
