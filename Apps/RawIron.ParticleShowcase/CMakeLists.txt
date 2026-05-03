add_executable(RawIron.ParticleShowcase
  src/main.cpp
)

target_link_libraries(RawIron.ParticleShowcase
  PRIVATE
    RawIron::Core
    RawIron::SceneUtilities
    RawIron::RenderVulkan
    RawIron::Audio
)

if (WIN32)
  target_link_libraries(RawIron.ParticleShowcase
    PRIVATE
      user32
      gdi32
  )
endif()

target_compile_features(RawIron.ParticleShowcase PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.ParticleShowcase)
rawiron_bundle_engine_textures(RawIron.ParticleShowcase)

if (WIN32 AND EXISTS "${RAWIRON_VULKAN_RUNTIME_DLL}")
  add_custom_command(TARGET RawIron.ParticleShowcase POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RAWIRON_VULKAN_RUNTIME_DLL}"
      "$<TARGET_FILE_DIR:RawIron.ParticleShowcase>/vulkan-1.dll"
  )
endif()
