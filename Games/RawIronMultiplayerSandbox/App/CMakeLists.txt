add_executable(RawIron.MultiplayerSandboxGame
  src/main.cpp
)

target_link_libraries(RawIron.MultiplayerSandboxGame
  PRIVATE
    RawIron::Game.MultiplayerSandbox
)

target_compile_features(RawIron.MultiplayerSandboxGame PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.MultiplayerSandboxGame)
rawiron_bundle_engine_textures(RawIron.MultiplayerSandboxGame)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_multiplayer_sandbox_runtime_dll_arg RawIron.MultiplayerSandboxGame)
  set(rawiron_multiplayer_sandbox_offline_expected
    "Sandbox runtime mode: net"
    "[Runtime Core]"
    "Net: offline mode enabled; authoritative transport is disabled."
  )
  add_test(
    NAME RawIron.MultiplayerSandbox.OfflineBoot
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.MultiplayerSandboxGame>"
      "-DCOMMAND_ARGS=--runtime-mode=net;--frames=2;--offline;--bots=2"
      "-DEXPECTED_STRINGS=${rawiron_multiplayer_sandbox_offline_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_multiplayer_sandbox_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )
endif()
