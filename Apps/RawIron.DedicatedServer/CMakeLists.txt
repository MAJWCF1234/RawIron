add_executable(RawIron.DedicatedServer
  src/main.cpp
)

target_link_libraries(RawIron.DedicatedServer
  PRIVATE
    RawIron::Core
    RawIron::Runtime
)

target_compile_features(RawIron.DedicatedServer PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.DedicatedServer)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_dedicated_runtime_dll_arg RawIron.DedicatedServer)
  set(rawiron_dedicated_expected
    "Host: RawIron.DedicatedServer"
    "[Runtime Core]"
    "Dedicated server online."
    "Dedicated server shutdown complete."
  )
  add_test(
    NAME RawIron.DedicatedServer.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.DedicatedServer>"
      "-DCOMMAND_ARGS=--frames;2;--port;27125;--rendezvous;direct;--issue-join-code"
      "-DEXPECTED_STRINGS=${rawiron_dedicated_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_dedicated_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )
endif()
