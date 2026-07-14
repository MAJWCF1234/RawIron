add_executable(RawIron.BotClient
  src/main.cpp
)

target_link_libraries(RawIron.BotClient
  PRIVATE
    RawIron::Core
    RawIron::Runtime
)

target_compile_features(RawIron.BotClient PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.BotClient)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_bot_runtime_dll_arg RawIron.BotClient)
  set(rawiron_bot_expected
    "Host: RawIron.BotClient"
    "[Runtime Core]"
    "Bot host online."
    "Bot host shutdown complete."
  )
  add_test(
    NAME RawIron.BotClient.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.BotClient>"
      "-DCOMMAND_ARGS=--frames;2;--offline;--bots;2"
      "-DEXPECTED_STRINGS=${rawiron_bot_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_bot_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )
endif()
