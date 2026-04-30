add_executable(RawIron.Core.Tests
  src/main.cpp
)

target_link_libraries(RawIron.Core.Tests
  PRIVATE
    RawIron::Core
    RawIron::Runtime
    RawIron::RenderVulkan
    RawIron::RenderSoftware
    RawIron::SceneUtilities
    RawIron::Trace
)
if (RAWIRON_BUILD_DEV_INSPECTOR)
  target_link_libraries(RawIron.Core.Tests PRIVATE RawIron::DevInspector)
endif()

target_compile_features(RawIron.Core.Tests PRIVATE cxx_std_20)

target_include_directories(RawIron.Core.Tests
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include")

rawiron_stage_runtime_dlls(RawIron.Core.Tests)
rawiron_bundle_engine_textures(RawIron.Core.Tests)

add_test(
  NAME RawIron.Core.Tests
  COMMAND RawIron.Core.Tests
)
