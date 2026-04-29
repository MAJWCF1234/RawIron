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
  foreach(shader_name IN ITEMS NativeScenePreview.vert NativeScenePreview.frag NativeSkybox.vert NativeSkybox.frag NativeShadowDepth.vert)
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

  add_custom_target(RawIron.Render.Vulkan.Shaders DEPENDS ${RAWIRON_VULKAN_SHADER_OUTPUTS})
endif()

add_library(RawIron.Render.Vulkan
  src/VulkanCommandBufferRecorder.cpp
  src/VulkanCommandList.cpp
  src/VulkanCommandRecorder.cpp
  src/VulkanFrameSubmission.cpp
  src/VulkanIntentStaging.cpp
  src/VulkanPipelineStateCache.cpp
  src/VulkanBootstrap.cpp
  src/VulkanNativeScenePreview.cpp
  src/VulkanPreviewPresenter.cpp
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
