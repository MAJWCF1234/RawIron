add_executable(RawIron.Editor
  src/EditorFilesInspector.cpp
  src/EditorHierarchy.cpp
  src/EditorInput.cpp
  src/EditorInspectorPanels.cpp
  src/EditorLeftPanel.cpp
  src/EditorPlaytestLauncher.cpp
  src/EditorProjectScaffolding.cpp
  src/EditorResourceBrowser.cpp
  src/EditorResourceDocument.cpp
  src/EditorResourceTextEditor.cpp
  src/EditorRenderer.cpp
  src/EditorSceneController.cpp
  src/EditorViewportRenderer.cpp
  src/EditorWorkspace.cpp
  src/main.cpp
)

target_link_libraries(RawIron.Editor
  PRIVATE
    RawIron::Content
    RawIron::Core
    RawIron::Editor.BundledGames
    RawIron::EditorPreview
    RawIron::RenderSoftware
    RawIron::Runtime
    RawIron::SceneUtilities
    RawIron::UI
    RawIron::World
)

if (WIN32)
  target_link_libraries(RawIron.Editor
    PRIVATE
      shell32
      user32
      gdi32
  )
endif()

target_compile_features(RawIron.Editor PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.Editor)

rawiron_bundle_engine_textures(RawIron.Editor)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_editor_runtime_dll_arg RawIron.Editor)
  set(rawiron_editor_forbidden
    failed
    exception
  )
  set(rawiron_editor_expected
    "[Editor Startup]"
    "Root nodes: 1"
    "Grid path:"
    "Editor frame 0 orbitYaw="
    "[Editor Shutdown]"
  )

  add_test(
    NAME RawIron.Editor.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;1;--no-scene-dump"
      "-DEXPECTED_STRINGS=${rawiron_editor_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_editor_forbidden}"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  set(rawiron_editor_mps_expected
    "[Editor Startup]"
    "Opened game project 'RawIron Multiplayer Sandbox'"
    "Game root: D:\\RawIron\\Games\\RawIronMultiplayerSandbox"
    "Scene state: D:\\RawIron\\Saved\\Editor\\rawiron-multiplayer-sandbox\\scene_state.ri_state"
    "Editor frame 11 orbitYaw="
    "[Editor Shutdown]"
  )

  add_test(
    NAME RawIron.Editor.MultiplayerSandbox
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;12;--log-every-frame;--no-scene-dump;--workspace-root=D:\\RawIron;--game=rawiron-multiplayer-sandbox"
      "-DEXPECTED_STRINGS=${rawiron_editor_mps_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_editor_forbidden}"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  set(rawiron_editor_liminal_expected
    "[Editor Startup]"
    "Opened game project 'Liminal Hall'"
    "Game root: D:\\RawIron\\Games\\LiminalHall"
    "Editor frame 11 orbitYaw="
    "[Editor Shutdown]"
  )

  add_test(
    NAME RawIron.Editor.LiminalHall
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;12;--log-every-frame;--no-scene-dump;--workspace-root=D:\\RawIron;--game=liminal-hall"
      "-DEXPECTED_STRINGS=${rawiron_editor_liminal_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_editor_forbidden}"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  set(rawiron_editor_wilderness_expected
    "[Editor Startup]"
    "Opened game project 'Wilderness Ruins'"
    "Game root: D:\\RawIron\\Games\\WildernessRuins"
    "Grid path: World/Grid"
    "Editor frame 7 orbitYaw="
    "[Editor Shutdown]"
  )

  add_test(
    NAME RawIron.Editor.WildernessRuins
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;8;--log-every-frame;--no-scene-dump;--workspace-root=D:\\RawIron;--game=wilderness-ruins"
      "-DEXPECTED_STRINGS=${rawiron_editor_wilderness_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_editor_forbidden}"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )
endif()
