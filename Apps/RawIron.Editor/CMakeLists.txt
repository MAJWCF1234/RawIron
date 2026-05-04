add_executable(RawIron.Editor
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

if (WIN32 AND EXISTS "${RAWIRON_VULKAN_RUNTIME_DLL}")
  add_custom_command(TARGET RawIron.Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RAWIRON_VULKAN_RUNTIME_DLL}"
      "$<TARGET_FILE_DIR:RawIron.Editor>/vulkan-1.dll"
  )
endif()

rawiron_stage_runtime_dlls(RawIron.Editor)

rawiron_bundle_engine_textures(RawIron.Editor)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  set(rawiron_editor_expected
    "[Editor Startup]"
    "Root nodes: 1"
    "Grid path: World/Grid"
    "Editor frame 0 orbitYaw=180.000000"
    "[Editor Shutdown]"
  )

  add_test(
    NAME RawIron.Editor.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Editor>"
      "-DCOMMAND_ARGS=--headless;--frames;1;--no-scene-dump"
      "-DEXPECTED_STRINGS=${rawiron_editor_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      -P "${rawiron_verify_command_output}"
  )
endif()
