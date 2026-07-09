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

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_visual_shell_runtime_dll_arg RawIron.VisualShell)
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}" RAWIRON_VISUAL_SHELL_SOURCE_ROOT_NATIVE)
  set(rawiron_visual_shell_actions_expected
    "[Shell Actions]"
    "List Projects"
    "Workspace Check"
    "Asset Formats"
    "Rigging Toolkit"
    "Kind: captured"
  )
  set(rawiron_visual_shell_projects_expected
    "[Shell Console]"
    "Tool:"
    "project id=liminal-hall"
    "project id=rawiron-multiplayer-sandbox"
    "project id=wilderness-ruins"
  )
  set(rawiron_visual_shell_describe_expected
    "Project id: rawiron-multiplayer-sandbox"
    "Runtime module: RawIron.Game.MultiplayerSandbox"
    "Format validation: pass"
  )
  set(rawiron_visual_shell_doctor_expected
    "[Shell Console]"
    "Doctor project id: rawiron-multiplayer-sandbox"
    "Doctor result: healthy"
  )
  set(rawiron_visual_shell_probe_editor_expected
    "[Shell Probe]"
    "Mode: open-editor"
    "Project id: rawiron-multiplayer-sandbox"
    "Ready: yes"
  )
  set(rawiron_visual_shell_probe_folder_expected
    "[Shell Probe]"
    "Mode: open-folder"
    "Project id: rawiron-multiplayer-sandbox"
    "Ready: yes"
  )

  add_test(
    NAME RawIron.VisualShell.ListActions
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.VisualShell>"
      "-DCOMMAND_ARGS=--list-actions"
      "-DEXPECTED_STRINGS=${rawiron_visual_shell_actions_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_visual_shell_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.VisualShell.ListProjects
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.VisualShell>"
      "-DCOMMAND_ARGS=--list-projects"
      "-DEXPECTED_STRINGS=${rawiron_visual_shell_projects_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_visual_shell_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.VisualShell.DescribeProject
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.VisualShell>"
      "-DCOMMAND_ARGS=--describe-project;rawiron-multiplayer-sandbox"
      "-DEXPECTED_STRINGS=${rawiron_visual_shell_describe_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Format validation: fail"
      "${rawiron_visual_shell_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.VisualShell.DoctorProject
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.VisualShell>"
      "-DCOMMAND_ARGS=--doctor-project=rawiron-multiplayer-sandbox"
      "-DEXPECTED_STRINGS=${rawiron_visual_shell_doctor_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Doctor result: unhealthy"
      "${rawiron_visual_shell_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.VisualShell.ProbeOpenEditor
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.VisualShell>"
      "-DCOMMAND_ARGS=--probe-open-editor;rawiron-multiplayer-sandbox"
      "-DEXPECTED_STRINGS=${rawiron_visual_shell_probe_editor_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Ready: no"
      "${rawiron_visual_shell_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.VisualShell.ProbeOpenFolder
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.VisualShell>"
      "-DCOMMAND_ARGS=--probe-open-folder;rawiron-multiplayer-sandbox"
      "-DEXPECTED_STRINGS=${rawiron_visual_shell_probe_folder_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Ready: no"
      "${rawiron_visual_shell_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )
endif()
