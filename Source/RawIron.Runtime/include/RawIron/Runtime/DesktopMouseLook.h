#pragma once

#include <cstdint>

namespace ri::runtime {
// Engine-owned raw mouse capture. Hidden/unfocused hosts never consume global input
// or release another window's cursor confinement.
class DesktopMouseLook {
public:
    DesktopMouseLook() = default;
    DesktopMouseLook(const DesktopMouseLook&) = delete;
    DesktopMouseLook& operator=(const DesktopMouseLook&) = delete;
    ~DesktopMouseLook();
    void HandleMessage(void* window, unsigned message, std::uint64_t wParam, std::int64_t lParam);
    void Update(void* window, float& yaw, float& pitch, float sensitivity);
    void Release();
private:
    bool captured_ = false;
    bool hidden_ = false;
    float x_ = 0, y_ = 0;
};
} // namespace ri::runtime
