if(CMAKE_CONFIGURATION_TYPES)
  set(rawiron_visual_shell_is_multi_config 1)
else()
  set(rawiron_visual_shell_is_multi_config 0)
endif()

rawiron_add_app(
  TARGET RawIron.VisualShell
  SOURCES
    src/main.cpp
    src/VisualShellApp.cpp
    src/VisualShellDesktop.cpp
    src/VisualShellUiDraw.cpp
    src/VisualShellLauncherArt.cpp
    src/ShellWorkshop.cpp
    src/WorkshopForgeGpu.cpp
  LIBRARIES
    RawIron::Core
    RawIron::SceneUtilities
  WIN32_LIBRARIES
    user32
    gdi32
    shell32
    Gdiplus
    d3d11
    dxgi
    d3dcompiler
  COMPILE_DEFINITIONS
    RAWIRON_CTEST_COMMAND="${CMAKE_CTEST_COMMAND}"
    RAWIRON_BUILD_CONFIGURATION="$<CONFIG>"
    RAWIRON_IS_MULTI_CONFIG=${rawiron_visual_shell_is_multi_config}
  EXPECTED_STRINGS
    "[Shell Ready]"
    "RawIron Visual Shell"
    "Preview Window [ready]"
    "Scene Kit Checks [ready]"
    "Vulkan Diagnostics [ready]"
    "Full Test Sweep [ready]"
  FORBIDDEN_STRINGS
    "failed"
    "exception"
  SMOKE_ARGS
    "--headless"
)
