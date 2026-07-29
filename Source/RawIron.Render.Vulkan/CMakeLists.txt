find_package(Vulkan REQUIRED)
find_program(RAWIRON_GLSLANG_VALIDATOR
  NAMES glslangValidator glslangValidator.exe
  HINTS ENV VULKAN_SDK
  PATH_SUFFIXES Bin bin
)

set(RAWIRON_VULKAN_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(RAWIRON_VULKAN_SHADER_OUTPUTS)

if(RAWIRON_GLSLANG_VALIDATOR)
  file(MAKE_DIRECTORY "${RAWIRON_VULKAN_SHADER_DIR}")
  foreach(shader_name IN ITEMS NativeScenePreview.vert NativeScenePreview.frag NativeSkybox.vert NativeSkybox.frag NativeShadowDepth.vert NativeShadowDepth.frag NativeComposite.vert NativeComposite.frag NativeHybridComposite.vert NativeHybridComposite.frag NativeHybridScreenSpace.vert NativeHybridScreenSpace.frag ProceduralMaps.comp)
    set(shader_input "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_name}")
    set(shader_output "${RAWIRON_VULKAN_SHADER_DIR}/${shader_name}.spv")
    add_custom_command(
      OUTPUT "${shader_output}"
      COMMAND "${RAWIRON_GLSLANG_VALIDATOR}" -V "${shader_input}" -o "${shader_output}"
      DEPENDS "${shader_input}"
      VERBATIM
    )
    list(APPEND RAWIRON_VULKAN_SHADER_OUTPUTS "${shader_output}")
  endforeach()

  set(RAWIRON_NATIVE_TEXTURE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/NativeTextures")
  file(GLOB_RECURSE RAWIRON_NATIVE_TEXTURE_INPUTS CONFIGURE_DEPENDS LIST_DIRECTORIES false
    "${RAWIRON_NATIVE_TEXTURE_ROOT}/*")
  foreach(texture_input IN LISTS RAWIRON_NATIVE_TEXTURE_INPUTS)
    file(RELATIVE_PATH texture_relative "${RAWIRON_NATIVE_TEXTURE_ROOT}" "${texture_input}")
    set(texture_output "${RAWIRON_VULKAN_SHADER_DIR}/NativeTextures/${texture_relative}")
    get_filename_component(texture_output_dir "${texture_output}" DIRECTORY)
    add_custom_command(
      OUTPUT "${texture_output}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${texture_output_dir}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${texture_input}" "${texture_output}"
      DEPENDS "${texture_input}"
      VERBATIM
    )
    list(APPEND RAWIRON_VULKAN_SHADER_OUTPUTS "${texture_output}")
  endforeach()

  add_custom_target(RawIron.Render.Vulkan.Shaders DEPENDS ${RAWIRON_VULKAN_SHADER_OUTPUTS})
endif()

add_library(RawIron.Render.Vulkan
  src/VulkanCommandBufferRecorder.cpp
  src/VulkanCommandList.cpp
  src/VulkanCommandRecorder.cpp
  src/VulkanFrameSubmission.cpp
  src/VulkanFrameTuning.cpp
  src/VulkanIntentStaging.cpp
  src/VulkanPipelineStateCache.cpp
  src/VulkanWarmupCache.cpp
  src/VulkanBootstrap.cpp
  src/VulkanNativeScenePreview.cpp
  src/VulkanPreviewPresenter.cpp
  src/VulkanScenePreviewBridge.cpp
  src/ProceduralMaterialMaps.cpp
)

add_library(RawIron::RenderVulkan ALIAS RawIron.Render.Vulkan)

target_include_directories(RawIron.Render.Vulkan
  PUBLIC
    include
)

if(RAWIRON_GLSLANG_VALIDATOR)
  add_dependencies(RawIron.Render.Vulkan RawIron.Render.Vulkan.Shaders)
  target_compile_definitions(RawIron.Render.Vulkan PRIVATE
    RAWIRON_VULKAN_NATIVE_PREVIEW_ENABLED=1
    RAWIRON_VULKAN_SHADER_DIR="${RAWIRON_VULKAN_SHADER_DIR}"
  )
else()
  message(WARNING "glslangValidator was not found; native Vulkan preview path will be disabled.")
  target_compile_definitions(RawIron.Render.Vulkan PRIVATE RAWIRON_VULKAN_NATIVE_PREVIEW_ENABLED=0)
endif()

target_link_libraries(RawIron.Render.Vulkan
  PUBLIC
    RawIron::Core
    RawIron::RenderSoftware
    RawIron::SceneUtilities
    Vulkan::Vulkan
)

if (UNIX AND NOT APPLE AND CMAKE_DL_LIBS)
  target_link_libraries(RawIron.Render.Vulkan
    PRIVATE
      ${CMAKE_DL_LIBS}
  )
endif()

target_compile_features(RawIron.Render.Vulkan PUBLIC cxx_std_20)

if(RAWIRON_BUILD_TESTS)
  add_executable(VulkanHybridPostProcessSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/VulkanHybridPostProcessSafetySmoke.cpp)
  target_link_libraries(VulkanHybridPostProcessSafetySmoke PRIVATE RawIron::RenderVulkan)
  add_test(NAME RawIron.Render.Vulkan.HybridPostProcessSafetySmoke COMMAND VulkanHybridPostProcessSafetySmoke)
  add_executable(VulkanCommandPipelineSmoke ${CMAKE_SOURCE_DIR}/Tests/VulkanCommandPipelineSmoke.cpp)
  target_link_libraries(VulkanCommandPipelineSmoke PRIVATE RawIron::RenderVulkan)
  add_test(NAME RawIron.Render.Vulkan.CommandPipelineSmoke COMMAND VulkanCommandPipelineSmoke)
  add_executable(VulkanFrameTuningSmoke ${CMAKE_SOURCE_DIR}/Tests/VulkanFrameTuningSmoke.cpp)
  target_link_libraries(VulkanFrameTuningSmoke PRIVATE RawIron::RenderVulkan)
  add_test(NAME RawIron.Render.Vulkan.FrameTuningSmoke COMMAND VulkanFrameTuningSmoke)
  add_executable(VulkanWarmupCacheSmoke ${CMAKE_SOURCE_DIR}/Tests/VulkanWarmupCacheSmoke.cpp)
  target_link_libraries(VulkanWarmupCacheSmoke PRIVATE RawIron::RenderVulkan)
  add_test(NAME RawIron.Render.Vulkan.WarmupCacheSmoke COMMAND VulkanWarmupCacheSmoke)
  add_executable(VulkanResourceTelemetrySmoke ${CMAKE_SOURCE_DIR}/Tests/VulkanResourceTelemetrySmoke.cpp)
  target_link_libraries(VulkanResourceTelemetrySmoke PRIVATE RawIron::RenderVulkan)
  add_test(NAME RawIron.Render.Vulkan.ResourceTelemetrySmoke COMMAND VulkanResourceTelemetrySmoke)
  add_test(
    NAME RawIron.Render.Vulkan.NativeScenePreviewDetailNormalSmoke
    COMMAND ${CMAKE_COMMAND}
      -DSHADER_PATH=${CMAKE_CURRENT_SOURCE_DIR}/shaders/NativeScenePreview.frag
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyNativeScenePreviewDetailNormal.cmake
  )
  add_test(
    NAME RawIron.Render.Vulkan.ExtendedCompositeWiringSmoke
    COMMAND ${CMAKE_COMMAND}
      -DRUNTIME_PATH=${CMAKE_CURRENT_SOURCE_DIR}/src/VulkanNativeScenePreview.cpp
      -DOPTIONS_PATH=${CMAKE_CURRENT_SOURCE_DIR}/include/RawIron/Render/VulkanPreviewPresenter.h
      -DSHADER_PATH=${CMAKE_CURRENT_SOURCE_DIR}/shaders/NativeComposite.frag
      -DLITE_SHADER_PATH=${CMAKE_CURRENT_SOURCE_DIR}/shaders/NativeHybridComposite.frag
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyExtendedCompositeWiring.cmake
  )

  add_test(
    NAME RawIron.Render.Vulkan.ReferenceShaderMigrationSmoke
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -DRAWIRON_BINARY_SHADER_DIR=${RAWIRON_VULKAN_SHADER_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyReferenceShaderMigration.cmake
  )
endif()
