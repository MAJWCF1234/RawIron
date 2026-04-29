add_executable(RawIron.ForestRuinsGame
  src/main.cpp
)

target_link_libraries(RawIron.ForestRuinsGame
  PRIVATE
    RawIron::Game.ForestRuins
)

target_compile_features(RawIron.ForestRuinsGame PRIVATE cxx_std_20)

if (WIN32 AND EXISTS "${RAWIRON_VULKAN_RUNTIME_DLL}")
  add_custom_command(TARGET RawIron.ForestRuinsGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RAWIRON_VULKAN_RUNTIME_DLL}"
      "$<TARGET_FILE_DIR:RawIron.ForestRuinsGame>/vulkan-1.dll"
  )
endif()

rawiron_stage_runtime_dlls(RawIron.ForestRuinsGame)

rawiron_bundle_engine_textures(RawIron.ForestRuinsGame)
