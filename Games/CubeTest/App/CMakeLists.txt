add_executable(RawIron.CubeTestGame
  src/main.cpp
)

target_link_libraries(RawIron.CubeTestGame
  PRIVATE
    RawIron::Game.CubeTest
)

target_compile_features(RawIron.CubeTestGame PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.CubeTestGame)
rawiron_bundle_engine_textures(RawIron.CubeTestGame)
