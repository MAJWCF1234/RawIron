add_executable(RawIron.LiminalGame
  src/main.cpp
)

target_link_libraries(RawIron.LiminalGame
  PRIVATE
    RawIron::Game.LiminalHall
)

target_compile_features(RawIron.LiminalGame PRIVATE cxx_std_20)

if (WIN32 AND EXISTS "${RAWIRON_VULKAN_RUNTIME_DLL}")
  add_custom_command(TARGET RawIron.LiminalGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RAWIRON_VULKAN_RUNTIME_DLL}"
      "$<TARGET_FILE_DIR:RawIron.LiminalGame>/vulkan-1.dll"
  )
endif()

rawiron_stage_runtime_dlls(RawIron.LiminalGame)

rawiron_bundle_engine_textures(RawIron.LiminalGame)
