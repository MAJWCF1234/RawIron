include(FetchContent)
find_package(Vulkan REQUIRED)
find_program(RAWIRON_XR_GLSLANG_VALIDATOR
  NAMES glslangValidator glslangValidator.exe
  HINTS ENV VULKAN_SDK
  PATH_SUFFIXES Bin bin
)

set(RAWIRON_XR_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(RAWIRON_XR_SHADER_OUTPUTS)
if(RAWIRON_XR_GLSLANG_VALIDATOR)
  file(MAKE_DIRECTORY "${RAWIRON_XR_SHADER_DIR}")
  foreach(shader_name IN ITEMS XrScene.vert XrScene.frag)
    set(shader_input "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_name}")
    set(shader_output "${RAWIRON_XR_SHADER_DIR}/${shader_name}.spv")
    add_custom_command(
      OUTPUT "${shader_output}"
      COMMAND "${RAWIRON_XR_GLSLANG_VALIDATOR}" -V "${shader_input}" -o "${shader_output}"
      DEPENDS "${shader_input}"
      VERBATIM
    )
    list(APPEND RAWIRON_XR_SHADER_OUTPUTS "${shader_output}")
  endforeach()
  add_custom_target(RawIron.XR.OpenXR.Shaders DEPENDS ${RAWIRON_XR_SHADER_OUTPUTS})
endif()

find_package(OpenXR CONFIG QUIET)
if(NOT TARGET OpenXR::openxr_loader)
  # Pinned official Khronos loader. It is fetched into the build cache, not committed
  # into the engine or a shipped game, keeping source distribution lean.
  set(BUILD_API_LAYERS OFF)
  set(BUILD_CONFORMANCE_TESTS OFF)
  set(BUILD_SDK_TESTS OFF)
  set(BUILD_TESTS OFF)
  set(BUILD_LOADER ON)
  set(DYNAMIC_LOADER OFF)
  FetchContent_Declare(
    openxr_sdk
    GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
    GIT_TAG 57af7fc61f9f2d492580cb28aab6d0ea59d8d417
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(openxr_sdk)
endif()

add_library(RawIron.XR.OpenXR
  src/OpenXrRuntime.cpp
)
add_library(RawIron::XrOpenXR ALIAS RawIron.XR.OpenXR)

target_include_directories(RawIron.XR.OpenXR PUBLIC include)
target_link_libraries(RawIron.XR.OpenXR
  PUBLIC RawIron::Core
  PRIVATE OpenXR::openxr_loader Vulkan::Vulkan
)
target_compile_features(RawIron.XR.OpenXR PUBLIC cxx_std_20)
if(RAWIRON_XR_GLSLANG_VALIDATOR)
  add_dependencies(RawIron.XR.OpenXR RawIron.XR.OpenXR.Shaders)
  target_compile_definitions(RawIron.XR.OpenXR PRIVATE
    RAWIRON_XR_HARDWARE_SCENE_ENABLED=1
    RAWIRON_XR_SHADER_DIR="${RAWIRON_XR_SHADER_DIR}"
  )
else()
  target_compile_definitions(RawIron.XR.OpenXR PRIVATE RAWIRON_XR_HARDWARE_SCENE_ENABLED=0)
endif()

if(RAWIRON_BUILD_TESTS)
  add_executable(OpenXrRuntimeSmoke tests/OpenXrRuntimeSmoke.cpp)
  target_link_libraries(OpenXrRuntimeSmoke PRIVATE RawIron::XrOpenXR)
  add_test(NAME RawIron.XR.OpenXR.RuntimeSmoke COMMAND OpenXrRuntimeSmoke)
endif()
