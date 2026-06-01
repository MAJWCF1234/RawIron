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
