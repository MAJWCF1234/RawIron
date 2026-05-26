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

if (WIN32 AND TARGET RawIron.Render.Vulkan.Shaders)
  add_dependencies(RawIron.ForestRuinsGame RawIron.Render.Vulkan.Shaders)
  add_custom_command(TARGET RawIron.ForestRuinsGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RawIron.ForestRuinsGame>/vulkan/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_BINARY_DIR}/Source/RawIron.Render.Vulkan/shaders"
      "$<TARGET_FILE_DIR:RawIron.ForestRuinsGame>/vulkan/shaders"
    COMMENT "Stage Vulkan SPIR-V next to RawIron.ForestRuinsGame for portable runs"
    VERBATIM
  )
endif()
