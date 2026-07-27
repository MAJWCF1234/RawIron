add_executable(RawIron.Editor
  src/EditorAuthoringCatalog.cpp
  src/EditorLogicLayer.cpp
  src/EditorStructuralPicker.cpp
  src/EditorCreatorPalette.cpp
  src/EditorFilesInspector.cpp
  src/EditorHelpDialog.cpp
  src/EditorHierarchy.cpp
  src/EditorInput.cpp
  src/EditorInspectorPanels.cpp
  src/EditorLeftPanel.cpp
  src/EditorNewGame.cpp
  src/EditorPlaytestLauncher.cpp
  src/EditorPluginManager.cpp
  src/EditorProjectHealth.cpp
  src/EditorLevelExport.cpp
  src/EditorProjectScaffolding.cpp
  src/EditorResourceBrowser.cpp
  src/EditorResourceDocument.cpp
  src/EditorResourceTextEditor.cpp
  src/EditorRenderer.cpp
  src/EditorSceneController.cpp
  src/EditorViewportRenderer.cpp
  src/EditorVulkanViewport.cpp
  src/EditorWorkspace.cpp
  src/main.cpp
)

target_link_libraries(RawIron.Editor
  PRIVATE
    RawIron::Content
    RawIron::Core
    RawIron::Logic
    RawIron::Trace
    RawIron::Editor.BundledGames
    RawIron::EditorPreview
    RawIron::RenderSoftware
    RawIron::RenderVulkan
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
      msimg32
  )
endif()

if(MSVC)
  # Wilderness Ruins world build shares the same deep scene-builder stack frame as the game exe.
  target_link_options(RawIron.Editor PRIVATE /STACK:67108864)
endif()

target_compile_features(RawIron.Editor PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.Editor)

rawiron_bundle_engine_textures(RawIron.Editor)

