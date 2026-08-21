add_executable(RawIron.VRShowcase src/main.cpp)
target_link_libraries(RawIron.VRShowcase
  PRIVATE
    RawIron::Core
    RawIron::Game.CubeTest
    RawIron::RenderSoftware
    RawIron::XrOpenXR
)
target_compile_features(RawIron.VRShowcase PRIVATE cxx_std_20)
if(WIN32)
  target_link_libraries(RawIron.VRShowcase PRIVATE advapi32)
endif()
rawiron_stage_runtime_dlls(RawIron.VRShowcase)
