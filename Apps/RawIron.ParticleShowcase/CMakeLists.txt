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

# Native Vulkan preview loads SPIR-V from a compile-time path that breaks when the repo is moved
# to another drive. Copy shaders next to the exe so ResolveVulkanNativeShaderDirectory() finds them.
if (WIN32 AND TARGET RawIron.Render.Vulkan)
  add_custom_command(TARGET RawIron.ParticleShowcase POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RawIron.ParticleShowcase>/vulkan/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "$<TARGET_PROPERTY:RawIron.Render.Vulkan,BINARY_DIR>/shaders"
      "$<TARGET_FILE_DIR:RawIron.ParticleShowcase>/vulkan/shaders"
    COMMENT "Stage Vulkan SPIR-V next to RawIron.ParticleShowcase for portable runs"
    VERBATIM
  )
  set(RAWIRON_PARTICLE_SHOWCASE_REFERENCE_SHADERS "${CMAKE_SOURCE_DIR}/Source/RawIron.Render.Vulkan/ReferenceShaders")
  if (EXISTS "${RAWIRON_PARTICLE_SHOWCASE_REFERENCE_SHADERS}")
    add_custom_command(TARGET RawIron.ParticleShowcase POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RawIron.ParticleShowcase>/vulkan/reference-shaders"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RAWIRON_PARTICLE_SHOWCASE_REFERENCE_SHADERS}"
        "$<TARGET_FILE_DIR:RawIron.ParticleShowcase>/vulkan/reference-shaders"
      COMMENT "Stage reference FX pack next to RawIron.ParticleShowcase"
      VERBATIM
    )
  endif()
endif()
