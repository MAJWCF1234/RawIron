#include "RawIron/XR/OpenXrRuntime.h"

#include <cstdlib>
#include <string>

int main() {
    ri::xr::OpenXrRuntime runtime;
    std::string diagnostic;
    const bool ready = runtime.Initialize("Raw Iron OpenXR Smoke", diagnostic);
    if (ready) {
        if (!runtime.IsSystemReady() || runtime.Info().stereoViews.size() != 2U
            || !runtime.Info().vulkanEnable2) {
            return EXIT_FAILURE;
        }
        runtime.PollEvents();
    } else if (diagnostic.empty()) {
        // A missing runtime/HMD is valid for CI, but it must always be diagnosed.
        return EXIT_FAILURE;
    }
    runtime.Shutdown();
    return EXIT_SUCCESS;
}
