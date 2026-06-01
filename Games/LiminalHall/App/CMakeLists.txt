add_executable(RawIron.LiminalGame
  src/main.cpp
)

target_link_libraries(RawIron.LiminalGame
  PRIVATE
    RawIron::Game.LiminalHall
)

target_compile_features(RawIron.LiminalGame PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.LiminalGame)

rawiron_bundle_engine_textures(RawIron.LiminalGame)
