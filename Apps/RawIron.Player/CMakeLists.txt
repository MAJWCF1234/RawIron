add_executable(RawIron.Player
  src/main.cpp
)

target_link_libraries(RawIron.Player
  PRIVATE
    RawIron::Audio
    RawIron::Core
    RawIron::RenderVulkan
    RawIron::SceneUtilities
)

target_compile_features(RawIron.Player PRIVATE cxx_std_20)

if (WIN32 AND EXISTS "${RAWIRON_VULKAN_RUNTIME_DLL}")
  add_custom_command(TARGET RawIron.Player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RAWIRON_VULKAN_RUNTIME_DLL}"
      "$<TARGET_FILE_DIR:RawIron.Player>/vulkan-1.dll"
  )
endif()

rawiron_stage_runtime_dlls(RawIron.Player)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  set(rawiron_player_expected
    "[Player Startup]"
    "[Player Vulkan]"
    "Renderable nodes: 6"
    "Orbit camera path: World/OrbitRig/OrbitSwivel/MainCamera"
    "Player simulation tick 0 crate=(0.00, 0.50, 0.00)"
    "[Player Shutdown]"
  )
  if (WIN32)
    list(APPEND rawiron_player_expected
      "Vulkan platform: Windows"
      "Vulkan surface: ready"
    )
  elseif(UNIX AND NOT APPLE)
    list(APPEND rawiron_player_expected
      "Vulkan platform: Linux"
      "Vulkan surface: platform-layer-pending"
    )
  endif()

  add_test(
    NAME RawIron.Player.Smoke
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Player>"
      "-DCOMMAND_ARGS=--frames;1;--no-scene-dump"
      "-DEXPECTED_STRINGS=${rawiron_player_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      -P "${rawiron_verify_command_output}"
  )
endif()
