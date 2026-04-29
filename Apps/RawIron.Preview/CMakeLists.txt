add_executable(RawIron.Preview
  src/main.cpp
)

target_link_libraries(RawIron.Preview
  PRIVATE
    RawIron::Core
    RawIron::SceneUtilities
    RawIron::RenderSoftware
    RawIron::RenderVulkan
)

if (WIN32)
  target_link_libraries(RawIron.Preview
    PRIVATE
      user32
      gdi32
  )
endif()

target_compile_features(RawIron.Preview PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.Preview)

rawiron_bundle_engine_textures(RawIron.Preview)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  set(rawiron_preview_smoke_root "${CMAKE_CURRENT_BINARY_DIR}/preview-smoke")
  set(rawiron_preview_expected
    "[Preview Startup]"
    "Headless preview saved:"
    "Image size: 320x240"
  )

  add_test(
    NAME RawIron.Preview.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Preview>"
      "-DCOMMAND_ARGS=--headless;--output;${rawiron_preview_smoke_root}/preview.bmp;--width;320;--height;240"
      "-DEXPECTED_STRINGS=${rawiron_preview_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "-DVERIFY_FILES_ROOT=${rawiron_preview_smoke_root}"
      "-DREQUIRED_RELATIVE_FILES=preview.bmp"
      -P "${rawiron_verify_command_output}"
  )
endif()
