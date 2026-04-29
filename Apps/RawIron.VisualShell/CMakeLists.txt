add_executable(RawIron.VisualShell
  src/main.cpp
  src/VisualShellApp.cpp
  src/VisualShellDesktop.cpp
  src/VisualShellUiDraw.cpp
  src/VisualShellLauncherArt.cpp
  src/ShellWorkshop.cpp
  src/WorkshopForgeGpu.cpp
)

target_link_libraries(RawIron.VisualShell
  PRIVATE
    RawIron::Core
    RawIron::SceneUtilities
)

if (WIN32)
  target_link_libraries(RawIron.VisualShell
    PRIVATE
    user32
    gdi32
    shell32
    Gdiplus
    d3d11
    dxgi
    d3dcompiler
  )
endif()

target_compile_features(RawIron.VisualShell PRIVATE cxx_std_20)

target_compile_definitions(RawIron.VisualShell
  PRIVATE
    RAWIRON_CTEST_COMMAND="${CMAKE_CTEST_COMMAND}"
)

if(CMAKE_CONFIGURATION_TYPES)
  set(rawiron_visual_shell_is_multi_config 1)
else()
  set(rawiron_visual_shell_is_multi_config 0)
endif()

target_compile_definitions(RawIron.VisualShell
  PRIVATE
    RAWIRON_BUILD_CONFIGURATION="$<CONFIG>"
    RAWIRON_IS_MULTI_CONFIG=${rawiron_visual_shell_is_multi_config}
)

rawiron_stage_runtime_dlls(RawIron.VisualShell)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  set(rawiron_visual_shell_expected
    "[Shell Ready]"
    "RawIron Visual Shell"
    "Preview Window [ready]"
    "Scene Kit Checks [ready]"
    "Vulkan Diagnostics [ready]"
    "Full Test Sweep [ready]"
  )

  add_test(
    NAME RawIron.VisualShell.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.VisualShell>"
      "-DCOMMAND_ARGS=--headless"
      "-DEXPECTED_STRINGS=${rawiron_visual_shell_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      -P "${rawiron_verify_command_output}"
  )
endif()
