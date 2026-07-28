add_executable(RawIron.Player
  src/main.cpp
)

target_link_libraries(RawIron.Player
  PRIVATE
    RawIron::Audio
    RawIron::Content
    RawIron::Core
    RawIron::RenderVulkan
    RawIron::SceneUtilities
)

target_compile_features(RawIron.Player PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.Player)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_player_runtime_dll_arg RawIron.Player)
  set(rawiron_player_expected
    "[Player Startup]"
    "Running headless player runtime."
    "Mounted game: Cube Test (cube-test)"
    "Imported level: 7 primitives from levels/assembly.primitives.csv."
    "Orbit camera path: World/OrbitRig/OrbitSwivel/MainCamera"
    "Player simulation tick 0 crate=(0.00, 0.50, 0.00)"
    "[Player Shutdown]"
  )

  add_test(
    NAME RawIron.Player.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Player>"
      "-DCOMMAND_ARGS=--headless;--frames;1;--no-scene-dump;--game;cube-test"
      "-DEXPECTED_STRINGS=${rawiron_player_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_player_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )
endif()
