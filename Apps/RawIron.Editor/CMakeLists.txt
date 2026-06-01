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

rawiron_stage_runtime_dlls(RawIron.Editor)

rawiron_bundle_engine_textures(RawIron.Editor)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_editor_runtime_dll_arg RawIron.Editor)
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
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_editor_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )
endif()