if (RAWIRON_BUILD_TESTS)
  add_executable(EditorViewportPerformanceSmoke
    src/EditorViewportRenderer.cpp
    src/EditorRenderer.cpp
    ${CMAKE_SOURCE_DIR}/Tests/EditorViewportPerformanceSmoke.cpp
  )
  target_include_directories(EditorViewportPerformanceSmoke PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(EditorViewportPerformanceSmoke PRIVATE RawIron::RenderSoftware)
  if (WIN32)
    target_link_libraries(EditorViewportPerformanceSmoke PRIVATE gdi32 user32 msimg32)
  endif()
  target_compile_features(EditorViewportPerformanceSmoke PRIVATE cxx_std_20)
  add_test(NAME RawIron.Editor.ViewportPerformanceSmoke COMMAND EditorViewportPerformanceSmoke)

  add_executable(EditorPluginManagerSmoke
    src/EditorPluginManager.cpp
    ${CMAKE_SOURCE_DIR}/Tests/EditorPluginManagerSmoke.cpp
  )
  target_include_directories(EditorPluginManagerSmoke PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(EditorPluginManagerSmoke PRIVATE RawIron::Content RawIron::Core)
  target_compile_features(EditorPluginManagerSmoke PRIVATE cxx_std_20)
  add_test(NAME RawIron.Editor.PluginManagerSmoke COMMAND EditorPluginManagerSmoke)

  add_executable(EditorSceneControllerSmoke
    src/EditorSceneController.cpp
    ${CMAKE_SOURCE_DIR}/Tests/EditorSceneControllerSmoke.cpp
  )
  target_include_directories(EditorSceneControllerSmoke PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(EditorSceneControllerSmoke PRIVATE RawIron::SceneUtilities)
  target_compile_features(EditorSceneControllerSmoke PRIVATE cxx_std_20)
  add_test(NAME RawIron.Editor.SceneControllerSmoke COMMAND EditorSceneControllerSmoke)

  add_executable(EditorWorkspaceIndexSmoke
    src/EditorWorkspace.cpp
    ${CMAKE_SOURCE_DIR}/Tests/EditorWorkspaceIndexSmoke.cpp
  )
  target_include_directories(EditorWorkspaceIndexSmoke PRIVATE ${CMAKE_SOURCE_DIR})
  target_link_libraries(EditorWorkspaceIndexSmoke PRIVATE RawIron::Content)
  target_compile_features(EditorWorkspaceIndexSmoke PRIVATE cxx_std_20)
  add_test(NAME RawIron.Editor.WorkspaceIndexSmoke COMMAND EditorWorkspaceIndexSmoke)

  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_editor_runtime_dll_arg RawIron.Editor)
  set(rawiron_editor_forbidden
    failed
    exception
  )
  set(rawiron_editor_expected
    "[Editor Startup]"
    "Opened game project 'Liminal Hall'"
    "Workspace root: ${PROJECT_SOURCE_DIR}"
    "Root nodes: 1"
    "Grid path:"
    "Editor frame 0 orbitYaw="
    "[Editor Shutdown]"
  )

  set(rawiron_editor_external_working_directory "$ENV{TEMP}/RawIronEditorWorkspaceDiscovery")
  file(MAKE_DIRECTORY "${rawiron_editor_external_working_directory}")

  add_test(
    NAME RawIron.Editor.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;1;--no-scene-dump"
      "-DEXPECTED_STRINGS=${rawiron_editor_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_editor_forbidden}"
      "-DWORKING_DIRECTORY=${rawiron_editor_external_working_directory}"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Editor.AuthoringHandoff
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;1;--no-scene-dump;--workspace-root;${PROJECT_SOURCE_DIR};--open-asset;${PROJECT_SOURCE_DIR}/Assets/Source/models/scenekit_triangle.gltf"
      "-DEXPECTED_STRINGS=Authoring handoff: ready;Handoff asset kind: model-source;scenekit_triangle.gltf"
      "-DFORBIDDEN_STRINGS=rejected;failed;exception"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  set(rawiron_editor_mps_expected
    "[Editor Startup]"
    "Opened game project 'RawIron Multiplayer Sandbox'"
    "Game root: ${CMAKE_SOURCE_DIR}/Games/RawIronMultiplayerSandbox"
    "Scene state: ${CMAKE_SOURCE_DIR}/Saved/Editor/rawiron-multiplayer-sandbox/scene_state.ri_state"
    "Editor frame 11 orbitYaw="
    "[Editor Shutdown]"
  )

  add_test(
    NAME RawIron.Editor.MultiplayerSandbox
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;12;--log-every-frame;--no-scene-dump;--workspace-root=${CMAKE_SOURCE_DIR};--game=rawiron-multiplayer-sandbox"
      "-DEXPECTED_STRINGS=${rawiron_editor_mps_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_editor_forbidden}"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  set(rawiron_editor_liminal_expected
    "[Editor Startup]"
    "Opened game project 'Liminal Hall'"
    "Game root: ${CMAKE_SOURCE_DIR}/Games/LiminalHall"
    "Editor frame 11 orbitYaw="
    "[Editor Shutdown]"
  )

  add_test(
    NAME RawIron.Editor.LiminalHall
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;12;--log-every-frame;--no-scene-dump;--workspace-root=${CMAKE_SOURCE_DIR};--game=liminal-hall"
      "-DEXPECTED_STRINGS=${rawiron_editor_liminal_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_editor_forbidden}"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  set(rawiron_editor_wilderness_expected
    "[Editor Startup]"
    "Opened game project 'Wilderness Ruins'"
    "Game root: ${CMAKE_SOURCE_DIR}/Games/WildernessRuins"
    "Grid path: WildernessRuinsLayer/ForestAuthoringGrid"
    "Editor frame 7 orbitYaw="
    "[Editor Shutdown]"
  )

  add_test(
    NAME RawIron.Editor.WildernessRuins
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;8;--log-every-frame;--no-scene-dump;--workspace-root=${CMAKE_SOURCE_DIR};--game=wilderness-ruins"
      "-DEXPECTED_STRINGS=${rawiron_editor_wilderness_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_editor_forbidden}"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Editor.PluginStorePackages
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyPluginStorePackages.cmake
  )
endif()
